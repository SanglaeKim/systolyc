/*
 * tpu_timer.h
 *
 *  Created on: 2025. 1. 19.
 *      Author: User
 */

#ifndef SRC_TPU_TIMER_H_
#define SRC_TPU_TIMER_H_

#include <stdbool.h>

#include "xscugic.h"
#include "xttcps.h"
#include "xil_exception.h"
#include "platform.h"

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are only defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define TTC_TICK_DEVICE_ID	XPAR_XTTCPS_0_DEVICE_ID
#define TTC_TICK_INTR_ID	XPAR_XTTCPS_0_INTR

#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID

//#define ETH_LINK_DETECT_INTERVAL (4U)

/**************************** Type Definitions *******************************/
typedef struct {
	u32 OutputHz;	/* Output frequency */
	XInterval Interval;	/* Interval value */
	u8 Prescaler;	/* Prescaler value */
	u16 Options;	/* Option settings */
} TmrCntrSetup;

int SetupPWM(void);

int SetupTicker(XScuGic *pXScuGicInst, XTtcPs *pXTtcPsInst);
int SetupTimer(XTtcPs *pXTtcPs, int DeviceID);

void TickHandler(void *CallBackRef);
void PWMHandler(void *CallBackRef);
int WaitForDutyCycleFull(void);



#endif /* SRC_TPU_TIMER_H_ */
