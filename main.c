#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>#include <avr/delay.h>
#include "usart_ATmega1284.h"
#include "io.c"
#include "scheduler.h"

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks
// Set timer to TCCR3
// Servo is using register 1
void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B 	= (1<<WGM12)|(1<<CS11)|(1<<CS10);
	// WGM12 (bit3) = 1: CTC mode (clear timer on compare)
	// CS12,CS11,CS10 (bit2bit1bit0) = 011: prescaler /64
	// Thus TCCR1B = 00001011 or 0x0B
	// So, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	//OCR1A 	= 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	OCR1A = 290;
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register

	#if defined (__AVR_ATmega1284__)
	TIMSK1 	= (1<<OCIE1A); // OCIE1A (bit1): enables compare match interrupt - ATMega1284
	#else
	TIMSK 	= (1<<OCIE1A); // OCIE1A (bit1): enables compare match interrupt - ATMega32
	#endif

	// Initialize avr counter
	TCNT1 = 0;

	// TimerISR will be called every tasksPeriodCntDown milliseconds
	tasksPeriodCntDown = tasksPeriodGCD;

	// Enable global interrupts
	SREG |= 0x80;	// 0x80: 1000000
}

void A2D_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: Enables analog-to-digital conversion
	// ADSC: Starts analog-to-digital conversion
	// ADATE: Enables auto-triggering, allowing for constant
	//	    analog to digital conversions.
}

void ADC_init(void)
{
	ADMUX|=(1<<REFS0);
	ADCSRA|=(1<<ADEN)|(1<<ADPS0)|(1<<ADPS1)|(1<<ADPS2); //ENABLE ADC, PRESCALER 128
}

unsigned short readadc(uint8_t ch)
{
	ch&=0b00000111;         //ANDing to limit input to 7
	ADMUX = (ADMUX & 0xf8)|ch;  //Clear last 3 bits of ADMUX, OR with ch
	ADCSRA|=(1<<ADSC);        //START CONVERSION
	while((ADCSRA)&(1<<ADSC));    //WAIT UNTIL CONVERSION IS COMPLETE
	return(ADCW);        //RETURN ADC VALUE
}

enum motor_states {motor_wait, motor_open, motor_close, motor_manual_open, motor_manual_close} motor_state;
enum menu_states {menu_off, menu_on, menu_wait, menu_user_login, menu_user, menu_pass_login, menu_pass, menu_menu, menu_select, menu_upchange} menu_state;

unsigned short inputUD, inputLR;
unsigned short pos;
unsigned char powerSwitch;
unsigned char button;

unsigned char cursor = 1;
unsigned char correctUser = 0;
unsigned char correctPass = 0;
unsigned char letter = 65;				// 'A'
unsigned char number = 48;
unsigned char count = 30;
float speed = 1;
uint8_t data = 0;
unsigned char motorFlag = 0;
unsigned char menuState = 0;
unsigned char menuFlag = 0;
unsigned char upFlag = 0;
unsigned char temp = 0;
unsigned char cursorArr1[3] = {1, 17, 32};

char userAttempt[5] = "";
unsigned char sz = 0;
char passAttempt[5] = "";
unsigned char psz = 0;
char* userpass = "AAAA00000";
 
void changeUser(char src[]) {
	for(int i = 0; i < 4; i++) {
		eeprom_write_byte((uint8_t*)46+i, src[i]);
		userpass[i] = eeprom_read_byte((uint8_t*)46+i);
		LCD_ClearScreen();
		_delay_ms(10000);
		//LCD_WriteData(userpass[i]);
		LCD_WriteData(userpass[5]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[6]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[7]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[8]);
		_delay_ms(10000);
		LCD_ClearScreen();
	}
}
void changePass(char* src) {
	for(int i = 5; i < 9; i++) {
		eeprom_write_byte((uint8_t*)46+i, src[i-5]);
		userpass[i] = eeprom_read_byte((uint8_t*)46+i);

	}
}

char* getUser() {
	char* temp = "";
	for(int i = 0; i < 4; i++) {
		//eeprom_write_byte((uint8_t*)46+i, userpass[i]);
		temp[i] = eeprom_read_byte((uint8_t*)46+i);
		_delay_ms(1);
		LCD_WriteData(userpass[0]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[1]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[2]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[3]);
		_delay_ms(1);
		LCD_ClearScreen();
		//LCD_WriteData(temp[i]);
	}
	return temp;
}
char* getPass() {
	char* temp = "";
	for(int i = 5; i < 9; i++) {
		//eeprom_write_byte((uint8_t*)99+i, userpass[i]);
		temp[i-5] = eeprom_read_byte((uint8_t*)46+i);
		//LCD_WriteData(userpass[i]);
			
	}
	return temp;
}

void firstUser()
{
	for(int i = 0; i < 4; i++) {
		eeprom_write_byte((uint8_t*)46+i, 'A');
		userpass[i] = eeprom_read_byte((uint8_t*)46+i);
		/*_delay_ms(10000);
		LCD_WriteData(userpass[0]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[1]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[2]);
		//_delay_ms(5000);
		LCD_WriteData(userpass[3]);
		_delay_ms(10000);*/
		LCD_ClearScreen();
	}
}
void firstPass()
{
	for(int i = 5; i < 9; i++) {
		eeprom_write_byte((uint8_t*)46+i, '0');
		userpass[i] = eeprom_read_byte((uint8_t*)46+i);
	}
}

 
int tick_Menu(int menu_state) {
	powerSwitch = ~PINB & 0x01;
	button = (~PINB & 0x02) >> 0x01;
	
	inputLR = readadc(0);
	inputUD = readadc(1);
	
	pos = OCR1A;

	switch(menu_state) { // State actions
		case menu_off:
			if(powerSwitch)
			{
				LCD_WriteCommand(0x0C); //no cursor
				LCD_WriteCommand(0x07); //scrolling display
				LCD_DisplayString(1, "   Welcome to      Pandora's Box");
			}
			else {
				LCD_WriteCommand(0x08); //no display
				data = 4;
				/*LCD_WriteCommand(0x0C); //no cursor
				eeprom_write_byte((uint8_t*)46, userpass[0]);
				userAttempt[0] = eeprom_read_byte((uint8_t*)46);
				LCD_WriteData(userAttempt[0]);
				LCD_Cursor(1);*/
			}
		break;
		
		case menu_on:
			count--;
			if(inputUD >= 400 && inputUD <= 600 && inputLR >= 400 && inputLR <= 600) {
				LCD_Cursor(cursor);
				LCD_WriteData(' ');
			}
			else if(inputUD > 600 && !(inputLR < 400 || inputLR > 600)) {
				LCD_Cursor(cursor);
				LCD_WriteData(' ');
			}
			else if(inputUD < 400 && !(inputLR < 400 || inputLR > 600)) {
				LCD_Cursor(cursor);
				LCD_WriteData(' ');
			}
			else if(inputLR > 600 && !(inputUD < 400 || inputUD > 600)) {
				LCD_WriteData(' ');
				LCD_Cursor(cursor);
			}
			else if(inputLR < 400 && !(inputUD < 400 || inputUD > 600)) {
				LCD_Cursor(cursor);
				LCD_WriteData(' ');
			}
		break;
		
		case menu_wait:
			userpass = getUser();
			userpass = getPass();
			LCD_WriteCommand(0x0C); //no cursor
			LCD_WriteCommand(0x06); //default display
			LCD_ClearScreen();
			LCD_DisplayString(1, "User:           Pass:");
		break;
		
		case menu_user_login:
			if(inputUD >= 400 && inputUD <= 600 && inputLR >= 400 && inputLR <= 600) {						//get user
			}
			else if(inputUD > 600 && !(inputLR < 400 || inputLR > 600)) {
				if(letter == 90)		letter = 97;
				else if(letter == 122)	letter = 12;
				else					letter++;
			}
			else if(inputUD < 400 && !(inputLR < 400 || inputLR > 600)) {
				if(letter == 97)		letter = 90;
				else if(letter == 65)	letter = 65;
				else					letter--;
			}
			else if(inputLR > 600 && !(inputUD < 400 || inputUD > 600)) {
				userAttempt[sz] = letter;
				sz++;
				if(cursor == 9) {
					LCD_Cursor(10);
					LCD_WriteData(' ');
				}
				cursor++;
				break;
			}
			else if(inputLR < 400 && !(inputUD < 400 || inputUD > 600)) {
				if(sz > 0)				sz--;
					LCD_Cursor(cursor);
					LCD_WriteData(127);
				if(cursor > 6){
					cursor--;
				}
			}
			if(cursor < 10) {
				LCD_Cursor(cursor);
				LCD_WriteData(letter);
			}
		break;
		
		case menu_user:
			LCD_WriteCommand(0x0F); // show cursor
			/*LCD_WriteData(userpass[0]);
			LCD_WriteData(userpass[1]);
			LCD_WriteData(userpass[2]);
			LCD_WriteData(userpass[3]);*/
			LCD_DisplayString(16, " Is this you? Y/N");
			if(inputLR > 600 && !(inputUD < 400 || inputUD > 600)) {
				cursor = 32;
			}
			else if(inputLR < 400 && !(inputUD < 400 || inputUD > 600)) {
				cursor = 30;
			}
			LCD_Cursor(cursor);
			
			if(button) {
				if(cursor == 30) {
					if( userAttempt[0] == userpass[0] && userAttempt[1] == userpass[1] && userAttempt[2] == userpass[2] && userAttempt[3] == userpass[3]) {
						correctUser = 1;
						data = 2;
					}
					else {
						LCD_ClearScreen();
						LCD_DisplayString(1, "Name does not   match.Try again.");
						data = 0;
						_delay_ms(10000);
						correctUser = 2;
					}
				}
				else if(cursor == 32) {
					correctUser = 2;
				}
			}
			
			if(correctUser == 1) {
				LCD_WriteCommand(0x0C); // no cursor
				LCD_ClearScreen();
				LCD_DisplayString(1, "User:           Pass:");
				LCD_Cursor(6);
				LCD_WriteData(userpass[0]);
				//_delay_ms(5000);
				LCD_WriteData(userpass[1]);
				//_delay_ms(5000);
				LCD_WriteData(userpass[2]);
				//_delay_ms(5000);
				LCD_WriteData(userpass[3]);
				LCD_DisplayString(6, userpass);
				cursor = 22;
				LCD_Cursor(cursor);
				/*LCD_DisplayString(1, "Attempt:        Real:");										//test user results
				cursor = 9;
				LCD_Cursor(cursor);
				LCD_DisplayString(cursor, userAttempt);
				cursor = 22;
				LCD_Cursor(cursor);
				//char* user;
				//eeprom_read_block(user,&userpass, 4);
				LCD_DisplayString(cursor, userpass);*/	
			}
		break;
		
		case menu_pass_login:
			if(inputUD >= 400 && inputUD <= 600 && inputLR >= 400 && inputLR <= 600) {				//test passcode
			}
			else if(inputUD > 600 && !(inputLR < 400 || inputLR > 600)) {
				if(number == 57)		number = 57;
				else					number++;
			}
			else if(inputUD < 400 && !(inputLR < 400 || inputLR > 600)) {
				if(number == 48)		number = 48;
				else					number--;
			}
			else if(inputLR > 600 && !(inputUD < 400 || inputUD > 600)) {
				passAttempt[psz] = number;
				psz++;
				if(cursor == 25) {
					LCD_Cursor(26);
					LCD_WriteData(' ');
				}
				cursor++;
				break;
			}
			else if(inputLR < 400 && !(inputUD < 400 || inputUD > 600)) {
				if(psz > 0)				psz--;
				LCD_Cursor(cursor);
				LCD_WriteData(127);
				if(cursor > 22){
					cursor--;
				}
			}
			if(cursor> 21 && cursor < 26) {
				LCD_Cursor(cursor);
				LCD_WriteData(number);
			}
		break;
		
		case menu_pass:
			if( passAttempt[0] == userpass[5] && passAttempt[1] == userpass[6] && passAttempt[2] == userpass[7] && passAttempt[3] == userpass[8]) {
				LCD_ClearScreen();
				LCD_DisplayString(1, "  Welcome Back  ");
				data = 2;
				//LCD_DisplayString(23, userpass );
				LCD_Cursor(23);
				LCD_WriteData(userpass[0]);
				//_delay_ms(5000);
				LCD_WriteData(userpass[1]);
				//_delay_ms(5000);
				LCD_WriteData(userpass[2]);
				//_delay_ms(5000);
				LCD_WriteData(userpass[3]);
				correctPass = 1;
				_delay_ms(10000);
				break;
			}
			else {
				LCD_ClearScreen();
				LCD_DisplayString(1, "Wrong password. Try again.");
				data = 0;
				_delay_ms(10000);
				correctPass = 2;
			}
			LCD_ClearScreen();
		break;
		
		case menu_menu:
			if(menuState == 0) {
				LCD_DisplayString(1, "1-Open/Close(A) 2-Open/Close(M)>");
				LCD_Cursor(1);
				cursor = 0;
				temp = 0;
			}
			else if(menuState == 1) {
				LCD_DisplayString(1, "3-Settings      4-Log Out      <");
				LCD_Cursor(1);
				cursor = 0;
				temp = 0;
			}
			else if(menuState == 2) {
				LCD_DisplayString(1, "1-Motor Speed   2-User/Pass    <");
				LCD_Cursor(1);
				cursor = 0;
				temp = 0;
			}
			else if(menuState == 3) {
				LCD_DisplayString(1, "1-Speed Up      2-Slow Down    <");
				LCD_Cursor(1);
				cursor = 0;
				temp = 0;
			}
			else if(menuState == 4) {
				LCD_DisplayString(1, "1-Change User   2-Change Pass  <");
				LCD_Cursor(1);
				cursor = 0;
				temp = 0;
			}
		break;
		
		case menu_select:
			LCD_WriteCommand(0x0F);
			if(inputLR > 600 && !(inputUD < 400 || inputUD > 600)) {
				if(temp < 2)	temp++;
			}
			else if(inputLR < 400 && !(inputUD < 400 || inputUD > 600)) {
				if(temp > 0)	temp--;
			}
			else
				temp = temp;
			
			cursor = cursorArr1[temp];
			LCD_Cursor(cursor);
			
			if(button) {
				if(menuState == 0) {
					if(cursor == 1) {
						if(pos > 150)//motorFlag == 0 || motorFlag == 2)
							motorFlag = 1;
						else if(pos < 290)//motorFlag == 1)
							motorFlag = 2;
					}
					else if(cursor == 17) {
						if(motorFlag == 0 || motorFlag == 2)
							motorFlag = 3;
						else if(motorFlag == 1)
							motorFlag = 4;
					}
					else if(cursor == 16) {
						menuFlag = 0;
						menuState = 0;
					}
					else if(cursor == 32) {
						menuFlag = 1;
						menuState = 1;
					}
				}
				else if(menuState == 1) {
					if(cursor == 1) {
						menuFlag = 1;
						menuState = 2;
					}
					else if(cursor == 17) {
						LCD_ClearScreen();
						LCD_WriteCommand(0x0C);
						LCD_DisplayString(1, "    Goodbye!    ");
						_delay_ms(10000);
					}
					else if(cursor == 32) {
						menuFlag = 1;
						menuState = 0;
					}
				}
				else if(menuState == 2) {
					if(cursor == 1) {
						menuFlag = 1;
						menuState = 3;
					}
					else if(cursor == 17) {
						menuFlag = 1;
						menuState = 4;
					}
					else if(cursor == 32) {
						menuFlag = 1;
						menuState = 1;
					}
				}
				else if(menuState == 3) {
					if(cursor == 1) {
						if(speed != 1)	speed -= 10;
					}
					else if(cursor == 17) {
						speed+= 10;;
					}
					else if(cursor == 32) {
						menuFlag = 1;
						menuState = 2;
					}
				}
				else if(menuState == 4) {
					if(cursor == 1) {
						upFlag = 1;
						
						LCD_DisplayString(1, "New:            Old:            ");
						LCD_Cursor(21);
						LCD_WriteData(userpass[0]);
						//_delay_ms(5000);
						LCD_WriteData(userpass[1]);
						//_delay_ms(5000);
						LCD_WriteData(userpass[2]);
						//_delay_ms(5000);
						LCD_WriteData(userpass[3]);
						//_delay_ms(5000);	
						cursor = 5;	
						sz = 0;
					}
					else if(cursor == 17) {
						upFlag = 2;
						
						LCD_DisplayString(1, "New:            Old:            ");
						LCD_Cursor(21);
						LCD_WriteData(userpass[5]);
						//_delay_ms(5000);
						LCD_WriteData(userpass[6]);
						//_delay_ms(5000);
						LCD_WriteData(userpass[7]);
						//_delay_ms(5000);
						LCD_WriteData(userpass[8]);
						cursor = 5;
						psz = 0;
					}
					else if(cursor == 32) {
						menuFlag = 1;
						menuState = 2;
					}
				}
			}
			
		break;
		
		case menu_upchange:
			LCD_WriteCommand(0x0C);
			if(upFlag == 1) {
				if(inputUD >= 400 && inputUD <= 600 && inputLR >= 400 && inputLR <= 600) {						//get user
				}
				else if(inputUD > 600 && !(inputLR < 400 || inputLR > 600)) {
					if(letter == 90)		letter = 97;
					else if(letter == 122)	letter = 12;
					else					letter++;
				}
				else if(inputUD < 400 && !(inputLR < 400 || inputLR > 600)) {
					if(letter == 97)		letter = 90;
					else if(letter == 65)	letter = 65;
					else					letter--;
				}
				else if(inputLR > 600 && !(inputUD < 400 || inputUD > 600)) {
					userAttempt[sz] = letter;
					sz++;
					if(cursor == 8) {
						LCD_Cursor(9);
						LCD_WriteData(' ');
					}
					cursor++;
				}
				else if(inputLR < 400 && !(inputUD < 400 || inputUD > 600)) {
					if(sz > 0)				sz--;
					LCD_Cursor(cursor);
					LCD_WriteData(127);
					if(cursor > 5){
						cursor--;
					}
				}
				
				if(cursor> 4 && cursor < 9) {
					LCD_Cursor(cursor);
					LCD_WriteData(letter);
				}
				else {
					changeUser(userAttempt);
					upFlag = 0;
				}
			}
			else if(upFlag == 2) {
				if(inputUD >= 400 && inputUD <= 600 && inputLR >= 400 && inputLR <= 600) {				//test passcode
				}
				else if(inputUD > 600 && !(inputLR < 400 || inputLR > 600)) {
					if(number == 57)		number = 57;
					else					number++;
				}
				else if(inputUD < 400 && !(inputLR < 400 || inputLR > 600)) {
					if(number == 48)		number = 48;
					else					number--;
				}
				else if(inputLR > 600 && !(inputUD < 400 || inputUD > 600)) {
					passAttempt[psz] = number;
					psz++;
					if(cursor == 8) {
						LCD_Cursor(9);
						LCD_WriteData(' ');
					}
					cursor++;
				}
				else if(inputLR < 400 && !(inputUD < 400 || inputUD > 600)) {
					if(psz > 0)				psz--;
					LCD_Cursor(cursor);
					LCD_WriteData(127);
					if(cursor > 5){
						cursor--;
					}
				}
				
				if(cursor> 4 && cursor < 9) {
					LCD_Cursor(cursor);
					LCD_WriteData(number);
				}
				else {
					changePass(passAttempt);
					upFlag = 0;
				}
			}
			//upFlag = 0;
		break;
	
		default: // ADD default behavior below
		break;
	} // State actions
	
	switch(menu_state) { // Transitions
		case menu_off:
			if(powerSwitch) {
				data = 3;
				menu_state = menu_on;
				//menu_state = menu_wait;
			}	
			else {
				menu_state = menu_off;
				upFlag = 0;
				menuFlag = 0;
			}
		break;
		
		case menu_on:
			data = 3;
			if(!powerSwitch) {
				menu_state = menu_off;
			}
			else if(count != 0) {
				menu_state = menu_on;
				//count++;
			}
			else		menu_state = menu_wait;
		break;
		
		case menu_wait:
			data = 3;
			if(!powerSwitch) {
				menu_state = menu_off;
			}
			else {
				cursor = 6;
				menu_state = menu_user_login;
			}
		break;
		
		case menu_user_login:
			data = 1;
			if(!powerSwitch) {
				menu_state = menu_off;
			}
			else if(cursor == 10) {
				correctUser = 0;
				cursor = 30;
				menu_state = menu_user;
			}
			else menu_state = menu_user_login;
		break;
		
		case menu_user:
			if(!powerSwitch) {
				menu_state = menu_off;
			}
			else if(correctUser == 2)	menu_state = menu_wait;
			else if(correctUser == 1)	menu_state = menu_pass_login;
			else						menu_state = menu_user;
		break;
		
		case menu_pass_login:
			data = 1;
			if(!powerSwitch) {
				menu_state = menu_off;
			}
			else if(cursor == 26) {
				correctPass = 0;
				menu_state = menu_pass;
			}
			else menu_state = menu_pass_login;
		break;
		
		case menu_pass:
			data = 1;
			if(!powerSwitch)			menu_state = menu_off;
			else if(correctPass == 2)	menu_state = menu_wait;
			else if(correctPass == 1)	menu_state = menu_menu;
			else						menu_state = menu_pass;
		break;
		
		case menu_menu:
			data = 3;
			if(!powerSwitch) {
				menu_state = menu_off;
			}
			else {
				menu_state = menu_select;
			}
		break;
		
		case menu_select:
			data = 3;
			if(!powerSwitch) {
				menu_state = menu_off;
			}
			else if(menuFlag && !upFlag) {
				menuFlag = 0;
				menu_state = menu_menu;
			}
			else if(cursor == 17 && menuState == 1 && button) { 
				upFlag = 0;
				menuFlag = 0;
				menuState = 0;
				letter = 65;
				number = 48;
				menu_state = menu_wait;
			}
			else if(upFlag) {
				menu_state = menu_upchange;
			}
			else {
				menu_state = menu_select;
			}
		break;
		
		case menu_upchange:
			data = 3;
			if(!powerSwitch)	menu_state = menu_off;
			else if(upFlag == 0)	menu_state = menu_menu;
			else				menu_state = menu_upchange;
		break;
		
		default:
		menu_state = menu_off;
		break;
	} // Transitions
	
	return menu_state;
}

int tick_Motor(int motor_state) {
	inputLR = readadc(0);
	inputUD = readadc(1);
	pos = OCR1A;
	
	switch(motor_state) {
		case motor_wait:
		if(motorFlag == 1)		motor_state = motor_open;
		else if(motorFlag == 2)	motor_state = motor_close;
		else if(motorFlag == 3)	motor_state = motor_manual_open;
		else if(motorFlag == 4)	motor_state = motor_manual_close;
		else					motor_state = motor_wait;
		
		break;
		
		case motor_open:
		if(motorFlag == 1)		motor_state = motor_open;
		else if(motorFlag == 2)	motor_state = motor_close;
		else if(motorFlag == 3)	motor_state = motor_manual_open;
		else if(motorFlag == 4)	motor_state = motor_manual_close;
		else					motor_state = motor_wait;
		break;
		
		case motor_close:
		if(motorFlag == 1)		motor_state = motor_open;
		else if(motorFlag == 2)	motor_state = motor_close;
		else if(motorFlag == 3)	motor_state = motor_manual_open;
		else if(motorFlag == 4)	motor_state = motor_manual_close;
		else					motor_state = motor_wait;
		break;
		
		case motor_manual_open:
		if(motorFlag == 1)		motor_state = motor_open;
		else if(motorFlag == 2)	motor_state = motor_close;
		else if(motorFlag == 3)	motor_state = motor_manual_open;
		else if(motorFlag == 4)	motor_state = motor_manual_close;
		else					motor_state = motor_wait;
		break;
		
		case motor_manual_close:
		if(motorFlag == 1)		motor_state = motor_open;
		else if(motorFlag == 2)	motor_state = motor_close;
		else if(motorFlag == 3)	motor_state = motor_manual_open;
		else if(motorFlag == 4)	motor_state = motor_manual_close;
		else					motor_state = motor_wait;
		break;
		
		default:
		motor_state = motor_wait;
		break;
	}
	
	switch(motor_state) {
		case motor_wait:
			OCR1A = pos;
		break;
		
		case motor_open:
			for(unsigned rep = pos; rep >= 150; rep--) {
				OCR1A = rep;
				for(float k = 0; k < speed; k++)
					_delay_ms(1);//speed);
			}
		break;
		
		case motor_close:
			for(unsigned rep = pos; rep <= 290; rep++) {
				OCR1A = rep;
				for(float k = 0; k < speed; k++)
					_delay_ms(1);//speed);
			}
		break;
		
		case motor_manual_open:
			if(inputUD >= 500 && inputUD < 535)					OCR1A = 290;
			else if(inputUD >= 535 && inputUD < 570)			OCR1A = 280;
			else if(inputUD >= 570 && inputUD < 605)			OCR1A = 270;
			else if(inputUD >= 605 && inputUD < 640)			OCR1A = 260;
			else if(inputUD >= 640 && inputUD < 675)			OCR1A = 250;
			else if(inputUD >= 675 && inputUD < 710)			OCR1A = 240;
			else if(inputUD >= 710 && inputUD < 745)			OCR1A = 230;
			else if(inputUD >= 745 && inputUD < 780)			OCR1A = 220;
			else if(inputUD >= 780 && inputUD < 815)			OCR1A = 210;
			else if(inputUD >= 815 && inputUD < 850)			OCR1A = 200;
			else if(inputUD >= 850 && inputUD < 885)			OCR1A = 190;
			else if(inputUD >= 885 && inputUD < 920)			OCR1A = 180;
			else if(inputUD >= 920 && inputUD < 955)			OCR1A = 170;
			else if(inputUD >= 955 && inputUD < 990)			OCR1A = 160;
			else if(inputUD >= 990 && inputUD < 1000) {			OCR1A = 150;
				motorFlag = 1;
			}
			else												OCR1A = pos;
		break;
		case motor_manual_close:
			if(inputUD >= 0 && inputUD < 35) {					OCR1A = 290;
				motorFlag = 2;
			}
			else if(inputUD >=  35 && inputUD < 70)				OCR1A = 280;
			else if(inputUD >=  70 && inputUD < 105)			OCR1A = 270;
			else if(inputUD >= 105 && inputUD < 140)			OCR1A = 260;
			else if(inputUD >= 140 && inputUD < 175)			OCR1A = 250;
			else if(inputUD >= 175 && inputUD < 210)			OCR1A = 240;
			else if(inputUD >= 210 && inputUD < 245)			OCR1A = 230;
			else if(inputUD >= 245 && inputUD < 280)			OCR1A = pos;
			else if(inputUD >= 280 && inputUD < 315)			OCR1A = 210;
			else if(inputUD >= 315 && inputUD < 350)			OCR1A = 200;
			else if(inputUD >= 350 && inputUD < 385)			OCR1A = 190;
			else if(inputUD >= 385 && inputUD < 420)			OCR1A = 180;
			else if(inputUD >= 420 && inputUD < 455)			OCR1A = 170;
			else if(inputUD >= 455 && inputUD < 490)			OCR1A = 160;
			else if(inputUD >= 490 && inputUD <= 500)			OCR1A = 150;
			else												OCR1A = pos;
		break;
		
		default:
		break;
	}
	
	return motor_state;
}

int main(void)
{
	//Init ADC
	ADC_init();
	
	TCCR1A|=(1<<COM1A1)|(1<<COM1B1)|(1<<WGM11);
	TCCR1B|=(1<<WGM13)|(1<<WGM12)|(1<<CS11)|(1<<CS10);
	ICR1=4999;  //50Hz send servo
	
	DDRB = 0x00; PORTB = 0xFF;
	DDRC = 0xFF; PORTC = 0x00; // LCD data lines
	DDRD = 0xFF; PORTD = 0x00;//LCD control lines
	DDRD |= (1<<PD5);  //Servo Output PD4-PD5
	
	// Initializes the LCD display
	LCD_init();
	//Initializes USART
	initUSART(0);
	
	
	userpass = getUser();
	if( userpass[0] == 255 && userpass[1] == 255 && userpass[2] == 255 && userpass[3] == 255) {
		//LCD_WriteData('a');
		firstUser();
	}
	
	userpass = getPass();
	if( userpass[5] == 255 && userpass[6] == 255 && userpass[7] == 255 && userpass[8] == 255) {
		firstPass();
	}
	
	
	tasksNum = 2; // declare number of tasks
	task tsks[2]; // initialize the task array
	tasks = tsks; // set the task array
	
	unsigned char i=0; // task counter
	tasks[i].state = menu_off;
	tasks[i].period = 75;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &tick_Menu;
	i++;
	
	tasks[i].state = motor_wait;
	tasks[i].period = 10;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &tick_Motor;
	
	TimerSet(5); // value set should be GCD of all tasks
	TimerOn();
	
	while(1) {
 		uint8_t val = data;

		while(!USART_IsSendReady(0));
		USART_Send(val, 0);  //send this value serially to arduino
		while(!USART_HasTransmitted(0));
		asm("nop");
	}
	return 0;
}
