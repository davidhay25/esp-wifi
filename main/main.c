#include <stdio.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/queue.h"
#include "driver/gpio.h"

#include "my_wifi_utils.h"
#include "cJSON.h"

#include "server.h"

//The reset pin will reset the IP address to the default (192.169.20.99)
#define PIN_RESET 0
//#define PIN_SWITCH 17
#define PIN_LED 13
//pin s 2 on other esp


xSemaphoreHandle connectionSemaphore;
xSemaphoreHandle ledFlasherSemaphore;

char *TAG = "CONNECTION";
char *TAGJSON = "JSON";

xQueueHandle buttonQueue;

static void IRAM_ATTR gpio_isr_handler(void *args)
{
    int pinNumber = (int)args;
    xQueueSendFromISR(buttonQueue, &pinNumber, NULL);
}

//flashes the LED 
void flashLEDTask(void *params)
{
    //int pinNumber, count = 0;
    while (true) {
    if (xSemaphoreTake(ledFlasherSemaphore, portMAX_DELAY)) {}
      gpio_set_level(PIN_LED, 1);    
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      gpio_set_level(PIN_LED, 0);    
    }
}

//process response from GET message`
void OnGotData(char *incomingBuffer, char* output){
  printf("process response");
  cJSON *payload = cJSON_Parse(incomingBuffer);
  cJSON *resourceType = cJSON_GetObjectItem(payload, "resourceType");

  printf(resourceType->valuestring);

  cJSON_Delete(payload);
}



void resetButtonTask(void *params)
{
    int pinNumber, count = 0;
    while (true)
    {
        if (xQueueReceive(buttonQueue, &pinNumber, portMAX_DELAY))
        {

          gpio_isr_handler_remove(PIN_RESET);
          xSemaphoreGive(ledFlasherSemaphore);    //will be picked up by the led flasher task

           do {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            } while(gpio_get_level(pinNumber) == 1);

          printf("GPIO %d was pressed %d times. The state is %d\n", pinNumber, count++, gpio_get_level(PIN_RESET));


          //remove the configured IP address (so will revert to default) 

           nvs_handle_t my_handle;

          ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
          nvs_erase_key(my_handle,"saved_ip");
          nvs_commit(my_handle);

        gpio_isr_handler_add(PIN_RESET, gpio_isr_handler, (void *)PIN_RESET);
        ESP_LOGI(TAG,"IP has been reset to default. Reboot to enable.");


        /* - keep so the syntax for calling a GET with a callback is preserved...
          struct GetParms getParams;
          getParams.OnGotData = OnGotData;
          myGET("http://home.clinfhir.com:8054/baseR4/Patient/8167",&getParams);
          */

         

        }
    }
}


//activated when the connection semaphore is set when the internet connection is complete
//NOT CURRENTLY USED
void OnConnected(void *para)
{
  while (true)
  {
    if (xSemaphoreTake(connectionSemaphore, 10000 / portTICK_RATE_MS))
    {
      xSemaphoreTake(connectionSemaphore, portMAX_DELAY);
        ESP_LOGI(TAG,"in onconnect");
        //RegisterEndPoints();

          struct GetParms getParams;
          getParams.OnGotData = OnGotData;
          myGET("http://home.clinfhir.com:8054/baseR4/Patient/8167",&getParams);


    }
    else
    {
      ESP_LOGE(TAG, "Failed to connect. Retry in");
      for (int i = 0; i < 15; i++)
      {
        ESP_LOGE(TAG, "...%d", i);
        vTaskDelay(1000 / portTICK_RATE_MS);
      }
      esp_restart();
    }
  }
}

void setUpGPIO() {
    gpio_pad_select_gpio(PIN_LED);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(PIN_RESET);
    gpio_set_direction(PIN_RESET, GPIO_MODE_INPUT);
    gpio_pulldown_en(PIN_RESET);
    // gpio_pullup_en(PIN_SWITCH);
    gpio_pullup_dis(PIN_RESET);

    printf("enabled gpio...");
}

void app_main(void)
{
  printf("Initializing wifi...\n");


  ESP_ERROR_CHECK(nvs_flash_init());

  connectionSemaphore = xSemaphoreCreateBinary();
  ledFlasherSemaphore = xSemaphoreCreateBinary();

  myWifiInit();     //in my_wifi_utils



  //init for switch and LED
  printf("set up GPIO...\n");
  setUpGPIO();

  //xTaskCreate(&OnConnected, "Connected", 1024 * 3, NULL, 3, NULL);

  gpio_set_intr_type(PIN_RESET, GPIO_INTR_POSEDGE);

  buttonQueue = xQueueCreate(10, sizeof(int));

  xTaskCreate(&resetButtonTask, "buttonPushedTask", 4096, NULL, 1, NULL);
  xTaskCreate(&flashLEDTask, "flashLEDTask", 2048, NULL, 5, NULL);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(PIN_RESET, gpio_isr_handler, (void *)PIN_RESET);




  
}
