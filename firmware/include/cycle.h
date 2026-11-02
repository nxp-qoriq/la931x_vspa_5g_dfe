// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef CYCLE_MSG_H_
#define CYCLE_MSG_H_

static inline ccnt_t ccnt_read_msb16(void) 
{
    return iord(CYC_CNTR_MSB, 0x0000FFFF);
}

__inline unsigned int get_timestamp32()
{
	ccnt_read_msb16();
	return ccnt_read_lsb32();
}

__inline unsigned short get_timestamp16msb()
{
	return ccnt_read_msb16();
}

#endif /* CYCLE_MSG_H_ */
