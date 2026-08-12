// TI File $Revision: /main/9 $
// Checkin $Date: August 10, 2007   09:05:08 $
//###########################################################################
//
// FILE:	Example_EpwmSetup.c
//
// TITLE:	Frequency measurement using EQEP peripheral
//
// DESCRIPTION:
//
// This file contains source for the ePWM initialization for the
// freq calculation module
//
//###########################################################################
// Original Author: SD
//
// $TI Release: DSP2833x Header Files V1.01 $
// $Release Date: September 26, 2007 $
//###########################################################################

#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File


#if (CPU_FRQ_150MHZ)
  #define CPU_CLK   150e6
#endif
#if (CPU_FRQ_100MHZ)
  #define CPU_CLK   100e6
#endif
#define PWM_CLK   60e3                // #############If diff freq. desired, change freq here.
#define SP        CPU_CLK/(2*PWM_CLK)
#define TBCTLVAL  0x200E              // Up-down cnt, timebase = SYSCLKOUT


void EPwm1Setup()
{
	InitEPwm1Gpio();

		    EALLOW;
		    EPwm1Regs.TBPRD=SP;
		    EPwm1Regs.CMPA.half.CMPA=0;
		    EPwm1Regs.CMPB=0;

		    EPwm1Regs.TBPHS.half.TBPHS=0;                 //时基相位寄存器设置为0
		    EPwm1Regs.TBCTR=0;                            //时基计数器清零
		    EPwm1Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;  //增减模式
		    EPwm1Regs.TBCTL.bit.PRDLD=TB_SHADOW;
		    EPwm1Regs.TBCTL.bit.PHSEN=TB_DISABLE;         //禁止相位控制
		    EPwm1Regs.TBCTL.bit.SYNCOSEL=TB_CTR_ZERO;     //CTR=ZERO输出同步信号
		    EPwm1Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;         //TBCLK=SYSCLKOUT
		    EPwm1Regs.TBCTL.bit.CLKDIV=TB_DIV1;

		    EPwm1Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;      //CPU向影子寄存器写入值
		    EPwm1Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;
		    EPwm1Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;    //CTR=ZERO时装载
		    EPwm1Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;

		//动作
		    EPwm1Regs.AQCTLA.bit.CAU=AQ_SET;
		    EPwm1Regs.AQCTLA.bit.CAD=AQ_CLEAR;
		    // EPwm1Regs.AQCTLB.bit.ZRO=AQ_SET;
		    // EPwm1Regs.AQCTLB.bit.CBU=AQ_CLEAR;
			EPwm1Regs.AQSFRC.all=0;
			EPwm1Regs.AQCSFRC.all=0;

		//死区
			EPwm1Regs.DBCTL.bit.POLSEL=DB_ACTV_HIC;
			EPwm1Regs.DBCTL.bit.IN_MODE=DBA_ALL;
			EPwm1Regs.DBCTL.bit.OUT_MODE= DB_FULL_ENABLE;
			//EPwm1Regs.DBCTL.all=0xb;
			EPwm1Regs.DBRED=50;//上升延时
			EPwm1Regs.DBFED=50;//下降延时

		//错误联防
			EPwm1Regs.TZSEL.all=0x0300;
			EPwm1Regs.TZCTL.bit.TZA=TZ_FORCE_LO;
			EPwm1Regs.TZCTL.bit.TZB=TZ_FORCE_LO;
			EPwm1Regs.TZEINT.bit.CBC=0;
			EPwm1Regs.TZEINT.bit.OST=1;
			EPwm1Regs.TZFLG.all=0;
			EPwm1Regs.TZCLR.all=0;
			EPwm1Regs.TZFRC.all=0;

			EPwm1Regs.ETSEL.all=0;            // Interrupt when TBCTR = 0x0000
			EPwm1Regs.ETFLG.all=0;
			EPwm1Regs.ETCLR.all=0;
			EPwm1Regs.ETFRC.all=0;

			//EPwm1Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;//事件触发条件
			//EPwm1Regs.ETSEL.bit.INTEN = 1;//中断使能
			//EPwm1Regs.ETPS.bit.INTPRD = ET_1ST;

			EPwm1Regs.PCCTL.all=0;


			EDIS;
}
void EPwm2Setup()
{
	InitEPwm2Gpio();

		    EALLOW;
		    EPwm2Regs.TBPRD=SP;
		    EPwm2Regs.CMPA.half.CMPA=0;
		    EPwm2Regs.CMPB=0;

		    EPwm2Regs.TBPHS.half.TBPHS=0;                 //时基相位寄存器设置为0
		    EPwm2Regs.TBCTR=0;                            //时基计数器清零
		    EPwm2Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;  //增减模式
		    EPwm2Regs.TBCTL.bit.PRDLD=TB_SHADOW;
		    EPwm2Regs.TBCTL.bit.PHSEN=TB_DISABLE;         //禁止相位控制
		    EPwm2Regs.TBCTL.bit.SYNCOSEL=TB_CTR_ZERO;     //CTR=ZERO输出同步信号
		    EPwm2Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;         //TBCLK=SYSCLKOUT
		    EPwm2Regs.TBCTL.bit.CLKDIV=TB_DIV1;

		    EPwm2Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;      //CPU向影子寄存器写入值
		    EPwm2Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;
		    EPwm2Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;    //CTR=ZERO时装载
		    EPwm2Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;

		//动作
		    EPwm2Regs.AQCTLA.bit.CAU=AQ_SET;
		    EPwm2Regs.AQCTLA.bit.CAD=AQ_CLEAR;
		    // EPwm1Regs.AQCTLB.bit.ZRO=AQ_SET;
		    // EPwm1Regs.AQCTLB.bit.CBU=AQ_CLEAR;
			EPwm2Regs.AQSFRC.all=0;
			EPwm2Regs.AQCSFRC.all=0;

		//死区
			EPwm2Regs.DBCTL.bit.POLSEL=DB_ACTV_HIC;
			EPwm2Regs.DBCTL.bit.IN_MODE=DBA_ALL;
			EPwm2Regs.DBCTL.bit.OUT_MODE= DB_FULL_ENABLE;
			//EPwm1Regs.DBCTL.all=0xb;
			EPwm2Regs.DBRED=50;//上升延时
			EPwm2Regs.DBFED=50;//下降延时

		//错误联防
			EPwm2Regs.TZSEL.all=0x0300;
			EPwm2Regs.TZCTL.bit.TZA=TZ_FORCE_LO;
			EPwm2Regs.TZCTL.bit.TZB=TZ_FORCE_LO;
			EPwm2Regs.TZEINT.bit.CBC=0;
			EPwm2Regs.TZEINT.bit.OST=1;
			EPwm2Regs.TZFLG.all=0;
			EPwm2Regs.TZCLR.all=0;
			EPwm2Regs.TZFRC.all=0;

			EPwm1Regs.ETSEL.all=0;            // Interrupt when TBCTR = 0x0000
			EPwm1Regs.ETFLG.all=0;
			EPwm1Regs.ETCLR.all=0;
			EPwm1Regs.ETFRC.all=0;

			//EPwm2Regs.ETSEL.bit.INTSEL = ET_CTR_ZERO;//事件触发条件
			//EPwm2Regs.ETSEL.bit.INTEN = 1;//中断使能
			//EPwm2Regs.ETPS.bit.INTPRD = ET_1ST;

			EPwm2Regs.PCCTL.all=0;


			EDIS;
}
