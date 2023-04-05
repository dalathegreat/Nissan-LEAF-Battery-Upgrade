#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stddef.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <avr/eeprom.h>
#include "Descriptors.h"
#include "LUFA/Platform/Platform.h"
#include "LUFA/Drivers/USB/USB.h"
#include "sp_driver.h"
#include "mcp25xx.h"
#include "user_defines.h"
#include "nissan_can_structs.h"
#include "helper_functions.h"

//function prototypes
void hw_init(void);
void send_can(uint8_t can_bus, can_frame_t frame);
void send_can1(can_frame_t frame);
void send_can2(can_frame_t frame);
void send_can3(can_frame_t frame);
void can_handler(uint8_t can_bus);
void check_can1(void);
void check_can2(void);
void check_can3(void);

//defines
#define TC0_CLKSEL_DIV1_gc		0b0001
#define TC0_CLKSEL_DIV256_gc	0b0110
#define TC0_CLKSEL_DIV1024_gc	0b0111
#define TC0_OVFINTLVL_HI_gc		0b11
#define TC0_OVFINTLVL_LO_gc		0b01
#define TC0_CCAINTLVL_HI_gc		0x03
#define TC0_CCBINTLVL_HI_gc		0x0C
#define TC0_CCCINTLVL_HI_gc		0x30
#define TC0_WGMODE_SINGLESLOPE_bm	0x03
#define CHARGING_QUICK_START	0x40
#define CHARGING_QUICK			0xC0
#define CHARGING_QUICK_END		0xE0
#define CHARGING_SLOW			0x20
#define CHARGING_IDLE			0x60
#define QUICK_CHARGE			0b00000
#define NORMAL_CHARGE_200V_100	0b01001
#define NORMAL_CHARGE_200V_80	0b01010
#define NORMAL_CHARGE_100V_100	0b10001
#define NORMAL_CHARGE_100V_80	0b10010
#define	SHIFT_P					0x00
#define	SHIFT_D					0x40
#define	SHIFT_N					0x30
#define	SHIFT_R					0x20
#define TXBUFFER_SIZE			16