#include <SD.h>
#include <Ethernet.h>
#include <EthernetServer.h>
#include <util.h>

#include "serverTask.h"
#include "freeram.h"
#include "watchDog.h"

EthernetServer server(80);

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,0,177);

void serverInit(void)
{
  // Initialize the Ethernet server library
  // with the IP address and port you want to use 
  // (port 80 is default for HTTP):

  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
}

void sendClientConnectionClose(EthernetClient &client)
{
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));  
}

void sendClientOK(EthernetClient &client)
{
  client.println(F("HTTP/1.1 200 OK"));
  sendClientConnectionClose(client);
  client.println();
}

void sendClient404(EthernetClient &client)
{
  client.println(F("HTTP/1.1 404 Not Found"));
  sendClientConnectionClose(client); 
  client.println(F("<h2>File Not Found!</h2>"));  
}

void serveClientTask(void)
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
             sendClientOK(client);
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
              sendClient404(client);
              break;
            }

            sendClientOK(client);
#if  1
            Serial.print(F("freeRam before sending file="));
            Serial.println(freeRam());
            
            while(file.available()) {
              client.write(file.read());
            }
#else            
            int16_t c;
            watchDog_reset();
            while ((c = file.read()) > 0)
            {
              client.print((char)c);
            }
#endif
            file.close();
          }
          else
          {
            // everything else is a 404
            sendClient404(client);
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

// A function that takes care of the listing of files for the
// main page one sees when they first connect to the arduino.
// it only lists the files in the /data/ folder. Make sure this
// exists on your SD card.
void ListFiles(EthernetClient client)
{
  File workingDir = SD.open("/data");
  client.println(F("<ul>"));

  while (true)
  {
    File entry = workingDir.openNextFile();
    if (!entry)
    {
      break;
    }
    client.print(F("<li><a href=\"/HC.htm?file="));
    client.print(entry.name());
    client.print(F("\">"));
    client.print(entry.name());
    client.println(F("</a></li>"));
    entry.close();
  }
  client.println(F("</ul>"));
  workingDir.close();
}

