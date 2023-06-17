/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
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
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */
#include "main.h"
#include <stdio.h>
#include <string.h>

static void CanTxToSTM(CAN_FRAME *frame, CAN_TxHeaderTypeDef *header, uint8_t *data);
static void CANSTMToRx(CAN_FRAME *frame, CAN_RxHeaderTypeDef *header, uint8_t *data);


static uint8_t CanBuffer[2][2][CAN_QUEUE][sizeof(CAN_FRAME)];
static volatile uint8_t canHead[2][2] = {0};
static volatile uint8_t canTail[2][2] = {0};

/* USER CODE END 0 */

CAN_HandleTypeDef hcan1;
CAN_HandleTypeDef hcan2;

/* CAN1 init function */
void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 4;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_15TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}
/* CAN2 init function */
void MX_CAN2_Init(void)
{

  /* USER CODE BEGIN CAN2_Init 0 */

  /* USER CODE END CAN2_Init 0 */

  /* USER CODE BEGIN CAN2_Init 1 */

  /* USER CODE END CAN2_Init 1 */
  hcan2.Instance = CAN2;
  hcan2.Init.Prescaler = 4;
  hcan2.Init.Mode = CAN_MODE_NORMAL;
  hcan2.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan2.Init.TimeSeg1 = CAN_BS1_15TQ;
  hcan2.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan2.Init.TimeTriggeredMode = DISABLE;
  hcan2.Init.AutoBusOff = ENABLE;
  hcan2.Init.AutoWakeUp = DISABLE;
  hcan2.Init.AutoRetransmission = ENABLE;
  hcan2.Init.ReceiveFifoLocked = DISABLE;
  hcan2.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN2_Init 2 */

  /* USER CODE END CAN2_Init 2 */

}

static uint32_t HAL_RCC_CAN1_CLK_ENABLED=0;

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    HAL_RCC_CAN1_CLK_ENABLED++;
    if(HAL_RCC_CAN1_CLK_ENABLED==1){
      __HAL_RCC_CAN1_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(CAN1_TX_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
  else if(canHandle->Instance==CAN2)
  {
  /* USER CODE BEGIN CAN2_MspInit 0 */

  /* USER CODE END CAN2_MspInit 0 */
    /* CAN2 clock enable */
    __HAL_RCC_CAN2_CLK_ENABLE();
    HAL_RCC_CAN1_CLK_ENABLED++;
    if(HAL_RCC_CAN1_CLK_ENABLED==1){
      __HAL_RCC_CAN1_CLK_ENABLE();
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**CAN2 GPIO Configuration
    PB5     ------> CAN2_RX
    PB6     ------> CAN2_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    __HAL_AFIO_REMAP_CAN2_ENABLE();

    /* CAN2 interrupt Init */
    HAL_NVIC_SetPriority(CAN2_TX_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN2_TX_IRQn);
    HAL_NVIC_SetPriority(CAN2_RX0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN2_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN2_RX1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN2_RX1_IRQn);
  /* USER CODE BEGIN CAN2_MspInit 1 */

  /* USER CODE END CAN2_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_CAN1_CLK_ENABLED--;
    if(HAL_RCC_CAN1_CLK_ENABLED==0){
      __HAL_RCC_CAN1_CLK_DISABLE();
    }

    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
  else if(canHandle->Instance==CAN2)
  {
  /* USER CODE BEGIN CAN2_MspDeInit 0 */

  /* USER CODE END CAN2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN2_CLK_DISABLE();
    HAL_RCC_CAN1_CLK_ENABLED--;
    if(HAL_RCC_CAN1_CLK_ENABLED==0){
      __HAL_RCC_CAN1_CLK_DISABLE();
    }

    /**CAN2 GPIO Configuration
    PB5     ------> CAN2_RX
    PB6     ------> CAN2_TX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_5|GPIO_PIN_6);

    /* CAN2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(CAN2_TX_IRQn);
    HAL_NVIC_DisableIRQ(CAN2_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN2_RX1_IRQn);
  /* USER CODE BEGIN CAN2_MspDeInit 1 */

  /* USER CODE END CAN2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

void AddCANFilters(CAN_HandleTypeDef* canHandle)
{
  CAN_FilterTypeDef  sFilterConfig;

  /*##-2- Configure the CAN Filter ###########################################*/
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 0;

  if (HAL_CAN_ConfigFilter(canHandle, &sFilterConfig) != HAL_OK)
  {
    /* Filter configuration Error */
    Error_Handler();
  }

  /*##-3- Start the CAN peripheral ###########################################*/
  if (HAL_CAN_Start(canHandle) != HAL_OK)
  {
    /* Start Error */
    Error_Handler();
  }

  /*##-4- Activate CAN RX notification #######################################*/
  if (HAL_CAN_ActivateNotification(canHandle, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY) != HAL_OK)
  {
    /* Notification Error */
    Error_Handler();
  }	
}

static CAN_HandleTypeDef *CANChanRef[2] = { &hcan1, &hcan2 };

void HAL_CAN_RxFIFO0MsgPendingCallback1( CAN_HandleTypeDef *canChan )
{    
    CAN_RxHeaderTypeDef Header;
    uint8_t             data[8];
    CAN_FRAME           frame;
    
    /*HAL_StatusTypeDef res = */HAL_CAN_GetRxMessage(canChan, CAN_RX_FIFO0, &Header, data);
    CANSTMToRx( &frame, &Header, data);
    PushCan( 0, CAN_RX, &frame );
}


void HAL_CAN_RxFIFO1MsgPendingCallback1( CAN_HandleTypeDef *canChan )
{
    CAN_RxHeaderTypeDef Header;
    uint8_t             data[8];
    CAN_FRAME           frame;
    
    /*HAL_StatusTypeDef res = */HAL_CAN_GetRxMessage(canChan, CAN_RX_FIFO1, &Header, data);
    CANSTMToRx( &frame, &Header, data);
    PushCan( 0, CAN_RX, &frame );
}


void HAL_CAN_RxFIFO0MsgPendingCallback2( CAN_HandleTypeDef *canChan )
{    
    CAN_RxHeaderTypeDef Header;
    uint8_t             data[8];
    CAN_FRAME           frame;
    
    /*HAL_StatusTypeDef res = */HAL_CAN_GetRxMessage(canChan, CAN_RX_FIFO0, &Header, data);
    CANSTMToRx( &frame, &Header, data);
    PushCan( 1, CAN_RX, &frame );
}


void HAL_CAN_RxFIFO1MsgPendingCallback2( CAN_HandleTypeDef *canChan )
{
    CAN_RxHeaderTypeDef Header;
    uint8_t             data[8];
    CAN_FRAME           frame;
    
    /*HAL_StatusTypeDef res = */HAL_CAN_GetRxMessage(canChan, CAN_RX_FIFO1, &Header, data);
    CANSTMToRx( &frame, &Header, data);
    PushCan( 1, CAN_RX, &frame );
}


void sendCan( uint8_t channel )
{
    CAN_FRAME frame;
    CAN_TxHeaderTypeDef header;
    uint8_t data[8];
    uint32_t mailbox;
    
    if( LenCan( channel, CAN_TX ) )
    {
        if( HAL_CAN_GetTxMailboxesFreeLevel( CANChanRef[ channel ] ) == 3 )
        {
            PopCan( channel, CAN_TX, &frame );
            CanTxToSTM(&frame, &header, data);
            HAL_CAN_AddTxMessage( CANChanRef[ channel ], &header, data, &mailbox );
        }
    } 
}


static void CanTxToSTM(CAN_FRAME *frame, CAN_TxHeaderTypeDef *header, uint8_t *data)
{
    if( frame->ide )
    {
        header->ExtId = frame->ID;
        header->IDE = CAN_ID_EXT;        
    }
    else
    {
        header->StdId = frame->ID;
        header->IDE = CAN_ID_STD;    
    }
    
    header->DLC = frame->dlc;
    
    if( frame->rtr )
    {
        header->RTR = CAN_RTR_REMOTE;
    }
    else
    {
        header->RTR = CAN_RTR_DATA;
        if( header->DLC > 0 )
        {
            memcpy( data, frame->data, header->DLC );
        }
    }
}


static void CANSTMToRx(CAN_FRAME *frame, CAN_RxHeaderTypeDef *header, uint8_t *data)
{
    if( header->IDE == CAN_ID_EXT )
    {
        frame->ID = header->ExtId;
        frame->ide = 1;
    }
    else
    {
        frame->ID = header->StdId;
        frame->ide = 0;       
    }
    
    frame->dlc = (uint8_t)header->DLC;
    
    if( header->RTR == CAN_RTR_REMOTE )
    {
        frame->rtr = 1;
    }
    else
    {
        frame->rtr = 0;
        
        if( frame->dlc > 0 )
        {
            memcpy( frame->data, data, frame->dlc );            
        }
    }
}


CQ_STATUS PushCan( uint8_t canNum, uint8_t TxRx, CAN_FRAME *frame )
{
    CQ_STATUS retVal = CQ_FULL;

    if( canNum > 1 )
    {
        return CQ_IGNORED;
    }
 
    if((( canHead[ TxRx ][canNum] + 1 ) % CAN_QUEUE ) != canTail[ TxRx ][ canNum ] )
    {
        memcpy( CanBuffer[ TxRx ][ canNum ][ canHead[ TxRx ][ canNum ]], frame, sizeof(CAN_FRAME) );
        canHead[ TxRx ][ canNum ] = ( canHead[ TxRx ][ canNum ] + 1 ) % CAN_QUEUE;        
        retVal = CQ_OK;
    }
    
    sendCan( canNum );
   
    return retVal;
}


CQ_STATUS PopCan( uint8_t canNum, uint8_t TxRx, CAN_FRAME *frame )
{
    CQ_STATUS retVal = CQ_EMPTY;
    
    if( canTail[ TxRx ][ canNum ] != canHead[ TxRx ][ canNum ] )
    {
        memcpy( frame, CanBuffer[ TxRx ][ canNum ][ canTail[ TxRx ][ canNum ] ], sizeof(CAN_FRAME) );
        canTail[ TxRx ][ canNum ] = ( canTail[ TxRx ][ canNum ] + 1 ) % CAN_QUEUE;         
        retVal = CQ_OK;
    }

    return retVal;
}


uint8_t LenCan( uint8_t canNum, uint8_t TxRx )
{
    if( canHead[ TxRx ][ canNum ] == canTail[ TxRx ][ canNum ] )
    {
        return 0;
    }
    else if( canHead[ TxRx ][ canNum ] > canTail[ TxRx ][ canNum ] )
    {
        return canHead[ TxRx ][ canNum ] - canTail[ TxRx ][ canNum ];
    }
    else
    {
        return CAN_QUEUE - canTail[ TxRx ][ canNum ] + canHead[ TxRx ][ canNum ];
    }
}


/* USER CODE END 1 */
