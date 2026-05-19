/*
 * gpio_config.h — SCI-A GPIO pin configuration
 *
 * GPIO35 → SCITXDA (output)
 * GPIO36 → SCIRXDA (input, async mode, pull-up enabled)
 */

#ifndef APP_GPIO_GPIO_CONFIG_H_
#define APP_GPIO_GPIO_CONFIG_H_

void Init_Scia_Gpio(void);

#endif /* APP_GPIO_GPIO_CONFIG_H_ */
