#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "gpio_config.h"
#include "sci_driver.h"
#include "app_sci.h"
#include "leds.h"



void main(void)
{
    InitSysCtrl();           // 系统初始化

    DINT;                    // 关中断
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();


    // GPIO 和 SCI 初始化
    Init_Scia_Gpio();
    Init_Scia();
    LED_Init();

    Scia_SendString("DSP28335 SCI Initialized!\r\n");
    // 在 main() 的 while(1) 之前添加，用于测试

    while(1)
    {
        App_Sci_Process();
    }
}


