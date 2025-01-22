/*
 * globals.h
 *
 *  Created on: 2025. 1. 21.
 *      Author: User
 */

#ifndef SRC_GLOBALS_H_
#define SRC_GLOBALS_H_

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
extern volatile int isr_cnt_cdma;
extern volatile int cdmaErrCount;

extern volatile  u32 Done_ps2pl; /* Dma transfer is done */
extern volatile  u32 Error_ps2pl; /* Dma Bus Error occurs */
extern volatile int isr_cnt_cdma_ps2pl;

extern volatile u8 head;
extern volatile u8 tail;

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

#endif /* SRC_GLOBALS_H_ */
