#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "gpio_config.h"
#include "sci_driver.h"
#include "app_sci.h"
#include "leds.h"
#include "epwm_config.h"

void main(void)
{
    InitSysCtrl();     // 系统初始化，设定150MHz
    DINT;
    InitPieCtrl();
    IER = 0; IFR = 0;
    InitPieVectTable();
    // 调用ePWM初始化
    Init_EPWM6_1kHz_50Percent();
    while(1); // PWM自动运行，无需干预
}
