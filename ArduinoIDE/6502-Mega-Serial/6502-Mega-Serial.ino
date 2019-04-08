/*
  6502 CPU connected with an Arduino Mega 2560 

  DDRx  = port X Data Direction Register
  PINx  = port X Input Pins Register
  PORTx = port X Data Register 
  Port     7   6   5   4   3   2   1   0
  ------ |---|---|---|---|---|---|---|---|
  digital
  PORTA:  29, 28, 27, 26, 25, 24, 23, 22
  PORTB:  13, 12, 11, 10, 50, 51, 52, 53
  PORTC:  30, 31, 32, 33, 34, 35, 36, 37
  PORTD:  38,  X,  X,  X, 18, 19, 20, 21
  PORTE:   X,  X, 03, 02, 05, xx, 01, 00
  PORTG:   X,  X, 04,  X,  X, 39, 40, 41
  PORTH:   X, 09, 08, 07, 06,  X, 16, 17
  PORTJ:   X,  X,  X,  X,  X,  X, 14, 15
  PORTL:  42, 43, 44, 45, 46, 47, 48, 49
  analog
  PORTF:  07, 06, 05, 04, 03, 02, 01, 00
  PORTK:  15, 14, 13, 12, 11, 10, 09, 08

  6502 + Arduino Mega 2560
    6502 lines:
      Address Bus Low : PA0-PA7   (22 to 29)
      Address Bus High: PC0-PC7   (37 to 30)
      PHI0            : PD7       (38)
      PHI2            : PG2       (39)
      RW              : PG1       (40)
      RST             : PG0       (41)
      Data Bus        : PL0-PL7   (49 to 42)
    
    Para debug:
      DBG_MCU         : PB0       (53)
*/
 
#include <Arduino.h>

// Clock Period in microseconds (divisible by 4)
//#define FREQ_6502         1000000UL   // 1Hz
//#define FREQ_6502          200000UL   // 5Hz
//#define FREQ_6502          100000UL   // 10Hz
//#define FREQ_6502           40000UL   // 25Hz
//#define FREQ_6502           10000UL   // 100Hz
//#define FREQ_6502           6000UL   // 166.66Hz
#define FREQ_6502           5400UL   // 185.185Hz
//#define FREQ_6502            5000UL   // 200Hz  -- limit?
//#define FREQ_6502            1000UL   // 1KHz  
//#define FREQ_6502             400UL   // 2.5KHz   
//#define FREQ_6502             200UL   // 5KHz 
//#define FREQ_6502             128UL   // 7.8125KHz   
//#define FREQ_6502             100UL   // 10KHz   

void setup() {
  // 6502 RST: PG0
  DDRG  |= B00000001;       // output
  PORTG &= B11111110;       // freeze the 6502 - RST to LOW

  // 6502 RW: PG1
  DDRG  &= B11111101;       // input
  PORTG &= B11111101;       // disable pullup    

  // 6502 PHI2: PG2
  DDRG  &= B11111011;       // input
  PORTG &= B11111011;       // disable pullup  
  
  // 6502 PHI0: PD7
  DDRD  |= B10000000;       // output
  PORTD &= B01111111;       // LOW first time

  // 6502 ADDR BUS: PC7-PC0 PA7-PA0
  DDRC   = B00000000;       // input
  PORTC  = B00000000;       // disable pullup
  DDRA   = B00000000;       // input
  PORTA  = B00000000;       // disable pullup

  // 6502 DATA BUS: PL7-PL0
  DDRL   = B00000000;       // input
  PORTL  = B00000000;       // disable pullup

  // Debug: PB0 
  DDRB  |= B00000001;       // ouput
  PORTB &= B11111110;       // LOW first time

  // Serial conn
  Serial.begin( 1000000 );
  delay( 500 );
  Serial.flush();
  while( Serial.available() > 0 )
    Serial.read();
  delay( 500 );
}


void loop() {
  uint16_t addr;            // 6502 address bus
  uint8_t b, phi2, rw;      // a byte and the 6502 phi2 and rw lines
  uint8_t boot = true;      // 6502 is booting
  uint8_t go = true;        // go on phi2 rising edge
  uint8_t data_out[4];           

  // SYNC with memory/devices server
  while( ( Serial.read() ) != 0xAA );
  while( ( Serial.read() ) != 0xAA );
  Serial.write( 0x55 );
  Serial.flush();
  delay( 2000 );
  
  // wait some cycles
  b = 0;
  while( b<5 ){
    if( doClock() == 0x81 )
      b++;  
  }
  
  // start the 6502
  PORTG |= B00000001;       // RST to HIGH
  
  // forever
  while( 1 ){
    // 6502 PHI0
    doClock();
    
    // need PHI2 and RW
    b = PING;
    phi2 = b & B00000100;
    rw   = b & B00000010;

    // R/W only on phi2 rising edge
    if( phi2 ){
      // only once
      if( go ){
        // address bus
        addr = ( ((uint16_t)PINC)<<8 ) | PINA;

        // booting? need =xFFFC on address bus
        if( boot ){
            if( addr != 0xFFFC ){
              go = false;
              continue;
            }
            boot = false;
        }

        // init debug
        PORTB |= B00000001;

        data_out[1] = (addr>>8) & 0xFF;
        data_out[2] = addr & 0xFF;
        
        // read from memory/peripheral
        if( rw ){
          // request the byte from the memory/peripheral
          data_out[0] = 'R';
          data_out[3] = 0x00;
          
          Serial.write( data_out, 4 );
          Serial.flush();
          
          // get the byte
          while( Serial.available()<= 0 );
          b = Serial.read() & 0xFF;
          
          // put it on data bus
          DDRL = B11111111;     // output mode
          PORTL = b;            // the byte
        }
        // write to memory/peripheral
        else{
          // get the byte from the data bus
          b = PINL;

          // send it to memory/peripheral
          data_out[0] = 'W';
          data_out[3] = b;
          
          Serial.write( data_out, 4 );
          Serial.flush();

          // get answer
          while( Serial.available()<= 0 );
          Serial.read() & 0xFF;
        }
        
        // end debug
        PORTB &= B11111110;

        go = false;
      }
    }
    else {
      // HI-Z data bus on phi2 falling edge
      setHIZ();
      
      // for the next phi2 rising edge
      go = true;
    }    
  }
}


// clock for the 6502
// 0x00: no change - 0x80: falling edge - 0x081: rising edge
byte doClock(){
  static unsigned long last_time = FREQ_6502;

  if( micros() - last_time > FREQ_6502/2UL ){
    last_time = micros();
    if( PORTD & B10000000 ){
      PORTD &= B01111111;
      return 0x80;
    }
    else {
      PORTD |= B10000000;
      return 0x81;
    }
  }  
  return 0x00;
}


// set data bus in HI-Z
uint8_t setHIZ(){
  DDRL   = B00000000;       // input mode
  PORTL  = B00000000;       // disable pullup
}
