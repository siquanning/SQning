# Board reference clock: 50 MHz.
create_clock -name clk_50m -period 20.000 [get_ports {clk_50m}]

# Generate the 150 MHz PLL output clock and standard uncertainty values.
derive_pll_clocks
derive_clock_uncertainty

# These ports are asynchronous to clk_50m/clk_150m and are explicitly
# synchronized by sync_2ff or pwm_capture before use.
set_false_path -from [get_ports {gates_in[*]}]
set_false_path -from [get_ports {pwm_cap_in}]
