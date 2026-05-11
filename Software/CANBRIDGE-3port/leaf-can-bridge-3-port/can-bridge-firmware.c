#include "can-bridge-firmware.h"

#define MY_BATTERY_24KWH    0
#define MY_BATTERY_30KWH    1
#define MY_BATTERY_40KWH    2
#define MY_BATTERY_62KWH    3

volatile uint8_t My_Battery = MY_BATTERY_24KWH; //Startup in 24kWh mode, autodetect actual battery type later on

#define MY_LEAF_2011        0       // Nissan Leaf ZE0 2010-2013 (light interior)
#define MY_LEAF_2014        1       // Nissan Leaf AZE0 2013-2017 (dark interior, old exterior style, 24/30kWh battery)

volatile uint8_t My_Leaf  = 1;			// Startup in AZE0 mode, switches to ZE0 if it detects older generation LEAF

#define MINPERCENTAGE 50 //Adjust this value to tune what realSOC% will display as 0% on the dash
#define MAXPERCENTAGE 950 //Adjust this value to tune what realSOC% will display as 100% on the dash

#define DISABLE_CAN3				//if CAN3 is not connected, enable this

// Charge timer minutes lookup values
#define time_100_with_200V_in_minutes 430
#define time_80_with_200V_in_minutes 340
#define time_100_with_100V_in_minutes 700
#define time_80_with_100V_in_minutes 560
#define time_100_with_QC_in_minutes 60
#define time_80_with_66kW_in_minutes 150
#define time_100_with_66kW_in_minutes 190

//Because the MCP25625 transmit buffers seem to be able to corrupt messages (see errata), we're implementing
//our own buffering. This is an array of frames-to-be-sent, FIFO. Messages are appended to buffer_end++ as they
//come in and handled according to buffer_pos until buffer_pos == buffer_end, at which point both pointers reset
//the buffer size should be well in excess of what this device will ever see
can_frame_t tx0_buffer[TXBUFFER_SIZE];
uint8_t		tx0_buffer_pos		= 0;
uint8_t		tx0_buffer_end		= 0;

can_frame_t tx2_buffer[TXBUFFER_SIZE];
uint8_t		tx2_buffer_pos		= 0;
uint8_t		tx2_buffer_end		= 0;

can_frame_t tx3_buffer[5];
uint8_t		tx3_buffer_pos		= 0;
uint8_t		tx3_buffer_end		= 0;

//Lookup table battery temperature,	offset -40C		0    1   2   3  4    5   6   7   8   9  10  11  12
uint8_t		temp_lut[13]						= {25, 28, 31, 34, 37, 50, 63, 76, 80, 82, 85, 87, 90};

//charging variables
volatile	uint8_t		charging_state			= 0;
volatile	uint8_t		max_charge_80_requested	= 0;
volatile	uint8_t		starting_up				= 0;

//other variables
#define LB_MIN_SOC 0
#define LB_MAX_SOC 1000
volatile	uint16_t	GIDS 					= 0; 
volatile	uint16_t	MaxGIDS					= 0;
volatile	uint8_t		can_busy				= 0;
volatile	uint8_t		battery_soc				= 0;
volatile	int16_t		dash_soc				= 0;
volatile	uint16_t	battery_soc_pptt		= 0;

volatile	uint8_t		swap_idx				= 0;
volatile	uint8_t		swap_5c0_idx			= 0;
volatile	uint8_t		VCM_WakeUpSleepCommand	= 0;
volatile	uint8_t		Byte2_50B				= 0;
volatile	uint8_t		ALU_question			= 0;
volatile	uint8_t		cmr_idx					= QUICK_CHARGE;

volatile	uint8_t		CANMASK					= 0;
volatile	uint8_t		skip_5bc				= 1; //for 2011 battery swap, skip 4 messages
volatile	uint8_t		alternate_5bc			= 0; //for 2011 battery swap
volatile	uint16_t	current_capacity_wh		= 0; //for 2011 battery swap
volatile	uint8_t		seen_1da				= 0; //for 2011 battery swap 
volatile	uint8_t		seconds_without_1f2		= 0; //bugfix: 0x603/69C/etc. isn't sent upon charge start on the gen1 Leaf, so we need to trigger our reset on a simple absence of messages
volatile	uint8_t		main_battery_temp		= 0; 
volatile	uint8_t		battery_can_bus			= 2; //keeps track on which CAN bus the battery talks. 
volatile	uint16_t	startup_counter_1DB		= 0;
volatile 	uint8_t		startup_counter_39X 	= 0;
//timer variables
volatile	uint16_t	sec_timer			= 1;	//actually the same as ms_timer but counts down from 1000

//bits															10					10					4				8				7			1				3	(2 space)		1					5						13
volatile	Leaf_2011_5BC_message swap_5bc_remaining	= {.LB_CAPR = 0x12C, .LB_FULLCAP = 0x32, .LB_CAPSEG = 0, .LB_AVET = 50, .LB_SOH = 99, .LB_CAPSW = 0, .LB_RLIMIT = 0, .LB_CAPBALCOMP = 1, .LB_RCHGTCON = 0b01001, .LB_RCHGTIM = 0};
volatile	Leaf_2011_5BC_message swap_5bc_full			= {.LB_CAPR = 0x12C, .LB_FULLCAP = 0x32, .LB_CAPSEG = 12, .LB_AVET = 50, .LB_SOH = 99, .LB_CAPSW = 1, .LB_RLIMIT = 0, .LB_CAPBALCOMP = 1, .LB_RCHGTCON = 0b01001, .LB_RCHGTIM = 0};
volatile	Leaf_2011_5BC_message leaf_40kwh_5bc;

volatile	Leaf_2011_5C0_message swap_5c0_max			= {.LB_HIS_DATA_SW = 1, .LB_HIS_HLVOL_TIMS = 0, .LB_HIS_TEMP_WUP = 66, .LB_HIS_TEMP = 66, .LB_HIS_INTG_CUR = 0, .LB_HIS_DEG_REGI = 0x02, .LB_HIS_CELL_VOL = 0x39, .LB_DTC = 0};
volatile	Leaf_2011_5C0_message swap_5c0_avg			= {.LB_HIS_DATA_SW = 2, .LB_HIS_HLVOL_TIMS = 0, .LB_HIS_TEMP_WUP = 64, .LB_HIS_TEMP = 64, .LB_HIS_INTG_CUR = 0, .LB_HIS_DEG_REGI = 0x02, .LB_HIS_CELL_VOL = 0x28, .LB_DTC = 0};
volatile	Leaf_2011_5C0_message swap_5c0_min			= {.LB_HIS_DATA_SW = 3, .LB_HIS_HLVOL_TIMS = 0, .LB_HIS_TEMP_WUP = 64, .LB_HIS_TEMP = 64, .LB_HIS_INTG_CUR = 0, .LB_HIS_DEG_REGI = 0x02, .LB_HIS_CELL_VOL = 0x15, .LB_DTC = 0};

volatile	can_frame_t		saved_5bc_frame				= {.can_id = 0x5BC, .can_dlc = 8};

//battery swap fixes
static		can_frame_t		ZE1_625_message		= {.can_id = 0x625, .can_dlc = 6, .data = {0x02,0x00,0xff,0x1d,0x20,0x00}}; // Sending 625 removes U215B [HV BATTERY]
static		can_frame_t		ZE1_5C5_message		= {.can_id = 0x5C5, .can_dlc = 8, .data = {0x40,0x01,0x2F,0x5E,0x00,0x00,0x00,0x00}};// Sending 5C5 Removes U214E [HV BATTERY]
static		can_frame_t		ZE1_5EC_message		= {.can_id = 0x5EC, .can_dlc = 1, .data = {0x00}};
static		can_frame_t		ZE1_355_message		= {.can_id = 0x355, .can_dlc = 8, .data = {0x14,0x0a,0x13,0x97,0x10,0x00,0x40,0x00}};
volatile	uint8_t			ticker40ms			= 0;
volatile	can_frame_t		ZE1_3B8_message		= {.can_id = 0x3B8, .can_dlc = 5, .data = {0x7F,0xE8,0x01,0x07,0xFF}}; // Sending 3B8 removes U1000 and P318E [HV BATTERY]
volatile	uint8_t			content_3B8			= 0;
volatile	uint8_t			flip_3B8			= 0;

static		can_frame_t		swap_605_message	= {.can_id = 0x605, .can_dlc = 1, .data = {0}};
static		can_frame_t		swap_607_message	= {.can_id = 0x607, .can_dlc = 1, .data = {0}};
static		can_frame_t		AZE0_45E_message	= {.can_id = 0x45E, .can_dlc = 1, .data = {0x00}};
static		can_frame_t		AZE0_481_message	= {.can_id = 0x481, .can_dlc = 2, .data = {0x40,0x00}};
volatile	uint8_t			PRUN_39X			= 0;

static		can_frame_t		AZE0_390_message	= {.can_id = 0x390, .can_dlc = 8, .data = {0x04,0x00,0x00,0x00,0x00,0x80,0x3c,0x07}}; // Sending removes P3196
static		can_frame_t		AZE0_393_message	= {.can_id = 0x393, .can_dlc = 8, .data = {0x00,0x10,0x00,0x00,0x20,0x00,0x00,0x02}}; // Sending removes P3196

void hw_init(void){
	uint8_t caninit;

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, 48000000);		
	
	//turn off everything we don' t use
	PR.PRGEN		= PR_AES_bm | PR_RTC_bm | PR_DMA_bm;
	PR.PRPA			= PR_ADC_bm | PR_AC_bm;
	PR.PRPC			= PR_TWI_bm | PR_USART0_bm | PR_HIRES_bm;
	PR.PRPD			= PR_TWI_bm | PR_USART0_bm | PR_TC0_bm | PR_TC1_bm;
	PR.PRPE			= PR_TWI_bm | PR_USART0_bm;
	
	//blink output
	PORTB.DIRSET	= 3;
	
	//start 16MHz crystal and PLL it up to 48MHz
	OSC.XOSCCTRL	= OSC_FRQRANGE_12TO16_gc |		//16MHz crystal
	OSC_XOSCSEL_XTAL_16KCLK_gc;						//16kclk startup
	OSC.CTRL	   |= OSC_XOSCEN_bm;				//enable crystal
	while(!(OSC.STATUS & OSC_XOSCRDY_bm));			//wait until ready
	OSC.PLLCTRL		= OSC_PLLSRC_XOSC_gc | 2;		//XTAL->PLL, 2x multiplier (32MHz)
	OSC.CTRL	   |= OSC_PLLEN_bm;					//start PLL
	while (!(OSC.STATUS & OSC_PLLRDY_bm));			//wait until ready
	CCP				= CCP_IOREG_gc;					//allow changing CLK.CTRL
	CLK.CTRL		= CLK_SCLKSEL_PLL_gc;			//use PLL output as system clock	
	
	//output 16MHz clock to MCP25625 chips (PE0)
	//next iteration: put this on some other port, pin  4 or 7, so we can use the event system
	TCE0.CTRLA		= TC0_CLKSEL_DIV1_gc;						//clkper/1
	TCE0.CTRLB		= TC0_CCAEN_bm | TC0_WGMODE_SINGLESLOPE_bm;	//enable CCA, single-slope PWM
	TCE0.CCA		= 1;										//compare value
	TCE0.PER		= 1;										//period of 1, generates 24MHz output
	
	PORTE.DIRSET	= PIN0_bm;									//set CLKOUT pin to output
	
	//setup CAN pin interrupts
	PORTC.INTCTRL	= PORT_INT0LVL_HI_gc;
	PORTD.INTCTRL	= PORT_INT0LVL_HI_gc | PORT_INT1LVL_HI_gc;	
	
	PORTD.INT0MASK	= PIN0_bm;						//PORTD0 has can1 interrupt
	PORTD.PIN0CTRL	= PORT_OPC_PULLUP_gc | PORT_ISC_LEVEL_gc;
	
	PORTD.INT1MASK	= PIN5_bm;						//PORTD5 has can2 interrupt
	PORTD.PIN5CTRL	= PORT_OPC_PULLUP_gc | PORT_ISC_LEVEL_gc;
	
	#ifndef DISABLE_CAN3
	PORTC.INT0MASK	= PIN2_bm;						//PORTC2 has can3 interrupt
	PORTC.PIN0CTRL	= PORT_OPC_PULLUP_gc | PORT_ISC_LEVEL_gc;
	#endif
	
	//buffer checking interrupt
	TCC1.CTRLA		= TC0_CLKSEL_DIV1_gc;			//32M/1/4800 ~ 100usec
	TCC1.PER		= 3200;
	TCC1.INTCTRLA	= TC0_OVFINTLVL_HI_gc;			//same priority as can interrupts
	
	//we want to optimize performance, so we're going to time stuff
	//48MHz/48=1us timer, which we just freerun and reset whenever we want to start timing something
	//frame time timer
	TCC0.CTRLA		= TC0_CLKSEL_DIV1_gc;
	TCC0.PER		= 32000;						//32MHz/32000=1ms
	TCC0.INTCTRLA	= TC0_OVFINTLVL_HI_gc;			//interrupt on overflow
	
	PORTB.OUTCLR	= (1 << 0);
	
	can_system_init:
			
	//Init SPI and CAN interface:
	if(RST.STATUS & RST_WDRF_bm){ //if we come from a watchdog reset, we don't need to setup CAN
		caninit = can_init(MCP_OPMOD_NORMAL, 1); //on second thought, we do
	} else {
		caninit = can_init(MCP_OPMOD_NORMAL, 1);
	}
	
	if(caninit){		
		//PORTB.OUTSET |= (1 << 0);					//green LED, uncommented to save power
	} else {		
		//PORTB.OUTSET |= (1 << 1);					//red LED
		_delay_ms(10);
		goto can_system_init;
	}
	
	//Set and enable interrupts with round-robin
	XMEGACLK_CCP_Write((void * ) &PMIC.CTRL, PMIC_RREN_bm | PMIC_LOLVLEN_bm | PMIC_HILVLEN_bm);//PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm| PMIC_HILVLEN_bm;
	
	USB_Init(USB_OPT_RC32MCLKSRC | USB_OPT_BUSEVENT_PRILOW);

	wdt_enable(WDTO_15MS);
	
	sei();
}

void reset_state(){
	charging_state = 0; //Reset charging state 
	startup_counter_1DB = 0;
	startup_counter_39X = 0;
}

int main(void){

	hw_init();

	while(1){
		//Setup complete, wait for can messages to trigger interrupts
	}
}

//fires every 1ms
ISR(TCC0_OVF_vect){	
	wdt_reset(); //Reset the watchdog
	sec_timer--; //Increment the 1000ms timer
	
	//fires every second (1000ms tasks go here)
	if(sec_timer == 0){
		PORTB.OUTCLR = (1 << 1);
		sec_timer = 1000;

		if(My_Leaf == MY_LEAF_2011)
		{
			seconds_without_1f2++;
			if(seconds_without_1f2 > 1)
			{
				reset_state();
			}
		}
		
	}
}

//fires approx. every 100us
ISR(TCC1_OVF_vect){
	check_can1();
	check_can2();
	check_can3();
}

//can1 interrupt
ISR(PORTD_INT0_vect){
	can_busy = 1;
	can_handler(1);
}

//can2 interrupt
ISR(PORTD_INT1_vect){
	can_busy = 1;
	can_handler(2);
}

//can3 receive interrupt
ISR(PORTC_INT0_vect){
	can_busy = 1;
	can_handler(3);
}

//VCM side of the CAN bus (in Muxsan)
void can_handler(uint8_t can_bus){
	can_frame_t frame;
	uint16_t temp2; //Temporary variable used in many functions
	uint8_t flag = can_read(MCP_REG_CANINTF, can_bus);
		
	if (flag & (MCP_RX0IF | MCP_RX1IF)){

		if(flag & MCP_RX1IF){ //prioritize the rollover buffer
			can_read_rx_buf(MCP_RX_1, &frame, can_bus);
			can_bit_modify(MCP_REG_CANINTF, MCP_RX1IF, 0x00, can_bus);
			} else {
			can_read_rx_buf(MCP_RX_0, &frame, can_bus);
			can_bit_modify(MCP_REG_CANINTF, MCP_RX0IF, 0x00, can_bus);
		}
		
		switch(frame.can_id){				
			case 0x1ED:
				//this message is only sent by 62kWh packs. We use this info to autodetect battery size
				My_Battery = MY_BATTERY_62KWH;
			break;
			case 0x5B9:
				send_can(battery_can_bus, AZE0_45E_message); // 500ms
				send_can(battery_can_bus, AZE0_481_message); // 500ms
				if (My_Leaf == MY_LEAF_2011)
				{
					frame.can_dlc = 7;
					if(charging_state == CHARGING_SLOW)
					{
						frame.data[5] = 0xFF;
					}
					else
					{
						frame.data[5] = 0x4A;
					}
	
					frame.data[6] = 0x80;
				}
			break;
			case 0x5EB:
				//This message is sent by 40/62kWh packs.
				if (My_Battery == MY_BATTERY_62KWH)
				{
					//Do nothing, we already identified the battery
				}
				else
				{
					My_Battery = MY_BATTERY_40KWH;
				}
			break;
			case 0x1DA:

				if( My_Leaf == MY_LEAF_2011 )
				{
					seen_1da = 10; // this variable is used to make the motor controller happy on shutdown
				}

			break;
			case 0x284: // Hacky way of generating missing inverter message

				// Upon reading VCM originating 0x284 every 20ms, send the missing message(s) to the inverter every 40ms
				ticker40ms++;
			
				if (ticker40ms > 1)
				{
					ticker40ms = 0;
					send_can(battery_can_bus, ZE1_355_message); // 40ms
				}

			break;
			case 0x1DB:
				battery_can_bus = can_bus; // Guarantees that messages go to right bus. Without this there is a risk that the messages get sent to VCM instead of HVBAT

				if (frame.data[2] == 0xFF)
				{
					starting_up |= 1;
				}
				else
				{
					starting_up &= ~1;
				}
			
				if( My_Leaf == MY_LEAF_2011 )
				{
					// seems like we just need to clear any faults and show permission
					if (VCM_WakeUpSleepCommand == 3)
					{                                                  // VCM command: wakeup
						frame.data[3] = (frame.data[3] & 0xD7) | 0x28; // FRLYON=1, INTERLOCK=1
					}
					if (startup_counter_1DB >= 100 && startup_counter_1DB <= 300) // Between 1s and 3s after poweron
					{
						frame.data[3] = (frame.data[3] | 0x10); // Set the full charge flag to ON during startup
					}											  // This is to avoid instrumentation cluster scaling bars incorrectly					
					if(startup_counter_1DB < 1000)
					{
						startup_counter_1DB++;
					}
				}

				if( My_Leaf == MY_LEAF_2014 )
				{
					//Calculate the SOC% value to send to the dash (Battery sends 10-95% which needs to be rescaled to dash 0-100%)
					dash_soc = LB_MIN_SOC + (LB_MAX_SOC - LB_MIN_SOC) * (1.0 * battery_soc_pptt - MINPERCENTAGE) / (MAXPERCENTAGE - MINPERCENTAGE);
					if (dash_soc < 0)
					{ //avoid underflow
						dash_soc = 0;
					}
					if (dash_soc > 1000)
					{ //avoid overflow
						dash_soc = 1000;
					}
					dash_soc = (dash_soc/10);
					frame.data[4] = (uint8_t) dash_soc;  //If this is not written, soc% on dash will say "---"
				}

				if(max_charge_80_requested)
				{
					if((charging_state == CHARGING_SLOW) && (battery_soc > 80))
					{
						frame.data[1] = (frame.data[1] & 0xE0) | 2; //request charging stop
						frame.data[3] = (frame.data[3] & 0xEF) | 0x10; //full charge completed
					}
				}

				calc_crc8(&frame);
			break;
			case 0x50A:
				if(frame.can_dlc == 6)
				{	//On ZE0 this message is 6 bytes long
					My_Leaf = MY_LEAF_2011;
					//We extend the message towards the battery
					frame.can_dlc = 8;
					frame.data[6] = 0x04;
					frame.data[7] = 0x00;
				}
				else if(frame.can_dlc == 8)
				{	//On AZE0 and ZE1, this message is 8 bytes long
					My_Leaf = MY_LEAF_2014;
				}
				break;
				case 0x380:
				if(can_bus == battery_can_bus)
				{
					//message is originating from a 40/62kWh pack when quickcharging
				}
				else
				{
					//message is originating from ZE0 OBC (OR ZE1!)
					My_Leaf = MY_LEAF_2011;
				}
			break;
			case 0x50B:

				VCM_WakeUpSleepCommand = (frame.data[3] & 0xC0) >> 6;
				Byte2_50B = frame.data[2];
			
				if( My_Leaf == MY_LEAF_2011 )
				{
					CANMASK = (frame.data[2] & 0x04) >> 2;
					frame.can_dlc = 7; //Extend the message from 6->7 bytes
					frame.data[6] = 0x00;
				}

				break;

				case 0x50C: // Fetch ALU and send lots of missing 100ms messages towards battery

				ALU_question = frame.data[4];
				send_can(battery_can_bus, ZE1_625_message); // 100ms
				send_can(battery_can_bus, ZE1_5C5_message); // 100ms
				send_can(battery_can_bus, ZE1_3B8_message); // 100ms

				if( My_Leaf == MY_LEAF_2011 ) // ZE0 OBC messages wont satisfy the battery. Send 2013+ PDM messages towards it!
				{

					if(startup_counter_39X < 250){
						startup_counter_39X++;
					}
					AZE0_390_message.data[0] = 0x04;
					AZE0_390_message.data[3] = 0x00;
					AZE0_390_message.data[5] = 0x80;
					AZE0_390_message.data[6] = 0x3C;
					AZE0_393_message.data[1] = 0x10;

					if(startup_counter_39X > 13){
						AZE0_390_message.data[3] = 0x02;
						AZE0_390_message.data[6] = 0x00;
						AZE0_393_message.data[1] = 0x20;
					}
					if(startup_counter_39X > 15){
						AZE0_390_message.data[3] = 0x03;
						AZE0_390_message.data[6] = 0x00;
					}
					if((startup_counter_39X > 20) && (Byte2_50B == 0)){ //Shutdown
						AZE0_390_message.data[0] = 0x08;
						AZE0_390_message.data[3] = 0x01;
						AZE0_390_message.data[5] = 0x90;
						AZE0_390_message.data[6] = 0x00;
						AZE0_393_message.data[1] = 0x70;
					}

					PRUN_39X = (PRUN_39X + 1) % 3;
					AZE0_390_message.data[7] = (PRUN_39X << 4);
					AZE0_393_message.data[7] = (PRUN_39X << 4);
					calc_checksum4(&AZE0_390_message);
					calc_checksum4(&AZE0_393_message);
					send_can(battery_can_bus, AZE0_390_message); // 100ms
					send_can(battery_can_bus, AZE0_393_message); // 100ms
				}

				content_3B8 = (content_3B8 + 1) % 15;
				ZE1_3B8_message.data[2] = content_3B8; // 0 - 14 (0x00 - 0x0E)

				if (flip_3B8)
				{
					flip_3B8 = 0;
					ZE1_3B8_message.data[1] = 0xC8;
				}
				else
				{
					flip_3B8 = 1;
					ZE1_3B8_message.data[1] = 0xE8;
				}

				break;

				case 0x55B:

				if (ALU_question == 0xB2)
				{
					frame.data[2] = 0xAA;
				}
				else
				{
					frame.data[2] = 0x55;
				}

				if( My_Leaf == MY_LEAF_2011 )
				{
					if (CANMASK == 0)
					{
						frame.data[6] = (frame.data[6] & 0xCF) | 0x20;
					}
					else
					{
						frame.data[6] = (frame.data[6] & 0xCF) | 0x10;
					}
				}

				battery_soc_pptt = (uint16_t) ((frame.data[0] << 2) | ((frame.data[1] & 0xC0) >> 6)); //Capture SOC% 0-100.0%
				battery_soc = (uint8_t) (battery_soc_pptt / 10); // Capture SOC% 0-100

				calc_crc8(&frame);

			break;
			case 0x5BC:
				if(frame.data[0] != 0xFF){
					starting_up &= ~4;
					} else {
					starting_up |= 4;
				}
				
				if( My_Leaf == MY_LEAF_2011 )
				{
				temp2 = ((frame.data[4] & 0xFE) >> 1); //Collect SOH value
				if(frame.data[0] != 0xFF){ //Only modify values when GIDS value is available, that means LBC has booted
					if((frame.data[5] & 0x10) == 0x00){ //If everything is normal (no output power limit reason)
						convert_array_to_5bc(&leaf_40kwh_5bc, (uint8_t *) &frame.data);
						swap_5bc_remaining.LB_CAPR = leaf_40kwh_5bc.LB_CAPR;
						swap_5bc_full.LB_CAPR = leaf_40kwh_5bc.LB_CAPR;
						swap_5bc_remaining.LB_SOH = temp2;
						swap_5bc_full.LB_SOH = temp2;
						current_capacity_wh = swap_5bc_full.LB_CAPR * 80;
						main_battery_temp = frame.data[3] / 20;					
						main_battery_temp = temp_lut[main_battery_temp] + 1;
						} else { //Output power limited

					}
				
					} else {
					swap_5bc_remaining.LB_CAPR = 0x3FF;
					swap_5bc_full.LB_CAPR = 0x3FF;
					swap_5bc_remaining.LB_RCHGTIM = 0;
					swap_5bc_remaining.LB_RCHGTCON = 0;
				}
				
				if(startup_counter_1DB < 600) // During the first 6s of bootup, write GIDS to the max value for the pack
				{
					switch (My_Battery)
					{
						case MY_BATTERY_24KWH:
						swap_5bc_remaining.LB_CAPR = 220;
						swap_5bc_full.LB_CAPR = 220;
						break;
						case MY_BATTERY_30KWH:
						swap_5bc_remaining.LB_CAPR = 310;
						swap_5bc_full.LB_CAPR = 310;
						break;
						case MY_BATTERY_40KWH:
						swap_5bc_remaining.LB_CAPR = 420;
						swap_5bc_full.LB_CAPR = 420;
						break;
						case MY_BATTERY_62KWH:
						swap_5bc_remaining.LB_CAPR = 630;
						swap_5bc_full.LB_CAPR = 630;
						break;
					}
				}
			
				skip_5bc--;
				if(!skip_5bc){
					switch(cmr_idx){
						case QUICK_CHARGE:
						//swap_5bc_remaining.LB_RCHGTIM = 8191; //1FFFh is unavailable value
						swap_5bc_full.LB_RCHGTIM = 0x003C; //60 minutes remaining
						swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
						swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
						swap_5bc_full.LB_RCHGTCON = cmr_idx;
						cmr_idx = NORMAL_CHARGE_200V_100;
						break;
						case NORMAL_CHARGE_200V_100:
						swap_5bc_remaining.LB_RCHGTIM = (time_100_with_200V_in_minutes - ((time_100_with_200V_in_minutes * battery_soc)/100));
						swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
						swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
						swap_5bc_full.LB_RCHGTCON = cmr_idx;
						cmr_idx = NORMAL_CHARGE_200V_80;
						break;
						case NORMAL_CHARGE_200V_80:
						if(battery_soc > 80){swap_5bc_remaining.LB_RCHGTIM = 0;}
						else{swap_5bc_remaining.LB_RCHGTIM = (time_80_with_200V_in_minutes - ((time_80_with_200V_in_minutes * battery_soc)/100)); }
						swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
						swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
						swap_5bc_full.LB_RCHGTCON = cmr_idx;
						cmr_idx = NORMAL_CHARGE_100V_100;
						break;
						case NORMAL_CHARGE_100V_100:
						swap_5bc_remaining.LB_RCHGTIM = (time_100_with_100V_in_minutes - ((time_100_with_100V_in_minutes * battery_soc)/100));
						swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
						swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
						swap_5bc_full.LB_RCHGTCON = cmr_idx;
						cmr_idx = NORMAL_CHARGE_100V_80;
						break;
						case NORMAL_CHARGE_100V_80:
						if(battery_soc > 80){swap_5bc_remaining.LB_RCHGTIM = 0;}
						else{swap_5bc_remaining.LB_RCHGTIM = (time_80_with_100V_in_minutes - ((time_80_with_100V_in_minutes * battery_soc)/100)); }
						swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
						swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
						swap_5bc_full.LB_RCHGTCON = cmr_idx;
						cmr_idx = QUICK_CHARGE;
						break;
					}

					swap_5bc_remaining.LB_AVET = main_battery_temp;
					swap_5bc_full.LB_AVET = main_battery_temp;
					
					//ZE0 LEAF just cannot cope with capacities >24kWh
					//when quick charging, we change capacity to reflect a proportion of 21.3kWh (266 GIDs)
                    if( My_Battery != MY_BATTERY_24KWH )
                    {
	                    if (charging_state == CHARGING_QUICK)
	                    {
		                    temp2 = battery_soc * 300; // e.g. 55 * 300 = 16500
		                    temp2 = temp2 / 100;            // e.g. 16500/100 = 165
		                    swap_5bc_remaining.LB_CAPR = temp2;
		                    swap_5bc_full.LB_CAPR = temp2;
	                    }
                    }
				
					if(alternate_5bc){
						convert_5bc_to_array(&swap_5bc_remaining, (uint8_t *) &saved_5bc_frame.data);
						alternate_5bc = 0;
					} else {
						convert_5bc_to_array(&swap_5bc_full, (uint8_t *) &saved_5bc_frame.data);
						alternate_5bc = 1;
					}
				
				
					skip_5bc = 5;
					}
			
					frame = saved_5bc_frame;
				}
				if( My_Leaf == MY_LEAF_2014 )
				{
				//5bc on 2014 looks extremely similar to 2018, except for the extra LB_MaxGIDS switch, so we remove and ignore that
                if ((frame.data[5] & 0x10) == 0x00)
                { //LB_MAXGIDS is 0, store actual GIDS remaining to this variable
	                GIDS = (uint16_t) ((frame.data[0] << 2) | ((frame.data[1] & 0xC0) >> 6));
                }
                //Avoid blinking GOM by always writing remaining GIDS
                frame.data[0] = (uint8_t)(GIDS >> 2);
                frame.data[1] = (GIDS << 6) & 0xC0;
				
				//Collect temperature for 5C0 message
				main_battery_temp = frame.data[3] / 20;                    // Temperature needed for charger section
				main_battery_temp = temp_lut[main_battery_temp] + 1;       // write main_battery_temp to be used by 5C0 message

				//Correct charge timer estimates
				//This code is WIP, currently the 3.3 and 6.6kW times are good, but 100V is messed up somehow. Seems to be differences in LEAF firmware
				cmr_idx = ((frame.data[5] & 0x03) << 3) | ((frame.data[6] & 0xE0) >> 5);
				switch(cmr_idx){
					case 0: //QC
					break;
					case 5: //6.6kW 100%
						temp2 = (time_100_with_66kW_in_minutes - ((time_100_with_66kW_in_minutes * battery_soc)/100));
						frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
						frame.data[7] = (temp2 & 0xFF);
					break;
					case 8: //200V 100%
						temp2 = (time_100_with_200V_in_minutes - ((time_100_with_200V_in_minutes * battery_soc)/100));
						frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
						frame.data[7] = (temp2 & 0xFF);
					break;
					case 11: //100V 100%
						temp2 = (time_100_with_100V_in_minutes - ((time_100_with_100V_in_minutes * battery_soc)/100));
						frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
						frame.data[7] = (temp2 & 0xFF);
					break;
					case 18: //6.6kW 80%
						if(battery_soc < 80)
						{
							temp2 = (time_80_with_66kW_in_minutes - ((time_80_with_66kW_in_minutes * (battery_soc+20))/100));
							frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
							frame.data[7] = (temp2 & 0xFF);
						}
						else
						{
							temp2 = 0; //0 since we are over 80% SOC
							frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
							frame.data[7] = (temp2 & 0xFF);
						}
					break;
					case 21: //200V 80%
						if(battery_soc < 80)
						{
							temp2 = (time_80_with_200V_in_minutes - ((time_80_with_200V_in_minutes * (battery_soc+20))/100));
							frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
							frame.data[7] = (temp2 & 0xFF);
						}
						else
						{
							temp2 = 0; //0 since we are over 80% SOC
							frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
							frame.data[7] = (temp2 & 0xFF);
						}
					break;
					case 24: //100V 80%
						if(battery_soc < 80)
						{
							temp2 = (time_80_with_100V_in_minutes - ((time_80_with_100V_in_minutes * (battery_soc+20))/100));
							frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
							frame.data[7] = (temp2 & 0xFF);
						}
						else
						{
							temp2 = 0; //0 since we are over 80% SOC
							frame.data[6] = (frame.data[6] & 0xE0) | (temp2 >> 8);
							frame.data[7] = (temp2 & 0xFF);
						}
					break;
					case 31: //Unavailable
					break;
					
					}
				
				}

				
				break;
			case 0x5C0: //Send 500ms messages here
				send_can(battery_can_bus, ZE1_5EC_message);//500ms
				
				swap_5c0_max.LB_HIS_TEMP = main_battery_temp;
				swap_5c0_max.LB_HIS_TEMP_WUP = main_battery_temp;
				swap_5c0_avg.LB_HIS_TEMP = main_battery_temp;
				swap_5c0_avg.LB_HIS_TEMP_WUP = main_battery_temp;
				swap_5c0_min.LB_HIS_TEMP = main_battery_temp;
				swap_5c0_min.LB_HIS_TEMP_WUP = main_battery_temp;
					
				if(swap_5c0_idx == 0){
					convert_5c0_to_array(&swap_5c0_max, (uint8_t *) &frame.data);
					} 
				else if(swap_5c0_idx == 1){
					convert_5c0_to_array(&swap_5c0_avg, (uint8_t *) &frame.data);
					}
				else {
					convert_5c0_to_array(&swap_5c0_min, (uint8_t *) &frame.data);
				}
					
				swap_5c0_idx = (swap_5c0_idx + 1) % 3;
				//This takes advantage of the modulus operator % to reset the value of swap_5c0_idx to 0 once it reaches 3.
				//It also eliminates the need for an if statement and a conditional check, which improves performance (but sacrifices readability)

				break;
			case 0x1F2:
 				//Collect charging state
				charging_state = frame.data[2];
				//Check if VCM wants to only charge to 80%
				max_charge_80_requested = ((frame.data[0] & 0x80) >> 7);
 
				if( My_Leaf == MY_LEAF_2011 )
				{
					seconds_without_1f2 = 0; // Keep resetting this variable, vehicle is not turned off
	 
					if (seen_1da && charging_state == CHARGING_IDLE)
					{
						frame.data[2] = 0;
						seen_1da--;
					}
	 
					if(( My_Battery == MY_BATTERY_40KWH ) || ( My_Battery == MY_BATTERY_62KWH ) )
					{
						// Only for 40/62kWh packs retrofitted to ZE0
						frame.data[3] = 0xA0;                    // Change from gen1->gen4+ . But doesn't seem to help much. We fix it anyways.
						calc_sum2(&frame);
					}
	 
				}
				break;

				case 0x59E: // QC capacity message, adjust for AZE0 with 30/40/62kWh pack swaps

				if( My_Leaf == MY_LEAF_2014 )
				{
					frame.data[2] = 0x0E; // Set LB_Full_Capacity_for_QC to 23000Wh (default value for 24kWh LEAF)
					frame.data[3] = 0x60;

					// Calculate new LBC_QC_CapRemaining value
					temp2 = ((230 * battery_soc) / 100);                       // Crazy advanced math
					frame.data[3] = (frame.data[3] & 0xF0) | ((temp2 >> 5) & 0xF);  // store the new LBC_QC_CapRemaining
					frame.data[4] = (uint8_t) ((temp2 & 0x1F) << 3) | (frame.data[4] & 0x07); // to the 59E message out to vehicle
					calc_crc8(&frame);
				}
				break;

 
				case 0x68C:
				case 0x603:
				reset_state(); // Reset all states, vehicle is starting up

				send_can(battery_can_bus, swap_605_message); // Send these ZE1 messages towards battery
				send_can(battery_can_bus, swap_607_message);
				break;
			default:
			break;
			}
		
		
		//block unwanted messages
			uint8_t blocked = 0;
			switch(frame.can_id){
				case 0x1C2:
				if( My_Leaf == MY_LEAF_2011 )
				{
					blocked = 1;
				}
				break;
				case 0x633:	//new 40kWh message, block to save CPU
					blocked = 1;
					break;
				case 0x625:	//Block this incase inverter upgrade code sends it towards battery (we generate our own)
					blocked = 1;
					break;
				case 0x355:	//Block this incase inverter upgrade code sends it towards battery (we generate our own)
					blocked = 1;
					break;
				case 0x5EC:	//Block this incase inverter upgrade code sends it towards battery (we generate our own)
					blocked = 1;
					break;
				case 0x5C5:	//Block this incase inverter upgrade code sends it towards battery (we generate our own)
					blocked = 1;
					break;
				case 0x3B8:	//Block this incase inverter upgrade code sends it towards battery (we generate our own)
					blocked = 1; 
					break;
				case 0x59E: //new AZE0 battery message, block on ZE0 LEAF to save resources
					if( My_Leaf == MY_LEAF_2011 )
					{
						blocked = 1;
					}
					break;
				default:
					blocked = 0;
					break;
			}
			if(!blocked){
				if(can_bus == 1){send_can2(frame);} else {send_can1(frame);}
				
			}
		}		
	
	if(flag & 0xA0){
		uint8_t flag2 = can_read(MCP_REG_EFLG, can_bus);
		if(flag2 & 0xC0){
			can_write(MCP_REG_EFLG, 0, can_bus); //reset all errors, we hit an CANX RX OVF
		}
		if(flag2 > 0){ PORTB.OUTSET = (1 << 1); }
		if(flag & 0xE0){ can_bit_modify(MCP_REG_CANINTF, (flag & 0xE0), 0x00, can_bus);	}
	}
	can_busy = 0;
}


void send_can(uint8_t can_bus, can_frame_t frame){
	if(can_bus == 1) send_can1(frame);
	if(can_bus == 2) send_can2(frame);
	if(can_bus == 3) send_can3(frame);
}

void send_can1(can_frame_t frame){	
	//put in the buffer
	memcpy(&tx0_buffer[tx0_buffer_end++], &frame, sizeof(frame));
	
	if(tx0_buffer_end >= TXBUFFER_SIZE){ //silently handle buffer overflows
		tx0_buffer_end = TXBUFFER_SIZE - 1;
	}
	
	check_can1();
}



void check_can1(void){
	uint8_t reg;
	
	if(tx0_buffer_end != tx0_buffer_pos){
		//check if TXB0 is free use
		reg = can1_read(MCP_REG_TXB0CTRL);
	
		if(!(reg & MCP_TXREQ_bm)){ //we're free to send
			can1_load_txbuff(0, (can_frame_t *) &tx0_buffer[tx0_buffer_pos++]);
			can1_rts(0);
			if(tx0_buffer_pos == tx0_buffer_end){ //end of buffer, reset
				tx0_buffer_end = 0;
				tx0_buffer_pos = 0;
			}
		}
	}
}

void send_can2(can_frame_t frame){
	//put in the buffer
	memcpy(&tx2_buffer[tx2_buffer_end++], &frame, sizeof(frame));
	
	if(tx2_buffer_end >= TXBUFFER_SIZE){ //silently handle buffer overflows
		tx2_buffer_end = TXBUFFER_SIZE - 1;
	}
	
	check_can2();
}

void check_can2(void){
	uint8_t reg;
	
	if(tx2_buffer_end != tx2_buffer_pos){
		//check if TXB0 is free use
		reg = can2_read(MCP_REG_TXB0CTRL);
		
		if(!(reg & MCP_TXREQ_bm)){ //we're free to send
			can2_load_txbuff(0, (can_frame_t *) &tx2_buffer[tx2_buffer_pos++]);
			can2_rts(0);
			if(tx2_buffer_pos == tx2_buffer_end){ //end of buffer, reset
				tx2_buffer_end = 0;
				tx2_buffer_pos = 0;
			}
		}
	}
}

void send_can3(can_frame_t frame){
	//put in the buffer
	memcpy(&tx3_buffer[tx3_buffer_end++], &frame, sizeof(frame));
	
	if(tx3_buffer_end >= TXBUFFER_SIZE){ //silently handle buffer overflows
		tx3_buffer_end = TXBUFFER_SIZE - 1;
	}
	
	check_can3();
}

void check_can3(void){
	uint8_t reg;
	
	if(tx3_buffer_end != tx3_buffer_pos){
		//check if TXB0 is free use
		reg = can3_read(MCP_REG_TXB0CTRL);
		
		if(!(reg & MCP_TXREQ_bm)){ //we're free to send
			can3_load_txbuff(0, (can_frame_t *) &tx3_buffer[tx3_buffer_pos++]);
			can3_rts(0);
			if(tx3_buffer_pos == tx3_buffer_end){ //end of buffer, reset
				tx3_buffer_end = 0;
				tx3_buffer_pos = 0;
			}
		}
	}
}
