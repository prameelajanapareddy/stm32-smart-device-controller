/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
/* USER CODE BEGIN Variables */
typedef enum { MODE_AUTO, MODE_MANUAL } SystemMode_t;

volatile SystemMode_t systemMode = MODE_AUTO;
volatile uint8_t currentTemp = 20;
volatile uint8_t tempThreshold = 30;
volatile uint8_t criticalThreshold = 40;

volatile uint8_t fanState = 0;
volatile uint8_t heaterState = 0;
volatile uint8_t alarmAuxState = 0;

extern UART_HandleTypeDef huart1;
extern char rxLine[];
extern volatile uint8_t rxLineReady;

static void UartTask_SendString(const char *s)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), 100);
}
/* USER CODE END Variables */
/* Definitions for SensorTask */
osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for UartTask */
osThreadId_t UartTaskHandle;
const osThreadAttr_t UartTask_attributes = {
  .name = "UartTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sensorQueue */
osMessageQueueId_t sensorQueueHandle;
const osMessageQueueAttr_t sensorQueue_attributes = {
  .name = "sensorQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartSensorTask(void *argument);
void StartUartTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of sensorQueue */
  sensorQueueHandle = osMessageQueueNew (4, 4, &sensorQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of SensorTask */
  SensorTaskHandle = osThreadNew(StartSensorTask, NULL, &SensorTask_attributes);

  /* creation of UartTask */
  UartTaskHandle = osThreadNew(StartUartTask, NULL, &UartTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartSensorTask */
/**
  * @brief  Function implementing the SensorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  uint32_t flags;
  for(;;)
  {
    /* Block here until TIM2's ISR wakes us - this is what keeps the task
       from spinning uselessly; it sleeps until there's real work to do */
    flags = osThreadFlagsWait(0x0001, osFlagsWaitAny, osWaitForever);

    if (flags & 0x0001)
    {
      /* Simulate a drifting temperature reading (0-44, wraps around) */
      currentTemp = (currentTemp + 1) % 45;

      if (systemMode == MODE_AUTO)
      {
        fanState = (currentTemp > tempThreshold) ? 1 : 0;
        heaterState = (currentTemp < 10) ? 1 : 0;

        HAL_GPIO_WritePin(ACT_FAN_GPIO_Port, ACT_FAN_Pin,
                           fanState ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(ACT_HEATER_GPIO_Port, ACT_HEATER_Pin,
                           heaterState ? GPIO_PIN_SET : GPIO_PIN_RESET);
      }

      /* Critical alarm overrides mode entirely - LED blinks fast */
      if (currentTemp > criticalThreshold)
      {
        HAL_GPIO_TogglePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin);
      }
      else
      {
        /* active-low LED, so SET = off */
        HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_SET);
      }

      /* Pack status into one uint32_t (matches our 4-byte queue item size)
         so UartTask can report it later via STATUS */
      uint32_t status = ((uint32_t)currentTemp)
                       | ((uint32_t)systemMode   << 8)
                       | ((uint32_t)fanState     << 16)
                       | ((uint32_t)heaterState  << 24);

      osMessageQueuePut(sensorQueueHandle, &status, 0, 0);
    }
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartUartTask */
/**
* @brief Function implementing the UartTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUartTask */
void StartUartTask(void *argument)
{
  /* USER CODE BEGIN StartUartTask */
  char txBuf[80];
  uint32_t flags;

  UartTask_SendString("\r\n=== Smart Device Controller Ready ===\r\n");

  for(;;)
  {
    flags = osThreadFlagsWait(0x0002, osFlagsWaitAny, osWaitForever);

    if ((flags & 0x0002) && rxLineReady)
    {
      rxLineReady = 0;

      if (strncmp(rxLine, "STATUS", 6) == 0)
      {
        uint32_t status;
        if (osMessageQueueGet(sensorQueueHandle, &status, NULL, 0) == osOK)
        {
          uint8_t temp  = status & 0xFF;
          uint8_t mode  = (status >> 8) & 0xFF;
          uint8_t fan   = (status >> 16) & 0xFF;
          uint8_t heat  = (status >> 24) & 0xFF;

          snprintf(txBuf, sizeof(txBuf),
                   "TEMP=%d MODE=%s FAN=%s HEATER=%s\r\n",
                   temp,
                   mode == MODE_AUTO ? "AUTO" : "MANUAL",
                   fan ? "ON" : "OFF",
                   heat ? "ON" : "OFF");
        }
        else
        {
          snprintf(txBuf, sizeof(txBuf), "TEMP=%d (no fresh reading)\r\n", currentTemp);
        }
        UartTask_SendString(txBuf);
      }
      else if (strncmp(rxLine, "MODE AUTO", 9) == 0)
      {
        systemMode = MODE_AUTO;
        UartTask_SendString("OK: mode=AUTO\r\n");
      }
      else if (strncmp(rxLine, "MODE MANUAL", 11) == 0)
      {
        systemMode = MODE_MANUAL;
        UartTask_SendString("OK: mode=MANUAL\r\n");
      }
      else if (strncmp(rxLine, "SET THRESHOLD", 13) == 0)
      {
        int val = atoi(rxLine + 14);
        tempThreshold = (uint8_t)val;
        snprintf(txBuf, sizeof(txBuf), "OK: threshold=%d\r\n", tempThreshold);
        UartTask_SendString(txBuf);
      }
      else if (strncmp(rxLine, "SET PA0 ", 8) == 0)
      {
        if (systemMode != MODE_MANUAL)
        {
          UartTask_SendString("ERR: switch to MODE MANUAL first\r\n");
        }
        else
        {
          fanState = (strstr(rxLine, "ON") != NULL) ? 1 : 0;
          HAL_GPIO_WritePin(ACT_FAN_GPIO_Port, ACT_FAN_Pin,
                             fanState ? GPIO_PIN_SET : GPIO_PIN_RESET);
          UartTask_SendString(fanState ? "OK: PA0=ON\r\n" : "OK: PA0=OFF\r\n");
        }
      }
      else if (strncmp(rxLine, "SET PA1 ", 8) == 0)
      {
        if (systemMode != MODE_MANUAL)
        {
          UartTask_SendString("ERR: switch to MODE MANUAL first\r\n");
        }
        else
        {
          heaterState = (strstr(rxLine, "ON") != NULL) ? 1 : 0;
          HAL_GPIO_WritePin(ACT_HEATER_GPIO_Port, ACT_HEATER_Pin,
                             heaterState ? GPIO_PIN_SET : GPIO_PIN_RESET);
          UartTask_SendString(heaterState ? "OK: PA1=ON\r\n" : "OK: PA1=OFF\r\n");
        }
      }
      else if (strncmp(rxLine, "SET PA2 ", 8) == 0)
      {
        if (systemMode != MODE_MANUAL)
        {
          UartTask_SendString("ERR: switch to MODE MANUAL first\r\n");
        }
        else
        {
          alarmAuxState = (strstr(rxLine, "ON") != NULL) ? 1 : 0;
          HAL_GPIO_WritePin(ACT_ALARM_AUX_GPIO_Port, ACT_ALARM_AUX_Pin,
                             alarmAuxState ? GPIO_PIN_SET : GPIO_PIN_RESET);
          UartTask_SendString(alarmAuxState ? "OK: PA2=ON\r\n" : "OK: PA2=OFF\r\n");
        }
      }
      else
      {
        UartTask_SendString("ERR: unknown command\r\n");
      }
    }
  }
  /* USER CODE END StartUartTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

