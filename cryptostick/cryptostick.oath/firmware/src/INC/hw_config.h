/*
* Author: Copyright (C) STMicroelectronics	 			Date:	04/27/2009
*												 MCD Application Team			Version V3.0.1
*
* This file is part of GPF Crypto Stick.
*
* GPF Crypto Stick is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* GPF Crypto Stick is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with GPF Crypto Stick. If not, see <http://www.gnu.org/licenses/>.
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported define -----------------------------------------------------------*/
#define BULK_MAX_PACKET_SIZE  0x00000040
#define LED_ON_INTERVAL 2000
#define LED_OFF_INTERVAL 500

/* Exported functions ------------------------------------------------------- */
void Set_System(void);
void Set_USBClock(void);
void Enter_LowPowerMode(void);
void Leave_LowPowerMode(void);
void USB_Interrupts_Config(void);
void USB_Cable_Config (FunctionalState NewState);
void USB_Disconnect_Config(void);
void Get_SerialNum(void);
void MAL_Config(void);
char HexToAscii (uint8_t nHex);
void SwitchSmartcardLED (FunctionalState NewState);
void SwitchOATHLED (FunctionalState NewState);
void StartBlinkingOATHLED(uint8_t times);
void DisableFirmwareDownloadPort (void);
void DisableSmartcardLED (void);
void EnableSmartcardLED (void);
void EnableOATHLED (void);
void EnableButton (void);
uint8_t ReadButton(void);

/* External variables --------------------------------------------------------*/

extern uint64_t currentTime;
extern uint64_t lastOATHBlinkTime;
extern __IO uint32_t cardSerial;
extern uint8_t blinkOATHLEDTimes;

#endif  /*__HW_CONFIG_H*/
