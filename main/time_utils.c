#include "time_utils.h"

#include <string.h>

#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "Time utils";

static char start_time[64];

char* get_current_local_time() {
    char *time_buf = malloc(64 * sizeof(char));
    time_t now = 0;
    struct tm timeinfo = { 0 };
    
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(time_buf, 64, "%c %Z", &timeinfo);
    return time_buf;
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    if (start_time[0] == '\0')
    {
        char* str = get_current_local_time();
        strncpy(start_time, str, 64);
        ESP_LOGI(TAG, "The start date/time is: %s", start_time);
        free(str);
        str = get_current_local_time();
        strncpy(start_time, str, 64);
        free(str);
    }

}

char* get_start_time() {
    return start_time;
}