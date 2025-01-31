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

#define DES_BUFFER_DEPTH (8)

void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* Source and Destination buffer for DMA transfer. */
static u8 DesBuffer[DES_BUFFER_DEPTH][16][NUM_BYTES_UL] __attribute__ ((aligned (64)));
static uint32_t des_buffer_head = 0; 
static uint32_t des_buffer_tail = 0; 

volatile  PL2PS_EVENT_t rd_event_type = PL2PS_EVENT_EVEN;
volatile  bool bPL2PS_READ_EVENT = false;

//StTpuPktHeader gStTpuPktHeader[RECV_BUFFER_DEPTH];


static const u32 sramBaseAddr[PL2PS_EVENT_MAX] = { SRAM_ADDR_UL_EVEN, SRAM_ADDR_UL_ODD };

void tpu_update_isr(u32 *pFileNameIndex){

  int Status;

  if (bPL2PS_READ_EVENT) {

	u32 SrcAddr = sramBaseAddr[rd_event_type];
	uint32_t des_buffer_head_ = des_buffer_head % DES_BUFFER_DEPTH;

	Xil_AssertVoid(des_buffer_head - des_buffer_tail < (DES_BUFFER_DEPTH));

	Status = XAxiCdma_SimpleTransfer(&instCDMA_PLtoPS
									 ,  (UINTPTR) SrcAddr
									 , (UINTPTR) &DesBuffer[des_buffer_head_][*pFileNameIndex][0]
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
	  des_buffer_head++;
	}
  }
}

void tpu_upload(){

  if (des_buffer_head > des_buffer_tail) {

	uint32_t des_buffer_tail_ = des_buffer_tail % DES_BUFFER_DEPTH;
		
	// After DMA, invalidate cache to load from DDR
	Xil_DCacheInvalidateRange((UINTPTR) &DesBuffer[des_buffer_tail_], sizeof(DesBuffer[0]));

	//tpu_upload_data(&DesBuffer[0][0], sizeof(DesBuffer));
	tpu_upload_data((u8*)&DesBuffer[des_buffer_tail_], sizeof(DesBuffer[0]));
	des_buffer_tail++;

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

//void tpu_update_enet(struct pbuf *p, err_t err, enTpuState *penTpuState ){

void tpu_update_enet(enTpuState *penTpuState ){

  const u32 szStTpuPktHeader = sizeof(StTpuPktHeader);

  Xil_AssertVoid(head - tail < RECV_BUFFER_DEPTH);
  u32 head_ = head % RECV_BUFFER_DEPTH;
  uint32_t rcv_buffer_tail_ = rcv_buffer_tail % RCV_BUFFER_SIZE;

  switch(*penTpuState){
  case E_GET_HEAD:{
		
	// Do we have enough for Header?
	if (rcv_buffer_head >= rcv_buffer_tail + szStTpuPktHeader) {

	  // Are we in the boundary?
	  if ((rcv_buffer_tail_ + szStTpuPktHeader) > RCV_BUFFER_SIZE) {
		uint32_t partialNumBytes = RCV_BUFFER_SIZE - rcv_buffer_tail_;
		memcpy(&g_StTpuPktArr[head_], &rcv_buffer[rcv_buffer_tail_], partialNumBytes);
		memcpy(&g_StTpuPktArr[head_]+partialNumBytes, &rcv_buffer[0], szStTpuPktHeader -partialNumBytes);
	  }else{
		memcpy(&g_StTpuPktArr[head_], &rcv_buffer[rcv_buffer_tail_], szStTpuPktHeader);
	  }
	  rcv_buffer_tail += szStTpuPktHeader;
	  *penTpuState = E_GET_PAYLOAD;
	}
  }
	break;

  case E_GET_PAYLOAD:{

	uint32_t uiNumBytes = g_StTpuPktArr[head_].stTpuPktHeader.uiNumBytes;
	// Do we have enough for Header?
	if (rcv_buffer_head >= rcv_buffer_tail + uiNumBytes) {


	  // Are we in the boundary?
	  if (rcv_buffer_tail_ + uiNumBytes > RCV_BUFFER_SIZE) {
		uint32_t partialNumBytes = RCV_BUFFER_SIZE - rcv_buffer_tail_;
		memcpy(&g_StTpuPktArr[head_].ucPayloadArr[0], &rcv_buffer[rcv_buffer_tail_], partialNumBytes);
		memcpy(&g_StTpuPktArr[head_].ucPayloadArr[0]+partialNumBytes, &rcv_buffer[0], uiNumBytes -partialNumBytes);
	  }else{
		memcpy(&g_StTpuPktArr[head_].ucPayloadArr[0], &rcv_buffer[rcv_buffer_tail_], uiNumBytes);
	  }
	  rcv_buffer_tail += uiNumBytes;
	  *penTpuState = E_GET_CHKSUM;
	}
  }
	break;

  case E_GET_CHKSUM:{
		

	// Do we have enough for Header?
	if (rcv_buffer_head >= rcv_buffer_tail + CHKSUM_SIZE) {

	  // Are we in the boundary?
	  if (rcv_buffer_tail_ + CHKSUM_SIZE > RCV_BUFFER_SIZE) {
		uint32_t partialNumBytes = RCV_BUFFER_SIZE - rcv_buffer_tail_;
		memcpy(&g_StTpuPktArr[head_].uiChksum, &rcv_buffer[rcv_buffer_tail_], partialNumBytes);
		memcpy(&g_StTpuPktArr[head_].uiChksum+partialNumBytes, &rcv_buffer[0], CHKSUM_SIZE -partialNumBytes);
	  }else{
		memcpy(&g_StTpuPktArr[head_].uiChksum, &rcv_buffer[rcv_buffer_tail_], CHKSUM_SIZE);
	  }
	  uint32_t uiNumBytes = g_StTpuPktArr[head_].stTpuPktHeader.uiNumBytes;

	  uint32_t uiRcvChecksum = g_StTpuPktArr[head_].uiChksum;
	  uint32_t uiCalChecksum = crc32((uint8_t*)&g_StTpuPktArr[head_], uiNumBytes+szStTpuPktHeader);
	  if (uiRcvChecksum != uiCalChecksum) {
		xil_printf("[Checksum ERROR] cal: x0%x, rcv: 0x%x\r\n", uiCalChecksum ,uiRcvChecksum);
	  }else{
		head++;
	  }
	  rcv_buffer_tail += CHKSUM_SIZE;
	  *penTpuState = E_GET_HEAD;
	}
  }
	break;
  default:
	break;
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

