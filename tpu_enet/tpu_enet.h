/*
 * tpu.h
 *
 *  Created on: 2025. 1. 13.
 *      Author: User
 */

#ifndef SRC_TPU_ENET_H_
#define SRC_TPU_ENET_H_

#include <stdint.h>
#include <stdbool.h>

#include "xil_types.h"

#include "lwip/err.h"
#include "lwip/tcp.h"

#define RECV_BUFFER_DEPTH   (16U)
#define CHECKSUM_SIZE		(4U)
//#define RECV_BUFFER_SIZE    (103040U+CHECKSUM_SIZE)
#define RECV_BUFFER_SIZE    (103040U)

// for upload, pl->ps
#define SRAM_BASE_UL        (0xA1000000U)
#define OFFSET_UL      		(0x00020000U)

#define NUM_BYTES_UL        (51200U)
#define NUM_WORDS_UL     	(NUM_BYTES_UL/4)

#define SRAM_ADDR_UL_EVEN   (SRAM_BASE_UL)
#define SRAM_ADDR_UL_ODD    (SRAM_BASE_UL + OFFSET_UL)

typedef enum{E_IDLE, E_ON_RCV}EnTPU;

//typedef struct _StTpuPkt{
//  uintptr_t puiBase;
//  uint32_t uiNumBytes;
//  uint8_t *pPayload;
//}StTpuPkt;

typedef struct _StTpuPktHeader{
  uint32_t uiBase;
  uint32_t uiNumBytes;
}StTpuPktHeader;

typedef struct _StTpuPkt {
  StTpuPktHeader stTpuPktHeader;
  uint8_t ucPayloadArr[RECV_BUFFER_SIZE];
}StTpuPkt;

void displayAppInfo(void);
void tpu_init(void);
void tpu_update_enet(struct pbuf *p, err_t err);
void tpu_update_sram(void);
void tpu_update_isr(u32 *pFileNameIndex);


int tpu_upload_data(u8 *sndBuff, int msgSize);
void ProcessTCPEvents();

#endif /* SRC_TPU_ENET_H_ */
