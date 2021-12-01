/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
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
#include "crc.h"
#include "dma2d.h"
#include "i2c.h"
#include "ltdc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h> 
#include <stdlib.h>
#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
//typedef struct {
//  uint16_t x;
//	uint16_t y;
//	uint16_t r;
//	uint32_t c;
//} Circle;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SCENE_SIZE 512
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static TS_StateTypeDef  TS_State;

static uint32_t uiLastTapTime = 0;
static uint32_t uiTapDelay = 500;

static uint32_t colors[] = {LCD_COLOR_GREEN, LCD_COLOR_RED, LCD_COLOR_BLUE, LCD_COLOR_YELLOW, LCD_COLOR_MAGENTA, LCD_COLOR_WHITE};
static uint16_t radiuses[] = {8, 10, 13, 16};

static uint16_t selectedRadius = 8;
static uint32_t selectedColor = LCD_COLOR_YELLOW;
static uint32_t foregroundColor = LCD_COLOR_WHITE;
static uint32_t backgroundColor = LCD_COLOR_BLACK;
static uint16_t uiY = 50;
static uint16_t uiButtonsCount = 3;

//static int sceneIndex = 0;
//static Circle scene[SCENE_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void switchSelectedColor() {
	int n = sizeof(colors) / sizeof(colors[0]);
	for(int i = 0; i < n; i++) {
		if (selectedColor == colors[i]) {
			selectedColor = colors[(i + 1) % n];
			break;
		}
	}
}

void switchSelectedRadius() {
	int n = sizeof(radiuses) / sizeof(radiuses[0]);
	for(int i = 0; i < n; i++) {
		if (selectedRadius == radiuses[i]) {
			selectedRadius = radiuses[(i + 1) % n];
			break;
		}
	}
}

void printUartTouchData(uint16_t x, uint16_t y) {
	int lengthX = snprintf(NULL, 0, "%d", x);
	int lengthY = snprintf(NULL, 0, "%d", y);
	int length = 2 + lengthX + 1 + 2 + lengthY + 2; //x:x y:y\r\n
	char str[length + 1];
	snprintf(str, length + 1, "x:%d y:%d\r\n", x, y);
	HAL_UART_Transmit(&huart1, (uint8_t*)str, length + 1, 1000);
}

void resetScreen() {
	BSP_LCD_Clear(backgroundColor);
}

void drawCircle(uint16_t x, uint16_t y, uint16_t r, uint32_t c) {
	BSP_LCD_SetTextColor(c);
	BSP_LCD_FillCircle(x, y, r);
}

void addCircle(uint16_t x, uint16_t y, uint16_t r, uint32_t c) {
	drawCircle(x, y, r, c);
	
	int lengthX = snprintf(NULL, 0, "%u", x);
	int lengthY = snprintf(NULL, 0, "%u", y);
	int lengthR = snprintf(NULL, 0, "%u", r);
	int lengthC = snprintf(NULL, 0, "%u", c);
	
	// X,Y,R,C\r\n
	int length = lengthX + lengthY + lengthR + lengthC + 3 + 2;
	char str[length + 1];
	snprintf(str, length + 1, "%u,%u,%u,%u\r\n", x, y, r, c);
	HAL_UART_Transmit(&huart1, (uint8_t*)str, length + 1, 1000);
}

void resetScene() {
	resetScreen();
	HAL_UART_Transmit(&huart1, (uint8_t*)"CLEAR\r\n\0", 8, 1000);
}

void redrawUI() {
	BSP_LCD_SetTextColor(backgroundColor);
	BSP_LCD_FillRect(0, 0, uiButtonsCount * uiY, uiY);
	
	BSP_LCD_SetTextColor(foregroundColor);
	BSP_LCD_DrawLine(0, uiY, uiButtonsCount * uiY, uiY);
	
	for(int index = 0; index < uiButtonsCount; index++) {
		BSP_LCD_SetTextColor(foregroundColor);
		BSP_LCD_DrawLine((index + 1) * uiY, 0, (index + 1) * uiY, uiY);
		
		switch (index) {
			case 0: {
				BSP_LCD_SetTextColor(selectedColor);
				drawCircle(((index + (index + 1)) * uiY) / 2, uiY / 2, selectedRadius, selectedColor);
				break;
			}
			case 1: {
				BSP_LCD_SetTextColor(foregroundColor);
				BSP_LCD_SetBackColor(backgroundColor);
				BSP_LCD_SetFont(&Font16);
				
				int length = snprintf(NULL, 0, "%d", selectedRadius);
				char str[length + 1];
				snprintf(str, length + 1, "%d", selectedRadius);
				BSP_LCD_DisplayStringAt((((index + (index + 1)) * uiY) / 2) - (4 * length), (uiY / 2) - 6, (uint8_t *)str, LEFT_MODE);
				break;
			}
			case 2: {
				BSP_LCD_SetTextColor(foregroundColor);
				BSP_LCD_DrawLine((index + 1) * uiY, 0, index * uiY, uiY);
				BSP_LCD_DrawLine(index * uiY, 0, (index + 1) * uiY, uiY);
				break;
			}
		}
	}
}

void handleUiTap(uint16_t x) {
	uint32_t now = HAL_GetTick();
	if (now < uiLastTapTime + uiTapDelay) {
		return;
	}
	uiLastTapTime = now;
	
	int index = x / uiY;
	
	switch (index) {
		case 0: {
			switchSelectedColor();
			redrawUI();
			break;
		}
		case 1: {
			switchSelectedRadius();
			redrawUI();
			break;
		}
		case 2: {
			resetScene();
			redrawUI();
			break;
		}
	}
}

uint8_t isUiXY(uint16_t x, uint16_t y) {
	return (y <= uiY && x <= uiButtonsCount * uiY);
}
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
  MX_CRC_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  MX_I2C3_Init();
  MX_LTDC_Init();
  MX_SPI5_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_USB_OTG_HS_HCD_Init();
  /* USER CODE BEGIN 2 */
	
	BSP_LCD_Init();
  BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);
  BSP_LCD_SelectLayer(0);
	BSP_LCD_Clear(backgroundColor);
	BSP_LCD_DisplayOn();
	
	if (BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize()) != TS_OK) {
		BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
		BSP_LCD_SetBackColor(LCD_COLOR_BLUE);
		BSP_LCD_SetFont(&Font20);
		BSP_LCD_DisplayStringAt(0, 0, (uint8_t *)"TS ERROR", CENTER_MODE);
	}
	
	redrawUI();
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
		
		BSP_TS_GetState(&TS_State); 
		
		if(TS_State.TouchDetected) {
			int x = TS_State.X;
			int y = TS_State.Y;
			
			// printUartTouchData(x, y);
			
			if (!isUiXY(x, y - selectedRadius)) {
				if ((x > selectedRadius && x < 239 - selectedRadius) && (y > selectedRadius && y < 319 - selectedRadius)) {
					addCircle(x, y, selectedRadius, selectedColor);
				}
			} else {
				handleUiTap(x);
			}
		}
		
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
//void SystemClock_Config(void)
//{
//  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
//  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

//  /** Configure the main internal regulator output voltage
//  */
//  __HAL_RCC_PWR_CLK_ENABLE();
//  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
//  /** Initializes the RCC Oscillators according to the specified parameters
//  * in the RCC_OscInitTypeDef structure.
//  */
//  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
//  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
//  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
//  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
//  RCC_OscInitStruct.PLL.PLLM = 4;
//  RCC_OscInitStruct.PLL.PLLN = 72;
//  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
//  RCC_OscInitStruct.PLL.PLLQ = 3;
//  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  /** Initializes the CPU, AHB and APB buses clocks
//  */
//  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
//                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
//  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
//  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
//  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
//  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

//  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
//  {
//    Error_Handler();
//  }
//}

/* USER CODE BEGIN 4 */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();
  
  /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
  
  if(HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
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

