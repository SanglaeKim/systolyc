/*
 * tpu_crc.c
 *
 *  Created on: 2025. 1. 28.
 *      Author: User
 */


//#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint32_t crc32_table[256] __attribute__ ((aligned (64)));

void init_crc32_table() {
  uint32_t crc;
  for (int i = 0; i < 256; i++) {
	crc = i;
	for (int j = 8; j > 0; j--) {
	  if (crc & 1) {
		crc = (crc >> 1) ^ 0xEDB88320;
	  } else {
		crc >>= 1;
	  }
	}
	crc32_table[i] = crc;
  }
}

uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;

  for (size_t i = 0; i < length; i++) {
	uint8_t byte = data[i];
	crc = (crc >> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
  }

  return crc ^ 0xFFFFFFFF;
}

