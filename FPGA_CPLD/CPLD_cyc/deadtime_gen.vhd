-- =========================================================
-- 死区时间生成器 (Deadtime Generator)
-- =========================================================
-- 用途：
--   为半桥上下管互补信号插入死区时间，防止直通（shoot-through）。
--   上管导通前等待下管完全关断，下管导通前等待上管完全关断。
--
-- 工作原理：
--   - 当 in_p 请求导通且 in_n 已请求关断时，延迟 DT_CYCLES 个时钟
--     周期后才允许 out_p 输出高电平
--   - 当 in_n 请求导通且 in_p 已请求关断时，延迟 DT_CYCLES 个时钟
--     周期后才允许 out_n 输出高电平
--   - 任意一管请求关断时立即响应（无延迟关断）
--   - 硬件硬互锁保护：无论何种情况，绝对禁止上下管同时输出高电平
--
-- 参数：
--   DT_CYCLES = 死区时钟周期数
--   clk = 150 MHz（周期 ≈ 6.67 ns）
--   DT_CYCLES = 150 → 死区时间 = 150 × 6.67 ns ≈ 1.0 us
-- =========================================================

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity deadtime_gen is
    generic (
        -- 150 MHz 时钟周期 ≈ 6.67 ns
        -- 150 cycles = 1000 ns (1 us) 死区时间
        DT_CYCLES : integer := 150
    );
    port (
        clk      : in  std_logic;
        rst_n    : in  std_logic;
        in_p     : in  std_logic;  -- 同步后的上管信号
        in_n     : in  std_logic;  -- 同步后的下管信号
        out_p    : out std_logic;  -- 带死区的上管输出
        out_n    : out std_logic   -- 带死区的下管输出
    );
end entity;

architecture rtl of deadtime_gen is
    signal cnt_p : integer range 0 to DT_CYCLES := 0;
    signal cnt_n : integer range 0 to DT_CYCLES := 0;
    signal p_reg : std_logic := '0';
    signal n_reg : std_logic := '0';
begin

    process(clk, rst_n)
    begin
        if rst_n = '0' then
            cnt_p <= 0;
            p_reg <= '0';
            cnt_n <= 0;
            n_reg <= '0';
        elsif rising_edge(clk) then
            -- 上管 P 处理：当 P 请求导通且 N 已请求关断时开始延时计数
            if (in_p = '1') and (in_n = '0') then
                if cnt_p < DT_CYCLES then
                    cnt_p <= cnt_p + 1;
                    p_reg <= '0';
                else
                    p_reg <= '1';
                end if;
            else
                cnt_p <= 0;
                p_reg <= '0';       -- 立即关断
            end if;

            -- 下管 N 处理：当 N 请求导通且 P 已请求关断时开始延时计数
            if (in_n = '1') and (in_p = '0') then
                if cnt_n < DT_CYCLES then
                    cnt_n <= cnt_n + 1;
                    n_reg <= '0';
                else
                    n_reg <= '1';
                end if;
            else
                cnt_n <= 0;
                n_reg <= '0';       -- 立即关断
            end if;
        end if;
    end process;

    -- 硬件硬互锁保护：无论何种情况，绝对禁止上、下管同时为高
    out_p <= p_reg when (n_reg = '0') else '0';
    out_n <= n_reg when (p_reg = '0') else '0';

end architecture;
