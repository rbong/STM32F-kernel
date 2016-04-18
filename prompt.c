#include <stdio.h>
#include <string.h>
#include "stm32f3xx_hal.h"
#include "stm32f3_discovery.h"
#include "stm32f3_discovery_accelerometer.h"
#include "stm32f3_discovery_gyroscope.h"

#include "common.h"

/*  CPU manual: */
/*  http://www.st.com/web/en/resource/technical/document/programming_manual/DM00046982.pdf */

extern void my_Tick( void );
int prompt_Setup(void);

extern volatile uint32_t myTickCount;
volatile uint32_t myTickCount;

/* Private variables ---------------------------------------------------------*/
const Led_TypeDef LEDs[] = {LED3, LED4, LED5, LED6, LED7, LED8, LED9, LED10};
const uint32_t numLEDs = sizeof(LEDs)/sizeof(LEDs[0]);

/* Private function prototypes -----------------------------------------------*/

void my_Tick(void){
	myTickCount++;
}

int prompt_Setup(void){
  uint32_t i;
  uint8_t accelRc, gyroRc;

  HAL_Init();

  TerminalInit();  /* Initialize UART and USB */
  /* Configure the LEDs... */
  for(i=0; i<numLEDs; i++) {
    BSP_LED_Init(LEDs[i]);
  }

  /* Initialize the pushbutton */
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);

  /* Initialize the Accelerometer */
  accelRc = BSP_ACCELERO_Init();
  if(accelRc != ACCELERO_OK) {
    printf("Failed to initialize acceleromter\n");
    Error_Handler();
  }

  /* Initialize the Gyroscope */
  gyroRc = BSP_GYRO_Init();
  if(gyroRc != GYRO_OK) {
    printf("Failed to initialize Gyroscope\n");
    Error_Handler();
  }

  return 0;
}



/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  BSP_LED_On(LED6);
  /* Infinite loop */
  while(1)
  {
  }
}

void CmdLED(int mode)
{
  uint32_t led,val;
  int rc;
  if(mode != CMD_INTERACTIVE) {
    return;
  }

  rc = fetch_uint32_arg(&led);
  if(rc) {
    printf("Missing LED index\n");
    return;
  }
    
  rc = fetch_uint32_arg(&val);
  if(rc) {
    printf("Missing state value, 0 for Off, 1 for On\n");
    return;
  }
  
  if((led < 3) || (led > 10)) {
    printf("Led index of %u is out of the range (3..10)\n",
	   (unsigned int)led);
    return;
  }

  led -= 3;
  if(val) {
    BSP_LED_On(LEDs[led]);
  } else {
    BSP_LED_Off(LEDs[led]);
  }

} 

ADD_CMD("led",CmdLED,"<index> <state> Turn off/on LED")

void CmdAccel(int mode)
{
  int16_t xyz[3];

  if(mode != CMD_INTERACTIVE) {
    return;
  }

  BSP_ACCELERO_GetXYZ(xyz);

  printf("Accelerometer returns:\n"
	 "   X: %d\n"
	 "   Y: %d\n"
	 "   Z: %d\n",
	 xyz[0],xyz[1],xyz[2]);


}

ADD_CMD("accel", CmdAccel,"                Read Accelerometer");

void CmdGyro(int mode)
{
  float xyz[3];

  if(mode != CMD_INTERACTIVE) {
    return;
  }

  BSP_GYRO_GetXYZ(xyz);

  printf("Gyroscope returns:\n"
	 "   X: %d\n"
	 "   Y: %d\n"
	 "   Z: %d\n",
	 (int)(xyz[0]*256),
	 (int)(xyz[1]*256),
	 (int)(xyz[2]*256));
}

ADD_CMD("gyro", CmdGyro,"                Read Gyroscope");

void CmdButton(int mode)
{
  uint32_t button;

  if(mode != CMD_INTERACTIVE) {
    return;
  }

  button = BSP_PB_GetState(BUTTON_USER);
  
  printf("Button is currently: %s\n",
	 button ? "Pressed" : "Released");

  return;
}

ADD_CMD("button", CmdButton,"                Print status of user button");
