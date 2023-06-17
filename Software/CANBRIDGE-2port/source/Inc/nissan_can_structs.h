#if !defined(NISSAN_CAN_STRUCTS_H)
#define NISSAN_CAN_STRUCTS_H

#include "main.h"
typedef struct {
	uint64_t			LB_CAPR:10;
	uint64_t			LB_FULLCAP:10;
	uint64_t			LB_CAPSEG:4;
	uint64_t			LB_AVET:8;
	uint64_t			LB_SOH:7;
	uint64_t			LB_CAPSW:1;
	uint64_t			LB_RLIMIT:3;
	uint64_t			SPACER:2;
	uint64_t			LB_CAPBALCOMP:1;
	uint64_t			LB_RCHGTCON:5;
	uint64_t			LB_RCHGTIM:13;
} Leaf_2011_5BC_message;

typedef struct {
	uint64_t			LB_HIS_DATA_SW:2;
	uint64_t			SPACER_1:2;
	uint64_t			LB_HIS_HLVOL_TIMS:4;
	uint64_t			LB_HIS_TEMP_WUP:7;
	uint64_t			SPACER_2:1;
	uint64_t			LB_HIS_TEMP:7;
	uint64_t			SPACER_3:1;
	uint64_t			LB_HIS_INTG_CUR:8;
	uint64_t			LB_HIS_DEG_REGI:7;
	uint64_t			SPACER_4:1;
	uint64_t			LB_HIS_CELL_VOL:6;
	uint64_t			SPACER_5:10;
	uint64_t			LB_DTC:8;
} Leaf_2011_5C0_message;

#endif

