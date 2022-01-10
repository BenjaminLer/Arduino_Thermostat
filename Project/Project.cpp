#include "Arduino.h"
#include "time.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "hd44780/HD44780.hpp"
#include "uartLib/uart.hpp"

#include <avr/eeprom.h>

// LED definition
#define LED1 PB5
#define LED2 PB4
#define LED3 PB3
#define LED4 PB2

// Button definition
#define SW1 PD3
#define SW2 PD2
#define SW3 PD1
#define SW4 PD0

// ADCIN
#define ADCIN0 PC0
#define ADCIN1 PC1

// UART definition
#define BAUD 9600
#define MYUBRR  F_CPU/16/BAUD-1

// Command code for UART communication
#define timeConfig			0x10
#define requestTemp			0x20
#define requestThreshold	0x30

// Definition of an enum for all the mode for the ADC
enum ADCMode {Temperature, Threshold, NotConfigured};

// Definition of an enum for the state machine
enum State {TimeDisplay, ThresholdDisplay, TemperatureDisplay};

// Definition of an enum for temperature and threshold
enum Acquisition {ReadTemperature, ReadThreshold};

// Functions prototype
void ADCInit(enum ADCMode mode);
void timerInit(void);
void interruptInit(void);
void customCharInit(void);
float convertTemperature(int adc);
void correctTime(void);
void pwm_init(void);
uint16_t ADCRead(void);
uint8_t EEPROM_read(unsigned int uiAddress);
void EEPROM_write(unsigned int uiAddress, unsigned char ucData);
void setThreshold(float threshold);
float getThreshold(void);
void float2Bytes(byte bytes_temp[4],float float_variable);

// Global variable
volatile uint8_t hour, minute, second;
volatile float temperature;
volatile enum State state;
volatile uint64_t timer1InterruptSinceStartup;
volatile uint64_t lastTimeSinceButtonPressed;

// Interrupt of Timer1 for counting seconds
ISR(TIMER1_OVF_vect) {
	timer1InterruptSinceStartup++;
	if(timer1InterruptSinceStartup%4 == 0) {
		second++;
		correctTime();
	}
}

// Interrupt on data reception in order to execute some specific actions
ISR(USART_RX_vect) {
	uint8_t dataReceived[3];

	// Reading the first byte of data for getting the command code
	// We don't need to wait for data to be available due to how the interrupt work
	dataReceived[0] = UDR0;

	switch(dataReceived[0]) {
		// Update current time
		case timeConfig:
			// Read the 3 bytes for time information
			for(int i = 0; i < 3; i++) {
				while(!(UCSR0A & (1<<RXC0)));
				dataReceived[i] = UDR0;
			}

			hour = dataReceived[0];
			minute = dataReceived[1];
			second = dataReceived[2];
			break;

		// Request current temperature
		case requestTemp:
			char temperatureUART[10];
			dtostrf(temperature, 2, 2, temperatureUART);
			USART_Transmit_String(temperatureUART);
			break;

		// Request current threshold
		case requestThreshold:
			char thresholdUART[10];
			dtostrf(getThreshold(), 2, 2, thresholdUART);
			USART_Transmit_String(thresholdUART);
			break;

		default:
			USART_Transmit_String("Invalid command code");
			break;
	}
}

// Interrupt on Button 2 -> Threshold menu
ISR(INT0_vect) {
	// Ignore button interrupt if too fast
	if(lastTimeSinceButtonPressed == timer1InterruptSinceStartup) {
		return;
	}
	lastTimeSinceButtonPressed = timer1InterruptSinceStartup;

	// Check if button 2 pressed
	if((PIND & 1<<SW2) == 0) {
		// Disable interrupt on button pin
		EIMSK |= (0<<INT0);

		if(state == ThresholdDisplay) {
			state = TimeDisplay;
		}
		else {
			state = ThresholdDisplay;
		}
	}
}

// Interrupt on Button 1 -> Temperature menu
ISR(INT1_vect) {
	// Ignore button interrupt if too fast
	if(lastTimeSinceButtonPressed == timer1InterruptSinceStartup) {
		return;
	}
	lastTimeSinceButtonPressed = timer1InterruptSinceStartup;

	if((PIND & 1<<SW1) == 0) {
		EIMSK |= (0<<INT1);

		if(state == TemperatureDisplay) {
			state = TimeDisplay;
		}
		else {
			state = TemperatureDisplay;
		}
	}
}

int main(void) {
	// LED pins as output
	DDRB |= (1<<LED1) | (1<<LED2) | (1<<LED3) | (1<<LED4);

	// Button pins as input
	DDRD &=~ (1<<SW1) | (1<<SW2) | (1<<SW3) | (1<<SW4);

	// Pull up button pin
	PORTD |=  (1<<SW1) | (1<<SW2) | (1<<SW3) | (1<<SW4);

	// ADC pin as input
	DDRC &=~ (1<<ADCIN1);
	DDRC &=~ (1<<ADCIN0);

	// Variable definition
	char infoDisplay[20];
	char timeDisplay[20];

	enum Acquisition nextAcquisition = ReadTemperature;

	state = TimeDisplay;

	// Init time in second since startup
	timer1InterruptSinceStartup = 0;

	lastTimeSinceButtonPressed = 0;

	// Init time at 00:00:00
	hour = 0;
	minute = 0;
	second = 0;

	// Init functions
	pwm_init();
	timerInit();
	interruptInit();
	LCD_Initalize();
	init_uart(MYUBRR);
	customCharInit();

	PORTB |= ((1<<LED1) | (1<<LED2) | (1<<LED3) | (1<<LED4)); // Turn off all LEDs

	// Enable interrupt of RX
	UCSR0B |= (1<<RXCIE0);

	while(true) {
		switch(nextAcquisition) {
			case ReadTemperature: // Temperature acquisition
				ADCInit(Temperature);
				_delay_ms(100);

				temperature = convertTemperature(ADCRead());

				nextAcquisition = ReadThreshold;
				break;

			case ReadThreshold: // Threshold acquisition
				if(state == ThresholdDisplay) {
					ADCInit(Threshold);
					setThreshold((ADCRead() * 40.0) / 1024.0);
				}
				_delay_ms(100);

				nextAcquisition = ReadTemperature;
				break;
		}

		// Handle PWM
		if(temperature > getThreshold()) {
			// Set PWM ratio ratio to 0
			OCR2A = 0;

			// Disable PWM source
			TCCR2B &= ~(1<<CS21);

			// Set pin output to 0
			PORTB |= (1<<LED3);
		}
		else {
			// Enable PWM source
			TCCR2B |= (1<<CS21);

			// Set the PWM ratio depending on how far we are from the threshold
			OCR2A = (uint8_t)(255.0 - ((temperature * 255.0) / getThreshold()));

		}

		// Display measure
		switch(state) {
			case TimeDisplay: // Default mode -> only display time
				// Enable interrupt on button pin
				EIMSK |= (1<<INT0);
				EIMSK |= (1<<INT1);

				LCD_Clear();

				// Format time display HH:MM:SS
				sprintf(timeDisplay, "%02d:%02d:%02d", hour, minute, second);

				LCD_GoTo(0,0);
				LCD_WriteText("Current Time:");

				LCD_GoTo(0,1);
				LCD_WriteText(timeDisplay);
				break;

			case ThresholdDisplay: // Threshold display mode -> configure threshold
				// Enable interrupt on button pin
				EIMSK |= (1<<INT0);
				EIMSK |= (1<<INT1);

				LCD_Clear();

				dtostrf(getThreshold(), 2, 2, infoDisplay);

				LCD_GoTo(0,0);
				LCD_WriteText("Set Threshold :");

				LCD_GoTo(0,1);
				LCD_WriteText(infoDisplay);

				LCD_DisplayCustomChar(0);
				LCD_WriteText("C");
				break;

			case TemperatureDisplay: // Temperature display mode
				// Enable interrupt on button pin
				EIMSK |= (1<<INT0);
				EIMSK |= (1<<INT1);

				LCD_Clear();

				dtostrf(temperature, 2, 1, infoDisplay);

				LCD_GoTo(0,0);
				LCD_WriteText("Temperature: ");

				LCD_GoTo(0,1);
				LCD_WriteText(infoDisplay);

				//Display °C
				LCD_DisplayCustomChar(0);
				LCD_WriteText("C");
				break;
		}
	}
}

void ADCInit(enum ADCMode mode) {
	// ADC configuration register
	ADCSRA = (1<<ADEN) | (1<<ADPS0) | (1<<ADPS1) | (1<<ADPS2);
	//ADEN - ADC Enable
	//ADPS2:0 - pre-scaler (128 in this case)

	// multiplexer selection register depending on the mode
	switch(mode) {
		case Temperature:
			ADMUX = (1<<REFS1) | (1<<REFS0) | (1<<MUX0);
			break;

		case Threshold:
			ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<MUX0);
			break;

		default:
			break;
	}
}

void interruptInit(void) {
	// Interrupt upon receiving data by RX
	UCSR0B = (1 << RXCIE0) | (1 << RXEN0);

	// Enable timer1 overflow interrupt(TOIE1)
	TIMSK1 = (1 << TOIE1);

	// Configuration of interrupt on button 2
	EIMSK |= (1<<INT0);
	EICRA |= (1<<ISC01) | (0<<ISC00);

	// Configuration of interrupt on button 1
	EIMSK |= (1<<INT1);
	EICRA |= (1<<ISC11) | (0<<ISC10);

	// Enable interrupt
	sei();
}

void timerInit(void) {
	TCCR1A |= (1<<COM1B0);
	TCCR1B |= ((1<<CS10) | (1<<CS11)); // Set pre-scaller to 64
}

void customCharInit(void) {
	// Custom character for LCD screen
	char customDegreeChar[] = { // °
		  B00111,
		  B00101,
		  B00111,
		  B00000,
		  B00000,
		  B00000,
		  B00000,
		  B00000
		};

	LCD_RegisterCustomChar(customDegreeChar, 0);
}

float convertTemperature(int adc) {
	float temperature;
	temperature = (float)adc / 1024;
	temperature *= 1.1;
	return temperature * 100;
}

void correctTime(void) {
	if(second >= 60) {
		minute++;
		second = 0;
	}

	if(minute >= 60) {
		hour++;
		minute = 0;
	}

	if(hour >= 24) {
		hour = 0;
	}
}

uint16_t ADCRead(void) {
	ADCSRA |= (1<<ADSC);		//	ADC - Start Conversion
	while(ADCSRA & (1<<ADSC)); 	//	wait for finish of conversion

	return ADC;
}

void pwm_init()
{
    // initialize TCCR2 as: fast pwm mode, non inverting
    TCCR2A |= (1<<COM2A1) | (1<<COM2A0) | (1<<WGM21) | (1<<WGM20);
    TCCR2B |= (1<<CS21); // clkT2S/8 prescale
}

void EEPROM_write(unsigned int uiAddress, unsigned char ucData)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE));

	/* Set up address and Data Registers */
	EEAR = uiAddress;
	EEDR = ucData;

	/* Write logical one to EEMPE */
	EECR |= (1<<EEMPE);

	/* Start eeprom write by setting EEPE */
	EECR |= (1<<EEPE);
}

uint8_t EEPROM_read(unsigned int uiAddress)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE));

	/* Set up address register */
	EEAR = uiAddress;

	/* Start eeprom read by writing EERE */
	EECR |= (1<<EERE);

	/* Return data from Data Register */
	return EEDR;
}

void float2Bytes(byte bytes_temp[4],float float_variable) {
  memcpy(bytes_temp, (unsigned char*) (&float_variable), 4);
}

void setThreshold(float threshold) {
	byte thresholdArray[4];

	float2Bytes(&thresholdArray[0], threshold);

	for(int i = 0; i < 4; i++) {
		EEPROM_write(i, thresholdArray[i]);
	}
}

float getThreshold(void) {
	byte thresholdArray[4];

	for(int i = 0; i < 4; i++) {
		thresholdArray[i] = EEPROM_read(i);
	}

	float threshold = *(float *)&thresholdArray;
	return threshold;
}
