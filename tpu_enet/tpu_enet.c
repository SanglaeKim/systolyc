/*
 * tpu.c
 *
 *  Created on: 2025. 1. 13.
 *      Author: User
 */

#include "tpu_enet.h"
#include "tpu_isr.h"
#include "netif/xadapter.h"
#include "lwip/tcp.h"
#include "globals.h"

#define TPU_DEBUG
#define WEIGHT_BN_BASE_ADDR (0xA3000000U)

void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* Source and Destination buffer for DMA transfer. */
static u8 DesBuffer[16][NUM_BYTES_UL] __attribute__ ((aligned (64)));

volatile  PL2PS_EVENT_t rd_event_type = PL2PS_EVENT_EVEN;
volatile  bool bPL2PS_READ_EVENT = false;

 StTpuPktHeader gStTpuPktHeader[RECV_BUFFER_DEPTH];
 static u8 recv_buffer[RECV_BUFFER_DEPTH][RECV_BUFFER_SIZE]__attribute__ ((aligned(64)));
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

void tpu_update_sram(void){
	int Status;

	if(head == tail){
		return;
	}
	u8 ucMsb =gStTpuPktHeader[tail].uiBase >> 24 & 0xFF;
	if( ucMsb == 0xA3 ){

		memcpy((void*) (uintptr_t)gStTpuPktHeader[tail].uiBase
				 ,                    &recv_buffer[tail][0]
				,                  gStTpuPktHeader[tail].uiNumBytes);
	}
	else{

		// Ensure that the address is aligned
		if ((gStTpuPktHeader[tail].uiBase % 4) != 0)
		{
			xil_printf("Error: Misaligned address detected\n");
			return;
		}
//	  Xil_DCacheFlushRange((UINTPTR) &recv_buffer[tail][0], gStTpuPktHeader[tail].uiNumBytes);
	  Status = XAxiCdma_SimpleTransfer(&instCDMA_PStoPL
			  	  	  	  	  	  	  ,  (UINTPTR) &recv_buffer[tail][0]
									   , (UINTPTR) gStTpuPktHeader[tail].uiBase
									   ,           gStTpuPktHeader[tail].uiNumBytes
									   , isr_cdma_ps2pl,
									   (void *) &instCDMA_PStoPL);
	  while (!Done_ps2pl) {
		// Wait for CDMA transfer to complete!!
	  }
	  Done_ps2pl = 0;
	  if (Status == XST_FAILURE) ps2pl_cdmaErrCount++;
	}

	tail = (tail+1) % RECV_BUFFER_DEPTH;
}

uint32_t ReadESR_EL1(void) {
    uint32_t esr_el1;

    __asm volatile ("MRS %0, ESR_EL1" : "=r" (esr_el1));
    return esr_el1;
}

//
//void Xil_SErrorAbortHandler(void *CallBackRef) {
//    uint32_t esr_el1 = ReadESR_EL1();
//
//    xil_printf("Synchronous abort detected. ESR_EL1: 0x%08X\n", esr_el1);
//
//    while (1) {
//        // Infinite loop to halt the system
//    }
//}


void tpu_update_enet(struct pbuf *p, err_t err){

	static EnTPU enTPU_STATE = E_IDLE;

	  while (p != NULL) {

		  switch(enTPU_STATE){
		  case E_IDLE:

			  //[DEFAULT]? need to check because the value is changed to false in transfer_data
			  recv_buffer_offset = 0;

			  if(err == ERR_OK){

				  u32 szStTpuPktHeader = sizeof(StTpuPktHeader);
				  memcpy(&gStTpuPktHeader[head], p->payload, szStTpuPktHeader);

				  memcpy(&recv_buffer[head][0], p->payload+szStTpuPktHeader  , p->len-szStTpuPktHeader);

				  if( p->len == gStTpuPktHeader[head].uiNumBytes + szStTpuPktHeader){
					  head   	 = (head + 1) % RECV_BUFFER_DEPTH;
				  }else{
					  recv_buffer_offset = p->len - szStTpuPktHeader;
					  enTPU_STATE 		 = E_ON_RCV;
				  }
			  }
			  break;

		  case E_ON_RCV:
			  memcpy(&recv_buffer[head][recv_buffer_offset], p->payload  , p->len);
			  recv_buffer_offset += p->len;

			  if (recv_buffer_offset >= gStTpuPktHeader[head].uiNumBytes ) {
				  recv_buffer_offset = 0;
				  head   	 = (head + 1) % RECV_BUFFER_DEPTH;
				  enTPU_STATE 	 	 = E_IDLE;
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
		while (loopRun && (totSendedSize < msgSize))
	{
		ProcessTCPEvents ();

		if (hTCP_ServerPort->snd_buf > 0)
		{
			//	전송할 사이즈 확인
			sndSize = msgSize - totSendedSize;

			if (sndSize > hTCP_ServerPort->snd_buf)
				sndSize = hTCP_ServerPort->snd_buf;


			//	전송
			rtn = tcp_write(hTCP_ServerPort, &sndBuff[totSendedSize], sndSize, 1);

			tcp_output (hTCP_ServerPort);

			if (rtn == ERR_OK)
			{
				totSendedSize = totSendedSize + sndSize;
				errCnt = 0;
			}
			else
			{
				tcp_output (hTCP_ServerPort);
				errCnt++;
			}
		}
		else
		{
			tcp_output (hTCP_ServerPort);
			errCnt++;
		}

		//	오류 처리
		if (errCnt > 20000000)
		{
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

