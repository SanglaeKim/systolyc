/*
 * globals.h
 *
 *  Created on: 2025. 1. 21.
 *      Author: User
 */

#ifndef SRC_GLOBALS_H_
#define SRC_GLOBALS_H_


#include "xscugic.h"
#include "xaxicdma.h"
#include "tpu_isr.h"

extern u8 enet_rcv_buffer[ENET_RCV_BUFFER_SIZE]__attribute__ ((aligned(64)));
extern volatile uint64_t enet_rcv_buffer_head;
extern volatile uint64_t enet_rcv_buffer_tail;

extern  enTpuState g_enTpuState;

extern StTpuPkt g_StTpuPktArr[PKT_BUFFER_SIZE] __attribute__ ((aligned (64)));

extern u32 wBases[6];
extern u32 wFileSize[6];

// batch normalize
extern const u32 bBases[6];
extern const u32 bFileSize[6];

// input feature map?
extern const u32 iBases[16];
extern const u32 iFileSize[16];

extern XScuGic xScuGic; /* Instance of the Interrupt Controller */

extern XAxiCdma instCDMA_PLtoPS; /* Instance of the XAxiCdma */
extern XAxiCdma instCDMA_PStoPL; /* Instance of the XAxiCdma */

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;

extern struct netif *echo_netif;

extern volatile  u32 Done ; /* Dma transfer is done */
extern volatile  u32 Error; /* Dma Bus Error occurs */

extern volatile int isr_cnt_even;
extern volatile int isr_cnt_odd ;
extern volatile int isr_cnt_cdma_pl2ps;
extern volatile int cdmaErrCount;

extern volatile  u32 Done_ps2pl; /* Dma transfer is done */
extern volatile  u32 Error_ps2pl; /* Dma Bus Error occurs */
extern volatile int isr_cnt_cdma_ps2pl;

extern volatile u32 tpu_pkt_head;
extern volatile u32 tpu_pkt_tail;

extern u32 g_uiTimerCallbackCount;
extern bool g_bOnTimerCallback;

extern struct 	tcp_pcb *hTCP_ServerPort;	//	TCP 서버 포트

extern volatile u32 qBufDiff;
extern volatile u32 pl_dl_count;
extern int ps2pl_cdmaErrCount;

extern volatile  PL2PS_EVENT_t rd_event_type;
extern volatile  bool bPL2PS_READ_EVENT;

extern volatile  bool bPS2PL_EVENT;

extern bool g_bOnTimerCallback;
extern u32 g_uiTimerCallbackCount;

extern u32 fileNameIndex;

extern bool g_bOnTickHandler;
extern u32 g_uiOnTickHandler;

extern volatile u32 g_uiE_ON_RCV_COUNT;

#endif /* SRC_GLOBALS_H_ */
