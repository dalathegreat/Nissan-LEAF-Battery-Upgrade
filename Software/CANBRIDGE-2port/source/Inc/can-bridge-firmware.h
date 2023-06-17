#if !defined(can_bridge_firmware_h)
#define can_bridge_firmware_h

#include "main.h"
#include "can.h"

//Leaf-specific defines
#define CHARGING_QUICK_START	0x40
#define CHARGING_QUICK			0xC0
#define CHARGING_QUICK_END		0xE0
#define CHARGING_SLOW			0x20
#define CHARGING_IDLE			0x60
//ZE0 LB_RCHGTIM defines
#define QUICK_CHARGE			0u
#define NORMAL_CHARGE_200V_100	9u
#define NORMAL_CHARGE_200V_80	10u
#define NORMAL_CHARGE_100V_100	17u
#define NORMAL_CHARGE_100V_80	18u

#define		SHIFT_P		0x00
#define		SHIFT_D		0x40
#define		SHIFT_N		0x30
#define		SHIFT_R		0x20

volatile extern uint8_t My_Battery;
volatile extern uint8_t My_Leaf;

void can_handler(uint8_t can_bus, CAN_FRAME *frame);
void one_second_ping( void );

#endif
