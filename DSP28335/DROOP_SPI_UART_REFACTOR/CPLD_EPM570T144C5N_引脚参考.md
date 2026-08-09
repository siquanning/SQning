# EPM570T144C5N (U69) 引脚参考

## 基本信息
- **位号**: U69
- **型号**: EPM570T144C5N
- **封装**: TQFP-144
- **原理图页**: CPLD_EPM570T144C5N (sheet 15/17)

---

## 电源引脚

| Pin | 名称 | 电压域 |
|-----|------|--------|
| 9 | VCCIO1(2) | IO Bank 1 (2.5V/3.3V) |
| 25 | VCCIO1(2) | IO Bank 1 (2.5V/3.3V) |
| 46 | VCCIO1(2) | IO Bank 1 (2.5V/3.3V) |
| 64 | VCCIO1(2) | IO Bank 1 (2.5V/3.3V) |
| 82 | VCCIO2(2) | IO Bank 2 (2.5V/3.3V) |
| 100 | VCCIO2(2) | IO Bank 2 (2.5V/3.3V) |
| 116 | VCCIO2(2) | IO Bank 2 (2.5V/3.3V) |
| 136 | VCCIO2(2) | IO Bank 2 (2.5V/3.3V) |
| 19 | VCCINT(1) | 内核供电 (1.8V?) |
| 56 | VCCINT(1) | 内核供电 |
| 90 | VCCINT(1) | 内核供电 |
| 126 | VCCINT(1) | 内核供电 |

---

## 地引脚

| Pin | 名称 | 域 |
|-----|------|-----|
| 10 | GNDIO | IO Bank 1 GND |
| 26 | GNDIO | IO Bank 1 GND |
| 47 | GNDIO | IO Bank 1 GND |
| 65 | GNDIO | IO Bank 1 GND |
| 83 | GNDIO | IO Bank 2 GND |
| 99 | GNDIO | IO Bank 2 GND |
| 115 | GNDIO | IO Bank 2 GND |
| 135 | GNDIO | IO Bank 2 GND |
| 17 | GNDINT | 内核 GND |
| 54 | GNDINT | 内核 GND |
| 92 | GNDINT | 内核 GND |
| 128 | GNDINT | 内核 GND |

---

## JTAG 引脚

| Pin | 名称 | 功能 |
|-----|------|------|
| 33 | TMS | Test Mode Select |
| 34 | TDI | Test Data In |
| 35 | TCK | Test Clock |
| 36 | TDO | Test Data Out |

---

## 全局时钟 (GCLK)

| Pin | 名称 | 状态 | 备注 |
|-----|------|------|------|
| 18 | GCLK0 | 已连接 | 全局时钟输入0 |
| 20 | GCLK1 | **NC (未连接)** | 全局时钟输入1 |
| 89 | GCLK2 | **NC (未连接)** | 全局时钟输入2 |
| 91 | GCLK3 | **NC (未连接)** | 全局时钟输入3 |

---

## 专用控制引脚

| Pin | 名称 | 功能 |
|-----|------|------|
| 60 | DEV_OE | 全局输出使能 |
| 61 | DEV_CLRN | 全局清零/复位 |

---

## GPIO Bank A (左侧, Pins 1-8)

| Pin | 名称 |
|-----|------|
| 1 | GPIOA1 |
| 2 | GPIOA2 |
| 3 | GPIOA3 |
| 4 | GPIOA4 |
| 5 | GPIOA5 |
| 6 | GPIOA6 |
| 7 | GPIOA7 |
| 8 | GPIOA8 |

## GPIO Bank B (左侧, Pins 11-16, 21-22)

| Pin | 名称 |
|-----|------|
| 11 | GPIOB1 |
| 12 | GPIOB2 |
| 13 | GPIOB3 |
| 14 | GPIOB4 |
| 15 | GPIOB5 |
| 16 | GPIOB6 |
| 21 | GPIOB7 |
| 22 | GPIOB8 |

## GPIO Bank C (左侧, Pins 23-24, 27-32)

| Pin | 名称 |
|-----|------|
| 23 | GPIOC1 |
| 24 | GPIOC2 |
| 27 | GPIOC3 |
| 28 | GPIOC4 |
| 29 | GPIOC5 |
| 30 | GPIOC6 |
| 31 | GPIOC7 |
| 32 | GPIOC8 |

## GPIO Bank D (左侧, Pins 37-44)

| Pin | 名称 |
|-----|------|
| 37 | GPIOD1 |
| 38 | GPIOD2 |
| 39 | GPIOD3 |
| 40 | GPIOD4 |
| 41 | GPIOD5 |
| 42 | GPIOD6 |
| 43 | GPIOD7 |
| 44 | GPIOD8 |

## GPIO Bank E (左侧, Pins 45, 48-53, 55)

| Pin | 名称 |
|-----|------|
| 45 | GPIOE1 |
| 48 | GPIOE2 |
| 49 | GPIOE3 |
| 50 | GPIOE4 |
| 51 | GPIOE5 |
| 52 | GPIOE6 |
| 53 | GPIOE7 |
| 55 | GPIOE8 |

## GPIO Bank F (左侧, Pins 57-59, 62-63, 66-68)

| Pin | 名称 |
|-----|------|
| 57 | GPIOF1 |
| 58 | GPIOF2 |
| 59 | GPIOF3 |
| 62 | GPIOF4 |
| 63 | GPIOF5 |
| 66 | GPIOF6 |
| 67 | GPIOF7 |
| 68 | GPIOF8 |

## GPIO Bank G (左侧, Pins 69-75, right side pins)

| Pin | 名称 |
|-----|------|
| 69 | GPIOG1 |
| 70 | GPIOG2 |
| 71 | GPIOG3 |
| 72 | GPIOG4 |
| 73 | GPIOG5 |
| 74 | GPIOG6 |
| 75 | GPIOG7 |

(Note: GPIOG8 not present — pin 76 is GPIOH1)

## GPIO Bank H (右侧, Pins 76-81, 84-85)

| Pin | 名称 |
|-----|------|
| 76 | GPIOH1 |
| 77 | GPIOH2 |
| 78 | GPIOH3 |
| 79 | GPIOH4 |
| 80 | GPIOH5 |
| 81 | GPIOH6 |
| 84 | GPIOH7 |
| 85 | GPIOH8 |

## GPIO Bank I (右侧, Pins 86-88, 93-97)

| Pin | 名称 |
|-----|------|
| 86 | GPIOI1 |
| 87 | GPIOI2 |
| 88 | GPIOI3 |
| 93 | GPIOI4 |
| 94 | GPIOI5 |
| 95 | GPIOI6 |
| 96 | GPIOI7 |
| 97 | GPIOI8 |

## GPIO Bank J (右侧, Pins 98, 101-107)

| Pin | 名称 |
|-----|------|
| 98 | GPIOJ1 |
| 101 | GPIOJ2 |
| 102 | GPIOJ3 |
| 103 | GPIOJ4 |
| 104 | GPIOJ5 |
| 105 | GPIOJ6 |
| 106 | GPIOJ7 |
| 107 | GPIOJ8 |

## GPIO Bank K (右侧, Pins 108-114, 117)

| Pin | 名称 |
|-----|------|
| 108 | GPIOK1 |
| 109 | GPIOK2 |
| 110 | GPIOK3 |
| 111 | GPIOK4 |
| 112 | GPIOK5 |
| 113 | GPIOK6 |
| 114 | GPIOK7 |
| 117 | GPIOK8 |

## GPIO Bank L (右侧, Pins 118-125)

| Pin | 名称 |
|-----|------|
| 118 | GPIOL1 |
| 119 | GPIOL2 |
| 120 | GPIOL3 |
| 121 | GPIOL4 |
| 122 | GPIOL5 |
| 123 | GPIOL6 |
| 124 | GPIOL7 |
| 125 | GPIOL8 |

## GPIO Bank M (右侧, Pins 127, 129-134, 137)

| Pin | 名称 |
|-----|------|
| 127 | GPIOM1 |
| 129 | GPIOM2 |
| 130 | GPIOM3 |
| 131 | GPIOM4 |
| 132 | GPIOM5 |
| 133 | GPIOM6 |
| 134 | GPIOM7 |
| 137 | GPIOM8 |

## GPIO Bank N (右侧, Pins 138-144)

| Pin | 名称 | 状态 |
|-----|------|------|
| 138 | GPION1 | |
| 139 | GPION2 | **NC (未连接)** |
| 140 | GPION3 | |
| 141 | GPION4 | |
| 142 | GPION5 | |
| 143 | GPION6 | |
| 144 | GPION7 | |

(Note: GPION8 not present)

---

## 未连接 (NC) 引脚汇总

| Pin | 名称 | 类型 |
|-----|------|------|
| 20 | GCLK1 | 全局时钟 |
| 89 | GCLK2 | 全局时钟 |
| 91 | GCLK3 | 全局时钟 |
| 139 | GPION2 | GPIO |

---

## GPIO 引脚总数统计

| Bank | 引脚数 | 说明 |
|------|--------|------|
| A | 8 | GPIOA1-A8 |
| B | 8 | GPIOB1-B8 |
| C | 8 | GPIOC1-C8 |
| D | 8 | GPIOD1-D8 |
| E | 8 | GPIOE1-E8 |
| F | 8 | GPIOF1-F8 |
| G | 7 | GPIOG1-G7 (无 G8) |
| H | 8 | GPIOH1-H8 |
| I | 8 | GPIOI1-I8 |
| J | 8 | GPIOJ1-J8 |
| K | 8 | GPIOK1-K8 |
| L | 8 | GPIOL1-L8 |
| M | 8 | GPIOM1-M8 |
| N | 7 | GPION1-N7 (无 N8) |
| **总计** | **108** | GPIO |

+ 4 GCLK + 2 专用控制 + 4 JTAG = **118 I/O**

---

> **注意**: 以上数据从嘉立创EDA原理图自动提取。实际编程时请以 Intel/Altera MAX II 官方数据手册为准。
> 引脚在原理图上的连接网络名称待进一步提取。
