# gps-lock-indicator
Use an AVR microcontroller to monitor the serial stream from a GPS to look for 3D lock

This is the software part of a device which watches a GPS' serial
output stream for the lock status (in the $GPGSA message) and outputs
status appropriate for a bi-color LED, as well as signal level outputs.
It uses an Atmel Mega168 AVR (like that in an Arduino, although this is
not an Arduino sketch).

There is one pair of outputs intended to be connected to a bi-color
(red/green) LED.  The LED will blink based on the lock status:

* if there is no data from the GPS, long blink red (on 5 seconds, off 5 seconds)

* if there is data but no lock (or only 2D lock) blink red every second

* if there is 3D lock, blink green every second

There are two signal level outputs: one that goes high when there is 3D
lock, the other that goes low when there is 3D lock.

AVR connections:
* 	PB0	LED, bi-color
* 	PB1	LED, bi-color
* 	PC0	GOOD output	goes low when data is good
* 	PC1	GOOD output	goes high when data is good
*	PD1	TxD - debug
*	PD2	RxD - from GPS

The LED is connected with a 330 ohm resistor between pins PB0 and PB1

The file gpslock.out is the linked executable from avr-gcc, (etc), and the
hex file is ready to be burned to a Mega168 AVR (using, for example, avrdude).

