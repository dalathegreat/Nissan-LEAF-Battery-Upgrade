/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.h
  * @brief   This file contains all the function prototypes for
  *          the can.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan1;

extern CAN_HandleTypeDef hcan2;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_CAN1_Init(void);
void MX_CAN2_Init(void);

/* USER CODE BEGIN Prototypes */

#define CAN_QUEUE  16

#define MYCAN1     0
#define MYCAN2     1

#define CAN_TX     0
#define CAN_RX     1

/* USER CODE END Private defines */

void MX_CAN1_Init(void);
void MX_CAN2_Init(void);

/* USER CODE BEGIN Prototypes */
typedef struct
{
    uint32_t    ID;
    uint8_t     dlc;
    uint8_t     ide;
    uint8_t     rtr;
    uint8_t     pad;
    uint8_t     data[8];
}CAN_FRAME;

typedef enum
{
    CQ_OK,
    CQ_FULL,
    CQ_EMPTY,
    CQ_IGNORED
} CQ_STATUS;

typedef struct
{
    uint8_t rec;
    uint8_t trans;
    uint8_t lastErrorCode;
    uint8_t boff;
    uint8_t passive;
    uint8_t errorFlag;    
}MYCAN_Errors;

CQ_STATUS PushCan( uint8_t canNum, uint8_t TxRx, CAN_FRAME *frame );
CQ_STATUS PopCan( uint8_t canNum, uint8_t TxRx, CAN_FRAME *frame );
uint8_t LenCan( uint8_t canNum, uint8_t TxRx );
void sendCan( uint8_t channel );
void HAL_CAN_RxFIFO0MsgPendingCallback1( CAN_HandleTypeDef *canChan );
void HAL_CAN_RxFIFO1MsgPendingCallback1( CAN_HandleTypeDef *canChan );
void HAL_CAN_RxFIFO0MsgPendingCallback2( CAN_HandleTypeDef *canChan );
void HAL_CAN_RxFIFO1MsgPendingCallback2( CAN_HandleTypeDef *canChan );
void AddCANFilters(CAN_HandleTypeDef* canHandle);


/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */

