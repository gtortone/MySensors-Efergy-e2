/*
 * The Efergy e2 electricity monitor (http://efergy.com/it/products/electricity-monitors/e2-classic) 
 * provides a sensor that wirelessly sends information about the amount of electricity 
 * you are using to the display monitor. The monitor converts this into kilowatt-hours. 
 * 
 * This sketch use a JeeNode v5 (http://jeelabs.net/projects/hardware/wiki/JeeNode) equipped
 * with 433/868 MHz RFM12B module (www.hoperf.com/upload/rf/RFM12B.pdf) configured in OOK mode
 * to capture wireless data (on pin D3) and forward it through NRF24L01+ with MySensors library
 * 
 * Created by Gennaro Tortone  <gtortone@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

// Enable debug prints to serial monitor
#define MY_DEBUG
//#define VERBOSE

#define MY_PARENT_NODE_ID 0
#define MY_PARENT_NODE_IS_STATIC

// Enable and select radio type attached
#define MY_RADIO_NRF24
#define MY_RF24_CS_PIN  8
#define MY_RF24_CE_PIN  9
#define MY_RF24_PA_LEVEL RF24_PA_MAX

#include <SPI.h>
#include <MySensors.h>  
#include <JeeLib.h>

#define DATA  3
#define CHILD_ID_POWER 9

unsigned long SLEEP_TIME = 2000; // Sleep time between reads (in milliseconds)
MyMessage msg(CHILD_ID_POWER, V_WATT);

enum { IDLE, START, ACQUIRE };

volatile byte state = IDLE;
volatile byte prevstate = IDLE;

byte payload[8];
byte nbytes = 0;
volatile bool lock = false;

bool decode_payload(byte *pl);

float current;        // Ampere
byte voltage = 220;   // Volt
float power;          // Watt
uint16_t no_err, short_err, long_err, crc_err;

void setup()
{
  Serial.begin(115200);
  Serial.println("[Efergy e2]");
  
  pinMode(DATA, INPUT);
  attachInterrupt(digitalPinToInterrupt(DATA), efergy_e2_rx, CHANGE);

  rf12_initialize(0, RF12_433MHZ);
  rf12_control(0x8097);       // disable FIFO
  rf12_control(0xA586);       // frequency to 433.535 MHz
  rf12_control(0xC622);       // data rate: 10142 bps
  rf12_control(0xCC76);       // max bw 86.2 kbps
  rf12_control(0x94C4);       // bandwidth changed to 67 kHz, LNA gain 0 dB
  rf12_control(0x82DD);       // RECEIVER_ON

  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Efergy e2 power meter", "1.0");

  // Register all sensors to gateway (they will be created as child devices)
  present(CHILD_ID_POWER, S_POWER);

  no_err = short_err = long_err = crc_err = 0;
}

void loop()
{

  while(!lock) ;
  
#ifdef VERBOSE   
  Serial.println("-- S --");

  for(int i=0; i<nbytes; i++) {

    Serial.println(payload[i]);
  }

  Serial.println("-- E --");
#endif

  if(decode_payload()) {

#ifdef VERBOSE
    Serial.print("current = ");
    Serial.print(current);
    Serial.print("A - power = ");
    Serial.print(power);
    Serial.println(" W"); 
#endif

    send(msg.set((int16_t)(power)));
  }

#ifdef VERBOSE
  Serial.print("OK = ");
  Serial.print(no_err);
  Serial.print(" - short pk = ");
  Serial.print(short_err);
  Serial.print(" - long pk = ");
  Serial.print(long_err);
  Serial.print(" - crc bad = ");
  Serial.println(crc_err);
#endif
  
  lock = false;
  
}

bool decode_payload(void) {

  uint16_t adc_value;

  if(nbytes < 8) {

    short_err++;
    return 0;
  } 
  
  if(!crc_payload()) {

    crc_err++;
    return 0;
  
  } 

  // here crc is ok
  
  if(nbytes > 8) {

    // track if long packet
    long_err++;

  }
    
  adc_value = (payload[4] << 8) + payload[5];
  current = ((float)(adc_value) / 32768) * (float)(pow(2, payload[6]));
  power = (float) (current * voltage);
    
  no_err++; 
  
  return 1;
}

bool crc_payload(void) {

  uint8_t val = 0;

  for(int i=0; i<=6; i++)
    val += payload[i];
    
  if(val == payload[7])
    return 1;
  else
    return 0;
}

/*
 * Efergy e2 power monitor http://efergy.com/it/products/electricity-monitors/e2-classic
 * transmission protocol features: 
 * 
 * frequency: 433.535 MHz
 * bandwith: 67 kHz
 * data rate: 10142 bps
 * 
 * preamble: ~1800 us LOW - ~500 us HIGH
 * 
 * payload: 
 * - '0' => 140 us LOW - 60 us HIGH
 * - '1' => 60 us LOW - 140 HIGH
 * 
 * payload format:
 * 
 * 8 bytes 
 * ------- 
 * bytes #1:  0x09    fixed value
 * bytes #2:  0XF0    fixed value
 * bytes #3:  0x2F    fixed value
 * bytes #4:  0x40    fixed value
 * bytes #5:  current MSB
 * bytes #6:  current LSB
 * bytes #7:  exponent 
 * bytes #8:  CRC
 * 
 * current detected = ((MSB << 8) + LSB / 32768) * pow(2, exponent)
 * 
 * example:
 * 
 * 0x09 0xF0 0x2F 0x40 0x6B 0x85 0x01 0x59
 * 
 * current = (0x6B85 / 32768) * (2^1) = 27525/32768 * 2 = 0.839 * 2 = 1.67 A
 * 
 */

void efergy_e2_rx(void) {
  
  static uint32_t lasttime = 0;
  static uint32_t duration = 0;
  static byte pos = 0;
  static byte samples = 0;
  static uint16_t nbits = 0;
  static byte value = 0;
  
  uint32_t t = micros();

  if(lock) return;
  
  duration = t - lasttime;

  if( (state == START) ) {
    
    if( (duration >= 400) && (duration <= 700) ) {      
      
      prevstate = state;    // START -> ACQUIRE
      state = ACQUIRE;

      pos = samples = nbits = value = 0;
    
    } else {

      prevstate = state;    // START -> IDLE
      state = IDLE;
    }
    
  } else if(state == ACQUIRE) {

    if( (duration <= 200) && (duration >= 10) ) {

      samples++;

      if(samples == 2)    {   
  
        if( duration <= 100 ) {   // bit = 0

          nbits++;
          value = value << 1;

          if(nbits % 8 == 0) {

            payload[pos++] = value;
            value = 0;
          }
       
        } else {     // bit = 1
  
          nbits++;
          value = (value << 1) + 1;
          
          if(nbits % 8 == 0) {

            payload[pos++] = value;
            value = 0;
          }
        }

        samples = 0;
      }
    
    } else {

      prevstate = state;    // ACQUIRE -> IDLE
      state = IDLE;

      nbytes = pos;
      
      lock = true;
    }

  } else if(state == IDLE) {

    if(duration >= 1800) {
    
      prevstate = state;    // IDLE -> START
      state = START;
    }
  } 

  lasttime = t;
}

