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

/* header definition ******************************************************** */
#ifndef XDK110_SENDACCELEROMETERDATAOVERBLE_H_
#define XDK110_SENDACCELEROMETERDATAOVERBLE_H_

/* local interface declaration ********************************************** */

/* local type and macro definitions */
#define DELAY 			      			UINT16_C(100)
#define ONESECONDDELAY 			      UINT16_C(1000)	               /**< one second is represented by this macro */
#define TIMERBLOCKTIME 			      UINT16_C(0xffff)                 /**< Macro used to define blocktime of a timer*/
#define USEI2CPROTOCOL 			      UINT8_C(0)                       /**< I2c protocol is represented by this macro*/
#define NUMBER_ZERO 			      UINT8_C(0)                       /**< Zero value for BLE variables */
#define NUMBER_ONE 			          UINT8_C(1)                       /**< One value for BLE variables */
#define I2C_ADDRESS 			      UINT8_C(0x18)                    /**< i2c address for BMA280 */
#define STACK_SIZE_FOR_TASK           (configMINIMAL_STACK_SIZE + 10)  /**< TASK stack size for BLE application handler */
#define BLE_TASK_PRIORITY  		      UINT8_C(1)                       /**< TASK Priority value for BLE */
#define BLETRANSMITLENGTH  		      UINT8_C(1)                      /**<Transmit length for BLE */
#define ACCEL_RECEIVELENGTH 	      UINT8_C(30)                      /**< Receive length for BLE */
#define ENABLE_FLAG                   UINT8_C(1)                       /**< macro to represent the "enable" */
#define DISABLE_FLAG                  UINT8_C(0)                       /**< macro to represent the "Disable" */
#define REMOTE_ADDRESS_LENGTH         UINT8_C(6)                       /**< The length of remote device address */
#define MAX_DEVICE_LENGTH             UINT8_C(20)                      /**< The Maximum length of bluetooth device name */
#define SET_IDLE_MODE                 UINT8_C('i')                     /**< command to set the device in idle mode */
#define SET_DISCOVERABLE_MODE         UINT8_C('d')                     /**< command to set the device in discoverable mode */
#define SET_SLEEP_MODE                UINT8_C('s')                     /**< command to set the device in sleep mode */
#define GET_DEVICE_MODE               UINT8_C('g')                     /**< command to get the device current state */
#define TIMER_NO_ERROR                (0L)							   /**<Macro to define no error in timer*/
#define TIMER_NOT_ENOUGH_MEMORY       (-1L)							   /**<Macro to define not enough memory error in timer*/
#define TIMER_AUTORELOAD_ON           UINT32_C(1)            		   /**< Auto reload of timer is enabled*/
#define TIMER_AUTORELOAD_OFF          UINT32_C(0)             		   /**< Auto reload of timer is disabled*/
#define BLE_DEVICE_NAME               "EyesOnTheRoad"              		   /**< Name of the BLE device*/
#define BLE_DEVICE_NAME_LENGTH         UINT8_C(sizeof(BLE_DEVICE_NAME))/**< length of the BLE device name*/

/* local function prototype declarations */

/** The function used to initialize the BLE Device and handle, various of event in the state machine
 *
 * @brief BLE device Initialization and handling the BLE events in state machine i.e Device connect/discover/Sleep etc
 *
 * @param[in] pvParameters Rtos task should be defined with the type void *(as argument)
 */
static void bleAppHandler(void *pParameters);

/**
 * @brief   The ALPWDATAEXCHANGE callback used in ALPWISE Data Exchange Profile
 *
 * @param[in] event: current device state or status
 *
 * @param[in] status: Event status i.e BLESTATUS_SUCCESS or BLESTATUS_FAILURE
 *
 * @param[in] parms : This void data pointer has more information of event i.e connection host data/event,
 *                    status,command etc
 */
static void bleAlpwDataExchangeService(BleAlpwDataExchangeEvent event, BleStatus status, void *parms);

/**
 * @brief  The BEA_bleServiceRegister is used to register the BLE Alpwise DataExchange
 *         service's into attribute database.
 */
static void bleAppServiceRegister(void);


static void checkForSleep(void *pvParameters);

/**
 * @brief API called by OS_timerPendFunctionCallFromISR function, which is registered during the USB ISR
 *
 * @param [in]callBackParam1 data buffer
 *
 * @param [in]callBackParam2 length
 */
static void interruptHandling(void *callBackParam1, uint32_t callBackParam2);

/* local module global variable declarations */

/* local inline function definitions */

#endif /* XDK110_SENDACCELEROMETERDATAOVERBLE_H_ */

/** ************************************************************************* */
