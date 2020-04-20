/***************************************************************************//**
 * @file
 * @brief LESENSE multi channel demo for EFM32GG11. This project initalizes four
 *        LESENSE channels and scans through all of them. Whenever a positive edge
 *        is detected on one of the channels, an interrupt will trigger.
 * @version 5.5.0
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Labs, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "em_device.h"
#include "em_acmp.h"
#include "em_assert.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_core.h"
#include "em_lesense.h"
#include "em_pcnt.h"

#include "bspconfig.h"
#include "bsp.h"
#include "bsp_trace.h"

/***************************************************************************//**
 * Macro definitions*/
 /******************************************************************************/

  #define LESENSE_SCAN_FREQ 20

/***************************************************************************//**
 * Global variables*/
 /******************************************************************************/

  volatile uint32_t setflag;	//set flag for LED0 toggle for GG11

/***************************************************************************//**/
 /* Prototypes*/
 /******************************************************************************/

/************************************************************************************//*
 * @brief  Sets up the ACMP to count LC sensor pulses
 **************************************************************************************/
static void setupACMP(void)
{
  // ACMP configuration constant table.
  static const ACMP_Init_TypeDef initACMP =
  {
    .fullBias                 = true,                  // fullBias
    .biasProg                 = 0x1F,                  // biasProg
    .interruptOnFallingEdge   = false,                 // interrupt on rising edge
    .interruptOnRisingEdge    = false,                 // interrupt on falling edge
    .inputRange               = acmpInputRangeFull,    // Full ACMP rang
    .accuracy                 = acmpAccuracyHigh,      // high accuracy
    .powerSource              = acmpPowerSourceAvdd,   // Use AVDD as power source, default to 3.3V
    .hysteresisLevel_0        = acmpHysteresisLevel0,  // hysteresis level 0
    .hysteresisLevel_1        = acmpHysteresisLevel0,  // hysteresis level 1
    .vlpInput                 = acmpVLPInputVADIV,     // Use VADIV as the VLP input source.
    .inactiveValue            = false,                 // no inactive value
    .enable                   = true                   // Enable after init
  };

  static const ACMP_VAConfig_TypeDef initVa =
  {
    acmpVAInputVDD,                                    // Use VDD as input for VA
    32,                                                // VA divider when ACMP output is 0, VDD/2
    32                                                 // VA divider when ACMP output is 1, VDD/2
  };

  CMU_ClockEnable(cmuClock_ACMP0, true);

  //Initialize ACMP
  ACMP_Init(ACMP0, &initACMP);

  //Setup VADIV
  ACMP_VASetup(ACMP0, &initVa);

  //Set ACMP0 CH0
  ACMP_ChannelSet(ACMP0, acmpInputVADIV, acmpInputAPORT0XCH0);

  //Set ACMP0 CH1
  ACMP_ChannelSet(ACMP0, acmpInputVADIV, acmpInputAPORT0XCH1);

  //Set ACMP0 CH2
  ACMP_ChannelSet(ACMP0, acmpInputVADIV, acmpInputAPORT0XCH2);

  //Set ACMP0 CH3
  ACMP_ChannelSet(ACMP0, acmpInputVADIV, acmpInputAPORT0XCH3);

  //Enable LESENSE control of ACMP
  ACMP_ExternalInputSelect(ACMP0, acmpExternalInputAPORT0X);
}

/**************************************************************************//**
 * @brief GPIO initialization
 *****************************************************************************/
void initGPIO(void)
{
  // Enable GPIO clock
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Configure LESENSE channel 0-3 pins as input with filter enabled
  // LESENSE channel 0-3 can be mapped to either ACMP0 port or ACMP1 port when only one of the ACMP is used
  GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPullFilter, 0);
  GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPullFilter, 0);
  GPIO_PinModeSet(gpioPortC, 2, gpioModeInputPullFilter, 0);
  GPIO_PinModeSet(gpioPortC, 3, gpioModeInputPullFilter, 0);

  //Configure LED0 for output
  GPIO_PinModeSet(BSP_GPIO_LED0_PORT, BSP_GPIO_LED0_PIN, gpioModePushPull, 1);
}

/**********************************************************************************************//*
 * @brief  Sets up the LESENSE
 ************************************************************************************************/
static void setupLESENSE(void)
{
  // LESENSE configuration structure
  static const LESENSE_Init_TypeDef initLesense =
  {
    .coreCtrl         =
    {
       .scanStart    = lesenseScanStartPeriodic,                // set scan to periodic scan
	   .prsSel       = lesensePRSCh0,                           // PRS selection channel 0
	   .scanConfSel  = lesenseScanConfDirMap,                   // direct scan configuration register usage
	   .invACMP0     = false,                                   // no invert ACMP0
	   .invACMP1     = false,                                   // no invert ACMP1
	   .dualSample   = false,                                   // no dualSample
	   .storeScanRes = false,                                   // do not Store SCANERS in RAM after each scan
	   .bufOverWr    = true,                                    // always write to buffer even if it is full
	   .bufTrigLevel = lesenseBufTrigHalf,                      // set DMA and interrupt flag when buffer is half full
	   .wakeupOnDMA  = lesenseDMAWakeUpDisable,                 // Disable DMA wakeup
	   .biasMode     = lesenseBiasModeDutyCycle,                // Duty cycle bias between low power and high accuracy mode
	   .debugRun     = false                                    // LESENSE not running on debugging mode
    },

	.timeCtrl          =
	{
      .startDelay      = 0
	},

	.perCtrl           =
	{
	  .acmp0Mode       = lesenseACMPModeMux,                    // Enable ACMP0 MUX
	  .acmp1Mode       = lesenseACMPModeDisable,                // Disable ACMP1
	  .warmupMode      = lesenseWarmupModeNormal                // Normal warmup
	},
  };

  // Channel configuration
  static const LESENSE_ChDesc_TypeDef initLesenseCh =
  {
    .enaScanCh     = true,
	.enaPin        = true,
	.enaInt        = true,
	.chPinExMode   = lesenseChPinExDis,       //Disable excitation
	.chPinIdleMode = lesenseChPinIdleDis,     //Disable idle
	.useAltEx      = false,
	.shiftRes      = false,
	.invRes        = false,
	.storeCntRes   = true,
	.exClk         = lesenseClkHF,
	.sampleClk     = lesenseClkLF,
	.exTime        = 0x07,
	.sampleDelay   = 0x0A,
	.measDelay     = 0x00,
	.acmpThres     = 0x00,
	.sampleMode    = lesenseSampleModeACMP,	  //Sample directly from ACMP
	.intMode       = lesenseSetIntPosEdge,    //Interrupt on positive edge
	.cntThres      = 0x0000,
	.compMode      = lesenseCompModeLess,     //Compare mode less than threshold
  };

  // Use LFXO as LESENSE clock source since it is already used by the RTCC
  CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
  CMU_ClockEnable(cmuClock_HFLE, true);
  CMU_ClockEnable(cmuClock_LESENSE, true);

  //Initialize LESENSE interface _with_ RESET
  LESENSE_Init(&initLesense, true);

  // Configure channel 0, 1, 2, 3
  LESENSE_ChannelConfig(&initLesenseCh, 0);
  LESENSE_ChannelConfig(&initLesenseCh, 1);
  LESENSE_ChannelConfig(&initLesenseCh, 2);
  LESENSE_ChannelConfig(&initLesenseCh, 3);

  // Set scan frequency to 20hz
  LESENSE_ScanFreqSet(0, LESENSE_SCAN_FREQ);

  // Set clock divisor for LF clock
  LESENSE_ClkDivSet(lesenseClkLF, lesenseClkDiv_2);
  // Set clock divisor for HF clock
  LESENSE_ClkDivSet(lesenseClkHF, lesenseClkDiv_1);

  //Enable interrupt in NVIC
  NVIC_EnableIRQ(LESENSE_IRQn);

  // Start continuous scan
  LESENSE_ScanStart();
}

/***************************************************************************//**
 * @brief  Main function
 ******************************************************************************/
int main(void)
{
  /*Initialize DCDC for series 1 board*/
  EMU_DCDCInit_TypeDef dcdcInit = EMU_DCDCINIT_STK_DEFAULT;

  /* Chip errata */
  CHIP_Init();

  // Init DCDC regulator with kit specific parameters
  EMU_DCDCInit(&dcdcInit);

  /* Initialize LED Driver*/
  BSP_LedsInit();

  /* Initialize GPIO */
  initGPIO();
  /* Initialize ACMP */
  setupACMP();
  /*Initialize LESENSE*/
  setupLESENSE();

  while(1) {
    EMU_EnterEM2(false);        //put system into EM2 mode
  }
}

void LESENSE_IRQHandler(void)
{
  /* Clear all LESENSE interrupt flag */
  LESENSE_IntClear(LESENSE_IFC_CH0
		  |LESENSE_IFC_CH1
		  |LESENSE_IFC_CH2
		  |LESENSE_IFC_CH3);

  if(setflag == 1) {
    GPIO_PinOutClear(BSP_GPIO_LED0_PORT, BSP_GPIO_LED0_PIN);	//note for GG11 clear = set
    setflag = 0;
  }
  else {
    GPIO_PinOutSet(BSP_GPIO_LED0_PORT, BSP_GPIO_LED0_PIN);
    setflag = 1;
  }
}

