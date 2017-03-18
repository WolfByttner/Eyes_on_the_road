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

/* own header files */
#include "Accelerometer.h"

/* additional interface header files */
#include "BCDS_Retcode.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "BCDS_Accelerometer.h"
#include "XdkSensorHandle.h"
#include "BCDS_Assert.h"

/* constant definitions ***************************************************** */
#define DELAY  						UINT32_C(50)      /**< Timer delay is represented by this macro */
#define THREESECONDDELAY            UINT32_C(3000) /**< three seconds delay is represented by this macro */
#define TIMERBLOCKTIME              UINT32_C(0xffff) /**< Macro used to define blocktime of a timer*/
#define VALUE_ZERO                  UINT8_C(0) /**< default value*/
#define FAILURE                     INT8_C(-1) /**< macro to failure state */
#define EVENNUMBER_IDENTIFIER       UINT8_C(2) /**< macro to identify even numbers */
#define ACCELEROMETER_SLOPE_THRESHOLD UINT8_C(100)  /**< Macro used to define slope threshold for accelerometer interrupt */
#define INTERRUPT_DISABLE			INT8_C(0)	/**< Macro used to Disable interrupt */
#define INTERRUPT_ENABLE			INT8_C(1)	/**< Macro used to enable interrupt */
#define TIMER_NOT_ENOUGH_MEMORY            (-1L)/**<Macro to define not enough memory error in timer*/
#define TIMER_AUTORELOAD_ON             UINT32_C(1)             /**< Auto reload of timer is enabled*/
#define GRAVITY 		9.80740
#define xBias 			218.701618
#define yBias			210.8776767
#define zBias			1.962095465

/* local variables ********************************************************** */
static xTimerHandle printTimerHandle = 0;/**< variable to store timer handle*/
static int8_t accelerometerIntConfigStatus = VALUE_ZERO; /**< variable to identify interrupt configuration is done or not*/
/**
 * @brief Application's deferred callback function that will be executed in deferred context.  This callback will be
 *        scheduled from application's real time callback after lowering the context.  It is deferred from application
 *        for demonstration.
 *
 * @param[in] void* pointer to a Queue which is valid, if the interrupt data is more than 4 byte.
 *            Presently this field is not used.
 *
 * @param[in] uint32_t holds interrupt data when ISR sends 4 bytes or less.
 *
 */
static void appDeferredISRCallback(void *interruptDataQueuePtr, uint32_t interruptData)
{
    BCDS_UNUSED(interruptDataQueuePtr);
    BCDS_UNUSED(interruptData);

    printf("appDeferredISRCallback received \r\n");
}

/**
 * @brief   Application real time callback function that serves accelerometer sensor interrupt in ISR context.
 *          This will be ISR function when user wanted to register their real time callback.  This function has to
 *          pend to deferred context by its own to do any bus access.  Whereas, if user has registered deferred
 *          callback handle, it is already deferred by the XDK and they can directly do bus access.
 *
 */
static void realTimeISRCallback(void)
{

    /* defer the context to acknowledge the interrupt */
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    if (xTimerPendFunctionCallFromISR(appDeferredISRCallback, NULL, UINT8_C(0), &xHigherPriorityTaskWoken) == pdPASS)
    {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        assert(false);
    }
}

/**
 * @brief   Application deferred callback function that serves accelerometer sensor interrupt in deferred context.
 *          This API is written to demonstrate XDK's deferred callback registration.  When we register this API as deferred
 *          callback, the context is already lowered by the XDK, bus access is directly possible from here. It is deferred
 *          from driver for demonstration.
 *
 * @param[in] void *  pointer to a Queue which is valid, if interrupt data is more than 4 byte.
 *
 * @param[in] uint32_t holds interrupt data when ISR sends 4 bytes or less.
 *
 */
static void driverDeferredISRCallback(void *interruptDataQueuePtr, uint32_t interruptData)
{
    BCDS_UNUSED(interruptDataQueuePtr);
    BCDS_UNUSED(interruptData);

    printf("driverDeferredISRCallback received \r\n");
}

/**
 * @brief Helper function to switch callback routines (real time and deferred) in the runtime.
 *        When user registers real time callback their callback will be execute in ISR context.
 *        User must not use any delay functions or bus access in ISR context.
 *        When user registers deferred callback their callback will be execute in timer deamon context,
 *        where there is no restriction of using delay functions and/or APIs related to bus access.
 *
 * @param[in] AccelerometerHandle_t sensor handle.
 *
 * @param[in] uint8_t count used to decide when to switch the callback.
 *
 */
static Retcode_T switchCallback(Accelerometer_HandlePtr_T sensorHandle, uint8_t count)
{
    Retcode_T returnValue = (Retcode_T) RETCODE_FAILURE;
    Accelerometer_InterruptChannel_T interruptChannel = ACCELEROMETER_BMA280_INTERRUPT_CHANNEL1;

    if (NULL == sensorHandle)
    {
        return ((Retcode_T) RETCODE_INVALID_PARAM);
    }

    if (!(count % EVENNUMBER_IDENTIFIER))
    {
        returnValue = Accelerometer_regDeferredCallback(sensorHandle, interruptChannel, driverDeferredISRCallback);
        if ( RETCODE_OK != returnValue)
        {
            accelerometerIntConfigStatus = FAILURE;
            printf("accelerometerRegisterDeferredCallback Failed...\n\r");
        }
    }
    else
    {
        returnValue = Accelerometer_regRealTimeCallback(sensorHandle, interruptChannel, realTimeISRCallback);
        if ( RETCODE_OK != returnValue)
        {
            accelerometerIntConfigStatus = FAILURE;
            printf("accelerometerRegisterRealTimeCallback Failed...\n\r");
        }
    }

    return (returnValue);
}

/** The function to get and print the accel data using printf
 * @brief Gets the raw data from BMA280 Accel and prints through the USB printf on serial port
 *
 * @param[in] pvParameters Rtos task should be defined with the type void *(as argument)
 */
static void printAccelData(void *pvParameters)
{
    BCDS_UNUSED(pvParameters);
    static uint8_t switchCallbackCount = VALUE_ZERO;
    static float accX, accY, accZ;

    Accelerometer_XyzData_T getaccelData = { INT32_C(0), INT32_C(0), INT32_C(0) };
/*
    if ( RETCODE_OK == Accelerometer_readXyzLsbValue(xdkAccelerometers_BMA280_Handle, &getaccelData))
    {
        printf("BMA280 Accel Raw Data :\n\rx =%ld\n\ry =%ld\n\rz =%ld\n\r",
                (long int) getaccelData.xAxisData, (long int) getaccelData.yAxisData, (long int) getaccelData.zAxisData);
    }
    else
    {
        printf("BMA280 XYZ Data read FAILED\n\r");
    }
*/

    if ( RETCODE_OK == Accelerometer_readXyzGValue(xdkAccelerometers_BMA280_Handle, &getaccelData))
    {

    	accX = GRAVITY*getaccelData.xAxisData - xBias;
    	accY = GRAVITY*getaccelData.yAxisData - yBias;
    	accZ = GRAVITY*getaccelData.zAxisData - zBias;

        printf("ACC: %f,%f,%f\n",accX, accY, accZ);

    	//printf("%f,%f,%f\n",accX, accY, accZ);
    }
    else
    {
        printf("Accelerometer Gravity XYZ Data read FAILED\n\r");
    }

    if (accelerometerIntConfigStatus != FAILURE)
    {
        /* demostrate switching callback functions (real time and deferred) at runtime */
        switchCallbackCount++;
        if ( RETCODE_OK != switchCallback(xdkAccelerometers_BMA280_Handle, switchCallbackCount))
        {
            printf("switchCallback Failed\n\r");
        }
    }
    else
    {
        printf("PAD interrupt is not configured\n\r");
    }
}

/* global functions ********************************************************* */

/**
 * @brief The function initializes BMA(accelerometer)creates and starts a auto reloaded
 * timer task which gets and prints the accel raw data
 */
void accelerometerSensorInit(void)
{
    /* Return value for Timer start */
    int8_t retValPerSwTimer = TIMER_NOT_ENOUGH_MEMORY;
    Accelerometer_InterruptChannel_T interruptChannel = ACCELEROMETER_BMA280_INTERRUPT_CHANNEL1;
    Accelerometer_InterruptType_T interruptType = ACCELEROMETER_BMA280_SLOPE_INTERRUPT;
    Accelerometer_ConfigSlopeIntr_T slopeInterruptConfig;

    /*initialize accel*/
    if (RETCODE_OK == Accelerometer_init(xdkAccelerometers_BMA280_Handle))
    {
        if (RETCODE_OK == Accelerometer_regRealTimeCallback(xdkAccelerometers_BMA280_Handle, interruptChannel, realTimeISRCallback))
        {
            /* Configure interrupt conditions */
            slopeInterruptConfig.slopeDuration = ACCELEROMETER_BMA280_SLOPE_DURATION4;
            slopeInterruptConfig.slopeThreshold = ACCELEROMETER_SLOPE_THRESHOLD;
            slopeInterruptConfig.slopeEnableX = INTERRUPT_DISABLE;
            slopeInterruptConfig.slopeEnableY = INTERRUPT_ENABLE;
            slopeInterruptConfig.slopeEnableZ = INTERRUPT_DISABLE;

            if ( RETCODE_OK != Accelerometer_configInterrupt(xdkAccelerometers_BMA280_Handle, interruptChannel, interruptType, &slopeInterruptConfig))
            {
                accelerometerIntConfigStatus = FAILURE;
                printf("accelerometerConfigureSlopeInterrupt Failed...\n\r");
            }
            else
            {
                printf("accelerometerConfigureSlopeInterrupt Success...\n\r");
            }
        }
        else
        {
            accelerometerIntConfigStatus = FAILURE;
            printf("accelerometerRegisterRealTimeCallback Failed...\n\r");
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
        /* create timer task to get and print accel data every one second automatically*/
        printTimerHandle = xTimerCreate((const char * const ) "printAccelData",
                Ticks, TIMER_AUTORELOAD_ON, NULL, printAccelData);

        /* timer create fail case */
        if (NULL == printTimerHandle)
        {
            /* Assertion Reason: "This software timer was not Created, Due to Insufficient heap memory" */
            assert(false);
        }

        /*start the created timer*/
        retValPerSwTimer = xTimerStart(printTimerHandle,
                TIMERBLOCKTIME);

        /* PAD timer start fail case */
        if (TIMER_NOT_ENOUGH_MEMORY == retValPerSwTimer)
        {
            /* Assertion Reason: This software timer was not started, Due to Insufficient heap memory */
            assert(false);
        }
    }
    else
    {
        printf("Accelerometer initialization FAILED\n\r");
    }

}

/* API Documentation in interface header file */
void accelerometerSensorDeinit(void)
{
    Retcode_T returnValue = (Retcode_T)RETCODE_FAILURE;
    returnValue = Accelerometer_deInit(xdkAccelerometers_BMA280_Handle);
    if (RETCODE_OK == returnValue)
    {
        printf("Accelerometer Deinit Success\n\r");
    }
    else
    {
        printf("Accelerometer Deinit Failed\n\r");
    }
}

/** ************************************************************************* */
