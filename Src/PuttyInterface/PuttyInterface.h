/*
 * PuttyInterface.h
 *
 *  Created on: Sep 29, 2017
 *      Author: Leon
 */

#ifndef PUTTYINTERFACE_H_
#define PUTTYINTERFACE_H_

#define PUTTY_USART // or choose #define PUTTY_USART

#ifdef PUTTY_USART
#define putty_huart huart1
#endif /* PUTTY_USART */

#ifdef PUTTY_USART
#include "usart.h"
#endif/* PUTTY_USART */
#ifdef PUTTY_USB
#include "usbd_cdc_if.h"
#endif /* PUTTY_USB */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// amount of commands that will be remembered
#define COMMANDS_TO_REMEMBER 16
#define MAX_COMMAND_LENGTH   32

// function that works like normal printf()
#ifdef PUTTY_USART
#define uprintf(...) do { \
	while(putty_huart.gState != HAL_UART_STATE_READY);\
	putty_length = sprintf(smallStrBuffer, __VA_ARGS__); \
	HAL_UART_Transmit_IT(&putty_huart, (uint8_t*)smallStrBuffer, putty_length);} while(0)
#endif
#ifdef PUTTY_USB
bool usb_comm;
#define uprintf(...) do { \
		if(usb_comm){\
			putty_length = sprintf(smallStrBuffer, __VA_ARGS__); \
			while(CDC_Transmit_FS((uint8_t*)smallStrBuffer, putty_length) == USBD_BUSY);\
		}\
	} while(0)
#endif
// function that will be called when HandlePcInput is done.
typedef void (*HandleLine)(char * input);

typedef struct {
	uint8_t rec_buf[32];
	char small_buf[32];
	uint huart_Rx_len;
	HandleLine handle;
}PuttyInterfaceTypeDef;

char smallStrBuffer[1024];
uint8_t putty_length;

// Transmit raw data
// data[] is the data,
// length is the length of the data
void PuttyInterface_HexOut(uint8_t*, uint8_t);

void PuttyInterface_Init(PuttyInterfaceTypeDef* pitd);

void PuttyInterface_Update(PuttyInterfaceTypeDef* pitd);

#endif /* PUTTYINTERFACE_H_ */
