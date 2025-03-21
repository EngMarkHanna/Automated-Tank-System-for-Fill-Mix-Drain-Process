#include <p18cxxx.h>
#include <delays.h>
#include <LCD4PICPROTOv1.h>
#include "BCDlib.h"

#pragma     config      PBADEN = OFF          // Port B AD enable off
#pragma     config      FOSC = INTIO67        // Oscillator Selection bits (Internal oscillator block)

#define     StartStop     FLAGS.B0            // StartStop = 1/0 (start/stop) 
#define     FillValve     LATDbits.LATD0
#define     DrainValve    LATDbits.LATD1
#define     Mixer         LATDbits.LATD2
#define     Buzzer        LATDbits.LATD3
#define     TankFull      LATDbits.LATD4
#define     TankEmpty     LATDbits.LATD5


enum {OFF, ON};
enum {Fill, Mix, Drain} State = Fill;   
volatile unsigned char count = 0;           //counter for mix state
volatile unsigned int percentage = 0;       //Approx percentage 
volatile unsigned char Digits[5];           //Used for LCD and SSD 

void setup(void);
void ISR(void);
void Beep(void);



void main(void) {
    setup();
    while (1) {
        switch (State) {
            case Fill: 
                if (StartStop) {
                    FillValve = ON;
                    DispRomStr(Ln1Ch0, (ROM *) "State: Fill     ");
                    DispRomStr(Ln2Ch0, (ROM *) "Fill Valve:   ON");
                                                        
                    //When tank fills up, DAC is at max value, change state
                    if(TankFull){
                        State = Mix;
                        FillValve = OFF;
                        DispRomStr(Ln1Ch0, (ROM *) "Next State: Mix ");
                        DispRomStr(Ln2Ch0, (ROM *) "Fill Valve:  OFF");
                        Beep();         //Beep for 3 seconds  
                        count = 20;     //Timer = 20 seconds
                    }
                    
                }
                else {
                    //User stops process, close fill valve, change LCD output
                    FillValve = OFF;
                    DispRomStr(Ln2Ch0, (ROM *) "Fill Valve:  OFF");
                }
                break;

            case Mix:  
                DispRomStr(Ln1Ch0, (ROM *) "Mix is: ON      ");  
                DispRomStr(Ln2Ch0, (ROM *) "Time left:     s");
                Mixer = ON;
                
                //count variable decrements every 1 sec based on timer0
                for( ;count != 0; ){
                    
                    while(!StartStop) Mixer = OFF; //PB pressed, stop the mixer
                    Mixer = ON;
                    //Display counter on LCD
                    Bin2Asc(count,Digits);
                    DispVarStr(Digits, Ln2Ch11, 4);
                } 
                Mixer = OFF;
                DispRomStr(Ln1Ch0, (ROM *) "Next State:Drain");
                DispRomStr(Ln2Ch0, (ROM *) "Mix is: OFF     ");               
                
                Beep();         //Beep for 3 seconds
                State = Drain;  //Mix is finished, now we drain the tank
                break;

            case Drain: 
                if (StartStop) {
                    DrainValve = ON;
                    DispRomStr(Ln1Ch0, (ROM *) "State: Drain    ");
                    DispRomStr(Ln2Ch0, (ROM *) "Drain Valve: ON ");
                    
                    //When tank is empty, DAC is at min value, reset program
                    if(TankEmpty){                       
                        DrainValve = OFF;
                        DispRomStr(Ln1Ch0, (ROM *) "Program Ending!!");
                        DispRomStr(Ln2Ch0, (ROM *) "Drain Valve: OFF");
                        Beep();        //Beep for 3 seconds
                        Reset();
                    }
                }
                else {
                    //User stops process, close drain valve, change LCD output
                    DrainValve = OFF;
                    DispRomStr(Ln2Ch0, (ROM *) "Drain Valve: OFF");
                    
                }
                break;
        }   
    }

}

void setup(void) {
    OSCCON = 0b01010000;            // 4 MHz internal clk
    
   
    FillValve = DrainValve = OFF;   // valves are initially closed
    Mixer = OFF;                    // Mixer is initially turned off
    Buzzer = OFF;                   // Buzzer is initially muted
    
    TRISD &= 0b01000000;            // RD7, RD5 .. RD0 are outputs
    INTCON2 &= 0x0F;                // RBPU is asserted, INTx on -ve edge
    INTCON2bits.RBPU = 0;           // RBPU is asserted
    INTCON2bits.INTEDG0 = 0;        // INT0 reacts to a falling edge
           
    TRISA = 0x04;                   //output port RA2
    VREFCON1bits.DACEN = 1;         //DAC enable
    VREFCON1bits.DACOE = 1;         //enable output
    VREFCON1bits.DACPSS = 0b00;     //Vsrc+ = VDD
    VREFCON1bits.DACNSS = 0;        //Vsrc- = Vss
    VREFCON1bits.DACLPS = 1;        //enable Vsrc+ and Vscr-
    VREFCON2bits.DACR = 0b00000;    //set value to 0 V
    StartStop = 0;                  // Initially machine is off
    
    //For LCD
    InitLCD();                      //Initialize LCD
    DispRomStr(Ln1Ch0, (ROM *) "Start/Stop Valve");     //Display on LCD
    DispRomStr(Ln2Ch0, (ROM *) "Valve is: OFF   ");     //Display on LCD
    
    //For port C
    TRISC = 0;                      //RC7 .. RC0 are outputs
    ANSELC = 0;                     //RC7 .. RC0 are digital
    
    //For port B
    TRISB = 0X01;                   //RB7 .. RB4 are outputs
    
    //For timer 0 interrupt
    T0CON = 0b10010011;             //16 bit timer, enabled, prescale, /16
    TMR0H = (65536 - 62500) / 256;  // 62500 * 16 us = 1 s
    TMR0L = (65536 - 62500) % 256;
    
    INTCONbits.INT0IE = 1;          // enable local interrupt enable
    INTCONbits.TMR0IE = 1;          // enable local interrupt enable of timer
    INTCONbits.GIE = 1;             // enable global interrupt enable
}

#pragma code ISR = 0x0008
#pragma interrupt ISR

void ISR(void){	
    if(INTCONbits.INT0IF){
        INTCONbits.INT0IF = 0;          //Clear flag
        StartStop = !StartStop;
        return;
    }else if(INTCONbits.TMR0IF){
        TMR0H = (65536 - 62500) / 256;  // 62500 * 16 us = 1 s
        TMR0L = (65536 - 62500) % 256;  // 62500 * 16 us = 1 s
        INTCONbits.TMR0IF = 0;          //Clear flag
        
        
        
        if(State == Fill && StartStop){
            VREFCON2bits.DACR++;                                       //Increment output voltage
            percentage = ((unsigned int)VREFCON2bits.DACR * 1000) /31; //Compute Approx percentage
        }else if(StartStop && State == Mix){
            count--;                                                   //Decrement count every second
        }else if(StartStop && State == Drain){
            if(TankEmpty){
                count--;                                               //Decrement count every second
            }else{
                VREFCON2bits.DACR--;                                   //Decrement output voltage
            }            
            percentage = ((unsigned int)VREFCON2bits.DACR * 1000) /31; //Compute Approx percentage                                                                
        }
        
        
        
        
        
        
        
        if(State == Fill || State == Drain){
            //Print on SSD the Approx percentage
            Bin2BcdE(percentage ,Digits);            
            PORTC = Digits[2] << 4 | Digits[3];                        //RC 3..0 holds 2nd digit, RC 7..4 holds the 3rd digit            
            PORTB =  (Digits[1] << 4) | ((Digits[4] & 0x07 ) << 1);    //RB 7..4 holds the 1st digit, RB 3 .. 1 holds 3 least significant of 1st digit             
            PORTD ^= ( (Digits[4] << 4) & 0x80);                       //RD7 holds the most significant bit of the 1st digit
        }
        
        //Set tank full and empty sensors
        TankFull = (VREFCON2bits.DACR == 0b11111 );
        TankEmpty = (VREFCON2bits.DACR == 0b00000);
        return;
    }   
}	


void Beep(void) {
    Buzzer = ON;
    count = 3;
    while(count != 0); //buzzer stays on for 3 seconds
    Buzzer = OFF;
}


