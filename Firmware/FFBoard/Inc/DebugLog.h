
#ifndef DEBUGLOG_H_
#define DEBUGLOG_H_

#include "target_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// LOG DEBUG
#ifdef HW_ESP32SX
#define FFB_LOGI(format, ...) do {                                          \
            ESP_LOGI(__FUNCTION__, "%s:%d -- " format, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
    } while(0)
#define FFB_LOGW(format, ...) do {                                          \
            ESP_LOGW(__FUNCTION__, "%s:%d -- " format, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
    } while(0)
#define FFB_LOGE(format, ...) do {                                          \
            ESP_LOGE(__FUNCTION__, "%s:%d -- " format, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
    } while(0)
#define FFB_LOGD(format, ...) do {                                          \
            ESP_LOGD(__FUNCTION__, "%s:%d -- " format, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
    } while(0)

#else

#define FFB_LOGI(format, ...) 
#define FFB_LOGW(format, ...) 
#define FFB_LOGE(format, ...) 
#define FFB_LOGD(format, ...) 

#endif


#ifdef __cplusplus
}
#endif

#endif /* DEBUGLOG_H_ */
