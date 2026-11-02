// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP



#ifndef SINGLE_TONE_H_
#define SINGLE_TONE_H_

//Use the following to set frequency of the single tone.
//f: frequency needed
//r: sampling rate
//exampling: to get 10Khz@122.88Msps, set f=10000,r=122880000
#define SET_SINGLE_TONE_FREQ(f,r)	(4294967296.0*(f)/(r))
/**********************************************************************************
void single_tone_asm(__fx16* buf_dest, unsigned int freq, unsigned int phase, unsigned int num_lines);
function: generate single tone to specified address
void single_tone_add_asm(void* buf_dest, void* buf_src, unsigned int freq, unsigned int phase, unsigned int amplitude, unsigned int num_lines);
function: generate single tone and add it to previous existing waveform data from buf_src, store the mixed waveform
          to buf_dest. This is used to generate multiple single tones.
buf_dest: the buffer address where to put the single tone samples, 128 bytes aligned
buf_src:  the buffer address where to fetch previous single tone samples, 128 bytes aligned
freq: freqency of the single tone, freq = 0x20000000 will generate single tone with the period = 8 samples, which are 0/45/90/135/180/225/270/315 degree.
phase: phase, which sample in the period is the starting sample.
amplitude: amplitude of the single tone. bit15-0(LSB): amplitude for I, bit 31-16: amplitude for Q
           valid range 0-0x7FFF, 0x7FFF will drive DAC to full scale,
num_lines: number of lines of data to generate, 1 line is 128 bytes.  Sample format: half fixed, 16bit I and Q. each line contains 32 samples.
***********************************************************************************/
void single_tone_asm(void* buf, unsigned int freq, unsigned int phase, unsigned int amplitude, unsigned int num_lines);
void single_tone_add_asm(void* buf_dest, void* buf_src, unsigned int freq, unsigned int phase, unsigned int amplitude, unsigned int num_lines);
/*
To get phase continuous if the function is called multiple time for same single tone,
phase should be set to the next sample of previous call
example:  to generate 10Khz for 122.88Msps with amplitude full scale, the buffer size is 64KB=16K samples
	single_tone_asm(buf, SET_SINGLE_TONE_FREQ(10000,122880000), 0, 0x7fff, 65536/128);  //generate first 64KB single tone
	//send the first 64KB single tone data to DAC ant buffer here
	single_tone_asm(buf, SET_SINGLE_TONE_FREQ(10000,122880000), 16384, 0x7fff, 65536/128);  //generate 2nd 64KB single tone with phase continuous with first 64KB
	//send the 2nd 64KB single tone data to DAC ant buffer here
	single_tone_asm(buf, SET_SINGLE_TONE_FREQ(10000,122880000), 16384*2, 0x7fff, 65536/128);  //generate 3rd 64KB single tone with phase continuous with first 64KB
	//send the 3rd 64KB single tone data to DAC ant buffer here, and repeat for ever if needed
*/





//inverse I Q, amplitude adjustment of I Q. LSB16 of IQ_factor controls I(even position), MSB16 controls Q(odd position), each 16bit is in half format.
//inverse I, set IQ_factor to 0x3c00bc00
//inverse Q, set IQ_factor to 0xbc003c00
//inverse I&Q, set IQ_factor to 0xbc00bc00
//adjust IQ amplitude, set IQ_factor to any value between (-1,1). If value is negative, polarity will be inverted. 
//out and in are IQ in half_fixed format,IQ_factor in half format, num_line is number of input or output lines
void IQ_control_asm(void* out, void* in, unsigned int IQ_factor, unsigned int num_line);
//out and in are IQ in single format,IQ_factor in half format, num_line is number of input or output lines
//this function can be used to scale up or down any data in single format, and can scale even and odd positions with different factor
void IQ_control_f32_asm(void* out, void* in, unsigned int IQ_factor, unsigned int num_line);
//in and out are half, others are same as above
void IQ_control_f16_asm(void* out, void* in, unsigned int IQ_factor, unsigned int num_line);
unsigned int vspa_log2(float data);
#endif /* SINGLE_TONE_H_ */
