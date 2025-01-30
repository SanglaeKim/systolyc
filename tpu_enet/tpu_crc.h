/*
 * tpu_crc.h
 *
 *  Created on: 2025. 1. 28.
 *      Author: User
 */

#ifndef SRC_TPU_CRC_H_
#define SRC_TPU_CRC_H_

void init_crc32_table();

uint32_t crc32(const uint8_t *data, size_t length);

#endif /* SRC_TPU_CRC_H_ */
