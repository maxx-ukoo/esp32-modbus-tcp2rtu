#include "ui_stepper_handler.h"

#include "curtains/curtains.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define CONFIG_STEP_GPIO 23
#define CONFIG_DIR_GPIO 32
#define CONFIG_HI_GPIO 36

static const char *TAG = "UI_STEPPER";

esp_err_t stepper_calibrate_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    char param[32];

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    int curtain = -1;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "curtain", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGD(TAG, "Found URL query parameter => position=%s", param);
                curtain = atoi(param);
            }
        }
        free(buf);
    }
    ESP_LOGD(TAG, "Calibrate : curtain = %d", curtain);
    esp_err_t status = ESP_FAIL;
    if (curtain >= 0) {
        status = IOTCurtains::calibrate(curtain);
    }
    if (status == ESP_OK) {
        ESP_LOGD(TAG, "Calibrating finished");
        httpd_resp_sendstr(req, "OK");
    } else {
        ESP_LOGD(TAG, "Calibrating finished but with FAIL");
        httpd_resp_sendstr(req, "FAIL");        
    }
    return ESP_OK;
}

esp_err_t stepper_move_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    char param[32];

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    int position = 0;
    int curtain = -1;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "position", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGD(TAG, "Found URL query parameter => position=%s", param);
                position = atoi(param);
            }
            if (httpd_query_key_value(buf, "curtain", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGD(TAG, "Found URL query parameter => curtain=%s", param);
                curtain = atoi(param);
            }
        }
        free(buf);
    }
    ESP_LOGD(TAG, "Move : curtain = %d, position = %d%%", curtain, position);
    esp_err_t status = ESP_FAIL;
    if (curtain >= 0) {
        status = IOTCurtains::move(curtain, position);
    }
    if (status == ESP_OK) {
        ESP_LOGD(TAG, "Move finished");
        httpd_resp_sendstr(req, "OK");
    } else {
        ESP_LOGD(TAG, "Move finished but with FAIL");
        httpd_resp_sendstr(req, "FAIL");        
    }
    
    return ESP_OK;
}

esp_err_t stepper_steps_handler(httpd_req_t *req)
{

    char *buf;
    size_t buf_len;
    char param[32];

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    int direction = -1;
    int steps = 0;
    int curtain = -1;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "direction", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGD(TAG, "Found URL query parameter => direction=%s", param);
                direction = atoi(param);
            }
            if (httpd_query_key_value(buf, "steps", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGD(TAG, "Found URL query parameter => steps=%s", param);
                steps = atoi(param);
            }
            if (httpd_query_key_value(buf, "curtain", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGD(TAG, "Found URL query parameter => curtain=%s", param);
                curtain = atoi(param);
            }
        }
        free(buf);
    }
    ESP_LOGD(TAG, "Steps : curtain = %d, direction = %d, steps = %d", curtain, direction, steps);
    esp_err_t status = ESP_FAIL;
    if (steps > 0 && curtain >=0)
    {
        status = IOTCurtains::step(curtain, direction, steps);
    }
    if (status == ESP_OK) {
        ESP_LOGD(TAG, "Steps finished");
        httpd_resp_sendstr(req, "OK");
    } else {
        ESP_LOGD(TAG, "Steps finished but with FAIL");
        httpd_resp_sendstr(req, "FAIL");        
    }
    
    return ESP_OK;
}
