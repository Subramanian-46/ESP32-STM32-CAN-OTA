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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>   /* memcpy */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    OTA_IDLE,
    OTA_WAIT_FW_INFO,
    OTA_RECEIVING,
    OTA_VERIFYING,
    OTA_DONE
} OtaState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CAN_ID_DOWNLINK   0x200U   /* ESP32 -> STM32 */
#define CAN_ID_UPLINK     0x201U   /* STM32 -> ESP32 */

#define CMD_ENTER_UPDATE  0x01U
#define CMD_ACK           0x02U
#define CMD_FW_INFO       0x03U
#define CMD_DATA          0x04U
#define CMD_DATA_ACK      0x05U
#define CMD_DATA_NACK     0x06U
#define CMD_TRANSFER_DONE 0x07U
#define CMD_CRC_RESULT    0x08U
#define CMD_REBOOT        0x09U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan1;

/* USER CODE BEGIN PV */
volatile OtaState ota_state       = OTA_IDLE;
volatile uint32_t fw_size         = 0;
volatile uint16_t fw_version      = 0;
volatile uint16_t expected_seq    = 0;
volatile uint32_t bytes_received  = 0;

/* kept from the earlier basic handshake test, harmless to leave in */
volatile uint8_t rx_flag = 0;
uint8_t rx_buffer[8];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
/* USER CODE BEGIN PFP */
void Send_Ack(uint8_t cmd, uint16_t seq, uint8_t *extra, uint8_t extraLen);
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
  MX_FDCAN1_Init();
  /* USER CODE BEGIN 2 */

  /* --- Filter: accept CAN_ID_DOWNLINK (0x200) into RX FIFO0 --- */
  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0;
  sFilterConfig.FilterType   = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = CAN_ID_DOWNLINK;
  sFilterConfig.FilterID2    = 0x7FF;   /* exact-match mask */
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* Reject anything that doesn't match a configured filter */
  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                    FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	/* USER CODE END 3 */
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = ENABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 2;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 13;
  hfdcan1.Init.NominalTimeSeg2 = 2;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief Sends an 8-byte frame on CAN_ID_UPLINK: [cmd][seq_hi][seq_lo][extra...]
  * @param cmd      command/ack byte to send
  * @param seq      sequence number being acked/reported
  * @param extra    optional extra payload bytes (may be NULL)
  * @param extraLen number of extra bytes (0-5)
  * @retval None
  */
void Send_Ack(uint8_t cmd, uint16_t seq, uint8_t *extra, uint8_t extraLen)
{
    FDCAN_TxHeaderTypeDef TxHeader;
    uint8_t TxData[8] = {0};

    TxData[0] = cmd;
    TxData[1] = (seq >> 8) & 0xFF;
    TxData[2] = seq & 0xFF;
    if (extra != NULL && extraLen > 0)
    {
        if (extraLen > 5) extraLen = 5;
        memcpy(&TxData[3], extra, extraLen);
    }

    TxHeader.Identifier          = CAN_ID_UPLINK;
    TxHeader.IdType              = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
    TxHeader.DataLength          = FDCAN_DLC_BYTES_8;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker       = 0;

    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, TxData);
}

/**
  * @brief RX FIFO0 callback - dispatches incoming OTA protocol commands
  * @retval None
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    FDCAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];

    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0) return;
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) return;
    if (RxHeader.IdType != FDCAN_STANDARD_ID || RxHeader.Identifier != CAN_ID_DOWNLINK) return;

    uint8_t  cmd = RxData[0];
    uint16_t seq = (RxData[1] << 8) | RxData[2];

    switch (cmd)
    {
        case CMD_ENTER_UPDATE:
            ota_state      = OTA_WAIT_FW_INFO;
            expected_seq   = 0;
            bytes_received = 0;
            Send_Ack(CMD_ACK, seq, NULL, 0);
            break;

        case CMD_FW_INFO:
            /* payload layout: RxData[3..6] = size (32-bit, big-endian), RxData[7] unused for now */
            fw_size = ((uint32_t)RxData[3] << 24) | ((uint32_t)RxData[4] << 16) |
                      ((uint32_t)RxData[5] << 8)  |  (uint32_t)RxData[6];

            /* TODO (next step): erase the target flash region sized to fw_size here,
               respecting STM32G0 2KB page-erase granularity, before accepting DATA frames */

            ota_state = OTA_RECEIVING;
            Send_Ack(CMD_ACK, seq, NULL, 0);
            break;

        case CMD_DATA:
            if (ota_state != OTA_RECEIVING)
            {
                /* got a data frame before FW_INFO - ignore/nack */
                Send_Ack(CMD_DATA_NACK, expected_seq, NULL, 0);
                break;
            }

            if (seq == expected_seq)
            {
                uint8_t chunkLen = (uint8_t)(RxHeader.DataLength >> 16); /* HAL encodes DLC in bits [16:19] */
                if (chunkLen > 5) chunkLen = 5; /* our payload budget, bytes 3-7 */

                /* TODO (next step): HAL_FLASH_Program() RxData[3 .. 3+chunkLen-1] into the
                   next free position in the target flash region */

                bytes_received += chunkLen;
                expected_seq++;
                Send_Ack(CMD_DATA_ACK, seq, NULL, 0);
            }
            else
            {
                /* out of order / duplicate - tell the sender what we actually need next */
                Send_Ack(CMD_DATA_NACK, expected_seq, NULL, 0);
            }
            break;

        case CMD_TRANSFER_DONE:
        {
            ota_state = OTA_VERIFYING;

            /* TODO (next step): compute CRC32 over the written flash region and compare
               against the CRC32 sent in RxData[3..6]; result byte below is a placeholder */
            uint8_t crcOk = 1; /* placeholder until CRC check is implemented */
            Send_Ack(CMD_CRC_RESULT, seq, &crcOk, 1);

            if (crcOk)
            {
                ota_state = OTA_DONE;
                /* TODO: mark image valid, then reboot into new application */
            }
            break;
        }

        default:
            /* unknown command - ignore */
            break;
    }
}

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
