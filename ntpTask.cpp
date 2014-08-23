#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <util.h>
#include "ntpTask.h"

EthernetUDP Udp;
unsigned long ntpTime = 0;

// A function to get the Ntp Time. This is used to make sure that the data
// points recorded by the arduino are referenced to some meaningful time
// which in our case is UTC represented as unix time (choosen because it 
// works simply with highcharts without too much unecessary computation).

void ntpInit(void)
{
    unsigned int localPort = 8888;          // local port to listen for UDP packets 
    Udp.begin(localPort);
}

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

void updateTimeTask()
{
  static byte eState = eStateTimeRead;
  static unsigned long taskMillis = 0;
  byte packetBuffer[NTP_PACKET_SIZE];
  unsigned long currentMillis = millis();
  
  if (eState == eStateTimeRead && ((taskMillis == 0) || (currentMillis - taskMillis) >= 1000*20) )
  {
    IPAddress timeServer(132, 163, 4, 101); //NIST time server IP address: for more info
                                            //see http://tf.nist.gov/tf-cgi/servers.cgi
    sendNTPpacket(timeServer, packetBuffer); // send an NTP packet to a time server
    eState = eStateNTPSent;
    taskMillis = currentMillis;
  }
  else if (eState == eStateNTPSent && (currentMillis - taskMillis) >= 1000 )
  {
    ntpTime = getNtpTime(packetBuffer);
    eState = eStateTimeRead;
    taskMillis = currentMillis;
  }
}

