/* SPDX-License-Identifier: BSD-3-Clause
 * dfe_types.h — the one VSPA type the kernel harnesses need.
 *
 * `__fx16` is a compiler built-in in -mvcpu mode. Declared here rather than
 * pulled from vspa-sdk/inc/vcpu.h, which is not self-contained outside the
 * firmware's own include set.
 */
#ifndef DFE_TYPES_H
#define DFE_TYPES_H

typedef __fx16 fixed16_t;
typedef struct cfixed16_t { __fx16 real; __fx16 imag; } cfixed16_t;

#endif /* DFE_TYPES_H */
