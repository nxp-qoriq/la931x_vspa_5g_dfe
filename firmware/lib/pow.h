// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __POW_H__
#define __POW_H__

#define SIZE_BYTE_POW_ACC	128

/* pow_acc_asm: accumulate the sample power. sum(In*In+Qn*Qn)
 * sample_in: pointer input samples, line aligned
 * acc_inout: pointer to power accumulator buffer, size must be SIZE_BYTE_POW_ACC, line aligned.
 * nL:        num of lines of input samples
 */
void pow_acc_asm(void* sample_in, void *acc_inout, unsigned int nL);

/* pow_sum_asm: sum up all the power values in acc_inout. input is half, output is float
 * num_points: num of power values in acc_inout, should be 32.
 * return value: the toal power of all samples, float.
 */
unsigned int pow_sum_asm(void* acc_in, unsigned int num_points);

#endif // __POW_H__


