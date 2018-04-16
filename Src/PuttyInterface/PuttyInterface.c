/*
 * PuttyInterface.c
 *
 *  Created on: Sep 29, 2017
 *      Author: Leon
 */

#include "PuttyInterface.h"

#ifdef STM32F0
#  include "stm32f0xx_hal.h"
#endif
#ifdef STM32F1
#  include "stm32f1xx_hal.h"
#endif
#ifdef STM32F3
#  include "stm32f3xx_hal.h"
#endif
#ifdef STM32F4
#  include "stm32f4xx_hal.h"
#endif

#include <string.h>
#include <stdio.h>
#ifdef PUTTY_USB
#include "usb_device.h"
#endif

#ifdef PUTTY_USART
#define MAX_UART_RX 3
#endif

enum {
	first_byte,
	escape_seq,
	arrow_key
}reception_State;

// clears the current line, so new text can be put in
static inline void ClearLine();

/*	modulo keeping the value within the real range
 *	val is the start value,
 *	dif is the difference that will be added
 *	modulus is the value at which it wraps
 */
static inline uint8_t wrap(uint8_t val, int8_t dif, uint8_t modulus);
/*	This function deals with input by storing values and calling function func with a string of the input when its done
 * 	also handles up and down keys
 * 	input is a pointer to the characters to handle,
 * 	n_chars is the amount of chars to handle
 * 	func is the function to call when a command is complete
 */
static void HandlePcInput(char * input, size_t n_chars, HandleLine func);

void PuttyInterface_HexOut(uint8_t data[], uint8_t length){
#ifdef PUTTY_USART
	HAL_UART_Transmit_IT(&putty_huart, data, length);
#endif
#ifdef PUTTY_USB
	if(usb_comm){
		CDC_Transmit_FS(data, length);
		HAL_Delay(1);
	}
#endif
}

void PuttyInterface_Init(PuttyInterfaceTypeDef* pitd){
	pitd->huart_Rx_len = 0;
	putty_length = 0;
	reception_State = first_byte;
#ifdef PUTTY_USB
	usb_comm = false;
#endif
	char * startmessage = "----------PuttyInterface_Init-----------\n\r";
	uprintf(startmessage);
#ifdef PUTTY_USART
	HAL_UART_Receive_IT(&putty_huart, pitd->rec_buf, MAX_UART_RX);
#endif
}

void PuttyInterface_Update(PuttyInterfaceTypeDef* pitd){
#ifdef PUTTY_USB
	if(!usb_comm){
		usb_comm = !!(hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED);
	}else{
		if(pitd->huart_Rx_len){
			HandlePcInput((char*)&pitd->small_buf, pitd->huart_Rx_len, pitd->handle);
			pitd->huart_Rx_len = 0;
		}
	}
#endif
#ifdef PUTTY_USART
	static uint8_t received = 0;
	uint8_t RxXferCount = putty_huart.RxXferSize - putty_huart.RxXferCount;
	if(RxXferCount - received){
		//uprintf("[%u][%0x %0x %0x\n\r", RxXferCount, putty_huart.pRxBuffPtr-(RxXferCount), putty_huart.pRxBuffPtr-(RxXferCount-1), putty_huart.pRxBuffPtr-(RxXferCount-2));
		HandlePcInput((char*)putty_huart.pRxBuffPtr-(RxXferCount- received), RxXferCount- received, pitd->handle);
		pitd->huart_Rx_len = 0;
		received = 	RxXferCount;
		if(HAL_OK == HAL_UART_Receive_IT(&putty_huart, pitd->rec_buf, MAX_UART_RX))
			received = 0;
	}
#endif
}

/* ----------------------- static functions --------------------------------*/

static inline void ClearLine(){
	static char array[MAX_COMMAND_LENGTH] = {'\r', [0 ... MAX_COMMAND_LENGTH-5] = ' ','\r'};
	uprintf(array);
}

static inline uint8_t wrap(uint8_t val, int8_t dif, uint8_t modulus){
	dif %= modulus;
	if(dif < 0)
		dif += modulus;
	dif += (val);
	if(dif >= modulus)
		dif -= modulus;
	return (uint8_t) dif;
}

static void HandlePcInput(char * input, size_t n_chars, HandleLine func){
	static char PC_Input[COMMANDS_TO_REMEMBER][MAX_COMMAND_LENGTH];			// Matrix which holds the entered commands
	static uint8_t char_cnt = 0;											// counts the letters in the current forming command
	static int8_t commands_counter = 0;										// counts the entered commands
	static int8_t kb_arrow_counter = 0;										// counts the offset from the last entered command
	static bool full_matrix = 0;											// checks if there are COMMANDS_TO_REMEMBER commands stored
	uint8_t seq_cnt = 0;
	uint8_t cur_pos;

	switch(reception_State){
	case first_byte:
		if(input[0] == '\r'){													// newline, is end of command
			PC_Input[commands_counter][char_cnt] = '\0';
			char_cnt = 0;
			kb_arrow_counter = 0;												// reset the arrow input counter
			uprintf("\r");
			uprintf(PC_Input[commands_counter]);
			uprintf("\n\r");
			func(PC_Input[commands_counter++]);									// Callback func
			commands_counter = commands_counter % COMMANDS_TO_REMEMBER;
			full_matrix = full_matrix || !commands_counter;						// if there are more than the maximum amount of stored values, this needs to be known
			PC_Input[commands_counter][0] = '\0';
		}else if(input[0] == 0x7F){//backspace
			if(char_cnt < 1){
				char_cnt = 0;
			}else{
				char_cnt -= 1;													// reset the arrow input counter
			}
			PC_Input[commands_counter][char_cnt] = '\0';
			ClearLine();
			uprintf(PC_Input[commands_counter]);
		}else if(input[seq_cnt++] == '\e'){//escape, also used for special keys in escape sequences
			reception_State = escape_seq;
		}else{// If it is not a special character, the value is put in the current string
			if(char_cnt >= MAX_COMMAND_LENGTH){
				ClearLine();
				uprintf("ERROR: command too long\n\r");
				memset(PC_Input[commands_counter], 0, MAX_COMMAND_LENGTH);
				char_cnt = 0;
			}else{
				uprintf("%c", input[0]);
				PC_Input[commands_counter][char_cnt++] = (char)input[0];
			}
			break;
		}
		//no break
	case escape_seq:
		if(input[seq_cnt++] == 0x5b){
			reception_State = arrow_key;
		}else{
			reception_State = first_byte;
			break;
		}
		//no break
	case arrow_key:
		switch(input[seq_cnt]){
		case 'A'://arrow ^
			kb_arrow_counter--;
			cur_pos = full_matrix ? wrap(commands_counter, kb_arrow_counter, COMMANDS_TO_REMEMBER) : wrap(commands_counter, kb_arrow_counter, commands_counter+1);
			char_cnt = strlen(PC_Input[cur_pos]);
			strcpy(PC_Input[commands_counter], PC_Input[cur_pos]);
			ClearLine();
			uprintf(PC_Input[commands_counter]);
			break;
		case 'B'://arrow \/;
			kb_arrow_counter++;
			cur_pos = full_matrix ? wrap(commands_counter, kb_arrow_counter, COMMANDS_TO_REMEMBER) : wrap(commands_counter, kb_arrow_counter, commands_counter+1);
			char_cnt = strlen(PC_Input[cur_pos]);
			strcpy(PC_Input[commands_counter], PC_Input[cur_pos]);
			ClearLine();
			uprintf(PC_Input[commands_counter]);
			break;
		case 'C'://arrow ->
			break;
		case 'D'://arrow <-
			break;
		}
		reception_State = first_byte;
		break;
	}

}
