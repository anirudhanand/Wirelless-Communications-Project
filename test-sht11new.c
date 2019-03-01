#include <stdio.h>
#include "contiki.h"
#include "net/rime.h"
#include "dev/button-sensor.h"
#include "dev/tmp102.h"
#include "dev/leds.h"
#include "dev/cc2420.h"
#include "dev/adxl345.h"
#include "clicker.h"
#include "clock.h"
#include "clock.c"
#include "sys/etimer.h"
#include "sys/stimer.h"
#include "sys/timer.h"
#include "sys/rtimer.h"

#define LED_INT_ONTIME        CLOCK_SECOND/2
#define ACCM_READ_INTERVAL    CLOCK_SECOND
#define TMP102_READ_INTERVAL  CLOCK_SECOND/2
#define OFF_TIME CLOCK_SECOND

static process_event_t ledOff_event;

PROCESS(temp_process, "Test Temperature process");
PROCESS(led_process, "LED handling process");
PROCESS(accel_process, "Test Accel process");

AUTOSTART_PROCESSES(&temp_process, &led_process  , &accel_process);
/*---------------------------------------------------------------------------*/
static struct etimer et;

/* As several interrupts can be mapped to one interrupt pin, when interrupt 
    strikes, the adxl345 interrupt source register is read. This function prints
    out which interrupts occurred. Note that this will include all interrupts,
    even those mapped to 'the other' pin, and those that will always signal even if
    not enabled (such as watermark). */
void
print_int(uint16_t reg){
#define ANNOYING_ALWAYS_THERE_ANYWAY_OUTPUT 0
#if ANNOYING_ALWAYS_THERE_ANYWAY_OUTPUT
  if(reg & ADXL345_INT_OVERRUN) {
    printf("Overrun ");
  }
  if(reg & ADXL345_INT_WATERMARK) {
    printf("Watermark ");
  }
  if(reg & ADXL345_INT_DATAREADY) {
    printf("DataReady ");
  }
#endif
  if(reg & ADXL345_INT_FREEFALL) {
    printf("Freefall ");
  }
  if(reg & ADXL345_INT_INACTIVITY) {
    printf("InActivity ");
  }
  if(reg & ADXL345_INT_ACTIVITY) {
    printf("Activity ");
  }
  if(reg & ADXL345_INT_DOUBLETAP) {
    printf("DoubleTap ");
  }
  if(reg & ADXL345_INT_TAP) {
    printf("Tap ");
  }
  printf("\n");
}

/* accelerometer free fall detection callback */
void
accm_ff_cb(uint8_t reg){
  L_ON(LEDS_B);
  process_post(&led_process, ledOff_event, NULL);
  printf("~~[%u] Freefall detected! (0x%02X) -- ", ((uint16_t) clock_time())/128, reg);
  print_int(reg);
}

void
accm_tap_cb(uint8_t reg){
  process_post(&led_process, ledOff_event, NULL);
      //printf("package sent\n");

  if(reg & ADXL345_INT_DOUBLETAP){
    //L_ON(LEDS_G);
    printf("~~[%u] DoubleTap detected! (0x%02X) -- ", ((uint16_t) clock_time())/128, reg);
  } else {
    //L_ON(LEDS_R);
    printf("~~[%u] Tap detected! (0x%02X) -- ", ((uint16_t) clock_time())/128, reg);
  }
  print_int(reg);
}

/*---------------------------------------------------------------------------*/
/* When posted an ledOff event, the LEDs will switch off after LED_INT_ONTIME.
      static process_event_t ledOff_event;
      ledOff_event = process_alloc_event();
      process_post(&led_process, ledOff_event, NULL);
*/

static struct etimer ledETimer;
PROCESS_THREAD(led_process, ev, data) {
  PROCESS_BEGIN();
  while(1){
    PROCESS_WAIT_EVENT_UNTIL(ev == ledOff_event);
    etimer_set(&ledETimer, LED_INT_ONTIME);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&ledETimer));
    //L_OFF(LEDS_R + LEDS_G + LEDS_B);
  }
  PROCESS_END();
}

static void recv(struct broadcast_conn *c, const rimeaddr_t *from) {
}

/* Broadcast handle to receive and send (identified) broadcast
 * packets. */
static struct broadcast_conn bc;
/* A structure holding a pointer to our callback function. */
static struct broadcast_callbacks bc_callback = { recv };

static struct etimer et;
PROCESS_THREAD(accel_process, ev, data)
{
  static struct etimer timer_etimer;

  PROCESS_BEGIN();
  {
    int16_t x,new_x=0,y, new_y=0;
    static int old_x=0, old_y=0;
        leds_on(LEDS_BLUE);

    broadcast_open(&bc, CLICKER_CHANNEL, &bc_callback);
    cc2420_set_channel(IEEE802_15_4_CHANNEL);
    cc2420_set_txpower(CC2420_TX_POWER);
 

    // Register the event used for lighting up an LED when interrupt strikes. 
    ledOff_event = process_alloc_event();

    // Start and setup the accelerometer with default values, eg no interrupts enabled. 
    accm_init();

    // Register the callback functions for each interrupt 
    ACCM_REGISTER_INT1_CB(accm_ff_cb);
    ACCM_REGISTER_INT2_CB(accm_tap_cb);

    // Set what strikes the corresponding interrupts. Several interrupts per pin is 
      //possible. For the eight possible interrupts, see adxl345.h and adxl345 datasheet. 
    accm_set_irq(ADXL345_INT_FREEFALL, ADXL345_INT_TAP + ADXL345_INT_DOUBLETAP);

   
    while (1) 
    {
       // printf("Inside accel loop1\n");

      leds_toggle(LEDS_BLUE);
      x = accm_read_axis(X_AXIS);
      new_x=x;
      y = accm_read_axis(Y_AXIS);
      new_y=y;
      //printf("x: %d\n", old_x);
  
      if ((new_x>=old_x+50 || new_y>=old_y+50) || (new_x<=old_x-50 || new_y<=old_y-50 ))
      {
        printf("x: %d\n", new_x);
  
        packetbuf_copyfrom("hej", 4);
        broadcast_send(&bc);
          
    //printf("Inside accel loop yield\n");

      }
      old_x=new_x;
      old_y=new_y;

            
      
      //z = accm_read_axis(Z_AXIS);
      
      process_poll(&temp_process);
      PROCESS_YIELD();
      etimer_set(&et, ACCM_READ_INTERVAL);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
              //printf("Inside accel loop exit\n");

      }
  }
  PROCESS_END();
}

//static struct  etimer led_off;
  
  static struct  etimer timer_temp;


PROCESS_THREAD(temp_process, ev, data)
{
  PROCESS_BEGIN();
  

  //printf("inside temp loop1\n");
  int16_t tempint;
  //uint16_t tempfrac;
  int16_t raw;
  uint16_t absraw;
  int16_t sign;
  //char minus = ' ';

  broadcast_open(&bc, CLICKER_CHANNEL, &bc_callback);
  cc2420_set_channel(IEEE802_15_4_CHANNEL);
  cc2420_set_txpower(CC2420_TX_POWER);
  //printf("inside temp loop2\n");

 //Register the event used for lighting up an LED when interrupt strikes. 
  ledOff_event = process_alloc_event();


  tmp102_init();

  //printf("inside temp loop3\n");

  while(1) {
  //printf("inside temp loop while\n");

  //PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
  etimer_set(&timer_temp, OFF_TIME);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer_temp));
      //clock_delay(300);
   //printf("inside temp loop\n");

   leds_toggle(LEDS_RED);
    sign = 1;

    raw = tmp102_read_temp_raw();
    absraw = raw;
     //printf("inside temp loop4\n");

    tempint = (absraw >> 8) * sign;
    //tempfrac = ((absraw >> 4) % 16) * 625;  // Info in 1/10000 of degree
    //minus = ((tempint == 0) & (sign == -1)) ? '-' : ' ';
    //printf("Temp = %c%d.%04d\n", minus, tempint, tempfrac);
    //printf("Temp : %d\n", tempint);

  //PROCESS_PAUSE();

    if (tempint>28)
    {
      
    
      //leds_on(LEDS_RED);
     // leds_off(LEDS_GREEN);
      printf("Above 25\n");
      packetbuf_copyfrom("hej", 4);
      broadcast_send(&bc);
    }
    else
    {
      printf("Below 25\n");
      //leds_on(LEDS_GREEN);
      //leds_off(LEDS_RED);
       //packetbuf_copyfrom("hej", 4);
      //broadcast_send(&bc);
    
    }
      //printf("inside temp loop5\n");

    process_poll(&accel_process);
    PROCESS_YIELD();
    etimer_set(&et, TMP102_READ_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      //printf("inside temp loop exit\n");

  }

  PROCESS_END();
}

