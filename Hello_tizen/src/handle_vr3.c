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
 ******************************************************************************
 *   @note
 *        This driver is for elechouse Voice Recognition V3 Module
 ******************************************************************************
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <peripheral_io.h>
#include <system_info.h>
#include <unistd.h>
#include <app_common.h>
#include <time.h>
#include "hello.h"
#include "vr3.h"

/* Record for test */
#define onRecord    (1)		/* On record */
#define offRecord   (2)		/* Off record */
#define fwRecord    (3)		/* Forward record */
#define bwRecord    (4)		/* Backward record */

static int timeout = VR_DEFAULT_TIMEOUT;

/** temp data buffer */
uint8_t vr_buf[32];
uint8_t hextab[17]="0123456789ABCDEF";


int millis(void)
{
    // a current time of milliseconds
    return ( clock() * 1000 / CLOCKS_PER_SEC );
}


/**
    @brief send data packet in Voice Recognition module protocol format.
    @param cmd --> command
           subcmd --> subcommand
           buf --> data area
           len --> length of buf
*/
void resource_VR_send_cmd2_pkt(uint8_t cmd, uint8_t subcmd, uint8_t *buf, uint8_t len)
{
	uint8_t data[len+5];

	data[0] = FRAME_HEAD;
	data[1] = len+3;
	data[2] = cmd;
	data[3] = subcmd;
	memcpy(&data[4], buf, len);
	data[len+4] = FRAME_END;

	if (resource_write_data(&data[0], len+5) == false) {
		LOGE("resource_write_data failed");
	}
}

/**
    @brief send data packet in Voice Recognition module protocol format.
    @param cmd --> command
           buf --> data area
           len --> length of buf
*/
void resource_VR_send_cmd_pkt(uint8_t cmd, uint8_t *buf, uint8_t len)
{
	uint8_t data[len+4];

	data[0] = FRAME_HEAD;
	data[1] = len+2;
	data[2] = cmd;
	memcpy(&data[3], buf, len);
	data[len+3] = FRAME_END;

	if (resource_write_data(&data[0], len+4) == false) {
		LOGE("resource_write_data failed");
	}
}

/**
    @brief send data packet in Voice Recognition module protocol format.
    @param buf --> data area
           len --> length of buf
*/
void resource_VR_send_pkt(uint8_t *buf, uint8_t len)
{
	uint8_t data[len+3];

	data[0] = FRAME_HEAD;
	data[1] = len+1;
	memcpy(&data[2], buf, len);
	data[len+2] = FRAME_END;

	if (resource_write_data(&data[0], len+3) == false) {
		LOGE("resource_write_data failed");
	}
}


/**
    @brief receive data .
    @param buf --> return value buffer.
           len --> length expect to receive.
           timeout --> time of reveiving
    @retval number of received bytes, 0 means no data received.
*/
int resource_VR_receive(uint8_t *buf, int len, uint16_t timeout)
{
	int ret;

	if (resource_read_data(buf, len, true) == false) {
		LOGE("resource_read_data failed");
		ret = false;
	}
	ret = len;

	return ret;
}

/**
    @brief receive a valid data packet in Voice Recognition module protocol format.
    @param buf --> return value buffer.
           timeout --> time of reveiving
    @retval '>0' --> success, packet lenght(length of all data in buf)
            '<0' --> failed
*/
int resource_VR_receive_pkt(uint8_t *buf, uint16_t timeout)
{
	int ret;
	ret = resource_VR_receive(buf, 2, timeout);
	if(ret != 2){
		return -1;
	}
	if(buf[0] != FRAME_HEAD){
		return -2;
	}
	if(buf[1] < 2){
		return -3;
	}
	ret = resource_VR_receive(buf+2, buf[1], timeout);
	if(buf[buf[1]+1] != FRAME_END){
		return -4;
	}

	//LOGI(buf, buf[1]+2);

	return buf[1]+2;
}

/** remove duplicates */
int resource_VR_cleanDup(uint8_t *des, uint8_t *buf, int len)
{
	if(len<1){
		return -1;
	}

	int i, j, k=0;
	for(i=0; i<len; i++){
		for(j=0; j<k; j++){
			if(buf[i] == des[j]){
				break;
			}
		}
		if(j==k){
			des[k] = buf[i];
			k++;
		}
	}
	return k;
}


/**
	@brief VR class constructor.
	@param buf --> return data .
	 	buf[0]  -->  Group mode(FF: None Group, 0x8n: User, 0x0n:System
        	buf[1]  -->  number of record which is recognized.
        	buf[2]  -->  Recognizer index(position) value of the recognized record.
        	buf[3]  -->  Signature length
        	buf[4]~buf[n] --> Signature
		timeout --> wait time for receiving packet.
	@retval length of valid data in buf. 0 means no data received.
*/
int handle_VR_recognize(uint8_t *buf, int timeout)
{
	int ret, i;
	ret = resource_VR_receive_pkt(vr_buf, timeout);
	if(vr_buf[2] != FRAME_CMD_VR){
		return -1;
	}
	if(ret > 0){
		for(i = 0; i < (vr_buf[1] - 3); i++){
			buf[i] = vr_buf[4+i];
		}
		return i;
	}

	return 0;
}


/**
    @brief Load records to recognizer.
    @param records --> record data buffer pointer.
           len --> number of records.
           buf --> pointer of return value buffer, optional.
             buf[0]    -->  number of records which are load successfully.
             buf[2i+1]  -->  record number
             buf[2i+2]  -->  record load status.
                00 --> Loaded
                FC --> Record already in recognizer
                FD --> Recognizer full
                FE --> Record untrained
                FF --> Value out of range"
             (i = 0 ~ '(retval-1)/2' )
	@retval '>0' --> length of valid data in buf.
            0 --> success, buf=0, and no data returned.
            '<0' --> failed.
*/
int handle_VR_load(uint8_t *records, uint8_t len, uint8_t *buf)
{
	uint8_t ret;
	resource_VR_send_cmd_pkt(FRAME_CMD_LOAD, records, len);
	ret = resource_VR_receive_pkt(vr_buf, timeout);
	if(ret<=0){
		return -1;
	}
	if(vr_buf[2] != FRAME_CMD_LOAD){
		return -1;
	}
	if(buf != 0){
		memcpy(buf, vr_buf+3, vr_buf[1]-2);
		return vr_buf[1]-2;
	}
	return 0;
}

/**
    @brief Load one record to recognizer.
    @param record --> record value.
           buf --> pointer of return value buffer, optional.
             buf[0]    -->  number of records which are load successfully.
             buf[2i+1]  -->  record number
             buf[2i+2]  -->  record load status.
                00 --> Loaded
                FC --> Record already in recognizer
                FD --> Recognizer full
                FE --> Record untrained
                FF --> Value out of range"
             (i = 0 ~ '(retval-1)/2' )
	@retval '>0' --> length of valid data in buf.
            0 --> success, buf=0, and no data returned.
            '<0' --> failed.
*/
int handle_VR_load_one(uint8_t record, uint8_t *buf)
{
	uint8_t ret;
	resource_VR_send_cmd_pkt(FRAME_CMD_LOAD, &record, 1);
	ret = resource_VR_receive_pkt(vr_buf, timeout);
	if(ret<=0){
		return -1;
	}
	if(vr_buf[2] != FRAME_CMD_LOAD){
		return -1;
	}
	if(buf != 0){
		memcpy(buf, vr_buf+3, vr_buf[1]-2);
		return vr_buf[1]-2;
	}
	return 0;
}

/**
    @brief set autoload.
    @param records --> record buffer.
           len --> records length.
    @retval 0 --> success
            -1 --> failed
*/
int handle_VR_setAutoLoad(uint8_t *records, uint8_t len)
{
	int ret;
	uint8_t map;
	if(len == 0 && records == 0){
		map = 0;
	}else if(len != 0 && records != 0){
		map = 0;
		for(int i=0; i<len; i++){
			map |= (1<<i);
		}
	}else{
		return -1;
	}

	resource_VR_send_cmd2_pkt(FRAME_CMD_SET_AL, map, records, len);
	ret = resource_VR_receive_pkt(vr_buf, timeout);
	if(ret<=0){
		return -1;
	}
	if(vr_buf[2] != FRAME_CMD_SET_AL){
		return -1;
	}
	return 0;
}

/**
    @brief disable autoload.
    @param records --> record buffer.
           len --> records length.
    @retval 0 --> success
            -1 --> failed
*/
int handle_VR_disableAutoLoad(void)
{
	/* record & len to 0 for disable */
    return handle_VR_setAutoLoad(0, 0);
}


/**
    @brief set signature(alias) for a record.
    @param record --> record value.
           buf --> signature buffer.
           len --> length of buf.
    @retval 0 --> success, buf=0, and no data returned.
            '<0' --> failed.
*/
int handle_VR_setSignature(uint8_t record, const void *buf, uint8_t len)
{
	int ret;

	if(len == 0 && buf == 0){
		/** delete signature */
	}else if(len == 0 && buf != 0){
		if(buf == 0){
			return -1;
		}
		len = strlen((char *)buf);
		if(len>10){
			return -1;
		}
	}else if(len != 0 && buf != 0){

	}else{
		return -1;
	}
	resource_VR_send_cmd2_pkt(FRAME_CMD_SET_SIG, record, (uint8_t *)buf, len);
	ret = resource_VR_receive_pkt(vr_buf, timeout);
	if(ret<=0){
		return -1;
	}
	if(vr_buf[2] != FRAME_CMD_SET_SIG){
		return -1;
	}
	return 0;
}

/**
    @brief delete signature(alias) of a record.
    @param record --> record value.
    @retval  0 --> success
            -1 --> failed
*/
int handle_VR_deleteSignature(uint8_t record)
{
	/* buf & len to 0 for delete */
    return handle_VR_setSignature(record, 0, 0);
}

/**
    @brief check the signature(alias) of a record.
    @param record --> record value.
           buf --> signature, return value buffer.
    @retval '>0' --> length of valid data in buf.
            0 --> success, buf=0, and no data returned.
            '<0' --> failed.
*/
int handle_VR_checkSignature(uint8_t record, uint8_t *buf)
{
	int ret;
	if(record < 0){
		return -1;
	}
	resource_VR_send_cmd2_pkt(FRAME_CMD_CHECK_SIG, record, 0, 0);
	ret = resource_VR_receive_pkt(vr_buf, timeout);

	if(ret<=0){
		return -1;
	}
	if(vr_buf[2] != FRAME_CMD_CHECK_SIG){
		return -1;
	}

	if(vr_buf[4]>0){
		memcpy(buf, vr_buf+5, vr_buf[4]);
		return vr_buf[4];
	}else{
		return 0;
	}
}

/**
    @brief clear recognizer.
    @retval  0 --> success
            -1 --> failed
*/
int handle_VR_clear(void)
{
	int len;

	resource_VR_send_cmd_pkt(FRAME_CMD_CLEAR, 0, 0);
	len = resource_VR_receive_pkt(vr_buf, timeout);
	if(len<=0){
		LOGE("Module Clear Fail...");
		return -1;
	}

	if(vr_buf[2] != FRAME_CMD_CLEAR){
		LOGE("Module Clear Fail...");
		return -1;
	}

	LOGI("VR Module Cleared");
	return 0;
}

/**
    @brief clear recognizer.
    @param buf --> return value buffer.
             buf[0]     -->  Number of valid voice records in recognizer
             buf[i+1]   -->  Record number.(0xFF: Not loaded(Nongroup mode), or not set (Group mode))
                (i= 0, 1, ... 6)
             buf[8]     -->  Number of all voice records in recognizer
             buf[9]     -->  Valid records position indicate.
             buf[10]    -->  Group mode indicate(FF: None Group, 0x8n: User, 0x0n:System
    @retval '>0' --> success, length of data in buf
            -1 --> failed
*/
int handle_VR_checkRecognizer(uint8_t *buf)
{
	int len;
	resource_VR_send_cmd_pkt(FRAME_CMD_CHECK_BSR, 0, 0);
	len = resource_VR_receive_pkt(vr_buf, timeout);
	if(len<=0){
		return -1;
	}

	if(vr_buf[2] != FRAME_CMD_CHECK_BSR){
		return -1;
	}

	if(vr_buf[1] != 0x0D){
		return -1;
	}

	memcpy(buf, vr_buf+3, vr_buf[1]-2);

	return vr_buf[1]-2;
}

/**
    @brief check record train status.
    @param buf --> return value
             buf[0]     -->  Number of checked records
             buf[2i+1]  -->  Record number.
             buf[2i+2]  -->  Record train status. (00: untrained, 01: trained, FF: record value out of range)
             (i = 0 ~ buf[0]-1 )
    @retval Number of trained records
*/
int handle_VR_checkRecord(uint8_t *buf, uint8_t *records, uint8_t len)
{
	int ret;
	int cnt = 0;
	unsigned long start_millis;
	if(records == 0 && len==0){
        memset(buf, 0xF0, 255);
		resource_VR_send_cmd2_pkt(FRAME_CMD_CHECK_TRAIN, 0xFF, 0, 0);
		start_millis = millis();
		while(1){
			len = resource_VR_receive_pkt(vr_buf, timeout);
			if(len>0){
				if(vr_buf[2] == FRAME_CMD_CHECK_TRAIN){
                    for(int i=0; i<vr_buf[1]-3; i+=2){
                        buf[vr_buf[4+i]]=vr_buf[4+i+1];
                    }
					cnt++;
					if(cnt == 51){
						return vr_buf[3];
					}
				}else{
					return -3;
				}
				start_millis = millis();
			}

			if(millis()-start_millis > 500){
				if(cnt>0){
					buf[0] = cnt*5;
					return vr_buf[3];
				}
				return -2;
			}

		}

	}else if(len>0){
		ret = resource_VR_cleanDup(vr_buf, records, len);
		resource_VR_send_cmd_pkt(FRAME_CMD_CHECK_TRAIN, vr_buf, ret);
		ret = resource_VR_receive_pkt(vr_buf, timeout);
		if(ret>0){
			if(vr_buf[2] == FRAME_CMD_CHECK_TRAIN){
				memcpy(buf+1, vr_buf+4, vr_buf[1]-3);
				buf[0] = (vr_buf[1]-3)/2;
				return vr_buf[3];
			}else{
				return -3;
			}
		}else{
			return -1;
		}
	}else{
		return -1;
	}

}

/****************************************************************************/
/******************************* VR3 CONTROL ********************************/

/**
  @brief   Print signature, if the character is invisible,
           print hexible value instead.
  @param   buf     --> command length
           len     --> number of parameters
*/

void resource_VR_setup(void)
{
	LOGI("Elechouse Voice Recognition V3 Module\r\nControl PWM Motor sample");

	if(handle_VR_clear() == 0){
		LOGI("Recognizer cleared.");
	}else{
		LOGI("Not find VoiceRecognitionModule.");
		LOGI("Please check connection and restart Arduino.");
		while(1);
	}

	if(handle_VR_load_one((uint8_t)onRecord, 0) >= 0){
		LOGI("onRecord loaded");
	}

	if(handle_VR_load_one((uint8_t)offRecord, 0) >= 0){
		LOGI("offRecord loaded");
	}

	if(handle_VR_load_one((uint8_t)fwRecord, 0) >= 0){
		LOGI("fwRecord loaded");
	}

	if(handle_VR_load_one((uint8_t)bwRecord, 0) >= 0){
		LOGI("bwRecord loaded");
	}
}


/**
  @brief   Print signature, if the character is invisible,
           print hexible value instead.
  @param   buf  -->  VR module return value when voice is recognized.
             buf[0]  -->  Group mode(FF: None Group, 0x8n: User, 0x0n:System
             buf[1]  -->  number of record which is recognized.
             buf[2]  -->  Recognizer index(position) value of the recognized record.
             buf[3]  -->  Signature length
             buf[4]~buf[n] --> Signature
*/
void resource_VR_printVR(uint8_t *buf)
{
	char group[10], signature[10];

	LOGI("VR Index\tGroup\tRecordNum\tSignature");
	memset(signature,0,10);

	if(buf[0] == 0xFF){
		sprintf (group, "NONE");
	}
	else if(buf[0]&0x80){
		sprintf (group, "UG %d", buf[0]&(~0x80));
	}
	else{
		sprintf (group, "SG %d", buf[0]);
	}

	if(buf[3]>0){
		strncpy(signature, (char *)(buf+4), buf[3]);
	}
	else{
		sprintf (signature, "NONE");
	}

	LOGI("%d\t\t %s\t %d\t\t %s\n", buf[2], group, buf[1], signature);

}

void handle_VR_loop_check(void)
{
	int ret;
	uint8_t buf[64];

	ret = handle_VR_recognize(buf, 50);

	if(ret>0){
		switch(buf[1]){
			case onRecord:
				/** turn on */
				LOGI("Record function On ");
				break;

			case offRecord:
				/** turn off */
				LOGI("Record function Off ");
				break;

			case fwRecord:
				/** Forward */
				LOGI("Record function Forward ");
				break;

			case bwRecord:
				/** Backward */
				LOGI("Record function Backword ");
				break;

			default:
				LOGI("Record function undefined");
				break;
		}

		/* return to center */
//		resource_motor_driving(2);

		/** voice recognized */
		resource_VR_printVR(buf);
	}
}
