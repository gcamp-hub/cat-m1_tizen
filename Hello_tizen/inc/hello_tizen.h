#ifndef __hello_tizen_H__
#define __hello_tizen_H__

#include <dlog.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "hello_tizen"

void pwm_motor_test_main(void);
int  spi_gyro_test_main(void);

int mdm_init(void);
int mdm_IsRegistred(void);
int mdm_getIMEI(char *imei, int length);
int mdm_pdpAct(bool _enable);

int mdm_socketOpen(char *IP, int port, bool isTCP);
void mdm_socketClose(void);
int mdm_socketSend(char *sendMsg, int length);
int mdm_socketRecv(char *recvMsg, int length);

int mdm_powerON(void);
int mdm_powerOFF(void);


#endif /* __hello_tizen_H__ */
