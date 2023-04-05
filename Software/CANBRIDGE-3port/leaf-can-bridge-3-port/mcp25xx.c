/*

Extra special quad CAN driver for SPIC and SPID on XMEGA with up to 4 CAN devices

The crystal is on SPIC with the regular enable line. Initialization starts by 
configuring the crystal output on this CAN device

Then initialization cycles through the rest of the devices

CAN messages can be received and sent by CAN bus (0 or 1) and device (0 or 1).

Timing logic:

EV-CAN is about 5m, let's calculate with 10m worst case. Propagation delay is 2*(235ns + 10mx5ns)=570ns
With BRP=0 and so Tq=125ns, we need 5Tq PRSEG and 16PRSEG total. So PHSEG1/2=5 Tq

NBT should be 1/500kHz=2us
each time quantum is 2*(BRP+1)/Fosc with Fosc=16M so Tclk=62.5ns
so:
BRP   Tq     Tq in 2us PRSEG(min) PHSEG1(max)
0     125    16        5          8
2     250    8         3          2

So a logical option would be BRP=2, PRSEG=3, PHSEG1=2, PHSEG2=2, SJW=2Tq
or otherwise                 BRP=0, PRSEG=5, PHSEG1=5, PHSEG2=5, SJW=4Tq
and SAM=1

Then again, if we want to stick to 87.5% sample point, then:
							 BRP=0, PRSEG=8, PHSEG1=5, PHSEG2=2, SJW=1

Driver optimization:

We're really just doing the same thing over and over again, so I coalesced all functionality in minimal functions
that specifically serve the muxsan leaf CAN bridge

A CAN frame with 8 data bytes is 108 bits, @500kbaud, that makes ~216us
we can service a CAN interrupt within 30us, so we should never see RX1 hits
yet... we do! so something else in the code is limiting us

Upgrade to 3-port:
Let's fix the inconsistent naming and actually make the silkscreen names match the function names!
CAN3 sits on SPI pins with the -3 suffix
CAN2 sits on the un-numbered suffix
CAN1 sits on the -2 suffix
*/

//uncomment this to run at 250 instead of 500kHz
//#define BAUDRATE_250kHz

#include "mcp25xx.h"
//#define F_CPU 48000000UL
#include <util/delay.h>

#define SPI0			SPIC
#define SPI0_PORT		PORTC
#define SPI0_SCK		PIN7_bm
#define SPI0_MISO		PIN6_bm
#define SPI0_MOSI		PIN5_bm

#define CAN1_PORT		PORTD
#define CAN1_STBY		PIN1_bm
#define CAN1_CS_PORT	PORTC
#define	CAN1_CS			PIN4_bm

#define CAN2_PORT		PORTE
#define CAN2_STBY		PIN1_bm
#define CAN2_CS_PORT	PORTD
#define	CAN2_CS			PIN4_bm

#define CAN3_PORT		PORTC
#define CAN3_STBY		PIN3_bm
#define CAN3_CS_PORT	PORTB
#define	CAN3_CS			PIN3_bm

#define can1_standby()			CAN1_PORT.OUTSET = CAN1_STBY
#define can2_standby()			CAN2_PORT.OUTSET = CAN2_STBY
#define can3_standby()			CAN3_PORT.OUTSET = CAN3_STBY
#define can1_unstandby()		CAN1_PORT.OUTCLR = CAN1_STBY
#define can2_unstandby()		CAN2_PORT.OUTCLR = CAN2_STBY
#define can3_unstandby()		CAN3_PORT.OUTCLR = CAN3_STBY

#define can1_select()			CAN1_CS_PORT.OUTCLR = CAN1_CS
#define can2_select()			CAN2_CS_PORT.OUTCLR = CAN2_CS
#define can3_select()			CAN3_CS_PORT.OUTCLR = CAN3_CS
#define can1_deselect()			CAN1_CS_PORT.OUTSET = CAN1_CS
#define can2_deselect()			CAN2_CS_PORT.OUTSET = CAN2_CS
#define can3_deselect()			CAN3_CS_PORT.OUTSET = CAN3_CS

uint8_t can_init(uint8_t opmod, uint8_t reset){
	uint8_t mode1, mode2, mode3;
	
	SPI0_PORT.DIRSET		= SPI0_SCK | SPI0_MOSI;
	SPI0_PORT.DIRCLR		= SPI0_MISO;
	CAN1_PORT.DIRSET		= CAN1_STBY;
	CAN2_PORT.DIRSET		= CAN2_STBY;
	CAN3_PORT.DIRSET		= CAN3_STBY;
	CAN1_CS_PORT.DIRSET		= CAN1_CS;
	CAN2_CS_PORT.DIRSET		= CAN2_CS;
	CAN3_CS_PORT.DIRSET		= CAN3_CS;
	SPI0.CTRL				= SPI_ENABLE_bm | SPI_MASTER_bm | SPI_CLK2X_bm |	//enable SPI SPI_CLK2X_bm |
							  SPI_PRESCALER_DIV4_gc;							//run faster than usual (clkperx2/4=24MHz)
	
	if(!reset) return 1;
	
	can1_deselect();
	can2_deselect();
	can3_deselect();
	
	_delay_ms(0.1);
	
	can1_reset();
	can2_reset();
	can3_reset();
	
	_delay_ms(1);												//wait for MCP25625 to be ready
	
	mode1 = can1_read(MCP_REG_CANCTRL);							//read opmode
	mode1 = (mode1 & MCP_MASK_OPMOD)>>5;						//mask with opmode bits	
	
	mode2 = can2_read(MCP_REG_CANCTRL);							//read opmode
	mode2 = (mode2 & MCP_MASK_OPMOD)>>5;						//mask with opmode bits	

	mode3 = can3_read(MCP_REG_CANCTRL);							//read opmode
	mode3 = (mode3 & MCP_MASK_OPMOD)>>5;						//mask with opmode bits
	
	if(mode1 != MCP_OPMOD_CONFIG) return 0;						//return error if not opmode CONFIG
	if(mode2 != MCP_OPMOD_CONFIG) return 0;						//return error if not opmode CONFIG
	if(mode3 != MCP_OPMOD_CONFIG) return 0;						//return error if not opmode CONFIG

	_delay_ms(0.1);												//wait a bit for other CAN devices to start up
	#ifdef BAUDRATE_250kHz
	can1_write(MCP_REG_CNF1,0xC1);								//set BRP to 0 so we get Tq = 1/8M, then we need 16xTq to get 500kHz
	can2_write(MCP_REG_CNF1,0xC1);
	can1_write(MCP_REG_CNF2,0xF0);								//PHSEG1 is 7xTq, PRSEG = 1xTq, SAM=1
	can2_write(MCP_REG_CNF2,0xF0);
	can1_write(MCP_REG_CNF3,0x06);								//PHSEG2 is 7xTq, so total is (7+7+1+1)=16Tq
	can2_write(MCP_REG_CNF3,0x06);
	#else
	can1_write(MCP_REG_CNF1,0b00000000);						//set BRP to 0 so we get Tq = 1/8M, then we need 16xTq to get 500kHz, SJW=1
	can2_write(MCP_REG_CNF1,0b00000000);
	can3_write(MCP_REG_CNF1,0b00000000);
	can1_write(MCP_REG_CNF2,0b11100111);						//PHSEG1 is 5xTq, PRSEG = 8xTq, SAM=1
	can2_write(MCP_REG_CNF2,0b11100111);
	can3_write(MCP_REG_CNF2,0b11100111);
	can1_write(MCP_REG_CNF3,0b00000001);						//PHSEG2 is 2xTq, so total is (5+5+5+1)=16Tq
	can2_write(MCP_REG_CNF3,0b00000001);
	can3_write(MCP_REG_CNF3,0b00000001);
	#endif

	
	//we leave RXM to 0 and set acceptance filters to 0 to avoid bogus data from error frames
	//ending up in the data stream

	can1_bit_modify(MCP_REG_RXB0CTRL, MCP_RXB_BUKT_MASK, MCP_RXB_BUKT_MASK);//configure message rollover
	can2_bit_modify(MCP_REG_RXB0CTRL, MCP_RXB_BUKT_MASK, MCP_RXB_BUKT_MASK);
	can3_bit_modify(MCP_REG_RXB0CTRL, MCP_RXB_BUKT_MASK, MCP_RXB_BUKT_MASK);
	can1_write(MCP_REG_CANINTE, 0xA3);							//enable both RX interrupts and the error interrupt
	can2_write(MCP_REG_CANINTE, 0xA3);
	can3_write(MCP_REG_CANINTE, 0xA3);
	can1_bit_modify(MCP_REG_CANCTRL, MCP_MASK_OPMOD, opmod);	//Set device mode per argument
	can2_bit_modify(MCP_REG_CANCTRL, MCP_MASK_OPMOD, opmod);
	can3_bit_modify(MCP_REG_CANCTRL, MCP_MASK_OPMOD, opmod);
	can1_unstandby();											//set to active	
	can2_unstandby();
	can3_unstandby();
	return 1;
}

//reset
void all_reset(){	can1_reset();can2_reset();can3_reset();}
void can1_reset(){ 	can1_select();spi0_write(MCP_RESET);can1_deselect(); }	
void can2_reset(){ 	can2_select();spi0_write(MCP_RESET);can2_deselect(); }
void can3_reset(){ 	can3_select();spi0_write(MCP_RESET);can3_deselect(); }

//bit modify
void can_bit_modify(uint8_t reg, uint8_t mask, uint8_t val, uint8_t bus){if(bus == 1){can1_select();can123_bit_modify(reg,mask,val);can1_deselect();}
																		if(bus == 2){can2_select();can123_bit_modify(reg,mask,val);can2_deselect();}
																		if(bus == 3){can3_select();can123_bit_modify(reg,mask,val);can3_deselect();}}
void can1_bit_modify(uint8_t reg, uint8_t mask, uint8_t val){ 	can1_select();can123_bit_modify(reg,mask,val);can1_deselect(); }
void can2_bit_modify(uint8_t reg, uint8_t mask, uint8_t val){ 	can2_select();can123_bit_modify(reg,mask,val);can2_deselect(); }
void can3_bit_modify(uint8_t reg, uint8_t mask, uint8_t val){ 	can3_select();can123_bit_modify(reg,mask,val);can3_deselect(); }
void can123_bit_modify(uint8_t reg, uint8_t mask, uint8_t val){	spi0_write(MCP_BITMOD); spi0_write(reg); spi0_write(mask); spi0_write(val);}

//RTS from SPI
void can1_rts(uint8_t channel){ can1_select(); can123_rts(channel); can1_deselect(); }
void can2_rts(uint8_t channel){ can2_select(); can123_rts(channel); can2_deselect(); }
void can3_rts(uint8_t channel){ can3_select(); can123_rts(channel); can3_deselect(); }

void can123_rts(uint8_t channel){
    switch (channel){
    	case MCP_TX_0: spi0_write(MCP_RTS_TXB0); break;
    	case MCP_TX_1: spi0_write(MCP_RTS_TXB1); break;
    	case MCP_TX_2: spi0_write(MCP_RTS_TXB2); break;
    	default: return;
}	}

void can1_load_txbuff(uint8_t channel, can_frame_t* frame){ can1_select(); can123_load_txbuff(channel, frame); can1_deselect(); }
void can2_load_txbuff(uint8_t channel, can_frame_t* frame){ can2_select(); can123_load_txbuff(channel, frame); can2_deselect(); }
void can3_load_txbuff(uint8_t channel, can_frame_t* frame){ can3_select(); can123_load_txbuff(channel, frame); can3_deselect(); }

void can123_load_txbuff(uint8_t channel, can_frame_t* frame){	
    uint8_t tmp = 0;
	
	spi0_write(MCP_LOAD_BUF_TXB0SIDH + (channel * 2)); //set buffer
	
	#ifdef EXTENDED_ID_SUPPORT
	if(frame->can_id > 0x7FF){
		frame->can_id |= 0x20000000;
	}
	
	if (frame->can_id & 0x20000000){
		spi0_write((uint8_t)(frame->can_id>>21));		// send XXXnSIDH
		tmp = ((uint8_t)(frame->can_id >> 13) ) & 0xe0;
		tmp |= ((uint8_t)(frame->can_id >> 16) ) & 0x03;
		tmp |= 0x08;
		spi0_write(tmp);				// send XXXnSIDL
		spi0_write((uint8_t)(frame->can_id >> 8));	// send XXXnEID8
		spi0_write((uint8_t)(frame->can_id));		// send XXXnEID0
		} else {
	#endif
	
	spi0_write((uint8_t)(frame->can_id>>3));		// send XXXnSIDH
	tmp = ((uint8_t)(frame->can_id << 5) ) & 0xe0;
	tmp |= ((uint8_t)(frame->can_id << 2) ) & 0x03;
	spi0_write(tmp);				// send XXXnSIDL
	spi0_write(0);					// send XXXnEID8
	spi0_write(0);					// send XXXnEID0
	
	#ifdef TC_CHARGER
		}
	#endif
	
    spi0_write(frame->can_dlc & 0x0f);

    for(uint8_t i= 0; i<frame->can_dlc; i++){ spi0_write(frame->data[i]); }
}

//Read rxbuffer to a frame
uint8_t can_read_rx_buf(uint8_t channel, can_frame_t* frame, uint8_t bus){uint8_t res = 0; if(bus == 1){res = can1_read_rx_buf(channel, frame);} if(bus == 2){res = can2_read_rx_buf(channel, frame);} if(bus == 3){res = can3_read_rx_buf(channel, frame);} return res;}
uint8_t can1_read_rx_buf(uint8_t channel, can_frame_t* frame){uint8_t res = 0; can1_select(); res = can123_read_rx_buf(channel, frame); can1_deselect(); return res;}
uint8_t can2_read_rx_buf(uint8_t channel, can_frame_t* frame){uint8_t res = 0; can2_select(); res = can123_read_rx_buf(channel, frame); can2_deselect(); return res;}
uint8_t can3_read_rx_buf(uint8_t channel, can_frame_t* frame){uint8_t res = 0; can3_select(); res = can123_read_rx_buf(channel, frame); can3_deselect(); return res;}

uint8_t can123_read_rx_buf(uint8_t channel, can_frame_t* frame){
	uint16_t id = 0;
	uint8_t data;
	
	spi0_write(channel);
	
	data = spi0_write(0); // read XXXnSIDH
	id = (((uint16_t)data) << 3);

	data = spi0_write(0); // read XXXnSIDL
	id |= (((uint16_t)(data & 0xe0)) >> 5);

	spi0_write(0); // read XXXnEID8
	spi0_write(0); // read XXXnEID0

    frame->can_id = id;
    frame->can_dlc = spi0_write(0); // read length of the frame
    frame->can_dlc &= 0x0f;	

    for (uint8_t i=0; i<frame->can_dlc; i++){ frame->data[i] = spi0_write(0); }
	
    return 1;
}

//read a register
uint8_t can_read(uint8_t reg, uint8_t bus){if(bus == 1){return can1_read(reg);} else if(bus == 2) {return can2_read(reg);} else {return can3_read(reg);}}
uint8_t can1_read(uint8_t reg){uint8_t res=0; can1_select(); spi0_write(MCP_READ); spi0_write(reg); res=spi0_write(0); can1_deselect(); return res;}
uint8_t can2_read(uint8_t reg){uint8_t res=0; can2_select(); spi0_write(MCP_READ); spi0_write(reg); res=spi0_write(0); can2_deselect(); return res;}
uint8_t can3_read(uint8_t reg){uint8_t res=0; can3_select(); spi0_write(MCP_READ); spi0_write(reg); res=spi0_write(0); can3_deselect(); return res;}

//write a register.
void can_write(uint8_t reg, uint8_t value, uint8_t bus){if(bus == 1){can1_write(reg, value);} else if(bus == 2){can2_write(reg, value);} else {can3_write(reg, value);}}
void can1_write(uint8_t reg, uint8_t value){can1_select(); spi0_write(MCP_WRITE);spi0_write(reg);spi0_write(value); can1_deselect();}
void can2_write(uint8_t reg, uint8_t value){can2_select(); spi0_write(MCP_WRITE);spi0_write(reg);spi0_write(value); can2_deselect();}
void can3_write(uint8_t reg, uint8_t value){can3_select(); spi0_write(MCP_WRITE);spi0_write(reg);spi0_write(value); can3_deselect();}

uint8_t spi0_write(uint8_t data){
	SPI0.STATUS = SPI0.STATUS;
	SPI0.DATA = data;
	//Send the data out, wait for data to be shifted out.
	while (!(SPI0.STATUS & SPI_IF_bm));
	return SPI0.DATA;
}
