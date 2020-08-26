/*
 * hello.h
 *
 *  Created on: Dec 23, 2019
 *      Author: byoungnamjang
 */

#ifndef HELLO_H_
#define HELLO_H_

#include <dlog.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "HELLO"
#define LOGE(fmt, args...) dlog_print(DLOG_ERROR, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)
#define LOGW(fmt, args...) dlog_print(DLOG_WARN, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)
#define LOGI(fmt, args...) dlog_print(DLOG_INFO, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)
#define LOGD(fmt, args...) dlog_print(DLOG_DEBUG, LOG_TAG, "%s : %s(%d) > "fmt"\n", rindex(__FILE__, '/') + 1, __func__, __LINE__, ##args)

#define START dlog_print(DLOG_DEBUG, LOG_TAG, "%s : %s(%d) >>>>>>>> called", rindex(__FILE__, '/') + 1, __func__, __LINE__)
#define END dlog_print(DLOG_DEBUG, LOG_TAG, "%s : %s(%d) <<<<<<<< ended", rindex(__FILE__, '/') + 1, __func__, __LINE__)

#ifdef __cplusplus
}
#endif


#endif /* HELLO_H_ */
