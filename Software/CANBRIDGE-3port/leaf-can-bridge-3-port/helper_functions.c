#include <stdio.h>
#include <avr/pgmspace.h>
#include "mcp25xx.h"
#include "nissan_can_structs.h"
#include "helper_functions.h"

const uint8_t	crctable[256] = {0,133,143,10,155,30,20,145,179,54,60,185,40,173,167,34,227,102,108,233,120,253,247,114,80,213,223,90,203,78,68,193,67,198,204,73,216,93,87,210,240,117,127,250,107,238,228,97,160,37,47,170,59,190,180,49,19,150,156,25,136,13,7,130,134,3,9,140,29,152,146,23,53,176,186,63,174,43,33,164,101,224,234,111,254,123,113,244,214,83,89,220,77,200,194,71,197,64,74,207,94,219,209,84,118,243,249,124,237,104,98,231,38,163,169,44,189,56,50,183,149,16,26,159,14,139,129,4,137,12,6,131,18,151,157,24,58,191,181,48,161,36,46,171,106,239,229,96,241,116,126,251,217,92,86,211,66,199,205,72,202,79,69,192,81,212,222,91,121,252,246,115,226,103,109,232,41,172,166,35,178,55,61,184,154,31,21,144,1,132,142,11,15,138,128,5,148,17,27,158,188,57,51,182,39,162,168,45,236,105,99,230,119,242,248,125,95,218,208,85,196,65,75,206,76,201,195,70,215,82,88,221,255,122,112,245,100,225,235,110,175,42,32,165,52,177,187,62,28,153,147,22,135,2,8,141};

//recalculates the CRC-8 with 0x85 poly
void calc_crc8(can_frame_t *frame){
	uint8_t crc = 0;
	for(uint8_t i = 0; i < 7; i++){
		crc = crctable[(crc ^ ((int) (*frame).data[i])) % 256];
	}
	(*frame).data[7] = crc;
}

void calc_sum2(can_frame_t *frame){ // Checksum. All message nibbles summed together, plus 2. End result in hex is anded with 0xF.
	uint8_t sum = 0;
	for(uint8_t i = 0; i < 7; i++){
		sum += (*frame).data[i] >> 4;
		sum += (*frame).data[i] & 0xF;
	}
	sum = (sum + 2) & 0xF;
	(*frame).data[7] = ((*frame).data[7] & 0xF0) + sum;
}

void calc_checksum4(can_frame_t *frame){ // Checksum. Sum of all nibbles (-4). End result in hex is anded with 0xF.
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 7; i++){
        sum += (*frame).data[i] >> 4;  // Add the upper nibble
        sum += (*frame).data[i] & 0x0F;  // Add the lower nibble
    }
    sum = (sum - 4) & 0x0F;  // Subtract 4 and AND with 0xF to get the checksum
    (*frame).data[7] = ((*frame).data[7] & 0xF0) + sum;  // Place the checksum in the lower nibble of data[7]
}

void convert_5bc_to_array(volatile Leaf_2011_5BC_message * src, uint8_t * dest){
	dest[0] = (uint8_t) (src->LB_CAPR >> 2);
	dest[1] = (uint8_t) (((src->LB_CAPR << 6) & 0xC0) | ((src->LB_FULLCAP >> 4) & 0x1F));
	dest[2] = (uint8_t) (((src->LB_FULLCAP << 4) & 0xF0) | ((src->LB_CAPSEG) & 0x0F));
	dest[3] = (uint8_t) (src->LB_AVET);
	dest[4] = (uint8_t) (((src->LB_SOH << 1) & 0xFE) | ((src->LB_CAPSW) & 1));
	dest[5] = (uint8_t) (((src->LB_RLIMIT << 5) & 0xE0) | ((src->LB_CAPBALCOMP << 2) & 4) | ((src->LB_RCHGTCON >> 3) & 3));
	dest[6] = (uint8_t) (((src->LB_RCHGTCON << 5) & 0xE0) | ((src->LB_RCHGTIM >> 8) & 0x1F));
	dest[7] = (uint8_t) (src->LB_RCHGTIM);
}

void convert_array_to_5bc(volatile Leaf_2011_5BC_message * dest, uint8_t * src){
	dest->LB_CAPR = (src[0] << 2) | (src[1] & 0xC0 >> 6);
}

void convert_5c0_to_array(volatile Leaf_2011_5C0_message * src, uint8_t * dest){
	dest[0] = (src->LB_HIS_DATA_SW << 6) | src->LB_HIS_HLVOL_TIMS;
	dest[1] = src->LB_HIS_TEMP_WUP << 1;
	dest[2] = src->LB_HIS_TEMP << 1;
	dest[3] = src->LB_HIS_INTG_CUR;
	dest[4] = src->LB_HIS_DEG_REGI << 1;
	dest[5] = src->LB_HIS_CELL_VOL << 2;
	dest[7] = src->LB_DTC;
}