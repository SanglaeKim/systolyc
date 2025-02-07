/*
 * tpu_isr.c
 *
 *  Created on: 2025. 1. 13.
 *      Author: User
 */
//#include "xscugic.h"
#include "tpu_enet.h"
#include "tpu_isr.h"
#include "globals.h"

#define CLR_INTERRUPT (0x0U)
#define DMA_DONE      (0x2U)

volatile static	uint32_t *pTpuIntCntlReg = (uint32_t*)0xA3007D00;

void isr_even_pl2ps(void *CallbackRef) {
  if (!bPL2PS_READ_EVENT) {

	//*pTpuIntCntlReg = CLR_INTERRUPT;

	isr_cnt_even++;
	bPL2PS_READ_EVENT = true;
	rd_event_type = PL2PS_EVENT_EVEN;
//	XScuGic_Acknowledge(&xScuGic, INTERRUPT_ID_PL2PS_EVEN);
  }
}

void isr_odd_pl2ps(void *CallbackRef) {
  if (!bPL2PS_READ_EVENT) {

	//*pTpuIntCntlReg = CLR_INTERRUPT;

	isr_cnt_odd++;
	bPL2PS_READ_EVENT = true;
	rd_event_type = PL2PS_EVENT_ODD;
//	XScuGic_Acknowledge(&xScuGic, INTERRUPT_ID_PL2PS_ODD);
  }
}

void isr_cdma_pl2ps(void *CallBackRef, u32 IrqMask, int *IgnorePtr) {

  *pTpuIntCntlReg = DMA_DONE;

  bPL2PS_READ_EVENT = false;
  isr_cnt_cdma_pl2ps++;
  if (IrqMask & XAXICDMA_XR_IRQ_ERROR_MASK) {Error = TRUE;}
  if (IrqMask & XAXICDMA_XR_IRQ_IOC_MASK  ) {Done  = TRUE;}
}

void isr_cdma_ps2pl(void *CallBackRef, u32 IrqMask, int *IgnorePtr) {
  bPS2PL_EVENT = false;
  isr_cnt_cdma_ps2pl++;
  if (IrqMask & XAXICDMA_XR_IRQ_ERROR_MASK) {Error_ps2pl = TRUE;}
  if (IrqMask & XAXICDMA_XR_IRQ_IOC_MASK  ) {Done_ps2pl  = TRUE;}
}

//=================================================================
//	SCU GIC (global interrupt) 초기화
//=================================================================
int GIC_Init(XScuGic *pXScuGic) {
  int Status;
  XScuGic_Config *IntcConfig;


  XScuGic_DeviceInitialize(INTC_DEVICE_ID);

  IntcConfig = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);
  if (NULL == IntcConfig) {	return XST_FAILURE;  }

  Status = XScuGic_CfgInitialize(pXScuGic, IntcConfig, IntcConfig->CpuBaseAddress);
  if (Status != XST_SUCCESS) {	return XST_FAILURE;  }

  Xil_ExceptionRegisterHandler(
		  	    XIL_EXCEPTION_ID_IRQ_INT
			  ,(Xil_ExceptionHandler) XScuGic_InterruptHandler
			  , pXScuGic);
  Xil_ExceptionInit();

  Xil_ExceptionEnable();

  return XST_SUCCESS;
}

//=================================================================
//	Interrupt 연결
//=================================================================

int Enable_IntrruptSystem(XScuGic *pXScuGicInst, u16 IntrruptId,  Xil_ExceptionHandler InterruptHandler) {
  int Status = 0;

  //	DP_RAM 인터럽트 트리거 설정
  if (IntrruptId == INTERRUPT_ID_PL2PS_EVEN || IntrruptId == INTERRUPT_ID_PL2PS_ODD) {
	//0x3 : rising_edge, 0x0 : low level trig, 0x1 : high level trig
	XScuGic_SetPriorityTriggerType(pXScuGicInst, IntrruptId, 0x00, 0x3);
//	XScuGic_SetPriorityTriggerType(pXScuGicInst, IntrruptId, 0x08, 0x3);
  }

  Status = XScuGic_Connect(pXScuGicInst
		  	  	  	  	  , IntrruptId
						  , (Xil_ExceptionHandler) InterruptHandler
						  , (void *) pXScuGicInst);

  if (Status != XST_SUCCESS) {
	return Status;
  }

  XScuGic_Enable(pXScuGicInst, IntrruptId);

  return Status;
}

void setupInterrupt(XScuGic *pXScuGic) {
  int Status;
  //	GIC 생성 및 초기화
  GIC_Init(pXScuGic);

  //	인터럽트 설정
  //	PL to PS DP_RAM Even Interrupt 등록 (GIC에 인터럽트 연결) - PL(FPGA)에서 데이터 읽어오기 위한 인터럽트
  Enable_IntrruptSystem(pXScuGic, INTERRUPT_ID_PL2PS_EVEN,	isr_even_pl2ps);

  //	PL to PS DP_RAM Odd Interrupt 등록  (GIC에 인터럽트 연결) - PL(FPGA)에서 데이터 읽어오기 위한 인터럽트
  Enable_IntrruptSystem(pXScuGic, INTERRUPT_ID_PL2PS_ODD,	isr_odd_pl2ps);

//  Enable_IntrruptSystem(&pXScuGic, XPAR_XTTCPS_0_INTR,	(Xil_ExceptionHandler)timer_callback);
//  Enable_IntrruptSystem(&pXScuGic, XPAR_XTTCPS_0_INTR,	timer_callback);

  //	DMA 디바이스 초기화
  //	PL to PS DMA
  Status = Init_CDMA(pXScuGic, &instCDMA_PLtoPS, DEVICE_ID_CDMA0, INTERRUPT_ID_CDMA0);
  if (Status == XST_FAILURE) xil_printf("Init_CDMA Failed!!\r\n");

  Status = Init_CDMA(pXScuGic, &instCDMA_PStoPL, DEVICE_ID_CDMA1, INTERRUPT_ID_CDMA1);
  if (Status == XST_FAILURE) xil_printf("Init_CDMA Failed!!\r\n");

}

//================================================================================
//	CDMA 생성 및 인터럽트 등록
//================================================================================
int Init_CDMA(XScuGic *pXScuGicInst, XAxiCdma *pXAxiCdmaInst, u16 DeviceId,  u32 IntrId)
{
  XAxiCdma_Config *CfgPtr;
  int Status;

  // Initialize the XAxiCdma device.
  CfgPtr = XAxiCdma_LookupConfig(DeviceId);
  if (!CfgPtr) {
	return XST_FAILURE;
  }

  Status = XAxiCdma_CfgInitialize(pXAxiCdmaInst, CfgPtr, CfgPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
	return XST_FAILURE;
  }

  //	CDMA를 GIC에 연결 (CDMA 인터럽트 등록)
  Enable_CMDA_Intrrupt(pXScuGicInst, pXAxiCdmaInst, IntrId);

  // Enable all (completion/error/delay) interrupts
  XAxiCdma_IntrEnable(pXAxiCdmaInst, XAXICDMA_XR_IRQ_ALL_MASK);

  return XST_SUCCESS;
}

int Enable_CMDA_Intrrupt(XScuGic *pXScuGicInst, XAxiCdma *pXAxiCdmaInst, u32 IntrId)
{
  int Status;
  //if(IntrId == INTERRUPT_ID_CDMA0){
  //	  XScuGic_SetPriorityTriggerType(pXScuGicInst, IntrId, 0x00, 0x3);
  //}else{
  //	  XScuGic_SetPriorityTriggerType(pXScuGicInst, IntrId, 0xA0, 0x3);
  //}
  XScuGic_SetPriorityTriggerType(pXScuGicInst, IntrId, 0xA0, 0x3);

  // Connect the device driver handler that will be called when an
  // interrupt for the device occurs, the handler defined above performs
  // the specific interrupt processing for the device.

  Status = XScuGic_Connect(pXScuGicInst
		  	  	  	  , IntrId
					  , (Xil_InterruptHandler) XAxiCdma_IntrHandler
					  , pXAxiCdmaInst);

  if (Status != XST_SUCCESS) {
	return Status;
  }

  //Enable the interrupt for the DMA device.
  XScuGic_Enable(pXScuGicInst, IntrId);

  return XST_SUCCESS;
}




