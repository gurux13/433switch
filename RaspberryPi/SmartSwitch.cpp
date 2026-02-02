#include <iostream>
#include <wiringPi.h>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <map>
#include <string>
#include <cstring>

extern "C"
{
#include "MQTTClient.h"
}

#define ADDRESS "tcp://localhost:1883"
#define CLIENTID "Buttoner"
#define TOPIC "homeassistant/device_automation/livingroom_switch/action_b%d/action"
#define TOPIC_GENERIC "homeassistant/device_automation/433_switch_%s/action_b%d/action"
#define PAYLOAD "PRESS"
#define QOS 1
#define TIMEOUT 10000L

// --- Configuration Constants ---
const int PIN_SIGNAL = 2; // wiringPi Pin 2 (BCM 27)

// Protocol Timing (Microseconds)
const int SYNC_GAP_MIN = 8000;  // Minimum low duration to signify start
const int PERIOD_TARGET = 1200; // Target total cycle length (High + Low)
const int BIT_0_HIGH = 300;     // High duration for a '0'
const int BIT_1_HIGH = 900;     // High duration for a '1'

const int TOLERANCE = 250; // +/- deviation allowed
const int BIT_COUNT = 24;  // Number of bits per frame

// --- Concurrency Control ---
std::mutex dataMutex;
std::condition_variable dataReadyCv;
unsigned int sharedResult = 0;
bool dataAvailable = false;

void isrLogic()
{
    static unsigned long lastTime = 0;
    static unsigned long highDuration = 0;

    // Decoder State
    static bool syncFound = false;
    static int bitIndex = 0;
    static unsigned int bitBuffer = 0;
    static unsigned int lastConfirmedValue = 0;

    unsigned long now = micros();
    unsigned long duration = now - lastTime;
    lastTime = now;

    int state = digitalRead(PIN_SIGNAL);

    if (state == LOW)
    {
        // Falling Edge: The High pulse just finished.
        highDuration = duration;
    }
    else
    {
        // Rising Edge: The Low pulse just finished. Cycle Complete.
        unsigned long lowDuration = duration;

        // 1. Check for Sync (Long Low)
        if (lowDuration >= SYNC_GAP_MIN)
        {
            syncFound = true;
            bitIndex = 0;
            bitBuffer = 0;
            return;
        }

        // 2. Decode Bit if Synced
        if (syncFound)
        {
            unsigned long totalPeriod = highDuration + lowDuration;
            // A. Validate Total Period
            if (totalPeriod < (PERIOD_TARGET - TOLERANCE) ||
                totalPeriod > (PERIOD_TARGET + TOLERANCE))
            {
                syncFound = false;
                return;
            }

            // B. Determine Bit Value
            bool isBit1 = (highDuration >= (BIT_1_HIGH - TOLERANCE) &&
                           highDuration <= (BIT_1_HIGH + TOLERANCE));
            bool isBit0 = (highDuration >= (BIT_0_HIGH - TOLERANCE) &&
                           highDuration <= (BIT_0_HIGH + TOLERANCE));

            if (isBit0)
            {
                bitBuffer = (bitBuffer << 1);
                bitIndex++;
            }
            else if (isBit1)
            {
                bitBuffer = (bitBuffer << 1) | 1;
                bitIndex++;
            }
            else
            {
                // Invalid High Duration
                syncFound = false;
                return;
            }

            // 3. Check for Full Frame
            if (bitIndex == BIT_COUNT)
            {
                // Check double-match consistency
                if (bitBuffer == lastConfirmedValue)
                {
                    // Notify Main Thread
                    std::lock_guard<std::mutex> lock(dataMutex);
                    sharedResult = bitBuffer;
                    dataAvailable = true;
                    dataReadyCv.notify_one();
                }

                lastConfirmedValue = bitBuffer;
                syncFound = false; // Reset to wait for next sync
            }
        }
    }
}

const std::map<int, std::string> ALLOWED_PREFIXES{
    {0xC31E, "br_blinds"},
    {0xE1E7, "bathroom"}};

void send_update(int btn, std::string sw = "")
{
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

    char topic[256];
    if (sw == "")
    {
        snprintf(topic, 256, TOPIC, btn + 1);
    }
    else
    {
        snprintf(topic, 256, TOPIC_GENERIC, sw.c_str(), btn);
    }
    char p1[] = PAYLOAD;
    pubmsg.payload = p1;
    pubmsg.payloadlen = (int)strlen(p1);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

void print_now()
{
    char buffer[80];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 80, "Now it's %F %R.", timeinfo);
    puts(buffer);
}
time_t last_btn = 0;

void on_data(unsigned int value)
{
    print_now();
    printf("Found %d\n", value);
    if (time(NULL) - last_btn >= 1)
    {
        if (value % 777 == 0)
        {
            int btn = value / 777 - 1;
            printf("Detected button %d\n", btn);
            send_update(btn);
            last_btn = time(NULL);
        }
        else
        {
            int prefix = value >> 8;
            printf("Prefix: %x\n", prefix);
            auto prefix_elem = ALLOWED_PREFIXES.find(prefix);
            if (prefix_elem != ALLOWED_PREFIXES.end())
            {
                int btn = value & 255;
                int mask = 1;
                while (btn > 0 && mask <= btn)
                {
                    if ((btn & mask) == 0)
                    {
                        mask <<= 1;
                        continue;
                    }
                    btn ^= mask;
                    std::string sw = prefix_elem->second;
                    printf("Detected button %d on switch %s\n", mask, sw.c_str());
                    send_update(mask, sw);
                    last_btn = time(NULL);
                }
            }
        }
    }
}

int main()
{
    // 1. Setup
    if (wiringPiSetup() < 0)
    {
        std::cerr << "WiringPi Setup Failed" << std::endl;
        return 1;
    }

    pinMode(PIN_SIGNAL, INPUT);

    if (wiringPiISR(PIN_SIGNAL, INT_EDGE_BOTH, &isrLogic) < 0)
    {
        std::cerr << "ISR Setup Failed" << std::endl;
        return 1;
    }

    std::cout << "Decoder Started on Pin " << PIN_SIGNAL << std::endl;

    // 2. Event Loop
    while (true)
    {
        unsigned int lastResult = 0;
        {
            std::unique_lock<std::mutex> lock(dataMutex);

            // Wait efficiently until the ISR signals data is ready
            dataReadyCv.wait(lock, []
                             { return dataAvailable; });
            lastResult = sharedResult;
            dataAvailable = false;
        }

        // Process Data
        std::cout << "Valid Signal: 0x"
                  << std::hex << std::uppercase << std::setw(6) << std::setfill('0')
                  << lastResult
                  << std::dec << "\n";
        on_data(lastResult);
    }

    return 0;
}
