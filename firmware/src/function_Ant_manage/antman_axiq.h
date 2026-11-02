// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef __AXIQ_H__
#define __AXIQ_H__

//! @brief      AXIQ bank enumeration.LOWORD
enum axiq_bank_e {
    AXIQ_BANK_L0,       //!< AXIQ low speed bank 0.
    AXIQ_BANK_L1,       //!< AXIQ low speed bank 1.
    AXIQ_BANK_HS,       //!< AXIQ high speed bank.
    AXIQ_BANK_CNT       //!< Number of AXIQ banks.
};

//! @brief      AXIQ FIFO enumeration.
enum axiq_fifo_e {
    AXIQ_FIFO_RX0,      //!< AXIQ receive FIFO 0.
    AXIQ_FIFO_RX1,      //!< AXIQ receive FIFO 1.
    AXIQ_FIFO_TX0,      //!< AXIQ transmit FIFO 0.
    AXIQ_FIFO_TX1,      //!< AXIQ transmit FIFO 1.
    AXIQ_FIFO_CNT,      //!< Number of AXIQ FIFO per bank.
};

// -----------------------------------------------------------------------------
// AXIQ FIFO interface
// -----------------------------------------------------------------------------

//! @brief      AXIQ FIFO descriptor.
struct axiq_fifo_t {
    enum axiq_bank_e bank;          //!< AXIQ bank.
    enum axiq_fifo_e fifo;          //!< AXIQ FIFO.
    struct axiq_fifo_dma_t {
        uint32_t chan;              //!< DMA channel.
        uint32_t mask;              //!< DMA mask.
    } dma;                          //!< DMA information.
    uint32_t addr;                  //!< AXI address.
};

// ----------------------------------------------------------------------------
//! @brief Geul RF AXIQ Base Address Space Mapping
// ----------------------------------------------------------------------------
#define HS_AXIQ_FIFO_BASE_ADDR       0xD2000000
#define LS_AXIQ0_FIFO_BASE_ADDR      0xD2010000
#define LS_AXIQ1_FIFO_BASE_ADDR      0xD2020000
 
// ----------------------------------------------------------------------------
//! @brief Geul RF AXIQ FIFO Address Offsets (from Base Space above)
// ----------------------------------------------------------------------------
#define HS_AXIQ_FIFO_OFST_RX0        0x0000
#define HS_AXIQ_FIFO_OFST_RX1        0x8000
#define HS_AXIQ_FIFO_OFST_TX0        0x0000
#define HS_AXIQ_FIFO_OFST_TX1        0x8000

#define LS_AXIQ_FIFO_OFST_RX0       0x0000
#define LS_AXIQ_FIFO_OFST_RX1       0x1000
#define LS_AXIQ_FIFO_OFST_TX0       0x0000
#define LS_AXIQ_FIFO_OFST_TX1       0x1000

// ----------------------------------------------------------------------------
//! @brief Geul RF AXIQ FIFO Base Addresses
// ----------------------------------------------------------------------------
#define HS_AXIQ_FIFO_ADDR_RX0       (HS_AXIQ_FIFO_BASE_ADDR + HS_AXIQ_FIFO_OFST_RX0)
#define HS_AXIQ_FIFO_ADDR_RX1       (HS_AXIQ_FIFO_BASE_ADDR + HS_AXIQ_FIFO_OFST_RX1)
#define HS_AXIQ_FIFO_ADDR_TX0       (HS_AXIQ_FIFO_BASE_ADDR + HS_AXIQ_FIFO_OFST_TX0)
#define HS_AXIQ_FIFO_ADDR_TX1       (HS_AXIQ_FIFO_BASE_ADDR + HS_AXIQ_FIFO_OFST_TX1)

#define LS_AXIQ0_FIFO_ADDR_RX0       (LS_AXIQ0_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_RX0)
#define LS_AXIQ0_FIFO_ADDR_RX1       (LS_AXIQ0_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_RX1)
#define LS_AXIQ0_FIFO_ADDR_TX0       (LS_AXIQ0_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_TX0)
#define LS_AXIQ0_FIFO_ADDR_TX1       (LS_AXIQ0_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_TX1)

#define LS_AXIQ1_FIFO_ADDR_RX0       (LS_AXIQ1_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_RX0)
#define LS_AXIQ1_FIFO_ADDR_RX1       (LS_AXIQ1_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_RX1)
#define LS_AXIQ1_FIFO_ADDR_TX0       (LS_AXIQ1_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_TX0)
#define LS_AXIQ1_FIFO_ADDR_TX1       (LS_AXIQ1_FIFO_BASE_ADDR + LS_AXIQ_FIFO_OFST_TX1)

// ----------------------------------------------------------------------------
//! @brief Geul RF AXIQ DMA Channels
// ----------------------------------------------------------------------------
#define DMA_CH_HS_AXIQ_RX0           DMAC_CHAN_HS_RX0
#define DMA_CH_HS_AXIQ_RX1           DMAC_CHAN_HS_RX1
#define DMA_CH_HS_AXIQ_TX0           DMAC_CHAN_HS_TX0
#define DMA_CH_HS_AXIQ_TX1           DMAC_CHAN_HS_TX1

#define DMA_CH_LS_AXIQ0_RX0          DMAC_CHAN_L0_RX0
#define DMA_CH_LS_AXIQ0_RX1          DMAC_CHAN_L0_RX1
#define DMA_CH_LS_AXIQ0_TX0          DMAC_CHAN_L0_TX0
#define DMA_CH_LS_AXIQ0_TX1          DMAC_CHAN_L0_TX1

#define DMA_CH_LS_AXIQ1_RX0          DMAC_CHAN_L1_RX0
#define DMA_CH_LS_AXIQ1_RX1          DMAC_CHAN_L1_RX1
#define DMA_CH_LS_AXIQ1_TX0          DMAC_CHAN_L1_TX0
#define DMA_CH_LS_AXIQ1_TX1          DMAC_CHAN_L1_TX1

// ----------------------------------------------------------------------------
//! @brief AXIQ HS Status GPIN registers no
// ----------------------------------------------------------------------------
#define AXIQ_HS_STATUS0_GPIN		 14
#define AXIQ_HS_STATUS1_GPIN	     15
#define AXIQ_HS_STATUS2_GPIN	     16

// ----------------------------------------------------------------------------
//! @brief AXIQ LS Status GPIN registers no
// ----------------------------------------------------------------------------
#define AXIQ0_LS_STATUS0_GPIN		8
#define AXIQ0_LS_STATUS1_GPIN	    9
#define AXIQ0_LS_STATUS2_GPIN	    10

#define AXIQ1_LS_STATUS0_GPIN		11
#define AXIQ1_LS_STATUS1_GPIN	    12
#define AXIQ1_LS_STATUS2_GPIN	    13

// ----------------------------------------------------------------------------
//! @brief AXIQ HS Control GPOUT registers no
// ----------------------------------------------------------------------------
#define AXIQ_HS_CONTROL0_GPOUT	     12
#define AXIQ_HS_CONTROL1_GPOUT	     13

#define AXIQ0_LS_CONTROL0_GPOUT	     8
#define AXIQ0_LS_CONTROL1_GPOUT	     9

#define AXIQ1_LS_CONTROL0_GPOUT	     10
#define AXIQ1_LS_CONTROL1_GPOUT	     11

// ----------------------------------------------------------------------------
//! @brief HS AXIQ Control Bit Defines (mapped to VSPA GPout)
// ----------------------------------------------------------------------------
#define HS_AXIQ_CTL_RX_CH_EN_B          0
#define HS_AXIQ_CTL_RX_FIFO_THRESH_LSB  1
#define HS_AXIQ_CTL_RX_SWAP_B           3
#define HS_AXIQ_CTL_RX_CLEAR_ERROR_B    4
#define HS_AXIQ_CTL_RX_RESET_FIFO_B     5
#define HS_AXIQ_CTL_RX_2G_MODE_B        6

#define HS_AXIQ_CTL_TX_CH_EN_B         16
#define HS_AXIQ_CTL_TX_FIFO_THRESH_LSB 17
#define HS_AXIQ_CTL_TX_SWAP_B          19
#define HS_AXIQ_CTL_TX_CLEAR_ERROR_B   20
#define HS_AXIQ_CTL_TX_RESET_FIFO_B    21

#define HS_AXIQ_CTL_CH_OUT_SEL_LSB     29
#define HS_AXIQ_CTL_VSPA_RESTART_RX_B  31

// ----------------------------------------------------------------------------
//! @brief LS AXIQ0 Control Bit Defines (mapped to VSPA GPout)
// ----------------------------------------------------------------------------
#define LS_AXIQ_CTL_RX0_CH_EN_B          0
#define LS_AXIQ_CTL_RX0_FIFO_THRESH_LSB  1
#define LS_AXIQ_CTL_RX0_SWAP_B           3
#define LS_AXIQ_CTL_RX0_CLEAR_ERROR_B    4
#define LS_AXIQ_CTL_RX0_RESET_FIFO_B     5
#define LS_AXIQ_CTL_RX0_COMPLEX_MODE_B   6
#define LS_AXIQ_CTL_RX0_NON_INT_MODE_B   7
#define LS_AXIQ_CTL_RX0_HALF_ADC_MODE_B  24

#define LS_AXIQ_CTL_RX1_CH_EN_B          8
#define LS_AXIQ_CTL_RX1_FIFO_THRESH_LSB  9
#define LS_AXIQ_CTL_RX1_SWAP_B           11
#define LS_AXIQ_CTL_RX1_CLEAR_ERROR_B    12
#define LS_AXIQ_CTL_RX1_RESET_FIFO_B     13
#define LS_AXIQ_CTL_RX1_COMPLEX_MODE_B   14
#define LS_AXIQ_CTL_RX1_NON_INT_MODE_B   15
#define LS_AXIQ_CTL_RX1_HALF_ADC_MODE_B  25

#define LS_AXIQ_CTL_RX2_CH_EN_B          16
#define LS_AXIQ_CTL_RX2_FIFO_THRESH_LSB  17
#define LS_AXIQ_CTL_RX2_CLEAR_ERROR_B    20
#define LS_AXIQ_CTL_RX2_RESET_FIFO_B     21

#define LS_AXIQ_CTL_TX0_CH_EN_B           0
#define LS_AXIQ_CTL_TX0_FIFO_THRESH_LSB   1
#define LS_AXIQ_CTL_TX0_SWAP_B            3
#define LS_AXIQ_CTL_TX0_CLEAR_ERROR_B     4
#define LS_AXIQ_CTL_TX0_RESET_FIFO_B      5
#define LS_AXIQ_CTL_TX0_COMPLEX_MODE_B    6

#define LS_AXIQ_CTL_TX1_CH_EN_B          8
#define LS_AXIQ_CTL_TX1_FIFO_THRESH_LSB  9
#define LS_AXIQ_CTL_TX1_SWAP_B           11
#define LS_AXIQ_CTL_TX1_CLEAR_ERROR_B    12
#define LS_AXIQ_CTL_TX1_RESET_FIFO_B     13
#define LS_AXIQ_CTL_TX1_COMPLEX_MODE_B   14

#define LS_AXIQ_CTL_CH_OUT_SEL_LSB     28
#define LS_AXIQ_CTL_VSPA_RESTART_RX_B  24


// ----------------------------------------------------------------------------
//! @brief HS AXIQ Control Bit Masks Defines (mapped to VSPA GPout)
// ----------------------------------------------------------------------------
#define HS_AXIQ_CTL_RX_CH_EN_MASK          (0x1 << HS_AXIQ_CTL_RX_CH_EN_B)
#define HS_AXIQ_CTL_RX_FIFO_THRESH_MASK    (0x3 << HS_AXIQ_CTL_RX_FIFO_THRESH_LSB)
#define HS_AXIQ_CTL_RX_SWAP_MASK           (0x1 << HS_AXIQ_CTL_RX_SWAP_B)
#define HS_AXIQ_CTL_RX_CLEAR_ERROR_MASK    (0x1 << HS_AXIQ_CTL_RX_CLEAR_ERROR_B)
#define HS_AXIQ_CTL_RX_RESET_FIFO_MASK     (0x1 << HS_AXIQ_CTL_RX_RESET_FIFO_B)
#define HS_AXIQ_CTL_RX_2G_MODE_MASK        (0x1 << HS_AXIQ_CTL_RX_2G_MODE_B)

#define HS_AXIQ_CTL_TX_CH_EN_MASK          (0x1 << HS_AXIQ_CTL_TX_CH_EN_B)
#define HS_AXIQ_CTL_TX_FIFO_THRESH_MASK    (0x3 << HS_AXIQ_CTL_TX_FIFO_THRESH_LSB)
#define HS_AXIQ_CTL_TX_SWAP_MASK           (0x1 << HS_AXIQ_CTL_TX_SWAP_B)
#define HS_AXIQ_CTL_TX_CLEAR_ERROR_MASK    (0x1 << HS_AXIQ_CTL_TX_CLEAR_ERROR_B)
#define HS_AXIQ_CTL_TX_RESET_FIFO_MASK     (0x1 << HS_AXIQ_CTL_TX_RESET_FIFO_B)

#define HS_AXIQ_CTL_CH_OUT_SEL_MASK        (0x3 << HS_AXIQ_CTL_CH_OUT_SEL_LSB)
#define HS_AXIQ_CTL_VSPA_RESTART_RX_MASK   (0x1 << HS_AXIQ_CTL_VSPA_RESTART_RX_B)

// ----------------------------------------------------------------------------
//! @brief LS AXIQ Control Bit Masks Defines (mapped to VSPA GPout)
// ----------------------------------------------------------------------------
#define LS_AXIQ_CTL_RX0_CH_EN_MASK          (0x1 << LS_AXIQ_CTL_RX0_CH_EN_B)
#define LS_AXIQ_CTL_RX0_FIFO_THRESH_MASK    (0x3 << LS_AXIQ_CTL_RX0_FIFO_THRESH_LSB)
#define LS_AXIQ_CTL_RX0_SWAP_MASK           (0x1 << LS_AXIQ_CTL_RX0_SWAP_B)
#define LS_AXIQ_CTL_RX0_CLEAR_ERROR_MASK    (0x1 << LS_AXIQ_CTL_RX0_CLEAR_ERROR_B)
#define LS_AXIQ_CTL_RX0_RESET_FIFO_MASK     (0x1 << LS_AXIQ_CTL_RX0_RESET_FIFO_B)
#define LS_AXIQ_CTL_RX0_COMPLEX_MODE_MASK   (0x1 << LS_AXIQ_CTL_RX0_COMPLEX_MODE_B)
#define LS_AXIQ_CTL_RX0_NON_INT_MODE_MASK   (0x1 << LS_AXIQ_CTL_RX0_NON_INT_MODE_B)
#define LS_AXIQ_CTL_RX0_HALF_ADC_MODE_MASK  (0x1 << LS_AXIQ_CTL_RX0_HALF_ADC_MODE_B)

#define LS_AXIQ_CTL_RX1_CH_EN_MASK          (0x1 << LS_AXIQ_CTL_RX1_CH_EN_B)
#define LS_AXIQ_CTL_RX1_FIFO_THRESH_MASK    (0x3 << LS_AXIQ_CTL_RX1_FIFO_THRESH_LSB)
#define LS_AXIQ_CTL_RX1_SWAP_MASK           (0x1 << LS_AXIQ_CTL_RX1_SWAP_B)
#define LS_AXIQ_CTL_RX1_CLEAR_ERROR_MASK    (0x1 << LS_AXIQ_CTL_RX1_CLEAR_ERROR_B)
#define LS_AXIQ_CTL_RX1_RESET_FIFO_MASK     (0x1 << LS_AXIQ_CTL_RX1_RESET_FIFO_B)
#define LS_AXIQ_CTL_RX1_COMPLEX_MODE_MASK   (0x1 << LS_AXIQ_CTL_RX1_COMPLEX_MODE_B)
#define LS_AXIQ_CTL_RX1_NON_INT_MODE_MASK   (0x1 << LS_AXIQ_CTL_RX1_NON_INT_MODE_B)
#define LS_AXIQ_CTL_RX1_HALF_ADC_MODE_MASK  (0x1 << LS_AXIQ_CTL_RX1_HALF_ADC_MODE_B)

#define LS_AXIQ_CTL_RX2_CH_EN_MASK          (0x1 << LS_AXIQ_CTL_RX2_CH_EN_B)
#define LS_AXIQ_CTL_RX2_FIFO_THRESH_MASK    (0x3 << LS_AXIQ_CTL_RX2_FIFO_THRESH_LSB)
#define LS_AXIQ_CTL_RX2_CLEAR_ERROR_MASK    (0x1 << LS_AXIQ_CTL_RX2_CLEAR_ERROR_B)
#define LS_AXIQ_CTL_RX2_RESET_FIFO_MASK     (0x1 << LS_AXIQ_CTL_RX2_RESET_FIFO_B)

#define LS_AXIQ_CTL_TX0_CH_EN_MASK          (0x1 << LS_AXIQ_CTL_TX0_CH_EN_B)
#define LS_AXIQ_CTL_TX0_FIFO_THRESH_MASK    (0x3 << LS_AXIQ_CTL_TX0_FIFO_THRESH_LSB)
#define LS_AXIQ_CTL_TX0_SWAP_MASK           (0x1 << LS_AXIQ_CTL_TX0_SWAP_B)
#define LS_AXIQ_CTL_TX0_CLEAR_ERROR_MASK    (0x1 << LS_AXIQ_CTL_TX0_CLEAR_ERROR_B)
#define LS_AXIQ_CTL_TX0_RESET_FIFO_MASK     (0x1 << LS_AXIQ_CTL_TX0_RESET_FIFO_B)
#define LS_AXIQ_CTL_TX0_COMPLEX_MODE_MASK   (0x1 << LS_AXIQ_CTL_TX0_COMPLEX_MODE_B)

#define LS_AXIQ_CTL_TX1_CH_EN_MASK          (0x1 << LS_AXIQ_CTL_TX1_CH_EN_B)
#define LS_AXIQ_CTL_TX1_FIFO_THRESH_MASK    (0x3 << LS_AXIQ_CTL_TX1_FIFO_THRESH_LSB)
#define LS_AXIQ_CTL_TX1_SWAP_MASK           (0x1 << LS_AXIQ_CTL_TX1_SWAP_B)
#define LS_AXIQ_CTL_TX1_CLEAR_ERROR_MASK    (0x1 << LS_AXIQ_CTL_TX1_CLEAR_ERROR_B)
#define LS_AXIQ_CTL_TX1_RESET_FIFO_MASK     (0x1 << LS_AXIQ_CTL_TX1_RESET_FIFO_B)
#define LS_AXIQ_CTL_TX1_COMPLEX_MODE_MASK   (0x1 << LS_AXIQ_CTL_TX1_COMPLEX_MODE_B)

#define LS_AXIQ_CTL_CH_OUT_SEL_MASK         (0x3 << LS_AXIQ_CTL_CH_OUT_SEL_LSB)
#define LS_AXIQ_CTL_VSPA_RESTART_RX_MASK    (0x1 << LS_AXIQ_CTL_VSPA_RESTART_RX_B)

// ----------------------------------------------------------------------------
//! @brief HS AXIQ Status1 Bit Defines (mapped to VSPA GPin)
// ----------------------------------------------------------------------------
#define HS_AXIQ_STS_RX0_CH_EN_B             0
#define HS_AXIQ_STS_RX0_FIFO_NOT_EMPTY_B    1
#define HS_AXIQ_STS_RX0_CH_ALLOWED_B        2
#define HS_AXIQ_STS_RX0_FIFO_RST_COMPL_B    3
#define HS_AXIQ_STS_RX0_UNDERFLOW_ERR_B     4
#define HS_AXIQ_STS_RX0_OVERFLOW_ERR_B      5
#define HS_AXIQ_STS_RX0_FIFO_RST_ERR_B      6

#define HS_AXIQ_STS_TX0_CH_EN_B             8
#define HS_AXIQ_STS_TX0_FIFO_NOT_FULL_B     9
#define HS_AXIQ_STS_TX0_CH_ALLOWED_B       10
#define HS_AXIQ_STS_TX0_FIFO_RST_COMPL_B   11
#define HS_AXIQ_STS_TX0_UNDERFLOW_ERR_B    12
#define HS_AXIQ_STS_TX0_OVERFLOW_ERR_B     13
#define HS_AXIQ_STS_TX0_FIFO_RST_ERR_B     14
#define HS_AXIQ_STS_CFG0_LOAD_IN_PROGR_B   15

#define HS_AXIQ_STS_RX1_CH_EN_B            16
#define HS_AXIQ_STS_RX1_FIFO_NOT_EMPTY_B   17
#define HS_AXIQ_STS_RX1_CH_ALLOWED_B       18
#define HS_AXIQ_STS_RX1_FIFO_RST_COMPL_B   19
#define HS_AXIQ_STS_RX1_UNDERFLOW_ERR_B    20
#define HS_AXIQ_STS_RX1_OVERFLOW_ERR_B     21
#define HS_AXIQ_STS_RX1_FIFO_RST_ERR_B     22

#define HS_AXIQ_STS_TX1_CH_EN_B            24
#define HS_AXIQ_STS_TX1_FIFO_NOT_FULL_B    25
#define HS_AXIQ_STS_TX1_CH_ALLOWED_B       26
#define HS_AXIQ_STS_TX1_FIFO_RST_COMPL_B   27
#define HS_AXIQ_STS_TX1_UNDERFLOW_ERR_B    28
#define HS_AXIQ_STS_TX1_OVERFLOW_ERR_B     29
#define HS_AXIQ_STS_TX1_FIFO_RST_ERR_B     30
#define HS_AXIQ_STS_CFG1_LOAD_IN_PROGR_B   31

// ----------------------------------------------------------------------------
//! @brief LS AXIQ Status1 Bit Defines (mapped to VSPA GPin)
// ----------------------------------------------------------------------------
#define LS_AXIQ_STS_RX0_CH_EN_B             0
#define LS_AXIQ_STS_RX0_FIFO_NOT_EMPTY_B    1
#define LS_AXIQ_STS_RX0_CH_ALLOWED_B        2
#define LS_AXIQ_STS_RX0_FIFO_RST_COMPL_B    3
#define LS_AXIQ_STS_RX0_UNDERFLOW_ERR_B     4
#define LS_AXIQ_STS_RX0_OVERFLOW_ERR_B      5
#define LS_AXIQ_STS_RX0_FIFO_RST_ERR_B      6

#define LS_AXIQ_STS_RX1_CH_EN_B            8
#define LS_AXIQ_STS_RX1_FIFO_NOT_EMPTY_B   9
#define LS_AXIQ_STS_RX1_CH_ALLOWED_B       10
#define LS_AXIQ_STS_RX1_FIFO_RST_COMPL_B   11
#define LS_AXIQ_STS_RX1_UNDERFLOW_ERR_B    12
#define LS_AXIQ_STS_RX1_OVERFLOW_ERR_B     13
#define LS_AXIQ_STS_RX1_FIFO_RST_ERR_B     14

#define LS_AXIQ_STS_RX2_CH_EN_B            16
#define LS_AXIQ_STS_RX2_FIFO_NOT_EMPTY_B   17
#define LS_AXIQ_STS_RX2_CH_ALLOWED_B       18
#define LS_AXIQ_STS_RX2_FIFO_RST_COMPL_B   19
#define LS_AXIQ_STS_RX2_UNDERFLOW_ERR_B    20
#define LS_AXIQ_STS_RX2_OVERFLOW_ERR_B     21
#define LS_AXIQ_STS_RX2_FIFO_RST_ERR_B     22
#define LS_AXIQ_STS_CFG0_LOAD_IN_PROGR_B   31

#define LS_AXIQ_STS_TX0_CH_EN_B             0
#define LS_AXIQ_STS_TX0_FIFO_NOT_FULL_B     1
#define LS_AXIQ_STS_TX0_CH_ALLOWED_B        2
#define LS_AXIQ_STS_TX0_FIFO_RST_COMPL_B    3
#define LS_AXIQ_STS_TX0_UNDERFLOW_ERR_B     4
#define LS_AXIQ_STS_TX0_OVERFLOW_ERR_B      5
#define LS_AXIQ_STS_TX0_FIFO_RST_ERR_B      6

#define LS_AXIQ_STS_TX1_CH_EN_B             8
#define LS_AXIQ_STS_TX1_FIFO_NOT_FULL_B     9
#define LS_AXIQ_STS_TX1_CH_ALLOWED_B        10
#define LS_AXIQ_STS_TX1_FIFO_RST_COMPL_B    11
#define LS_AXIQ_STS_TX1_UNDERFLOW_ERR_B     12
#define LS_AXIQ_STS_TX1_OVERFLOW_ERR_B      13
#define LS_AXIQ_STS_TX1_FIFO_RST_ERR_B      14
#define LS_AXIQ_STS_CFG1_LOAD_IN_PROGR_B    31

// ----------------------------------------------------------------------------
//! @brief HS AXIQ Status1 Bit Masks Defines (mapped to VSPA GPin)
// ----------------------------------------------------------------------------
#define HS_AXIQ_STS_RX0_CH_EN_MASK             (0x1 << HS_AXIQ_STS_RX0_CH_EN_B)
#define HS_AXIQ_STS_RX0_FIFO_NOT_EMPTY_MASK    (0x1 << HS_AXIQ_STS_RX0_FIFO_NOT_EMPTY_B)
#define HS_AXIQ_STS_RX0_CH_ALLOWED_MASK        (0x1 << HS_AXIQ_STS_RX0_CH_ALLOWED_B)
#define HS_AXIQ_STS_RX0_FIFO_RST_COMPL_MASK    (0x1 << HS_AXIQ_STS_RX0_FIFO_RST_COMPL_B)
#define HS_AXIQ_STS_RX0_UNDERFLOW_ERR_MASK     (0x1 << HS_AXIQ_STS_RX0_UNDERFLOW_ERR_B)
#define HS_AXIQ_STS_RX0_OVERFLOW_ERR_MASK      (0x1 << HS_AXIQ_STS_RX0_OVERFLOW_ERR_B)
#define HS_AXIQ_STS_RX0_FIFO_RST_ERR_MASK      (0x1 << HS_AXIQ_STS_RX0_FIFO_RST_ERR_B)

#define HS_AXIQ_STS_TX0_CH_EN_MASK             (0x1 << HS_AXIQ_STS_TX0_CH_EN_B)
#define HS_AXIQ_STS_TX0_FIFO_NOT_FULL_MASK     (0x1 << HS_AXIQ_STS_TX0_FIFO_NOT_FULL_B)
#define HS_AXIQ_STS_TX0_CH_ALLOWED_MASK        (0x1 << HS_AXIQ_STS_TX0_CH_ALLOWED_B)
#define HS_AXIQ_STS_TX0_FIFO_RST_COMPL_MASK    (0x1 << HS_AXIQ_STS_TX0_FIFO_RST_COMPL_B)
#define HS_AXIQ_STS_TX0_UNDERFLOW_ERR_MASK     (0x1 << HS_AXIQ_STS_TX0_UNDERFLOW_ERR_B)
#define HS_AXIQ_STS_TX0_OVERFLOW_ERR_MASK      (0x1 << HS_AXIQ_STS_TX0_OVERFLOW_ERR_B)
#define HS_AXIQ_STS_TX0_FIFO_RST_ERR_MASK      (0x1 << HS_AXIQ_STS_TX0_FIFO_RST_ERR_B)
#define HS_AXIQ_STS_CFG0_LOAD_IN_PROGR_MASK    (0x1 << HS_AXIQ_STS_CFG0_LOAD_IN_PROGR_B)

#define HS_AXIQ_STS_RX1_CH_EN_MASK             (0x1 << HS_AXIQ_STS_RX1_CH_EN_B)
#define HS_AXIQ_STS_RX1_FIFO_NOT_EMPTY_MASK    (0x1 << HS_AXIQ_STS_RX1_FIFO_NOT_EMPTY_B)
#define HS_AXIQ_STS_RX1_CH_ALLOWED_MASK        (0x1 << HS_AXIQ_STS_RX1_CH_ALLOWED_B)
#define HS_AXIQ_STS_RX1_FIFO_RST_COMPL_MASK    (0x1 << HS_AXIQ_STS_RX1_FIFO_RST_COMPL_B)
#define HS_AXIQ_STS_RX1_UNDERFLOW_ERR_MASK     (0x1 << HS_AXIQ_STS_RX1_UNDERFLOW_ERR_B)
#define HS_AXIQ_STS_RX1_OVERFLOW_ERR_MASK      (0x1 << HS_AXIQ_STS_RX1_OVERFLOW_ERR_B)
#define HS_AXIQ_STS_RX1_FIFO_RST_ERR_MASK      (0x1 << HS_AXIQ_STS_RX1_FIFO_RST_ERR_B)

#define HS_AXIQ_STS_TX1_CH_EN_MASK             (0x1 << HS_AXIQ_STS_TX1_CH_EN_B)
#define HS_AXIQ_STS_TX1_FIFO_NOT_FULL_MASK     (0x1 << HS_AXIQ_STS_TX1_FIFO_NOT_FULL_B)
#define HS_AXIQ_STS_TX1_CH_ALLOWED_MASK        (0x1 << HS_AXIQ_STS_TX1_CH_ALLOWED_B)
#define HS_AXIQ_STS_TX1_FIFO_RST_COMPL_MASK    (0x1 << HS_AXIQ_STS_TX1_FIFO_RST_COMPL_B)
#define HS_AXIQ_STS_TX1_UNDERFLOW_ERR_MASK     (0x1 << HS_AXIQ_STS_TX1_UNDERFLOW_ERR_B)
#define HS_AXIQ_STS_TX1_OVERFLOW_ERR_MASK      (0x1 << HS_AXIQ_STS_TX1_OVERFLOW_ERR_B)
#define HS_AXIQ_STS_TX1_FIFO_RST_ERR_MASK      (0x1 << HS_AXIQ_STS_TX1_FIFO_RST_ERR_B)
#define HS_AXIQ_STS_CFG1_LOAD_IN_PROGR_MASK    (0x1 << HS_AXIQ_STS_CFG1_LOAD_IN_PROGR_B)
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//! @brief LS AXIQ Status1 Bit Masks Defines (mapped to VSPA GPin)
// ----------------------------------------------------------------------------
#define LS_AXIQ_STS_RX0_CH_EN_MASK             (0x1 << LS_AXIQ_STS_RX0_CH_EN_B)
#define LS_AXIQ_STS_RX0_FIFO_NOT_EMPTY_MASK    (0x1 << LS_AXIQ_STS_RX0_FIFO_NOT_EMPTY_B)
#define LS_AXIQ_STS_RX0_CH_ALLOWED_MASK        (0x1 << LS_AXIQ_STS_RX0_CH_ALLOWED_B)
#define LS_AXIQ_STS_RX0_FIFO_RST_COMPL_MASK    (0x1 << LS_AXIQ_STS_RX0_FIFO_RST_COMPL_B)
#define LS_AXIQ_STS_RX0_UNDERFLOW_ERR_MASK     (0x1 << LS_AXIQ_STS_RX0_UNDERFLOW_ERR_B)
#define LS_AXIQ_STS_RX0_OVERFLOW_ERR_MASK      (0x1 << LS_AXIQ_STS_RX0_OVERFLOW_ERR_B)
#define LS_AXIQ_STS_RX0_FIFO_RST_ERR_MASK      (0x1 << LS_AXIQ_STS_RX0_FIFO_RST_ERR_B)

#define LS_AXIQ_STS_TX0_CH_EN_MASK             (0x1 << LS_AXIQ_STS_TX0_CH_EN_B)
#define LS_AXIQ_STS_TX0_FIFO_NOT_FULL_MASK     (0x1 << LS_AXIQ_STS_TX0_FIFO_NOT_FULL_B)
#define LS_AXIQ_STS_TX0_CH_ALLOWED_MASK        (0x1 << LS_AXIQ_STS_TX0_CH_ALLOWED_B)
#define LS_AXIQ_STS_TX0_FIFO_RST_COMPL_MASK    (0x1 << LS_AXIQ_STS_TX0_FIFO_RST_COMPL_B)
#define LS_AXIQ_STS_TX0_UNDERFLOW_ERR_MASK     (0x1 << LS_AXIQ_STS_TX0_UNDERFLOW_ERR_B)
#define LS_AXIQ_STS_TX0_OVERFLOW_ERR_MASK      (0x1 << LS_AXIQ_STS_TX0_OVERFLOW_ERR_B)
#define LS_AXIQ_STS_TX0_FIFO_RST_ERR_MASK      (0x1 << LS_AXIQ_STS_TX0_FIFO_RST_ERR_B)
#define LS_AXIQ_STS_CFG0_LOAD_IN_PROGR_MASK    (0x1 << LS_AXIQ_STS_CFG0_LOAD_IN_PROGR_B)


#define LS_AXIQ_STS_RX1_CH_EN_MASK             (0x1 << LS_AXIQ_STS_RX1_CH_EN_B)
#define LS_AXIQ_STS_RX1_FIFO_NOT_EMPTY_MASK    (0x1 << LS_AXIQ_STS_RX1_FIFO_NOT_EMPTY_B)
#define LS_AXIQ_STS_RX1_CH_ALLOWED_MASK        (0x1 << LS_AXIQ_STS_RX1_CH_ALLOWED_B)
#define LS_AXIQ_STS_RX1_FIFO_RST_COMPL_MASK    (0x1 << LS_AXIQ_STS_RX1_FIFO_RST_COMPL_B)
#define LS_AXIQ_STS_RX1_UNDERFLOW_ERR_MASK     (0x1 << LS_AXIQ_STS_RX1_UNDERFLOW_ERR_B)
#define LS_AXIQ_STS_RX1_OVERFLOW_ERR_MASK      (0x1 << LS_AXIQ_STS_RX1_OVERFLOW_ERR_B)
#define LS_AXIQ_STS_RX1_FIFO_RST_ERR_MASK      (0x1 << LS_AXIQ_STS_RX1_FIFO_RST_ERR_B)

#define LS_AXIQ_STS_TX1_CH_EN_MASK             (0x1 << LS_AXIQ_STS_TX1_CH_EN_B)
#define LS_AXIQ_STS_TX1_FIFO_NOT_FULL_MASK     (0x1 << LS_AXIQ_STS_TX1_FIFO_NOT_FULL_B)
#define LS_AXIQ_STS_TX1_CH_ALLOWED_MASK        (0x1 << LS_AXIQ_STS_TX1_CH_ALLOWED_B)
#define LS_AXIQ_STS_TX1_FIFO_RST_COMPL_MASK    (0x1 << LS_AXIQ_STS_TX1_FIFO_RST_COMPL_B)
#define LS_AXIQ_STS_TX1_UNDERFLOW_ERR_MASK     (0x1 << LS_AXIQ_STS_TX1_UNDERFLOW_ERR_B)
#define LS_AXIQ_STS_TX1_OVERFLOW_ERR_MASK      (0x1 << LS_AXIQ_STS_TX1_OVERFLOW_ERR_B)
#define LS_AXIQ_STS_TX1_FIFO_RST_ERR_MASK      (0x1 << LS_AXIQ_STS_TX1_FIFO_RST_ERR_B)
#define LS_AXIQ_STS_CFG1_LOAD_IN_PROGR_MASK    (0x1 << LS_AXIQ_STS_CFG1_LOAD_IN_PROGR_B)

// ----------------------------------------------------------------------------


//! @brief      Initialize an AXIQ FIFO descriptor.
//! @param      this    The AXIQ FIFO descriptor address.
//! @param      bank    The AXIQ bank identifier.
//! @param      fifo    The AXIQ FIFO identifier.
//! @return     This function does not return a value.
extern void axiq_fifo_init(struct axiq_fifo_t *this, enum axiq_bank_e bank,
    enum axiq_fifo_e fifo);

//! @brief      Enable an AXIQ FIFO.
//! @param      this    The AXIQ FIFO descriptor address.
//! @return     This function does not return a value.
extern void axiq_fifo_enable(struct axiq_fifo_t const *this);

//! @brief      Disable an AXIQ FIFO.
//! @param      this    The AXIQ FIFO descriptor address.
//! @return     This function does not return a value.
extern void axiq_fifo_disable(struct axiq_fifo_t const *this);

//! @brief      Reset the pointers of an AXIQ FIFO.
//! @param      this    The AXIQ FIFO descriptor address.
//! @return     This function does not return a value.
extern void axiq_fifo_reset(struct axiq_fifo_t const *this);

//! @brief      Read data from an AXIQ FIFO.
//! @param      this    The AXIQ FIFO descriptor address.
//! @param      data    The AXI-aligned read buffer address.
//! @param      size    The number of bytes to read.
//! @return     This function does not return a value.
static inline void axiq_fifo_rd(struct axiq_fifo_t const *this, void *data,
    size_t size)
{

}

//! @brief      Write data to an AXIQ FIFO.
//! @param      this    The AXIQ FIFO descriptor address.
//! @param      data    The AXI-aligned write buffer address.
//! @param      size    The number of bytes to write.
//! @return     This function does not return a value.
static inline void axiq_fifo_wr(struct axiq_fifo_t const *this,
    void const *data, size_t size)
{

}

#endif // __AXIQ_H__
