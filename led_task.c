#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "iot_helper.h"
#include "led_task.h"
#include "sdkconfig.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#define ESP32
#endif

static const char *TAG = "LED";

// LED queue
QueueHandle_t  led_queue = NULL;

TaskHandle_t led_task;

static led_status leds[MAX_LEDS];

static int iter = 0;

void led_loop(void *pvParameter) {
    led_cmd cmd;

    ESP_LOGI(TAG,"[LED] Running on core #%d", xPortGetCoreID());
#ifdef ESP32
    esp_task_wdt_add(NULL);
#endif

    ESP_LOGD(TAG, "stack: %d", uxTaskGetStackHighWaterMark(NULL));

    while(1) {
        if (led_queue != NULL) {

            for (int i=0;i<MAX_LEDS;i++) {
                switch (leds[i].state) {
                    case LED_OFF:
                        if (leds[i].pulses_left != 0) {  // pulses_left can be -1 (infinite) or >0 if we continue pulsing
                            if (iot_timestamp() - leds[i].timestamp > leds[i].off_time) {
                                node_handle_led_set(i, leds[i].r, leds[i].g, leds[i].b);
                                leds[i].state = LED_ON;
                                if (leds[i].pulses_left != -1)
                                    leds[i].pulses_left--;
                                leds[i].timestamp = iot_timestamp();
                            }
                        } else if (leds[i].bursts_left != 0) { // bursts_left can be -1 (infinite) or >0 if we continue bursting
                            if (iot_timestamp() - leds[i].timestamp > leds[i].burst_interval) {
                                node_handle_led_set(i, leds[i].r, leds[i].g, leds[i].b);
                                leds[i].state = LED_ON;
                                if (leds[i].bursts_left != -1)
                                    leds[i].bursts_left--;
                                leds[i].pulses_left = leds[i].pulse_count - 1;
                                leds[i].timestamp = iot_timestamp();
                            }
                        }
                        break;
                    case LED_ON:
                        if (leds[i].on_time != -1) {
                            if (iot_timestamp() - leds[i].timestamp > leds[i].on_time) {
                                node_handle_led_set(i, 0, 0, 0);
                                leds[i].state = LED_OFF;
                                if ( (leds[i].pulses_left != 0) || (leds[i].bursts_left != 0) ) {
                                    leds[i].timestamp = iot_timestamp();
                                } else {
                                    // end of pulses/bursts
                                    leds[i].priority = 0;
                                }
                            }
                        }
                        break;
                    default:
                        break;
                }
            }

            if (xQueueReceive(led_queue,&cmd,(TickType_t )(20/portTICK_PERIOD_MS))) {
                ESP_LOGD(TAG, "queue rcv stack: %d", uxTaskGetStackHighWaterMark(NULL));
                //ESP_LOGI(TAG,"led %d : %d %d %d (%d ms)", cmd.index, cmd.r, cmd.g, cmd.b, cmd.on_time);
                bool allowed=true;

                if (cmd.priority==-1) {
                    ESP_LOGI(TAG,"led (PRIORITY RELEASED) %d : %d %d %d (%d ms)", cmd.index, cmd.r, cmd.g, cmd.b, cmd.on_time);
                    leds[cmd.index].priority = 0;
                } else if (cmd.priority >= leds[cmd.index].priority) {
                    if (cmd.priority>0) {
                        ESP_LOGI(TAG,"led (PRIORITY %d) %d : %d %d %d (%d ms)", cmd.priority, cmd.index, cmd.r, cmd.g, cmd.b, cmd.on_time);                        
                    } else {
                        ESP_LOGI(TAG,"led %d : %d %d %d (%d ms)", cmd.index, cmd.r, cmd.g, cmd.b, cmd.on_time);
                    }
                    leds[cmd.index].priority = cmd.priority;
                } else {
                    ESP_LOGE(TAG,"led (LOWER PRIORITY-> DO NOT SET) %d : %d %d %d (%d ms)", cmd.index, cmd.r, cmd.g, cmd.b, cmd.on_time);
                    allowed=false;
                }

                if (allowed) {
                    node_handle_led_set(cmd.index, cmd.r, cmd.g, cmd.b);
                    leds[cmd.index].state = LED_ON;
                    leds[cmd.index].r = cmd.r;
                    leds[cmd.index].g = cmd.g;
                    leds[cmd.index].b = cmd.b;
                    leds[cmd.index].on_time = cmd.on_time;
                    leds[cmd.index].off_time = cmd.off_time;
                    leds[cmd.index].pulse_count = cmd.pulse_count;
                    leds[cmd.index].pulses_left = cmd.pulse_count;
                    // this is the first pulse
                    if (leds[cmd.index].pulses_left > 0)
                        leds[cmd.index].pulses_left--;
                    leds[cmd.index].bursts_left = cmd.burst_count;
                    // this is the first burst
                    if (leds[cmd.index].bursts_left > 0)
                        leds[cmd.index].bursts_left--;
                    leds[cmd.index].burst_interval = cmd.burst_interval;
                    leds[cmd.index].on_time = cmd.on_time;
                    leds[cmd.index].timestamp = iot_timestamp();
                }
            }

            iter++;
            if (iter>20) {
                //ESP_LOGI(TAG, "[LED] Stack: %d", uxTaskGetStackHighWaterMark(NULL));
                iter=0;
#ifdef ESP32
                if (esp_task_wdt_status(NULL)==ESP_OK)
                    esp_task_wdt_reset();
#endif
            }
        } else {
            vTaskDelay(100 / portTICK_RATE_MS);            
        }
    }
}

void iot_led_set( uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    led_cmd cmd;
    cmd.index = index;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.on_time = -1;
    cmd.priority = 0;
    cmd.burst_count = 0;
    cmd.pulse_count = 0;
    if (led_queue != NULL) {
        if( xQueueSend( led_queue, ( void * ) &cmd, ( TickType_t ) 10) != pdPASS )
            {
            ESP_LOGE(TAG,"iot_led_set: error queueing led cmd!");
            }
    }
}

void iot_led_set_priority( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int priority) {
    led_cmd cmd;
    cmd.index = index;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.on_time = -1;
    cmd.priority = priority;
    cmd.burst_count = 0;
    cmd.pulse_count = 0;
    if (led_queue != NULL) {
        if( xQueueSend( led_queue, ( void * ) &cmd, ( TickType_t ) 10) != pdPASS )
            {
            ESP_LOGE(TAG,"iot_led_set_exclusive: error queueing led cmd!");
            }
    }
}

void iot_led_burst( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int on_time, int off_time, int pulse_count, int burst_count, int burst_interval, int priority) {
    led_cmd cmd;
    cmd.index = index;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.on_time = on_time;
    cmd.off_time = off_time;
    cmd.pulse_count = pulse_count;
    cmd.burst_count = burst_count;
    cmd.burst_interval = burst_interval;
    cmd.priority = priority;
    if (led_queue != NULL) {
        if( xQueueSend( led_queue, ( void * ) &cmd, ( TickType_t ) 10) != pdPASS )
            {
            ESP_LOGE(TAG,"iot_led_blink: error queueing led cmd!");
            }
    }
}

void iot_led_pulse( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int on_time, int off_time, int pulse_count, int priority) {
    led_cmd cmd;
    cmd.index = index;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.on_time = on_time;
    cmd.off_time = off_time;
    cmd.burst_count = 1;
    cmd.pulse_count = pulse_count;
    cmd.priority = priority;
    cmd.burst_count = 0;
    if (led_queue != NULL) {
        if( xQueueSend( led_queue, ( void * ) &cmd, ( TickType_t ) 10) != pdPASS )
            {
            ESP_LOGE(TAG,"iot_led_blink: error queueing led cmd!");
            }
    }
}

void iot_led_blink( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int on_time, int priority) {
    led_cmd cmd;
    cmd.index = index;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.on_time = on_time;
    cmd.pulse_count = 1;
    cmd.burst_count = 1;
    cmd.priority = priority;
    if ((led_queue != NULL) && (on_time>0)) {
        if( xQueueSend( led_queue, ( void * ) &cmd, ( TickType_t ) 10) != pdPASS )
            {
            ESP_LOGE(TAG,"iot_led_blink: error queueing led cmd!");
            }
    }
}

void led_task_wdt(bool enable) {
#ifdef ESP32
    if (enable) {
        if (esp_task_wdt_status(led_task)==ESP_ERR_NOT_FOUND)
            esp_task_wdt_add(led_task);
    } else {
        if (esp_task_wdt_status(led_task)==ESP_OK)
            esp_task_wdt_delete(led_task);
    }
#endif
}


void iot_led_init() {

    printf("Create led task..\r\n");
#ifdef ESP32
    xTaskCreatePinnedToCore(&led_loop, "led_task", 4096, NULL, LED_TASK_PRIORITY, &led_task, LED_TASK_CORE);
#else
    xTaskCreatePinnedToCore(&led_loop, "led_task", 1024, NULL, LED_TASK_PRIORITY, &led_task, LED_TASK_CORE);
#endif

#ifdef ESP32
    led_task_wdt(true);
#endif

    led_queue = xQueueCreate( 10, sizeof( led_cmd  ) );
    for (int i=0;i<MAX_LEDS;i++) {
        leds[i].state = false;
        leds[i].priority = 0;
    }
}


