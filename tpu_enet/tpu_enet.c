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

//#define ENABLE_CHK_CRC
//#define ENABLE_CHK_CRC

#define NOE(arr) (sizeof(arr)/sizeof(arr[0]))

#define TPU_DEBUG
#define WEIGHT_BN_BASE_ADDR (0xA3000000U)

#define DES_BUFFER_DEPTH (4)

void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* Source and Destination buffer for DMA transfer. */
static u8 DesBuffer[DES_BUFFER_DEPTH][16][NUM_BYTES_UL] __attribute__ ((aligned (64)));
static uint32_t des_buffer_head = 0; 
static uint32_t des_buffer_tail = 0; 

volatile  PL2PS_EVENT_t rd_event_type = PL2PS_EVENT_EVEN;
volatile  bool bPL2PS_READ_EVENT = false;

static const u32 sramBaseAddr[PL2PS_EVENT_MAX] = { SRAM_ADDR_UL_EVEN, SRAM_ADDR_UL_ODD };

void tpu_enet_receive(enTpuState *penTpuState )
{

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
#ifdef ENABLE_CHK_BASE
	  if(!isTheBaseValid(uiBase, wBases, NOE(wBases)) && 
		 !isTheBaseValid(uiBase, bBases, NOE(bBases)) &&
		 !isTheBaseValid(uiBase, iBases, NOE(iBases))) {
		xil_printf("[ERROR - 0x%X] Invalid Address!!", uiBase);
		return;
	  }
#endif

	  rcv_buffer_tail += szStTpuPktHeader;
	  *penTpuState = E_GET_PAYLOAD;
	}
  } break;

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
  } break;

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

#ifdef ENABLE_CHK_CRC
	  uint32_t uiNumBytes = g_StTpuPktArr[head_].stTpuPktHeader.uiNumBytes;
	  uint32_t uiRcvChecksum = g_StTpuPktArr[head_].uiChksum;
	  uint32_t uiCalChecksum = crc32((uint8_t*)&g_StTpuPktArr[head_], uiNumBytes+szStTpuPktHeader);
	  if (uiRcvChecksum != uiCalChecksum) {
		xil_printf("[Checksum ERROR] cal: x0%x, rcv: 0x%x\r\n", uiCalChecksum ,uiRcvChecksum);
	  }
#endif
	  head++;
	  rcv_buffer_tail += CHKSUM_SIZE;
	  *penTpuState = E_GET_HEAD;
	}
  } break;

  default:{*penTpuState = E_GET_HEAD;} break;

  }
}

void tpu_update_sram(void)
{
  int Status;

  if(head == tail){return;}

  u32 tail_ = tail % RECV_BUFFER_DEPTH;

  uint32_t uiBase     = g_StTpuPktArr[tail_].stTpuPktHeader.uiBase;
  uint32_t uiNumBytes = g_StTpuPktArr[tail_].stTpuPktHeader.uiNumBytes;
  u8       ucMsb      = uiBase >> 24 & 0xFF;

  if( ucMsb == 0xA3 ) {

#ifdef ENABLE_CHK_BASE
	if(!isTheBaseValid(uiBase, wBases, NOE(wBases)) && 
	   !isTheBaseValid(uiBase, bBases, NOE(bBases))) {
	  xil_printf("[ERROR - 0x%X] Invalid Address!!", uiBase);
	  return;
	}
#endif

	memcpy((void*) (uintptr_t)uiBase
		   ,  &g_StTpuPktArr[tail_].ucPayloadArr[0]
		   ,   uiNumBytes);
  }else{

#ifdef ENABLE_CHK_BASE
	if(!isTheBaseValid(uiBase, iBases, NOE(iBases))) {
	  xil_printf("[ERROR - 0x%X] Invalid Address!!", uiBase);
	  return;
	}
#endif

	// Ensure that the address is aligned
	if ((uiBase % 4) != 0) {
	  xil_printf("Error: Misaligned address detected\n");
	  return;
	}

	Xil_DCacheFlushRange((UINTPTR) &g_StTpuPktArr[tail_].ucPayloadArr[0], uiNumBytes);
	Status = XAxiCdma_SimpleTransfer(&instCDMA_PStoPL
									 , (UINTPTR) &g_StTpuPktArr[tail_].ucPayloadArr[0]
									 , (UINTPTR) uiBase
									 ,           uiNumBytes
									 , isr_cdma_ps2pl
									 , (void *) &instCDMA_PStoPL);
	while (!Done_ps2pl) {
	  // Wait for CDMA transfer to complete!!
	}
	Done_ps2pl = 0;
	if (Status == XST_FAILURE) ps2pl_cdmaErrCount++;
  }
  tail++;
}

void tpu_cdma_pl2ps(uint32_t *pFileNameIndex)
{
  static int counter = 0;
  int Status;

  if (bPL2PS_READ_EVENT) {

	// local copy
	uint32_t fileNameIndex = *pFileNameIndex;
	PL2PS_EVENT_t rd_event_type_ = rd_event_type;
	uint32_t isr_cnt_odd_        = isr_cnt_odd;
	uint32_t isr_cnt_even_       = isr_cnt_even;

	u32 SrcAddr = sramBaseAddr[rd_event_type_];
	uint32_t des_buffer_head_ = des_buffer_head % DES_BUFFER_DEPTH;


	Xil_AssertVoid(des_buffer_head - des_buffer_tail < (DES_BUFFER_DEPTH));

	uint32_t retries = 16;

	while(retries){
	  retries -= 1;
	  
	  Status = XAxiCdma_SimpleTransfer(&instCDMA_PLtoPS
									   , (UINTPTR) SrcAddr
									   , (UINTPTR) &DesBuffer[des_buffer_head_][fileNameIndex % 16][0]
									   , NUM_BYTES_UL
									   , isr_cdma_pl2ps,
									   (void *) &instCDMA_PLtoPS);
	  if (Status == XST_SUCCESS) {
		break; 
	  }
	}

	if (!retries) {
	  xil_printf("[ERROR] : retries!!\r\n");
	  return;
	}

	while (!Done) {
	  // Wait for CDMA transfer to complete!!
	}
	Done = 0;

	if (Status == XST_FAILURE) cdmaErrCount++;

	*pFileNameIndex = *pFileNameIndex + 1;
	if (((*pFileNameIndex) % 16) == 0) {
	  if(isr_cnt_even_ != isr_cnt_odd_){
		xil_printf("[ERROR] isr_cnt_even: %d, isr_cnt_odd: %d\r\n", isr_cnt_even_, isr_cnt_odd_);
	  }
	  if(++counter % 10 == 0){
		xil_printf("%d\r\n", counter);
	  }
	  *pFileNameIndex = 0;
	  des_buffer_head++;  
	}
  }
}

void tpu_enet_send()
{

  if (des_buffer_head > des_buffer_tail) {

	uint32_t des_buffer_tail_ = des_buffer_tail % DES_BUFFER_DEPTH;

	// After DMA, invalidate cache to load from DDR
	Xil_DCacheInvalidateRange((UINTPTR) &DesBuffer[des_buffer_tail_], sizeof(DesBuffer[0]));

		tpu_upload_data((u8*)&DesBuffer[des_buffer_tail_], sizeof(DesBuffer[0]));
	des_buffer_tail++;

  }


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

void displayAppInfo(void)
{
  xil_printf("\033[2J\033[H"); // clear screen
  xil_printf("\r\n--- Entering main() --- \r\n");
  printf("-- Build Info --\r\nFile Name: %s\r\nDate: %s\r\nTime: %s\r\n",	__FILE__, __DATE__, __TIME__);
}
