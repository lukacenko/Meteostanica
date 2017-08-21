#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
#include <Wire.h>
#include <BH1750.h>
BH1750 lightMeter;  
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;
#include "SparkFunHTU21D.h"
HTU21D myHumidity;
/*
This sketch sends a string to a corresponding Arduino
with nrf24 attached.  It appends a specific value 
(2 in this case) to the end to signify the end of the
message.
*/

// Set up the inputs and the interrupts
#define ANEMOMETER_PIN 3
#define ANEMOMETER_INT 1
#define VANE_PWR 4
#define VANE_PIN A0
#define RAIN_GAUGE_PIN 2
#define RAIN_GAUGE_INT 0


int msg[1];
RF24 radio(9,10);
const uint64_t pipe = 0xE8E8F0F0E1LL;

const int pinCidlaDS = 5;
// vytvoření instance oneWireDS z knihovny OneWire
OneWire oneWireDS(pinCidlaDS);
// vytvoření instance senzoryDS z knihovny DallasTemperature
DallasTemperature senzoryDS(&oneWireDS);
int ledPin = 8; //vybere pin pro připojené LED diody

void setup(void){

  setupWeatherInts();
  Serial.begin(9600);
  senzoryDS.begin();
  radio.begin();
  radio.openWritingPipe(pipe);
  pinMode(ledPin, OUTPUT);  //nastaví ledPin jako výstupní
  lightMeter.begin();
  if (!bmp.begin()) {
  Serial.println("Could not find a valid BMP085 sensor, check wiring!");
  }
  myHumidity.begin();

  }
void loop(void){


  double rychlostvetra1 = getUnitRain();
  double rychlostvetra2 = getGust();
  double smervetra = getWindVane();
  double zrazky = getUnitRain();
  
  Serial.println(rychlostvetra1);
  Serial.println(rychlostvetra2);
  Serial.println(smervetra);
  Serial.println(zrazky);

  senzoryDS.requestTemperatures();
  Serial.print(senzoryDS.getTempCByIndex(0));

  uint16_t lux = lightMeter.readLightLevel();
  Serial.print("Aktualne intenzita svetla je: ");
  Serial.print(lux);
  Serial.println(" lux");

  Serial.print("Temperature = ");
  Serial.print(bmp.readTemperature());
  Serial.println(" *C");
  
  Serial.print("Pressure = ");
  Serial.print(bmp.readPressure());
  Serial.println(" Pa");
  
  // Calculate altitude assuming 'standard' barometric
  // pressure of 1013.25 millibar = 101325 Pascal
  

  float humd = myHumidity.readHumidity();
  float temp = myHumidity.readTemperature();

  
  
  //String theMessage = String(rychlostvetra1)+"|"+String(rychlostvetra2)+"|"+String(smervetra)+"|"+ String(zrazky)+"|"+ String(senzoryDS.getTempCByIndex(0));
  String theMessage = String(rychlostvetra1)+"|"+String(rychlostvetra2)+"|"+String(smervetra)+"|"+ String(zrazky)+"|"+ String(senzoryDS.getTempCByIndex(0))+"|"+ String(humd)+"|"+ String(lux);
  //String theMessage = String(rychlostvetra1)+"|"+String(rychlostvetra2)+"|"+String(smervetra)+"|45678666";
  //String theMessage = String(rychlostvetra1)+"|"+String(rychlostvetra2)+"|"+String(smervetra)+"|45678";
  //String theMessage = " 260719937020";
  int messageSize = theMessage.length();
  for (int i = 0; i < messageSize; i++) {
    int charToSend[1];
    charToSend[0] = theMessage.charAt(i);
    radio.write(charToSend,1);
  }  
  Serial.println(theMessage);
//send the 'terminate string' value...  
  msg[0] = 2; 
  radio.write(msg,1);
  digitalWrite(8, HIGH);   // rozsvítí LED diodu, HIGH nastavuje výstupní napětí
  delay(2000);              // nastaví dobu svícení na 1000ms(1s)
  digitalWrite(8, LOW);    // vypne LED diodu, LOW nastaví výstupní napětí na 0V  
  delay(10000);
  radio.write(msg,1);
  
/*delay sending for a short period of time.  radio.powerDown()/radio.powerupp
//with a delay in between have worked well for this purpose(just using delay seems to
//interrupt the transmission start). However, this method could still be improved
as I still get the first character 'cut-off' sometimes. I have a 'checksum' function
on the receiver to verify the message was successfully sent.
*/
  radio.powerDown(); 
//  delay(60000);
  delay(1000);
  radio.powerUp();

  
}



void setupWeatherInts()
{
  pinMode(ANEMOMETER_PIN,INPUT);
  digitalWrite(ANEMOMETER_PIN,HIGH);  // Turn on the internal Pull Up Resistor
  pinMode(RAIN_GAUGE_PIN,INPUT);
  digitalWrite(RAIN_GAUGE_PIN,HIGH);  // Turn on the internal Pull Up Resistor
  pinMode(VANE_PWR,OUTPUT);
  digitalWrite(VANE_PWR,LOW);
  attachInterrupt(ANEMOMETER_INT,anemometerClick,FALLING);
  attachInterrupt(RAIN_GAUGE_INT,rainGageClick,FALLING);
  interrupts();
}

// ---------------------
// Wind speed (anemometer)

#define WIND_FACTOR 2.4
#define TEST_PAUSE 60000
 
volatile unsigned long anem_count=0;
volatile unsigned long anem_last=0;
volatile unsigned long anem_min=0xffffffff;
 
double getUnitWind()
{
  unsigned long reading=anem_count;
  anem_count=0;
  return (WIND_FACTOR*reading)/(TEST_PAUSE/1000);
}
 
double getGust()
{
 
  unsigned long reading=anem_min;
  anem_min=0xffffffff;
  double time=reading/1000000.0;
 
  return (1/(reading/1000000.0))*WIND_FACTOR;
}
 
void anemometerClick()
{
  long thisTime=micros()-anem_last;
  anem_last=micros();
  if(thisTime>500)
  {
    anem_count++;
    if(thisTime<anem_min)
    {
      anem_min=thisTime;
    }
 
  }
}

// ---------------------
// Wind direction (vane)

static const int vaneValues[] PROGMEM={66,84,92,127,184,244,287,406,461,600,631,702,786,827,889,946};
static const int vaneDirections[] PROGMEM={1125,675,900,1575,1350,2025,1800,225,450,2475,2250,3375,0,2925,3150,2700};
 
double getWindVane()
{
  analogReference(DEFAULT);
  digitalWrite(VANE_PWR,HIGH);
  delay(100);
  for(int n=0;n<10;n++)
  {
    analogRead(VANE_PIN);
  }
 
  unsigned int reading=analogRead(VANE_PIN);
  digitalWrite(VANE_PWR,LOW);
  unsigned int lastDiff=2048;
 
  for (int n=0;n<16;n++)
  {
    int diff=reading-pgm_read_word(&vaneValues[n]);
    diff=abs(diff);
    if(diff==0)
       return pgm_read_word(&vaneDirections[n])/10.0;
 
    if(diff>lastDiff)
    {
      return pgm_read_word(&vaneDirections[n-1])/10.0;
    }
 
    lastDiff=diff;
 }
 
  return pgm_read_word(&vaneDirections[15])/10.0;
 
}

// ---------------------
// Rain gauge

#define RAIN_FACTOR 0.65
 
volatile unsigned long rain_count=0;
volatile unsigned long rain_last=0;
 
double getUnitRain()
{
 
  unsigned long reading=rain_count;
  rain_count=0;
  double unit_rain=reading*RAIN_FACTOR;
 
  return unit_rain;
}
 
void rainGageClick()
{
    long thisTime=micros()-rain_last;
    rain_last=micros();
    if(thisTime>500)
    {
      rain_count++;
    }
}


