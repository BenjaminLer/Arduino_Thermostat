#include "uart.hpp"

void init_uart(unsigned short ubrr  ) {
    // setting the baud rate  based on the datasheet
    UBRR0H =(unsigned char)  ( ubrr>> 8);  // 0x00
    UBRR0L =(unsigned char) ubrr;  // 0x0C
    // enabling TX & RX
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);
    //UCSR0A = (1<<UDRE0)|(1<<U2X0);
    UCSR0C =  (1 << UCSZ01) | (1 << UCSZ00);    // Set frame: 8data, 1 stop
}

unsigned char USART_Receive( void ) {
    /* Wait for data to be received */
    while ( !(UCSR0A & (1<<RXC0)) );
    /* Get and return received data from buffer */
    return UDR0;
}

void USART_Transmit_TEST() {
	for (int j = 48; j < 58; j++){
		USART_Transmit_Byte((unsigned char)j);
	}
	USART_Transmit_Byte((unsigned char)10);
	USART_Transmit_Byte((unsigned char)13);
}

void USART_Transmit_Byte( unsigned char data ) {
    /* Wait for empty transmit buffer */
    while ( !( UCSR0A & (1<<UDRE0)) );
    /* Put data into buffer, sends the data */
    UDR0 = data;
}

void USART_Transmit_String( char* str) {
	for (int j = 0; j < strlen(str) + 1; j++){
		USART_Transmit_Byte((unsigned char)str[j]);
	}
	USART_Transmit_Byte('\r');
	USART_Transmit_Byte('\n');
}
