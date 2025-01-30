/******************************************************************************
 *
 * Copyright (C) 2009 - 2017 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Use of the Software is limited solely to applications:
 * (a) running on a Xilinx device, or
 * (b) that interact with a Xilinx device through a bus or interconnect.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Xilinx shall not be used
 * in advertising or otherwise to promote the sale, use or other dealings in
 * this Software without prior written authorization from Xilinx.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>

#include "xparameters.h"
#include "xscugic.h"
#include "xaxicdma.h"
#include "xttcps.h"
#include "netif/xadapter.h"

#include "platform.h"
#include "platform_config.h"
#if defined (__arm__) || defined(__aarch64__)
#include "xil_printf.h"
#endif

#include "lwip/tcp.h"
#include "xil_cache.h"

#include "main.h"
#include "tpu_enet.h"
#include "tpu_isr.h"
#include "tpu_timer.h"
#include "tpu_crc.h"


/*******    global variable s******/

StTpuPkt g_StTpuPktArr[16] __attribute__ ((aligned (64)));

u32 wBases[] = { 0xA3000000, 0xA30004B0, 0xA3002A30, 0xA3003390, 0xA3004650, 0xA3005910 };
u32 wFileSize[] = { 432, 4608, 1024, 2304, 2304, 1536 };

// batch normalize
u32 bBases[] = { 0xA3006590, 0xA3006690, 0xA3006890, 0xA3006A90, 0xA3006B90, 0xA3006C90 };
u32 bFileSize[] = { 256, 512, 512, 256, 256, 512 };

// input feature map?
u32 iBases[] = { 0xA0000000, 0xA0020000, 0xA0040000, 0xA0060000, 0xA0080000, 0xA00A0000, 0xA00C0000, 0xA00E0000
				 ,0xA0100000, 0xA0120000, 0xA0140000, 0xA0160000, 0xA0180000, 0xA01A0000, 0xA01C0000, 0xA01E0000 };

XScuGic g_XScuGic; /* Instance of the Interrupt Controller */

XAxiCdma instCDMA_PLtoPS; /* Instance of the XAxiCdma */
XAxiCdma instCDMA_PStoPL; /* Instance of the XAxiCdma */

XTtcPs g_XTtcPs; /* Two timer counters */

volatile  bool bPS2PL_EVENT = false;

/* Shared variables used to test the callbacks. */
volatile  u32 Done  = 0U; /* Dma transfer is done */
volatile  u32 Error = 0U; /* Dma Bus Error occurs */

volatile int isr_cnt_even = 0U;
volatile int isr_cnt_odd  = 0U;
volatile int isr_cnt_cdma = 0U;
volatile int cdmaErrCount = 0U;

volatile  u32 Done_ps2pl  = 0U; /* Dma transfer is done */
volatile  u32 Error_ps2pl = 0U; /* Dma Bus Error occurs */
volatile int isr_cnt_cdma_ps2pl = 0U;

volatile int TcpFastTmrFlag = 0;
volatile int TcpSlowTmrFlag = 0;

volatile u32 g_uiTimerCallbackCount = 0;
volatile bool g_bOnTimerCallback = false;

volatile int ps2pl_cdmaErrCount=0;

volatile u32 head = 0;
volatile u32 tail = 0;

volatile u32 pl_dl_count = 0;
volatile u32 qBufDiff = 0;

struct netif server_netif;
struct netif *echo_netif;

u32 fileNameIndex = 0;

volatile bool g_bOnTickHandler = false;
//volatile u32 g_uiOnTickHandler = 0;
//volatile u32 g_rxBuffUseCountMax = 0;
volatile u32 g_uiE_ON_RCV_COUNT = 0;

int main() {

  int Status;
  ip_addr_t ipaddr, netmask, gw;

  /* the mac address of the board. this should be unique per board */
  unsigned char mac_ethernet_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

  echo_netif = &server_netif;

  init_crc32_table();

  init_platform();

  displayAppInfo();

  setupInterrupt(&g_XScuGic);

  Status = SetupTicker(&g_XScuGic, &g_XTtcPs);
  if (Status != XST_SUCCESS) {
	return Status;
  }

  /* initliaze IP addresses to be used */
  IP4_ADDR(&ipaddr,  192, 168,   1, 10);
  IP4_ADDR(&netmask, 255, 255, 255,  0);
  IP4_ADDR(&gw,      192, 168,   1,  1);

  print_app_header();

  lwip_init();

  /* Add network interface to the netif_list, and set it as default */
  if (!xemac_add(echo_netif, &ipaddr, &netmask,
				 &gw, mac_ethernet_address,
				 PLATFORM_EMAC_BASEADDR)) {
	xil_printf("Error adding N/W interface\n\r");
	return -1;
  }

  netif_set_default(echo_netif);

  /* now enable interrupts */
  //	platform_enable_interrupts();
  XTtcPs_EnableInterrupts(&g_XTtcPs, XTTCPS_IXR_INTERVAL_MASK);
  /*
   * Start the tick timer/counter
   */
  XTtcPs_Start(&g_XTtcPs);

  /* specify that the network if is up */
  netif_set_up(echo_netif);

  print_ip_settings(&ipaddr, &netmask, &gw);

  /* start the application (web server, rxtest, txtest, etc..) */
  start_application();

  /* receive and process packets */
  while (1) {

	ProcessTCPEvents();
	//transfer_data();
	tpu_update_sram();
	tpu_update_isr(&fileNameIndex);

	if(g_bOnTickHandler){
	  g_bOnTickHandler = false;
	  g_uiE_ON_RCV_COUNT++;

	  ++g_uiTimerCallbackCount;
	  if(g_uiTimerCallbackCount % 4 == 0){
		  if(g_uiTimerCallbackCount % 64 == 0){
			xil_printf("\r\n%d : ", g_uiTimerCallbackCount/64);
		  }
		xil_printf("_._");
	  }
	}
  }
  
  /* never reached */
  cleanup_platform();

  return 0;
}

void print_ip(char *msg, ip_addr_t *ip)
{
  print(msg);
  xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
			 ip4_addr3(ip), ip4_addr4(ip));
}

void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{

  print_ip("Board IP: ", ip);
  print_ip("Netmask : ", mask);
  print_ip("Gateway : ", gw);
}
