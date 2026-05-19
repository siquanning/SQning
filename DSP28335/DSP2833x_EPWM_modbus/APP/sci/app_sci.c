/*
 * app_sci.c — Legacy echo-test handler
 *
 * NOTE: Not used in Modbus mode. SCI receive is handled by
 * sci_driver.c ISR → MB_FeedByte() → Modbus state machine.
 * Kept for reference / standalone SCI testing.
 */

#include "DSP2833x_Device.h"
#include "sci_driver.h"
#include "app_sci.h"

void App_Sci_Process(void)
{
    if (Scia_Data_Received_Flag == 1)
    {
        Scia_Data_Received_Flag = 0;
        Scia_SendChar(Scia_Received_Data);   // 回环测试
    }
}
