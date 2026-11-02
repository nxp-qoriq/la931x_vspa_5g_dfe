// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef ANT_TXRX_BUFFER_MANAGER_H_
#define ANT_TXRX_BUFFER_MANAGER_H_

#include "pair_config.h"
#include "dump_inject.h"
#include "antman_axiq.h"

//(2048+128) is the minimum extra size for FLUSH mode for LSDAC. Smaller size will get transfer error.
//(2048+832) is the minimum extra size for FLUSH mode for HSDAC. Smaller size will get transfer error.
//(4096+128) is the maximum extra size for FLUSH mode for HSDAC. larger size will get only 1 TDD pattern transmitted and VSPA core hang.
//extra size must be multiple of 64 bytes

//user configurable
//test result: 3 will get underflow easier than 2 on LA9310 at 61Msps (TX AXIQ DMA + RX AXIQ DMA + RX sym DMA from TCM, cauing TX AXIQ underflow with config 3. changing to 2 works)
#define FIFO_THRESHOLD_CONFIG_TX			2   //0,1,2,3: stands for 256/128/64/32 bytes.
#define FIFO_THRESHOLD_CONFIG_RX			2   //0,1,2,3: stands for 256/128/64/32 bytes.

#ifdef LA9310
#define SIZE_AXIQ_FIFO						512
#define SIZE_FOR_FLUSH_MODE					0               //LA9310 FLush mode doesn't work, size must be 0
#else
#define SIZE_AXIQ_FIFO						2048
#define SIZE_FOR_FLUSH_MODE					(SIZE_AXIQ_FIFO/2)	//must be a size to feed Flush mode. multiple of 512-bit burst.
#endif

#define AXIQFIFO_WIN_SIZE					4096
#define SIZE_AXIQ_THRESHOLD					(SIZE_AXIQ_FIFO/2)    // default AXIQ THRESHOLD is half of AXIQ BUFFER SIZE
#define AXIQFIFO_OFFSET_THRESHOLD_TX		(AXIQFIFO_WIN_SIZE-(SIZE_AXIQ_THRESHOLD>>FIFO_THRESHOLD_CONFIG_TX))
#define AXIQFIFO_OFFSET_THRESHOLD_RX		(AXIQFIFO_WIN_SIZE-(SIZE_AXIQ_THRESHOLD>>FIFO_THRESHOLD_CONFIG_RX))

#ifdef LA9310

	#define AXIQDMA_ID_TX0					11
	#define AXIQDMA_ID_TX1					11
	#define AXIQDMA_ID_TX2					11
	#define AXIQDMA_ID_TX3					11
	#define AXIQDMA_ID_TX4					11
	#define AXIQDMA_ID_TX5					11
	#define AXIQDMA_ID_TX(x)				11
	#define AXIQDMA_ID_RX0					1
	#define AXIQDMA_ID_RX1					2
	#define AXIQDMA_ID_RX2					3
	#define AXIQDMA_ID_RX3					4
	#define AXIQDMA_ID_RX4					5
	#define AXIQDMA_ID_RX5					6
	#define AXIQDMA_ID_RX(x)				((x)+1)
	#define AXIQFIFO_ADDR_BASE				0x44000000
	#define AXIQFIFO_ADDR_TX0				(AXIQFIFO_ADDR_BASE+0xB000)
	#define AXIQFIFO_ADDR_TX1				(AXIQFIFO_ADDR_BASE+0xB000)
	#define AXIQFIFO_ADDR_TX2				(AXIQFIFO_ADDR_BASE+0xB000)
	#define AXIQFIFO_ADDR_TX3				(AXIQFIFO_ADDR_BASE+0xB000)
	#define AXIQFIFO_ADDR_TX4				(AXIQFIFO_ADDR_BASE+0xB000)
	#define AXIQFIFO_ADDR_TX5				(AXIQFIFO_ADDR_BASE+0xB000)
	#define AXIQFIFO_ADDR_TX(x)				(AXIQFIFO_ADDR_BASE+0xB000)
	#define AXIQFIFO_ADDR_RX0				(AXIQFIFO_ADDR_BASE+0x1000)
	#define AXIQFIFO_ADDR_RX1				(AXIQFIFO_ADDR_BASE+0x2000)
	#define AXIQFIFO_ADDR_RX2				(AXIQFIFO_ADDR_BASE+0x3000)
	#define AXIQFIFO_ADDR_RX3				(AXIQFIFO_ADDR_BASE+0x4000)
	#define AXIQFIFO_ADDR_RX4				(AXIQFIFO_ADDR_BASE+0x5000)
	#define AXIQFIFO_ADDR_RX5				(AXIQFIFO_ADDR_BASE+0x6000)
	#define AXIQFIFO_ADDR_RX(x)				(AXIQFIFO_ADDR_BASE+((x)+1)*0x1000)
	#define AXIQ_GPO_TX0					7
	#define AXIQ_GPI_TX0					1
	#define AXIQ_STATUS_ENABLE_BITFIELD_TX0	(1<<16)
	#define AXIQ_STATUS_UNDERF_BITFIELD_TX0	((1<<18)|(1<<19))

#else
	#define AXIQDMA_ID_TX0					4
	#define AXIQDMA_ID_TX1					5
	#define AXIQDMA_ID_TX2					9
	#define AXIQDMA_ID_TX3					10
	#define AXIQDMA_ID_TX4					14
	#define AXIQDMA_ID_TX5					15
	#define AXIQDMA_ID_TX(x)				((x)/2*5+4+((x)%2))
	#define AXIQDMA_ID_RX0					1
	#define AXIQDMA_ID_RX1					2
	#define AXIQDMA_ID_RX2					6
	#define AXIQDMA_ID_RX3					7
	#define AXIQDMA_ID_RX4					11
	#define AXIQDMA_ID_RX5					12
	#define AXIQDMA_ID_RX(x)				((x)/2*5+1+((x)%2))
	#define AXIQFIFO_ADDR_BASE				0xD2000000
	#define AXIQFIFO_ADDR_TX0				(AXIQFIFO_ADDR_BASE+0x10000)
	#define AXIQFIFO_ADDR_TX1				(AXIQFIFO_ADDR_BASE+0x11000)
	#define AXIQFIFO_ADDR_TX2				(AXIQFIFO_ADDR_BASE+0x20000)
	#define AXIQFIFO_ADDR_TX3				(AXIQFIFO_ADDR_BASE+0x21000)
	#define AXIQFIFO_ADDR_TX4				(AXIQFIFO_ADDR_BASE+0x00000)
	#define AXIQFIFO_ADDR_TX5				(AXIQFIFO_ADDR_BASE+0x08000)
	#define AXIQFIFO_ADDR_RX0				(AXIQFIFO_ADDR_BASE+0x10000)
	#define AXIQFIFO_ADDR_RX1				(AXIQFIFO_ADDR_BASE+0x11000)
	#define AXIQFIFO_ADDR_RX2				(AXIQFIFO_ADDR_BASE+0x20000)
	#define AXIQFIFO_ADDR_RX3				(AXIQFIFO_ADDR_BASE+0x21000)
	#define AXIQFIFO_ADDR_RX4				(AXIQFIFO_ADDR_BASE+0x00000)
	#define AXIQFIFO_ADDR_RX5				(AXIQFIFO_ADDR_BASE+0x08000)
	#define AXIQFIFO_ADDR_TX(x)				(AXIQFIFO_ADDR_BASE+0x10000+(x)/2*0x10000+((x)%2)*0x1000)
	#define AXIQFIFO_ADDR_RX(x)				AXIQFIFO_ADDR_TX(x)
	#define HS_AXIQFIFO_ADDR_TX(x)			(AXIQFIFO_ADDR_BASE+0x08000*(x))
	#define HS_AXIQFIFO_ADDR_RX(x)			HS_AXIQFIFO_ADDR_TX(x)
	#define AXIQ_GPO_TX0					AXIQ0_LS_CONTROL1_GPOUT
	#define AXIQ_GPI_TX0					AXIQ0_LS_STATUS1_GPIN
	#define AXIQ_STATUS_ENABLE_BITFIELD_TX0	LS_AXIQ_STS_TX0_CH_EN_MASK
	#define AXIQ_STATUS_UNDERF_BITFIELD_TX0	(LS_AXIQ_STS_TX0_UNDERFLOW_ERR_MASK | LS_AXIQ_STS_TX0_OVERFLOW_ERR_MASK)
#endif


#if (SIZE_TX_ANT_BUFFER+SIZE_FOR_TX_AXIQ_FLUSH > (64*1024))
ERROR CONFIG	//TX/RX ANT BUFFER SIZE must not be larger than 60KB, as VSPA DMA SIZE is limited to 64KB, AXIQ FLUSH MODE needs extra size
#endif
#if (SIZE_RX_ANT_BUFFER+SIZE_RESERVED_FOR_FLUSH > (64*1024))
ERROR CONFIG	//TX/RX ANT BUFFER SIZE must not be larger than 60KB, as VSPA DMA SIZE is limited to 64KB, AXIQ FLUSH MODE needs extra size
#endif

/* VERSION history:
 * 1.0.1: Nov 26 2021. minor bug fix of software underrun status during idle state
 * 1.0 : Sep 28 2021. First version,  supporting TX only, tested on Simulator only.
 * 
 * 
 * Functionality:
 * Serves as interface between DFE output sample data and DAC. Users only need to call the API to pass the
 * DFE output samples to the API and doesn't need to do anything more. The API kernels will send the samples
 * to DAC.
 * CAUTION: Users should use FreeRTOS TBGEN APIs to initialize DAC and control tx_allowed signals according to
 *          FDD/TDD RF timing so that this APIs can work.
 * 
 * The APIs do the following:
 * >Initialize antenna TX buffers, AXIQ FIFO and AXIQ DMAs,�
 * >Manage the TX buffers, 
 * >Maintain the AXIQ DMA in pace with DCS sampling rate,
 * >Report buffer overrun/underrun 
 * >Support configurable buffer size and number of buffers, such as ping-pong or triple or more buffers.
 * 
 * USE CASE for this API:
 * 
 * FDD case:  Users keep applying for ant buffers and write DFE output data to ant buffers by calling 
 *            Ant_buffer_tx_apply_buffer() and Ant_buffer_tx_write_complete().
 * 
 * TDD case:  1. TBGEN controls the tx_allowed and rx_allowed precisely according to RF timing by TDD slot configuration
 *            2. Users apply for ant buffers and write DFE output data to ant buffers for the TX slots, and send a stop indication 
 *               by calling Ant_buffer_tx_stop();
 *            3. Before UL to DL switch point, users apply for ant buffers and write DFE output data to ant buffers for the next TX slots
 * 
 * CAUTION: In any case, Ant_buffer_tx_check_status() must be called regularly for this API to manage AXIQ DMAs, 
 *          otherwise it will cause hardware underrun.
 *          Absolute gap between 2 calls of Ant_buffer_tx_check_status() must be less than size_byte/4/DAC_rate.
 *          For size_byte=4096 bytes and 491Msps DAC sampling rate, size_byte/4/DAC_rate = 2us.
 *          size_byte is the parameter passed in Ant_buffer_tx_write_complete(size_byte).
 *          
 *          NOTE: Ant_buffer_tx_apply_buffer() and Ant_buffer_tx_write_complete() also includes all functionality of 
 *                Ant_buffer_tx_check_status(), the absolute gap requirement is met by 2 calls of any of the 3 APIs 
 *          
 * 
 */

#define NUM_SAMPLES_CLR_FOR_DAC_AFTER_TXALLOWED_FALLING		(2*FR1+8*FR2)  //not sure the num for LA9310

/******************************************************************************************************
 *                 TX APIs
 ******************************************************************************************************/
//return value: 0-success, non-zero: AXIQ error
int Ant_buffer_tx_init(unsigned int dcs_id, unsigned int iq_swap);
void Ant_buffer_tx_dma_check();
//apply for a buffer to store DFE output.
//return value: 0-fail, non-zero: success
//               users should write DFE output to the buffer with a size equal or less than size_buffer configured in Ant_buffer_tx_init()
//ptr: the buffer stat pointer successfully applied for when return value is 0
int Ant_buffer_tx_apply_buffer(struct_buf_stat_td** ptr);

//users should call this API after DFE has finished writing output to the buffer
void Ant_buffer_tx_write_complete();

//users should call this API when switching from Downlink to Uplink after all the TX symbol data has been finished by DFE and writing to Ant buffers completed.
//return value: 0-succeeded,  -1 failed due to last stop command processing has not be finished
//int Ant_buffer_tx_stop();

//check TX stop status
//return value: 1-stopped, 0: not stopped.  When TX has not stopped, users much call Ant_buffer_tx_check_status() regularly which will continue configure DMAs.
//int Ant_buffer_tx_stop_status();

//users should regularly call this API for proper functioning of the kernel
//return num of valid buffers
unsigned int Ant_buffer_tx_check_status();

void Ant_buffer_tx_error_recovery();
unsigned int get_tx_allowed_status();
//void Ant_buffer_tx_pattern_update_request(unsigned int tid, struct_static_tdd_patterns* pattern_p);
int mailbox_send_msg(unsigned int mailbox_id, unsigned int msb32, unsigned int lsb32);
/******************************************************************************************************
 *                 RX APIs
 ******************************************************************************************************/
#define IQSWAP_BITIDX(trid)			(0x0+(trid))
#define HWDCM_BITIDX				31
#define HWDCM_MASK					(1<<HWDCM_BITIDX)
#define ANT_ID_BITIDX(trid)			(0x0+(trid)*4)

//return value: 0-success, non-zero: AXIQ error
int Ant_buffer_rx_init(unsigned int dcs_id, unsigned int ctrl_iqswap);

//poll for a valid buffer with received ant data.
//return value: non-zero: success, 0 no valid buffer
//               users should call this API to poll with an interval less than (size_buffer/4*2.5) cycles (receiving interface 245Msps, 2.5 cycles each sample) 
//ptr: the buffer pointer to the received data when return value is 0
int Ant_buffer_rx_poll_buffer(struct_buf_stat_td** ptr);

//users should call this API to free the buffer after DFE has finished processing a buffer
void Ant_buffer_rx_free_buffer();


//users should regularly call this API for proper functioning of the kernel
//return value: 0-normal, non-zero:overflow
void Ant_buffer_rx_check_status();
unsigned int Ant_buffer_rx_check_status_obs();  //same function for OBS CHAN
void Ant_buffer_rx_dma_check();
void Ant_buffer_rx_dma_feed();
void Ant_buffer_rx_dma_feed_obs();

//get the number of buffers that are full with data received.
unsigned int get_rx_allowed_status();
//return value: 1-request allowed, 0-request refused
//void Ant_buffer_rx_pattern_update_request(unsigned int rid, struct_static_tdd_patterns* pattern_p);
void Ant_buffer_tx_dma_feed();
void Ant_buffer_tx_axiq_reset();
void Ant_buffer_tx_axiq_enable();
void Ant_buffer_rx_axiq_reset();
void Ant_buffer_rx_axiq_enable();

typedef struct struct_antman_ctrl_tx_s
{
	unsigned short	dcs_id;     //the highest dcs id in all dcs id
	unsigned short	num_buffers;
	unsigned short	num_DMA_in_progress;
	unsigned short 	axiq_dma_chan;
	unsigned short	num_valid_buffers;
	unsigned short	num_empty_buffers;
	unsigned int 	axiq_fifo_addr;
	unsigned short 	axiq_control_gpo;
	unsigned short	axiq_status_gpi;
	unsigned int 	axiq_control_enable_bitfield;
	unsigned int 	axiq_control_fifo_rst_clr_err_bitfield;
	unsigned int 	axiq_status_enable_bitfield;
	unsigned int 	axiq_status_rst_complete_bitfield;
	unsigned int 	axiq_status_err_bitfield;
	unsigned int 	axiq_underflow_overflow_bitfield;
	unsigned int 	tx_allowed_bitfield;
	struct_buf_stat_td	buf_stat_ant_tx[MAX_NUM_ANT_BUFFERS];
	struct_buf_stat_td*	wr_ptr_buf_stat_ant_tx;
	struct_buf_stat_td*	rd_ptr_buf_stat_ant_tx;
} struct_antman_ctrl_tx; 

typedef struct struct_antman_ctrl_rx_s
{
	unsigned short	dcs_id;												//0
	unsigned short	num_buffers;										//2
	unsigned short	num_DMA_in_progress;								//4
	unsigned short 	axiq_dma_chan;										//6
	unsigned short 	num_valid_buffers;									//8
	unsigned short	sps_div;											//0x0A
	unsigned int 	axiq_fifo_addr;										//0x0C
	unsigned short 	axiq_control_gpo;									//0x10
	unsigned short 	axiq_status_gpi;									//0x12
	unsigned int 	axiq_control_enable_bitfield;						//0x14
	unsigned int 	axiq_control_fifo_rst_clr_err_bitfield;				//0x18
	unsigned int 	axiq_status_err_bitfield;							//0x1C
	unsigned int 	axiq_status_rst_complete_bitfield;					//0x20
	unsigned int 	axiq_status_enable_bitfield;						//0x24
	unsigned int 	axiq_underflow_overflow_bitfield;					//0x28
	unsigned int 	rx_allowed_bitfield;								//0x2c
	struct_buf_stat_td	buf_stat_ant_rx[MAX_NUM_ANT_BUFFERS];			//0x30
	struct_buf_stat_td*	wr_ptr_buf_stat_ant_rx;							//0x70
	struct_buf_stat_td*	rd_ptr_buf_stat_ant_rx;							//0x74
	unsigned short	ant_buf_size;										//0x78
	unsigned short	sym_idx;											//0x7a
	unsigned int	offset_in_sframe;
	unsigned int	rxext_size;
//	unsigned int	rxext_size_remainder;
	unsigned int	skip1;
	unsigned int	rx_size;//feed_block_counter;
	unsigned short	dsr;    //down sampling rate
	unsigned short	num_ant;
} struct_antman_ctrl_rx;

#endif /* ANT_TXRX_BUFFER_MANAGER_H_ */
