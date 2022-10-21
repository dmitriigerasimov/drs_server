/*
 * ethernet_test.h
 *
 *  Created on: 24 дек. 2014 г.
 *      Author: Zubarev
 */
#pragma once

int eth_srv_init();
void eth_srv_deinit();
int eth_srv_start();
int eth_srv_loop();
