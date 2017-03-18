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

/* own header files */
#include "LightSensor.h"

/* additional interface header files */
#include "BCDS_Basics.h"
#include "BCDS_Retcode.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "BCDS_LightSensor.h"
#include "XdkSensorHandle.h"
#include "BCDS_Assert.h"

/* local prototypes ********************************************************* */

/* constant definitions ***************************************************** */
#define DELAY  							UINT32_C(1000)      /**< Timer delay is represented by this macro */
#define THREESECONDDELAY                UINT32_C(3000)      /**< three seconds delay is represented by this macro */
#define TIMERBLOCKTIME                  UINT32_C(0xffff)    /**< Macro used to define blocktime of a timer */
#define ZERO                            UINT8_C(0)          /**< default value */
#define ONE                             UINT8_C(1)          /**< default value */
#define DEFERRED_CALLBACK               UINT8_C(1)          /**< indicate deferred callback */
#define REALTIME_CALLBACK               UINT8_C(0)          /**< indicate real time callback */
#define UPPER_THRESHOLD_VALUE           UINT32_C(0x5a)      /**< upper threshold value */
#define LOWER_THRESHOLD_VALUE           UINT32_C(0x2a)      /**< lower threshold value */
#define THRESHOLD_TIMER_VALUE           UINT32_C(0X05)      /**< threshold timer value */
#define NIBBLE_SIZE                     UINT8_C(4)          /**< size of nibble */
#define MASK_NIBBLE                     UINT8_C(0x0F)       /**< macro to mask nibble */
#define EVENNUMBER_IDENTIFIER           UINT8_C(2)          /**< macro to identify even numbers */
#define APP_CALLBACK_DATA               UINT32_C(100)       /**< macro to indicate application time callback data for demo*/
#define FAILURE                         INT8_C(-1)
#define TIMER_NOT_ENOUGH_MEMORY            (-1L)/**<Macro to define not enough memory error in timer*/
#define TIMER_AUTORELOAD_ON             UINT32_C(1)             /**< Auto reload of timer is enabled*/

/* local variables ********************************************************** */
/** variable to identify interrupt configuration is done or not*/
static int8_t lightsensorIntConfigStatus = ZERO;

/* global variables ********************************************************* */
/** variable to store timer handle*/
xTimerHandle printTimerHandle;

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
/**
 * @brief Application's deferred callback function that will be executed in deferred context.  This callback will be
 *        scheduled from application's real time callback after lowering the context.
 *
 * @param[in] void* pointer to a Queue which is valid, if the interrupt data is more than 4 byte.
 *            Presently this field is not used.
 *
 * @param[in] uint32_t holds interrupt data when ISR sends 4 bytes or less.
 *
 */
static void appDeferredISRCallback(void *interruptDataQueuePtr,
        uint32_t interruptData)
{
    BCDS_UNUSED(interruptDataQueuePtr);

    uint8_t status = ZERO;

    /* clear the interrupt */
    if (RETCODE_OK == LightSensor_getInterruptStatus(xdkLightSensor_MAX44009_Handle,
            (LightSensor_ConfigInterruptPtr_T) &status))
    {
        //printf("realTimeISRCallback received %u \r\n",
                //(unsigned int) interruptData);
    }
}
/**
 * @brief Application real time callback function that serves light sensor interrupt in ISR context.
 *
 */static void realTimeISRCallback(void)
{
    /* defer the context to acknowledge the interrupt */
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    if (xTimerPendFunctionCallFromISR(appDeferredISRCallback, NULL, APP_CALLBACK_DATA, &xHigherPriorityTaskWoken) == pdPASS)
    {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        assert(false);
    }
}

/**
 * @brief Application deferred callback function that serves light sensor interrupt in deferred context.
 *
 * @param[in] void *  pointer to a Queue which is valid, if interrupt data is more than 4 byte.
 *
 * @param[in] uint32_t holds interrupt data when ISR sends 4 bytes or less.
 *
 */
static void driverDeferredISRCallback(void *interruptDataQueuePtr,
        uint32_t interruptData)
{
    BCDS_UNUSED(interruptDataQueuePtr);
    BCDS_UNUSED(interruptData);

    printf("deferredISRCallback received %u \r\n",
            (unsigned int) interruptData);
}

/**
 * @brief Helper function to switch callback routines (real time and deferred) in the runtime.
 * 		  When user registers real time callback their callback will be execute in ISR context.
 * 		  User must not use any delay functions	or bus access in ISR context.
 *		  When user registers deferred callback their callback will be execute in timer deamon context,
 *		  where there is no restriction of using delay functions and/or APIs related to bus access.
 *
 * @param[in] LightSensor_HandlePtr_T sensor handle.
 *
 * @param[in] uint8_t count used to decide when to switch the callback.
 */
static Retcode_T switchCallback(LightSensor_HandlePtr_T sensorHandle,
        uint8_t count)
{
    Retcode_T returnValue = (Retcode_T) RETCODE_FAILURE;

    if (NULL != sensorHandle)
    {
        if (!(count % EVENNUMBER_IDENTIFIER))
        {
            returnValue = LightSensor_registerDeferredCallback(sensorHandle,
                    driverDeferredISRCallback);
            if (RETCODE_OK != returnValue)
            {
                printf("lightsensorRegisterDeferredCallback Failed...\n\r");
            }
        }
        else
        {
            returnValue = LightSensor_registerRealTimeCallback(sensorHandle,
                    realTimeISRCallback);
            if (RETCODE_OK != returnValue)
            {
                printf("lightsensorRegisterRealTimeCallback Failed...\n\r");
            }
        }
    }

    return (returnValue);
}

/**
 * @brief Read data from light sensor and print through the USB
 *
 * @param[in] pxTimer timer handle
 */
void printLightSensorData(xTimerHandle pxTimer)
{
    Retcode_T returnValue = (Retcode_T) RETCODE_FAILURE;
    static uint8_t switchCallbackCount = ZERO;
    uint16_t luxRawData = UINT16_C(0);
    uint32_t milliLuxData = UINT32_C(0);

    static uint32_t light;

    BCDS_UNUSED(pxTimer); /* suppressing warning message */

    /* read Raw sensor data */
    returnValue = LightSensor_readRawData(xdkLightSensor_MAX44009_Handle,
            &luxRawData);
    if (returnValue != RETCODE_OK)
    {
        /**
         * @todo using printf is not thread safe, we need to consider using iprintf.
         */
        printf("lightsensorReadRawData Failed\n\r");
    }
    else
    {
        printf("LIG: %x\n", luxRawData);
    }

    /* read sensor data in milli lux*/
    /*
    returnValue = LightSensor_readLuxData(xdkLightSensor_MAX44009_Handle,
            &milliLuxData);
    if (returnValue != RETCODE_OK)
    {
        printf("lightsensorReadInMilliLux Failed\n\r");
    }
    else
    {
        printf("Light sensor data obtained in milli lux :%d \n\r",
                (unsigned int) milliLuxData);
    }
    */
/*
    if (RETCODE_OK == lightsensorIntConfigStatus)
    {
        switchCallbackCount++;
        printf("\n\r [Light]SWITCH..\n\r");
        returnValue = switchCallback(xdkLightSensor_MAX44009_Handle,
                switchCallbackCount);
        if (returnValue != RETCODE_OK)
        {
            printf("switchCallback Failed\n\r");
        }
    }
    else
    {
        printf("Lightsensor interrupt not configured.\n\r");
    }
    */
}

/* global functions ********************************************************* */
/**
 * @brief The function initializes MAX44009 sensor and creates, starts timer task in autoreloaded mode
 * every three second which reads and prints the lightsensor data
 */
void lightsensorInit(void)
{
    Retcode_T returnValue = (Retcode_T) RETCODE_FAILURE;
    int8_t retValPerSwTimer = TIMER_NOT_ENOUGH_MEMORY;

    /*initialize lightsensor*/
    returnValue = LightSensor_init(xdkLightSensor_MAX44009_Handle);
    if (RETCODE_OK == returnValue)
    {
        uint32_t Ticks = DELAY;

        if (Ticks != UINT32_MAX) /* Validated for portMAX_DELAY to assist the task to wait Infinitely (without timing out) */
        {
            Ticks /= portTICK_RATE_MS;
        }
        if (UINT32_C(0) == Ticks) /* ticks cannot be 0 in FreeRTOS timer. So ticks is assigned to 1 */
        {
            Ticks = UINT32_C(1);
        }
        /* create timer task to read and print lightsensor data every three seconds*/
        printTimerHandle = xTimerCreate((const char * const ) "printSensorData",
                Ticks, TIMER_AUTORELOAD_ON, NULL, printLightSensorData);
        /* timer create fail case */
        if (NULL == printTimerHandle)
        {
            /* Assertion Reason: "This software timer was not Created, Due to Insufficient heap memory" */
            assert(false);
        }

        /* Register interrupt callback */
        returnValue = LightSensor_registerRealTimeCallback(
                xdkLightSensor_MAX44009_Handle, realTimeISRCallback);

        if (RETCODE_OK == returnValue)
        {
            /* Configure interrupt conditions */
            returnValue = LightSensor_configureThresholdInterrupt(
                    xdkLightSensor_MAX44009_Handle, UPPER_THRESHOLD_VALUE,
                    LOWER_THRESHOLD_VALUE, THRESHOLD_TIMER_VALUE);
            if (RETCODE_OK != returnValue)
            {
                lightsensorIntConfigStatus = FAILURE;
                printf("lightsensorRegisterRealTimeCallback Failed...\n\r");
            }
        }
        else
        {
            lightsensorIntConfigStatus = FAILURE;
            printf("configInterrupt Failed...\n\r");
        }
        /*start the created timer*/
        retValPerSwTimer = xTimerStart(printTimerHandle, TIMERBLOCKTIME);

        /* LSD timer start fail case */
        if (TIMER_NOT_ENOUGH_MEMORY == retValPerSwTimer)
        {
            /* Assertion Reason:  "This software timer was not started,because the timer command queue is full." */
            assert(false);
        }
    }
    else
    {
        printf("lightsensorInit Failed\n\r");
    }
}

/**
 *  @brief  The function to deinitialize LightSensor
 *
 */
void lightsensorDeinit(void)
{
    Retcode_T returnValue = (Retcode_T) RETCODE_FAILURE;

    returnValue = LightSensor_deInit(xdkLightSensor_MAX44009_Handle);
    if (RETCODE_OK == returnValue)
    {
        printf("lightsensorDeinit Success\n\r");
    }
    else
    {
        printf("lightsensorDeinit Failed\n\r");
    }
}

/** ************************************************************************* */
