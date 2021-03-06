#include <stdio.h>
#include "contiki.h"
#include "net/rime.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "dev/cc2420.h"
#include "dev/adxl345.h"
#include "clicker.h"


#define LED_INT_ONTIME        CLOCK_SECOND/2
#define ACCM_READ_INTERVAL    CLOCK_SECOND


/* The client process should be started automatically when
 * the node has booted. */


/* Callback function for received packets.
 *
 * Whenever this node receives a packet for its broadcast handle,
 * this function will be called.
 *
 * As the client does not need to receive, the function does not do anything
 */

static process_event_t ledOff_event;
/*---------------------------------------------------------------------------*/
PROCESS(accel_process, "Test Accel process");
PROCESS(led_process, "LED handling process");
AUTOSTART_PROCESSES(&accel_process,&led_process);
/*---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------*/
/* accelerometer free fall detection callback */
void
accm_ff_cb(uint8_t reg){
  L_ON(LEDS_B);
  process_post(&led_process, ledOff_event, NULL);
  printf("~~[%u] Freefall detected! (0x%02X) -- ", ((uint16_t) clock_time())/128, reg);
  print_int(reg);
}
/*---------------------------------------------------------------------------*/
/* accelerometer tap and double tap detection callback */

void
accm_tap_cb(uint8_t reg){
  process_post(&led_process, ledOff_event, NULL);
  		//printf("package sent\n");

  if(reg & ADXL345_INT_DOUBLETAP){
    L_ON(LEDS_G);
    printf("~~[%u] DoubleTap detected! (0x%02X) -- ", ((uint16_t) clock_time())/128, reg);
  } else {
    L_ON(LEDS_R);
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
    L_OFF(LEDS_R + LEDS_G + LEDS_B);
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

PROCESS_THREAD(accel_process, ev, data) {
  PROCESS_BEGIN();
  {
    int16_t x,new_x=0,y, new_y=0;
    static int old_x=0, old_y=0;
  
    broadcast_open(&bc, CLICKER_CHANNEL, &bc_callback);
    cc2420_set_channel(IEEE802_15_4_CHANNEL);
    cc2420_set_txpower(CC2420_TX_POWER);
    //serial_shell_init();
    //shell_ps_init();
    //shell_file_init();  // for printing out files
    //shell_text_init();  // for binprint

    /* Register the event used for lighting up an LED when interrupt strikes. */
    ledOff_event = process_alloc_event();

    /* Start and setup the accelerometer with default values, eg no interrupts enabled. */
    accm_init();

    /* Register the callback functions for each interrupt */
    ACCM_REGISTER_INT1_CB(accm_ff_cb);
    ACCM_REGISTER_INT2_CB(accm_tap_cb);

    /* Set what strikes the corresponding interrupts. Several interrupts per pin is 
      possible. For the eight possible interrupts, see adxl345.h and adxl345 datasheet. */
    accm_set_irq(ADXL345_INT_FREEFALL, ADXL345_INT_TAP + ADXL345_INT_DOUBLETAP);

    while (1) {
	    x = accm_read_axis(X_AXIS);
	    new_x=x;
	    y = accm_read_axis(Y_AXIS);
	    new_y=y;
	    //printf("x: %d\n", old_x);
	
	    if ((new_x>=old_x+10 || new_y>=old_y+10) || (new_x<=old_x-10 || new_y<=old_y-10 )){
		printf("x: %d\n", new_x);
		leds_toggle(LEDS_RED);
		packetbuf_copyfrom("hej", 4);
		broadcast_send(&bc);
	        }
	    old_x=new_x;
	    old_y=new_y;
	    
            
	    
	    //z = accm_read_axis(Z_AXIS);
	    

      etimer_set(&et, ACCM_READ_INTERVAL);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
}
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/


