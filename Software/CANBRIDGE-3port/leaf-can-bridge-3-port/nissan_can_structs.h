
typedef struct {
	int			LB_CAPR:10;		//LB_Remain_Capacity or more commonly known as GIDS
	int			LB_FULLCAP:10;	//LB_New_Full_Capacity, Wh 20000 - 24000 [offset 250, factor 80]
	int			LB_CAPSEG:4;	//LB_Remaining_Capacity_Segment, 0-12 bars [SWITCHES BETWEEN REMAINING & FULL]
	int			LB_AVET:8;		//LB_Average_Battery_Temperature, -40 - 105*C [offset -40]
	int			LB_SOH:7;		//LB_Capacity_Deterioration_Rate, 0 - 100% SOH
	int			LB_CAPSW:1;		//LB_Remaining_Capaci_Segment_Switch, controls LB_CAPSEG
	int			LB_RLIMIT:3;	//LB_Output_Power_Limit_Reason
	int			SPACER:2;		//Empty
	int			LB_CAPBALCOMP:1;//LB_Capacity_Bal_Complete_Flag, signals that capacity balancing is complete
	int			LB_RCHGTCON:5;	//LB_Remain_charge_time_condition, Mux for LB_Remain_charge_time
	int			LB_RCHGTIM:13;	//LB_Remain_charge_time, 0 - 8190 minutes
} Leaf_2011_5BC_message;

typedef struct {
	int			LB_HIS_DATA_SW:2;
	int			SPACER_1:2;
	int			LB_HIS_HLVOL_TIMS:4;
	int			LB_HIS_TEMP_WUP:7;
	int			SPACER_2:1;
	int			LB_HIS_TEMP:7;
	int			SPACER_3:1;
	int			LB_HIS_INTG_CUR:8;
	int			LB_HIS_DEG_REGI:7;
	int			SPACER_4:1;
	int			LB_HIS_CELL_VOL:6;
	int			SPACER_5:10;
	int			LB_DTC:8;
} Leaf_2011_5C0_message;