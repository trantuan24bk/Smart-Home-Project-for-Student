/*
    Description 	: Firmware to control GSM, LCD, Relay
    Microcontroller : ATmega32A
    Compiler 		: Arduino v1.0.6
    Last modified 	: 01/05/2016
*/

#include <Wire.h>
#include <DS1307.h>
#include <LiquidCrystal.h>
#include <TimerOne.h>
#include <EEPROM.h>
#include <SIM900.h>

#define LCD_RS_PIN 		23					// RS Pin
#define LCD_E_PIN 		22					// EN Pin
#define LCD_D4_PIN 		21					// D4 Pin
#define LCD_D5_PIN 		20					// D5 Pin
#define LCD_D6_PIN 		19					// D6 Pin
#define LCD_D7_PIN 		18					// D7 Pin

#define BAT 			true
#define TAT 			false

#define OK 				true
#define ERROR 			false

#define SPEAKER_PIN 	15 					// Speaker pin
#define LM35_PIN 		A0 					// LM35 sensor

#define ACTION_ALERT 	0x01
#define ACTION_LCD 		0x02
#define ACTION_SMS 		0x04
#define ACTION_DIAL 	0x08

#define EEPROM_STARTUP 	0x47				// EEPROM 
#define EEPROM_ADDR 	10
#define EEPROM_RELAY 	11

SIM900* 		gsm;
DS1307Class* 	rtc;
LiquidCrystal* 	lcd;

uint8_t doActionCode 	= 0;

boolean lcdForceUpdate 	= true;
uint8_t loopCounter   	= 0;
uint8_t lastRelayStatus = 0x00;

int 	lastTempValue 	= 30;

char mobile_home[15];
char action_mobile[25];

const uint8_t Relay_Pins[] = {A2, A3, 13, 14};		// Relay pins
const uint8_t RF1_pins[] = {A5, A7, A4, A6};		// RF pins
const uint8_t RF2_pins[] = {1, 3, 0, 2};

uint8_t RF1_state[] = {TAT, TAT, TAT, TAT};
uint8_t RF2_state[] = {TAT, TAT, TAT, TAT};

int RTCValues[7];								// Date time

void setup() 
{
	int i;

	loopCounter 	= 0;
	lcdForceUpdate 	= true;
	
	// Cau hinh Timer1 de quet trang thai cua RF1 & RF2
	// us => 50ms
	Timer1.initialize(50000);
	Timer1.attachInterrupt(scan_rf_isr);

  	// Config pins
  	for (i = 0; i < 4; ++i)
  	{
  		pinMode(RF1_pins[i], INPUT);
  		pinMode(RF2_pins[i], INPUT);

  		pinMode(Relay_Pins[i], OUTPUT);
  		digitalWrite(Relay_Pins[i], LOW);
  	}

  	// Speaker control pin
  	pinMode(SPEAKER_PIN, OUTPUT);
  	digitalWrite(SPEAKER_PIN, HIGH);

  	// Write default mobile
	strcpy((char*)mobile_home, "+841234567890");

	// Validate data
	if(EEPROM.read(EEPROM_ADDR) != EEPROM_STARTUP)
	{
		// Ghi du lieu lan dau tien
		EEPROM.write(EEPROM_ADDR, EEPROM_STARTUP);

		// Trang thai cac relay
		// 0: OFF, 1: ON (4-bit lower)
		EEPROM.write(EEPROM_RELAY, 0x00);
		lastRelayStatus = 0x00;
	}
	else
	{
		// Trang thai cac relay
		// 0: OFF, 1: ON (4-bit lower)
		lastRelayStatus = EEPROM.read(EEPROM_RELAY);

		for(i = 0; i < 4; i++)
		{
			if(lastRelayStatus & (1<<i))
				digitalWrite(Relay_Pins[i], HIGH);
			else
				digitalWrite(Relay_Pins[i], LOW);
		}
	}

	// Start DS1307 timer
	rtc = new DS1307Class();
	rtc->begin();
	rtc->getDate(RTCValues);

	// Check valid year: >= 16 (2016) 
	if(RTCValues[0] < 16)
	{
		// yy, mm, dom, dow, hh, min, sec
		// rtc->setDate(15, 4,  29,  2,  15, 50, 0);
		rtc->setDate(16, 5,  2,  1,  12, 0, 0);
	}

	// Start LCD 20x4
	lcd = new LiquidCrystal(LCD_RS_PIN, LCD_E_PIN,  LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
	lcd->begin(20, 4);

	// Start SIM900
	lcd->setCursor(3, 1);
	lcd->print("SIM900 Init...");

	gsm = new SIM900();
	gsm->init("0");
	lcd->clear();
}

/*===============================================================================================
 Name         :  loop
 Description  :  
 Argument(s)  :  
 Return value :  None.
================================================================================================*/
void loop() 
{
	int i;
	uint8_t data;
	char *p, *msg, buff[30];

	if(loopCounter++ > 50)
		loopCounter = 0;
	
	// Read SMS from SIM900	
	if(loopCounter == 15)
	{
		msg = gsm->readSMS(1, (char*)action_mobile, 5000);

		strncpy((char*)buff, msg, 30-2);

		gsm->deleteSMS(1);

		msg = (char*)buff;
		
		if(msg != NULL)
		{
			lcdForceUpdate 	= true;

			if(strstr(msg, "ON TB1") != NULL)
			{
				digitalWrite(Relay_Pins[0], HIGH);
				gsm->sendSMS((char*)mobile_home, "TB1 IS ON");
			}
			else if(strstr(msg, "ON TB2") != NULL)
			{
				digitalWrite(Relay_Pins[1], HIGH);
				gsm->sendSMS((char*)mobile_home, "TB2 IS ON");
			}
			else if(strstr(msg, "ON TB3") != NULL)
			{
				digitalWrite(Relay_Pins[2], HIGH);
				gsm->sendSMS((char*)mobile_home, "TB3 IS ON");
			}
			else if(strstr(msg, "ON TB4") != NULL)
			{
				digitalWrite(Relay_Pins[3], HIGH);
				gsm->sendSMS((char*)mobile_home, "TB4 IS ON");
			}
			else if(strstr(msg, "OFF TB1") != NULL)
			{
				digitalWrite(Relay_Pins[0], LOW);
				gsm->sendSMS((char*)mobile_home, "TB1 IS OFF");
			}
			else if(strstr(msg, "OFF TB2") != NULL)
			{
				digitalWrite(Relay_Pins[1], LOW);
				gsm->sendSMS((char*)mobile_home, "TB2 IS OFF");
			}
			else if(strstr(msg, "OFF TB3") != NULL)
			{
				digitalWrite(Relay_Pins[2], LOW);
				gsm->sendSMS((char*)mobile_home, "TB3 IS OFF");
			}
			else if(strstr(msg, "OFF TB4") != NULL)
			{
				digitalWrite(Relay_Pins[3], LOW);
				gsm->sendSMS((char*)mobile_home, "TB4 IS OFF");
			}
			else if(strstr(msg, "TURN ALL ON") != NULL)
			{
				digitalWrite(Relay_Pins[0], HIGH);
				digitalWrite(Relay_Pins[1], HIGH);
				digitalWrite(Relay_Pins[2], HIGH);
				digitalWrite(Relay_Pins[3], HIGH);
				gsm->sendSMS((char*)mobile_home, "TURNED ALL ON");
			}
			else if(strstr(msg, "TURN ALL OFF") != NULL)
			{
				digitalWrite(Relay_Pins[0], LOW);
				digitalWrite(Relay_Pins[1], LOW);
				digitalWrite(Relay_Pins[2], LOW);
				digitalWrite(Relay_Pins[3], LOW);
				gsm->sendSMS((char*)mobile_home, "TURNED ALL OFF");
			}
			else if(strstr(msg, "ChangeTime_") != NULL)
			{
				byte hh, min, sec, dom, dow, mon, yy;

				// Update the device time
				// ChangeTime_183000_28022015
				// 18:30:00 28/02/2015
				p = strchr(msg, '_') + 1;

				hh = (*p - 48)*10 + (*(p + 1) - 48);		// hour
				p = p + 2;

				min = (*p - 48)*10 + (*(p + 1) - 48);		// minute
				p = p + 2;
				
				sec = (*p - 48)*10 + (*(p + 1) - 48);		// second
				p = p + 3;
				
				dom = (*p - 48)*10 + (*(p + 1) - 48);		// day of month
				p = p + 2;
				
				mon = (*p - 48)*10 + (*(p + 1) - 48);		// month
				p = p + 4;
				
				yy = (*p - 48)*10 + (*(p + 1) - 48);		// year
				
				// yy, mm, dom, dow, hh, min, sec
			    rtc->setDate(yy, mon,  dom,  4,  hh, min, sec);
			}
		}
	}

	// Update relay status
	if(((loopCounter%3) == 0) || (lcdForceUpdate == true))
	{
		data  = ((digitalRead(Relay_Pins[0]) == HIGH)? 1 : 0)<<0;
		data |= ((digitalRead(Relay_Pins[1]) == HIGH)? 1 : 0)<<1;
		data |= ((digitalRead(Relay_Pins[2]) == HIGH)? 1 : 0)<<2;
		data |= ((digitalRead(Relay_Pins[3]) == HIGH)? 1 : 0)<<3;

		if((data != lastRelayStatus) || (lcdForceUpdate == true))
		{
			lcdForceUpdate  = false;
			lastRelayStatus = data;
			
			EEPROM.write(EEPROM_RELAY, lastRelayStatus);

			for (i = 0; i < 4; ++i)
			{
				if(i == 0) 		lcd->setCursor(0,  0);
				else if(i == 1) lcd->setCursor(0,  1);
				else if(i == 2) lcd->setCursor(10, 0);
				else 			lcd->setCursor(10, 1);
				
				if(digitalRead(Relay_Pins[i]) == HIGH)
				{
					lcd->print("TB" + String(i+1) + ": ON");
				}
				else
				{
					lcd->print("TB" + String(i+1) + ": OFF");
				}
			}
		}
	}
	    
	// Read date, hour from DS1307
	if((loopCounter%3) == 0)
	{
		rtc->getDate(RTCValues);

		// Display dd:mm:yyyy
		lcd->setCursor(5, 2);
		lcd->print(RTCValues[2]); lcd->print("/"); 			// dd
		lcd->print(RTCValues[1]); lcd->print("/"); 			// mm
		lcd->print("20"); 		  lcd->print(RTCValues[0]); // yyyy
		lcd->print("  ");

		// Display hh:mm:ss
		lcd->setCursor(1, 3);
		lcd->print(RTCValues[4]); lcd->print(":"); 			// hh
		lcd->print(RTCValues[5]); lcd->print(":"); 			// mm
		lcd->print(RTCValues[6]); lcd->print("  "); 		// ss
	}
	
	// Read status from LM35 sensor
	if((loopCounter%10) == 0)
	{
		lastTempValue = (lastTempValue + (int)(analogRead(LM35_PIN)*0.488 - 120))/2;

		lcd->setCursor(13, 3);
		lcd->print(lastTempValue);
		lcd->print((char)223);
		lcd->print("C  ");
	}

	// Process command actions (RF, GSM)
	switch(doActionCode)
	{
		case 1: 											// Fired alarm
			lcd->clear();
			lcd->setCursor(6, 1);
			lcd->print("Fired alarm!");	
			gsm->sendSMS((char*)mobile_home, "Fired alarm!");

			i = 0x00;
			while(i++ <= 20)
			{
				digitalWrite(SPEAKER_PIN, LOW);
				__delay_ms(800);
				digitalWrite(SPEAKER_PIN, HIGH);
				__delay_ms(800);
			}
			
			lcd->clear();
			doActionCode 	= 0;
			lcdForceUpdate 	= true;
			break;
		case 2: 											// Theft alert
			lcd->clear();
			lcd->setCursor(6, 1);
			lcd->print("Theft alert!");
			gsm->sendSMS((char*)mobile_home, "Theft alert!");

			i = 0x00;
			while(i++ <= 20)
			{
				digitalWrite(SPEAKER_PIN, LOW);
				__delay_ms(800);
				digitalWrite(SPEAKER_PIN, HIGH);
				__delay_ms(800);			
			}
			
			lcd->clear();
			doActionCode 	= 0;
			lcdForceUpdate 	= true;
			break;
		case 3: 											// Gas leaked
			lcd->clear();
			lcd->setCursor(5, 1);
			lcd->print("Gas leaked!");			
			gsm->sendSMS((char*)mobile_home, "Gas leaked!");

			i = 0x00;
			while(i++ <= 20)
			{
				digitalWrite(SPEAKER_PIN, LOW);
				__delay_ms(800);
				digitalWrite(SPEAKER_PIN, HIGH);
				__delay_ms(800);
			}

			lcd->clear();
			doActionCode 	= 0;
			lcdForceUpdate 	= true;
			break;
		case 4: 											// Dial mobile
			lcd->clear();
			lcd->setCursor(2, 1);
			lcd->print("Calling...");
			gsm->call((char*)mobile_home);
			__delay_ms(5000);
			
			lcd->clear();
			doActionCode 	= 0;
			lcdForceUpdate 	= true;
			break;		
		default:
			break;
	}
}

void scan_rf_isr(void)
{
	int i;
	uint8_t preState;

	for (i = 0; i < 4; ++i)
	{
		preState = RF2_state[i];

		// Read status of RF1 channel
		if(digitalRead(RF1_pins[i]) == HIGH)
			RF1_state[i] = BAT;
		else
			RF1_state[i] = TAT;

		// Read status of RF2 channel
		if(digitalRead(RF2_pins[i]) == HIGH)
			RF2_state[i] = BAT;
		else
			RF2_state[i] = TAT;

		// RF2: Manual control by user
		if((preState == TAT) && (RF2_state[i] == BAT))
		{
			if(digitalRead(Relay_Pins[i]) == HIGH)
				digitalWrite(Relay_Pins[i], LOW);
			else
				digitalWrite(Relay_Pins[i], HIGH);

			lcdForceUpdate = true;
		}
	}

	if(RF1_state[0] == BAT)
	{
		// Fired alarm', display LCD, Speaker, Send SMS
		doActionCode = 1;
	}

	if(RF1_state[1] == BAT)
	{
		// Theft alert, display LCD, Speaker, Send SMS
		doActionCode = 2;
	}

	if(RF1_state[2] == BAT)
	{
		// Gas leaked, display LCD, Speaker, Send SMS
		doActionCode = 3;
	}

	if(RF1_state[3] == BAT)
	{
		// Dial number
		doActionCode = 4;
	}
}

void __delay_ms(unsigned long timeout) 
{
	volatile unsigned long i = 0, timeTotal;

	timeTotal = timeout*380;

	for (i = 0; i < timeTotal; i++)
	{
		__asm__ __volatile__ ("nop");
	}
}

void __delay_us(unsigned long timeout) 
{
	volatile unsigned long i = 0;

	for (i = 0; i < timeout; i++)
	{
		__asm__ __volatile__ ("nop");
	}
}

