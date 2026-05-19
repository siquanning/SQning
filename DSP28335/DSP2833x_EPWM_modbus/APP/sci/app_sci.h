/*
 * app_sci.h — Application-layer SCI processing
 *
 * NOTE: The SCI receive path now flows through sci_driver.c ISR → MB_FeedByte().
 * App_Sci_Process() is the legacy echo-test handler; it is retained for reference
 * but is NOT called in the current Modbus-based architecture.
 */

#ifndef APP_SCI_APP_SCI_H_
#define APP_SCI_APP_SCI_H_

void App_Sci_Process(void);   // legacy echo-test, not called in Modbus mode

#endif /* APP_SCI_APP_SCI_H_ */
