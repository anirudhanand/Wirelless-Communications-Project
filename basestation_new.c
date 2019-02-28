#include <stdio.h>
#include "contiki.h"
#include "net/rime.h"
#include "dev/leds.h"
#include "dev/cc2420.h"
#include "clicker.h"

#define OffTime CLOCK_SECOND*5

/* Declare our "main" process, the basestation_process */
PROCESS(basestation_process, "Clicker basestation");
PROCESS(timerProcess, "timer basestation");
/* The basestation process should be started automatically when
 * the node has booted. */
AUTOSTART_PROCESSES(&basestation_process, &timerProcess);

/* Holds the number of packets received. */
static struct etimer LedOff;
/* Callback function for received packets.
 *
 * Whenever this node receives a packet for its broadcast handle,
 * this function will be called.
 */
static void recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	
	printf("message recieved\n");
	/* 0bxxxxx allows us to write binary values */
	/* for example, 0b10 is 2 */
	

	leds_on(LEDS_ALL);
	process_poll(&timerProcess);
	printf("package 1\n");
}

/* Broadcast handle to receive and send (identified) broadcast
 * packets. */
static struct broadcast_conn bc;
/* A structure holding a pointer to our callback function. */
static struct broadcast_callbacks bc_callback = { recv };

PROCESS_THREAD(timerProcess, ev, data) {
	PROCESS_BEGIN();
	
	while(1){
	//printf("waiting\n");
	printf("package 2\n");
	PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
	//printf("process yield\n");
	etimer_set(&LedOff, OffTime);
	printf("package 3\n");
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&LedOff));
	leds_off(LEDS_ALL);
	printf("package 4\n");
	}
	PROCESS_END();
}
/* Our main process. */
PROCESS_THREAD(basestation_process, ev, data) {
	PROCESS_BEGIN();
	printf("package 5\n");

	/* Open the broadcast handle, use the rime channel
	 * defined by CLICKER_CHANNEL. */
	broadcast_open(&bc, CLICKER_CHANNEL, &bc_callback);
	/* Set the radio's channel to IEEE802_15_4_CHANNEL */
	cc2420_set_channel(IEEE802_15_4_CHANNEL);
	/* Set the radio's transmission power. */
	cc2420_set_txpower(CC2420_TX_POWER);

	/* That's all we need to do. Whenever a packet is received,
	 * our callback function will be called. */
	printf("package 6\n");
	PROCESS_END();
}

