# TI 库函数速查表 (DSP2833x 标准库)

> 本表是"跟着 AI 学 DSP"的基础课：常用 TI 封装函数，每个都说明"做什么 / 怎么用 / 底层原理一句话"。
> 用法：遇到不认识的函数先查这里；查不到就让 AI 补充（规则要求它把新函数记进本表）。

---

## 0. 封装函数 vs 裸寄存器（先建立直觉）

| 想做的事 | 封装函数（推荐） | 裸寄存器（尽量避免） |
|---|---|---|
| 配置 ePWM1 的引脚复用 | `InitEPwm1Gpio();` | `GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;` |
| 延时 10 微秒 | `DELAY_US(10);` | `for(;;);`（被禁止） |
| 配置 CPU Timer0 周期 | `ConfigCpuTimer(&CpuTimer0, 150, 16.67);` | `CpuTimer0Regs.PRD.all = ...;` |
| 系统时钟初始化 | `InitSysCtrl();` | `SysCtrlRegs.PLLCR = ...;` |
| 中断应答（无封装，必须寄存器） | 无 | `PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;` |

**规则**：有封装函数 → 必须用封装；没封装 → 寄存器操作必须配中文注释解释。

---

## 1. 系统初始化类

| 函数 | 做什么 | 底层原理（一句话） | 所在文件 |
|---|---|---|---|
| `InitSysCtrl()` | 配置 PLL 到 150MHz、关看门狗、开外设时钟 | 写 `SysCtrlRegs.PLLCR` 等寄存器 | DSP2833x_SysCtrl.c |
| `InitPieCtrl()` | 初始化中断控制器 PIE 的默认状态 | 写 `PieCtrlRegs` 寄存器 | DSP2833x_PieCtrl.c |
| `InitPieVectTable()` | 把默认中断服务函数填进向量表 | 写 `PieVectTable` 数组 | DSP2833x_PieVect.c |
| `InitCpuTimers()` | 初始化 3 个 CPU 定时器 | 写 `CpuTimer0~2Regs` | DSP2833x_CpuTimers.c |
| `ConfigCpuTimer(Timer, Freq, Period)` | 设置定时器中断周期（微秒） | 计算后写 PRD 寄存器 | DSP2833x_CpuTimers.c |
| `DELAY_US(n)` | 软件延时 n 微秒 | 汇编指令循环，不占定时器 | DSP2833x_usDelay.asm |
| `InitXintf() / InitXintf16Gpio()` | 初始化外部总线 XINTF 及其引脚 | 写 XINTF 时序寄存器 | DSP2833x_Xintf.c |

## 2. 外设引脚复用类（封装函数）

| 函数 | 作用 |
|---|---|
| `InitEPwm1Gpio()` ~ `InitEPwm6Gpio()` | 把 GPIO0~11 配置成 ePWM1~6 输出 |
| `InitSciaGpio()` / `InitScibGpio()` | 配置 SCI 串口引脚 |
| `InitSpiaGpio()` | 配置 SPI 引脚 |
| `InitECanaGpio()` | 配置 eCAN 引脚 |
| `InitAdc()` | ADC 模块初始化（时钟分频） |

> 这些函数内部其实也是一堆寄存器操作（TI 帮你写好了），你只需要调用，不用管细节。

## 3. 中断类

| 名称 | 作用 |
|---|---|
| `DINT` / `EINT` | 关 / 开全局中断（汇编指令） |
| `ERTM` | 开实时调试中断 |
| `EALLOW; ... EDIS;` | 解锁 / 上锁受保护的寄存器（改配置前必须） |
| `PieVectTable.xxx = &ISRxxx;` | 注册自己的中断服务函数（需 EALLOW/EDIS 包裹） |
| `PieCtrlRegs.PIEACK.all = PIEACK_GROUPx;` | 中断结束应答（**无封装，必须写**） |
| `IER` / `PIEIERx` | 使能中断组 / 通道 |

## 4. GPIO 数据操作（寄存器，驱动层用）

```c
GpioDataRegs.GPASET.bit.GPIO5 = 1;    // 置高（GPIO5 输出高电平）
GpioDataRegs.GPACLEAR.bit.GPIO5 = 1;  // 拉低
GpioDataRegs.GPADAT.bit.GPIO5         // 读取电平（1 或 0）
```

## 5. IQMath 定点数学（可选）

| 函数/宏 | 作用 |
|---|---|
| `_IQ(x)` | 把浮点数转成定点数（如 `_IQ(0.5)`） |
| `IQmpy(a, b)` | 定点乘法 |
| `IQdiv(a, b)` | 定点除法 |
| `_IQtoF(x)` | 定点数转回浮点数 |

## 6. 常见寄存器缩写（看不懂注释时对照）

| 缩写 | 含义 |
|---|---|
| PRD | Period，周期值 |
| TBCTL | Time-Base Control，时基控制 |
| CMPA / CMPB | 比较寄存器（决定占空比） |
| DB | Dead-Band，死区 |
| TZ | Trip-Zone，故障保护 |
| PIEACK | 中断应答 |
| IER / IFR | 中断使能 / 标志寄存器 |
| MUX | Multiplex，引脚复用选择 |
| DIR | Direction，方向（输入/输出） |

---

*维护规则：开发中遇到本表没有的新库函数，AI 必须把它补充进本表。*