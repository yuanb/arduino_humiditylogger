#ifndef  _NTPTask_H
#define  _NTPTASK_H

#define NTP_PACKET_SIZE   48
#define  eStateTimeRead   0
#define  eStateNTPSent    1

extern unsigned long ntpTime;

void ntpInit(void);
unsigned long getNtpTime(byte* packetBuffer);
void sendNTPpacket(IPAddress& address, byte* packetBuffer);

void updateTimeTask();

#endif  //_NTPTASK_H
