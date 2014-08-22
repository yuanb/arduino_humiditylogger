#include <avr/wdt.h>
#include "watchDog.h"
#include <Arduino.h>

//Arduino watchdog
//http://forum.arduino.cc/index.php?PHPSESSID=ca9vdgoaf8au01thde4locivn5&action=dlattach;topic=63651.0;attach=3585
//https://www.youtube.com/watch?v=BDsu8YhYn8g
void watchdogSetup(void)
{
  noInterrupts();          // disable all interrupts
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
  interrupts();
}

ISR(WDT_vect) // Watchdog timer interrupt.
{
// Include your code here - be careful not to use functions they may cause the interrupt to hang and
// prevent a reset.
}

 void watchDog_reset(void)
{
  wdt_reset(); 
}

