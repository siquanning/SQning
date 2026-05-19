/*
 * app_sci.c
 *
 *  Created on: 2026年5月9日
 *      Author: 32485
 */

#include "DSP2833x_Device.h"
#include "sci_driver.h"
#include "app_sci.h"

void App_Sci_Process(void)
{
    // 检测是否有新数据到达
    if (Scia_Data_Received_Flag == 1)
    {
        Scia_Data_Received_Flag = 0;  // 清除标志
        // 简单示例：将接收到的数据原样发回（回环测试）
        Scia_SendChar(Scia_Received_Data);
        // 扩展：可在这里添加复杂的协议解析
        // 例如，根据接收到的指令执行相应操作
        // switch(Scia_Received_Data) { ... }
    }
}


