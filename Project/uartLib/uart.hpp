#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

void init_uart(unsigned short ubrr);
unsigned char USART_Receive( void );
void USART_Transmit_TEST();
void USART_Transmit_Byte( unsigned char data);
void USART_Transmit_String( char* str);
