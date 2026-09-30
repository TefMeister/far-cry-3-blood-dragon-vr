#include "ctab.h"

/* Token stream layout (D3D9 shader model 1-3):
 *   version token (0xFFFE____ vertex, 0xFFFF____ pixel), then instructions.
 *   A comment is 0x____FFFE with its length in DWORDs in bits 16..30; the
 *   constant table is the comment whose first DWORD is 'CTAB'.
 *   The stream ends with 0x0000FFFF.
 *
 * CTAB block (offsets from the start of the block, i.e. after 'CTAB'):
 *   +0  Size (28)       +4  Creator     +8  Version     +12 Constants
 *   +16 ConstantInfo    +20 Flags       +24 Target
 * Each ConstantInfo is 20 bytes:
 *   +0 Name (offset)  +4 RegisterSet (u16)  +6 RegisterIndex (u16)
 *   +8 RegisterCount (u16)  +10 Reserved  +12 TypeInfo  +16 DefaultValue */

#define TOK_END        0x0000FFFFu
#define TOK_COMMENT    0xFFFEu
#define FOURCC_CTAB    0x42415443u /* 'C','T','A','B' little-endian */
#define CTAB_HEADER    28u
#define CTAB_INFO_SIZE 20u

static unsigned rd32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static unsigned rd16(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

/* Parse one CTAB block of `len` bytes. */
static int parse_block(const unsigned char *b, size_t len, bd_const *out, int max_out) {
    unsigned n, info, i;
    int written = 0;

    if (len < CTAB_HEADER || rd32(b) != CTAB_HEADER) {
        return -1;
    }
    n = rd32(b + 12);
    info = rd32(b + 16);
    if (info > len || n > (len - info) / CTAB_INFO_SIZE) {
        return -1;
    }
    for (i = 0; i < n && written < max_out; i++) {
        const unsigned char *ci = b + info + i * CTAB_INFO_SIZE;
        unsigned name_off = rd32(ci);
        bd_const *c = &out[written];
        size_t k = 0;

        if (name_off >= len) {
            continue;
        }
        while (k + 1 < BD_CTAB_NAME_MAX && name_off + k < len && b[name_off + k] != 0) {
            c->name[k] = (char)b[name_off + k];
            k++;
        }
        c->name[k] = 0;
        c->regset = (int)rd16(ci + 4);
        c->reg = (int)rd16(ci + 6);
        c->count = (int)rd16(ci + 8);
        written++;
    }
    return written;
}

int bd_ctab_parse(const void *bytecode, size_t max_bytes, bd_const *out, int max_out, int *is_vertex) {
    const unsigned char *p = (const unsigned char *)bytecode;
    size_t pos = 4;
    unsigned version;

    if (is_vertex) {
        *is_vertex = 0;
    }
    if (p == 0 || out == 0 || max_out <= 0) {
        return -1;
    }
    if (max_bytes == 0 || max_bytes > BD_CTAB_MAX_SHADER_BYTES) {
        max_bytes = BD_CTAB_MAX_SHADER_BYTES;
    }
    if (max_bytes < 8) {
        return -1;
    }
    version = rd32(p);
    if ((version >> 16) != 0xFFFEu && (version >> 16) != 0xFFFFu) {
        return -1;
    }
    if (is_vertex) {
        *is_vertex = (version >> 16) == 0xFFFEu;
    }

    /* The CTAB comment comes before the first instruction; walk comments only. */
    while (pos + 4 <= max_bytes) {
        unsigned tok = rd32(p + pos);
        size_t words, bytes;

        if (tok == TOK_END || (tok & 0xFFFFu) != TOK_COMMENT) {
            return -1;
        }
        words = (tok >> 16) & 0x7FFFu;
        bytes = words * 4;
        if (pos + 4 + bytes > max_bytes) {
            return -1;
        }
        if (bytes >= 4 && rd32(p + pos + 4) == FOURCC_CTAB) {
            return parse_block(p + pos + 8, bytes - 4, out, max_out);
        }
        pos += 4 + bytes;
    }
    return -1;
}
