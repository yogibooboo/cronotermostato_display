/**
 * @file http_server.h
 * @brief HTTP server for Cronotermostato web interface
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start HTTP server (if not already running)
 */
void start_webserver_if_not_running(void);

/**
 * @brief Stop HTTP server
 */
void stop_webserver(void);

/**
 * @brief Check if server is running
 * @return true if server is running
 */
bool is_webserver_running(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H
