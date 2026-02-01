#define TRANSMIT_PIN 3
#define TRANSMIT_EN_PIN 4
#define BTN1PIN 0
#define BTN2PIN 1
#define BTN3PIN 2

#include <RCSwitch.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif
RCSwitch transmitter = RCSwitch();
void setup() {
  transmitter.enableTransmit(TRANSMIT_PIN);
  cli();                            // Disable interrupts during setup
  GIMSK = 0b00100000;
  PCMSK = 0b00000111;
  
  pinMode(BTN1PIN, INPUT_PULLUP);   // Set our interrupt pin as input with a pullup to keep it stable
  pinMode(BTN2PIN, INPUT_PULLUP);   // Set our interrupt pin as input with a pullup to keep it stable
  pinMode(BTN3PIN, INPUT_PULLUP);   // Set our interrupt pin as input with a pullup to keep it stable
}

unsigned long last_transmit = 0;

void transmit(int btn) {
  auto now = millis();
  if (now > last_transmit && now - last_transmit < 100) {
    return;
  }
  last_transmit = now;
  pinMode(TRANSMIT_EN_PIN, OUTPUT);
  digitalWrite(TRANSMIT_EN_PIN, HIGH);
  delay(20);
  transmitter.send(777*(btn + 1), 24);
  digitalWrite(TRANSMIT_EN_PIN, LOW);
  pinMode(TRANSMIT_EN_PIN, INPUT);
}

ISR(PCINT0_vect)
{
}

void sleep() {
  cbi(ADCSRA,ADEN);
  sleep_enable();// Enable sleep mode  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_bod_disable();
  sei();
  sleep_cpu();// Enter sleep mode
  sleep_disable();
}

void loop() {
  sleep();
  if (!digitalRead(BTN1PIN)) {
    transmit(0);
  } else if (!digitalRead(BTN2PIN)) {
    transmit(1);
  } else if (!digitalRead(BTN3PIN)) {
    transmit(2);
  }
}
