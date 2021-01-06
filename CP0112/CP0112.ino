/*
CP0112 led display exemple Cyrob 2020
Arduino nano

This is an exemple or interupt driven 4 digit multiplexed display

=====================================================================================
==========================   OPEN SOURCE LICENCE   ==================================
=====================================================================================

Copyright 2020,2021,2022 Philippe Demerliac Cyrob.org

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

................................................................................................................
Release history
................................................................................................................
Version Date      Author    Comment
1.0d1   27/12/20  Phildem   First blood
1.0     28/12/20  Phildem   Display complete
1.1     30/12/20  Phildem   Interupt delay optim
*/

//OptimIO ......................................................................................................
#define CmpOpt_FastIO
#ifdef CmpOpt_FastIO
  #define setPin(b) ( (b)<8 ? PORTD |=(1<<(b)) : PORTB |=(1<<(b-8)) )
  #define clrPin(b) ( (b)<8 ? PORTD &=~(1<<(b)) : PORTB &=~(1<<(b-8)) )
  #define tstPin(b) ( (b)<8 ? (PORTD &(1<<(b)))!=0 : (PORTB &(1<<(b-8)))!=0 )
  
  #define FastDigW(b,How) if (How) setPin(b); else clrPin(b)
  
#endif

//debugTime ......................................................................................................
#define CmpOpt_DebugTim

#ifdef CmpOpt_DebugTim
  #define   kDebugOutSetV     A0
#endif

//IO Abstraction ...............................................................................................

// Intended for 4 common cathode digit number from 0 to 3 (left to right)
// Define pins for segments and transistors base (positiv logic)

#define   kDispOutSegA        13   // Display Segment (Activ high)
#define   kDispOutSegB        6
#define   kDispOutSegC        7
#define   kDispOutSegD        8
#define   kDispOutSegE        9
#define   kDispOutSegF        10
#define   kDispOutSegG        11
#define   kDispOutSegDP       12

#define   kDispOutDig0        5   // Display (Activ high) MSB
#define   kDispOutDig1        4
#define   kDispOutDig2        3
#define   kDispOutDig3        2  // LSB

// Segments display LUT
const byte KDispSeg[]= {
  B00111111,    //0
  B00110000,    //1
  B01101101,    //2
  B01111001,    //3
  B01110010,    //4
  B01011011,    //5 
  B01011111,    //6
  B00110001,    //7
  B01111111,    //8
  B01111011,    //9
  B00000000,    //Blank
  B01000000,    //Minus
  B00000001,    //Overflow
  B00001000     //UnderFlow
  };

#define   kDispSegBlank        10   // Blank value
#define   kDispSegMinus        11   // Minus sign
#define   kDispSegOver         12   // Minus sign
#define   kDispSegUnder        13   // Minus sign

#define   kDispMaxVal         9999   // Maximum display value
#define   kDispMinVal         -999   // Minimum display value

const byte KDispDigit[]= {kDispOutDig0,kDispOutDig1,kDispOutDig2,kDispOutDig3};

//Misc constants ...............................................................................................

#define   kSerBaudRate          9600  //  Serial output baudrate to be used if serial IO used

//============================================================================================================
// Class DispMux, 4 digit display Multiplex
// Can display signed int value from -999 to 9999
// Display up or bottom line if out of range
// Handle Decimal Point and trailling zero suppression
//____________________________________________________________________________________________________________ 

class DispMux {
  
  public:
  //============================================================
  // Must be called once at init
  void Init(){

    cli();  // idem noInterrupts(); Do not allow interupt here

    for (int i=0;i<4;i++){
      // Set cathode driver as output
      pinMode(KDispDigit[i], OUTPUT);
      // Set cathode driver inactiv
      digitalWrite(KDispDigit[i], LOW); 
    }

    // Set segments driver as output
    pinMode(kDispOutSegA, OUTPUT); 
    pinMode(kDispOutSegB, OUTPUT); 
    pinMode(kDispOutSegC, OUTPUT); 
    pinMode(kDispOutSegD, OUTPUT); 
    pinMode(kDispOutSegE, OUTPUT); 
    pinMode(kDispOutSegF, OUTPUT); 
    pinMode(kDispOutSegG, OUTPUT); 
    pinMode(kDispOutSegDP, OUTPUT);

    #ifdef CmpOpt_DebugTim
      pinMode(kDebugOutSetV, OUTPUT);
    #endif

    // Init members
    m_CurDisp=3;      // To start a new cycle
    m_Display=1;      // Insure non 0 value
    m_Dp=3;           // Dp on digit 3

    // Init Timer 2
    TCCR2A = 0;           //default
    TCCR2B = 0b00000110;  // Set prescaler clk/256 16us 
    TIMSK2 = 0b00000001;  // TOIE2
        
    sei(); // idem interrupts();  Interupts are allowed
    Display(0); // Set to 0
  };

  //==========================================================================================
  //Method to set the value to display 
  void Display(int Value){
    
      if (Value==m_Display)   // Optim if value unchanged
        return;

#ifdef CmpOpt_DebugTim
      digitalWrite(kDebugOutSetV, HIGH);    
#endif

      m_Display=Value;

      int  DBuff=m_Display;   // Update current value to display     
   
      int  Div=1000;          // Digit decade divider
      byte Rbi=m_Dp;          // First non blanked digit 
      byte DigVal;            // Digit loop calculation
      
      for (byte Digit=0;Digit<4;Digit++){
        
        if (DBuff>kDispMaxVal)      // Display Overflow if needed
          DigVal=kDispSegOver;
        else if (DBuff<kDispMinVal) // Display Underflow if needed
         DigVal=kDispSegUnder;
        else{
          // Calculate digit value
    
          if (DBuff<0){              // Handle negativ value and display the sign
            DigVal=kDispSegMinus;
            DBuff=-DBuff;
          } 
          else{
            DigVal=DBuff/Div;
            DBuff-=(DigVal*Div);
            
            if (Rbi>Digit){     // Handle ripple blanking for hid trailing zero
              if (DigVal==0)
                DigVal=kDispSegBlank;
              else
                Rbi=Digit;
            }
          }
          Div=Div/10;
        }  
        m_SegCache[Digit]=DigVal;
      }
      
#ifdef CmpOpt_DebugTim
      digitalWrite(kDebugOutSetV, LOW);    
#endif
      
   }

  //==========================================================================================
  //Method to set the decimal point
  void SetDp(byte Dp){
      m_Dp=Dp;
      int Tmp=m_Display;    // This code to force trailing zero display refresh
      m_Display++;
      Display(Tmp);
   } 

  //==========================================================================================
  //Method to call during Timer2 interupt
  void Interupt(){

#ifdef CmpOpt_FastIO
    // Switch off current Digit
    clrPin(KDispDigit[m_CurDisp]);

    m_CurDisp++;
    if (m_CurDisp>3)
      m_CurDisp=0;
    
    byte Segs=KDispSeg[m_SegCache[m_CurDisp]];

    // Output segments
    FastDigW(kDispOutSegA,Segs & 0x1);
    FastDigW(kDispOutSegB,Segs & 0x2);
    FastDigW(kDispOutSegC,Segs & 0x4);
    FastDigW(kDispOutSegD,Segs & 0x8);
    FastDigW(kDispOutSegE,Segs & 0x10);
    FastDigW(kDispOutSegF,Segs & 0x20);
    FastDigW(kDispOutSegG,Segs & 0x40);

    // Handle Decimal point
    FastDigW(kDispOutSegDP,m_CurDisp==m_Dp);

    // Light up the current digit 
    setPin(KDispDigit[m_CurDisp]);

#else
    // Switch off current Digit
    digitalWrite(KDispDigit[m_CurDisp], LOW);

    m_CurDisp++;
    if (m_CurDisp>3)
      m_CurDisp=0;
    
    byte Segs=KDispSeg[m_SegCache[m_CurDisp]];

    // Output segments 
    digitalWrite(kDispOutSegA,Segs & 0x1);
    digitalWrite(kDispOutSegB,Segs & 0x2);
    digitalWrite(kDispOutSegC,Segs & 0x4);
    digitalWrite(kDispOutSegD,Segs & 0x8);
    digitalWrite(kDispOutSegE,Segs & 0x10);
    digitalWrite(kDispOutSegF,Segs & 0x20);
    digitalWrite(kDispOutSegG,Segs & 0x40);

    // Handle Decimal point
    digitalWrite(kDispOutSegDP,m_CurDisp==m_Dp);

    // Light up the current digit 
    digitalWrite(KDispDigit[m_CurDisp], HIGH);

#endif
    
    // Reload timer
    TCNT2 = 256-250; // --> 250x16us = 4ms
   } 

   private: 
     
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Members
  volatile int  m_Display;      // Current displayed value
  volatile byte m_Dp;           // Digit of decimal point 0->3
  byte          m_CurDisp;      // Current displayed digit
  byte          m_SegCache[4];  // Segment cache
};

//Global Var ...............................................................................................

  DispMux     gMux;   // Display class global instance

//============================================================================================================
// Arduino Serial
//____________________________________________________________________________________________________________

//===================================================================================
// Init Serial must be called once at init
void InitSerial(){
  Serial.begin(kSerBaudRate);
}

//============================================================================================================
// Arduino SetUp
//____________________________________________________________________________________________________________
void setup() {

  // Init Serial Port
  InitSerial();

 // Init Display
  gMux.Init();

  gMux.Display(8888); // test Display
  delay(2000);
  gMux.Display(0);
  delay(1000);
}

//============================================================================================================
//Arduino Main Loop
//____________________________________________________________________________________________________________  

void loop() {

  // Here the program has just to call gMux.Display and gMux.SetDp to set the display
  // All the Display is handle by the Timer2 interrupt

  // Display some tests

  gMux.Display(10000);
  delay(2000);
  gMux.Display(-1000);
  delay(2000);

  gMux.Display(0);
  gMux.SetDp(2);
  delay(2000);

  gMux.SetDp(1);
  delay(2000);

  gMux.SetDp(0);
  delay(2000);

  gMux.SetDp(2);
  
  // Demo program
  gMux.Display(1234); // test Display
  delay(500);
  gMux.Display(4000); // test Display
  delay(500);
  gMux.Display(9999); // test Display
  delay(500);

 for (int d=0;d<4;d++){
    gMux.SetDp(d);

    for (int i=-22;i<=22;i++){
      gMux.Display(i); // test Display
      delay(400);
    }
  }

  for (int i=-1000;i<=10000;i++){
    gMux.Display(i); // test Display
    delay(5);
  }
 
}

//============================================================================================================
//Timer 2 interupt
//____________________________________________________________________________________________________________  
ISR (TIMER2_OVF_vect)
{
  gMux.Interupt();
} 
