/******************************************************************************
 *
 * Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
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
#include <string.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif

#include "tpu_enet.h"
#include "globals.h"

void tcp_err_cb(void *arg, err_t err);

unsigned int TCP_Conn_Status = 0;

void print_app_header()
{
  xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
  xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
					struct pbuf *p, err_t err)
{
  /* do not read the packet if we are not in ESTABLISHED state */
  if (!p) {
	tcp_close(tpcb);
	tcp_recv(tpcb, NULL);
	return ERR_OK;
  }

  /* indicate that the packet has been received */
  tcp_recved(tpcb, p->len);

  /* echo back the payload */
  if(p != NULL){
	if(p->len != p->tot_len){
	  xil_printf("p->len != p->len_tot\n");
	}
  }

  if(err != ERR_OK) return err;

//  while (p != NULL) {

	uint64_t enet_rcv_buffer_head_ = enet_rcv_buffer_head % ENET_RCV_BUFFER_SIZE;
	if(enet_rcv_buffer_head > enet_rcv_buffer_tail + ENET_RCV_BUFFER_SIZE){
	  xil_printf("[ERROR]lwip rcv buffer over flow[HEAD: %u, TAIL: %u\r\n", enet_rcv_buffer_head, enet_rcv_buffer_tail);
	}
	Xil_AssertNonvoid(enet_rcv_buffer_head < enet_rcv_buffer_tail + ENET_RCV_BUFFER_SIZE);

	// Are we in the boundary?
	if ((enet_rcv_buffer_head_+ p->len) > ENET_RCV_BUFFER_SIZE) {
	  uint32_t partialNumBytes = ENET_RCV_BUFFER_SIZE - enet_rcv_buffer_head_;
	  memcpy(&enet_rcv_buffer[enet_rcv_buffer_head_], p->payload, partialNumBytes);
	  memcpy(&enet_rcv_buffer[0], (uint8_t *)p->payload+partialNumBytes, p->len - partialNumBytes);
	}else{
	  memcpy(&enet_rcv_buffer[enet_rcv_buffer_head_], p->payload, p->len);
	}
	enet_rcv_buffer_head += p->len;

	// Free the pbuf and move to the next
//	struct pbuf *next = p->next;
//	pbuf_free(p);
//	p = next;
//  }

  //	/* in this case, we assume that the payload is < TCP_SND_BUF */
  //	if (tcp_sndbuf(tpcb) > p->len) {
  //		err = tcp_write(tpcb, p->payload, p->len, 1);
  //	} else
  //		xil_printf("no space in tcp_sndbuf\n\r");

  /* free the received pbuf */
  	pbuf_free(p);

  return ERR_OK;
}

err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  printf("_&_: %d", len);
  return ERR_OK;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  static int connection = 1;

  /* set the receive callback for this connection */
  tcp_recv(newpcb, recv_callback);

  //	tcp_sent(newpcb, sent_callback);
  //	error callback
  //tcp_err(newpcb, tcp_err_cb);

  /* just use an integer number indicating the connection id as the
	 callback argument */
  tcp_arg(newpcb, (void*)(UINTPTR)connection);

  //	포트 갱신
  hTCP_ServerPort = newpcb;

  /* increment for subsequent accepted connections */
  connection++;

  return ERR_OK;
}

//================================================================================
//	TCP Error CB
//================================================================================
void tcp_err_cb(void *arg, err_t err)
{
  TCP_Conn_Status = 0;

  xil_printf("TCP Err CB : %d\n", err);
}


int start_application()
{
  struct tcp_pcb *pcb;
  err_t err;
  unsigned port = 7;

  /* create new TCP PCB structure */
  pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!pcb) {
	xil_printf("Error creating PCB. Out of Memory\n\r");
	return -1;
  }

  /* bind to specified @port */
  err = tcp_bind(pcb, IP_ANY_TYPE, port);
  if (err != ERR_OK) {
	xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
	return -2;
  }

  /* we do not need any arguments to callback functions */
  tcp_arg(pcb, NULL);

  /* listen for connections */
  pcb = tcp_listen(pcb);
  if (!pcb) {
	xil_printf("Out of memory while tcp_listen\n\r");
	return -3;
  }

  /* specify callback to use for incoming connections */
  tcp_accept(pcb, accept_callback);

  xil_printf("TCP echo server started @ port %d\n\r", port);

  return 0;
}

err_t chk_if_error( err_t err) {
  if (err != ERR_OK) {
    xil_printf("recv_callback error: %d\n", err);

    // Handle specific error codes
    switch (err) {
	case ERR_MEM:
	  xil_printf("Memory error\n");
	  break;
	case ERR_BUF:
	  xil_printf("Buffer error\n");
	  break;
	case ERR_TIMEOUT:
	  xil_printf("Timeout error\n");
	  break;
	case ERR_ABRT:
	  xil_printf("Connection aborted\n");
	  break;
	case ERR_RST:
	  xil_printf("Connection reset by peer\n");
	  break;
	case ERR_CONN:
	  xil_printf("Connection error\n");
	  break;
	case ERR_VAL:
	  xil_printf("Invalid argument\n");
	  break;
	default:
	  xil_printf("Unknown error\n");
	  break;
    }

    return ERR_ABRT;
  }

  // Process received data
  // ...

  return ERR_OK;
}

