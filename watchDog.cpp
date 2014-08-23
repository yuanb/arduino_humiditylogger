#include <avr/wdt.h>
#include "watchDog.h"
#include <Arduino.h>

#define ledPin 13

//Arduino watchdog
//http://forum.arduino.cc/index.php?PHPSESSID=ca9vdgoaf8au01thde4locivn5&action=dlattach;topic=63651.0;attach=3585
//https://www.youtube.com/watch?v=BDsu8YhYn8g
void watchdogSetup(void)
{
#if 1
  pinMode(ledPin, OUTPUT);
#endif

  noInterrupts();          // disable all interrupts
  wdt_reset();    // reset the WDT timer
  
  wdt_enable(WDTO_2S);
#if 0  
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
#endif

#if 1
  // initialize timer1 
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 31250;            // compare match register 16MHz/256/2Hz
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
#endif

  interrupts(); 
}

void watchDog_reset(void)
{
  wdt_reset(); 
}

ISR(WDT_vect) // Watchdog timer interrupt.
{
// Include your code here - be careful not to use functions they may cause the interrupt to hang and
// prevent a reset.
}

#if 1
ISR(TIMER1_COMPA_vect)          // timer compare interrupt service routine
{
  digitalWrite(ledPin, digitalRead(ledPin) ^ 1);   // toggle LED pin
  watchDog_reset();
}
#endif
