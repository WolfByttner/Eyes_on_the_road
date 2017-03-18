/**
* Licensee agrees that the example code provided to Licensee has been developed and released by Bosch solely as an example to be used as a potential reference for Licensee’s application development. 
* Fitness and suitability of the example code for any use within Licensee’s applications need to be verified by Licensee on its own authority by taking appropriate state of the art actions and measures (e.g. by means of quality assurance measures).
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
#include "Magnetometer.h"

/* additional interface header files */
#include "FreeRTOS.h"
#include "timers.h"
#include "BCDS_Magnetometer.h"
#include "XdkSensorHandle.h"
#include "BCDS_Assert.h"

/* constant definitions ***************************************************** */
#define TIMERDELAY       UINT32_C(3000)          /**< three seconds delay is represented by this macro */
#define TIMERBLOCKTIME   UINT32_C(0xffff)        /** Macro used to define blocktime of a timer*/
#define ZEROVALUE        UINT32_C(0x00)          /** Macro used to define default value*/
#define TIMER_NOT_ENOUGH_MEMORY            (-1L)/**<Macro to define not enough memory error in timer*/
#define TIMER_AUTORELOAD_ON             UINT32_C(1)             /**< Auto reload of timer is enabled*/

/* local variables ********************************************************** */
static xTimerHandle printTimerHandle = ZEROVALUE; /**< variable to store timer handle*/

/* global variables ********************************************************* */

/* inline functions ********************************************************* */

/* local functions ********************************************************** */

/**@brief The function to gets the raw data BMM150 Magnetometer and prints through the USB printf on serial port
 *
 * @param[in] pvParameters Rtos task should be defined with the type void *(as argument)
 */

void printMagneticData(xTimerHandle pxTimer)
{
    /* Return value for Magnetometer Sensor */
    Retcode_T sensorApiRetValue = (Retcode_T)RETCODE_FAILURE;

    Magnetometer_XyzData_T getMagData =
            { INT32_C(0), INT32_C(0), INT32_C(0), INT32_C(0) };

    BCDS_UNUSED(pxTimer);

    sensorApiRetValue = Magnetometer_readXyzLsbData(xdkMagnetometer_BMM150_Handle,
            &getMagData);

    if ( RETCODE_OK == sensorApiRetValue)
    {
        /*prints the magnetometer lsb data from  BMM150 Magnetometer on serialport */
        printf("BMM150 Magnetometer lsb Data :\n\rx =%ld\n\ry =%ld\n\rz =%ld\n\r",
                (long int) getMagData.xAxisData,
                (long int) getMagData.yAxisData,
                (long int) getMagData.zAxisData);
    }
    else
    {
        printf("Magnetometer XYZ lsb Data read FAILED\n\r");
    }

    sensorApiRetValue = Magnetometer_readXyzTeslaData(
            xdkMagnetometer_BMM150_Handle, &getMagData);

    if ( RETCODE_OK == sensorApiRetValue)
    {
        /*prints the magnetometer converted data of BMM150 on serialport */
        printf(
                "BMM150 Magnetometer Converted data :\n\rx =%ld microTesla\n\ry =%ld microTesla\n\rz =%ld microTesla\n\r",
                (long int) getMagData.xAxisData,
                (long int) getMagData.yAxisData,
                (long int) getMagData.zAxisData);
    }
    else
    {
        printf("Magnetometer XYZ MicroTeslaData read FAILED\n\r");
    }
}

/* global functions ********************************************************* */

/**
 * @brief The function initializes BMM150(magnetometer)creates and starts a auto reloaded
 * timer task which gets and prints the magnetometer lsb and converted data.
 */
void magnetometerSensorInit(void)
{
    /* Return value for magnetometerInit and magnetometerSetMode api*/
    Retcode_T magReturnValue = (Retcode_T)RETCODE_FAILURE;

    /* Return value for Timer start */
    int8_t timerReturnValue = TIMER_NOT_ENOUGH_MEMORY;

    /* Initialization for Magnetometer Sensor */
    magReturnValue = Magnetometer_init(xdkMagnetometer_BMM150_Handle);

    if (RETCODE_OK == magReturnValue)
    {
        uint32_t Ticks = TIMERDELAY;

             if (Ticks != UINT32_MAX) /* Validated for portMAX_DELAY to assist the task to wait Infinitely (without timing out) */
             {
                 Ticks /= portTICK_RATE_MS;
             }
             if (UINT32_C(0) == Ticks) /* ticks cannot be 0 in FreeRTOS timer. So ticks is assigned to 1 */
             {
                 Ticks = UINT32_C(1);
             }
        /* create timer task to get and print Magnetometer data every three second automatically*/
        printTimerHandle = xTimerCreate(
                (const char * const) "printmagdata",
                Ticks, TIMER_AUTORELOAD_ON, NULL, printMagneticData);

        /* timer create fail case */
        if (NULL == printTimerHandle)
        {
           /* assertion Reason :  "This software timer was not created, Due to Insufficient heap memory" */
            assert(false);
        }

        /*start the created timer*/
        timerReturnValue = xTimerStart(printTimerHandle,
        TIMERBLOCKTIME);

        /* PMD timer start fail case */
        if (TIMER_NOT_ENOUGH_MEMORY == timerReturnValue)
        {
            /* Assertion Reason: "This software timer was not started,because the timer command queue is full." */
            assert(false);
        }
    }
    else
    {
        printf("Magnetometer initialization FAILED\n\r");
    }
}

/**
 *  @brief API to Deinitialize the MAG Sensor
 */
void magnetometerSensorDeinit(void)
{
    Retcode_T returnValue = (Retcode_T)RETCODE_FAILURE;
    returnValue = Magnetometer_deInit(xdkMagnetometer_BMM150_Handle);
    if (RETCODE_OK == returnValue)
    {
        printf("Magnetometer Deinit Success\n\r");
    }
    else
    {
        printf("Magnetometer Deinit Failed\n\r");
    }

}

/*************************************************************************** */
