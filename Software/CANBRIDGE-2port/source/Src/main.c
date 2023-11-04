/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "can.h"
#include "iwdg.h"
#include "gpio.h"

#include "stdio.h"
#include <string.h>
#include "can-bridge-firmware.h"

static MYCAN_Errors mErrors[2] = {0};
static uint32_t last_tick = 0;
uint32_t au32_UID[3] = {0}; 
static const uint8_t au8_lock[12] = {0x33,0x44,0x55,0x66,0x11,0x22,0x33,0x44,0x77,0x66,0x55,0x44};
static uint8_t config_Bits[2] = {0};
static uint32_t canErrors = 0;
static uint8_t idleTick = 0;

void SystemClock_Config(void);

int main(void)
{
  /* USER CODE BEGIN 1 */
   CAN_FRAME frame;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */

	HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();
    
    if( strncmp(STM32_UUID, (const char*)au8_lock, 12 ) != 0 )
    {
        NVIC_SystemReset();
    }

		MX_GPIO_Init();
		MX_CAN1_Init();
		MX_CAN2_Init();
    
    //MX_IWDG_Init(); //Init watchdog

    config_Bits[0] = (HAL_GPIO_ReadPin(Inp1_GPIO_Port, Inp1_Pin) == GPIO_PIN_SET ? 1:0) | (HAL_GPIO_ReadPin(Inp2_GPIO_Port, Inp2_Pin) == GPIO_PIN_SET ? 2:0); 
    config_Bits[1] = (HAL_GPIO_ReadPin(Inp3_GPIO_Port, Inp3_Pin) == GPIO_PIN_SET ? 1:0) | (HAL_GPIO_ReadPin(Inp4_GPIO_Port, Inp4_Pin) == GPIO_PIN_SET ? 2:0);  
    
    HAL_CAN_RegisterCallback(&hcan1, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO0MsgPendingCallback1 );
    HAL_CAN_RegisterCallback(&hcan1, HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO1MsgPendingCallback1 );    
    HAL_CAN_RegisterCallback(&hcan2, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO0MsgPendingCallback2 );
    HAL_CAN_RegisterCallback(&hcan2, HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO1MsgPendingCallback2 ); 
		
		HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
		HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);
    
    AddCANFilters( &hcan1 );
    AddCANFilters( &hcan2 );

    while (1)
    {           
        //HAL_IWDG_Refresh(&hiwdg);
      
        if(( HAL_GetTick() - last_tick ) >= 1000u )
        {
            // 1 Second has passed
            last_tick = HAL_GetTick();
            
            one_second_ping();
						
						if((LenCan( MYCAN1, CAN_RX )) == 0 && (LenCan( MYCAN2, CAN_RX ) == 0)){
							//Can bus is idle
							idleTick++;
							
							if(idleTick > 5){ //No can messages for 5s
								HAL_SuspendTick();
								HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
								HAL_ResumeTick();
								idleTick = 0;
							}
						}
        }
        
        if( LenCan( MYCAN1, CAN_RX ) > 0 )
        {
						idleTick = 0;
            PopCan( MYCAN1, CAN_RX, &frame );
            
            can_handler( MYCAN1, &frame );
        }
        
        if( LenCan( MYCAN2, CAN_RX ) > 0 )
        {   
						idleTick = 0;
            PopCan( MYCAN2, CAN_RX, &frame );

            can_handler( MYCAN2, &frame );            
        }

        sendCan( MYCAN1 );
        sendCan( MYCAN2 ); 
        
        canErrors = hcan1.Instance->ESR;
        mErrors[0].rec = canErrors >> 24;
        mErrors[0].trans = ( canErrors >> 16 ) &0xff;
        mErrors[0].lastErrorCode = ( canErrors & 0x70 ) >> 4;
        mErrors[0].boff = ( canErrors & 0x04 ) >> 2;
        mErrors[0].passive = ( canErrors & 0x02 ) >> 1;
        mErrors[0].errorFlag = canErrors & 1;
        
        canErrors = hcan2.Instance->ESR;
        mErrors[1].rec = canErrors >> 24;
        mErrors[1].trans = ( canErrors >> 16 ) &0xff;
        mErrors[1].lastErrorCode = ( canErrors & 0x70 ) >> 4;
        mErrors[1].boff = ( canErrors & 0x04 ) >> 2;
        mErrors[1].passive = ( canErrors & 0x02 ) >> 1;
        mErrors[1].errorFlag = canErrors & 1;       
        
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV5;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.Prediv1Source = RCC_PREDIV1_SOURCE_PLL2;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL2.PLL2State = RCC_PLL2_ON;
  RCC_OscInitStruct.PLL2.PLL2MUL = RCC_PLL2_MUL8;
  RCC_OscInitStruct.PLL2.HSEPrediv2Value = RCC_HSE_PREDIV2_DIV5;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2; //DIV1 is original for 72Mhz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1; //DIV2 is original for 72Mhz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the Systick interrupt time
  */
  __HAL_RCC_PLLI2S_ENABLE();
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
		HAL_NVIC_SystemReset ( );
}

