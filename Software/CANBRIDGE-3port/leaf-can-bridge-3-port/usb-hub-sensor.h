
/*
             LUFA Library
     Copyright (C) Dean Camera, 2013.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for VirtualSerial.c.
 */

#ifndef _VIRTUALSERIAL_H_
#define _VIRTUALSERIAL_H_

	#define NOP __asm__ __volatile__ ("nop\n\t");
	#define WAIT12  NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP

	#define shunt_PIN	(1 << 3)
	
	#define RESET_PIN	(1 << 1)

	#define LED_PIN		(1 << 3)
	#define LED2_PIN	(1 << 4)
	
	#define swin_PIN	(1 << 4)
	#define swbat_PIN	(1 << 5)

	#define ADC_MUX_Vsense1_bm		0b00000000	//ADC0
	#define ADC_MUX_Vref_bm			0b00001000  //ADC1
	#define ADC_MUX_GND_bm			0b00010000  //ADC2
	#define ADC_MUX_Vense2_bm		0b00011000  //ADC3
	#define ADC_MUX_OUTA_bm			0b00110000  //ADC6
	#define ADC_MUX_OUTB_bm			0b00111000  //ADC7
	#define ADC_MUX_INT1V_bm		0b00001000
	#define ADC_MUX_TEMP_bm			0b00000000

	#define reset()		CPU_CCP = CCP_IOREG_gc; RST.CTRL = RST_SWRST_bm

	#define LEDTOGGLE()	PORTD.OUTTGL = LED_PIN;
	#define LEDON()		PORTD.OUTSET = LED_PIN;
	#define LEDOFF()	PORTD.OUTCLR = LED_PIN;
	
	#define LED2TOGGLE()	PORTC.OUTTGL = LED2_PIN;
	#define LED2ON()		PORTC.OUTSET = LED2_PIN;
	#define LED2OFF()		PORTC.OUTCLR = LED2_PIN;
	
	#define TWI_SLAVE_NACK_gc (1 << 2)

	#define F_CPU 32000000

	/* Includes: */
	#include <avr/io.h>
	#include <avr/wdt.h>
	#include <avr/power.h>
	#include <avr/interrupt.h>
	#include <avr/pgmspace.h>
	#include <avr/eeprom.h>
	#include <string.h>
	#include <stdio.h>
	#include <stddef.h>
	
	#include "Descriptors.h"
	#include "LUFA/Platform/Platform.h"
	#include "LUFA/Drivers/USB/USB.h"
	#include "sp_driver.h"

	/* Function Prototypes: */
	void SetupHardware(void);
	void ProcessCDCCommand(void);
	void EVENT_USB_Device_Disconnect(void);
	void EVENT_USB_Device_ConfigurationChanged(void);
	void EVENT_USB_Device_ControlRequest(void);
	uint8_t ReadCalibrationByte(uint8_t index);
	void toDecimal(uint16_t number, char * string);
	void toDecimalRev(uint16_t number, char * string);
	void NVM_GetGUID(uint8_t * b); 
	void setupDMA(uint16_t transfercount);
	
	//0x42 data specification definitions
	struct Data_Specification_t {
		uint8_t					num_channels;
		uint16_t				num_samples;
		uint32_t				reserved : 24;
		uint8_t					data_specification;
		uint8_t					channel_specification[8];
	};
	
	//data_specification defines
	#define	COMMON_ENCRYPTION_ENABLED_gc	(1 << 7)
	#define COMMON_PACKED_gc				(1 << 4)
	#define COMMON_LITTLE_ENDIAN_gc			(0 << 3)
	#define COMMON_BIG_ENDIAN_gc			(1 << 3)
	#define COMMON_FAST_DATA_gc				(1 << 2)
	#define COMMON_SLOW_DATA_gc				(0 << 2)
	
	//channel_specification defines
	#define CHANNEL_TWOS_COMPLEMENT_gc		(1 << 7)
	#define CHANNEL_12_BIT_gc				(12 << 2)
	#define CHANNEL_13_BIT_gc				(13 << 2)
	#define CHANNEL_14_BIT_gc				(14 << 2)
	#define CHANNEL_15_BIT_gc				(15 << 2)
	#define CHANNEL_16_BIT_gc				(16 << 2)
	#define CHANNEL_1_BYTE_gc				(1 << 0)
	#define CHANNEL_2_BYTE_gc				(1 << 1)
	
	//0x43 data attributes definitions
	enum unit_specifier_t {
		UNIT_NANO_AMPERE	= 0,
		UNIT_MICRO_AMPERE	= 1,
		UNIT_MILLI_AMPERE	= 2,
		UNIT_AMPERE			= 3,
		UNIT_NANO_VOLT		= 4,
		UNIT_MICRO_VOLT		= 5,
		UNIT_MILLI_VOLT		= 6,
		UNIT_VOLT			= 7,
		UNIT_MILLI_KELVIN	= 8,
		UNIT_CENTI_KELVIN	= 9,
		UNIT_DECI_KELVIN	= 10,
		UNIT_KELVIN			= 11
	};
	
	struct Data_Attributes_t {
		uint8_t					identifier_length;
		uint16_t				calibration_length;
		enum unit_specifier_t	unit;		
		uint16_t				calibration;
		char					identifier[];
	};
	
	
#endif