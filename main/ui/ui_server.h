/** @file ui_server.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 

#ifndef UI_SERVER_H
#define UI_SERVER_H

#include <esp_https_server.h>

httpd_handle_t ui_http_webserver_start(const char *base_path);

#endif /* UI_SERVER_H */


