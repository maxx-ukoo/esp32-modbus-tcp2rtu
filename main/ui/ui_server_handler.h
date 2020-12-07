/** @file ui_server_handler.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 

#ifndef UI_SERVER_HANDLER_H
#define UI_SERVER_HANDLER_H

#include <esp_https_server.h>
#include "esp_system.h"
#include "esp_vfs.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;


esp_err_t component_control_post_handler(httpd_req_t *req);
esp_err_t modbus_control_post_handler(httpd_req_t *req, char * buf);
esp_err_t mqtt_control_post_handler(httpd_req_t *req, char * buf);
esp_err_t gpio_control_post_handler(httpd_req_t *req, char * buf);
esp_err_t system_reboot_post_handler(httpd_req_t *req, char * buf);
esp_err_t gpio_control_state_post_handler(httpd_req_t *req, char * buf);
esp_err_t gpio_control_level_post_handler(httpd_req_t *req, char * buf);
esp_err_t system_info_get_handler(httpd_req_t *req);
esp_err_t gpio_control_state_get_handler(httpd_req_t *req);


#endif /* UI_SERVER_HANDLER_H */


