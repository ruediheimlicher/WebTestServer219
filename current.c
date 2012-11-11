//***************************************************************************

// Im Hauptprogramm einfügen:
 
// current
volatile static uint32_t             currentcount=0;     // Anzahl steps von timer2 zwischen 2 Impulsen. Schritt 10uS
volatile uint32_t                     impulscount=0;     // Anzahl Impulse vom Stromzaehler fortlaufend


volatile static uint32_t            impulszeit=0;  // anzahl steps in INT1, wird nach div durch ANZAHLWERTE zu impulszeitsumme addiert.uebernommen
volatile static float               impulszeitsumme=0;
volatile static float               impulsmittelwert=0; // Mittelwertt der Impulszeiten in einem Messintervall

// Filter
volatile uint16_t                   filtercount = 0;        // counter fuer messwerte der Filterung
volatile static float               filtermittelwert = 0;   // gleitender Mittelwert
volatile uint8_t                    filterfaktor = 4;       // Filterlaenge


volatile uint8_t                    currentstatus=0; // Byte fuer Status der Strommessung
volatile uint8_t                    webstatus =0;     // Byte fuer Ablauf der Messung/Uebertragung

volatile uint8_t                    errstatus =0;     // Byte fuer errors der Messung/Uebertragung


volatile uint8_t                    sendWebCount=0;	// Zahler fuer Anzahl TWI-Events,
                                                      // nach denen Daten des Clients gesendet werden sollen


volatile uint8_t messungcounter;


volatile uint16_t                   lastint=0;
volatile uint16_t                   delta=2000;       // Bandbreite
volatile uint8_t                    biggercount=0;    // anzahl Werte die groesser waren
volatile uint8_t                    smallercount=0;


// Endwert fuer Compare-Match von Timer2
#define TIMER2_ENDWERT					 127; // 10 us

#define IMPULSBIT                   4 // gesetzt wenn Interrupt0 eintrifft. Nach Auswertung im Hauptprogramm wieder zurueckgesetzt

#define ANZAHLWERTE                 8 // Anzahl Werte fuer Mittelwertbildung

#define ANZAHLPAKETE                4 // Anzahl Pakete bis zur Uebertragung


#define OSZIPORT		PORTC
#define OSZIPORTDDR	DDRC
#define OSZIPORTPIN	PINC
#define PULS			1

#define OSZILO OSZIPORT &= ~(1<<PULS)
#define OSZIHI OSZIPORT |= (1<<PULS)
#define OSZITOGG OSZIPORT ^= (1<<PULS)

#define CURRENTSEND                 0     // Bit fuer: Daten an Server senden
#define CURRENTSTOP                 1     // Bit fuer: Impulse ignorieren
#define CURRENTWAIT                 2     // Bit fuer: Impulse wieder bearbeiten

#define	IMPULSPIN                  0    // Pin fuer Anzeige Impuls



// Fehlerbits
#define  CALLBACKERR    0

//volatile uint8_t timer2startwert=TIMER2_ENDWERT;

// SPI

//**************************************************************************
#include <avr/io.h>
#include "lcd.h"


/*
 TIMSK2=(1<<OCIE2A); // compare match on OCR2A
 TCNT2=0;  // init counter
 OCR2A=244; // value to compare against
 TCCR2A=(1<<WGM21); // do not change any output pin, clear at compare match

 */

// Timer2 fuer Atmega328

void timer2(void) // Takt fuer Strommessung
{ 
   //lcd_gotoxy(10,1);
	//lcd_puts("Tim2 ini\0");
   TCCR2A=0;
   PRR&=~(1<<PRTIM2); // write power reduction register to zero
  
   TIMSK2 |= (1<<OCIE2A);                 // CTC Interrupt Enable

   TCCR2A |= (1<<WGM21);                  // Toggle OC2A
    
   TCCR2A |= (1<<COM2A1);                  // CTC
   
   /*
    CS22	CS21	CS20	Description
    0    0     0     No clock source
    0    0     1     clk/1
    0    1     0     clk/8
    0    1     1     clk/32
    1    0     0     clk/64
    1    0     1     clk/128
    1    1     0     clk/256
    1    1     1     clk/1024
    */

   //TCCR2B |= (1<<CS22); // 
   //TCCR2B |= (1<<CS21);//							
	TCCR2B |= (1<<CS20);
   
	TIFR2 |= (1<<TOV2);							//Clear TOV0 Timer/Counter Overflow Flag. clear pending interrupts
	
   //TIMSK2 |= (1<<TOIE2);						//Overflow Interrupt aktivieren
   TCNT2 = 0;                             //Rücksetzen des Timers
	//OSZILO;
   OCR2A = TIMER2_ENDWERT;    
}



ISR(TIMER2_COMPA_vect) // CTC Timer2
{
   //OSZITOGG;
   currentcount++; // Zaehlimpuls
   //PORTB ^= (1<<0);
}



// ISR fuer Atmega328
/*
ISR (TIMER2_OVF_vect)
{	
	//OSZITOGG ;
//	TCCR2A=0;
	//lcd_clr_line(1);
   currentcount0 ++;
   if (currentcount0 == 0x0200)
   {
      lcd_gotoxy(0,1);
      lcd_puts("Tim2OVF \0");
      lcd_putint(currentcount);

      currentcount++;
      currentcount0=0;
   }

}
*/


ISR( INT1_vect )
{
   //lcd_gotoxy(0,1);
	//lcd_puts("I0:\0");
   //lcd_putint(impulscount);
   
   if (webstatus & (1<<CURRENTSTOP)) // Webevent im Gang, Impulse ignorieren
   {
      //lcd_puts("st\0");
     
      //return;
   }
   
   
   if (webstatus & (1<<CURRENTWAIT)) // Webevent fertig, neue Serie starten
   {
      //lcd_gotoxy(16,1);
      //lcd_puts("wt\0");
      
      webstatus &= ~(1<<CURRENTWAIT);
      
      
      TCCR2B |= (1<<CS20); // Timer wieder starten, Impuls ist Startimpuls, nicht auswerten
      
      currentcount = 0;
      filtercount = 0;
      return;
   }
   
   
   impulscount++; // Gesamtzahl der Impulse
   
   currentstatus |= (1<< IMPULSBIT); // Bit bearbeiten in WebServer
   
   {
      //OSZILO;
      impulszeit = currentcount;
      currentcount =0;
      
      //PORTB ^= (1<<IMPULSPIN);
   }
   
}	// ISR






void InitCurrent(void) 
{ 
    /*
	// interrupt on INT0 pin falling edge (sensor triggered) 
	EICRA = (1<<ISC01) | (0<<ISC00);
	// turn on interrupts!
	EIMSK  |= (1<<INT0);
*/
   // interrupt on INT1 pin falling edge 
	EICRA = (1<<ISC11) | (0<<ISC10);
	// turn on interrupts!
	EIMSK  |= (1<<INT1);


	lcd_gotoxy(0,0);
	lcd_puts("I1\0");
   
	sei(); // Enable global interrupts
   
} 


