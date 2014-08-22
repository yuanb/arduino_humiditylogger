#include <SD.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <util.h>

#include "serverTask.h"
#include "freeram.h"

EthernetServer server(80);

void serverInit(void)
{
  // Initialize the Ethernet server library
  // with the IP address and port you want to use 
  // (port 80 is default for HTTP):

  // Enter a MAC address and IP address for your controller below.
  // The IP address will be dependent on your local network:
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  IPAddress ip(192,168,0,177);
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
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


// A function that takes care of the listing of files for the
// main page one sees when they first connect to the arduino.
// it only lists the files in the /data/ folder. Make sure this
// exists on your SD card.
void ListFiles(EthernetClient client)
{

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

