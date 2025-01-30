/*
 * tpu.c
 *
 *  Created on: 2025. 1. 13.
 *      Author: User
 */
#include <stdlib.h>

#include "tpu_enet.h"
#include "tpu_isr.h"
#include "netif/xadapter.h"
#include "lwip/tcp.h"
#include "tpu_crc.h"
#include "globals.h"

#define NOE(arr) (sizeof(arr)/sizeof(arr[0]))

#define TPU_DEBUG
#define WEIGHT_BN_BASE_ADDR (0xA3000000U)

void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* Source and Destination buffer for DMA transfer. */
static u8 DesBuffer[16][NUM_BYTES_UL] __attribute__ ((aligned (64)));

volatile  PL2PS_EVENT_t rd_event_type = PL2PS_EVENT_EVEN;
volatile  bool bPL2PS_READ_EVENT = false;

//StTpuPktHeader gStTpuPktHeader[RECV_BUFFER_DEPTH];
//static u8 recv_buffer[RECV_BUFFER_DEPTH][RECV_BUFFER_SIZE]__attribute__ ((aligned(64)));
static u32 recv_buffer_offset = 0;


static const u32 sramBaseAddr[PL2PS_EVENT_MAX] = { SRAM_ADDR_UL_EVEN, SRAM_ADDR_UL_ODD };

void tpu_update_isr(u32 *pFileNameIndex){

  int Status;

  if (bPL2PS_READ_EVENT) {

	u32 SrcAddr = sramBaseAddr[rd_event_type];

	Status = XAxiCdma_SimpleTransfer(&instCDMA_PLtoPS
									 ,  (UINTPTR) SrcAddr
									 , (UINTPTR) &DesBuffer[*pFileNameIndex][0]
									 , NUM_BYTES_UL
									 , isr_cdma,
									 (void *) &instCDMA_PLtoPS);

	while (!Done) {
	  // Wait for CDMA transfer to complete!!
	}
	Done = 0;

	if (Status == XST_FAILURE) cdmaErrCount++;

	if ((++(*pFileNameIndex) % 16) == 0) {
	  *pFileNameIndex = 0;

	  // After DMA, invalidate cache to load from DDR
	  Xil_DCacheInvalidateRange((UINTPTR) &DesBuffer, sizeof(DesBuffer));

	  tpu_upload_data(&DesBuffer[0][0], sizeof(DesBuffer));

	}
  }
}

//bool isTheBaseValid(u32 Base, u32 Bases[], u32 numElements, u32 *puiBase)
bool isTheBaseValid(u32 Base, u32 Bases[], u32 numElements)
{
  bool ret = false;
  for(int i=0; i<numElements; i++){
	if(Base == Bases[i]){
	  ret = true;
	  break;
	}
  }
  return ret;
}

void tpu_update_sram(void)
{
  int Status;

  if(head == tail){return;}

  u32 tail_ = tail % RECV_BUFFER_DEPTH;

  //u8 ucMsb = gStTpuPktHeader[tail_].uiBase >> 24 & 0xFF;
  uint32_t uiBase = g_StTpuPktArr[tail_].stTpuPktHeader.uiBase;
  uint32_t uiNumBytes = g_StTpuPktArr[tail_].stTpuPktHeader.uiNumBytes;
  u8 ucMsb =  uiBase >> 24 & 0xFF;

  if( ucMsb == 0xA3 ) {

	if(!isTheBaseValid(uiBase, wBases, NOE(wBases)) && 
	   !isTheBaseValid(uiBase, bBases, NOE(bBases))) {
	  xil_printf("[ERROR - 0x%X] Invalid Address!!", uiBase);
	  return;
	}

	memcpy((void*) (uintptr_t)uiBase
		   ,                    &g_StTpuPktArr[tail_].ucPayloadArr[0]
		   ,                  uiNumBytes);
  }else{

	if(!isTheBaseValid(uiBase, iBases, NOE(iBases))) {
	  xil_printf("[ERROR - 0x%X] Invalid Address!!", uiBase);
	  return;
	}

	// Ensure that the address is aligned
	if ((uiBase % 4) != 0) {
	  xil_printf("Error: Misaligned address detected\n");
	  return;
	}
	//Xil_DCacheFlushRange((UINTPTR) &recv_buffer[tail_][0], gStTpuPktHeader[tail_].uiNumBytes);
	Xil_DCacheFlushRange((UINTPTR) &g_StTpuPktArr[tail_].ucPayloadArr[0], uiNumBytes);
	Status = XAxiCdma_SimpleTransfer(&instCDMA_PStoPL
									 ,  (UINTPTR) &g_StTpuPktArr[tail_].ucPayloadArr[0]
									 , (UINTPTR) uiBase
									 ,           uiNumBytes
									 , isr_cdma_ps2pl,
									 (void *) &instCDMA_PStoPL);
	while (!Done_ps2pl) {
	  // Wait for CDMA transfer to complete!!
	}
	Done_ps2pl = 0;
	if (Status == XST_FAILURE) ps2pl_cdmaErrCount++;
  }

  //	tail = (tail+1) % RECV_BUFFER_DEPTH;
  tail++;
}

void tpu_update_enet(struct pbuf *p, err_t err){
  if(err != ERR_OK){
	return;
  }

  const u32 szStTpuPktHeader = sizeof(StTpuPktHeader);
  static EnTPU enTPU_STATE = E_IDLE;

  while (p != NULL) {
	u32 head_ = head % RECV_BUFFER_DEPTH;

	switch(enTPU_STATE){
	case E_IDLE:

	  recv_buffer_offset = 0;


	  if(p->len >= szStTpuPktHeader){

		// copy only header 
		memcpy(&g_StTpuPktArr[head_], p->payload, szStTpuPktHeader);

		uint32_t uiBase = g_StTpuPktArr[head_].stTpuPktHeader.uiBase;
		if(!isTheBaseValid(uiBase, wBases, NOE(wBases)) &&
		   !isTheBaseValid(uiBase, bBases, NOE(bBases)) &&
		   !isTheBaseValid(uiBase, iBases, NOE(iBases))) {
		  xil_printf("[ERROR - 0x%X] Invalid Address!!", uiBase);
		  return;
		}

		uint32_t uiNumBytes = g_StTpuPktArr[head_].stTpuPktHeader.uiNumBytes;

		// check if we have header+payload+checksum all
		if (p->len >= (szStTpuPktHeader+uiNumBytes+CHECKSUM_SIZE)) {
		  // if so, copy 1. payload
		  uint32_t uiRcvChecksum; 
		  memcpy(&g_StTpuPktArr[head_].ucPayloadArr[0] , p->payload+szStTpuPktHeader , uiNumBytes);

		  //  2. checksum
		  memcpy(&uiRcvChecksum , p->payload + (p->len-CHECKSUM_SIZE) , CHECKSUM_SIZE);

		  uint32_t uiCalChecksum = crc32((uint8_t*)&g_StTpuPktArr[head_], p->len-CHECKSUM_SIZE);
		  if (uiRcvChecksum != uiCalChecksum) {
			xil_printf("[Checksum ERROR] cal: x0%x, rcv: 0x%x\r\n", uiCalChecksum ,uiRcvChecksum);
		  }
		  head++;
		}else{
		  // if so, copy 1. payload
		  uint32_t uiPayloadPartial = p->len - szStTpuPktHeader;
		  memcpy(&g_StTpuPktArr[head_].ucPayloadArr[0] , p->payload+szStTpuPktHeader , uiPayloadPartial);
		  recv_buffer_offset = uiPayloadPartial;
		  g_uiE_ON_RCV_COUNT = 0;
		  enTPU_STATE 		 = E_ON_RCV;
		}
	  }else{
		xil_printf("[%d : ERROR - Header Size too small]\r\n");
	  }
	  break;

	case E_ON_RCV:
	  if (g_uiE_ON_RCV_COUNT>4) {
		recv_buffer_offset = 0;
		enTPU_STATE = E_IDLE;
		xil_printf("[ERROR]g_uiE_ON_RCV_COUNT: %d", g_uiE_ON_RCV_COUNT);
	  }

	  uint32_t uiTotalRcvPayload = recv_buffer_offset + p->len;
	  uint32_t uiNumBytes = g_StTpuPktArr[head_].stTpuPktHeader.uiNumBytes;

	  if (uiTotalRcvPayload >= uiNumBytes+CHECKSUM_SIZE) {
		uint32_t uiRcvChecksum; 
		// copy remaining payload
		memcpy(&g_StTpuPktArr[head_].ucPayloadArr[recv_buffer_offset] , p->payload, p->len-CHECKSUM_SIZE);
		// copy checksum
		memcpy(&uiRcvChecksum , p->payload + (p->len-CHECKSUM_SIZE) , CHECKSUM_SIZE);

		uint32_t uiCalChecksum = crc32((uint8_t*)&g_StTpuPktArr[head_], uiNumBytes+szStTpuPktHeader);
		if (uiRcvChecksum != uiCalChecksum) {
		  xil_printf("[Checksum ERROR] cal: x0%x, rcv: 0x%x\r\n", uiCalChecksum ,uiRcvChecksum);
		}

		recv_buffer_offset = 0;
		head++;
		enTPU_STATE 	 	 = E_IDLE;
	  }else{
		memcpy(&g_StTpuPktArr[head_].ucPayloadArr[recv_buffer_offset] , p->payload, p->len);
		recv_buffer_offset += p->len;
	  }
	  break;
	default:
	  break;
	}

	// Free the pbuf and move to the next
	struct pbuf *next = p->next;
	pbuf_free(p);
	p = next;
  }

}

void displayAppInfo(void) {
  xil_printf("\033[2J\033[H"); // clear screen
  xil_printf("\r\n--- Entering main() --- \r\n");
  printf("-- Build Info --\r\nFile Name: %s\r\nDate: %s\r\nTime: %s\r\n",	__FILE__, __DATE__, __TIME__);
}

//=========================================================================
//	TCP 전송 함수
//=========================================================================
int tpu_upload_data(u8 *sndBuff, int msgSize)
{
  int rtn = 0;
  int errCnt = 0;
  int sndSize = 0;
  int totSendedSize = 0;
  int loopRun = 1;

  //	while (loopRun && TCP_Conn_Status && (totSendedSize < msgSize))
  while (loopRun && (totSendedSize < msgSize)) {
	ProcessTCPEvents ();

	if (hTCP_ServerPort->snd_buf > 0) {
	  //	전송할 사이즈 확인
	  sndSize = msgSize - totSendedSize;

	  if (sndSize > hTCP_ServerPort->snd_buf)
		sndSize = hTCP_ServerPort->snd_buf;


	  //	전송
	  rtn = tcp_write(hTCP_ServerPort, &sndBuff[totSendedSize], sndSize, 1);

	  tcp_output (hTCP_ServerPort);

	  if (rtn == ERR_OK) {
		totSendedSize = totSendedSize + sndSize;
		errCnt = 0;
	  } else {
		tcp_output (hTCP_ServerPort);
		errCnt++;
	  }
	} else {
	  tcp_output (hTCP_ServerPort);
	  errCnt++;
	}

	//	오류 처리
	if (errCnt > 20000000) {
	  xil_printf("Transfer error : %d\n", rtn);
	  //TCP_Conn_Status = 0;
	  //			tcp_server_abort (hTCP_ServerPort);
	  loopRun = 0;
	  break;
	}
  }
  //	송신 성공 확인
  if (totSendedSize == msgSize)
	rtn = totSendedSize;

  return rtn;
}

void ProcessTCPEvents(){
  if (TcpFastTmrFlag) {
	tcp_fasttmr();
	TcpFastTmrFlag = 0;
  }
  if (TcpSlowTmrFlag) {
	tcp_slowtmr();
	TcpSlowTmrFlag = 0;
  }
  xemacif_input(echo_netif);
}

