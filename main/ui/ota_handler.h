/** @file ota_server.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 
#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <esp_https_server.h>

void ota_system_reboot_task(void * parameter);
static esp_err_t ota_status_handler(httpd_req_t *req);
static esp_err_t ota_upload_firmware_handler(httpd_req_t *req);
static esp_err_t ota_list_files_handler(httpd_req_t *req);
static esp_err_t upload_file_handler(httpd_req_t *req);

esp_err_t ota_post_handler(httpd_req_t *req);
esp_err_t ota_get_handler(httpd_req_t *req);
	
#endif /* OTA_HANDLER_H */