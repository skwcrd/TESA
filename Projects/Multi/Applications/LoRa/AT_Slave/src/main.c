 /*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Generic lora driver implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Gregory Cristian and Wael Guibene
*/
/******************************************************************************
  * @file    main.c
  * @author  MCD Application Team
  * @version V1.1.5
  * @date    30-March-2018
  * @brief   this is the main!
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hw.h"
#include "low_power_manager.h"
#include "lora.h"
#include "timeServer.h"
#include "version.h"
#include "command.h"
#include "at.h"
#include "lora.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/**
 * @brief LoRaWAN Adaptive Data Rate
 * @note Please note that when ADR is enabled the end-device should be static
 */
#define LORAWAN_ADR_ON                              1

/**
 * When fast wake up is enabled, the mcu wakes up in ~20us and
 * does not wait for the VREFINT to be settled. THis is ok for
 * most of the case except when adc must be used in this case before
 * starting the adc, you must make sure VREFINT is settled
 */
#define ENABLE_FAST_WAKEUP

/*!
 * CAYENNE_LPP is myDevices Application server.
 */
#define CAYENNE_LPP
#define LPP_DATATYPE_DIGITAL_INPUT  	0x0
#define LPP_DATATYPE_TEMPERATURE    	0x67
#define LPP_APP_PORT 									99

/*!
 * LoRaWAN default confirm state
 */
#define LORAWAN_DEFAULT_CONFIRM_MSG_STATE           LORAWAN_UNCONFIRMED_MSG

/*!
 * LoRaWAN default endNode class port
 */
#define LORAWAN_DEFAULT_CLASS                       CLASS_A

/*!
 * User application data buffer size
 */
#define LORAWAN_APP_DATA_BUFF_SIZE                           64

/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void Send( void );
/* start the tx process*/
static void LoraStartTx( void );
/* tx timer callback function*/
static void OnTxTimerEvent( void );
/* call back when LoRa has received a frame*/
static void LoraRxData(lora_AppData_t *AppData);
/* call back when LoRa endNode has just joined*/
static void LORA_HasJoined( void );
/* call back when LoRa endNode has just switch the class*/
static void LORA_ConfirmClass ( DeviceClass_t Class );
/* call back when server needs endNode to send a frame*/
static void LORA_TxNeeded ( void );
	
/* Private variables ---------------------------------------------------------*/
static TimerEvent_t TxTimer;
/*!
 * User application data
 */
static uint8_t AppDataBuff[LORAWAN_APP_DATA_BUFF_SIZE];

/*!
 * User application data structure
 */
static lora_AppData_t AppData = { AppDataBuff,  0 ,0 };

/* load call backs*/
static LoRaMainCallback_t LoRaMainCallbacks = { HW_GetBatteryLevel,
                                                HW_GetTemperatureLevel,
                                                HW_GetUniqueId,
                                                HW_GetRandomSeed,
                                                LoraRxData,
                                                LORA_HasJoined,
                                                LORA_ConfirmClass,
                                                LORA_TxNeeded };

/**
 * Initialises the Lora Parameters
 */
static LoRaParam_t LoRaParamInit = { 	LORAWAN_ADR_ON,
																			DR_0,
																			LORAWAN_PUBLIC_NETWORK };

                                    
                                    
/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Main program
 * @param  None
 * @retval None
 */

/* Implementation of the HAL_Init() using LL functions */
void HW_Main_Init()
{
  /* Configure Buffer cache, Flash prefetch,  Flash preread */
#if (BUFFER_CACHE_DISABLE != 0)
  LL_FLASH_EnableBuffers();
#endif /* BUFFER_CACHE_DISABLE */

#if (PREREAD_ENABLE != 0)
  LL_FLASH_EnablePreRead();
#endif /* PREREAD_ENABLE */

#if (PREFETCH_ENABLE != 0)
  LL_FLASH_EnablePrefetch();
#endif /* PREFETCH_ENABLE */

  /*
   * Init the low level hardware
   * - Power clock enable
   * - Disable PVD
   * - Enable the Ultra Low Power mode
   * - Support DBG mode
   * - Take into account Fast Wakeup Mode
   * - Initialize GPIO
   */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_DisablePVD();      /* Disable the Power Voltage Detector */
  LL_PWR_EnableUltraLowPower();   /* Enables the Ultra Low Power mode */
  LL_FLASH_EnableSleepPowerDown();

  /*
   * In debug mode, e.g. when DBGMCU is activated, Arm core has always clocks
   * And will not wait that the FLACH is ready to be read. It can miss in this
   * case the first instruction. To overcome this issue, the flash remain clcoked during sleep mode
   */
  DBG(LL_FLASH_DisableSleepPowerDown(); );

#ifdef ENABLE_FAST_WAKEUP
  LL_PWR_EnableFastWakeUp();
#else
  LL_PWR_DisableFastWakeUp();
#endif

  HW_GpioInit();
}

int main( void )
{
  /* STM32 HAL library initialization*/
  HW_Main_Init();

  /* Configure the system clock*/
  SystemClock_Config();

  /* Configure the hardware*/
  HW_Init();

  /* Configure Debug mode */
  DBG_Init();
  
  /* USER CODE BEGIN 1 */
  CMD_Init();
  /*Disable standby mode*/
  LPM_SetOffMode(LPM_APPLI_Id, LPM_Disable);
  
  PRINTF("ATtention command interface\n\r");
  /* USER CODE END 1 */

  /* Configure the Lora Stack*/
  LORA_Init(&LoRaMainCallbacks, &LoRaParamInit);
	
	LoraStartTx( );
  /* main loop*/
  while (1)
  {
		/* Handle UART commands */
    CMD_Process();
    /*
     * low power section
     */
    DISABLE_IRQ();
    /*
     * if an interrupt has occurred after DISABLE_IRQ, it is kept pending
     * and cortex will not enter low power anyway
     * don't go in low power mode if we just received a char
     */
    if ( (IsNewCharReceived() == RESET))
    {
#ifndef LOW_POWER_DISABLE
      LPM_EnterLowPower();
#endif
    }
    ENABLE_IRQ();

    /* USER CODE BEGIN 2 */
    /* USER CODE END 2 */
  }
}

static void Send( void )
{
  int16_t 	temperature = 0;
  
  while ( LORA_JoinStatus () != LORA_SET)
  {
    /*Not joined, try again later*/
    LORA_Join();
  }
	float TEMP_VALUE = 25.76;

#ifdef CAYENNE_LPP
  uint8_t cchannel=0;
  temperature 			= ( int16_t )( TEMP_VALUE * 10 );
  uint32_t i = 0;
	
  AppData.Port = LPP_APP_PORT;
	
	// TEMPERATURE SENSOR
  AppData.Buff[i++] = cchannel++;
  AppData.Buff[i++] = LPP_DATATYPE_TEMPERATURE; 
  AppData.Buff[i++] = ( temperature >> 8 ) & 0xFF;
  AppData.Buff[i++] = temperature & 0xFF;
  AppData.Buff[i++] = cchannel++;
  AppData.Buff[i++] = LPP_DATATYPE_DIGITAL_INPUT; 
  AppData.Buff[i++] = 37;
#endif  /* CAYENNE_LPP */
  AppData.BuffSize = i;
  
  LORA_send( &AppData, LORAWAN_DEFAULT_CONFIRM_MSG_STATE);
  
  /* USER CODE END 3 */
}

static void OnTxTimerEvent( void )
{
  Send( );
  /*Wait for next tx slot*/
  TimerStart( &TxTimer );
}

static void LoraStartTx( void )
{
	/* send everytime timer elapses */
	TimerInit( &TxTimer, OnTxTimerEvent );
	TimerSetValue( &TxTimer, 30000 );
	OnTxTimerEvent( );
}

static void LoraRxData(lora_AppData_t *AppData)
{
   set_at_receive(AppData->Port, AppData->Buff, AppData->BuffSize);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  Error_Handler();
}
#endif

static void LORA_HasJoined( void )
{
#if( OVER_THE_AIR_ACTIVATION != 0 )
  PRINTF("JOINED\n\r");
#endif
	LORA_RequestClass( LORAWAN_DEFAULT_CLASS );
}

static void LORA_ConfirmClass ( DeviceClass_t Class )
{
  DBG_PRINTF("switch to class %c done\n\r","ABC"[Class] );
}

static void LORA_TxNeeded ( void )
{
  DBG_PRINTF("Network Server is asking for an uplink transmission\n\r");
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
