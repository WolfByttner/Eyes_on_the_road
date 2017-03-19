/**
* Licensee agrees that the example code provided to Licensee has been developed and released by Bosch solely as an example to be used as a potential reference for Licensee�s application development. 
* Fitness and suitability of the example code for any use within Licensee�s applications need to be verified by Licensee on its own authority by taking appropriate state of the art actions and measures (e.g. by means of quality assurance measures).
* Licensee shall be responsible for conducting the development of its applications as well as integration of parts of the example code into such applications, taking into account the state of the art of technology and any statutory regulations and provisions applicable for such applications. Compliance with the functional system requirements and testing there of (including validation of information/data security aspects and functional safety) and release shall be solely incumbent upon Licensee. 
* For the avoidance of doubt, Licensee shall be responsible and fully liable for the applications and any distribution of such applications into the market.
* 
* 
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions are 
* met:
* 
*     (1) Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer. 
* 
*     (2) Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.  
*     
*     (3)The name of the author may not be used to
*     endorse or promote products derived from this software without
*     specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
*  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
*  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
*  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
*  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
*  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
*  POSSIBILITY OF SUCH DAMAGE.
*/
//lint -esym(956,*) /* Suppressing "Non const, non volatile static or external variable" lint warning*/

/* module includes ********************************************************** */

/* system header files */
#include <stdio.h>
#include "BCDS_Basics.h"

/* additional interface header files */
#include "BCDS_Assert.h"
#include "PTD_portDriver_ph.h"
#include "PTD_portDriver_ih.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "task.h"
#include "XdkSensorHandle.h"
#include "BCDS_Accelerometer.h"
#include "BLE_stateHandler_ih.h"
#include "BLE_serialDriver_ih.h"
#include "BleAlpwDataExchange_Server.h"
#include "XdkUsbResetUtility.h"
#include "USB_ih.h"
#include "em_cmu.h"


/* own header files */
#include "SendBLE.h"

/* Place the following code
into local variables */
#define USB_ENABLE_FLAG                UINT8_C(1)
#define USB_DISABLE_FLAG               UINT8_C(0)
#define USB_RECEIVE_BUFFER_LENGTH      UINT8_C(5)
static volatile uint8_t interruptHandler = USB_ENABLE_FLAG;
static char receivedData[USB_RECEIVE_BUFFER_LENGTH];

/* constant definitions ***************************************************** */

/* local variables ********************************************************** */
static xTimerHandle bleTransmitTimerHandle; /**< variable to store timer handle*/
static xTaskHandle bleTransmitTaskHandle; /**< task handle for bleAppHandler API */
static uint16_t bleConnectionHandle; /**< BLE connection handler for Send or Receive to host */
static uint8_t bleTransmitStatus = NUMBER_ZERO; /**< Validate the repeated start flag */
static volatile uint8_t isInterruptHandled = ENABLE_FLAG;
static uint8_t recievedData[MAX_DEVICE_LENGTH];
static volatile uint16_t sleep_count;
/* global variables ********************************************************* */

/* inline functions ********************************************************* */

/* local functions ********************************************************** */

/**
 * @brief		Bluetooth connect/ disconnect callback function
 *
 * @param[in] 	 connectionDetails  the connection status and the remote device address
 *
 */
static void notificationCallback(BLE_connectionDetails_t connectionDetails)
{

    switch (connectionDetails.connectionStatus)
    {
    case BLE_CONNECTED_TO_DEVICE:
        printf("Device connected  : \r\n");
        break;
    case BLE_DISCONNECTED_FROM_DEVICE:
        printf("Device Disconnected   : \r\n");
        break;
    default:
        /* Assertion Reason : "invalid status of Bluetooth Device" */
        assert(false);
        break;
    }
}

/** The function used to initialize the BLE Device and handle, various of event in the state machine
 *
 * @brief BLE device Initialization and handling the BLE events in state machine i.e Device connect/discover/Sleep etc
 *
 * @param[in] pvParameters RTOS task should be defined with the type void *(as argument)
 */
static void bleAppHandler(void *pParameters)
{
    BCDS_UNUSED(pParameters); /* to quiet warnings */

    /* return variable for stack receive status from base band */
    BLE_return_t bleTrsprtRetVal;

    /* return variable for BLE state handler */
    uint8_t bleStateHandleRetVal;

    for (;;)
    {

        /* Notify the BLE Stack that some data have been received from the Base band(Chip) or Host */
        bleTrsprtRetVal = BLE_hciReceiveData();

        /* This function is used to run the BLE stack and register a BLE device with the specified role */
        bleStateHandleRetVal = BLE_coreStateMachine();

        UNUSED_PARAMETER(bleTrsprtRetVal);
        UNUSED_PARAMETER(bleStateHandleRetVal);
    }
}

/**
 * @brief   This function callback used in ALPWISE Data Exchange Profile to transfer/receive BLE data
 *
 * @param[in] event: current device state or status
 *
 * @param[in] status: Event status i.e BLESTATUS_SUCCESS or BLESTATUS_FAILURE
 *
 * @param[in] parms : This void data pointer has more information of event i.e connection host data/event,
 *                    status,command etc
 */
static void bleAlpwDataExchangeService(BleAlpwDataExchangeEvent event, BleStatus status, void *parms)
{
    BleStatus bleSendReturn = BLESTATUS_FAILED;
    /* check the Host side receive event */
    if ((BLEALPWDATAEXCHANGE_EVENT_RXDATA == event)
            && (BLESTATUS_SUCCESS == status)
            && (parms))
    {
        BleAlpwDataExchangeServerRxData *rxData = (BleAlpwDataExchangeServerRxData *) parms;
        uint8_t bleReceiveBuffer[rxData->rxDataLen];

        /* instance for BLE connection handle */
        bleConnectionHandle = rxData->connHandle;

        /* copy received data to local buffer */
        /* Assertion Reason : The local receive buffer must be able to hold the received data.  */
        assert(rxData->rxDataLen <= (uint8_t )sizeof(bleReceiveBuffer));

        memcpy(bleReceiveBuffer, rxData->rxData, rxData->rxDataLen);

        /* make sure that the received string is null-terminated */
        bleReceiveBuffer[rxData->rxDataLen] = '\0';

        /* validate received data */
        if ((NUMBER_ZERO == strcmp((const char *) bleReceiveBuffer, "start"))
                && (NUMBER_ZERO == bleTransmitStatus))
        {

        	if(sleep_count)
        	{
        		BLEALPWDATAEXCHANGE_SERVER_SendData(bleConnectionHandle, "sleep",UINT8_C(5));
        		sleep_count = 0;
        	}
        	else
        	{
        		BLEALPWDATAEXCHANGE_SERVER_SendData(bleConnectionHandle, "awake",UINT8_C(5));
        	}
        }
    }
}

/**
 * @brief  The bleAppServiceRegister is used to register the BLE Alpwise DataExchange
 *         service's into attribute database.
 */
static void bleAppServiceRegister(void)
{
    /* flag for service registry return */
    BleStatus serviceRegistryStatus;

    /* Alpwise data Exchange Service Register*/
    serviceRegistryStatus = BLEALPWDATAEXCHANGE_SERVER_Register(bleAlpwDataExchangeService);

    /* Checking data NULL pointer condition */
    if (serviceRegistryStatus == BLESTATUS_FAILED)
    {
        /* Assertion Reason:   "BLE Service registry was failure" */
        assert(false);
    }

}

/* global functions ********************************************************* */

/** the function initializes the BMA and its process
 *
 * @brief The function initializes BMA(accelerometer)creates a auto reloaded
 * timer task which gets and transmit the accel raw data via BLE
 */

/* Place the following code above USB_CallbackIsr()
 */
static void usbInterruptHandling(void *callBackParam1, uint32_t count)
{
    /* do something with receivedData and count */
    reSendIncomingData((uint8_t*)receivedData, count);
    /* re-enable  the usb interrupt flag*/
    interruptHandler = USB_ENABLE_FLAG;
}
/* Place the following code above initReceiveDataISR()
 */
extern void callbackIsr(uint8_t *usbRcvBuffer,uint16_t count)
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if (USB_ENABLE_FLAG == interruptHandler)
	{
		interruptHandler = USB_DISABLE_FLAG;
		memcpy(receivedData, usbRcvBuffer, count);

		//printf("I heard: %s\n",receivedData);

		if(NUMBER_ZERO == strcmp(receivedData,"sleep"))
		{
			sleep_count++;
		}
	}
}


/* Place the following code above appInitSystem()
 */

static void  initReceiveDataISR()
{
	UsbResetUtility_RegAppISR((UsbResetUtility_UsbAppCallback_T)callbackIsr);
}


void bleInit(void)
{

    /* return value for BLE stack configuration */
	int8_t retValPerSwTimer = TIMER_NOT_ENOUGH_MEMORY;
    BleStatus appInitReturn = BLESTATUS_FAILED;
    BLE_notification_t configParams;
    BLE_returnStatus_t returnValue;
    Retcode_T UsbReturnValue = (Retcode_T)RETCODE_FAILURE;
    BLE_status BleReturnValue = BLESTATUS_FAILED;
    /* return value for BLE task create */
    int8_t appTaskInitReturn = TIMER_NOT_ENOUGH_MEMORY;
    Retcode_T advancedApiRetValue = (Retcode_T) RETCODE_FAILURE;

    sleep_count = 0;

    USB_lineCoding_t
     USBinit = {.Baudrate = 19200,.dataBits = 8,.charFormat = 0,
     .parityType = 0, .dummy = 0, .usbRxCallback = NULL};

    CMU_ClockEnable(cmuClock_USB,true);
    USB_init(&USBinit);
    UsbResetUtility_Init();
    initReceiveDataISR();

    UsbReturnValue = UsbResetUtility_RegAppISR((UsbResetUtility_UsbAppCallback_T) callbackIsr);
    if (UsbReturnValue != RETCODE_OK)
    {
        assert(false);
    }

    /* avoid synchronize problem to get proper BMA280 chipID this delay is mandatory */
    static_assert((portTICK_RATE_MS != 0), "Tick rate MS is zero");
    vTaskDelay((portTickType) 1 / portTICK_RATE_MS);

    returnValue = BLE_setDeviceName((uint8_t *) BLE_DEVICE_NAME, BLE_DEVICE_NAME_LENGTH);
    if (returnValue != BLE_SUCCESS)
    {
        /* assertion reason : invalid device name */
        assert(false);
    }

    /* enable and register notification callback for bluetooth device connect and disconnect*/
    configParams.callback = notificationCallback;
    configParams.enableNotification = BLE_ENABLE_NOTIFICATION;

    returnValue = BLE_enablenotificationForConnect(configParams);
    if (returnValue != BLE_SUCCESS)
    {
        /* assertion reason : Enable Notification Failed*/
        assert(false);
    }
    /* Registering the BLE Services  */
    BleReturnValue = BLE_customServiceRegistry(bleAppServiceRegister);
    if (BleReturnValue != BLESTATUS_SUCCESS)
    {
        /* assertion reason : custom Service Registry Failed*/
        assert(false);
    }
    /* Initialize the whole BLE stack */
    appInitReturn = BLE_coreStackInit();

    if (BLESTATUS_FAILED == (appInitReturn))
    {
        /* Assertion Reason : BLE Boot up process Failed, */
        assert(false);
    }
    else
    {

        /* create task for BLE state machine */
        appTaskInitReturn = xTaskCreate(bleAppHandler, (const char * const ) "BLE", STACK_SIZE_FOR_TASK, NULL, (uint32_t) BLE_TASK_PRIORITY, &bleTransmitTaskHandle);

        /* BLE task creatioon fail case */
        if (pdPASS != appTaskInitReturn)
        {
            /* Assertion Reason : BLE Task was not created, Due to Insufficient heap memory */
            assert(false);
        }
        uint32_t Ticks = DELAY;

        if (Ticks != UINT32_MAX) /* Validated for portMAX_DELAY to assist the task to wait Infinitely (without timing out) */
        {
            Ticks /= portTICK_RATE_MS;
        }
        if (UINT32_C(0) == Ticks) /* ticks cannot be 0 in FreeRTOS timer. So ticks is assigned to 1 */
        {
            Ticks = UINT32_C(1);
        }

    }
}

/** ************************************************************************* */
