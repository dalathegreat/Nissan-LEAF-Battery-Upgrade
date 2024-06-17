#include "can.h"

#include "can-bridge-firmware.h"
#include "nissan_can_structs.h"

#include <stdio.h>
#include <string.h>

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

typedef enum
{
    TIME_100_WITH_200V_IN_MINUTES,
    TIME_80_WITH_200V_IN_MINUTES,
    TIME_100_WITH_100V_IN_MINUTES,
    TIME_80_WITH_100V_IN_MINUTES,
    TIME_100_WITH_QC_IN_MINUTES,
    TIME_80_WITH_66KW_IN_MINUTES,
    TIME_100_WITH_66KW_IN_MINUTES
}ChargeTimerMinutes;


// Charge timer minutes lookup values
static const uint16_t charge_time[4][7] =  {
                                        {  430, 340,  700,  560, 60, 150, 190 },   /* 24KW */
                                        {  600, 480,  900,  720, 60, 200, 245 },   /* 30KW */
                                        {  730, 580,  800,  640, 80, 270, 335 },   /* 40KW */
                                        { 1130, 900, 2000, 1600, 80, 430, 535 }    /* 62KW */
                                    };
// Lookup table battery temperature,	offset -40C		0    1   2   3  4    5   6   7   8   9  10  11  12
static const uint8_t temp_lut[13] = {25, 28, 31, 34, 37, 50, 63, 76, 80, 82, 85, 87, 90};

// charging variables
static volatile uint8_t charging_state = 0;
static volatile uint8_t	max_charge_80_requested	= 0;

// other variables
#define LB_MIN_SOC 0
#define LB_MAX_SOC 1000
static volatile uint16_t battery_soc_pptt = 0;
static volatile uint8_t battery_soc = 0;
static volatile int16_t dash_soc = 0;
static volatile uint8_t swap_5c0_idx = 0;
static volatile uint8_t VCM_WakeUpSleepCommand = 0;
static volatile	uint8_t	Byte2_50B		= 0;
static volatile uint8_t ALU_question = 0;
static volatile uint8_t cmr_idx = QUICK_CHARGE;
static volatile uint16_t GIDS 				= 0; 
static volatile uint16_t MaxGIDS = 0;
static volatile uint8_t main_battery_temp = 0;
static volatile uint8_t battery_can_bus = 2; // keeps track on which CAN bus the battery talks.

// 2011 variables
static volatile uint8_t CANMASK = 0;
static volatile uint8_t skip_5bc = 1;             // for 2011 battery swap, skip 4 messages
static volatile uint8_t alternate_5bc = 0;        // for 2011 battery swap
static volatile uint8_t seen_1da = 0;             // for 2011 battery swap
static volatile uint8_t seconds_without_1f2 = 0;  // bugfix: 0x603/69C/etc. isn't sent upon charge start on the gen1 Leaf, so we need to trigger our reset on a simple absence of messages
static volatile uint16_t startup_counter_1DB = 0;
static volatile uint8_t	startup_counter_39X 	= 0;
// bits															10					10					4				8				7			1				3	(2 space)		1					5						13
static Leaf_2011_5BC_message swap_5bc_remaining = {.LB_CAPR = 0x12C, .LB_FULLCAP = 0x32, .LB_CAPSEG = 0, .LB_AVET = 50, .LB_SOH = 99, .LB_CAPSW = 0, .LB_RLIMIT = 0, .LB_CAPBALCOMP = 1, .LB_RCHGTCON = 0x09, .LB_RCHGTIM = 0};
static Leaf_2011_5BC_message swap_5bc_full = {.LB_CAPR = 0x12C, .LB_FULLCAP = 0x32, .LB_CAPSEG = 12, .LB_AVET = 50, .LB_SOH = 99, .LB_CAPSW = 1, .LB_RLIMIT = 0, .LB_CAPBALCOMP = 1, .LB_RCHGTCON = 0x09, .LB_RCHGTIM = 0};
static Leaf_2011_5BC_message leaf_40kwh_5bc;

static Leaf_2011_5C0_message swap_5c0_max = {.LB_HIS_DATA_SW = 1, .LB_HIS_HLVOL_TIMS = 0, .LB_HIS_TEMP_WUP = 66, .LB_HIS_TEMP = 66, .LB_HIS_INTG_CUR = 0, .LB_HIS_DEG_REGI = 0x02, .LB_HIS_CELL_VOL = 0x39, .LB_DTC = 0};
static Leaf_2011_5C0_message swap_5c0_avg = {.LB_HIS_DATA_SW = 2, .LB_HIS_HLVOL_TIMS = 0, .LB_HIS_TEMP_WUP = 64, .LB_HIS_TEMP = 64, .LB_HIS_INTG_CUR = 0, .LB_HIS_DEG_REGI = 0x02, .LB_HIS_CELL_VOL = 0x28, .LB_DTC = 0};
static Leaf_2011_5C0_message swap_5c0_min = {.LB_HIS_DATA_SW = 3, .LB_HIS_HLVOL_TIMS = 0, .LB_HIS_TEMP_WUP = 64, .LB_HIS_TEMP = 64, .LB_HIS_INTG_CUR = 0, .LB_HIS_DEG_REGI = 0x02, .LB_HIS_CELL_VOL = 0x15, .LB_DTC = 0};

static CAN_FRAME saved_5bc_frame = {.ID = 0x5BC, .dlc = 8, .ide = 0, .rtr = 0};

// battery swap fixes
static CAN_FRAME ZE1_625_message = {.ID = 0x625, .dlc = 6, .ide = 0, .rtr = 0, .data = {0x02, 0x00, 0xff, 0x1d, 0x20, 0x00}};             // Sending 625 removes U215B [HV BATTERY]
static CAN_FRAME ZE1_5C5_message = {.ID = 0x5C5, .dlc = 8, .ide = 0, .rtr = 0, .data = {0x40, 0x01, 0x2F, 0x5E, 0x00, 0x00, 0x00, 0x00}}; // Sending 5C5 Removes U214E [HV BATTERY]
static CAN_FRAME ZE1_5EC_message = {.ID = 0x5EC, .dlc = 1, .ide = 0, .rtr = 0, .data = {0x00}};
static CAN_FRAME ZE1_355_message = {.ID = 0x355, .dlc = 8, .ide = 0, .rtr = 0, .data = {0x14, 0x0a, 0x13, 0x97, 0x10, 0x00, 0x40, 0x00}};
volatile static uint8_t ticker40ms = 0;
static CAN_FRAME ZE1_3B8_message = {.ID = 0x3B8, .dlc = 5, .ide = 0, .rtr = 0, .data = {0x7F, 0xE8, 0x01, 0x07, 0xFF}}; // Sending 3B8 removes U1000 and P318E [HV BATTERY]
volatile static uint8_t content_3B8 = 0;
volatile static uint8_t flip_3B8 = 0;

static CAN_FRAME swap_605_message = {.ID = 0x605, .dlc = 1, .ide = 0, .rtr = 0, .data = {0}};
static CAN_FRAME swap_607_message = {.ID = 0x607, .dlc = 1, .ide = 0, .rtr = 0, .data = {0}};
static CAN_FRAME AZE0_45E_message	= {.ID = 0x45E, .dlc = 1, .ide = 0, .rtr = 0, .data = {0x00}};
static CAN_FRAME AZE0_481_message	= {.ID =  0x481, .dlc = 2, .ide = 0, .rtr = 0, .data = {0x40,0x00}};
volatile	static uint8_t PRUN_39X			= 0;
static CAN_FRAME	AZE0_390_message	= {.ID = 0x390, .dlc = 8, .ide = 0, .rtr = 0, .data = {0x04,0x00,0x00,0x00,0x00,0x80,0x3c,0x07}}; // Sending removes P3196
static CAN_FRAME	AZE0_393_message	= {.ID = 0x393, .dlc = 8, .ide = 0, .rtr = 0, .data = {0x00,0x10,0x00,0x00,0x20,0x00,0x00,0x02}}; // Sending removes P3196


static uint8_t	crctable[256] = {0,133,143,10,155,30,20,145,179,54,60,185,40,173,167,34,227,102,108,233,120,253,247,114,80,213,223,90,203,78,68,193,67,198,204,73,216,93,87,210,240,117,127,250,107,238,228,97,160,37,47,170,59,190,180,49,19,150,156,25,136,13,7,130,134,3,9,140,29,152,146,23,53,176,186,63,174,43,33,164,101,224,234,111,254,123,113,244,214,83,89,220,77,200,194,71,197,64,74,207,94,219,209,84,118,243,249,124,237,104,98,231,38,163,169,44,189,56,50,183,149,16,26,159,14,139,129,4,137,12,6,131,18,151,157,24,58,191,181,48,161,36,46,171,106,239,229,96,241,116,126,251,217,92,86,211,66,199,205,72,202,79,69,192,81,212,222,91,121,252,246,115,226,103,109,232,41,172,166,35,178,55,61,184,154,31,21,144,1,132,142,11,15,138,128,5,148,17,27,158,188,57,51,182,39,162,168,45,236,105,99,230,119,242,248,125,95,218,208,85,196,65,75,206,76,201,195,70,215,82,88,221,255,122,112,245,100,225,235,110,175,42,32,165,52,177,187,62,28,153,147,22,135,2,8,141};

void convert_array_to_5bc(Leaf_2011_5BC_message * dest, uint8_t * src);
void calc_crc8(CAN_FRAME *frame);
void reset_state(void);
void convert_5bc_to_array(Leaf_2011_5BC_message * src, uint8_t * dest);
void convert_5c0_to_array(Leaf_2011_5C0_message * src, uint8_t * dest);
void calc_sum2(CAN_FRAME *frame);
void calc_checksum4(CAN_FRAME *frame);

void convert_array_to_5bc(Leaf_2011_5BC_message * dest, uint8_t * src)
{
	dest->LB_CAPR = (uint16_t)((src[0] << 2) | (src[1] & 0xC0 >> 6));
}

//recalculates the CRC-8 with 0x85 poly
void calc_crc8(CAN_FRAME *frame)
{
    uint8_t crc = 0;
    
	for(uint8_t j = 0; j < 7; j++)
    {
        crc = crctable[(crc ^ ((int) frame->data[j])) % 256];
    }
    
    frame->data[7] = crc;
}


void reset_state(void)
{
	charging_state = 0; //Reset charging state 
	startup_counter_1DB = 0;
	startup_counter_39X = 0;
}

void convert_5bc_to_array(Leaf_2011_5BC_message * src, uint8_t * dest)
{
	dest[0] = (uint8_t) (src->LB_CAPR >> 2);
	dest[1] = (uint8_t) (((src->LB_CAPR << 6) & 0xC0) | ((src->LB_FULLCAP >> 4) & 0x1F));
	dest[2] = (uint8_t) (((src->LB_FULLCAP << 4) & 0xF0) | ((src->LB_CAPSEG) & 0x0F));
	dest[3] = (uint8_t) (src->LB_AVET);
	dest[4] = (uint8_t) (((src->LB_SOH << 1) & 0xFE) | ((src->LB_CAPSW) & 1));
	dest[5] = (uint8_t) (((src->LB_RLIMIT << 5) & 0xE0) | ((src->LB_CAPBALCOMP << 2) & 4) | ((src->LB_RCHGTCON >> 3) & 3));
	dest[6] = (uint8_t) (((src->LB_RCHGTCON << 5) & 0xE0) | ((src->LB_RCHGTIM >> 8) & 0x1F));
	dest[7] = (uint8_t) (src->LB_RCHGTIM);
}


void convert_5c0_to_array(Leaf_2011_5C0_message * src, uint8_t * dest)
{
	dest[0] = (uint8_t) (src->LB_HIS_DATA_SW << 6) | src->LB_HIS_HLVOL_TIMS;
	dest[1] = (uint8_t) (src->LB_HIS_TEMP_WUP << 1);
	dest[2] = (uint8_t) (src->LB_HIS_TEMP << 1);
	dest[3] = src->LB_HIS_INTG_CUR;
	dest[4] = (uint8_t) (src->LB_HIS_DEG_REGI << 1);
	dest[5] = (uint8_t) (src->LB_HIS_CELL_VOL << 2);
	dest[7] = src->LB_DTC;
}


void calc_sum2(CAN_FRAME *frame)
{
	uint8_t sum = 0;
    
	for(uint8_t k = 0; k < 7; k++)
    {
		sum += frame->data[k] >> 4;
		sum += frame->data[k] & 0xF;
	}
    
	sum = (sum + 2) & 0xF;
	frame->data[7] = (frame->data[7] & 0xF0) + sum;
}

void calc_checksum4(CAN_FRAME *frame){ // Checksum. Sum of all nibbles (-4). End result in hex is anded with 0xF.
    uint8_t sum = 0;
    for(uint8_t m = 0; m < 7; m++){
        sum += frame->data[m] >> 4;  // Add the upper nibble
        sum += frame->data[m] & 0x0F;  // Add the lower nibble
    }
    sum = (sum - 4) & 0x0F;  // Subtract 4 and AND with 0xF to get the checksum
    frame->data[7] = (frame->data[7] & 0xF0) + sum;  // Place the checksum in the lower nibble of data[7]
}


void one_second_ping( void )
{
    if( My_Leaf == MY_LEAF_2011 )
    {
        seconds_without_1f2++;
        if(seconds_without_1f2 > 1)
        {
            reset_state();
        }
    }         
}

void can_handler(uint8_t can_bus, CAN_FRAME *frame)
{
    uint16_t temp; // Temporary variable used in many instances
		uint8_t blocked = 0;

    if (1)
    {
        switch (frame->ID)
        {
				case 0x1ED:
						//this message is only sent by 62kWh packs. We use this info to autodetect battery size
						My_Battery = MY_BATTERY_62KWH;
						break;
				case 0x5B9:
            if( My_Leaf == MY_LEAF_2011 )
            {
							PushCan(battery_can_bus, CAN_TX, &AZE0_45E_message); // 500ms
							PushCan(battery_can_bus, CAN_TX, &AZE0_481_message); // 500ms
							
							frame->dlc = 7;
							if (charging_state == CHARGING_SLOW)
							{
									frame->data[5] = 0xFF;
							}
							else
							{
									frame->data[5] = 0x4A;
							}
							frame->data[6] = 0x80;
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
                PushCan(battery_can_bus, CAN_TX, &ZE1_355_message); // 40ms
            }

            break;

        case 0x1DB:

            battery_can_bus = can_bus; // Guarantees that messages go to right bus. Without this there is a risk that the messages get sent to VCM instead of HVBAT
            
            if( My_Leaf == MY_LEAF_2011 )
            {
                // seems like we just need to clear any faults and show permission
                if (VCM_WakeUpSleepCommand == 3)
                {                                                  // VCM command: wakeup
										frame->data[3] = (frame->data[3] & 0xD7) | 0x28; // FRLYON=1, INTERLOCK=1
                }			
				if (startup_counter_1DB >= 100 && startup_counter_1DB <= 300) // Between 1s and 3s after poweron
				{
					frame->data[3] = (frame->data[3] | 0x10); // Set the full charge flag to ON during startup
				}											// This is to avoid instrumentation cluster scaling bars incorrectly
				if(startup_counter_1DB < 1000)
				{
					startup_counter_1DB++;
				}
								
			}

			if( My_Leaf == MY_LEAF_2014 ) 
			{
				//Calculate the SOC% value to send to the dash (Battery sends 10-95% which needs to be rescaled to dash 0-100%)
				dash_soc = (int16_t)(LB_MIN_SOC + (LB_MAX_SOC - LB_MIN_SOC) * (1.0 * battery_soc_pptt - MINPERCENTAGE) / (MAXPERCENTAGE - MINPERCENTAGE)); 
				if (dash_soc < 0)
				{ //avoid underflow
						dash_soc = 0;
				}
				if (dash_soc > 1000)
				{ //avoid overflow
						dash_soc = 1000;
				}
				dash_soc = (dash_soc/10);
				frame->data[4] = (uint8_t) dash_soc;  //If this is not written, soc% on dash will say "---"
			}

			if(max_charge_80_requested)
			{
				if((charging_state == CHARGING_SLOW) && (battery_soc > 80))
				{
					frame->data[1] = (frame->data[1] & 0xE0) | 2; //request charging stop
					frame->data[3] = (frame->data[3] & 0xEF) | 0x10; //full charge completed
				}
			}
            calc_crc8(frame);
            break;
				case 0x50A: //Message from VCM
					if(frame->dlc == 6)
					{	//On ZE0 this message is 6 bytes long
						My_Leaf = MY_LEAF_2011;
						//We extend the message towards the battery
						frame->dlc = 8;
						frame->data[6] = 0x04;
						frame->data[7] = 0x00;
					}
					else if(frame->dlc == 8)
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

            VCM_WakeUpSleepCommand = (frame->data[3] & 0xC0) >> 6;
						Byte2_50B = frame->data[2];

            if( My_Leaf == MY_LEAF_2011 )
            {
                CANMASK = (frame->data[2] & 0x04) >> 2;
								//Extend the message from 6->7 bytes
							frame->dlc = 7;
							frame->data[6] = 0x00;
            }

            break;

        case 0x50C: // Fetch ALU and send lots of missing 100ms messages towards battery

            ALU_question = frame->data[4];
            PushCan(battery_can_bus, CAN_TX, &ZE1_625_message); // 100ms
            PushCan(battery_can_bus, CAN_TX, &ZE1_5C5_message); // 100ms
            PushCan(battery_can_bus, CAN_TX, &ZE1_3B8_message); // 100ms
				
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
							AZE0_390_message.data[7] = (uint8_t)(PRUN_39X << 4);
							AZE0_393_message.data[7] = (uint8_t)(PRUN_39X << 4);
							calc_checksum4(&AZE0_390_message);
							calc_checksum4(&AZE0_393_message);
							
							PushCan(battery_can_bus, CAN_TX, &AZE0_390_message); // 100ms
							PushCan(battery_can_bus, CAN_TX, &AZE0_393_message); // 100ms
						}

            content_3B8++;
            if (content_3B8 > 14)
            {
                content_3B8 = 0;
            }

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
                frame->data[2] = 0xAA;
            }
            else
            {
                frame->data[2] = 0x55;
            }

            if( My_Leaf == MY_LEAF_2011 )
            {
                if (CANMASK == 0)
                {
                    frame->data[6] = (frame->data[6] & 0xCF) | 0x20;
                }
                else
                {
                    frame->data[6] = (frame->data[6] & 0xCF) | 0x10;
                }
            }
						battery_soc_pptt = (uint16_t) ((frame->data[0] << 2) | ((frame->data[1] & 0xC0) >> 6)); //Capture SOC% 0-100.0%
            battery_soc = (uint8_t) (battery_soc_pptt / 10); // Capture SOC% 0-100

            calc_crc8(frame);

            break;

        case 0x5BC:
						//Battery type detection
						if ((frame->data[5] & 0x10) == 16) //MaxGids active
						{
								if (frame->data[0] != 0xFF) //LBC has booted up
								{
										MaxGIDS = (uint16_t) ((frame->data[0] << 2) | ((frame->data[1] & 0xC0) >> 6));
										
										if(MaxGIDS < 363)
											{
												My_Battery = MY_BATTERY_30KWH;
											}
								}
						}

						//Battery upgrade code
            if( My_Leaf == MY_LEAF_2011 )
            {
                temp = ((frame->data[4] & 0xFE) >> 1); // Collect SOH value
                if (frame->data[0] != 0xFF)
                {
                    // Only modify values when GIDS value is available, that means LBC has booted
                    if ((frame->data[5] & 0x10) == 0x00)
                    {
                        // If everything is normal (no output power limit reason)
                        convert_array_to_5bc(&leaf_40kwh_5bc, (uint8_t *)&frame->data);
                        swap_5bc_remaining.LB_CAPR = leaf_40kwh_5bc.LB_CAPR;
                        swap_5bc_full.LB_CAPR = leaf_40kwh_5bc.LB_CAPR;
                        swap_5bc_remaining.LB_SOH = temp;
                        swap_5bc_full.LB_SOH = temp;
                        main_battery_temp = frame->data[3] / 20;
                        main_battery_temp = temp_lut[main_battery_temp] + 1;
                    }
                    else
                    {
                        // Output power limited
                    }
                }
                else
                {		//We are booting up, set charge times to unavailable
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
                
                if (!skip_5bc)
                {
                    switch (cmr_idx)
                    {
                    case QUICK_CHARGE:
                        // swap_5bc_remaining.LB_RCHGTIM = 8191; //1FFFh is unavailable value
                        swap_5bc_full.LB_RCHGTIM = 0x003C; // 60 minutes remaining
                        swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
                        swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
                        swap_5bc_full.LB_RCHGTCON = cmr_idx;
                        cmr_idx = NORMAL_CHARGE_200V_100;

                        break;

                    case NORMAL_CHARGE_200V_100:

                        swap_5bc_remaining.LB_RCHGTIM = (charge_time[My_Battery][TIME_100_WITH_200V_IN_MINUTES] - ((charge_time[My_Battery][TIME_100_WITH_200V_IN_MINUTES] * battery_soc) / 100));
                        
                        swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
                        swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
                        swap_5bc_full.LB_RCHGTCON = cmr_idx;
                        cmr_idx = NORMAL_CHARGE_200V_80;

                        break;

                    case NORMAL_CHARGE_200V_80:

                        if (battery_soc > 80)
                        {
                            swap_5bc_remaining.LB_RCHGTIM = 0;
                        }
                        else
                        {
                            swap_5bc_remaining.LB_RCHGTIM = (charge_time[My_Battery][TIME_80_WITH_200V_IN_MINUTES] - ((charge_time[My_Battery][TIME_80_WITH_200V_IN_MINUTES] * battery_soc) / 100));
                        }
    
                        swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
                        swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
                        swap_5bc_full.LB_RCHGTCON = cmr_idx;
                        cmr_idx = NORMAL_CHARGE_100V_100;

                        break;

                    case NORMAL_CHARGE_100V_100:

                        swap_5bc_remaining.LB_RCHGTIM = (charge_time[My_Battery][TIME_100_WITH_100V_IN_MINUTES] - ((charge_time[My_Battery][TIME_100_WITH_100V_IN_MINUTES] * battery_soc) / 100));
                        swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
                        swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
                        swap_5bc_full.LB_RCHGTCON = cmr_idx;
                        cmr_idx = NORMAL_CHARGE_100V_80;

                        break;

                    case NORMAL_CHARGE_100V_80:

                        if (battery_soc > 80)
                        {
                            swap_5bc_remaining.LB_RCHGTIM = 0;
                        }
                        else
                        {
                            swap_5bc_remaining.LB_RCHGTIM = (charge_time[My_Battery][TIME_80_WITH_100V_IN_MINUTES] - ((charge_time[My_Battery][TIME_80_WITH_100V_IN_MINUTES] * battery_soc) / 100));
                        }

                        swap_5bc_full.LB_RCHGTIM = swap_5bc_remaining.LB_RCHGTIM;
                        swap_5bc_remaining.LB_RCHGTCON = cmr_idx;
                        swap_5bc_full.LB_RCHGTCON = cmr_idx;
                        cmr_idx = QUICK_CHARGE;

                        break;
                    }

                    swap_5bc_remaining.LB_AVET = main_battery_temp;
                    swap_5bc_full.LB_AVET = main_battery_temp;

                    // ZE0 LEAF just cannot cope with capacities >24kWh
                    // when quick charging, we change capacity to reflect a proportion of 21.3kWh (266 GIDs)

                    if( My_Battery != MY_BATTERY_24KWH )
                    {
                        if (charging_state == CHARGING_QUICK)
                        {
                            temp = battery_soc * 300; // e.g. 55 * 300 = 16500
                            temp = temp / 100;            // e.g. 16500/100 = 165
                            swap_5bc_remaining.LB_CAPR = temp;
                            swap_5bc_full.LB_CAPR = temp;
                        }
                    }

                    if (alternate_5bc)
                    {
                        convert_5bc_to_array(&swap_5bc_remaining, (uint8_t *)&saved_5bc_frame.data);
                        alternate_5bc = 0;
                    }
                    else
                    {
                        convert_5bc_to_array(&swap_5bc_full, (uint8_t *)&saved_5bc_frame.data);
                        alternate_5bc = 1;
                    }

                    skip_5bc = 5;
                }
								frame->data[0] = saved_5bc_frame.data[0];
								frame->data[1] = saved_5bc_frame.data[1];
								frame->data[2] = saved_5bc_frame.data[2];
								frame->data[3] = saved_5bc_frame.data[3];
								frame->data[4] = saved_5bc_frame.data[4];
								frame->data[5] = saved_5bc_frame.data[5];
								frame->data[6] = saved_5bc_frame.data[6];
								frame->data[7] = saved_5bc_frame.data[7];
            }


            if( My_Leaf == MY_LEAF_2014 )
            {
                // 0x5BC on 2014 looks extremely similar to 2018, except for the extra LB_MaxGIDS switch, so we remove and ignore that
                if ((frame->data[5] & 0x10) == 0x00)
								{ //LB_MAXGIDS is 0, store actual GIDS remaining to this variable
										GIDS = (uint16_t) ((frame->data[0] << 2) | ((frame->data[1] & 0xC0) >> 6));
								}
								//Avoid blinking GOM by always writing remaining GIDS
								frame->data[0] = (uint8_t)(GIDS >> 2);
								frame->data[1] = (GIDS << 6) & 0xC0;
								
								//Collect temperature for 5C0 message
                main_battery_temp = frame->data[3] / 20;                    // Temperature needed for charger section
                main_battery_temp = temp_lut[main_battery_temp] + 1;       // write main_battery_temp to be used by 5C0 message

                // Correct charge timer estimates
                // This code is WIP, currently the 3.3 and 6.6kW times are good, but 100V is messed up somehow. Seems to be differences in LEAF firmware
                cmr_idx = (uint8_t)((frame->data[5] & 0x03) << 3) | ((frame->data[6] & 0xE0) >> 5);

                switch (cmr_idx)
                {
                case 0: // QC
                    break;

                case 5: // 6.6kW 100%

                    temp = (charge_time[My_Battery][TIME_100_WITH_66KW_IN_MINUTES] - ((charge_time[My_Battery][TIME_100_WITH_66KW_IN_MINUTES] * battery_soc) / 100));
                    frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                    frame->data[7] = (temp & 0xFF);
                    break;

                case 8: // 200V 100%

                    temp = (charge_time[My_Battery][TIME_100_WITH_200V_IN_MINUTES] - ((charge_time[My_Battery][TIME_100_WITH_200V_IN_MINUTES] * battery_soc) / 100));
                    frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                    frame->data[7] = (temp & 0xFF);
                    break;

                case 11: // 100V 100%

                    temp = (charge_time[My_Battery][TIME_100_WITH_100V_IN_MINUTES] - ((charge_time[My_Battery][TIME_100_WITH_100V_IN_MINUTES] * battery_soc) / 100));
                    frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                    frame->data[7] = (temp & 0xFF);
                    break;

                case 18: // 6.6kW 80%

                    if (battery_soc < 80)
                    {
                        temp = (charge_time[My_Battery][TIME_80_WITH_66KW_IN_MINUTES] - ((charge_time[My_Battery][TIME_80_WITH_66KW_IN_MINUTES] * (battery_soc + 20)) / 100));
                        frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                        frame->data[7] = (temp & 0xFF);
                    }
                    else
                    {
                        temp = 0; // 0 since we are over 80% SOC
                        frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                        frame->data[7] = (temp & 0xFF);
                    }
                    break;

                case 21: // 200V 80%

                    if (battery_soc < 80)
                    {
                        temp = (charge_time[My_Battery][TIME_80_WITH_200V_IN_MINUTES] - ((charge_time[My_Battery][TIME_80_WITH_200V_IN_MINUTES] * (battery_soc + 20)) / 100));
                        frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                        frame->data[7] = (temp & 0xFF);
                    }
                    else
                    {
                        temp = 0; // 0 since we are over 80% SOC
                        frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                        frame->data[7] = (temp & 0xFF);
                    }

                    break;

                case 24: // 100V 80%

                    if (battery_soc < 80)
                    {
                        temp = (charge_time[My_Battery][TIME_80_WITH_100V_IN_MINUTES] - ((charge_time[My_Battery][TIME_80_WITH_100V_IN_MINUTES] * (battery_soc + 20)) / 100));
                        frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                        frame->data[7] = (temp & 0xFF);
                    }
                    else
                    {
                        temp = 0; // 0 since we are over 80% SOC
                        frame->data[6] = (frame->data[6] & 0xE0) | (temp >> 8);
                        frame->data[7] = (temp & 0xFF);
                    }

                    break;

                case 31: // Unavailable
                    break;
                }
            }
            
            break;
        case 0x5C0: // Send 500ms messages here

            PushCan(battery_can_bus, CAN_TX, &ZE1_5EC_message); // 500ms
        
            //5C0 needs to be properly formatted for the temperature/charge bars on the dashboard
						swap_5c0_max.LB_HIS_TEMP = main_battery_temp;
						swap_5c0_max.LB_HIS_TEMP_WUP = main_battery_temp;
						swap_5c0_avg.LB_HIS_TEMP = main_battery_temp;
						swap_5c0_avg.LB_HIS_TEMP_WUP = main_battery_temp;
						swap_5c0_min.LB_HIS_TEMP = main_battery_temp;
						swap_5c0_min.LB_HIS_TEMP_WUP = main_battery_temp;

						if (swap_5c0_idx == 0)
						{
								convert_5c0_to_array(&swap_5c0_max, (uint8_t *)&frame->data);
						}
						else if (swap_5c0_idx == 1)
						{
								convert_5c0_to_array(&swap_5c0_avg, (uint8_t *)&frame->data);
						}
						else // == 2
						{
								convert_5c0_to_array(&swap_5c0_min, (uint8_t *)&frame->data);
						}

						swap_5c0_idx = (swap_5c0_idx + 1) % 3;
						//This takes advantage of the modulus operator % to reset the value of swap_5c0_idx to 0 once it reaches 3. 
						//It also eliminates the need for an if statement and a conditional check, which improves performance (but sacrifices readability)
            
            break;
        case 0x1F2: //Message from VCM
			//Collect charging state
			charging_state = frame->data[2];
			//Check if VCM wants to only charge to 80%
			max_charge_80_requested = ((frame->data[0] & 0x80) >> 7);
            
            if( My_Leaf == MY_LEAF_2011 )
            {     
								seconds_without_1f2 = 0; // Keep resetting this variable, vehicle is not turned off
							
                if (seen_1da && charging_state == CHARGING_IDLE)
                {
                    frame->data[2] = 0;
                    seen_1da--;
                }
                
                if(( My_Battery == MY_BATTERY_40KWH ) || ( My_Battery == MY_BATTERY_62KWH ) )
                {
                    // Only for 40/62kWh packs retrofitted to ZE0
                    frame->data[3] = 0xA0;                    // Change from gen1->gen4+ . But doesn't seem to help much. We fix it anyways.
                    calc_sum2(frame);
                }
                
            }
            break;

        case 0x59E: // QC capacity message, adjust for AZE0 with 30/40/62kWh pack swaps
            if( My_Leaf == MY_LEAF_2014 )
            {
                frame->data[2] = 0x0E; // Set LB_Full_Capacity_for_QC to 23000Wh (default value for 24kWh LEAF)
                frame->data[3] = 0x60;

                // Calculate new LBC_QC_CapRemaining value
                temp = ((230 * battery_soc) / 100);                       // Crazy advanced math
                frame->data[3] = (frame->data[3] & 0xF0) | ((temp >> 5) & 0xF);  // store the new LBC_QC_CapRemaining
                frame->data[4] = (uint8_t) ((temp & 0x1F) << 3) | (frame->data[4] & 0x07); // to the 59E message out to vehicle
                calc_crc8(frame);
            }
            break;
 
				case 0x68C:
        case 0x603:
            reset_state(); // Reset all states, vehicle is starting up
            PushCan(battery_can_bus, CAN_TX, &swap_605_message); // Send these ZE1 messages towards battery
            PushCan(battery_can_bus, CAN_TX, &swap_607_message);
        break;
        default:
            break;
        }

        // block unwanted messages
        switch (frame->ID)
        {

        case 0x633: // new 40kWh message, block to save CPU

            blocked = 1;
            break;

        case 0x625: // Block this incase inverter upgrade code sends it towards battery (we generate our own)

            blocked = 1;
            break;

        case 0x355: // Block this incase inverter upgrade code sends it towards battery (we generate our own)

            blocked = 1;
            break;

        case 0x5EC: // Block this incase inverter upgrade code sends it towards battery (we generate our own)

            blocked = 1;
            break;

        case 0x5C5: // Block this incase inverter upgrade code sends it towards battery (we generate our own)

            blocked = 1;
            break;

        case 0x3B8: // Block this incase inverter upgrade code sends it towards battery (we generate our own)

            blocked = 1;
            break;

        case 0x59E: // new 40kWh message (actually, 2014+ message)
        
            if( My_Leaf == MY_LEAF_2011 )
            {
                blocked = 1;
            }
            break;
            
        default:

            blocked = 0;
            break;
        }

        if (!blocked)
        {
            if (can_bus == 0)
            {
                PushCan(1, CAN_TX, frame);
            }
            else
            {
                PushCan(0, CAN_TX, frame);
            }
        }
    }
}
