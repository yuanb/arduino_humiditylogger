#include <SD.h>
#include <SPI.h>
#include <Time.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <OneWire.h>

#include "EEPROMAnything.h"
#include "freeram.h"
#include "watchDog.h"
#include "ntpTask.h"
#include "serverTask.h"

/*** DATA LOGGER AND TIMER CONTROLS ****/
#define HIH4030_Pin  A0
#define MEASURE_INTERVAL 600000     //10 minute intervals between measurements (in ms)
#define FILE_INTERVAL 604800        //One week worth of seconds

//A structure that stores file config variables from EEPROM
typedef struct{
	unsigned long newFileTime;      //Keeps track of when a newfile should be made.
	char workingFilename[19];       //The path and filename of the current week's file
} configuration;
configuration config;               //Actually make our config struct

OneWire  ONE_WIRE_BUS(2);  // Bus One-Wire (sonda DS18S20) sul pin 10

// Indirizzo della sonda DS18S20
// Nota: trova l'indirizzo della tua sonda col programma precedente e incollalo al posto di questo
byte T_SENSOR[8] = {0x10, 0x8F, 0x43, 0x37, 0x00, 0x08, 0x00, 0x2A};

void setup() {    
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  pinMode(10, OUTPUT);          // set the SS pin as an output (necessary!)
  digitalWrite(10, HIGH);       // but turn off the W5100 chip! 

  // see if the card is present and can be initialized:
  if (!SD.begin(4)) {
    Serial.println(F("SD failed, or not present"));
    // don't do anything more:
    return;
  }

  serverInit();
  ntpInit();
  
  EEPROM_readAnything(0,config); // make sure our config struct is syncd with EEPROM

  watchdogSetup();
  Serial.println(F("setup done."));
}

void loop()
{
  updateTimeTask();
  sampleSensorTask();
  serveClientTask();
  idleTask();
}

void generateNewFilename(unsigned long rawTime)
{
  char newFilename[18] = "";
  int dayInt = day(rawTime);
  int monthInt = month(rawTime);
  int yearInt = year(rawTime);
  
  char dayStr[3];
  char monthStr[3];
  char yearStr[5];
  char subYear[3];
  
  strcat(newFilename,"data/");
  itoa(dayInt, dayStr, 10);
  if (dayInt < 10){
    strcat(newFilename,"0");
  }
  
  strcat(newFilename,dayStr);
  strcat(newFilename,"-");
  itoa(monthInt,monthStr,10);
  if (monthInt < 10){
    strcat(newFilename,"0");
  }
  
  strcat(newFilename,monthStr);
  strcat(newFilename,"-");
  itoa(yearInt,yearStr,10);
  //we only want the last two digits of the year
  memcpy( subYear, &yearStr[2], 3 );
  strcat(newFilename,subYear);
  
  strcat(newFilename,".csv");
      
  //make sure we update our config variables:
  config.newFileTime += FILE_INTERVAL;      
  strcpy(config.workingFilename,newFilename);
  Serial.println(config.workingFilename);
  
  //Write the changes to EEPROM. Bad things may happen if power is lost midway through,
  //but it's a small risk we take. Manual fix with EEPROM_config sketch can correct it.
  EEPROM_writeAnything(0, config);  
}

// From Making accurate ADC readings on the Arduino
// http://hacking.majenko.co.uk/making-accurate-adc-readings-on-arduino
long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1125300L / result; // Back-calculate AVcc in mV
  return result;
}

float getHumidity(float degreesCelsius){
  //caculate relative humidity
  float supplyVolt = readVcc()/1000.0;
  
  //Serial.print(F("Vcc= "));
  //Serial.println(supplyVolt);

  // read the value from the sensor:
  int HIH4030_Value = analogRead(HIH4030_Pin);
  float voltage = HIH4030_Value/1023. * supplyVolt; // convert to voltage value

  // convert the voltage to a relative humidity
  // - the equation is derived from the HIH-4030/31 datasheet
  // - it is not calibrated to your individual sensor
  //  Table 2 of the sheet shows the may deviate from this line
  float sensorRH = 161.0 * voltage / supplyVolt - 25.8;
  float trueRH = sensorRH / (1.0546 - 0.0026 * degreesCelsius); //temperature adjustment 

  return trueRH;
}

void sampleSensorTask()
{
  static unsigned long lastIntervalTime = 0; //The time the last measurement occured.
  
  if ((millis() % lastIntervalTime) >= MEASURE_INTERVAL)    //Is it time for a new measurement?
  {
    Serial.print(F("freeRam @ sampleSensor()="));
    Serial.println(freeRam());  

    static int count = 0;
 
    if (ntpTime == 0 || ntpTime == 39) {
      //server seems to send 39 as an error code wait for next time slot if this happens
      //NIST considers retry interval of <4s as DoS attack, so fair warning.
      Serial.print(F("bad ntpTime, "));
      Serial.println(ntpTime);
      return;
    }
    
    if (ntpTime != 39)                          //If that worked, and we have a real time  
    {
      //Decide if it's time to make a new file or not. Files are broken
       //up like this to keep loading times for each chart bearable.
      //Lots of string stuff happens to make a new filename if necessary.
      if (ntpTime >= config.newFileTime)
      {
        generateNewFilename(ntpTime);
      }
     
      float temperature = getTemperature(T_SENSOR);
      
      char dataString[25] = "";
      //get the values and setup the string we want to write to the file
      float relativeHumidity  = getHumidity(temperature);
 
      Serial.print(F("temperature = "));
      Serial.print(temperature);
      Serial.print(F(" C, Relative humidity = "));
      Serial.print(relativeHumidity);
      Serial.println(F(" %"));
      
      char tempStr[12];
  
      ultoa(ntpTime,tempStr,10);        
      strcat(dataString,tempStr);
      strcat(dataString,",");

      dtostrf(relativeHumidity,6, 2, tempStr);
      strcat(dataString,tempStr);
      strcat(dataString,",");
      dtostrf(temperature,6, 2, tempStr);
      strcat(dataString,tempStr);
    
       //open the file we'll be writing to.
      File dataFile = SD.open(config.workingFilename, FILE_WRITE);
   
      // if the file is available, write to it:
      if (dataFile) {
        dataFile.println(dataString);
        dataFile.close();
  
        Serial.println(dataString);
      }  
      // if the file isn't open, pop up an error:
      else {
        Serial.println(F("Error opening datafile."));
      }
      
      //Update the time of the last measurment to the current timer value
      lastIntervalTime = millis();  
    }
  }
}

void writeTimeToScratchpad(byte* address)
{
  ONE_WIRE_BUS.reset();          // Reset 1 wire bus
  ONE_WIRE_BUS.select(address);  // Select DS1820 sensor
  ONE_WIRE_BUS.write(0x44,1);    // Richiamo la funzione di conversione temperatura (44h)
  delay(250);
}
 
void readTimeFromScratchpad(byte* address, byte* data)
{
  ONE_WIRE_BUS.reset();           // Resetto il bus
  ONE_WIRE_BUS.select(address);   // Seleziono la mia sonda DS18S20
  ONE_WIRE_BUS.write(0xBE);       // Comando di lettura dello scratchpad (comando Read Scratchpad)
  for (byte i=0;i<9;i++)
  {
    data[i] = ONE_WIRE_BUS.read(); // Leggo i 9 bytes che compongono lo scratchpad
  }
}
 
float getTemperature(byte* address)
{
  int tr;
  byte data[12];
  writeTimeToScratchpad(address); // Richiamo conversione temperatura
  readTimeFromScratchpad(address,data); // effettuo la lettura dello Scratchpad
  tr = data[0];   // Posiziono in TR il byte meno significativo
  // il valore di temperatura è contenuto nel byte 0 e nel byte 1
 
  // Il byte 1 contiene il segno
  if (data[1] > 0x80) // Se il valore è >128 allora la temperatura è negativa
  {
    tr = !tr + 1; // Correzione per complemento a due
    tr = tr * -1; // Ottengo il valore negativo
  }
 
  int cpc = data[7];   // Byte 7 dello scratchpad: COUNT PER °C (10h)
  int cr = data[6];   // Byte 6 dello scratchpad: COUNT REMAIN (0Ch)
  tr = tr >> 1;   // Rilascio il bit 0 come specificato dal datasheet per avere una risoluzione > di 9bit
 
  // Calcolo la temperatura secondo la formula fornita sul datasheet:
  // TEMPERATURA = LETTURA - 0.25 + (COUNT PER °C - COUNT REMAIN)/(COUNT PER °C)
 
  return tr - (float)0.25 + (cpc - cr)/(float)cpc;
}

void idleTask()
{
  watchDog_reset();
}

