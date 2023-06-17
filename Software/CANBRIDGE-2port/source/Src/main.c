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
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "iwdg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include <string.h>
#include "can-bridge-firmware.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static MYCAN_Errors    mErrors[2] = {0};
static uint32_t last_tick = 0;
uint32_t au32_UID[3] = {0}; 
static const uint8_t au8_lock[12] = {0x33,0x44,0x55,0x66,0x11,0x22,0x33,0x44,0x77,0x66,0x55,0x44};
static uint8_t config_Bits[2] = {0};
static uint32_t canErrors = 0;
//static CAN_FRAME frame2 = { .ID = 0x100, .dlc = 8, .ide = 0, .rtr = 0, .data = {1,2,3,4,5,6,7,8}};

#if defined( CODE_PROTECT )
static FLASH_OBProgramInitTypeDef OptionsBytesStruct;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
    CAN_FRAME frame;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
    
#if defined( CODE_PROTECT )
    
    /* Unlock the Flash to enable the flash control register access */
    HAL_FLASH_Unlock();

    /* Unlock the Options Bytes */
    HAL_FLASH_OB_Unlock();

    /* Clear any existing errors */
    __HAL_FLASH_CLEAR_FLAG( FLASH_FLAG_PGERR );

    /* Get pages write protection status */
    HAL_FLASHEx_OBGetConfig(&OptionsBytesStruct);

    /* Check if readoutprtection is enabled */
    if((OptionsBytesStruct.RDPLevel) == OB_RDP_LEVEL_0)
    {
        OptionsBytesStruct.OptionType   = OPTIONBYTE_RDP;
        OptionsBytesStruct.RDPLevel     = OB_RDP_LEVEL_1;

        if(HAL_FLASHEx_OBProgram(&OptionsBytesStruct) != HAL_OK)
        {
            /* Error occurred while options bytes programming. */
            /* Reset to attempt Read-protection */
            HAL_NVIC_SystemReset ( );
        }

        /* Generate System Reset to load the new option byte values */
        HAL_FLASH_OB_Launch();
    }

    /* Lock the Options Bytes */
    HAL_FLASH_OB_Lock();

    /* ****** WRITE PROTECTION (END) ***** */
#endif    
    
#if !defined(DEBUG)
    
    if( strncmp(STM32_UUID, (const char*)au8_lock, 12 ) != 0 )
    {
        NVIC_SystemReset();
    }
#endif

   
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_CAN2_Init();
    
#if !defined(DEBUG)
    MX_IWDG_Init();
#endif    
    
  /* USER CODE BEGIN 2 */

    config_Bits[0] = (HAL_GPIO_ReadPin(Inp1_GPIO_Port, Inp1_Pin) == GPIO_PIN_SET ? 1:0) | (HAL_GPIO_ReadPin(Inp2_GPIO_Port, Inp2_Pin) == GPIO_PIN_SET ? 2:0); 
    config_Bits[1] = (HAL_GPIO_ReadPin(Inp3_GPIO_Port, Inp3_Pin) == GPIO_PIN_SET ? 1:0) | (HAL_GPIO_ReadPin(Inp4_GPIO_Port, Inp4_Pin) == GPIO_PIN_SET ? 2:0);  
    
    HAL_CAN_RegisterCallback(&hcan1, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO0MsgPendingCallback1 );
    HAL_CAN_RegisterCallback(&hcan1, HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO1MsgPendingCallback1 );    
    HAL_CAN_RegisterCallback(&hcan2, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO0MsgPendingCallback2 );
    HAL_CAN_RegisterCallback(&hcan2, HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID, HAL_CAN_RxFIFO1MsgPendingCallback2 ); 
    
    AddCANFilters( &hcan1 );
    AddCANFilters( &hcan2 );

    //PushCan(0, CAN_TX, &frame2);
    //PushCan(1, CAN_TX, &frame2);    
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {      
#if !defined(DEBUG)        
        HAL_IWDG_Refresh(&hiwdg);
#endif        
      
        if(( HAL_GetTick() - last_tick ) >= 1000u )
        {
            // 1 Second has passed
            last_tick = HAL_GetTick();
            
            one_second_ping();
        }         
        
        if( LenCan( MYCAN1, CAN_RX ) > 0 )
        {
            PopCan( MYCAN1, CAN_RX, &frame );
            
            can_handler( MYCAN1, &frame );
        }
        
        if( LenCan( MYCAN2, CAN_RX ) > 0 )
        {         
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
        
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
    
  /* USER CODE END 3 */
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the Systick interrupt time
  */
  __HAL_RCC_PLLI2S_ENABLE();
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
__attribute__((noreturn)) void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
