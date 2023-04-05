#ifndef MCP25XX_H_
#define MCP25XX_H_

#include <avr/io.h>

typedef struct can_frame can_frame_t;

#define CAN_MAX_DLEN		8
//#define EXTENDED_ID_SUPPORT

#ifdef EXTENDED_ID_SUPPORT
struct can_frame{
	uint32_t	can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t		can_dlc; /* frame payload length in byte (0 .. CAN_MAX_DLEN) */
	uint8_t		data[CAN_MAX_DLEN];
};
#else
struct can_frame{
	uint16_t	can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t		can_dlc; /* frame payload length in byte (0 .. CAN_MAX_DLEN) */        
	uint8_t		data[CAN_MAX_DLEN];
};
#endif

//Instructions
#define MCP_RESET						0xC0

#define MCP_READ						0x03
#define MCP_READ_BUF_RXB0SIDH			0x90	//read data from begin address of Standard ID (MSB) of RX buffer 0
#define MCP_READ_BUF_RXB0D0				0x92	//read data from begin address of data byte 0 of RX buffer 0
#define MCP_READ_BUF_RXB1SIDH			0x94	//...RX buffer 1
#define MCP_READ_BUF_RXB1D0				0x96	//...RX buffer 1

#define MCP_WRITE						0x02
#define MCP_LOAD_BUF_TXB0SIDH			0x40	//load data from begin address of Standard ID (MSB) of TX buffer 0
#define MCP_LOAD_BUF_TXB0D0				0X41	//load data from begin address of data byte 0 of TX buffer 0
#define MCP_LOAD_BUF_TXB1SIDH			0x42	//...TX buffer 1
#define MCP_LOAD_BUF_TXB1D0				0X43	//...TX buffer 1
#define MCP_LOAD_BUF_TXB2SIDH			0x44	//...TX buffer 2
#define MCP_LOAD_BUF_TXB2D0				0X45	//...TX buffer 2
#define MCP_RTS_TXB0					0x81	//activate the RTS of TX buffer 0.
#define MCP_RTS_TXB1					0x82	//...TX buffer 1.
#define MCP_RTS_TXB2					0x84	//...TX buffer 2.
#define MCP_RTS_ALL						0x87	//...TX buffer 0, 1 and 2.
#define MCP_READ_RXTX_STATUS			0xA0
#define MCP_RX_STATUS					0xB0
	
#define MCP_BITMOD						0x05

#define MCP_LOAD_TX0					0x40
#define MCP_LOAD_TX1					0x42
#define MCP_LOAD_TX2					0x44

#define MCP_RTS_TX0						0x81
#define MCP_RTS_TX1						0x82
#define MCP_RTS_TX2						0x84
#define MCP_RTS_ALL						0x87

//Registers
#define MCP_MASK_CANSTAT				0x0E
#define MCP_MASK_CANCTRL				0x0F

#define MCP_REG_BFPCTRL					0x0C	//RXnBF pin control/status
#define MCP_REG_TXRTSCTRL				0x0D	//TXnRTS pin control/status
#define MCP_REG_CANSTAT					0x0E	//CAN status. any addr of MSB will give the same info???
#define MCP_REG_CANCTRL					0x0F	//CAN control status. any addr of MSB will give the same info???
#define MCP_REG_TEC						0x1C	//Transmit Error Counter
#define MCP_REG_REC						0x1D	//Receive Error Counter
#define MCP_REG_CNF3					0x28	//Phase segment 2
#define MCP_REG_CNF2					0x29	//Propagation segment & Phase segment 1 & n sample setting
#define MCP_REG_CNF1					0x2A	//Baud rate prescaler & Sync Jump Width
#define MCP_REG_CANINTE					0x2B	//CAN interrupt enable
#define MCP_REG_CANINTF					0x2C	//Interrupt flag
#define MCP_REG_EFLG					0x2D	//Error flag
#define MCP_REG_TXB0CTRL				0x30	//TX buffer 0 control
#define MCP_REG_TXB1CTRL				0x40	//TX buffer 1 control
#define MCP_REG_TXB2CTRL				0x50	//TX buffer 2 control
#define MCP_REG_RXB0CTRL				0x60	//RX buffer 0 control
#define MCP_REG_RXB1CTRL				0x70	//RX buffer 1 control

#define MCP_MASK_OPMOD					0xE0
#define MCP_OPMOD_NORMAL 				0x00
#define MCP_OPMOD_SLEEP					0x01
#define MCP_OPMOD_LOOPBACK				0x02
#define MCP_OPMOD_LISTEN				0x03
#define MCP_OPMOD_CONFIG				0x04

#define MCP_MASK_CLKEN					0x04
#define MCP_CLKEN						0x04

#define MCP_OPMOD_INVALID				0x07

#define MCP_RX_OPMOD_FILTER_ANY 		0
#define MCP_RX_OPMOD_FILTER_STANDARD	1
#define MCP_RX_OPMOD_FILTER_EXTENDED	2
#define MCP_RX_OPMOD_ANY 				3


#define MCP_TX_0						0
#define MCP_TX_1						1
#define MCP_TX_2						2

#define MCP_RX_0						0x90
#define MCP_RX_1						0x94

#define MCP_SIDH						0
#define MCP_SIDL						1
#define MCP_EID8						2
#define MCP_EID0						3

#define MCP_TXB_EXIDE_M					0x08    // In TXBnSIDL

//MCP_REG_RXB0CTRL
#define MCP_RXB_RX_ANY					0x60
#define MCP_RXB_RX_EX					0x40
#define MCP_RXB_RX_ST					0x20
#define MCP_RXB_RX_STDEXT				0x00
#define MCP_RXB_RX_MASK					0x60
#define MCP_RXB_BUKT_MASK				0x04

//MCP_REG_CANINTF Register Bits
#define MCP_RX0IF						0x01
#define MCP_RX1IF						0x02
#define MCP_TX0IF						0x04
#define MCP_TX1IF						0x08
#define MCP_TX2IF						0x10
#define MCP_ERRIF						0x20
#define MCP_WAKIF						0x40
#define MCP_MERRF						0x80

#define MCP_TXREQ_bm					0x08

uint8_t can_init(uint8_t opmod, uint8_t reset);
void all_reset(void);
void can1_reset(void);
void can2_reset(void);
void can3_reset(void);

//bit modify
void can_bit_modify(uint8_t reg, uint8_t mask, uint8_t val, uint8_t bus);
void can1_bit_modify(uint8_t reg, uint8_t mask, uint8_t val);
void can2_bit_modify(uint8_t reg, uint8_t mask, uint8_t val);	
void can3_bit_modify(uint8_t reg, uint8_t mask, uint8_t val);																																																																																																																																																																																																																																																																		 
void can123_bit_modify(uint8_t reg, uint8_t mask, uint8_t val);
void can1_rts(uint8_t channel);
void can2_rts(uint8_t channel);	
void can3_rts(uint8_t channel);	
void can123_rts(uint8_t channel);
void can1_load_txbuff(uint8_t channel, can_frame_t* frame);
void can2_load_txbuff(uint8_t channel, can_frame_t* frame);
void can3_load_txbuff(uint8_t channel, can_frame_t* frame);
void can123_load_txbuff(uint8_t channel, can_frame_t* frame);
uint8_t spi0_write(uint8_t data);
uint8_t can_read_rx_buf(uint8_t channel, can_frame_t* frame, uint8_t bus);
uint8_t can1_read_rx_buf(uint8_t channel, can_frame_t* frame);
uint8_t can2_read_rx_buf(uint8_t channel, can_frame_t* frame);
uint8_t can3_read_rx_buf(uint8_t channel, can_frame_t* frame);
uint8_t can123_read_rx_buf(uint8_t channel, can_frame_t* frame);
uint8_t can_read(uint8_t reg, uint8_t bus);
uint8_t can1_read(uint8_t reg);
uint8_t can2_read(uint8_t reg);
uint8_t can3_read(uint8_t reg);
void can_write(uint8_t reg, uint8_t value, uint8_t bus);
void can1_write(uint8_t reg, uint8_t value);
void can2_write(uint8_t reg, uint8_t value);
void can3_write(uint8_t reg, uint8_t value);

#endif