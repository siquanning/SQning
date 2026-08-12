library ieee;
use ieee.std_logic_1164.all;

entity tb_deadtime_gen is
end entity;

architecture sim of tb_deadtime_gen is
    signal clk   : std_logic := '0';
    signal rst_n : std_logic := '0';
    signal in_p  : std_logic := '0';
    signal in_n  : std_logic := '1';
    signal out_p : std_logic;
    signal out_n : std_logic;
    signal seen_p : boolean := false;
    signal seen_n : boolean := false;
begin
    clk <= not clk after 3.333 ns;

    dut: entity work.deadtime_gen(rtl)
        generic map (DT_CYCLES => 150)
        port map (
            clk => clk,
            rst_n => rst_n,
            in_p => in_p,
            in_n => in_n,
            out_p => out_p,
            out_n => out_n
        );

    stimulus: process
    begin
        wait for 100 ns;
        rst_n <= '1';
        for i in 0 to 7 loop
            wait for 25 us;
            in_p <= not in_p;
            in_n <= not in_n;
        end loop;
        wait for 5 us;
        assert seen_p report "out_p never asserted" severity failure;
        assert seen_n report "out_n never asserted" severity failure;
        report "deadtime_gen positive and negative outputs both toggled" severity note;
        wait;
    end process;

    monitor: process(clk)
    begin
        if rising_edge(clk) then
            if out_p = '1' then
                seen_p <= true;
            end if;
            if out_n = '1' then
                seen_n <= true;
            end if;
        end if;
    end process;
end architecture;
