#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "cc.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#define TAG "SERVER"


//handler for setip endpoint
static esp_err_t on_set_ip(httpd_req_t *req) {
    ESP_LOGI(TAG, "memory start : %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    bool canUpdateIP = true;

    char buf[150];  //need to ensure this is long enough! May need dynamic memory allocation in the future...
    memset(&buf, 0, sizeof(buf));
    httpd_req_recv(req, buf, req->content_len);

    cJSON *payload = cJSON_Parse(buf);
    cJSON *ipaddress = cJSON_GetObjectItem(payload, "ipaddress");

    char* newIP = cJSON_GetStringValue(ipaddress);

    //convert ip string - x.x.x.x to int32 version
    uint32_t dIP =  esp_ip4addr_aton(newIP);

    //the IP address couldn't be parsed at all
    if (dIP == IPADDR_NONE) {
        cJSON_AddStringToObject(payload,"error-up","unparsable IP address");
        canUpdateIP = false;
    }

    //convert int32 back to x.x.x.x
    char bufIP[16];
    esp_ip4_addr_t x;
    x.addr = dIP;
    esp_ip4addr_ntoa(&x,bufIP,sizeof(bufIP));
    ESP_LOGI(TAG, "string ip: %s",bufIP);


    if (strcmp(cJSON_GetStringValue(ipaddress),bufIP) != 0) {
         cJSON_AddStringToObject(payload,"error-diff","different IP after round trip");
         canUpdateIP = false;
    }

    char* subnet = "192.168.20";

    //the subnet is wrong
    if (strncmp(newIP,subnet,strlen(subnet)) != 0) {
        char message[100];
        sprintf(message,"subnet must be %s",subnet);
        cJSON_AddStringToObject(payload,"error-snt",message );
        canUpdateIP = false;
    }

    
    //add diagnostics to the JSON to be returned...
    cJSON_AddNumberToObject(payload,"decimalIP",dIP);
    cJSON_AddStringToObject(payload,"convertedIP",bufIP);


    if (canUpdateIP) {
        ESP_LOGI(TAG, "Updating ip: %d",dIP);

        //save the IP in nvs
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err != ESP_OK) {
            //if there's an error, we'll just use the default IP set above
            printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
            cJSON_AddStringToObject(payload,"error","failed to open nvs");
        } else {
            err = nvs_set_u32(my_handle,"saved_ip",dIP);
            if (err != ESP_OK) {
                cJSON_AddStringToObject(payload,"error","IP could not be saved");
            } else {
                cJSON_AddStringToObject(payload,"success","IP has been reset");
            }
        }
   




    }
    
    char* json = cJSON_Print(payload);

    httpd_resp_set_hdr(req, "Content-Type", "application/text+json");
    httpd_resp_send(req, json, strlen(json));

     ESP_LOGI(TAG, "memory before: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
     cJSON_Delete(payload);
    free(json);





    ESP_LOGI(TAG, "memory after:  %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    return ESP_OK;
}


//handler for getinfo endpoint
static esp_err_t on_get_info(httpd_req_t *req) {
    ESP_LOGI(TAG, "url %s was hit", req->uri);

    int largestMem = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    int totalMem = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    int freeMem = heap_caps_get_free_size(MALLOC_CAP_8BIT);


    char *json = NULL;
    //cJSON *memory = NULL;
    cJSON *summary = cJSON_CreateObject();
    cJSON_AddStringToObject(summary, "name", "ESP32");      //todo - store in menu
    cJSON *memory = cJSON_AddObjectToObject(summary,"memory");

    cJSON_AddNumberToObject(memory,"total",totalMem);
    cJSON_AddNumberToObject(memory,"free",freeMem);
    cJSON_AddNumberToObject(memory,"largestFreeBlock",largestMem);

    json = cJSON_Print(summary);



    httpd_resp_set_hdr(req, "Content-Type", "application/text+json");
    httpd_resp_send(req, json, strlen(json));

    cJSON_Delete(summary);
    free(json);
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

    //configure the /api/info endpoint
    httpd_uri_t info_end_point_config = {
        .uri = "/api/info",
        .method = HTTP_GET,
        .handler = on_get_info};

    httpd_register_uri_handler(server, &info_end_point_config);
    ESP_LOGI(TAG, "Wiring endpoint : %s", info_end_point_config.uri);

    //configure the /api/setip endpoint
    httpd_uri_t setip_endpoint_config = {
        .uri = "/api/setip",
        .method = HTTP_POST,
        .handler = on_set_ip};

    httpd_register_uri_handler(server, &setip_endpoint_config);
    ESP_LOGI(TAG, "Wiring endpoint : %s", setip_endpoint_config.uri);
}

//https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_netif.html