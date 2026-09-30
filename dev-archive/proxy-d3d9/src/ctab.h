/* ctab.h - read a D3D9 shader's constant table (CTAB): which named constant
 * sits in which register.
 *
 * Blood Dragon binds shader parameters BY NAME (the Far Cry 2 names:
 * ViewMatrix, InvViewMatrix, ViewProjectionMatrix, ModelViewProj, ...), so a
 * register seen in one shader says nothing about the next. The table travels
 * inside the bytecode handed to CreateVertexShader, so the proxy can read it
 * there, before the shader is used.
 *
 * Written for this project on 2026-09-30, after the Alan Wake proxy's ctab.c
 * (staging/alan-wake-vr/proxy-d3d9), which proved the approach but only
 * extracts Alan Wake's own names. This one returns every constant.
 *
 * Dependency-free (no windows.h, no d3d9.h, no CRT string calls) so
 * tools/ctab_test.c can run it on the host. D3D9 passes no length with the
 * bytecode, so every read is bounds-checked against a cap: this parses data
 * supplied by the game and must never be what faults.
 */
#ifndef BD_CTAB_H
#define BD_CTAB_H

#include <stddef.h>

/* Register sets, as D3DXREGISTER_SET. */
#define BD_RS_BOOL    0
#define BD_RS_INT4    1
#define BD_RS_FLOAT4  2
#define BD_RS_SAMPLER 3

/* Longest constant name kept; longer names are cut (and still NUL-ended). */
#define BD_CTAB_NAME_MAX 48

/* Hard cap on one shader's size, in bytes, for a stream with no end token. */
#define BD_CTAB_MAX_SHADER_BYTES (512 * 1024)

typedef struct {
    char name[BD_CTAB_NAME_MAX];
    int  regset;   /* BD_RS_* */
    int  reg;      /* first register */
    int  count;    /* registers used */
} bd_const;

/* Parses the constant table out of a shader token stream (`bytecode` points
 * at the version token). Writes up to `max_out` constants into `out` and
 * returns how many were written; returns -1 when there is no readable CTAB.
 * `is_vertex` (may be NULL) is set to 1 for a vertex shader, 0 otherwise. */
int bd_ctab_parse(const void *bytecode, size_t max_bytes, bd_const *out, int max_out, int *is_vertex);

#endif
