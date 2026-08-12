# CPLD_trs to CPLD_cyc migration

The VHDL implementation from `../CPLD_trs` is included unchanged.  The
`CPLD_cyc` entity is a thin top-level wrapper around `CPLD_pwm`, allowing the
Quartus project/revision name to remain `CPLD_cyc`.

## Target

- Family: Cyclone IV E
- Device: EP4CE10F17C8
- Input clock: 50 MHz
- PLL clock: 150 MHz

## Pin assignments

Known assignments were recovered from the byte-identical `../CPLD_pwm`
project, which targets the same device. The board top level exposes only these
33 verified pins: clock, reset, `gates_in[11:0]`, `gates_out[15:0]`, PWM
capture input/valid, and PLL lock status.

The currently unused core ports are not exposed as device pins:

- `gates_in[23:12]` is tied low.
- `mod_a`, `mod_b`, and `mod_c` are tied to Q15 value 8192.
- `gates_out[23:16]`, `cap_period`, and `cap_high_time` are left unconnected.
- All other unused package pins are configured as tri-stated inputs with weak
  pull-ups.

## Preserved source behavior

`CPLD_pwm_phase.vhd` currently sets `USE_FIXED_DUTY` to `true`, so Quartus
optimizes away the three modulation input buses.  This is inherited from the
source implementation and was intentionally not changed during migration.

## H1 activity guard

For the physically exposed A-phase H2 outputs, H1 all-low detection is filtered
for 300 cycles at 150 MHz (2 us). Short all-low intervals during normal H1
commutation therefore do not interrupt H2. The filter is implemented in
`CPLD_pwm.vhd` as a separate H2 enable guard; H1 pass-through and deadtime paths
remain independent.

The programmed build was verified non-intrusively with the IEEE 1149.1 SAMPLE
instruction. A3/A5 were strictly complementary, B5/B6 both toggled with no
simultaneous-high state, P3 remained continuously high, R3 remained low, and
R4/R5 produced PWM activity.

## Quartus verification

The project was compiled with Quartus II 13.0 SP1.  Analysis and synthesis,
fitting, assembly, and TimeQuest all completed with zero errors.  The fit uses
447 of 10,320 logic elements, 305 registers, and one PLL.  At the slow 85 C
timing corner, the worst setup and hold slacks are +1.852 ns and +0.454 ns.

`output_files/CPLD_cyc.sof` was generated with all 33 top-level pins assigned;
there are no unassigned-pin warnings. Before using it on power hardware, verify
that the recovered pin mapping matches the actual `CPLD_cyc` schematic.
External I/O timing constraints should be completed when the connected device
timing requirements are available.
