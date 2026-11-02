// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __PHCOM_H__
#define __PHCOM_H__

//  sample_out: half fixed, line aligned
//  sample_in: half fixed, line aligned
//  coeff: pointer to the coeff for this symbol, 8 bytes aligned, each coeff has I and Q, float.
//  nL: number of input sample lines
void phcom_asm(void *sample_out, void* sample_in, void* coeff, unsigned int nL);

#endif // __PHCOM_H__


