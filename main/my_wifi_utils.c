#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

#include "my_wifi_utils.h"

#include "server.h"

#define TAG "HTTPCLIENT"
#define HEADERTAG "HEADERTAG"

#define SSID "NETGEAR_EXT"
#define PASSWORD "ne11ieh@y"


char *buffer = NULL;
int indexBuffer = 0;

//extern xSemaphoreHandle connectionSemaphore;



//events raised during the HTTP interaction - ie GET / POST
esp_err_t clientEventHandler(esp_http_client_event_t *evt) {

    struct GetParms *getParams = (struct GetParms *)evt->user_data;

    switch (evt->event_id) {

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(HEADERTAG, "Key: %s :%s", (char*)evt->header_key, (char*)evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA Len=%d", evt->data_len);
            if (buffer == NULL){
                buffer = (char *)malloc(evt->data_len);
            } else {
                buffer = (char *)realloc(buffer, evt->data_len + indexBuffer);
            }
            memcpy(&buffer[indexBuffer], evt->data, evt->data_len);
            indexBuffer += evt->data_len;
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "Finished. Total length is %d bytes",indexBuffer);
          
            buffer = (char *)realloc(buffer, indexBuffer + 1);
            memcpy(&buffer[indexBuffer], "\0", 1);


            ESP_LOGI(TAG,"%s", buffer);

            getParams->OnGotData(buffer, getParams->message);
            
            //re-init for next call
            free(buffer);
            buffer = NULL;
            indexBuffer = 0;

            break;

        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "Error");
            break;
        default:
            break;
    }

    return ESP_OK;
}


static void set_static_ip(esp_netif_t *netif){
    //stop the DHCP client
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));

    //set the static IP address
    esp_netif_ip_info_t ip_info;

    // IP4_ADDR(&ip_info.ip, 192, 168, 20, 99);
    //set the gateway and netmask which will always be the same...

   	IP4_ADDR(&ip_info.gw, 192, 168, 20, 1);
   	IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);   

    esp_ip4_addr_t IPAddress;
    //set the default IP address (decimal format)
    IPAddress.addr = esp_ip4addr_aton("192.168.20.99");

    //see if there is an IP address set in nvs
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        //if there's an error, we'll just use the default IP set above
        ESP_LOGI(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));

        //printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        uint32_t savedIP = 0;
        if ( nvs_get_u32(my_handle, "saved_ip", &savedIP) == ESP_OK) {
            IPAddress.addr = savedIP;
            //dIP = savedIP;
            //printf("Setting IP to %d from nvs",savedIP);
            ESP_LOGI(TAG,"Setting IP to %d from nvs",savedIP);
        } else {
            ESP_LOGI(TAG,"No nvs entry for 'saved_ip' - using default");
        }
    }

    //Now can set the IP info...
    ip_info.ip = IPAddress;
    //...and save
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
    
    //set the IP address
    //esp_ip4_addr_t x;
    //x.addr = dIP; //1679075520;  //192.168.20.100
    //ip_info.ip = x;



    //set the dns
    esp_netif_dns_info_t dns;
    IP4_ADDR(&dns.ip.u_addr.ip4, 8, 8, 8, 8);
    dns.ip.type = IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
}

//events raised during wifi inialization...
static void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{

ESP_LOGI(TAG,"Wift event: %d",event_id);

  switch (event_id)
  {

    case SYSTEM_EVENT_STA_WPS_ER_PIN:
        ESP_LOGI(TAG,"SYSTEM_EVENT_STA_WPS_ER_PIN\n");
       //  esp_wifi_connect(); 
        break;
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG,"connecting...\n");
        esp_wifi_connect(); 
        break; 

    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG,"connected\n");

        if (event_base == WIFI_EVENT) {
            ESP_LOGI(TAG,"setting ip\n");
            set_static_ip(event_handler_arg);
        }

        break;


    case IP_EVENT_STA_GOT_IP:
        //when there's an IP we're good to go...
        ESP_LOGI(TAG,"got ip\n");

        //display the IP address
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_netif_t *netif = event->esp_netif;
        ESP_LOGI(TAG, "IP address:" IPSTR, IP2STR(&event->ip_info.ip));

        //display DNS
        esp_netif_dns_info_t dns_config;
        memset(&dns_config, 0 , sizeof(esp_netif_dns_info_t));
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN,  &dns_config);
        ESP_LOGI(TAG, "DNS:" IPSTR, IP2STR(&dns_config.ip.u_addr.ip4));

        StartServer();

        break;

    default:
        ESP_LOGI(TAG, "Unhandled event:%d",event_id);

    }




}



void myWifiInit() {
    //assume nvs_flash_init() set in main script (as may be used for other purposes)

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif_sta = esp_netif_create_default_wifi_sta();

    //initialize the wifi
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    //handle specific events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event_handler,netif_sta));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event_handler,netif_sta));

    //use RAM (rather than flash) for internal use of the wifi stack
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));


    //configuration for the station mode

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD
        }
    };


    //set the wifi mode. If not, it doesn't seem to connect properly...
    esp_wifi_set_mode(WIFI_MODE_STA);
    //esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    //and start'er up!
    ESP_ERROR_CHECK(esp_wifi_start());
}

//send a string to a server via POST
void myPOST(char *url, char *data){
    esp_http_client_config_t clientConfig = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = clientEventHandler};

    esp_http_client_handle_t client = esp_http_client_init(&clientConfig);

    //esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, data, strlen(data));

    esp_err_t err = esp_http_client_perform(client);
   
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    /*

const char *post_data = "{\"channel\":\"A\",\"value\":\"X\"}";
esp_http_client_set_method(client, HTTP_METHOD_POST);
esp_http_client_set_post_field(client, post_data, strlen(post_data));
//esp_http_client_set_header(client, "Content-Type", "application/json");
esp_err_t err = esp_http_client_perform(client);

*/

}

void myGET(char *url,struct GetParms *getParms){

    int mem = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Largest heap available: %d",mem);

    ESP_LOGI(TAG, "url = %s,",url);
   

    esp_http_client_config_t clientConfig = {
        .url = url,
        .event_handler = clientEventHandler,
        .user_data = getParms
        };

    esp_http_client_handle_t client = esp_http_client_init(&clientConfig);

    esp_err_t err = esp_http_client_perform(client);
   
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

//========= useful links...
//https://esp32.com/viewtopic.php?t=14689
//https://github.com/espressif/esp-idf/blob/f65c8249af109de349650b9cf79ae28399261750/examples/protocols/static_ip/main/static_ip_example_main.c
