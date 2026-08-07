# Copyright (C) 1991-2013 Altera Corporation
# Your use of Altera Corporation's design tools, logic functions 
# and other software and tools, and its AMPP partner logic 
# functions, and any output files from any of the foregoing 
# (including device programming or simulation files), and any 
# associated documentation or information are expressly subject 
# to the terms and conditions of the Altera Program License 
# Subscription Agreement, Altera MegaCore Function License 
# Agreement, or other applicable license agreement, including, 
# without limitation, that your use is for the sole purpose of 
# programming logic devices manufactured by Altera and sold by 
# Altera or its authorized distributors.  Please refer to the 
# applicable agreement for further details.

# Quartus II 64-Bit Version 13.0.1 Build 232 06/12/2013 Service Pack 1 SJ Web Edition
# File: E:\repos\FPGA_CPLD\CPLD_pwm\pin\CPLD_pwm.tcl
# Generated on: Fri Aug 07 11:09:11 2026

package require ::quartus::project

set_location_assignment PIN_B5 -to gates_out[0]
set_location_assignment PIN_B6 -to gates_out[1]
set_location_assignment PIN_B7 -to gates_out[2]
set_location_assignment PIN_A4 -to gates_out[3]
set_location_assignment PIN_A7 -to pll_locked
set_location_assignment PIN_C6 -to cap_valid
set_location_assignment PIN_A3 -to gates_in[0]
set_location_assignment PIN_A5 -to gates_in[1]
set_location_assignment PIN_A6 -to gates_in[2]
set_location_assignment PIN_B3 -to gates_in[3]
set_location_assignment PIN_B4 -to pwm_cap_in
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to gates_out[*]
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to gates_in[*]
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to pll_locked
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to cap_valid
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to pwm_cap_in
set_location_assignment PIN_M2 -to clk_50m
set_location_assignment PIN_M1 -to sys_rst_n
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to clk_50m
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to sys_rst_n
