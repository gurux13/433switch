#include "../rc-switch/RCSwitch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extern "C" {
	#include "MQTTClient.h"
}

#define ADDRESS     "tcp://localhost:1883"
#define CLIENTID    "Buttoner"
#define TOPIC       "homeassistant/device_automation/livingroom_switch/action_b%d/action"
#define PAYLOAD    "PRESS"
#define QOS         1
#define TIMEOUT     10000L


void send_update(int btn) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
    }
    
    char topic[] = TOPIC;
    snprintf(topic, sizeof(topic), TOPIC, btn + 1);
    char p1 [] = PAYLOAD;
    pubmsg.payload = p1;
    pubmsg.payloadlen = (int)strlen(p1);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

RCSwitch mySwitch;

void print_now() {
  char buffer [80];
  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (buffer, 80, "Now it's %F %R.", timeinfo);
  puts(buffer);
}	

int main(int argc, char *argv[]) {

	// This pin is not the first pin on the RPi GPIO header!
	// Consult https://projects.drogon.net/raspberry-pi/wiringpi/pins/
	// for more information.
	int PIN = 2;

	if(wiringPiSetup() == -1) {
		printf("wiringPiSetup failed, exiting...");
		return 0;
	}

	mySwitch.enableReceive(PIN);  // Receiver on interrupt 0 => that is pin #2
	mySwitch.setReceiveTolerance(80);
	time_t last_btn = 0;
    #if defined(WITH_LOCKS)
    mySwitch.enableLocks();
    #endif
	while(1) {
		if (mySwitch.available()) {
			
			int value = mySwitch.getReceivedValue();
			print_now();
			printf("Found %d\n", value);
			if ((value % 777) == 0 && time(NULL) - last_btn >= 2) {
				int btn = value / 777 - 1;
				printf("Detected button %d\n", btn);
				send_update(btn);
				last_btn = time(NULL);
			}
			mySwitch.resetAvailable();
		}
        #if defined(WITH_LOCKS)
        mySwitch.wait();
        #endif

		usleep(500); 
	}

	exit(0);
}

