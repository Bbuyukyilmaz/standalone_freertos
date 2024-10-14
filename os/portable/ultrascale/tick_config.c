/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Xilinx includes. */
#include "xttcps.h"
#include "xscugic.h"

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
		__attribute__((weak));
void vPortEnableInterrupt( uint8_t ucInterruptID );

BaseType_t xPortInstallInterruptHandler( uint8_t ucInterruptID, XInterruptHandler pxHandler, void *pvCallBackRef );
/*
 * Initialise the interrupt controller instance.
 */
static int32_t prvInitialiseInterruptController( void );

/* Ensure the interrupt controller instance variable is initialised before it is
 * used, and that the initialisation only happens once.
 */
static int32_t prvEnsureInterruptControllerIsInitialised( void );

static int32_t lInterruptControllerInitialised = pdFALSE;

/* Timer used to generate the tick interrupt. */
static XTtcPs xTimerInstance;
static XScuGic xInterruptController;
/*-----------------------------------------------------------*/

void vConfigureTickInterrupt( void )
{
	BaseType_t xStatus;
	XTtcPs_Config *pxTimerConfiguration;
	XInterval usInterval;
	uint8_t ucPrescale;
	const uint8_t ucLevelSensitive = 1;

		pxTimerConfiguration = XTtcPs_LookupConfig( configTIMER_ID );

		/* Initialise the device. */
		xStatus = XTtcPs_CfgInitialize( &xTimerInstance, pxTimerConfiguration, pxTimerConfiguration->BaseAddress );

		if( xStatus != XST_SUCCESS )
		{
			/* Not sure how to do this before XTtcPs_CfgInitialize is called as
			*xRTOSTickTimerInstance is set within XTtcPs_CfgInitialize(). */
			XTtcPs_Stop( &xTimerInstance );
			xStatus = XTtcPs_CfgInitialize( &xTimerInstance, pxTimerConfiguration, pxTimerConfiguration->BaseAddress );
			configASSERT( xStatus == XST_SUCCESS );
		}

		/* Set the options. */
		XTtcPs_SetOptions( &xTimerInstance, ( XTTCPS_OPTION_INTERVAL_MODE | XTTCPS_OPTION_WAVE_DISABLE ) );
		/*
		 * The Xilinx implementation of generating run time task stats uses the same timer used for generating
		 * FreeRTOS ticks. In case user decides to generate run time stats the timer time out interval is changed
		 * as "configured tick rate * 10". The multiplying factor of 10 is hard coded for Xilinx FreeRTOS ports.
		 */
	#if (configGENERATE_RUN_TIME_STATS == 1)
		XTtcPs_CalcIntervalFromFreq( &xTimerInstance, configTICK_RATE_HZ*10, &usInterval, &ucPrescale );
	#else
		XTtcPs_CalcIntervalFromFreq( &xTimerInstance, configTICK_RATE_HZ, &( usInterval ), &( ucPrescale ) );
	#endif

		/* Set the interval and prescale. */
		XTtcPs_SetInterval( &xTimerInstance, usInterval );
		XTtcPs_SetPrescaler( &xTimerInstance, ucPrescale );

		xPortInstallInterruptHandler((uint8_t)configTIMER_INTERRUPT_ID,
						( Xil_InterruptHandler ) FreeRTOS_Tick_Handler,
						( void * ) &xTimerInstance);
		/* The priority must be the lowest possible. */
		XScuGic_SetPriorityTriggerType( &xInterruptController, configTIMER_INTERRUPT_ID, portLOWEST_USABLE_INTERRUPT_PRIORITY << portPRIORITY_SHIFT, ucLevelSensitive );

		vPortEnableInterrupt(configTIMER_INTERRUPT_ID);

		/* Enable the interrupts in the timer. */
		XTtcPs_EnableInterrupts( &xTimerInstance, XTTCPS_IXR_INTERVAL_MASK );

		/* Start the timer. */
		XTtcPs_Start( &xTimerInstance );
}
/*-----------------------------------------------------------*/

void vClearTickInterrupt( void )
{
volatile uint32_t ulInterruptStatus;

	/* Read the interrupt status, then write it back to clear the interrupt. */
	ulInterruptStatus = XTtcPs_GetInterruptStatus( &xTimerInstance );
	XTtcPs_ClearInterruptStatus( &xTimerInstance, ulInterruptStatus );
	__asm volatile( "DSB SY" );
	__asm volatile( "ISB SY" );
}
/*-----------------------------------------------------------*/

void vApplicationIRQHandler( uint32_t ulICCIAR )
{
extern XScuGic_Config XScuGic_ConfigTable[];
static const XScuGic_VectorTableEntry *pxVectorTable = XScuGic_ConfigTable[ XPAR_SCUGIC_SINGLE_DEVICE_ID ].HandlerTable;
uint32_t ulInterruptID;
const XScuGic_VectorTableEntry *pxVectorEntry;

	/* Interrupts cannot be re-enabled until the source of the interrupt is
	cleared. The ID of the interrupt is obtained by bitwise ANDing the ICCIAR
	value with 0x3FF. */
	ulInterruptID = ulICCIAR & 0x3FFUL;
	if( ulInterruptID < XSCUGIC_MAX_NUM_INTR_INPUTS )
	{
		/* Call the function installed in the array of installed handler
		functions. */
		pxVectorEntry = &( pxVectorTable[ ulInterruptID ] );
		configASSERT( pxVectorEntry );
		pxVectorEntry->Handler( pxVectorEntry->CallBackRef );
	}
}


void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
/* Attempt to prevent the handle and name of the task that overflowed its stack
from being optimised away because they are not used. */
volatile TaskHandle_t xOverflowingTaskHandle = xTask;
volatile char *pcOverflowingTaskName = pcTaskName;

	( void ) xOverflowingTaskHandle;
	( void ) pcOverflowingTaskName;

	xil_printf( "HALT: Task %s overflowed its stack.", pcOverflowingTaskName );
	portDISABLE_INTERRUPTS();
	for( ;; );
}

#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
BaseType_t xPortInstallInterruptHandler( uint8_t ucInterruptID, XInterruptHandler pxHandler, void *pvCallBackRef )
#else
BaseType_t xPortInstallInterruptHandler( uint16_t ucInterruptID, XInterruptHandler pxHandler, void *pvCallBackRef )
#endif
{
	int32_t lReturn;

	/* An API function is provided to install an interrupt handler */
	lReturn = prvEnsureInterruptControllerIsInitialised();
	if ( lReturn == pdPASS ) {
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
		lReturn = XScuGic_Connect( &xInterruptController, ucInterruptID, pxHandler, pvCallBackRef );
#else
		lReturn = XConnectToInterruptCntrl(ucInterruptID, pxHandler, pvCallBackRef, IntrControllerAddr);
#endif
	}
	if ( lReturn == XST_SUCCESS ) {
		lReturn = pdPASS;
	}
	configASSERT( lReturn == pdPASS );

	return lReturn;
}

static int32_t prvEnsureInterruptControllerIsInitialised( void )
{
	int32_t lReturn;

	/* Ensure the interrupt controller instance variable is initialised before
	it is used, and that the initialisation only happens once. */
	if ( lInterruptControllerInitialised != pdTRUE ) {
		lReturn = prvInitialiseInterruptController();

		if ( lReturn == pdPASS ) {
			lInterruptControllerInitialised = pdTRUE;
		}
	}
	else {
		lReturn = pdPASS;
	}

	return lReturn;
}


static int32_t prvInitialiseInterruptController( void )
{
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
	BaseType_t xStatus;
	XScuGic_Config *pxGICConfig;
	Xil_ExceptionInit();
	/* Initialize the interrupt controller driver. */
	pxGICConfig = XScuGic_LookupConfig( configINTERRUPT_CONTROLLER_DEVICE_ID );
	xStatus = XScuGic_CfgInitialize( &xInterruptController, pxGICConfig, pxGICConfig->CpuBaseAddress );
	/* Connect the interrupt controller interrupt handler to the hardware
	interrupt handling logic in the ARM processor. */
	Xil_ExceptionRegisterHandler( XIL_EXCEPTION_ID_IRQ_INT,
				      ( Xil_ExceptionHandler ) XScuGic_InterruptHandler,
				      &xInterruptController);
	/* Enable interrupts in the ARM. */
	Xil_ExceptionEnable();
#else
	int xStatus;
	xStatus = XConfigInterruptCntrl(IntrControllerAddr);
#endif
	if ( xStatus == XST_SUCCESS ) {
		xStatus = pdPASS;
#if defined(XPAR_XILTIMER_ENABLED) || defined(SDT)
		XRegisterInterruptHandler(NULL, IntrControllerAddr);
		Xil_ExceptionInit();
		Xil_ExceptionEnable();
#endif
	}
	else {
		xStatus = pdFAIL;
	}
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
	configASSERT( xStatus == pdPASS );
#endif

	return xStatus;
}


#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
void vPortEnableInterrupt( uint8_t ucInterruptID )
#else
void vPortEnableInterrupt( uint16_t ucInterruptID )
#endif
{
	int32_t lReturn;

	/* An API function is provided to enable an interrupt in the interrupt
	controller. */
	lReturn = prvEnsureInterruptControllerIsInitialised();
	if ( lReturn == pdPASS ) {
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
		XScuGic_Enable( &xInterruptController, ucInterruptID );
#else
		XEnableIntrId(ucInterruptID, IntrControllerAddr);
#endif
	}
	configASSERT( lReturn );
}
