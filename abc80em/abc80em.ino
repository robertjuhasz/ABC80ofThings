
/************* ABC80 of Things ****************
 *                    - an ABC80 emulator for Arduino boards
 *  Copyright (c) Robert Juhasz 2018
 *  Based on the work of Marat Fayzullin for the Z80 emulation
 *  
 *  Provides
 *  - bare bones ABC80 emulation to run BASIC
 *  - bare bones ABC832 emulation to run floppy images with UFD DOS
 *  - ABC80 ROM with ufddos (out 100,80)
 *  - ABC800 basic adapted for ABC80 - i patched the output and called it ABC81 (out 100,81)
 *  - very studpid display emulation: for each byte written to the video ROM, a VT100 positioning + the character 
 *    is output. Can be quite slow but effective...
 *    
 *  Project goal: to be able to ineffectively script a few small nodes in ABC800 basic to collect and transmit 
 *  temperature data and humidity data, etc.
 *  
 *  I will possibly add an ESP8266 chip serially to the board and write a little device driver for ABC800 basic later..
 *  
 * 
 */

#include <SD.h>

#include "Z80.h"
#include "basicii.h"
#include "abc80rom.h"
#include "abc802rom.h"

#define RAMSIZE 32768
byte ram[RAMSIZE];
#define VMEMLEN 2048
#define VMEMOFS (32768-VMEMLEN);
byte vmem[VMEMLEN];

byte inchar;
byte cardsel=255;
File mffile[4];
static int mfdataidx = 0;
static int mfcmdidx = 0;
static byte mfcmd[4];
static byte mffilebuf[256];
static int mfstat1 = 128 + 8 + 1;
static int mfdrive,mfsec;

int machinetype;
const byte *abcrom;
sword romlen;

Z80 z80regs;


#define PRINTF_SERIAL
#ifdef __cplusplus
  extern "C" {
    //__attribute__((weak)) omit this as used in _write() in Print.cpp
  
    // this function overrides the one of the same name in Print.cpp
    int _write(int file, char *ptr, int len)
    {
        // send chars to zero or more outputs
        for (int i = 0; i < len; i++)  {
          #ifdef PRINTF_SERIAL  // see #define at top of file
            Serial.print(ptr[i]);
          #endif
          #ifdef PRINTF_SERIAL1  // see #define at top of file
            Serial1.print(ptr[i]);
          #endif
        }
        return 0;
    }
  } // end extern "C" section
#endif

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// Teensy audio board: pin 10
// Wiz820+SD board: pin 4
// Teensy 2.0: pin 0
// Teensy++ 2.0: pin 20
// CS for ESP8266 adafruit feather huzzah
const int chipSelect = BUILTIN_SDCARD; //Teensy 3.6    


/** RdZ80()/WrZ80() ******************************************/
/** These functions are called when access to RAM occurs.   **/
/** They allow to control memory access.                    **/
/************************************ TO BE WRITTEN BY USER **/


void outchar(byte c)
{
 Serial.print((char)c);
}

void poscur(byte r,byte c)
{
        Serial.print((char)27);
        Serial.print('[');
        Serial.print(r+1);
        Serial.print(';');
        Serial.print(c+1);
        Serial.print('H');
}

void waitesc()
{
  delay(3000);
}
void WrZ80(register sword Addr,register byte Value)
  {
    byte r,c;
    if (Addr >= (65536-RAMSIZE)) ram[(Addr-(65536-RAMSIZE)) & (RAMSIZE-1)]=Value;
     if (machinetype<800)  {
        if ((Addr >= (32768-2048)) && (Addr < ( 32768))) 
          {
            vmem[(Addr-(32768-2048)) & 2047]=Value;
            //Serial.print(Addr-32768-2048);
            r=(byte)(( Addr-0x7c00 ) >> 7);
            c= (byte)(Addr & 127);
            if (c>=40) { c-=40; r+=8; }
            if (c>=40) { c-=40; r+=8; }
            poscur(r,c);
            outchar(((char)Value));
       
          }
        }
        else // abc80x
        {
            if ((Addr >= (32768-2048)) && (Addr < ( 32768))) 
            {
              vmem[(Addr-(32768-2048)) & 2047]=Value;
              //Serial.print(Addr-32768-2048);
              r=(byte)(( Addr-0x7800 ) /80);
              c= (byte)((Addr-0x7800) % 80);
     
              poscur(r,c);
              //Serial.print(r); Serial.print(" "); Serial.print(c);
              Serial.print((char)Value);
         
            }
        }
      
  }
byte RdZ80(register sword Addr)
  {
    // ROM
     if (Addr<romlen) {
        return abcrom[Addr];
     }
    // or RAM?
    if (Addr > 32767) {
        return ram[(Addr-32768) & 0x7fff];
     }
    // or VMEM?
    if ((Addr >= (32768-2048)) && (Addr < 32768) && (machinetype<800))
        return vmem[(Addr-(32768-2048)) & 2047];
    // otherwise return no memory
    return 255;
  }
/** InZ80()/OutZ80() *****************************************/
/** Z80 emulation calls these functions to read/write from  **/
/** I/O ports. There can be 65536 I/O ports, but only first **/
/** 256 are usually used.                                   **/
/************************************ TO BE WRITTEN BY USER **/
void OutZ80(register sword Port,register byte Value)
  {
  
  if (cardsel == 44)    // if mf selected
      {
        switch (Port & 0xff)
        {
        case 0:
          if (mfcmdidx < 4)
          {
            mfcmd[mfcmdidx++] = Value;
            mfdataidx = -1;
          }         // set data idx to -1 to flag command is
          // starting
          if (mfcmdidx > 3)
          {
            if (mfcmd[0] == 3)
            {       // / a read command
                mfdrive=mfcmd [1]&3;
                mfsec=(mfcmd[3] & 2)+(mfcmd[3]>>5)*4; // abc832 has cluster size 4. bit0-1 sec within cluster, bit 5-7 cluster no
 
               if (mffile [mfdrive]) mffile[mfdrive].seek(((mfcmd[2]<<5)+((mfcmd[3]>>5)<<2)+(mfcmd[3] & 3))*256);
              if (mffile [mfdrive])  
                 mffile[mfdrive].read(mffilebuf,256);
              
              mfdataidx = 0;  // set to 0 to be readyfor buffer read
              mfstat1 = 8 + 1;
              mfcmdidx = 0;
            }
            if (mfcmd[0] == 12)
            {       // / a write command
    
              if ((mfdataidx < 256) && (mfdataidx >= 0))
              {
                mffilebuf[mfdataidx] = (byte) (Value & 0xff);

              }
              mfdataidx++;  // increase pointer: first time we will get
              // here will be when then command executes
              if (mfdataidx > 256)
                mfdataidx = 256;  // limit dataidx to 256
              mfstat1 = 8 + 1;
              if (mfdataidx == 256)
              {
                // if buffer is full, write it!
                mfdrive=mfcmd [1]&3;
                mfsec=(mfcmd[3] & 2)+4*(mfcmd[3]>>5); // abc832 has cluster size 4. bit0-1 sec within cluster, bit 5-7 cluster no
                if (mffile [mfdrive]) mffile[mfdrive].seek(((mfcmd[2]<<5)+((mfcmd[3]>>5)<<2)+(mfcmd[3] & 3))*256);
                if (mffile [mfdrive])  
                   mffile[mfdrive].write(mffilebuf,256);   
                mfcmdidx = 0;
                mfdataidx = 0;
                mfstat1 = 8 + 1 + 128;

              }
            }
    
          }
          break;
    
        case 3:
          break;
    
        case 2:
          mfdataidx = 0;
          mfcmdidx = 0;
          mfstat1 = 128 + 8 + 1;
          break;
    
        }
      }

      switch(Port & 0xff)
      {
        case 100: 
          machinetype=1024+Value; // 102 for 802, 80 for 80, 81 for 81 etc... add 1024 to indicate newly set value
          break;
        case 1: 
          cardsel=Value; // card select
         break;
      }
      
        
  
  }
  
byte InZ80(register sword Port)
  {
    byte res;
    //printf("IN: %i\n",Port & 255);
    switch(Port & 255)
    {
      case 56: res= inchar; break;
      default: res= 255;
    }

   if (cardsel == 44)    // 44 for mf (ABC832)
          { 
            switch (Port & 0xff)
            {
            case 0:
              if (mfcmd[0] == 12)
                res = 0;
              else if (mfcmd[0] == 3)
              {
                if (mfdataidx < 256)
                {
                  res = (mffilebuf[mfdataidx++] & 0xff);
                }
                else
                  res = 0;
              }
              mfstat1 = 128 + 8 + 1;
              break;
            case 1:
              res = mfstat1;
              break;
            }
        
        
          }
    return res;
  }

/** PatchZ80() ***********************************************/
/** Z80 emulation calls this function when it encounters a  **/
/** special patch command (ED FE) provided for user needs.  **/
/** For example, it can be called to emulate BIOS calls,    **/
/** such as disk and tape access. Replace it with an empty  **/
/** macro for no patching.                                  **/
/************************************ TO BE WRITTEN BY USER **/
void PatchZ80(register Z80 *R)
  {
    
  }

/** DebugZ80() ***********************************************/
/** This function should exist if DEBUG is #defined. When   **/
/** Trace!=0, it is called after each command executed by   **/
/** the CPU, and given the Z80 registers. Emulation exits   **/
/** if DebugZ80() returns 0.                                **/
/*************************************************************/
#ifdef DEBUG
byte DebugZ80(register Z80 *R)
  {
    
  }
#endif

/** LoopZ80() ************************************************/
/** Z80 emulation calls this function periodically to check **/
/** if the system hardware requires any interrupts. This    **/
/** function must return an address of the interrupt vector **/
/** (0x0038, 0x0066, etc.) or INT_NONE for no interrupt.    **/
/** Return INT_QUIT to exit the emulation loop.             **/
/************************************ TO BE WRITTEN BY USER **/
sword LoopZ80(register Z80 *R)
  {
    //delay(2);
  uint8_t i;
  bool charavail=false;
 
    if (inchar & 128 ) inchar = inchar & 127;
    if (Serial.available())
      {
        inchar=(byte)(Serial.read() & 255) | 128;
        //Serial.print(inchar);
        //inchar='#'+128;
        charavail = true;
        
      }
    digitalWrite(0,!digitalRead(0));
    if (charavail) {
      charavail = false;
      if (machinetype==81)
          return (90 << 1);
         if (machinetype==80)
          return (26 <<1 );
    }
     
  
    if (machinetype & 1024)
   {  
    machinetype &=1023;
    return INT_QUIT; // quit and restart
   }
    
    return INT_NONE;
  }

void setmachine()
{
  machinetype &= 1023;  // filter away newly set flag
switch(machinetype)
{ 

  case 102:
      abcrom=abcrom802;
      romlen=romlen802;
      break;
  case 81: 
      abcrom=abcrom81;
      romlen=romlen81;
      break;
  case 80:
  default: 
      abcrom=abcrom80;
      romlen=romlen80;
}


  
}


void openfiles()
{
  char ff[]="/MF0.DSK";
  for (int ii=0;ii<4;ii++)
  {
    ff[3]='0'+ii;
    if (mffile[ii]) {
     mffile[ii].close();
      Serial.print("Closed "); Serial.println(ff);
      }
    mffile[ii]=SD.open(ff,FILE_WRITE);
    if (mffile[ii])
        Serial.print("Success opening ");
    else
        Serial.print("Could not open "); 
    Serial.println(ff);
  }
  
}

void setup() {
  // put your setup code here, to run once:
  inchar=0;
  //pinMode(13,OUTPUT);  
  Serial.begin(115200);
    while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  machinetype=81;
  waitesc(); // strictly not necessary...
}

void loop() {
  // put your main code here, to run repeatedly:

  digitalWrite(13,1);
  Serial.println("Hello!");
  Serial.println(sizeof(sword));
  Serial.println(sizeof(byte));
  openfiles();
  //machinetype = 1;
  setmachine();
  z80regs.IPeriod=10000;
  ResetZ80(&z80regs);
  Serial.println("Reset done");
  RunZ80(&z80regs);
}
