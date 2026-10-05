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
#include "mdf.h"
#include "i2c.h"
#include "icache.h"
#include "octospi.h"
#include "spi.h"
#include "usart.h"
#include "ucpd.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LPS22HH_ADDR          (0x5DU << 1)  /* SA0 = 1 sur cette carte */
#define LPS22HH_WHO_AM_I      0x0FU
#define LPS22HH_CTRL_REG1     0x10U
#define LPS22HH_PRESS_OUT_XL  0x28U         /* 5 octets : P(3) + T(2) */
#define LPS22HH_ID            0xB3U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
/* USER CODE BEGIN PFP */
static void i2c2_recover(void);
static HAL_StatusTypeDef lps_read(uint8_t reg, uint8_t *b, uint16_t n);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Releases a stuck I2C slave (9 SCL pulses + STOP) then resets I2C2 */
static void i2c2_recover(void)
{
  GPIO_InitTypeDef g = {0};

  HAL_I2C_DeInit(&hi2c2);

  __HAL_RCC_GPIOH_CLK_ENABLE();
  g.Pin   = GPIO_PIN_4 | GPIO_PIN_5;        /* PH4 = SCL, PH5 = SDA */
  g.Mode  = GPIO_MODE_OUTPUT_OD;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &g);

  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(1);
  for (int i = 0; i < 9; i++)
  {
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_RESET); HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_SET);   HAL_Delay(1);
  }
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_RESET); HAL_Delay(1);  /* STOP */
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_SET);   HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_SET);   HAL_Delay(1);

  MX_I2C2_Init();                           /* sets the pins back to AF4 */
}

static HAL_StatusTypeDef lps_read(uint8_t reg, uint8_t *b, uint16_t n)
{
  return HAL_I2C_Mem_Read(&hi2c2, LPS22HH_ADDR, reg,
                          I2C_MEMADD_SIZE_8BIT, b, n, 100);
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

  /* Configure the System Power */
  SystemPower_Config();

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADF1_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_ICACHE_Init();
  MX_OCTOSPI1_Init();
  MX_OCTOSPI2_Init();
  MX_SPI2_Init();
  MX_UART4_Init();
  MX_USART1_UART_Init();
  MX_UCPD1_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint8_t ready = 0;
    char msg[96];
    int len;
    HAL_StatusTypeDef st;

    if (!ready)
    {
      uint8_t id = 0;
      st = lps_read(LPS22HH_WHO_AM_I, &id, 1);
      if (st == HAL_OK && id == LPS22HH_ID)
      {
        uint8_t cfg = 0x22;                 /* ODR = 10 Hz, BDU = 1 */
        st = HAL_I2C_Mem_Write(&hi2c2, LPS22HH_ADDR, LPS22HH_CTRL_REG1,
                               I2C_MEMADD_SIZE_8BIT, &cfg, 1, 100);
        ready = (st == HAL_OK);
      }
      len = snprintf(msg, sizeof msg, "init: st=%d err=0x%02lX id=0x%02X\r\n",
                     (int)st, (unsigned long)hi2c2.ErrorCode, id);
      HAL_UART_Transmit(&huart1, (uint8_t *)msg, len, 100);
      if (!ready) i2c2_recover();
    }
    else
    {
      uint8_t buf[5];
      st = lps_read(LPS22HH_PRESS_OUT_XL, buf, 5);
      if (st == HAL_OK)
      {
        int32_t raw_p = ((int32_t)buf[2] << 16) | ((int32_t)buf[1] << 8) | buf[0];
        int16_t raw_t = (int16_t)(((uint16_t)buf[4] << 8) | buf[3]);
        int32_t p_c = (raw_p * 100) / 4096;   /* centi-hPa (4096 LSB/hPa) */
        int32_t t_c = raw_t;                  /* centi-°C  (100 LSB/°C)   */
        const char *sign = (t_c < 0) ? "-" : "";
        if (t_c < 0) t_c = -t_c;

        len = snprintf(msg, sizeof msg, "P=%ld.%02ld hPa  T=%s%ld.%02ld C\r\n",
                       (long)(p_c / 100), (long)(p_c % 100),
                       sign, (long)(t_c / 100), (long)(t_c % 100));
      }
      else
      {
        len = snprintf(msg, sizeof msg, "lecture: st=%d err=0x%02lX\r\n",
                       (int)st, (unsigned long)hi2c2.ErrorCode);
        ready = 0;
        i2c2_recover();
      }
      HAL_UART_Transmit(&huart1, (uint8_t *)msg, len, 100);
    }
    HAL_Delay(1000);
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI
                              |RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{
  HAL_PWREx_EnableVddIO2();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
