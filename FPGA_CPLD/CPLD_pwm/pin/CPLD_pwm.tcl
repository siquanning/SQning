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
# Generated on: Fri Aug 07 14:51:52 2026

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
set_location_assignment PIN_M1 -to rst_n
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to clk_50m
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to rst_n
set_location_assignment PIN_N3 -to gates_in[4]
set_location_assignment PIN_L3 -to gates_in[5]
set_location_assignment PIN_L4 -to gates_in[6]
set_location_assignment PIN_K8 -to gates_in[7]
set_location_assignment PIN_L7 -to gates_out[4]
set_location_assignment PIN_L6 -to gates_out[5]
set_location_assignment PIN_K5 -to gates_out[6]
set_location_assignment PIN_K6 -to gates_out[7]
set_location_assignment PIN_G1 -to gates_in[8]
set_location_assignment PIN_J6 -to gates_in[9]
set_location_assignment PIN_F1 -to gates_in[10]
set_location_assignment PIN_J1 -to gates_in[11]
set_location_assignment PIN_F3 -to gates_out[8]
set_location_assignment PIN_G5 -to gates_out[9]
set_location_assignment PIN_M6 -to gates_out[10]
set_location_assignment PIN_A2 -to gates_out[11]
