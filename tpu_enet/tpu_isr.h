/*
 * tpu_isr.h
 *
 *  Created on: 2025. 1. 13.
 *      Author: User
 */

#ifndef SRC_TPU_ISR_H_
#define SRC_TPU_ISR_H_

#include "xscugic.h"
#include "xaxicdma.h"

#define INTERRUPT_ID_PL2PS_EVEN  	(122U)	//	PL to PS	even intr
#define INTERRUPT_ID_PL2PS_ODD		(123U)	//	PL to PS	odd  intr
#define INTERRUPT_ID_PS2PL			(124U)	//	PS to PL	input feature map

/******************** Constant Definitions **********************************/
#define DEVICE_ID_CDMA0 	XPAR_AXICDMA_0_DEVICE_ID
#define DEVICE_ID_CDMA1 	XPAR_AXICDMA_1_DEVICE_ID

#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define INTERRUPT_ID_CDMA0	XPAR_FABRIC_AXICDMA_0_VEC_ID
#define INTERRUPT_ID_CDMA1	XPAR_FABRIC_AXICDMA_1_VEC_ID

typedef enum {
	PL2PS_EVENT_EVEN, PL2PS_EVENT_ODD, PL2PS_EVENT_MAX
} PL2PS_EVENT_t;

void isr_even_pl2ps(void *CallbackRef);
void isr_odd_pl2ps(void *CallbackRef);
void isr_cdma(void *CallBackRef, u32 IrqMask, int *IgnorePtr) ;
void isr_cdma_ps2pl(void *CallBackRef, u32 IrqMask, int *IgnorePtr);


int GIC_Init(XScuGic *hGICInstPtr);

int Enable_CMDA_Intrrupt(XScuGic *IntcInstancePtr, XAxiCdma *InstancePtr, u32 IntrId) ;

int Init_CDMA(XScuGic *IntcInstancePtr, XAxiCdma *InstancePtr, u16 DeviceId,
			  u32 IntrId);

int Enable_IntrruptSystem(XScuGic *hGICInstPtr, u16 IntrruptId,
						  Xil_ExceptionHandler InterruptHandler);

int Init_CDMA(XScuGic *IntcInstancePtr, XAxiCdma *InstancePtr, u16 DeviceId,
			  u32 IntrId) ;

int GIC_Init(XScuGic *hGICInstPtr);

void displayAppInfo(void);

void setupInterrupt(XScuGic *pXScuGic);

#endif /* SRC_TPU_ISR_H_ */
