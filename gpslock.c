/*
 * Watch GPS data for a 3D lock with status output to a red/green LED and 
 * signal lines.
 *
 * This is targeted to the MegaAVR processor, built with avr-gcc.
 *
 * avr-gcc -I. -Wall -O1 -mmcu=atmega168 -DMHZ8 gpslock.c -o gpslock.out
 * avr-objcopy -O ihex gpslock.out gpslock.hex
 *
 * This looks for the GPS sentence $GPGSA.
 *   $GPGSA,A,1
 *
 * Indicate status on a red/green LED:
 * 	- no data  		blink red every 5 seconds
 * 	- data, no lock		blink red every second
 * 	- lock			blink green every second
 *
 * 	PB0	LED, bi-color
 * 	PB1	LED, bi-color
 * 	PC0	GOOD output	goes low when data is good
 * 	PC1	GOOD output	goes high when data is good
 *	PD1	TxD - debug
 *	PD2	RxD - from GPS
 *
 * 	The code messes with DDRB so the GOOD outputs are put on port C 
 * 
 * Copyright 2015-2016 Glen Overby
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * 
 */

#define F_CPU 8000000UL
#ifndef MHZ8
#define MHZ8
#endif
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <string.h>

volatile unsigned char	events;
#define	E_CLOCK	0x01

void tx(unsigned char c);

/*
 * The clock interrupt is currently not used.
 */
//ISR(SIG_OUTPUT_COMPARE0A)	- deprecated in 4.8.1
ISR(TIMER0_COMPA_vect)
{
	events |= E_CLOCK;
}


/*
 * MegaAVR USART as a serial port
 */
void
tx(unsigned char c)
{
	while ( !(UCSR0A & _BV(UDRE0)) )
		;
	UDR0 = c & 0x7f;
}

char
qrx()
{
	if (UCSR0A & _BV(RXC0))
		return (UDR0);
	else
		return 0;
}

void
txs(char *s)
{
	while (*s) {
		tx(*s);
		s++;
	}
}

/*
 */
void
ioinit()
{
	/*
	 * Set system clock divisor
	 */
#ifdef MHZ8
	CLKPR = _BV(CLKPCE);
	CLKPR = 0x0;		/* Divisor = 0 sets to 8mhz */
#endif
	/*
	 *  UBBRn = Fosc / 16 BAUD -1
	 *	1,000,000 /(16 * 19200) - 1 = 2.255
	 */
	//UBRR0 = 12;		/* 4800 bps @ 1mhz */
#ifdef MHZ8
	UBRR0 = 51;		/* 9600 bps @ 8mhz */
#endif
#ifdef MHZ1
	UBRR0 = 6;		/* 9600 bps @ 1mhz */
#endif
	//UBRR0 = 2;		/* 19200 bps @ 1mhz  -- WRONG */
	//UBRR0 = 25;		/* 19200 bps @ 8mhz */
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);
	UCSR0C = _BV(USBS0) | _BV(UCSZ00) | _BV(UCSZ01); /* 2 Stop, 8 data,  */

	/*
	 * I/O Ports
	 *	Output = 1
	 */

	PORTB = 0;
	DDRB = 0xf;

	PORTC = 0x01;
	DDRC = 0x3;

	/*
	 * Timer 0
	 * CTC mode: counter is cleared when it matches the OCR0A register 
	 */
#ifdef MHZ8
#ifdef HZ200 	/* 200hz */
	TCCR0A = 0x2;			/* CTC mode */
	TCCR0B = 0x04;			/* clk/256 */
	OCR0A = 156;			/* 8M / 256 = 31250 / 200 = 156 */
	TCNT0 = 0;
#else	/* 1 hz */
	OCR0A = 223;			/* 8Mhz / 1024 = 7812 / 223 = 35 */
	TCCR0A = _BV(WGM01);		/* CTC mode is WGM01: = 010 */
	TCCR0B = 0x5;			/* clk/1024 */
	TCNT0 = 0;
#endif
#elif defined MHZ1
#ifdef HZ200	/* 200 hz */
	OCR0A = 78;			/* 1Mhz / 64 = 15625 / 200 = 78.125 */
	TCCR0A = _BV(WGM01);		/* CTC mode is WGM01: = 010 */
	TCCR0B = 0x3;			/* clk/64 */
	TCNT0 = 0;
#else /* 1 hz */
	OCR0A = 244;			/* 1Mhz / 1024 = 976 / 244 = 4 */
	TCCR0A = _BV(WGM01);		/* CTC mode is WGM01: = 010 */
	TCCR0B = 0x5;			/* clk/1024 */
	TCNT0 = 0;
#endif
#endif

	sei();
}

int
main()
{
	char c;
	char tick;			// flag: clock tick
	int unlock;			// counter - no serial input
	unsigned char blink, hz;
	unsigned char state;		// What to blink and how often:	
					//   1 - lock - blink green
					//   2 - no lock - blink red
					//   3 - no data - blink red 5 seconds
	char statecount;		// counter for state
#define LINESIZE 128
	char line[LINESIZE];
	unsigned char l;		// pointer in line

	l = 0;
	hz = 0;
	blink = 0;
	tick = 0;
	unlock = 0;
	state = 3;			// start in no data state
	statecount = 0;

	ioinit();
	tx('!');

	while (1) {
		sei();
		/*
		 * Poll for a timer0 compare match (4hz clock)
		 */
		if (TIFR0 & (_BV(OCF0A))) {
			TIFR0 |= (_BV(OCF0A));
			tick = 1;
			// count of timer ticks with no data received.
			// Switch to no data when this occurs (60 seconds)
			if (++unlock == 1800) {
				state = 3;
				PORTC = 0x1;	/* UNlock */
			}
		}

		if ((c = qrx())) {
#if 0
			/* Used during development to test port configuration */
			//tx(c);
			if (c == '0') {
				PORTB = 0;
			}
			if (c == '1') {
				PORTB = 1;
			}
			if (c == '2') {
				PORTB = 2;
			}
			if (c == 'a') {
				state = 1;
			}
			if (c == 'b') {
				state = 2;
			}
			if (c == 'c') {
				state = 3;
			}
#endif
			line[l] = c;
			l++;
			if (l >= LINESIZE || c == '\n' || c == '\r') {
				//tx('?');
				if(!strncmp_P(line,PSTR("$GPGSA"),6)) {
					unlock = 0;	// reset data loss ctr
					//tick = 2;	// clock tick
					//TCNT1 = 0;	// reset cpu timer
					tx('=');
					if (l > 9) {
						c = line[9];
					} else {
						c = '.';
					}
					tx(c);
					if (c == '3') {
						state = 1;
						PORTC = 0x2;	/* lock */
					} else {
						state = 2;
						PORTC = 0x1;	/* UNlock */
					}
				}
				l = 0;
			}
		} 

#ifdef MHZ8
#define	COUNT	30
#elif MHZ1
#define	COUNT	4
#endif
		if (tick) {
			hz++;
			if (tick > 1 || hz >= COUNT) {
				//tx('@');
				if (state == 1) {	// Lock: blink green
					PORTB = 1;
					blink = ~blink;
					DDRB = blink;
				} else if(state == 2) {	// no lock: blink red
					PORTB = 2;
					blink = ~blink;
					DDRB = blink;
				} else if(state == 3) { // no data: blink red 5s
					PORTB = 2;
					if (++statecount >= 5) {
						blink = ~blink;
						DDRB = blink;
						statecount = 0;
					}
				}
				hz = 0;
			}
			tick = 0;
		}

	}
}

