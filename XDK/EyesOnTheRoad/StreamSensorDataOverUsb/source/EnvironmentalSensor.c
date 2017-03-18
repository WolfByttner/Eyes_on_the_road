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
#include <math.h>
#include "BCDS_Basics.h"

/* own header files */
#include "EnvironmentalSensor.h"

/* additional interface header files */
#include "FreeRTOS.h"
#include "timers.h"
#include "BCDS_Environmental.h"
#include "XdkSensorHandle.h"
#include "BCDS_Assert.h"

/* constant definitions ***************************************************** */
#define DELAY  							UINT32_C(1000)      /**< Timer delay is represented by this macro */
#define THREESECONDDELAY  UINT32_C(3000)        /**< three seconds delay is represented by this macro */
#define TIMERBLOCKTIME  UINT32_C(0xffff)    /** Macro used to define blocktime of a timer*/
#define VALUE_ZERO      UINT8_C(0)          /** default value*/
#define TIMER_NOT_ENOUGH_MEMORY            (-1L)/**<Macro to define not enough memory error in timer*/
#define TIMER_AUTORELOAD_ON             UINT32_C(1)             /**< Auto reload of timer is enabled*/
#define SEA_LEVEL			101325.0 //Pa
#define EATH_SCALE			7640.0   //meters

/* local variables ********************************************************** */
static xTimerHandle printTimerHandle;/**< variable to store timer handle*/
/* global variables ********************************************************* */

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
/** The function to get and print the Environmental data using printf
 * @brief Gets the raw data from BME280 Environmental and prints through the USB printf on serial port
 *
 * @param[in] pvParameters Rtos task should be defined with the type void *(as argument)
 */
static float convertPaHeight(uint32_t p)
{
	float pressureRatio = p/SEA_LEVEL;
	//printf("pressureRatio %f\n", pressureRatio);
	float height = -7640*log(pressureRatio);
	//printf("height %f\n", height);
	return height;
}


static void printEnvData(void *pvParameters)
{
    Retcode_T returnValue = (Retcode_T)RETCODE_FAILURE;
    Environmental_Data_T bme280 = { INT32_C(0), UINT32_C(0), UINT32_C(0) };
    Environmental_LsbData_T bme280lsb = { INT32_C(0), INT32_C(0), INT32_C(0) };
    BCDS_UNUSED(pvParameters);
    static float height;

    returnValue = Environmental_readDataLSB(xdkEnvironmental_BME280_Handle, &bme280lsb);
    if ( RETCODE_OK == returnValue)
    {
        /*print chip id and Environmental data of BME280 on serialport */
        //printf("BME280 Environmental Raw Data :\n\rp =%ld\n\rt =%ld\n\rh =%ld\n\r",
                //(long int) bme280lsb.pressure, (long int) bme280lsb.temperature, (long int) bme280lsb.humidity);
    }
    else
    {
        printf("Environmental Read Raw Data Failed\n\r");
    }

    /* Read temperature,pressure,humidity actual values */
    returnValue = Environmental_readData(xdkEnvironmental_BME280_Handle, &bme280);

    if ( RETCODE_OK == returnValue)
    {
    	//printf("raw %d\n", bme280.pressure);
    	height = convertPaHeight(bme280.pressure);
        /*print chip id and Environmental data of BME280 on serialport */
    	printf("ENV: %f,%d,%d\n",
    			height,(int)bme280.temperature,(int)bme280.humidity);

        //printf("BME280 Environmental Conversion Data :\n\rp =%ld Pa\n\rt =%ld mDeg\n\rh =%ld %%rh\n\r",
                //(long int) bme280.pressure, (long int) bme280.temperature, (long int) bme280.humidity);
    }
    else
    {
        printf("Environmental Read actual Data Failed\n\r");
    }

}


/* global functions ********************************************************* */

/**
 * @brief The function initializes BME(Environmental)creates and starts a auto reloaded
 * timer task which gets and prints the Environmental raw data and actual data
 */
void environmentalSensorInit(void)
{
    /* Return value for Timer start */
    int8_t retValPerSwTimer = TIMER_NOT_ENOUGH_MEMORY;
    Retcode_T returnValue = (Retcode_T)RETCODE_FAILURE;

    /*initialize Environmental sensor*/
    returnValue = Environmental_init(xdkEnvironmental_BME280_Handle);
    if ( RETCODE_OK == returnValue)
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
        /* create timer task to get and print Environmental data every one second automatically*/
        printTimerHandle = xTimerCreate((const char * const ) "printEnvData",
                Ticks, TIMER_AUTORELOAD_ON, NULL, printEnvData);

        /* timer create fail case */
        if (NULL == printTimerHandle)
        {
            /* Assertion Reason : "This software timer was not Created, Due to Insufficient heap memory" */
            assert(false);
        }
        /*start the created timer*/
        retValPerSwTimer = xTimerStart(printTimerHandle,
                TIMERBLOCKTIME);

        /* PED timer start fail case */
        if (TIMER_NOT_ENOUGH_MEMORY == retValPerSwTimer)
        {
            /* Assertion Reason :  "This software timer was not started, Due to Insufficient heap memory" */
            assert(false);
        }
        printf("Environmental Sensor initialization Success\n\r");
    }
    else
    {
        printf("Environmental Sensor initialization Failed\n\r");
    }

}

/**
 *  @brief API to Deinitialize the Environmental Sensor
 */
void environmentalSensorDeinit(void)
{
    Retcode_T returnValue = (Retcode_T) RETCODE_FAILURE;

    returnValue = Environmental_deInit(xdkEnvironmental_BME280_Handle);
    if (RETCODE_OK == returnValue)
    {
        printf("Environmental sensor Deinit Success\n\r");
    }
    else
    {
        printf("Environmental sensor Deinit Failed\n\r");
    }
}

/** ************************************************************************* */
