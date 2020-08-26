/*
 * Copyright (c) 2019 DIGNSYS Inc.
 *
 * Contact: Hyobok Ahn (hbahn@dignsys.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <peripheral_io.h>
#include <system_info.h>
#include <unistd.h>
#include <app_common.h>
#include "hello.h"
#include <Ecore.h>

#include "vr3.h"



#define UART_PORT_ANCHOR3		1	// RPI3 : UART0, ANCHOR3 : UART1
#define MAX_TRY_COUNT			10
#define MAX_FRAME_LEN			32

int incoming_byte = 0;          // for incoming serial data
char frame_buf[MAX_FRAME_LEN];  // for save protocol data
int frame_len = MAX_FRAME_LEN;  // length of frame
int byte_position = 0;          // next byte position in frame_buf
unsigned int calc_checksum = 0; // to save calculated checksum value

static bool initialized = false;
static peripheral_uart_h g_uart_h;
static peripheral_gpio_h g_gpio_h;

int pwrPin = 17;
int statPin = 27;

/*
 * open UART port and set UART handle resource
 * set BAUD rate, byte size, parity bit, stop bit, flow control
 * Default baud rate：9600bps Check bit：None Stop bit：1 bit
 */
bool resource_serial_init(void)
{
	if (initialized) return true;

	LOGI("----- resource_serial_init -----");
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;

	// Opens the UART slave device
	ret = peripheral_uart_open(UART_PORT_ANCHOR3, &g_uart_h);
	if (ret != PERIPHERAL_ERROR_NONE) {
		LOGE("UART port [%d] open Failed, ret [%d]", UART_PORT_ANCHOR3, ret);
		return false;
	}
	// Sets baud rate of the UART slave device.
	ret = peripheral_uart_set_baud_rate(g_uart_h, PERIPHERAL_UART_BAUD_RATE_115200);	// The number of signal in one second is 9600
	if (ret != PERIPHERAL_ERROR_NONE) {
		LOGE("uart_set_baud_rate set Failed, ret [%d]", ret);
		return false;
	}
	// Sets byte size of the UART slave device.
	ret = peripheral_uart_set_byte_size(g_uart_h, PERIPHERAL_UART_BYTE_SIZE_8BIT);	// 8 data bits
	if (ret != PERIPHERAL_ERROR_NONE) {
		LOGE("byte_size set Failed, ret [%d]", ret);
		return false;
	}
	// Sets parity bit of the UART slave device.
	ret = peripheral_uart_set_parity(g_uart_h, PERIPHERAL_UART_PARITY_NONE);	// No parity is used
	if (ret != PERIPHERAL_ERROR_NONE) {
		LOGE("parity set Failed, ret [%d]", ret);
		return false;
	}
	// Sets stop bits of the UART slave device
	ret = peripheral_uart_set_stop_bits (g_uart_h, PERIPHERAL_UART_STOP_BITS_1BIT);	// One stop bit
	if (ret != PERIPHERAL_ERROR_NONE) {
		LOGE("stop_bits set Failed, ret [%d]", ret);
		return false;
	}
	// Sets flow control of the UART slave device.
	// No software flow control & No hardware flow control
	ret = peripheral_uart_set_flow_control (g_uart_h, PERIPHERAL_UART_SOFTWARE_FLOW_CONTROL_NONE, PERIPHERAL_UART_HARDWARE_FLOW_CONTROL_NONE);
	if (ret != PERIPHERAL_ERROR_NONE) {
		LOGE("flow control set Failed, ret [%d]", ret);
		return false;
	}

	initialized = true;
	return true;
}

/*
 * To write data to a slave device
 */
bool resource_write_data(uint8_t *data, uint32_t length)
{
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;
	/* IMPLEMENT HERE
	 * write length byte data to UART
	 * API : peripheral_uart_write
	 * 	Args : g_uart_h, data & length
	 * 	return : ret
	 */
	ret = peripheral_uart_write(g_uart_h, data, length);

	if (ret != PERIPHERAL_ERROR_NONE) {
		LOGE("UART write failed, ret [%d]", ret);
		return false;
	}

	LOGE("OK!! UART write, ret [%d]", ret);

	return true;
}

/*
 * To read data from a slave device
 */
bool resource_read_data(uint8_t *data, uint32_t length, bool blocking_mode)
{
	int try_again = 0;
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;
	if (g_uart_h == NULL)
		return false;

	while (1) {
		// read length byte from UART
		ret = peripheral_uart_read(g_uart_h, data, length);
		if (ret == PERIPHERAL_ERROR_NONE)
			return true;

		// if data is not ready, try again
		if (ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			// if blocking mode, read again
			if (blocking_mode == true) {
				usleep(100 * 1000);
				LOGI(".");
				continue;
			} else {
				// if non-blocking mode, retry MAX_TRY_COUNT
				if (try_again >= MAX_TRY_COUNT) {
					LOGE("No data to receive");
					return false;
				} else {
					try_again++;
				}
			}
		} else {
			// if return value is not (PERIPHERAL_ERROR_NONE or PERIPHERAL_ERROR_TRY_AGAIN)
			// return with false
			LOGE("UART read failed, , ret [%d]", ret);
			return false;
		}
	}

	return true;
}

/*
 * close UART handle and clear UART resources
 */
void resource_serial_fini(void)
{
	LOGI("----- resource_serial_fini -----");
	if(initialized) {
		// Closes the UART slave device
		peripheral_uart_close(g_uart_h);
		initialized = false;
		g_uart_h = NULL;
	}
}

int mdm_isPowerON(void)
{
	int ret;
	int gpio_out;
	ret = peripheral_gpio_open(statPin, &g_gpio_h);

	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_open failed.");
		return -1;
	}

	ret = peripheral_gpio_set_direction(g_gpio_h, PERIPHERAL_GPIO_DIRECTION_IN);
	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_set_direction failed.");
		return -1;
	}

	ret = peripheral_gpio_read(g_gpio_h, &gpio_out);
	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_read failed.");
		return -1;
	}

	peripheral_gpio_close(g_gpio_h);


	LOGE("Cat.M1 Status : %s", gpio_out > 0?"ON":"OFF");

	return gpio_out;
}

int mdm_powerOFF(void)
{
	int ret;
	int gpio_out;
	ret = peripheral_gpio_open(statPin, &g_gpio_h);

	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_open failed.");
		return -1;
	}

	ret = peripheral_gpio_set_direction(g_gpio_h, PERIPHERAL_GPIO_DIRECTION_IN);
	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_set_direction failed.");
		return -1;
	}

	ret = peripheral_gpio_read(g_gpio_h, &gpio_out);
	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_read failed.");
		return -1;
	}

	peripheral_gpio_close(g_gpio_h);

	LOGE("Cat.M1 Status : %s", gpio_out > 0?"ON":"OFF");

	if(gpio_out>0)
	{
		LOGE("LTE Cat.M1 Modem Power OFF...");

		ret = peripheral_gpio_open(pwrPin, &g_gpio_h);

		if(ret){
			peripheral_gpio_close(g_gpio_h);
			LOGE("peripheral_gpio_open failed.");
			return -1;
		}

		ret = peripheral_gpio_set_direction(g_gpio_h, PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW);
		if(ret){
			peripheral_gpio_close(g_gpio_h);
			LOGE("peripheral_gpio_set_direction failed.");
			return -1;
		}

		gpio_out = 1;

		ret = peripheral_gpio_write(g_gpio_h, gpio_out);
		if(ret){
			peripheral_gpio_close(g_gpio_h);
			LOGE("peripheral_gpio_write failed.");
			return -1;
		}

		usleep(800 * 1000);	//Hold Time : Greater than or equal to 650ms

		gpio_out = 0;

		ret = peripheral_gpio_write(g_gpio_h, gpio_out);
		if(ret){
			peripheral_gpio_close(g_gpio_h);
			LOGE("peripheral_gpio_write failed.");
			return -1;
		}

		usleep(3000 * 1000);	//Release Time : Greater than or equal to 2000ms

		peripheral_gpio_close(g_gpio_h);
	}

	return 0;
}

int mdm_powerON(void)
{
	int ret;
	int gpio_out;

	LOGE("LTE Cat.M1 Modem Power ON...");

	ret = peripheral_gpio_open(pwrPin, &g_gpio_h);

	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_open failed.");
		return -1;
	}

	ret = peripheral_gpio_set_direction(g_gpio_h, PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW);
	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_set_direction failed.");
		return -1;
	}

	gpio_out = 1;

	ret = peripheral_gpio_write(g_gpio_h, gpio_out);
	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_write failed.");
		return -1;
	}

	usleep(600 * 1000);	//Hold Time : Greater than or equal to 500ms

	gpio_out = 0;

	ret = peripheral_gpio_write(g_gpio_h, gpio_out);
	if(ret){
		peripheral_gpio_close(g_gpio_h);
		LOGE("peripheral_gpio_write failed.");
		return -1;
	}

	usleep(5000 * 1000);	//Release Time : Greater than or equal to 4800ms

	peripheral_gpio_close(g_gpio_h);

}

static void uart_buffer_flush(void)
{
	bool ret = true;
	char buffer[128];
	int idx=0;
	char ch;

	LOGE("uart buffer flush...");
	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return;
	}

	while(true)
	{
		ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (ret == PERIPHERAL_ERROR_NONE)
		{
			LOGE("buffer Flush...");
		}
		// if data is not ready, try again
		if (ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			break;
		}
	}
//	resource_serial_fini();
}

int mdm_socketRecv(char *recvMsg, int length)
{
	bool ret = true;
	char buffer[1024];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return 1;
	}

	float Timeout = 10.0;
	static double cTime;

	cTime = ecore_time_get();
	int found = 1;
	char *checkFilter = NULL;

	usleep(3000 * 1000);
	const char *cmd = "AT+QIRD=0\r";

	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return 1;
	}

	char *checkPointer, *checkPointer2;
	int recvSize = 0;
	char filter3[] = "OK";
	found = 1;
	memset(buffer, 0x0, sizeof(buffer));

	/* Check Network Registred */
	do{
		ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r'){
				buffer[idx++] = ch;
			}
			else {
				checkFilter = strstr(buffer, filter3);

				if(checkFilter == NULL)
				{
				}
				else{
					if(recvSize>length)
						recvSize = length;
					//LOGE("recv Msg : %s", buffer);
					memcpy(recvMsg, buffer, recvSize);
					found = 0;
				}
			}
		}
		usleep(100 * 1000);
	}while( (Timeout>= (ecore_time_get() - cTime) ) && found);

	if(found == 0)
	{
		checkPointer = strstr(buffer, "+QIRD:");
		if( checkPointer != NULL ){
			checkPointer2 = checkPointer + 7;
			recvSize = atoi(checkPointer2);
			LOGE("recvSize : %d", recvSize);
		}

		checkPointer = strstr(buffer, "\n");
		if( checkPointer != NULL){
			checkPointer++;
			checkPointer2 = strstr(checkPointer, "\n");
			checkPointer2++;

			if(recvSize>length)
				recvSize = length;

			memset(recvMsg, 0x0, length);
			memcpy(recvMsg, checkPointer2, recvSize);
			//LOGE("recvMsg : %s", recvMsg);

		}
	}

	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return found;
}

int mdm_socketSend(char *sendMsg, int length)
{
	bool ret = true;
	char buffer[128];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return 1;
	}

	char cmd[128];
	memset(cmd, 0x0, sizeof(cmd));

	sprintf(cmd, "AT+QISEND=0,%d\r",length);
	LOGE("Socket Send : %s",cmd);

	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return 1;
	}

	float Timeout = 10.0;
	static double cTime;

	cTime = ecore_time_get();
	char *checkFilter = NULL;
	int found = 1;

	do{
		ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch == '>')
				found = 0;
		}
		// if data is not ready, try again
		if (ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			usleep(100 * 1000);
			LOGI(".");
			continue;
		}
	}while( (Timeout>= (ecore_time_get() - cTime) ) && found);

	LOGE("send : %s, %d",sendMsg, length);

	ret = resource_write_data(sendMsg, length);
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return 1;
	}

	char filter[] = "OK";
	found = 1;
	checkFilter = NULL;
	memset(buffer, 0x0, sizeof(buffer));

	do{
		ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r' && ch != '\n'){
				buffer[idx++] = ch;
			}
			else {
				checkFilter = strstr(buffer, filter);

				if(checkFilter == NULL)
				{
					//idx = 0;
				}
				else{
					found = 0;
				}
			}
		}
		usleep(100 * 1000);
	}while( (Timeout>= (ecore_time_get() - cTime) ) && found);

	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return found;
}

int mdm_socketOpen(char *IP, int port, bool isTCP)
{
	bool ret = true;
	char buffer[128];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;
	int found = 1;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return found;
	}

	char cmd[128];
	memset(cmd, 0x0, sizeof(cmd));

	sprintf(cmd,"AT+QIOPEN=1,0,\"%s\",\"%s\",%d,0,0\r",isTCP?"TCP":"UDP",IP,port);
	LOGE("Open : %s",cmd);

	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return found;
	}

	float Timeout = 10.0;
	static double cTime;

	cTime = ecore_time_get();

	char filter[] = "+QIOPEN:";
	char *checkFilter = NULL;

	peripheral_error_e _ret = PERIPHERAL_ERROR_NONE;

	do{
		_ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (_ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r'){
				buffer[idx++] = ch;
			}
			else {
				/* Debug */
#if 0
				for(int i=0; i<idx; i++)
				{
					LOGE("check String [%d] = %c", i, buffer[i]);
				}
#endif
				checkFilter = strstr(buffer, filter);
				if(checkFilter == NULL)
					idx = 0;
				else
					found = 0;
			}
		}
		// if data is not ready, try again
		if (_ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			usleep(100 * 1000);
			LOGI(".");
			continue;
		}
	}while( (Timeout>= (ecore_time_get() - cTime) ) && checkFilter == NULL);

#if 0
	for(int i=0; i<idx; i++)
	{
		LOGE("uart read buffer[%d] = %c", i, buffer[i]);
	}
#endif
	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return found;
}

void mdm_socketClose(void)
{
	bool ret = true;
	char buffer[128];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return;
	}

	LOGE("socket Close...");
	const char *cmd = "AT+QICLOSE=0,3\r";


	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return;
	}

	float Timeout = 13.0;
	static double cTime;

	cTime = ecore_time_get();

	char filter[] = "OK";
	char *checkFilter = NULL;

	peripheral_error_e _ret = PERIPHERAL_ERROR_NONE;

	do{
		_ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (_ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r'){
				buffer[idx++] = ch;
			}
			else {
				/* Debug */
#if 0
				for(int i=0; i<idx; i++)
				{
					LOGE("check String [%d] = %c", i, buffer[i]);
				}
#endif
				checkFilter = strstr(buffer, filter);
				if(checkFilter == NULL)
					idx = 0;
			}
		}
		// if data is not ready, try again
		if (_ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			usleep(100 * 1000);
			LOGI(".");
			continue;
		}
	}while( (Timeout>= (ecore_time_get() - cTime) ) && checkFilter == NULL);

#if 1
	for(int i=0; i<idx; i++)
	{
		LOGE("uart read buffer[%d] = %c", i, buffer[i]);
	}
#endif
	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return;
}

int mdm_getIMEI(char *imei, int length)
{
	bool ret = true;
	char buffer[128];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return 1;
	}

	const char *cmd = "AT+CGSN\r";

	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return 1;
	}

	float Timeout = 3.0;
	static double cTime;

	cTime = ecore_time_get();

	char filter2[] = "OK";
	int found = 1;
	char *checkFilter;

	/* Check Network Registred */
	do{
		ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r' && ch != '\n'){
				buffer[idx++] = ch;
			}
			else {
				checkFilter = strstr(buffer, filter2);

				if(checkFilter == NULL)
				{
					//idx = 0;
				}
				else{
					memset(imei, 0x0, length);
					memcpy(imei, buffer, 15);
					//LOGE("BG96 IMEI : %s",imei);
					found = 0;
				}
			}
		}
		usleep(100 * 1000);
	}while( (Timeout>= (ecore_time_get() - cTime) ) && found);

	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return found;
}

int  mdm_IsRegistred(void)
{
	bool ret = true;
	char buffer[128];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return 1;
	}

	const char *cmd = "AT+CEREG?\r";

	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return 1;
	}

	float Timeout = 3.0;
	static double cTime;

	cTime = ecore_time_get();

	char filter2[] = "+CEREG:";
	int found = 1;
	char *checkPointer, *checkPointer2;
	char *checkFilter;

	/* Check Network Registred */
	do{
		ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r'){
				buffer[idx++] = ch;
			}
			else {
				checkFilter = strstr(buffer, filter2);

				if(checkFilter == NULL)
				{
					idx = 0;
				}
				else{
					checkPointer = checkFilter+1;
					checkPointer2 = strstr(checkPointer, ",");
					checkPointer2++;

					if(*checkPointer2 == '1' || *checkPointer2 == '5')
					{
						LOGE("BG96 Network Registred");
						found = 0;
					}
				}
			}
		}
		usleep(100 * 1000);
	}while( (Timeout>= (ecore_time_get() - cTime) ) && found);

	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return found;
}

int mdm_init(void)
{
	bool ret = true;
	char buffer[128];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;
	int found = 1;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return found;
	}


//	const char *cmd = "AT\r";
	const char *cmd = "ATE0\r";


	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return found;
	}

	float Timeout = 3.0;
	static double cTime;

	cTime = ecore_time_get();

	char filter[] = "OK";
	char *checkFilter = NULL;

	peripheral_error_e _ret = PERIPHERAL_ERROR_NONE;

	do{
		_ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (_ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r'){
				buffer[idx++] = ch;
			}
			else {
				/* Debug */
#if 0
				for(int i=0; i<idx; i++)
				{
					LOGE("check String [%d] = %c", i, buffer[i]);
				}
#endif
				checkFilter = strstr(buffer, filter);
				if(checkFilter == NULL)
					idx = 0;
				else
					found = 0;
			}
		}
		// if data is not ready, try again
		if (_ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			usleep(100 * 1000);
			LOGI(".");
			continue;
		}
	}while( (Timeout>= (ecore_time_get() - cTime) ) && checkFilter == NULL);

#if 0
	for(int i=0; i<idx; i++)
	{
		LOGE("uart read buffer[%d] = %c", i, buffer[i]);
	}
#endif
	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return found;
}

int mdm_pdpAct(bool _enable)
{
	bool ret = true;
	char cmd[64];

	char buffer[128];
	memset(buffer, 0x0, sizeof(buffer));

	char ch = 0;
	int idx = 0;
	int found = 1;

	if(!mdm_isPowerON()){
		mdm_powerOFF();
		mdm_powerON();
	}

	ret = resource_serial_init();
	if (ret == false) {
		LOGE("Failed to resource_serial_init");
		return found;
	}

	if(_enable)
		strcpy(cmd, "AT+QIACT=1\r");
	else
		strcpy(cmd, "AT+QIDEACT=1\r");


	ret = resource_write_data(cmd, strlen(cmd));
	if (ret == false) {
		LOGE("Failed to resource_serial_write");
		return found;
	}

	float Timeout = 3.0;
	static double cTime;

	cTime = ecore_time_get();

	char filter[] = "OK";
	char *checkFilter = NULL;

	peripheral_error_e _ret = PERIPHERAL_ERROR_NONE;

	do{
		_ret = peripheral_uart_read(g_uart_h, &ch, 1);
		if (_ret == PERIPHERAL_ERROR_NONE)
		{
			if(ch != '\r'){
				buffer[idx++] = ch;
			}
			else {
				/* Debug */
#if 0
				for(int i=0; i<idx; i++)
				{
					LOGE("check String [%d] = %c", i, buffer[i]);
				}
#endif
				checkFilter = strstr(buffer, filter);
				if(checkFilter == NULL)
					idx = 0;
				else
					found = 0;
			}
		}
		// if data is not ready, try again
		if (_ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			usleep(100 * 1000);
			LOGI(".");
			continue;
		}
	}while( (Timeout>= (ecore_time_get() - cTime) ) && checkFilter == NULL);

#if 0
	for(int i=0; i<idx; i++)
	{
		LOGE("uart read buffer[%d] = %c", i, buffer[i]);
	}
#endif
	resource_serial_fini();
	LOGI("MDM Test Finished...");

	return found;
}
