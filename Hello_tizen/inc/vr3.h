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

#ifndef _VR3_H_
#define _VR3_H_

#define VR_DEFAULT_TIMEOUT				(1000)

/***************************************************************************/
#define FRAME_HEAD						(0xAA)
#define FRAME_END						(0x0A)

/***************************************************************************/
#define FRAME_CMD_CHECK_SYSTEM			(0x00)
#define FRAME_CMD_CHECK_BSR				(0x01)
#define FRAME_CMD_CHECK_TRAIN			(0x02)
#define FRAME_CMD_CHECK_SIG				(0x03)

#define FRAME_CMD_RESET_DEFAULT			(0x10)	//reset configuration
#define FRAME_CMD_SET_BR				(0x11)	//baud rate
#define FRAME_CMD_SET_IOM				(0x12)	//IO mode
#define FRAME_CMD_SET_PW				(0x13)	//pulse width
#define FRAME_CMD_RESET_IO				(0x14)	// reset IO OUTPUT
#define FRAME_CMD_SET_AL				(0x15)	// Auto load

#define FRAME_CMD_TRAIN					(0x20)
#define FRAME_CMD_SIG_TRAIN				(0x21)
#define FRAME_CMD_SET_SIG				(0x22)

#define FRAME_CMD_LOAD					(0x30)	//Load N records
#define FRAME_CMD_CLEAR					(0x31)	//Clear BSR buffer
#define FRAME_CMD_GROUP					(0x32)  //
	#define FRAME_CMD_GROUP_SET			(0x00)  //
	#define FRAME_CMD_GROUP_SUGRP			(0x01)  //
	#define FRAME_CMD_GROUP_LSGRP			(0x02)  //
	#define FRAME_CMD_GROUP_LUGRP			(0x03)  //
	#define FRAME_CMD_GROUP_CUGRP			(0x04)  //

#define FRAME_CMD_TEST					(0xEE)
	#define FRAME_CMD_TEST_READ				(0x01)
	#define FRAME_CMD_TEST_WRITE			(0x00)


#define FRAME_CMD_VR					(0x0D)	//Voice recognized
#define FRAME_CMD_PROMPT				(0x0A)
#define FRAME_CMD_ERROR					(0xFF)

/***************************************************************************/
// #define FRAME_ERR_UDCMD				(0x00)
// #define FRAME_ERR_LEN				(0x01)
// #define FRAME_ERR_DATA				(0x02)
// #define FRAME_ERR_SUBCMD				(0x03)

// //#define FRAME_ERR_
// #define FRAME_STA_SUCCESS				(0x00)
// #define FRAME_STA_FAILED				(0xFF)
/***************************************************************************/

typedef enum{
	PULSE = 0,
	TOGGLE = 1,
	SET = 2,
	CLEAR = 3
}io_mode_t;

typedef enum{
	LEVEL0 = 0,
	LEVEL1,
	LEVEL2,
	LEVEL3,
	LEVEL4,
	LEVEL5,
	LEVEL6,
	LEVEL7,
	LEVEL8,
	LEVEL9,
	LEVEL10,
	LEVEL11,
	LEVEL12,
	LEVEL13,
	LEVEL14,
	LEVEL15,
}pulse_width_level_t;

typedef enum{
	GROUP0 = 0,
	GROUP1,
	GROUP2,
	GROUP3,
	GROUP4,
	GROUP5,
	GROUP6,
	GROUP7,
	GROUP_ALL = 0xFF,
} group_t;


/* 
 * resource for serial port 
 */
bool resource_serial_init(void);
bool resource_write_data(uint8_t *data, uint32_t length);
bool resource_read_data(uint8_t *data, uint32_t length, bool blocking_mode);
void resource_serial_fini(void);

/* 
 * resource for VR3 Voice Recognition Module 
 */
int millis(void);
void resource_VR_send_cmd2_pkt(uint8_t cmd, uint8_t subcmd, uint8_t *buf, uint8_t len);
void resource_VR_send_cmd_pkt(uint8_t cmd, uint8_t *buf, uint8_t len);
void resource_VR_send_pkt(uint8_t *buf, uint8_t len);
int resource_VR_receive(uint8_t *buf, int len, uint16_t timeout);
int resource_VR_receive_pkt(uint8_t *buf, uint16_t timeout);
int resource_VR_cleanDup(uint8_t *des, uint8_t *buf, int len);
void resource_VR_setup(void);
void resource_VR_printVR(uint8_t *buf);


/* 
 * handle VR3 Voice Recognition Module 
 */
int handle_VR_recognize(uint8_t *buf, int timeout);
int handle_VR_load(uint8_t *records, uint8_t len, uint8_t *buf);
int handle_VR_load_one(uint8_t record, uint8_t *buf);
int handle_VR_setAutoLoad(uint8_t *records, uint8_t len);
int handle_VR_disableAutoLoad(void);
int handle_VR_setSignature(uint8_t record, const void *buf, uint8_t len);
int handle_VR_deleteSignature(uint8_t record);
int handle_VR_checkSignature(uint8_t record, uint8_t *buf);
int handle_VR_clear(void);
int handle_VR_checkRecognizer(uint8_t *buf);
int handle_VR_checkRecord(uint8_t *buf, uint8_t *records, uint8_t len);
void handle_VR_loop_check(void);


/* ---------------------------------------------------------------------------------------
 * ---------------------------------------------------------------------------------------
 */

/******************************* TRAIN CONTROL ******************************/
int handle_VR_train(uint8_t *records, uint8_t len, uint8_t *buf);
int handle_VR_train_one(uint8_t record, uint8_t *buf);
int handle_VR_trainWithSignature(uint8_t record, const void *buf, uint8_t len, uint8_t * retbuf);

/******************************* GROUP CONTROL ******************************/
int handle_VR_setGroupControl(uint8_t ctrl);
int handle_VR_checkGroupControl(void);
int handle_VR_setUserGroup(uint8_t grp, uint8_t *records, uint8_t len);
int handle_VR_checkUserGroup(uint8_t grp, uint8_t *buf);
int handle_VR_loadSystemGroup(uint8_t grp, uint8_t *buf);
int handle_VR_loadUserGroup(uint8_t grp, uint8_t *buf);
int handle_VR_restoreSystemSettings(void);
int handle_VR_checkSystemSettings(uint8_t* buf);

/***************************** RESOURCE CONTROL *****************************/
int resource_VR_setBaudRate(unsigned long br);
int resource_VR_setIOMode(io_mode_t mode);
int resource_VR_resetIO(uint8_t *ios, uint8_t len);
int resource_VR_setPulseWidth(uint8_t level);
int resource_VR_test(uint8_t cmd, uint8_t *bsr);
int resource_VR_writehex(uint8_t *buf, uint8_t len);


#endif /* _VR3_H_ */
