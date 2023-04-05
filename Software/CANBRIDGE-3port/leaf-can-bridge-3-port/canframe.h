/*
 * canframe.h
 *
 * Created: 1/13/2018 1:32:28 PM
 *  Author: Dick
 */ 
#ifndef CANFRAME_H_
#define CANFRAME_H_

typedef struct can_frame can_frame_t;

/**
 * struct can_frame - basic CAN frame structure
 * @can_id:  CAN ID of the frame and CAN_*_FLAG flags, see canid_t definition
 * @can_dlc: frame payload length in byte (0 .. 8) aka data length code
 *           N.B. the DLC field from ISO 11898-1 Chapter 8.4.2.3 has a 1:1
 *           mapping of the 'data length code' to the real payload length
 * @__pad:   padding
 * @__res0:  reserved / padding
 * @__res1:  reserved / padding
 * @data:    CAN frame payload (up to 8 byte)
 */
#define CAN_MAX_DLEN		8

struct can_frame{
	uint32_t	can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t		can_dlc; /* frame payload length in byte (0 .. CAN_MAX_DLEN) */        
	uint8_t		data[CAN_MAX_DLEN];
};



#endif /* CANFRAME_H_ */