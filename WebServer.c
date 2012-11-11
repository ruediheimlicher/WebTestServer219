/*********************************************
 * vim:sw=8:ts=8:si:et
 * To use the above modeline in vim you must have "set modeline" in your .vimrc
 * Author: Guido Socher
 * Copyright: GPL V2
 * See http://www.gnu.org/licenses/gpl.html
 *
 * Chip type           : Atmega88 or Atmega168 or Atmega328 with ENC28J60
 * Note: there is a version number in the text. Search for tuxgraphics
 *********************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "ip_arp_udp_tcp.c"
#include "websrv_help_functions.c"
#include "enc28j60.h"
//#include "timeout.h"
#include "net.h"

#include <avr/wdt.h>
#include "lcd.c"
#include "adc.c"
#include "current.c"

#include "datum.c"
#include "version.c"
#include "homedata.c"
#include "status.c"
//***********************************
//									*
//									*
//***********************************
//
//									*
//***********************************


//#define SDAPIN		4
//#define SCLPIN		5

/*
 #define TASTE1		38
 #define TASTE2		46
 #define TASTE3		54
 #define TASTE4		72
 #define TASTE5		95
 #define TASTE6		115
 #define TASTE7		155
 #define TASTE8		186
 #define TASTE9		205
 #define TASTEL		225
 #define TASTE0		235
 #define TASTER		245
 */

#define	LOOPLEDPORT		PORTB
#define	LOOPLEDPORTPIN	DDRB
#define	LOOPLED			1
#define	SENDLED			0



#define STARTDELAYBIT		0
#define WDTBIT             7
#define TASTATURPIN			0           //	Eingang fuer Tastatur
#define ECHOPIN            5           //	Ausgang fuer Impulsanzeige

#define MASTERCONTROLPIN	4           // Eingang fuer MasterControl: Meldung MasterReset

#define INT0PIN            2           // Pin Int0
#define INT1PIN            3           // Pin Int1



volatile uint16_t EventCounter=0;
static char baseurl[]="http://ruediheimlicher.dyndns.org/";



/* *************************************************************************  */
/* Eigene Deklarationen                                                       */
/* *************************************************************************  */

volatile uint16_t					timer2_counter=0;

//enum webtaskflag{IDLE, TWISTATUS,EEPROMREAD, EEPROMWRITE};

static uint8_t  webtaskflag =0;

static uint8_t monitoredhost[4] = {10,0,0,7};


static char CurrentDataString[64];

static char* teststring = "pw=Pong&strom=360\0";

//static char EEPROM_String[96];

//static  char d[4]={};
//static char* key1;
//static char *sstr;

//char HeizungVarString[64];

//static char AlarmDataString[64];

//static char ErrDataString[32];


volatile uint8_t oldErrCounter=0;



static volatile uint8_t stepcounter=0;

// Prototypes
void lcdinit(void);
void r_itoa16(int16_t zahl, char* string);
void tempbis99(uint16_t temperatur,char*tempbuffer);


// the password string (only the first 5 char checked), (only a-z,0-9,_ characters):
static char password[10]="ideur00"; // must not be longer than 9 char
static char resetpassword[10]="ideur!00!"; // must not be longer than 9 char


uint8_t TastenStatus=0;
uint16_t Tastencount=0;
uint16_t Tastenprellen=0x01F;


//static volatile uint8_t pingnow=1; // 1 means time has run out send a ping now
//static volatile uint8_t resetnow=0;
//static volatile uint8_t reinitmac=0;
//static uint8_t sendping=1; // 1 we send ping (and receive ping), 0 we receive ping only
static volatile uint8_t pingtimer=1; // > 0 means wd running
//static uint8_t pinginterval=30; // after how many seconds to send or receive a ping (value range: 2 - 250)
static char *errmsg; // error text

unsigned char TWI_Transceiver_Busy( void );
//static volatile uint8_t twibuffer[twi_buffer_size+1]; // Buffer fuer Data aus/fuer EEPROM
static volatile char twiadresse[4]; // EEPROM-Adresse
//static volatile uint8_t hbyte[4];
//static volatile uint8_t lbyte[4];
extern volatile uint8_t twiflag; 
static uint8_t aktuelleDatenbreite=8;
static volatile uint8_t send_cmd=0;


#define TIMERIMPULSDAUER            10    // us

#define CURRENTSEND                 0     // Daten an server senden

volatile uint16_t                   wattstunden=0;
volatile uint16_t                   kilowattstunden=0;

volatile float leistung =0;
float lastleistung =0;
uint8_t lastcounter=0;
volatile uint8_t  anzeigewert =0;

static char stromstring[10];



void timer0(void);
uint8_t WochentagLesen(unsigned char ADRESSE, uint8_t hByte, uint8_t lByte, uint8_t *Daten);
uint8_t SlavedatenLesen(const unsigned char ADRESSE, uint8_t *Daten);
void lcd_put_tempAbMinus20(uint16_t temperatur);

/* ************************************************************************ */
/* Ende Eigene Deklarationen																 */
/* ************************************************************************ */


// Note: This software implements a web server and a web browser.
// The web server is at "myip" and the browser tries to access "websrvip".
//
// Please modify the following lines. mac and ip have to be unique
// in your local area network. You can not have the same numbers in
// two devices:
//static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x29};

//RH4702 52 48 34 37 30 33
static uint8_t mymac[6] = {0x52,0x48,0x34,0x37,0x30,0x33};
//static uint8_t mymac[6] = {0x52,0x48,0x34,0x37,0x30,0x99};

// how did I get the mac addr? Translate the first 3 numbers into ascii is: TUX
// This web server's own IP.
//static uint8_t myip[4] = {10,0,0,29};
//static uint8_t myip[4] = {192,168,255,100};

// IP des Webservers 
static uint8_t myip[4] = {192,168,1,215};
//static uint8_t myip[4] = {192,168,1,219};

// IP address of the web server to contact (IP of the first portion of the URL):
//static uint8_t websrvip[4] = {77,37,2,152};


// ruediheimlicher
static uint8_t websrvip[4] = {193,17,85,42}; // ruediheimlicher 193.17.85.42

// The name of the virtual host which you want to contact at websrvip (hostname of the first portion of the URL):


#define WEBSERVER_VHOST "www.ruediheimlicher.ch"


// Default gateway. The ip address of your DSL router. It can be set to the same as
// websrvip the case where there is no default GW to access the 
// web server (=web server is on the same lan as this host) 

// ************************************************
// IP der Basisstation !!!!!
// Runde Basisstation : 
//static uint8_t gwip[4] = {192,168,1,5};// Rueti

// Viereckige Basisstation:
static uint8_t gwip[4] = {192,168,1,1};// Rueti

// ************************************************

static char urlvarstr[21];
// listen port for tcp/www:
#define MYWWWPORT 82
//

#define BUFFER_SIZE 800


static uint8_t buf[BUFFER_SIZE+1];
static uint8_t pingsrcip[4];
static uint8_t start_web_client=0;
static uint8_t web_client_sendok=0; // Anzahl callbackaufrufe

 static volatile uint8_t callbackerrcounter=0;

#define CURRENTSEND                 0     // Bit fuer: Daten an Server senden
#define CURRENTSTOP                 1     // Bit fuer: Impulse ignorieren
#define CURRENTWAIT                 2     // Bit fuer: Impulse wieder bearbeiten
#define CURRENTMESSUNG              3     // Bit fuer: Messen
#define DATASEND                    4     // Bit fuer: Daten enden
#define DATAOK                      5     // Data kan gesendet weren
#define DATAPEND                    6     // Senden ist pendent
#define DATALOOP                    7     // wird von loopcount1 gesetzt, startet senden


void str_cpy(char *ziel,char *quelle)
{
	uint8_t lz=strlen(ziel); //startpos fuer cat
	//printf("Quelle: %s Ziellaenge: %d\n",quelle,lz);
	uint8_t lq=strlen(quelle);
	//printf("Quelle: %s Quelllaenge: %d\n",quelle,lq);
	uint8_t i;
	for(i=0;i<lq;i++)
	{
		//printf("i: %d quelle[i]: %c\n",i,quelle[i]);
		ziel[i]=quelle[i];
	}
	lz=strlen(ziel);
	ziel[lz]='\0';
}

void str_cat(char *ziel,char *quelle)
{
	uint8_t lz=strlen(ziel); //startpos fuer cat
	//printf("Quelle: %s Ziellaenge: %d\n",quelle,lz);
	uint8_t lq=strlen(quelle);
	//printf("Quelle: %s Quelllaenge: %d\n",quelle,lq);
	uint8_t i;
	for(i=0;i<lq;i++)
	{
		//printf("i: %d quelle[i]: %c\n",i,quelle[i]);
		ziel[lz+i]=quelle[i];
		
	}
	//printf("ziel: %s\n",ziel);
	lz=strlen(ziel);
	ziel[lz]='\0';
}


// http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
char *trimwhitespace(char *str)
{
   char *end;
   
   // Trim leading space
   while(isspace(*str)) str++;
   
   if(*str == 0)  // All spaces?
      return str;
   
   // Trim trailing space
   end = str + strlen(str) - 1;
   while(end > str && isspace(*end)) end--;
   
   // Write new null terminator
   *(end+1) = 0;
   
   return str;
}



void timer0() // Analoganzeige
{
	//----------------------------------------------------
	// Set up timer 0 
	//----------------------------------------------------
   /*
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS00) | _BV(CS02);
	OCR0A = 0x2;
	TIMSK0 = _BV(OCIE0A);
    */
   
   DDRD |= (1<< PD6);   // OC0A Output

   TCCR0A |= (1<<WGM00);   // fast PWM  top = 0xff
   TCCR0A |= (1<<WGM01);   // PWM
   //TCCR0A |= (1<<WGM02);   // PWM
   
   TCCR0A |= (1<<COM0A1);   // set OC0A at bottom, clear OC0A on compare match
   TCCR0B |= 1<<CS02;
   TCCR0B |= 1<<CS00;
   
   OCR0A=0;
   TIMSK0 |= (1<<OCIE0A);
   
   
}

ISR(TIMER0_COMPA_vect)
{
   
  OCR0A = anzeigewert;
   //OCR0A++;
    
}

uint16_t http200ok(void)
{
	return(fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n")));
}

// we were ping-ed by somebody, store the ip of the ping sender
// and trigger an upload to http://tuxgraphics.org/cgi-bin/upld
// This is just for testing and demonstration purpose
void ping_callback(uint8_t *ip)
{
    uint8_t i=0;
    // trigger only first time in case we get many ping in a row:
    if (start_web_client==0)
    {
        start_web_client=1;
        //			lcd_gotoxy(12,0);
        //			lcd_puts("ping\0");
        // save IP from where the ping came:
       /*
        while(i<4)
        {
            pingsrcip[i]=ip[i];
            i++;
        }
        */
    }
}

void strom_browserresult_callback(uint8_t statuscode,uint16_t datapos)
{
   // datapos is not used in this example
   if (statuscode==0)
   {
      /*
         lcd_gotoxy(12,0);
         lcd_puts("        \0");
         lcd_gotoxy(12,0);
         lcd_puts("s cb OK\0");
       */
     // lcd_gotoxy(19,0);
     // lcd_putc(' ');
      lcd_gotoxy(19,0);
      lcd_putc('+');
      
      webstatus &= ~(1<<DATASEND);
      
      webstatus &= ~(1<<DATAPEND);
      
      callbackerrcounter=0; // callbackerrcounter reset
      // Messungen wieder starten
      
      webstatus &= ~(1<<CURRENTSTOP);
      webstatus |= (1<<CURRENTWAIT); // Beim naechsten Impuls Messungen wieder starten
      sei();
      
      web_client_sendok++;
      
      
   }
   else
   {
      /*
         lcd_gotoxy(0,1);
         lcd_puts("        \0");
         lcd_gotoxy(0,1);
         lcd_puts("s cb err\0");
         lcd_puthex(statuscode);
       */
      lcd_gotoxy(19,0);
      lcd_putc(' ');
      lcd_gotoxy(19,0);
      lcd_putc('-');
      
   }
}

void home_browserresult_callback(uint8_t statuscode,uint16_t datapos)
{
    // datapos is not used in this example
    if (statuscode==0)
    {
        
        lcd_gotoxy(0,0);
        lcd_puts("        \0");
        lcd_gotoxy(0,0);
        lcd_puts("h cb OK\0");
        
        web_client_sendok++;
        //				sei();
        
    }
    else
    {
        lcd_gotoxy(0,0);
        lcd_puts("        \0");
        lcd_gotoxy(0,0);
        lcd_puts("h cb err\0");
        lcd_puthex(statuscode);
        
    }
}



/* ************************************************************************ */
/* Eigene Funktionen														*/
/* ************************************************************************ */

uint8_t verify_password(char *str)
{
	// the first characters of the received string are
	// a simple password/cookie:
	if (strncmp(password,str,7)==0)
   {
		return(1);                 // PW OK
	}
	return(0);                    //PW falsch
}


uint8_t verify_reset_password(char *str)
{
	// the first characters of the received string are
	// a simple password/cookie:
	if (strncmp(resetpassword,str,7)==0)
   {
		return(1); // Reset-PW OK
	}
	return(0); //Reset-PW falsch
}


void delay_ms(unsigned int ms)/* delay for a minimum of <ms> */
{
	// we use a calibrated macro. This is more
	// accurate and not so much compiler dependent
	// as self made code.
	while(ms){
		_delay_ms(0.96);
		ms--;
	}
}

uint8_t Hex2Int(char *s) 
{ 
    long res; 
    char *Chars = "0123456789ABCDEF", *p; 
    
    if (strlen(s) > 8) 
    /* Error ... */ ; 
    
    for (res = 0L; *s; s++) { 
        if ((p = strchr(Chars, toupper(*s))) == NULL) 
        /* Error ... */ ; 
        res = (res << 4) + (p-Chars); 
    } 
    
    return res; 
} 

void tempbis99(uint16_t temperatur,char*tempbuffer)
{
	char buffer[8]={};
	//uint16_t temp=(temperatur-127)*5;
	uint16_t temp=temperatur*5;
	
	//itoa(temp, buffer,10);
	
	r_itoa16(temp,buffer);
	
	//lcd_puts(buffer);
	//lcd_putc('*');
	
	//char outstring[7]={};
	
	tempbuffer[6]='\0';
	tempbuffer[5]=' ';
	tempbuffer[4]=buffer[6];
	tempbuffer[3]='.';
	tempbuffer[2]=buffer[5];
	if (abs(temp)<100)
	{
		tempbuffer[1]=' ';
		
	}
	else 
	{
		tempbuffer[1]=buffer[4];
		
	}		
	tempbuffer[0]=buffer[0];
	
	
}

void tempAbMinus20(uint16_t temperatur,char*tempbuffer)
{
    
    char buffer[8]={};
    int16_t temp=(temperatur)*5;
    temp -=200;
    char Vorzeichen=' ';
    if (temp < 0)
    {
        Vorzeichen='-';
    }
    
    r_itoa16(temp,buffer);
    //		lcd_puts(buffer);
    //		lcd_putc(' * ');
    
    //		char outstring[7]={};
    
    tempbuffer[6]='\0';
    //outstring[5]=0xDF; // Grad-Zeichen
    tempbuffer[5]=' ';
    tempbuffer[4]=buffer[6];
    tempbuffer[3]='.';
    tempbuffer[2]=buffer[5];
    if (abs(temp)<100)
    {
		tempbuffer[1]=Vorzeichen;
		tempbuffer[0]=' ';
    }
    else
    {
		tempbuffer[1]=buffer[4];
		tempbuffer[0]=Vorzeichen;
    }
    //		lcd_puts(outstring);
}


// search for a string of the form key=value in
// a string that looks like q?xyz=abc&uvw=defgh HTTP/1.1\r\n
//
// The returned value is stored in the global var strbuf_A

// Andere Version in Webserver help funktions
/*
 uint8_t find_key_val(char *str,char *key)
 {
 uint8_t found=0;
 uint8_t i=0;
 char *kp;
 kp=key;
 while(*str &&  *str!=' ' && found==0){
 if (*str == *kp)
 {
 kp++;
 if (*kp == '\0')
 {
 str++;
 kp=key;
 if (*str == '=')
 {
 found=1;
 }
 }
 }else
 {
 kp=key;
 }
 str++;
 }
 if (found==1){
 // copy the value to a buffer and terminate it with '\0'
 while(*str &&  *str!=' ' && *str!='&' && i<STR_BUFFER_SIZE)
 {
 strbuf_A[i]=*str;
 i++;
 str++;
 }
 strbuf_A[i]='\0';
 }
 // return the length of the value
 return(i);
 }
 */


#pragma mark analye_get_url

// takes a string of the form ack?pw=xxx&rst=1 and analyse it
// return values:  0 error
//                 1 resetpage and password OK
//                 4 stop wd
//                 5 start wd
//                 2 /mod page
//                 3 /now page 
//                 6 /cnf page 

uint8_t analyse_get_url(char *str)	// codesnippet von Watchdog
{
	char actionbuf[32];
	errmsg="inv.pw";
	
    
	webtaskflag =0; //webtaskflag zuruecksetzen. webtaskflag bestimmt aktion, die ausgefuehrt werden soll. Wird an Master weitergegeben.
	// ack 
	
	// str: ../ack?pw=ideur00&rst=1
	if (strncmp("ack",str,3)==0)
	{
		lcd_clr_line(1);
		lcd_gotoxy(0,1);
		lcd_puts("ack\0");
        
		// Wert des Passwortes eruieren
		if (find_key_val(str,actionbuf,10,"pw"))
		{
			urldecode(actionbuf);
			
			// Reset-PW?
			if (verify_reset_password(actionbuf))
			{
				return 15;
			}
            
			// Passwort kontrollieren
			if (verify_password(actionbuf))
			{
				if (find_key_val(str,actionbuf,10,"tst"))
				{
					return(16);
				}
			}
		}
        
		return(0);
		
	}//ack
	
	if (strncmp("twi",str,3)==0)										//	Daten von HC beginnen mit "twi"	
	{
		//lcd_clr_line(1);
		//lcd_gotoxy(17,0);
		//lcd_puts("twi\0");
		
		// Wert des Passwortes eruieren
		if (find_key_val(str,actionbuf,10,"pw"))					//	Passwort kommt an zweiter Stelle
		{		
			urldecode(actionbuf);
			webtaskflag=0;
			//lcd_puts(actionbuf);
			// Passwort kontrollieren
			
			
			if (verify_password(actionbuf))							// Passwort ist OK
			{
				//OSZILO;
				if (find_key_val(str,actionbuf,10,"status"))		// Status soll umgeschaltet werden
				{
               return 1;
				}//st
				
				
				
				
				
				// Daten fuer EEPROM von Homeserver empfangen
				
				if (find_key_val(str,actionbuf,10,"wadr"))			// EEPROM-Daten werden von Homeserver gesendet
				{
						return (9);												// Empfang bestätigen
				} // wadr
				
				if (find_key_val(str,actionbuf,10,"iswriteok"))		// Anfrage ob writeok
				{
                    
					return (7);
				}
                
				if (find_key_val(str,actionbuf,12,"isstat0ok"))		// Anfrage ob statusok isstat0ok
				{
					return (10);
				}
				
                if (find_key_val(str,actionbuf,10,"reset")) // HomeCentral reseten
                {
                    
                }	
				
			}//verify pw
		}//find_key pw
		return(0);
	}//twi
    
	
    
	errmsg="inv.url";
	return(0);
}





uint16_t print_webpage_ok(uint8_t *buf,uint8_t *okcode)
{
	// Schickt den okcode als Bestaetigung fuer den Empfang des Requests
	uint16_t plen;
	plen=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n"));
	plen=fill_tcp_data_p(buf,plen,PSTR("<p>okcode="));
	plen=fill_tcp_data(buf,plen,(void*)okcode);	
	return plen;
}




#pragma mark Webpage_status

// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_webpage_status(uint8_t *buf)
{
	
	uint16_t plen=0;
	//char vstr[5];
	plen=http200ok();
	
	plen=fill_tcp_data_p(buf,plen,PSTR("<h1>HomeCurrent</h1>"));
	//
	
	//
	plen=fill_tcp_data_p(buf,plen,PSTR("<p>  HomeCurrent<br>  Falkenstrasse 20<br>  8630 Rueti"));
	plen=fill_tcp_data_p(buf,plen,PSTR("<hr><h4><font color=\"#00FF00\">Status</h4></font></p>"));
	
	
	//return(plen);
	
	
	plen=fill_tcp_data_p(buf,plen,PSTR("<p>Leistung: "));
	
	//Temperatur=WebRxDaten[2];
	//Temperatur=inbuffer[2]; // Vorlauf
	//Temperatur=Vorlauf;
	//tempbis99(Temperatur,TemperaturString);
	
	//r_itoa(Temperatur,TemperaturStringV);
	
   
   plen=fill_tcp_data(buf,plen,stromstring);
   plen=fill_tcp_data_p(buf,plen,PSTR(" Watt</p>"));

	   
   return(plen);
   
	// Taste und Eingabe fuer Passwort
	plen=fill_tcp_data_p(buf,plen,PSTR("<form action=/ack method=get>"));
	plen=fill_tcp_data_p(buf,plen,PSTR("<p>\nPasswort: <input type=password size=10 name=pw ><input type=hidden name=tst value=1>  <input type=submit value=\"Bearbeiten\"></p></form>"));
	
	plen=fill_tcp_data_p(buf,plen,PSTR("<p><hr>"));
	plen=fill_tcp_data(buf,plen,DATUM);
	plen=fill_tcp_data_p(buf,plen,PSTR("  Ruedi Heimlicher"));
	plen=fill_tcp_data_p(buf,plen,PSTR("<br>Version :"));
	plen=fill_tcp_data(buf,plen,VERSION);
	plen=fill_tcp_data_p(buf,plen,PSTR("\n<hr></p>"));
	
	//
	
	/*
	 // Tux
	 plen=fill_tcp_data_p(buf,plen,PSTR("<h2>web client status</h2>\n<pre>\n"));
	 
	 char teststring[24];
	 strcpy(teststring,"Data");
	 strcat(teststring,"-\0");
	 strcat(teststring,"Uploads \0");
	 strcat(teststring,"mit \0");
	 strcat(teststring,"ping : \0");
	 plen=fill_tcp_data(buf,plen,teststring);
	 
	 plen=fill_tcp_data_p(buf,plen,PSTR("Data-Uploads mit ping: "));
	 // convert number to string:
	 itoa(web_client_attempts,vstr,10);
	 plen=fill_tcp_data(buf,plen,vstr);
	 plen=fill_tcp_data_p(buf,plen,PSTR("\nData-Uploads aufs Web: "));
	 // convert number to string:
	 itoa(web_client_sendok,vstr,10);
	 plen=fill_tcp_data(buf,plen,vstr);
	 plen=fill_tcp_data_p(buf,plen,PSTR("\n</pre><br><hr>"));
	 */
	return(plen);
}

void master_init(void)
{
	
	DDRB |= (1<<PORTB1);	//Bit 1 von PORT B als Ausgang für Kontroll-LED
	PORTB |= (1<<PORTB1);	//Pull-up
	DDRB |= (1<<PORTB0);	//Bit 1 von PORT B als Ausgang für Kontroll-LED
	PORTB |= (1<<PORTB0);	//Pull-up
	
	DDRD |=(1<<ECHOPIN); //Pin 5 von Port D als Ausgang fuer Impuls-Echo
	PORTD &= ~(1<<ECHOPIN); //LO
	// Eventuell: PORTD5 verwenden, Relais auf Platine 
	
//  DDRD &=~(1<<INT0PIN); //Pin 2 von Port D als Eingang fuer Interrupt Impuls
//	PORTD |=(1<<INT0PIN); //HI
//   DDRD &=~(1<<INT1PIN); //Pin 3 von Port D als Eingang fuer Interrupt Impuls
//	PORTD |=(1<<INT1PIN); //HI

    
	DDRD &= ~(1<<MASTERCONTROLPIN); // Pin 4 von PORT D als Eingang fuer MasterControl
	PORTD |= (1<<MASTERCONTROLPIN);	// HI
	
   
   DDRD |= (1<<PORTB3);
	
}	

void initOSZI(void)
{
	OSZIPORTDDR |= (1<<PULS);
	OSZIPORT |= (1<<PULS); // HI
}

void lcdinit()
{
	//*********************************************************************
	//	Definitionen LCD im Hauptprogramm
	//	Definitionen in lcd.h
	//*********************************************************************
	
	LCD_DDR |= (1<<LCD_RSDS_PIN); //PIN 3 von PORT D als Ausgang fuer LCD_RSDS_PIN
	LCD_DDR |= (1<<LCD_ENABLE_PIN); //PIN 4 von PORT D als Ausgang fuer LCD_ENABLE_PIN
	LCD_DDR |= (1<<LCD_CLOCK_PIN); //PIN 5 von PORT D als Ausgang fuer LCD_CLOCK_PIN
	/* initialize the LCD */
	lcd_initialize(LCD_FUNCTION_8x2, LCD_CMD_ENTRY_INC, LCD_CMD_ON);
	lcd_puts("LCD Init\0");
	delay_ms(300);
	lcd_cls();
	
}






void WDT_off(void)
{
    cli();
    wdt_reset();
    /* Clear WDRF in MCUSR */
    MCUSR &= ~(1<<WDRF);
    /* Write logical one to WDCE and WDE */
    /* Keep old prescaler setting to prevent unintentional time-out
     */
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    /* Turn off WDT */
    WDTCSR = 0x00;
    sei();
}

uint8_t i=0;



/* ************************************************************************ */
/* Ende Eigene Funktionen														*/
/* ************************************************************************ */


#pragma mark main
int main(void)
{
	
	/* ************************************************************************ */
	/* Eigene Main														*/
	/* ************************************************************************ */
	//JTAG deaktivieren (datasheet 231)
   //	MCUCSR |=(1<<7);
   //	MCUCSR |=(1<<7);
   
	MCUSR = 0;
	wdt_disable();
	//SLAVE
	//uint16_t Tastenprellen=0x0fff;
	uint16_t loopcount0=0;
	uint16_t loopcount1=0;

   // ETH
	//uint16_t plen;
	uint8_t i=0;
	int8_t cmd;
	
	
	// set the clock speed to "no pre-scaler" (8MHz with internal osc or
	// full external speed)
	// set the clock prescaler. First write CLKPCE to enable setting of clock the
	// next four instructions.
	CLKPR=(1<<CLKPCE); // change enable
	CLKPR=0; // "no pre-scaler"
	delay_ms(1);
	
	i=1;
	//	WDT_off();
	//init the ethernet/ip layer:
   //	init_ip_arp_udp_tcp(mymac,myip,MYWWWPORT);
	// timer_init();
	
	//     sei(); // interrupt enable
	master_init();
	
	lcdinit();
	lcd_puts("Guten Tag \0");
	lcd_gotoxy(10,0);
	lcd_puts("V:\0");
	lcd_puts(VERSION);
   /*
   char versionnummer[7];
   strcpy(versionnummer,&VERSION[13]);
   versionnummer[6] = '\0';
   lcd_puts(versionnummer);
    */
	delay_ms(1600);
	//lcd_cls();
	

	TWBR =0;
   
   
	
	//Init_SPI_Master();
	initOSZI();
	/* ************************************************************************ */
	/* Ende Eigene Main														*/
	/* ************************************************************************ */
   
	uint16_t dat_p;
	char str[30];
	
	// set the clock speed to "no pre-scaler" (8MHz with internal osc or
	// full external speed)
	// set the clock prescaler. First write CLKPCE to enable setting of clock the
	// next four instructions.
	CLKPR=(1<<CLKPCE); // change enable
	CLKPR=0; // "no pre-scaler"
	_delay_loop_1(0); // 60us
	
   if (TESTSERVER)
   {
      lcd_gotoxy(0,1);
      lcd_puts("TESTSERVER\0");
      myip[3] = TESTIP;
      mymac[5] = TESTIP;
   }

   
	/*initialize enc28j60*/
	enc28j60Init(mymac);
	enc28j60clkout(2); // change clkout from 6.25MHz to 12.5MHz
	_delay_loop_1(0); // 60us
	
	sei();
	
	/* Magjack leds configuration, see enc28j60 datasheet, page 11 */
	// LEDB=yellow LEDA=green
	//
	// 0x476 is PHLCON LEDA=links status, LEDB=receive/transmit
	// enc28j60PhyWrite(PHLCON,0b0000 0100 0111 01 10);
	enc28j60PhyWrite(PHLCON,0x476);
	
	//DDRB|= (1<<DDB1); // LED, enable PB1, LED as output
	//PORTD &=~(1<<PD0);;
	
	//init the web server ethernet/ip layer:
	init_ip_arp_udp_tcp(mymac,myip,MYWWWPORT);
   
	// init the web client:
	client_set_gwip(gwip);  // e.g internal IP of dsl router
	client_set_wwwip(websrvip);
	register_ping_rec_callback(&ping_callback);
	
    
	timer0();
   
   //
   lcd_clr_line(1);
   lcd_gotoxy(0,0);
   lcd_puts("          \0");
   //OSZIHI;
   
   InitCurrent();
   timer2();
   
   static volatile uint16_t paketcounter=0;
   
  
  
   
#pragma  mark "while"
	
//   webstatus |= (1<<DATASEND);
   sei();
   lcd_clr_line(0);
   /*
   lcd_gotoxy(0,0);
   lcd_putint(mymac[5]);
   lcd_putc(' ');
   lcd_putint(myip[3]);
	delay_ms(1600);
   lcd_clr_line(0);
   */
   while(1)
	{
		sei();
		//Blinkanzeige
		loopcount0++;
		if (loopcount0>=0x4FFF)
		{
			loopcount0=0;
         
         if (webstatus & (1<<DATAPEND)&& loopcount1 > 6) // callback simulieren
         {
            callbackerrcounter++;
            
            webstatus &= ~(1<<DATASEND);
            
            webstatus &= ~(1<<DATAPEND);
            
            // Messungen wieder starten
            
            webstatus &= ~(1<<CURRENTSTOP);
            webstatus |= (1<<CURRENTWAIT); // Beim naechsten Impuls Messungen wieder starten
            sei();
            
            errstatus |= (1<<CALLBACKERR);
            
            //loopcount1=0;
         }
         
         
			
			if (loopcount1 >= 0x1FF)
			{
				
				loopcount1 = 0;
				//OSZITOGG;
				LOOPLEDPORT ^= (1<<SENDLED);           // TWILED setzen, Warnung
				webstatus |= (1<<DATALOOP);
            //TWBR=0;
				//lcdinit();
            //lcd_gotoxy(16,1);
            //lcd_putc('l');

			}
			else
			{
            if (loopcount1 == 20)
            {
               //lcd_gotoxy(16,1);
               //lcd_putc(' ');
            }
				loopcount1++;
				
			}
			LOOPLEDPORT ^=(1<<LOOPLED);
		}
		
		//**	Beginn Start-Routinen Webserver	***********************
		
		
		//**	Ende Start-Routinen	***********************
		
		
		//**	Beginn Current-Routinen	***********************
		
      if (currentstatus & (1<<IMPULSBIT)) // neuer Impuls angekommen
      {
         PORTD |=(1<<ECHOPIN);
         /*
         // Versuche, erste Messung zu verbessern
         if (messungcounter == 0)
         {
            //impulsmittelwert=0; // nutzlos
            //impulszeitsumme=0; // nutzlos
            // currentstatus &= 0xF0; // Bit 0-3 reset hier nutzlos
            //paketcounter =0; // nutzlos
            
         }
         else
         {
         }
         */

         //if((x & 1) == 0)
         
         /*
         if (webstatus & (1<<DATAPEND))
         {
            lcd_gotoxy(19,1);
            lcd_putc('p');
           
         }
          else
          {
             lcd_gotoxy(19,1);
             lcd_putc('-');
             
          }
          */
         /*
         
         if ((messungcounter & 1)==0)
         {
            lcd_gotoxy(1,1);
            lcd_putc(':');
            
         }
         else
         {
            lcd_gotoxy(1,1);
            lcd_putc(' ');
            
         }
          */
         //lcd_putint2(impulszeit/1000);
         //lcd_putint(impulszeit);
         
         messungcounter ++;
         currentstatus++; // ein Wert mehr gemessen
         impulszeitsumme += impulszeit/ANZAHLWERTE;     // Wert aufsummieren
  
         // Filter
         
         if (filtercount == 0) // neues Paket
         {
            filtermittelwert = impulszeit;
         }
         else
         {
            if (filtercount < filterfaktor)
            {
               filtermittelwert = ((filtercount-1)* filtermittelwert + impulszeit)/filtercount;
               
               //lcd_gotoxy(19,1);
               //lcd_putc('a');
               
            }
            else
            {
               filtermittelwert = ((filterfaktor-1)* filtermittelwert + impulszeit)/filterfaktor;
               //lcd_gotoxy(19,1);
               //lcd_putc('f');
               
            }
            
         }
         
         
         if ((int)filtermittelwert > (lastint + delta))
         {
            smallercount = 0;
            biggercount++;
         }
         else if ((int)filtermittelwert < (lastint - delta))
         {
            biggercount = 0;
            smallercount++;
         }
         else
         {
            biggercount = 0;
            smallercount = 0;
            lastint = (int)filtermittelwert; // speichern fuer naechsten Impuls
         }
         
         char filterstromstring[8];
         filtercount++;
         /*
          lcd_gotoxy(0,0);
          lcd_putint16(impulszeit/100);
          lcd_gotoxy(10,0);
          lcd_putint16(filtermittelwert/100);
          lcd_gotoxy(10,1);
          lcd_putint(filtercount);
          lcd_putc('*');
          
          dtostrf(filtermittelwert,5,1,filterstromstring);
          //lcd_puts(filterstromstring);
          */
         
         if (filtercount & (filterfaktor == 0)) // Wert anzeigen
         {
            //lcd_gotoxy(10,0);
            //lcd_putint16(filtermittelwert);
            //lcd_putc('*');
            //dtostrf(filtermittelwert,5,1,filterstromstring);
            //lcd_puts(filterstromstring);
         }
         

         
         // End Filter
        
         
         
         
         //lcd_gotoxy(10,1);
         //lcd_putint(currentstatus);
         //lcd_gotoxy(4,1);
         //lcd_putc('*');
         //lcd_putint(currentstatus & 0x0F);
         //lcd_gotoxy(19,0);
         //lcd_putc('x');
         PORTD &= ~(1<<ECHOPIN);
         
         if ((currentstatus & 0x0F) == ANZAHLWERTE)   // genuegend Werte
         {
            lcd_gotoxy(19,0);
            lcd_putc(' ');
            //lcd_putc(' ');
            //lcd_gotoxy(6,1);
            //lcd_putc(' ');

            //lcd_gotoxy(16,1);
            //lcd_puts("  \0");
            //lcd_gotoxy(0,1);
            //lcd_puts("    \0");

            //lcd_gotoxy(0,1);
            //lcd_putint(messungcounter);

            paketcounter++;
            //lcd_gotoxy(0,0);
            //lcd_puts("  \0");
            
            if ((paketcounter & 1)==0)
            {
               lcd_gotoxy(1,1);
               lcd_putc(':');
               
            }
            else
            {
               lcd_gotoxy(1,1);
               lcd_putc(' ');
               
            }

            //lcd_gotoxy(0,0);
            //lcd_putint2(paketcounter);

            
            impulsmittelwert = impulszeitsumme;
            impulszeitsumme = 0;
            
            currentstatus &= 0xF0; // Bit 0-3 reset
            
            
            //uint8_t lb= impulsmittelwert & 0xFF;
            //uint8_t hb = (impulsmittelwert>>8) & 0xFF;
            
            //                lcd_gotoxy(0,1);
            //               lcd_putc('I');
            //lcd_puts("INT0 \0");
            
            //lcd_puthex(hb);
            //lcd_puthex(lb);
            //                lcd_putc(':');
            
            //char impstring[12];
            //dtostrf(impulsmittelwert,8,2,impstring);
            //lcd_gotoxy(0,0);
            //lcd_puts(impstring);
            //lcd_putc(':');
            //lcd_putint16(impulsmittelwert);
            
            /*
             Impulsdauer: impulsmittelwert * TIMERIMPULSDAUER (10us)
             Umrechnung auf ms: /1000
             Energie pro Zählerimpuls: 360 Ws
             Leistung: (Energie pro Zählerimpuls)/Impulsabstand
             Umrechnung auf Sekunden: *1000
             Faktor: *100000
             */
            
            //     leistung = 0xFFFF/impulsmittelwert;
//            leistung = 360.0/impulsmittelwert*100000.0;
            //       leistung = 36000000.0/impulsmittelwert;
            //     Stromzaehler
            
            // neuer Mittelwert: filtermittelwert
            leistung = 360.0/filtermittelwert*100000.0;

            //anzeigewert = 0xFF/0x8000*leistung; // 0x8000/0x255 = 0x81
            
            //anzeigewert = leistung/0x81;
            anzeigewert = leistung/0x40; // fuer PWM Instrumentausgang
          
 
            wattstunden = impulscount/10;
            
            /*
            lcd_gotoxy(9,1);
            lcd_putint(wattstunden/1000);
            lcd_putc('.');
            lcd_putint3(wattstunden);
            lcd_putc('W');
            lcd_putc('h');
            */
            
            // Leistung in String umwandeln fuer Anzeige und pl
             dtostrf(leistung,5,0,stromstring);
            
               lcd_gotoxy(0,1);
               lcd_putc('L');
               //lcd_putc(':');
               
               
            if (!(paketcounter == 1))
            {
               //lcd_puts("     \0");
               lcd_gotoxy(2,1);
               lcd_puts(stromstring);
               lcd_putc(' ');
               lcd_putc('W');
            }
             //lcd_putc('*');
             //lcd_putc(' ');
             //lcd_putint16(leistung);
             //lcd_putc(' ');
             
            /*
             if (abs(leistung-lastleistung) > 10)
             {
             lastcounter++;
             
             if (lastcounter>3)
             {
             char diff[10];
             dtostrf(leistung-lastleistung,7,2,diff);
             lcd_gotoxy(10,1);
             lcd_putc('D');
             lcd_putc(':');
             lcd_puts(diff);
             lastleistung = leistung;
             }
             }
             else
             {
             lastcounter=0;
             }
             */
           
            //if (paketcounter  >= ANZAHLPAKETE)
            if (webstatus & (1<<DATALOOP))
            {
               webstatus &= ~(1<<DATALOOP);
               
               paketcounter=0;
               
               if (TEST)
               {
                  //lcd_gotoxy(0,1);
                  //lcd_putint(messungcounter);
                  //lcd_putc(' ');
                  
                  //lcd_gotoxy(9,1);
                  //lcd_putint(wattstunden/1000);
                  //lcd_putc('.');
                  //lcd_putint3(wattstunden);
                  //lcd_putc('W');
                  //lcd_putc('h');
               }
               
               if (!(webstatus & (1<<DATAPEND))) // wartet nicht auf callback
               {
                  // stromstring bilden
                  char key1[]="pw=";
                  char sstr[]="Pong";
                  
                  strcpy(CurrentDataString,key1);
                  strcat(CurrentDataString,sstr);
                  
                  strcat(CurrentDataString,"&strom=");
                  char webstromstring[10]={};
                  urlencode(stromstring,webstromstring);
                  
                  char* tempstromstring = (char*)trimwhitespace(webstromstring);
                  //strcat(CurrentDataString,stromstring);
                  strcat(CurrentDataString,tempstromstring);
               }
               
               // senden aktivieren
               webstatus |= (1<<DATASEND);
               webstatus |= (1<<DATAOK);
               
               webstatus |= (1<<CURRENTSTOP);
               
               webstatus |= (1<<CURRENTWAIT);
               
               paketcounter=0;
               //sendWebCount++;
                          //lcd_gotoxy(19,1);
                          //lcd_putc('>');
               
               
            }
            
            
            if (TEST)
            {
               //lcd_gotoxy(9,0);
               //lcd_putint(anzeigewert);
            }
            
            webstatus |= (1<<CURRENTSEND);
            
         } // genuegend Werte
         else
         {
            //lcd_gotoxy(8,1);
            //lcd_puts("    \0");
            
         }
         
         impulszeit=0;
         currentstatus &= ~(1<<IMPULSBIT);
         
         
      }
		//**    End Current-Routinen*************************
		
		
		if (sendWebCount >2)
		{
			//start_web_client=1;
			sendWebCount=0;
		}
      
      // strom busy?
		if (webstatus & (1<<DATASEND))
		{
#pragma mark packetloop
          
			// **	Beginn Ethernet-Routinen	***********************
			
			// handle ping and wait for a tcp packet
			
         cli();
			
			dat_p=packetloop_icmp_tcp(buf,enc28j60PacketReceive(BUFFER_SIZE, buf));
			//dat_p=1;
			
			if(dat_p==0) // Kein Aufruf, eigene Daten senden an Homeserver
			{
            
            if ((start_web_client==1)) // In Ping_Calback gesetzt: Ping erhalten
            {
               //OSZILO;
               
               //lcd_gotoxy(0,0);
               //lcd_puts("    \0");
               lcd_gotoxy(12,0);
               lcd_puts("ping ok\0");
               lcd_clr_line(1);
               delay_ms(100);
               start_web_client=2; // nur erstes ping beantworten. start_web_client wird in pl-send auf 0 gesetzt
               
               mk_net_str(str,pingsrcip,4,'.',10);
               char* pingsstr="ideur01\0";
               
               urlencode(pingsstr,urlvarstr);
               //lcd_gotoxy(0,1);
               //lcd_puts(urlvarstr);
               //delay_ms(1000);
               
            }
            
           if (webstatus & (1<<DATAOK) )
            {
               
               //lcd_gotoxy(0,0);
               //lcd_puts(CurrentDataString);
               //lcd_putc('*');
               
              // start_web_client=2;
               //strcat("pw=Pong&strom=360\0",(char*)teststring);

               
               start_web_client=0; // ping wieder ermoeglichen
               
               // Daten an strom.pl schicken
               
               if (TESTSERVER)
               {
                  strcat(CurrentDataString,"&e0=");
                  char temperr0[4]= {};
                  //uint8_t fehler0 = 11;
                  
                  itoa((int)callbackerrcounter,temperr0,10); // integer zu ascii im Zehnersystem
                  lcd_gotoxy(14,0);
                  lcd_puts(temperr0);
                  
                  strcat(CurrentDataString,temperr0);
                  
                  //	itoa((int)inbuffer[5],StatusString,10);

                  char ws[16]= {};
                  itoa((int)wattstunden,ws,10); // integer zu ascii im Zehnersystem
                  strcat(CurrentDataString,"&ws=");
                  strcat(CurrentDataString,ws);

                  char pk[16]= {};
                  itoa((int)paketcounter,pk,10); // integer zu ascii im Zehnersystem
                  strcat(CurrentDataString,"&pk=");
                  strcat(CurrentDataString,pk);
                  
                  client_browse_url((char*)PSTR("/cgi-bin/stromtest.pl?"),CurrentDataString,(char*)PSTR(WEBSERVER_VHOST),&strom_browserresult_callback);
                  
                  
                  
                  
                  lcd_gotoxy(18,1);
                  lcd_putc('t');
               }
               else
               {
               
               client_browse_url((char*)PSTR("/cgi-bin/strom.pl?"),CurrentDataString,(char*)PSTR(WEBSERVER_VHOST),&strom_browserresult_callback);
               }
               
               if (errstatus & (1<<CALLBACKERR))
               {
                  client_browse_url((char*)PSTR("/cgi-bin/stromtest.pl?"),CurrentDataString,(char*)PSTR(WEBSERVER_VHOST),&strom_browserresult_callback);
                  errstatus &= ~(1<<CALLBACKERR);
               }
               
               //client_browse_url((char*)PSTR("/cgi-bin/strom.pl?"),teststring,(char*)PSTR(WEBSERVER_VHOST),&strom_browserresult_callback);
               
               sendWebCount++;
               
               webstatus &= ~(1<<DATAOK); // client_browse nur einmal
               webstatus |= (1<<DATAPEND);
               
               lcd_gotoxy(19,0);
               lcd_putc('>');

               // Daten senden
               //www_server_reply(buf,dat_p); // send data
               
            }
            
            continue;
         } // dat_p=0
			
         sei();
         
			/*
			 if (strncmp("GET ",(char *)&(buf[dat_p]),4)!=0)
			 {
			 // head, post and other methods:
			 //
			 // for possible status codes see:
			 
			 // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
			 lcd_gotoxy(0,0);
			 lcd_puts("*GET*\0");
			 dat_p=http200ok();
			 dat_p=fill_tcp_data_p(buf,dat_p,PSTR("<h1>HomeCentral 200 OK</h1>"));
			 goto SENDTCP;
			 }
			 */
			
			
			if (strncmp("/ ",(char *)&(buf[dat_p+4]),2)==0) // Slash am Ende der URL, Status-Seite senden
			{
				lcd_gotoxy(12,1);
				lcd_puts("+/+\0");
				dat_p=http200ok(); // Header setzen
				dat_p=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>200 OK</h1>"));
            dat_p=fill_tcp_data_p(buf,dat_p,PSTR("<h1>HomeCurrent 200 OK</h1>"));
				dat_p=print_webpage_status(buf);
				goto SENDTCP;
			}
			else
			{
				// Teil der URL mit Form xyz?uv=... analysieren
				
                                                                                 #pragma mark cmd
				
				//out_startdaten=DATATASK;	// default
				
				// out_daten setzen
				cmd=analyse_get_url((char *)&(buf[dat_p+5]));
				
				//lcd_gotoxy(5,0);
				//lcd_puts("cmd:\0");
				//lcd_putint(cmd);
				//lcd_putc(' ');
				if (cmd == 1)
				{
					//dat_p = print_webpage_confirm(buf);
				}
				else if (cmd == 2)	// TWI > OFF
				{
                                                                                 #pragma mark cmd 2
				}
				
				else
				{
					dat_p=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\n\r\n<h1>401 Zugriff verweigert</h1>"));
				}
				cmd=0;
				// Eingangsdaten reseten, sofern nicht ein Status0-Wait im Gang ist:
				//if ((pendenzstatus & (1<<SEND_STATUS0_BIT)))
				{
					
				}
				
				goto SENDTCP;
			}
			//
         
		SENDTCP:
              OSZIHI;
         www_server_reply(buf,dat_p); // send data
			
			
		} // strom not busy
	}
	return (0);
}
