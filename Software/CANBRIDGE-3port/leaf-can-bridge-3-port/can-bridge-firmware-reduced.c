#include "usb-hub-sensor.h"

USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface = {
	.Config = {
		.ControlInterfaceNumber   = 0,
		.DataINEndpoint           = {
			.Address          = CDC_TX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.DataOUTEndpoint =	{
			.Address          = CDC_RX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.NotificationEndpoint =	{
			.Address          = CDC_NOTIFICATION_EPADDR,
			.Size             = CDC_NOTIFICATION_EPSIZE,
			.Banks            = 1,
		},
	},
};
//general global variables
uint8_t		autosend					= 0;									//boolean that toggles automatic sending of values over serial line
uint8_t		configSuccess				= false;								//tracks whether device successfully enumerated
static FILE USBSerialStream;													//fwrite target for CDC
uint8_t		signature[11];														//signature bytes

//variables for ProcessCDCCommand()
int16_t		cmd, cmd2;
uint16_t	i = 0, j = 0, k = 0;
uint32_t	temp;
uint16_t	ReportStringLength;
char *		ReportString;
uint32_t	calibration_temp_array[40];					//array that holds exactly one EEPROM calibration table
		
volatile uint8_t dummy; //dummy variable to dummy-read registers




/*  Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	//initialize peripherals and USB hardware
    SetupHardware();
	NVM_GetGUID(signature);
    /* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
    CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

    GlobalInterruptEnable();

    while(1){
            ProcessCDCCommand();
            CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
            USB_USBTask();
    }
}


/* Configures the board hardware and chip peripherals */
void SetupHardware(void)
{				
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
    XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, 32000000);
    XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

    /* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
    XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
    XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, 48000000);
			
	/* enable multi-level interrupt controller */
    XMEGACLK_CCP_Write((void * ) &PMIC.CTRL, PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm);
		
    /* USB hardware Initialization (LUFA) */
    USB_Init(USB_OPT_RC32MCLKSRC | USB_OPT_BUSEVENT_PRIMED); //USB on medium level to improve current sense timing precision		
}


/* services commands received over the virtual serial port */
void ProcessCDCCommand(void)
{
	int16_t	cmd	 = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
	
	if(cmd > -1){
		switch(cmd){
			case 255: //send ident
				ReportString = "usb sensor rev4 \n"; ReportStringLength = 17;					
				break;
					
			default: //when all else fails
				ReportString = "Unrecognized Command:   \n"; ReportStringLength = 25;	
				ReportString[22] = cmd;					
				break;		
		}			
		if(ReportStringLength && (autosend == 0)){
			fwrite(ReportString, ReportStringLength, 1, &USBSerialStream);
			Endpoint_ClearIN();
		}
	}
}

/* Event handler for the LUFA library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void){
	LEDOFF();
	if(configSuccess) reset(); //reset AVR when device was previously configured
}

/* Event handler for the LUFA library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void){
    LEDON(); configSuccess = true;
    configSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}


/* Event handler for the LUFA library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void){
    CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/* ADC calibration byte read routine */
uint8_t ReadCalibrationByte( uint8_t index ){
	uint8_t result;

	/* Load the NVM Command register to read the calibration row. */
	NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
	result = pgm_read_byte(index);

	/* Clean up NVM Command register. */
	NVM_CMD = NVM_CMD_NO_OPERATION_gc;

	return result;
}


//reads signature bytes
void NVM_GetGUID(uint8_t * b) {
	enum {
		LOTNUM0=8,  // Lot Number Byte 0, ASCII
		LOTNUM1,    // Lot Number Byte 1, ASCII
		LOTNUM2,    // Lot Number Byte 2, ASCII
		LOTNUM3,    // Lot Number Byte 3, ASCII
		LOTNUM4,    // Lot Number Byte 4, ASCII
		LOTNUM5,    // Lot Number Byte 5, ASCII
		WAFNUM =16, // Wafer Number
		COORDX0=18, // Wafer Coordinate X Byte 0
		COORDX1,    // Wafer Coordinate X Byte 1
		COORDY0,    // Wafer Coordinate Y Byte 0
		COORDY1,    // Wafer Coordinate Y Byte 1
	};
	b[ 0]=ReadCalibrationByte(LOTNUM0);
	b[ 1]=ReadCalibrationByte(LOTNUM1);
	b[ 2]=ReadCalibrationByte(LOTNUM2);
	b[ 3]=ReadCalibrationByte(LOTNUM3);
	b[ 4]=ReadCalibrationByte(LOTNUM4);
	b[ 5]=ReadCalibrationByte(LOTNUM5);
	b[ 6]=ReadCalibrationByte(WAFNUM );
	b[ 7]=ReadCalibrationByte(COORDX0);
	b[ 8]=ReadCalibrationByte(COORDX1);
	b[ 9]=ReadCalibrationByte(COORDY0);
	b[10]=ReadCalibrationByte(COORDY1);
}