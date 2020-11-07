#ifndef _LED_TASK_H
#define _LED_TASH_H

#define MAX_LEDS 16

#define LED_TASK_CORE 1
//#define LED_TASK_PRIORITY 15 // Higher priority than WiFi in order to bitbang correctly Neopixels on ESP8266
#define LED_TASK_PRIORITY 10 

typedef enum {
	LED_OFF = 0,
	LED_ON = 1,
} led_state;

typedef struct led_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;    
} led_color_t;

typedef struct led_status {
    uint8_t r;
    uint8_t g;
    uint8_t b;
	led_state state;
    int on_time;
    int off_time;
    int priority;
    unsigned long timestamp;
    int pulses_left;
    int pulse_count;
    int bursts_left;
    int burst_interval;
} led_status;

typedef struct led_cmd {
    uint8_t index;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int pulse_count;
    int burst_count;
    int burst_interval;
    int on_time;
    int off_time;
    int priority;
} led_cmd;

void iot_led_init();
void led_task_wdt(bool enable);


void iot_led_set_2( uint8_t index, led_color_t col );

void iot_led_set( uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/*
	Priority: 
		-1	reset previous priority setting
		0 	default/low priority. 
		>0  set custom priority

	Trying to alter leds (that have current high priority setting) with lower priority setting will fail. 
*/ 
void iot_led_set_priority( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int priority);

void iot_led_blink( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int on_time, int priority);

// pulse_count = -1 (repeat infinitely)
void iot_led_pulse( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int on_time, int off_time, int pulse_count, int priority);

// Burst equals ( Pulses of pulse_count ) * burst_count, with burst_interval milliseconds between them
// Example: x__x__x _____ x__x__x _______x__x__x__ ...
// burst_count = -1 (repeat infinitely)
void iot_led_burst( uint8_t index, uint8_t r, uint8_t g, uint8_t b, int on_time, int off_time, int pulse_count, int burst_count, int burst_interval, int priority);

extern void node_handle_led_set( uint8_t index, uint8_t r, uint8_t g, uint8_t b);

#endif