/*
 * main.h
 *
 *  Created on: 2025. 1. 21.
 *      Author: User
 */

#ifndef SRC_MAIN_H_
#define SRC_MAIN_H_

/* defined by each RAW mode application */
void print_app_header();
int start_application();
int transfer_data();

/* missing declaration in lwIP */
void lwip_init();
void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw);

#endif /* SRC_MAIN_H_ */
