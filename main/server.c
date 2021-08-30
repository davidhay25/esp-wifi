#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

#define TAG "SERVER"

static esp_err_t on_get_info(httpd_req_t *req) {
    ESP_LOGI(TAG, "url %s was hit", req->uri);

    int mem = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    int total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    int free = heap_caps_get_free_size(MALLOC_CAP_8BIT);


    char *json = NULL;
    //cJSON *memory = NULL;
    cJSON *summary = cJSON_CreateObject();
    cJSON_AddStringToObject(summary, "name", "ESP32");      //todo - store in menu
    cJSON *memory = cJSON_AddObjectToObject(summary,"memory");

    cJSON_AddNumberToObject(memory,"total",total);
    cJSON_AddNumberToObject(memory,"free",free);
    cJSON_AddNumberToObject(memory,"largestFreeBlock",mem);

    json = cJSON_Print(summary);

    httpd_resp_set_hdr(req, "Content-Type", "application/text+json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}


void StartServer(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "starting server");
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "COULD NOT START SERVER");
    }

    httpd_uri_t info_end_point_config = {
        .uri = "/api/info",
        .method = HTTP_GET,
        .handler = on_get_info};

    httpd_register_uri_handler(server, &info_end_point_config);

}
