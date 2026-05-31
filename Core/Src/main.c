/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "itm.h"
#include "rtrecd.h"
#include "lcd.h"
#include "scd4x_i2c.h"
#include "ds18b20_app.h"
#include "app_menu.h"

#undef Error_Handler
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
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart1;

/* Definitions for TaskInput */
osThreadId_t TaskInputHandle;
const osThreadAttr_t TaskInput_attributes = {
  .name = "TaskInput",
  .stack_size = 500 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskUI */
osThreadId_t TaskUIHandle;
const osThreadAttr_t TaskUI_attributes = {
  .name = "TaskUI",
  .stack_size = 500 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskLCD */
osThreadId_t TaskLCDHandle;
const osThreadAttr_t TaskLCD_attributes = {
  .name = "TaskLCD",
  .stack_size = 300 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskDS18B20 */
osThreadId_t TaskDS18B20Handle;
const osThreadAttr_t TaskDS18B20_attributes = {
  .name = "TaskDS18B20",
  .stack_size = 300 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for QueueEC11 */
osMessageQueueId_t QueueEC11Handle;
const osMessageQueueAttr_t QueueEC11_attributes = {
  .name = "QueueEC11"
};
/* Definitions for QueueSCD41 */
osMessageQueueId_t QueueSCD41Handle;
const osMessageQueueAttr_t QueueSCD41_attributes = {
  .name = "QueueSCD41"
};
/* Definitions for QueueDS18B20 */
osMessageQueueId_t QueueDS18B20Handle;
const osMessageQueueAttr_t QueueDS18B20_attributes = {
  .name = "QueueDS18B20"
};
/* Definitions for MutexI2C2 */
osMutexId_t MutexI2C2Handle;
const osMutexAttr_t MutexI2C2_attributes = {
  .name = "MutexI2C2"
};
/* Definitions for MutexSCD41 */
osMutexId_t MutexSCD41Handle;
const osMutexAttr_t MutexSCD41_attributes = {
  .name = "MutexSCD41"
};
/* Definitions for MutexMenu */
osMutexId_t MutexMenuHandle;
const osMutexAttr_t MutexMenu_attributes = {
  .name = "MutexMenu"
};
/* USER CODE BEGIN PV */

static rtrecd_t g_rtrecd = {
  .pin_a = {GPIOB, GPIO_PIN_12},
  .pin_b = {GPIOB, GPIO_PIN_13},
  .pin_sw = {GPIOB, GPIO_PIN_14}
};

/* Menu context - shared between TaskUI (writer) and TaskLCD (reader/renderer) */
static app_menu_ctx_t g_menu_ctx;
osMutexId_t MutexMenuHandle;


uint32_t ramduinput, ramduui, ramdulcd, ramduds18b20;
uint32_t free_heap __attribute__((unused));
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
void StartTaskInput(void *argument);
void StartTaskUI(void *argument);
void StartTaskLCD(void *argument);
void StartTaskDS18B20(void *argument);

/* USER CODE BEGIN PFP */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
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

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of MutexI2C2 */
  MutexI2C2Handle = osMutexNew(&MutexI2C2_attributes);

  /* creation of MutexSCD41 */
  MutexSCD41Handle = osMutexNew(&MutexSCD41_attributes);

  /* creation of MutexMenu */
  MutexMenuHandle = osMutexNew(&MutexMenu_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  MutexMenuHandle = osMutexNew(&MutexMenu_attributes);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of QueueEC11 */
  QueueEC11Handle = osMessageQueueNew (16, sizeof(rtrecd_queue_item_t), &QueueEC11_attributes);

  /* creation of QueueSCD41 */
  QueueSCD41Handle = osMessageQueueNew (16, sizeof(scd41_queue_item_t), &QueueSCD41_attributes);

  /* creation of QueueDS18B20 */
  QueueDS18B20Handle = osMessageQueueNew (16, sizeof(Ds18b20QueueItem), &QueueDS18B20_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of TaskInput */
  TaskInputHandle = osThreadNew(StartTaskInput, NULL, &TaskInput_attributes);

  /* creation of TaskUI */
  TaskUIHandle = osThreadNew(StartTaskUI, NULL, &TaskUI_attributes);

  /* creation of TaskLCD */
  TaskLCDHandle = osThreadNew(StartTaskLCD, NULL, &TaskLCD_attributes);

  /* creation of TaskDS18B20 */
  TaskDS18B20Handle = osThreadNew(StartTaskDS18B20, NULL, &TaskDS18B20_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  free_heap = xPortGetFreeHeapSize();

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_HalfDuplex_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pins : PB12 PB13 PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_12)
  {
    rtrecd_isr_a(&g_rtrecd);
  }
  else if (GPIO_Pin == GPIO_PIN_13)
  {
    rtrecd_isr_b(&g_rtrecd);
  }
  else if (GPIO_Pin == GPIO_PIN_14)
  {
    rtrecd_isr_sw(&g_rtrecd);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  owReadHandler(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  owErrorHandler(huart);
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartTaskInput */
/**
  * @brief  Function implementing the TaskInput thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTaskInput */
void StartTaskInput(void *argument)
{
  /* USER CODE BEGIN 5 */
	if (rtrecd_init(&g_rtrecd) == false)
	{
		Error_Handler();
	}

	scd41_config_t scd41_config = {0};
	scd41_context_t scd41_context = {0};

	scd41_config.i2c_handle = &hi2c1;
	scd41_config.i2c_mutex = MutexSCD41Handle;

	scd4x_runtime_init(&scd41_config, &scd41_context);
	scd4x_runtime_start_periodic_measurement(&scd41_config, &scd41_context);





	uint32_t scd41_last_tick = osKernelGetTickCount();

  /* Infinite loop */
  for(;;)
  {
	  rtrecd_service(&g_rtrecd, QueueEC11Handle);

	  if((osKernelGetTickCount() - scd41_last_tick) >= 500U)
	  {
		  scd41_last_tick = osKernelGetTickCount();
	  Scd41Api_Service(&scd41_config,
					   &scd41_context,
					   QueueSCD41Handle,
					   scd4x_runtime_default_itm_event_handler);

	  }
	  ramduinput = uxTaskGetStackHighWaterMark(NULL);
	  osDelay(2);

  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTaskUI */
/**
* @brief Function implementing the TaskUI thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskUI */
void StartTaskUI(void *argument)
{
  /* USER CODE BEGIN StartTaskUI */
	rtrecd_queue_item_t ev;
	scd41_queue_item_t scd41_data;
	Ds18b20QueueItem ds18b20_data;

	bool got_event;

  /* Infinite loop */
  for(;;)
  {
	  got_event = false;

	  /* EC11 encoder -> menu navigation */
	  if (osMessageQueueGet(QueueEC11Handle, &ev, NULL, 0U) == osOK)
	  {
		  itm_print("EC11: ");
		  itm_print(rtrecd_queue_item_to_str(ev));
		  itm_print("\r\n");

		  osMutexAcquire(MutexMenuHandle, osWaitForever);
		  app_menu_handle_event(&g_menu_ctx, ev);
		  osMutexRelease(MutexMenuHandle);

		  got_event = true;
	  }

	  /* SCD41 data -> menu sensor update + ITM log */
	  if (osMessageQueueGet(QueueSCD41Handle, &scd41_data, NULL, 0U) == osOK)
	  {
		  scd41_print_scd41_measurement(scd41_data.co2,
				  scd41_data.temperature_m_deg_c,
				  scd41_data.humidity_m_percent_rh);

		  osMutexAcquire(MutexMenuHandle, osWaitForever);
		  app_menu_update_scd41(&g_menu_ctx, &scd41_data);
		  osMutexRelease(MutexMenuHandle);

		  got_event = true;
	  }

	  /* DS18B20 data -> menu sensor update + ITM log */
	  if (osMessageQueueGet(QueueDS18B20Handle, &ds18b20_data, NULL, 0U) == osOK)
	  {
		  Ds18b20Api_PrintItem(&ds18b20_data);

		  osMutexAcquire(MutexMenuHandle, osWaitForever);
		  app_menu_update_ds18b20(&g_menu_ctx, &ds18b20_data);
		  osMutexRelease(MutexMenuHandle);

		  got_event = true;
	  }

	  if(got_event)
	  {
		  continue;
	  }
	  osDelay(10);

	  ramduui = uxTaskGetStackHighWaterMark(NULL);

  }
  /* USER CODE END StartTaskUI */
}

/* USER CODE BEGIN Header_StartTaskLCD */
/**
* @brief Function implementing the TaskLCD thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskLCD */
void StartTaskLCD(void *argument)
{
  /* USER CODE BEGIN StartTaskLCD */
	lcd_i2c_config_t lcd_cfg = {
	    .hi2c = &hi2c2,
	    .i2c_addr = 0x27 << 1,
	    .mutex = MutexI2C2Handle,
	};

    itm_print("[LCD] init...\r\n");
    lcd_init(&lcd_cfg);
    itm_print("[LCD] init OK\r\n");

    /* Khởi tạo menu sau khi LCD đã sẵn sàng */
    osMutexAcquire(MutexMenuHandle, osWaitForever);
    app_menu_init(&g_menu_ctx);
    osMutexRelease(MutexMenuHandle);

  /* Infinite loop */
  for(;;)
  {
	  osMutexAcquire(MutexMenuHandle, osWaitForever);
	  app_menu_render(&g_menu_ctx);
	  osMutexRelease(MutexMenuHandle);

	  ramdulcd = uxTaskGetStackHighWaterMark(NULL);
	  osDelay(50);
  }
  /* USER CODE END StartTaskLCD */
}

/* USER CODE BEGIN Header_StartTaskDS18B20 */
/**
* @brief Function implementing the TaskDS18B20 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskDS18B20 */
void StartTaskDS18B20(void *argument)
{
  /* USER CODE BEGIN StartTaskDS18B20 */
	  OneWire_Config  owCfg1;
	  OneWire_Context owCtx1;

	  owCfg1.huart      = &huart1;
	  owCfg1.maxDevices = 5U;

	  Ds18b20Api_Init(&owCfg1, &owCtx1);

  /* Infinite loop */
  for(;;)
  {
	Ds18b20Api_Service(&owCfg1, &owCtx1,
			  	  	  QueueDS18B20Handle,
                      Ds18b20Api_DefaultOnWireFault);
	ramduds18b20 = uxTaskGetStackHighWaterMark(NULL);
    osDelay(5000);
  }
  /* USER CODE END StartTaskDS18B20 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
