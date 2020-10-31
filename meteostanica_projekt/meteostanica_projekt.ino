

#include <SPI.h>                     //kniznica SPI
#include <Ethernet.h>                //kniznica k ethernet shieldu

#include <DallasTemperature.h>
#include <OneWire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
BH1750 lightMeter;  
int nadmorska_vyska=225;

byte mac[] = { 0x20, 0x1A, 0x06, 0x75, 0x8C, 0xAA };  
#define BME280_ADRESA (0x76)
#define ANEMOMETER_PIN 3
#define ANEMOMETER_INT 1
#define VANE_PWR 4
#define VANE_PIN A0
#define RAIN_GAUGE_PIN 2
#define RAIN_GAUGE_INT 0
Adafruit_BME280 bme;
int ledPin = 8; //vybere pin pro připojené LED diody

const int pinCidlaDS = 5;
// vytvoření instance oneWireDS z knihovny OneWire
OneWire oneWireDS(pinCidlaDS);
// vytvoření instance senzoryDS z knihovny DallasTemperature
DallasTemperature senzoryDS(&oneWireDS);

    
char server[] = "iteplota.eu";   //webserver
IPAddress dnServer(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip(192, 168, 1, 45);                    
EthernetClient client;

void setup() {

  setupWeatherInts();
  senzoryDS.begin();
  pinMode(ledPin, OUTPUT);  //nastaví ledPin jako výstupní
  if (!bme.begin(BME280_ADRESA)) {
    Serial.println("BME280 senzor nenalezen, zkontrolujte zapojeni!");
  }
  lightMeter.begin();
  Serial.begin(9600);
  if (Ethernet.begin(mac) == 0) {                  //V PRIPADE ZLYHANIA NASTAVENIA DHCP
    Serial.println("Chyba konfiguracie, manualne nastavenie");
    Ethernet.begin(mac, ip, dnServer, gateway, subnet);
  }
}

void loop() {

    double rychlostvetra = getGust();
    double smervetra = getWindVane();
    double zrazky = getUnitRain();
  
    senzoryDS.requestTemperatures();
    float tlak_s = (bme.readPressure()/100.00);
    float humd = bme.readHumidity();
    float tlak_jm = tlak_s * 9.80655 * nadmorska_vyska;
    float nadvys400 = nadmorska_vyska/400;
    float tlak_ct2 = 273 + bme.readTemperature() + nadvys400; // počítat s venkovní teplotou, pokud je barometr venku, lze použít teplota z něj
    float tlak_ct = 287 * tlak_ct2;
    float pressure = tlak_jm / tlak_ct + tlak_s;
    uint16_t lux = lightMeter.readLightLevel();

    // Calculate altitude assuming 'standard' barometric
    // pressure of 1013.25 millibar = 101325 Pascal
  
   if (Ethernet.begin(mac) == 0) {
    Serial.println("Chyba konfiguracie, manualne nastavenie");
    Ethernet.begin(mac, ip, dnServer, gateway, subnet);
  }
  else
  {
  delay(1000);
  if (client.connect(server, 80)) {
    // AK SA NAPOJI NA SERVER NA PORTE 80 (HTTP)
    // teplota
    client.print("GET /meteostanica_services.php?teplota=");        
    client.print(senzoryDS.getTempCByIndex(0)); 
    // smer vetra 
    client.print("&smer_vetra=");
    client.print(smervetra);
    // rychlost vetra
    client.print("&rychlost_vetra=");
    client.print(rychlostvetra);
    // zrazky
    client.print("&zrazky=");
    client.print(zrazky);
    // vlhkost
    client.print("&vlhkost=");
    client.print(humd);
    // tlak
    client.print("&tlak=");
    client.print(pressure);
    // osvetlenie LUX
    client.print("&lux=");
    client.print(lux);
    client.println(" HTTP/1.1");                 // UKONCENIE REQUESTU ZALOMENIM RIADKA A DOPLNENIM HLAVICKY HTTP S VERZIOU
    client.println("Host: www.iteplota.eu"); // ADRESA HOSTA, NA KTOREHO BOL MIERENY REQUEST (NIE PHP SUBOR)
    client.println("Connection: close");         //UKONCENIE PRIPOJENIA ZA HTTP HLAVICKOU
    client.println();                            //ZALOMENIE RIADKA KLIENTSKEHO ZAPISU
    client.stop();
    Serial.println("Data boli odoslané na WEB iteplota.eu");
    String theMessage = "A|"+String(rychlostvetra)+"|"+String(smervetra)+"|"+ String(zrazky)+"|"+String(senzoryDS.getTempCByIndex(0))+"|"+ String(humd)+"|"+ String(lux)+"|"+ String(pressure)+"|";
    Serial.println(theMessage);
    digitalWrite(8, HIGH);   // rozsvítí LED  diodu, HIGH nastavuje výstupní napětí
    delay(2000);             // nastaví dobu svícení na 1000ms(1s)
    digitalWrite(8, LOW);    // vypne LED diodu, LOW nastaví výstupní napětí na 0V
    delay(160000);

    }else{
    Serial.println("Neuspesne pripojenie pre odoslanie teploty a hodnoty dazdoveho senzoru");
    }    
    
  }
  delay(2000);
    
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

#define RAIN_FACTOR 0.35
 
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



    
