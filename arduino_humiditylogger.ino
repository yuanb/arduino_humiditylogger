#include <SD.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
//#include <Timezone.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <avr/wdt.h>
#include "EEPROMAnything.h"

/*** DATA LOGGER AND TIMER CONTROLS ****/
#define HIH4030_Pin  0
#define MEASURE_INTERVAL 600000     //10 minute intervals between measurements (in ms)
#define FILE_INTERVAL 604800        //One week worth of seconds

//A structure that stores file config variables from EEPROM
typedef struct{
	unsigned long newFileTime;      //Keeps track of when a newfile should be made.
	char workingFilename[19];       //The path and filename of the current week's file
} configuration;
configuration config;               //Actually make our config struct

#define NTP_PACKET_SIZE   48
#define  eStateTimeRead   0
#define  eStateNTPSent    1
unsigned long ntpTime = 0;

EthernetServer server(80);
EthernetUDP Udp;

OneWire  ONE_WIRE_BUS(2);  // Bus One-Wire (sonda DS18S20) sul pin 10

// Indirizzo della sonda DS18S20
// Nota: trova l'indirizzo della tua sonda col programma precedente e incollalo al posto di questo
byte T_SENSOR[8] = {0x10, 0x8F, 0x43, 0x37, 0x00, 0x08, 0x00, 0x2A};

int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

//Arduino watchdog
//http://forum.arduino.cc/index.php?PHPSESSID=ca9vdgoaf8au01thde4locivn5&action=dlattach;topic=63651.0;attach=3585
//https://www.youtube.com/watch?v=BDsu8YhYn8g
void watchdogSetup(void)
{
  cli();          // disable all interrupts
  wdt_reset();    // reset the WDT timer
  /*
  WDTCSR configuration:
  WDIE = 1: Interrupt Enable
  WDE = 1 :Reset Enable
  WDP3 = 0 :For 2000ms Time-out
  WDP2 = 1 :For 2000ms Time-out
  WDP1 = 1 :For 2000ms Time-out
  WDP0 = 1 :For 2000ms Time-out
  */
  // Enter Watchdog Configuration mode:
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  // Set Watchdog settings:
  WDTCSR = (1<<WDIE) | (1<<WDE) | (0<<WDP3) | (1<<WDP2) | (1<<WDP1) | (1<<WDP0);
  sei();
}

ISR(WDT_vect) // Watchdog timer interrupt.
{
// Include your code here - be careful not to use functions they may cause the interrupt to hang and
// prevent a reset.
}

void setup() {  
  // Initialize the Ethernet server library
  // with the IP address and port you want to use 
  // (port 80 is default for HTTP):

  // Enter a MAC address and IP address for your controller below.
  // The IP address will be dependent on your local network:
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  IPAddress ip(192,168,0,177);
  unsigned int localPort = 8888;          // local port to listen for UDP packets  
  
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

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Udp.begin(localPort);
  
  EEPROM_readAnything(0,config); // make sure our config struct is syncd with EEPROM

  watchdogSetup();
  Serial.println(F("setup done."));
}

// A function that takes care of the listing of files for the
// main page one sees when they first connect to the arduino.
// it only lists the files in the /data/ folder. Make sure this
// exists on your SD card.
void ListFiles(EthernetClient client) {

	File workingDir = SD.open("/data");

	client.println("<ul>");

	while (true) {
		File entry = workingDir.openNextFile();
		if (!entry) {
			break;
		}
		client.print("<li><a href=\"/HC.htm?file=");
		client.print(entry.name());
		client.print("\">");
		client.print(entry.name());
		client.println("</a></li>");
		entry.close();
	}
	client.println("</ul>");
	workingDir.close();
}

// A function to get the Ntp Time. This is used to make sure that the data
// points recorded by the arduino are referenced to some meaningful time
// which in our case is UTC represented as unix time (choosen because it 
// works simply with highcharts without too much unecessary computation).

unsigned long getNtpTime(byte* packetBuffer)
{
  unsigned long ret = 0;

  if ( Udp.parsePacket() ) {  
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer
 
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
 
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;  
    
    //time_t eastern;
    //TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
    //TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours
    //Timezone usEastern(usEDT, usEST);
    
    // return Unix time:
    //ret = usEastern.toLocal(epoch);
    ret = epoch;
  }
  return ret;  
}

// send an NTP request to the time server at the given address,
// necessary for getTime().
void sendNTPpacket(IPAddress& address, byte* packetBuffer)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
 
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:         
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
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

void serveClientTask()
{
// How big our line buffer should be for sending the files over the ethernet.
// 75 has worked fine for me so far.
#define BUFSIZ 75
  char clientline[75];

   // listen for incoming clients
   EthernetClient client = server.available();
   if (client)
   {
      Serial.print(F("freeRam @ serveClient()="));
      Serial.println(freeRam());         

      // reset the input buffer
      int index = 0;

      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          
          // If it isn't a new line, add the character to the buffer
          if (c != '\n' && c != '\r')
          {
            clientline[index] = c;
            index++;
            
            // are we too big for the buffer? start tossing out data
            if (index >= BUFSIZ) 
              index = BUFSIZ -1;
              
              // continue to read more data!
              continue;
          }
         
          // got a \n or \r new line, which means the string is done
          clientline[index] = 0;
         
          // Print it out for debugging
          Serial.println(clientline);
        
          // Look for substring such as a request to get the root file
          if (strstr(clientline, "GET / ") != 0)
          {
             client.println(F("HTTP/1.1 200 OK"));
             client.println(F("Content-Type: text/html"));
             client.println();
             client.println(F("<h2>View data for the week of (dd-mm-yy):</h2>"));
             ListFiles(client);
          }
          else if (strstr(clientline, "GET /") != 0)
          {            
            // this time no space after the /, so a sub-file!
            char *filename;
            
            filename = strtok(clientline + 5, "?"); // look after the "GET /" (5 chars) but before
            // the "?" if a data file has been specified. A little trick, look for the " HTTP/1.1"
            // string and turn the first character of the substring into a 0 to clear it out.
            (strstr(clientline, " HTTP"))[0] = 0;
            
            File file = SD.open(filename,FILE_READ);
            if (!file)
            {
              client.println(F("HTTP/1.1 404 Not Found"));
              client.println(F("Content-Type: text/html"));
              client.println();
              break;
            }

            client.println(F("HTTP/1.1 200 OK"));
            client.println(F("Content-Type: text/html"));
            client.println(F("Connection: close"));
            client.println();
            
            while(file.available()) {
              client.write(file.read());
            }
            
            //int16_t c;
            //while ((c = file.read()) > 0)
            //{

            //  client.print((char)c);
            // }
            file.close();
          }
          else
          {
            // everything else is a 404
            client.println(F("HTTP/1.1 404 Not Found"));
            client.println(F("Content-Type: text/html"));
            client.println();
            client.println(F("<h2>File Not Found!</h2>"));
          }
          break;
        }
     }
    
     // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}

void updateTimeTask()
{
  static byte eState = eStateTimeRead;
  static unsigned long taskMillis = 0;
  byte packetBuffer[NTP_PACKET_SIZE];
  
  if (eState == eStateTimeRead && ((taskMillis == 0) || (millis() - taskMillis) >= 1000*20) ) {
    IPAddress timeServer(132, 163, 4, 101); //NIST time server IP address: for more info
                                            //see http://tf.nist.gov/tf-cgi/servers.cgi
    sendNTPpacket(timeServer, packetBuffer); // send an NTP packet to a time server
    eState = eStateNTPSent;
    taskMillis = millis();
  } else if (eState == eStateNTPSent && (millis() - taskMillis) >= 1000 ) {
    ntpTime = getNtpTime(packetBuffer);
    eState = eStateTimeRead;
    taskMillis = millis();
  }
}

void writeTimeToScratchpad(byte* address)
{
  ONE_WIRE_BUS.reset();          // Reset 1 wire bus
  ONE_WIRE_BUS.select(address);  // Select DS1820 sensor
  ONE_WIRE_BUS.write(0x44,1);    // Richiamo la funzione di conversione temperatura (44h)
  delay(1000);
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
  wdt_reset(); 
}

void loop()
{
  updateTimeTask();
  sampleSensorTask();
  serveClientTask();
  idleTask();
}

