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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "LTC6811.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

#define TOTAL_SEGMENT         1
#define CELL_PER_SEGMENT      17
#define TOTAL_IC              TOTAL_SEGMENT * 2
#define TOTAL_CELL            TOTAL_SEGMENT * CELL_PER_SEGMENT

typedef struct {
  float voltage;
  float temperature;
} Cell;

typedef struct {
  Cell cells[TOTAL_CELL];
} Bank;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ENABLED               1
#define DISABLED              0
#define DATALOG_ENABLED       1
#define DATALOG_DISABLED      0

#define TOTAL_SEGMENT         1
#define CELL_PER_SEGMENT      17
#define TOTAL_IC              TOTAL_SEGMENT * 2
#define TOTAL_CELL            TOTAL_SEGMENT * CELL_PER_SEGMENT

//ADC Command Configurations. See LTC681x.h for options.
#define ADC_OPT               ADC_OPT_DISABLED //!< ADC Mode option bit
#define ADC_CONVERSION_MODE   MD_7KHZ_3KHZ //!< ADC Mode
#define ADC_DCP               DCP_ENABLED //!< Discharge Permitted
#define CELL_CH_TO_CONVERT    CELL_CH_ALL //!< Channel Selection for ADC conversion
#define AUX_CH_TO_CONVERT     AUX_CH_ALL //!< Channel Selection for ADC conversion
#define STAT_CH_TO_CONVERT    STAT_CH_ALL //!< Channel Selection for ADC conversion
#define SEL_ALL_REG           REG_ALL //!< Register Selection
#define SEL_REG_A             REG_1 //!< Register Selection
#define SEL_REG_B             REG_2 //!< Register Selection

#define MEASUREMENT_LOOP_TIME 500 //!< Loop Time in milliseconds(ms)

//Under Voltage and Over Voltage Thresholds
#define OV_THRESHOLD          41000 //!< Over voltage threshold ADC Code. LSB = 0.0001 ---(4.1V)
#define UV_THRESHOLD          30000 //!< Under voltage threshold ADC Code. LSB = 0.0001 ---(3V)

//Loop Measurement Setup. These Variables are ENABLED or DISABLED. Remember ALL CAPS
#define WRITE_CONFIG          DISABLED  //!< This is to ENABLED or DISABLED writing into to configuration registers in a continuous loop
#define READ_CONFIG           DISABLED //!< This is to ENABLED or DISABLED reading the configuration registers in a continuous loop
#define MEASURE_CELL          ENABLED //!< This is to ENABLED or DISABLED measuring the cell voltages in a continuous loop
#define MEASURE_AUX           ENABLED //!< This is to ENABLED or DISABLED reading the auxiliary registers in a continuous loop
#define MEASURE_STAT          DISABLED //!< This is to ENABLED or DISABLED reading the status registers in a continuous loop
#define PRINT_PEC             DISABLED //!< This is to ENABLED or DISABLED printing the PEC Error Count in a continuous loop

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void measurement_loop(uint8_t datalog_en);
void print_menu(void);
void print_wrconfig(void);
void print_rxconfig(void);
void print_cells(uint8_t datalog_en);
void print_aux(uint8_t datalog_en);
void print_stat(void);
void print_sumofcells(void);
void check_mux_fail(void);
void print_selftest_errors(uint8_t adc_reg, int8_t error);
void print_overlap_results(int8_t error);
void print_digital_redundancy_errors(uint8_t adc_reg, int8_t error);
void print_open_wires(void);
void print_pec_error_count(void);
int8_t select_s_pin(void);
void print_wrpwm(void);
void print_rxpwm(void);
void print_wrsctrl(void);
void print_rxsctrl(void);
void print_wrcomm(void);
void print_rxcomm(void);
void print_conv_time(uint32_t conv_time);
void check_error(int error);
void serial_print_text(char data[]);
void serial_print_hex(uint8_t data);
char read_hex(void);
char get_char(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

cell_asic BMS_IC[TOTAL_IC]; //!< Global Battery Variable

uint8_t REFON = 1; //!< Reference Powered Up Bit
uint8_t ADCOPT = 0; //!< ADC Mode option bit
uint8_t GPIOBITS_A[5] = {1, 1, 0, 0, 0}; //!< GPIO Pin Control // Gpio 1,2,3,4,5    First two are ADC inputs, set to TRUE; latter 3 are MUX sel, outputs
uint16_t UV = UV_THRESHOLD; //!< Under-voltage Comparison Voltage
uint16_t OV = OV_THRESHOLD; //!< Over-voltage Comparison Voltage
uint8_t DCCBITS_A[12] = { false, false, false, false, false, false, false, false, false, false, false, false }; //!< Discharge cell switch //Dcc 1,2,3,4,5,6,7,8,9,10,11,12
uint8_t DCTOBITS[4] = { true, false, true, false }; //!< Discharge time value // Dcto 0,1,2,3 // Programed for 4 min

float getVoltage(uint16_t val) {
  return val == 65535 ? -42 : val * 0.0001;
}

float getTemperature(uint16_t val) {
  return val == 65535 ? -42 : val * 0.0001;
}

void print_rxconfig(void) {

  char str[64];

  sprintf(str, "Received Configuration \r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

  for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
    sprintf(str, "CFGA IC %d ", current_ic + 1);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

    for (int i = 0; i < 6; i++) {
      sprintf(str, ", 0x%d", BMS_IC[current_ic].config.rx_data[i]);
      HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);
    }

    sprintf(str, ", Received PEC: 0x%d, 0x%d\r\n", BMS_IC[current_ic].config.rx_data[6],
        BMS_IC[current_ic].config.rx_data[7]);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);
  }
}

void print_wrconfig(void) {
  int cfg_pec;

  char str[64];

  sprintf(str, "Written Configuration: \r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

  for (int current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
    sprintf(str, "CFGA IC %d", current_ic + 1);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

    for (int i = 0; i < 6; i++) {
      sprintf(str, "0x%d", BMS_IC[current_ic].config.tx_data[i]);
      HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

    }
    cfg_pec = pec15_calc(6, &BMS_IC[current_ic].config.tx_data[0]);

    sprintf(str, ", Calculated PEC: 0x%d, 0x%d\r\n", (uint8_t) (cfg_pec >> 8), cfg_pec);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);
  }
}

int8_t temp_poll(int8_t temperature_ch)
{
  int8_t error = 0;
  LTC6811_init_cfg(TOTAL_IC, BMS_IC);
  GPIOBITS_A[4] = (temperature_ch >> 2) & 0b1;
  GPIOBITS_A[3] = (temperature_ch >> 1) & 0b1;
  GPIOBITS_A[2] = (temperature_ch >> 0) & 0b1;
  GPIOBITS_A[1] = 0b1;
  GPIOBITS_A[0] = 0b1;
  for (uint8_t i = 0; i < TOTAL_IC; i ++)
  {
    LTC6811_set_cfgr(i, BMS_IC, REFON, ADCOPT, GPIOBITS_A, DCCBITS_A, DCTOBITS, UV, OV);
  }

  LTC6811_reset_crc_count(TOTAL_IC, BMS_IC);
  LTC6811_init_reg_limits(TOTAL_IC, BMS_IC);

  wakeup_sleep(TOTAL_IC);
  LTC6811_wrcfg(TOTAL_IC, BMS_IC);

  wakeup_idle(TOTAL_IC);
  LTC6811_adax(ADC_CONVERSION_MODE, AUX_CH_ALL);
  LTC6811_pollAdc();
  wakeup_idle(TOTAL_IC);
  error = LTC6811_rdaux(SEL_ALL_REG, TOTAL_IC, BMS_IC); // Set to read back all aux registers
  return error;
}

void measurement_loop(uint8_t datalog_en) {

  Bank bank;

  int8_t error = 0;

  char str[128];



  LTC6811_init_cfg(TOTAL_IC, BMS_IC);
  uint8_t temperature_ch = 0;
  GPIOBITS_A[4] = (temperature_ch >> 2) & 0b1;
  GPIOBITS_A[3] = (temperature_ch >> 1) & 0b1;
  GPIOBITS_A[2] = (temperature_ch >> 0) & 0b1;
  GPIOBITS_A[1] = 0b1;
  GPIOBITS_A[0] = 0b1;
  LTC6811_set_cfgr(0, BMS_IC, REFON, ADCOPT, GPIOBITS_A, DCCBITS_A, DCTOBITS, UV, OV);
  LTC6811_set_cfgr(1, BMS_IC, REFON, ADCOPT, GPIOBITS_A, DCCBITS_A, DCTOBITS, UV, OV);

  LTC6811_reset_crc_count(TOTAL_IC, BMS_IC);
  LTC6811_init_reg_limits(TOTAL_IC, BMS_IC);

  wakeup_sleep(TOTAL_IC);
  LTC6811_wrcfg(TOTAL_IC, BMS_IC);



  wakeup_idle(TOTAL_IC);
  LTC6811_adcv(ADC_CONVERSION_MODE, ADC_DCP, CELL_CH_TO_CONVERT);
  LTC6811_pollAdc();
  wakeup_idle(TOTAL_IC);
  error = LTC6811_rdcv(SEL_ALL_REG, TOTAL_IC, BMS_IC);

  for (uint8_t seg = 0; seg < TOTAL_SEGMENT; seg += 1)
  {
    bank.cells[seg * 17 + 16].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[0]);
    bank.cells[seg * 17 + 15].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[1]);
    bank.cells[seg * 17 + 14].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[2]);
    bank.cells[seg * 17 + 13].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[3]);
    bank.cells[seg * 17 + 12].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[6]);
    bank.cells[seg * 17 + 11].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[7]);
    bank.cells[seg * 17 + 10].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[8]);
    bank.cells[seg * 17 + 9 ].voltage = getVoltage(BMS_IC[seg * 2].cells.c_codes[9]);

    bank.cells[seg * 17 + 8 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[0]);
    bank.cells[seg * 17 + 7 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[1]);
    bank.cells[seg * 17 + 6 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[2]);
    bank.cells[seg * 17 + 5 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[3]);
    bank.cells[seg * 17 + 4 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[4]);
    bank.cells[seg * 17 + 3 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[6]);
    bank.cells[seg * 17 + 2 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[7]);
    bank.cells[seg * 17 + 1 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[8]);
    bank.cells[seg * 17 + 0 ].voltage = getVoltage(BMS_IC[seg * 2 + 1].cells.c_codes[9]);
  }

  temp_poll(0);
  for (uint8_t seg = 0; seg < TOTAL_SEGMENT; seg += 1) 
  {
    bank.cells[seg * 17 + 12].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[0]);
    bank.cells[seg * 17 + 16].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[1]);
    bank.cells[seg * 17 + 4 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[0]);
    bank.cells[seg * 17 + 8 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[1]);
  }

  temp_poll(1);
  for (uint8_t seg = 0; seg < TOTAL_SEGMENT; seg += 1) 
  {
    bank.cells[seg * 17 + 11].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[0]);
    bank.cells[seg * 17 + 15].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[1]);
    bank.cells[seg * 17 + 3 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[0]);
    bank.cells[seg * 17 + 7 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[1]);
  }

  temp_poll(2);
  for (uint8_t seg = 0; seg < TOTAL_SEGMENT; seg += 1) 
  {
    bank.cells[seg * 17 + 10].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[0]);
    bank.cells[seg * 17 + 14].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[1]);
    bank.cells[seg * 17 + 2 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[0]);
    bank.cells[seg * 17 + 6 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[1]);
  }

  temp_poll(3);
  for (uint8_t seg = 0; seg < TOTAL_SEGMENT; seg += 1) 
  {
    bank.cells[seg * 17 + 9 ].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[0]);
    bank.cells[seg * 17 + 13].temperature = getTemperature(BMS_IC[seg * 2    ].aux.a_codes[1]);
    bank.cells[seg * 17 + 1 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[0]);
    bank.cells[seg * 17 + 5 ].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[1]);
  }

  temp_poll(4);
  for (uint8_t seg = 0; seg < TOTAL_SEGMENT; seg += 1) 
  bank.cells[seg * 17].temperature = getTemperature(BMS_IC[seg * 2 + 1].aux.a_codes[0]);

  sprintf(str, "======= Bank %d ========\r\n", 0);
  HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

  sprintf(str, "Cells:\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

  for (uint16_t cell_idx=0; cell_idx<17; cell_idx+=1) {
    sprintf(str, "%d\t", cell_idx + 1);
//    sprintf(str, "%01X\t", cell_idx + 1);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);
  }

  sprintf(str, "\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);


  for (uint16_t cell_idx=0; cell_idx<17; cell_idx+=1) {
    sprintf(str, "%.3f\t", bank.cells[cell_idx].voltage);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);
  }

  sprintf(str, "\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);


  for (uint16_t cell_idx=0; cell_idx<17; cell_idx+=1) {
    sprintf(str, "%.3f\t", bank.cells[cell_idx].temperature);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);
  }


    sprintf(str, "\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

    sprintf(str, "Vref2: %.3f\r\n\r\n", BMS_IC[0].aux.a_codes[5] * 0.0001);
    HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 100);

  HAL_Delay(MEASUREMENT_LOOP_TIME);

}


/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

//  Serial.begin(115200);
//  quikeval_SPI_connect();
//  spi_enable(SPI_CLOCK_DIV16); // This will set the Linduino to have a 1MHz Clock
  LTC6811_init_cfg(TOTAL_IC, BMS_IC);
  uint8_t temperature_ch=0;
        GPIOBITS_A[4] = (temperature_ch >> 2) & 0b1;
        GPIOBITS_A[3] = (temperature_ch >> 1) & 0b1;
        GPIOBITS_A[2] = (temperature_ch >> 0) & 0b1;
        GPIOBITS_A[1] = 0b1;
        GPIOBITS_A[0] = 0b1;
        LTC6811_set_cfgr(0, BMS_IC, REFON, ADCOPT, GPIOBITS_A, DCCBITS_A, DCTOBITS, UV, OV);

    //    temperatures[temperature_ch] = BMS_IC[ic_idx].aux.a_codes[1];


  LTC6811_reset_crc_count(TOTAL_IC, BMS_IC);
  LTC6811_init_reg_limits(TOTAL_IC, BMS_IC);
//  print_menu();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    measurement_loop(1);
    HAL_Delay(1000);
  }

  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = { 0 };

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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