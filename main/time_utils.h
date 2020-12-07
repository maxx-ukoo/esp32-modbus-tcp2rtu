#ifndef TIME_UTILS_H
#define TIME_UTILS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_sntp.h"


void time_sync_notification_cb(struct timeval *tv);
char* get_current_local_time();
char* get_start_time();

#ifdef __cplusplus
}
#endif

#endif /* TIME_UTILS_H */