library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Cyclone IV board top level. Only signals with verified PCB pin mappings are
-- exposed here. The full functional implementation remains in CPLD_pwm.
entity CPLD_cyc is
    port (
        clk_50m    : in  std_logic;
        rst_n      : in  std_logic;
        gates_in   : in  std_logic_vector(11 downto 0);
        gates_out  : out std_logic_vector(15 downto 0);
        pwm_cap_in : in  std_logic;
        cap_valid  : out std_logic;
        pll_locked : out std_logic
    );
end entity;

architecture structural of CPLD_cyc is
    signal gates_in_full  : std_logic_vector(23 downto 0);
    signal gates_out_full : std_logic_vector(23 downto 0);
    signal cap_period_nc  : unsigned(15 downto 0);
    signal cap_high_nc    : unsigned(15 downto 0);
begin
    -- Reserved H1 inputs are inactive. The core currently uses fixed Q15
    -- modulation, so no physical modulation input pins are required.
    gates_in_full(11 downto 0)  <= gates_in;
    gates_in_full(23 downto 12) <= (others => '0');
    gates_out <= gates_out_full(15 downto 0);

    u_pwm: entity work.CPLD_pwm(structural)
        port map (
            clk_50m       => clk_50m,
            rst_n         => rst_n,
            gates_in      => gates_in_full,
            mod_a         => to_signed(8192, 16),
            mod_b         => to_signed(8192, 16),
            mod_c         => to_signed(8192, 16),
            gates_out     => gates_out_full,
            pwm_cap_in    => pwm_cap_in,
            cap_period    => cap_period_nc,
            cap_high_time => cap_high_nc,
            cap_valid     => cap_valid,
            pll_locked    => pll_locked
        );
end architecture;
