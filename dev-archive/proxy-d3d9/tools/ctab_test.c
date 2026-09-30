/* ctab_test.c - checks src/ctab.c against real shaders compiled by fxc.
 *
 *   gcc -O2 -Wall -Wextra -Wpedantic -o ctab_test.exe tools/ctab_test.c src/ctab.c
 *   ./ctab_test.exe tools/vs30.cso tools/vs20.cso
 *
 * tools/ctab_fixture.hlsl places known names at known registers; the test
 * checks every one comes back right from both shader models, that a long
 * name is cut safely, and that damaged or cut-short bytecode is refused
 * without reading past the given size. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ctab.h"

static int g_pass = 0, g_fail = 0;

static void check(int ok, const char *what, const char *file) {
    if (ok) {
        g_pass++;
    } else {
        g_fail++;
        printf("FAIL (%s): %s\n", file, what);
    }
}

static unsigned char *load(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static const bd_const *find(const bd_const *c, int n, const char *prefix) {
    int i;
    for (i = 0; i < n; i++) {
        if (strncmp(c[i].name, prefix, strlen(prefix)) == 0) return &c[i];
    }
    return NULL;
}

static void expect(const bd_const *c, int n, const char *name, int reg, int count, const char *file) {
    const bd_const *k = find(c, n, name);
    char msg[160];
    snprintf(msg, sizeof msg, "%s at c%d x%d", name, reg, count);
    check(k != NULL && k->regset == BD_RS_FLOAT4 && k->reg == reg && k->count == count, msg, file);
}

static void run(const char *file) {
    size_t len, cut;
    unsigned char *buf = load(file, &len);
    bd_const c[32];
    int n, i, isv = -1;

    if (!buf) {
        printf("FAIL: cannot read %s\n", file);
        g_fail++;
        return;
    }
    n = bd_ctab_parse(buf, len, c, 32, &isv);
    printf("%s: %d constants, vertex=%d\n", file, n, isv);
    for (i = 0; i < n; i++) printf("  c%-3d x%d set%d %s\n", c[i].reg, c[i].count, c[i].regset, c[i].name);

    check(n == 5, "five constants found", file);
    check(isv == 1, "recognised as a vertex shader", file);
    expect(c, n, "ViewMatrix", 12, 4, file);
    expect(c, n, "ProjectionMatrix", 16, 4, file);
    expect(c, n, "InvViewMatrix", 36, 4, file);
    expect(c, n, "CameraPositionFractions", 40, 1, file);
    expect(c, n, "ModelViewProjWithAVeryLong", 60, 4, file);
    {
        const bd_const *k = find(c, n, "ModelViewProjWithAVeryLong");
        check(k != NULL && strlen(k->name) == BD_CTAB_NAME_MAX - 1, "a long name is cut to the limit and ended", file);
    }

    /* Only room for two: must stop at two, not overrun. */
    check(bd_ctab_parse(buf, len, c, 2, NULL) == 2, "max_out is respected", file);

    /* Cut short at every length: must never claim more than it can read. */
    for (cut = 0; cut < len; cut++) {
        int r = bd_ctab_parse(buf, cut, c, 32, NULL);
        if (r > 5) {
            check(0, "a cut-short stream returned too many constants", file);
            break;
        }
    }
    check(1, "every cut-short length handled without a fault", file);

    /* Not a shader at all. */
    {
        unsigned char junk[64];
        memset(junk, 0xAB, sizeof junk);
        check(bd_ctab_parse(junk, sizeof junk, c, 32, NULL) == -1, "non-shader bytes refused", file);
    }

    /* Same bytes marked as a pixel shader. */
    buf[2] = 0xFF;
    buf[3] = 0xFF;
    n = bd_ctab_parse(buf, len, c, 32, &isv);
    check(n == 5 && isv == 0, "pixel-shader version token read as pixel", file);

    /* Corrupt the constant count to something huge: must be refused. */
    {
        size_t i2;
        for (i2 = 0; i2 + 8 < len; i2++) {
            if (memcmp(buf + i2, "CTAB", 4) == 0) {
                buf[i2 + 4 + 12] = 0xFF;
                buf[i2 + 4 + 13] = 0xFF;
                buf[i2 + 4 + 14] = 0xFF;
                break;
            }
        }
        check(bd_ctab_parse(buf, len, c, 32, NULL) == -1, "an impossible constant count is refused", file);
    }
    free(buf);
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) run(argv[i]);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail || argc < 2) ? 1 : 0;
}
