-- =========================================================
-- 1. 载波发生器 —— 中心对齐三角波 + H2 180° 移相输出
-- =========================================================
-- 时钟与载波参数：
--   系统时钟  = 150 MHz（周期约 6.67 ns）
--   PWM 频率  = 20 kHz（周期 50 us）
--   计数模式  = 中心对齐（上下计数）
--   TBPRD    = 150M / (2 × 20k) = 3750
--   完整载波周期 = 2 × TBPRD = 7500 个时钟 = 50 us
--
-- 载波波形：
--   carrier（H1 参考）：0 → 1 → ... → 3750 → 3749 → ... → 0 → 重复
--   carrier_h2（H2 比较用）：3750 → 3749 → ... → 0 → 1 → ... → 3750 → 重复
--   两者互为 180° 相移，H2 载波由组合逻辑 3750 - count 产生
--
-- 移相目的：
--   H1 和 H2 桥臂交错并联，180° 移相使等效开关频率翻倍（40 kHz），
--   输出电流纹波频率加倍、幅值减半，降低滤波器件体积和损耗。
-- =========================================================

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity CPLD_pwm_carrier is
    port (
        clk        : in  std_logic;                     -- 150 MHz 系统时钟
        rst_n      : in  std_logic;                     -- 低有效异步复位
        carrier    : out integer range 0 to 3750;       -- H1 参考载波（供 DSP 侧参照，本设计预留）
        carrier_h2 : out integer range 0 to 3750        -- H2 比较载波（移相 180°，送 phase 模块做 PWM 比较）
    );
end entity;

architecture rtl of CPLD_pwm_carrier is
    -- TBPRD：三角波峰值计数值，决定 PWM 周期
    -- 150 MHz 时钟下：TBPRD = 150e6 / (2 × 20e3) = 3750
    constant TBPRD : integer := 3750;

    -- count：当前三角波计数值，上坡 0→3750，下坡 3750→0
    signal count : integer range 0 to TBPRD := 0;

    -- dir：计数方向标志，'1' = 递增（上坡），'0' = 递减（下坡）
    signal dir   : std_logic := '1';
begin
    -- -------------------------------------------------------
    -- 中心对齐三角波计数器
    -- 每个 clk 上升沿 count 递增或递减，在 0 和 TBPRD 处换向
    -- -------------------------------------------------------
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            -- 异步复位：计数器归零，方向置为上坡
            count <= 0;
            dir   <= '1';
        elsif rising_edge(clk) then
            if dir = '1' then
                -- 上坡区间：count 从 0 递增至 TBPRD
                if count = TBPRD then
                    -- 到达峰值 3750，换向为下坡，count 降至 3749
                    -- 避免在峰值停留两个周期（保持对称）
                    dir   <= '0';
                    count <= TBPRD - 1;
                else
                    count <= count + 1;
                end if;
            else
                -- 下坡区间：count 从 TBPRD 递减至 0
                if count = 0 then
                    -- 到达谷值 0，换向为上坡，count 升至 1
                    dir   <= '1';
                    count <= 1;
                else
                    count <= count - 1;
                end if;
            end if;
        end if;
    end process;

    -- -------------------------------------------------------
    -- 载波输出
    -- -------------------------------------------------------
    -- H1 参考载波：直接输出计数器值，三角波 0↔3750
    carrier    <= count;

    -- H2 移相载波：3750 - count，将三角波峰谷互换，等效相移 180°
    -- 例如：count = 0    → carrier_h2 = 3750（H2 在谷值时 H1 在峰值）
    --       count = 3750 → carrier_h2 = 0    （H2 在峰值时 H1 在谷值）
    -- 纯组合逻辑减法，不引入额外时钟延迟
    carrier_h2 <= TBPRD - count;
end architecture;
