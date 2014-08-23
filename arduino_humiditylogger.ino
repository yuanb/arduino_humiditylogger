#include <SD.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <Time.h>

#include "configuration.h"
#include "EEPROMAnything.h"
#include "freeram.h"
#include "watchDog.h"
#include "ntpTask.h"
#include "serverTask.h"
#include "sampleSensorTask.h"

#define FILE_INTERVAL 604800        //One week worth of seconds

OneWire  ONE_WIRE_BUS(2);  // Bus One-Wire (sonda DS18S20) sul pin 10
configuration config;               //Actually make our config struct

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

void idleTask()
{ 
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


