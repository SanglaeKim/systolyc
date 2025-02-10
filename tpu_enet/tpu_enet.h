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

#define ENET_RCV_BUFFER_SIZE     (4*1024*1024)

#define PKT_BUFFER_SIZE     (4U)

#define CHKSUM_SIZE		    (4U)
#define TPU_PKT_PL_SIZE    (103040U)

// for upload, pl->ps
#define SRAM_BASE_UL        (0xA1000000U)
#define OFFSET_UL      		(0x00020000U)

#define NUM_BYTES_UL        (51200U)
#define NUM_WORDS_UL     	(NUM_BYTES_UL/4)

#define SRAM_ADDR_UL_EVEN   (SRAM_BASE_UL)
#define SRAM_ADDR_UL_ODD    (SRAM_BASE_UL + OFFSET_UL)

typedef enum{E_GET_HEAD, E_GET_PAYLOAD, E_GET_CHKSUM}enTpuState;

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
  uint8_t ucPayloadArr[TPU_PKT_PL_SIZE];
  uint32_t uiChksum;
}StTpuPkt;

void displayAppInfo(void);
void tpu_init(void);

void tpu_enet_receive(enTpuState *penTpuState );

void tpu_update_sram(void);
void tpu_cdma_pl2ps(uint32_t *pFileNameIndex);

int tpu_upload_data(u8 *sndBuff, int msgSize);
void tpu_enet_send();
void ProcessTCPEvents();
bool isTheBaseValid(const u32 Base, const u32 Bases[], u32 numElements);

#endif /* SRC_TPU_ENET_H_ */
