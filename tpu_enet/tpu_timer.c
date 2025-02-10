/*
 * tpu_timer.c
 *
 *  Created on: 2025. 1. 19.
 *      Author: User
 */
#include "xaxicdma.h"

//#include "lwip/tcp.h"
#include "netif/xadapter.h"

#include "tpu_isr.h"
#include "tpu_enet.h"
#include "tpu_timer.h"
#include "globals.h"

/************************** Variable Definitions *****************************/
static TmrCntrSetup SettingsTable[2] = {
	{4, 0, 0, 0},	/* Ticker timer counter initial setup, only output freq */
	{4, 0, 0, 0}, /* PWM timer counter initial setup, only output freq */
};

static volatile u32 PWM_UpdateFlag;	/* Flag used by Ticker to signal PWM */
static volatile u8 ErrorCount;		/* Errors seen at interrupt time */
static volatile u32 TickCount;		/* Ticker interrupts between PWM change */

/****************************************************************************/
/**
*
* This function sets up the Ticker timer.
*
* @param	None
*
* @return	XST_SUCCESS if everything sets up well, XST_FAILURE otherwise.
*
* @note		None
*
*****************************************************************************/
int SetupTicker(XScuGic *pXScuGic, XTtcPs *pXTtcPs)
{
	int Status;
	TmrCntrSetup *TimerSetup;

	TimerSetup = &(SettingsTable[TTC_TICK_DEVICE_ID]);

	/*
	 * Set up appropriate options for Ticker: interval mode without
	 * waveform output.
	 */
	TimerSetup->Options |= (XTTCPS_OPTION_INTERVAL_MODE | XTTCPS_OPTION_WAVE_DISABLE);

	/*
	 * Calling the timer setup routine
	 *  . initialize device
	 *  . set options
	 */
	Status = SetupTimer(pXTtcPs, TTC_TICK_DEVICE_ID);
	if(Status != XST_SUCCESS) { return Status; }

	/*
	 * Connect to the interrupt controller
	 */
	Status = XScuGic_Connect(pXScuGic
			, TTC_TICK_INTR_ID
			, (Xil_ExceptionHandler)TickHandler
			, (void *)pXTtcPs);
	if (Status != XST_SUCCESS) { return XST_FAILURE;}

	/*
	 * Enable the interrupt for the Timer counter
	 */
	XScuGic_Enable(pXScuGic, TTC_TICK_INTR_ID);

	/*
	 * Enable the interrupts for the tick timer/counter
	 * We only care about the interval timeout.
	 */
//	XTtcPs_EnableInterrupts(pXTtcPs, XTTCPS_IXR_INTERVAL_MASK);

//	/*
//	 * Start the tick timer/counter
//	 */
//	XTtcPs_Start(pXTtcPs);

	return Status;
}


/****************************************************************************/
/**
*
* This function sets up a timer counter device, using the information in its
* setup structure.
*  . initialize device
*  . set options
*  . set interval and prescaler value for given output frequency.
*
* @param	DeviceID is the unique ID for the device.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None.
*
*****************************************************************************/
int SetupTimer(XTtcPs *pXTtcPs, int DeviceID)
{
	int Status;
	XTtcPs_Config *Config;

	TmrCntrSetup *TimerSetup;

	TimerSetup = &SettingsTable[DeviceID];

	/*
	 * Look up the configuration based on the device identifier
	 */
	Config = XTtcPs_LookupConfig(DeviceID);
	if (NULL == Config) { return XST_FAILURE; }

	/*
	 * Initialize the device
	 */
	Status = XTtcPs_CfgInitialize(pXTtcPs, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) { return XST_FAILURE; }

	/*
	 * Set the options
	 */
	XTtcPs_SetOptions(pXTtcPs, TimerSetup->Options);

	/*
	 * Timer frequency is preset in the TimerSetup structure,
	 * however, the value is not reflected in its other fields, such as
	 * IntervalValue and PrescalerValue. The following call will map the
	 * frequency to the interval and prescaler values.
	 */
	XTtcPs_CalcIntervalFromFreq(pXTtcPs
			, TimerSetup->OutputHz
			, &(TimerSetup->Interval)
			, &(TimerSetup->Prescaler));

	/*
	 * Set the interval and prescale
	 */
	XTtcPs_SetInterval(pXTtcPs, TimerSetup->Interval);
	XTtcPs_SetPrescaler(pXTtcPs, TimerSetup->Prescaler);

	return XST_SUCCESS;
}

/***************************************************************************/
/**
*
* This function is the handler which handles the periodic tick interrupt.
* It updates its count, and set a flag to signal PWM timer counter to
* update its duty cycle.
*
* This handler provides an example of how to handle data for the TTC and
* is application specific.
*
* @param	CallBackRef contains a callback reference from the driver, in
*		this case it is the instance pointer for the TTC driver.
*
* @return	None.
*
* @note		None.
*
*****************************************************************************/

 void TickHandler(void *CallBackRef)
{
	u32 StatusEvent;
	g_bOnTickHandler = true;

	/*
	 * Read the interrupt status, then write it back to clear the interrupt.
	 */
	StatusEvent = XTtcPs_GetInterruptStatus((XTtcPs *)CallBackRef);
	XTtcPs_ClearInterruptStatus((XTtcPs *)CallBackRef, StatusEvent);

	static int DetectEthLinkStatus = 0;
	/* we need to call tcp_fasttmr & tcp_slowtmr at intervals specified
	 * by lwIP. It is not important that the timing is absoluetly accurate.
	 */
	static int odd = 1;

	DetectEthLinkStatus++;
    TcpFastTmrFlag = 1;
	odd = !odd;
	if (odd) { TcpSlowTmrFlag = 1; }

	/* For detecting Ethernet phy link status periodically */
	if (DetectEthLinkStatus == ETH_LINK_DETECT_INTERVAL) {
		eth_link_detect(echo_netif);
		DetectEthLinkStatus = 0;
	}



	if (0 != (XTTCPS_IXR_INTERVAL_MASK & StatusEvent)) {
		TickCount++;

//		if (TICKS_PER_CHANGE_PERIOD == TickCount) {
//			TickCount = 0;
//			PWM_UpdateFlag = TRUE;
//
//		}

	}
	else {
		/*
		 * The Interval event should be the only one enabled. If it is
		 * not it is an error
		 */
		ErrorCount++;
	}
}

