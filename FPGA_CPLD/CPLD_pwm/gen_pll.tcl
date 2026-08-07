# Generate ALTPLL for Cyclone IV E: 20 MHz -> 150 MHz
# Run: D:\altera\13.0sp1\quartus\bin64\quartus_sh -t gen_pll.tcl

load_package flow
project_open CPLD_pwm -revision CPLD_pwm

# Remove old hand-written PLL from project, will replace with generated one
set_global_assignment -remove -name VHDL_FILE pll_20m_to_150m.vhd

# The megawizard generation command for Quartus 13.0
# Use the IP catalog approach
catch { eval exec quartus_ipgenerate --output-directory=. --file-set=QUARTUS_SYNTH --project=CPLD_pwm }

project_close
