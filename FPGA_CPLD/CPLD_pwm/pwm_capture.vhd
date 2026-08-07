-- =========================================================
-- PWM 捕获模块（含异步双级触发器同步器）
-- =========================================================
-- 功能:
--   对外部异步 PWM 输入信号进行双级同步后，测量其周期和高电平时间。
--   测量结果以 150 MHz 系统时钟周期（~6.67 ns）为单位。
--
-- 测量时序:
--   ┌─────────┐        ┌──────────┐
--   │ 上升沿   │   →    │ 上升沿    │
--   │ 启动计数 │        │ 锁存结果  │
--   └────┬────┘        └─────┬─────┘
--        ↓                   ↓
--   ┌────┴───────────────────┴────┐
--   │  pwm_in  ─┬──────┐    ┌─────
--   │           │high  │    │
--   └───────────┴──────┴────┴─────┘
--        ↑              ↑
--   period_cnt 持续计数  │
--       high_cnt  ──────┘
--
-- 输出:
--   period      : PWM 周期（单位：clk 周期 ≈ 6.67 ns）
--   high_time   : PWM 高电平时间
--   duty_valid  : 单周期脉冲，指示 period 和 high_time 有效
-- =========================================================

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity pwm_capture is
    port (
        clk         : in  std_logic;       -- 150 MHz 系统时钟
        rst_n       : in  std_logic;       -- 低有效异步复位
        pwm_in      : in  std_logic;       -- 待测 PWM 信号（异步输入）
        period      : out unsigned(15 downto 0);  -- 测得周期（clk 数）
        high_time   : out unsigned(15 downto 0);  -- 测得高电平时间（clk 数）
        duty_valid  : out std_logic        -- 测量完成脉冲（高有效，持续 1 clk）
    );
end entity;

architecture rtl of pwm_capture is
    -- =========================================================
    -- 信号定义
    -- =========================================================

    -- 同步器输出（2-FF 同步后的 PWM 信号）
    signal pwm_sync     : std_logic;

    -- 边沿检测
    signal pwm_d1       : std_logic;       -- 延迟一拍
    signal rising_edge_det  : std_logic;   -- 上升沿检测
    signal falling_edge_det : std_logic;   -- 下降沿检测

    -- 计数器
    signal period_cnt   : unsigned(15 downto 0);
    signal high_cnt     : unsigned(15 downto 0);
    signal high_active  : std_logic;       -- 高电平计数使能标志

    -- 结果锁存
    signal period_latch : unsigned(15 downto 0);
    signal high_latch   : unsigned(15 downto 0);
    signal valid_flag   : std_logic;

    -- ASYNC_REG 属性用于同步器
    attribute async_reg : string;
    signal sync_ff0     : std_logic;
    signal sync_ff1     : std_logic;
    attribute async_reg of sync_ff0 : signal is "true";
    attribute async_reg of sync_ff1 : signal is "true";
begin
    -- =========================================================
    -- 第 1 级：双触发器同步器（异步 → 150 MHz 域）
    -- 输入 pwm_in 来自外部异步域，必须经过双级同步
    -- =========================================================
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            sync_ff0 <= '0';
            sync_ff1 <= '0';
        elsif rising_edge(clk) then
            sync_ff0 <= pwm_in;           -- 第 1 级（可能亚稳态）
            sync_ff1 <= sync_ff0;         -- 第 2 级（亚稳态概率可忽略）
        end if;
    end process;
    pwm_sync <= sync_ff1;

    -- =========================================================
    -- 第 2 级：边沿检测
    -- pwm_d1 为上一拍的同步值，用于边沿检测
    -- =========================================================
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            pwm_d1 <= '0';
        elsif rising_edge(clk) then
            pwm_d1 <= pwm_sync;
        end if;
    end process;

    -- 上升沿 = 当前为 1 且上一拍为 0
    rising_edge_det  <= '1' when (pwm_sync = '1' and pwm_d1 = '0') else '0';
    -- 下降沿 = 当前为 0 且上一拍为 1
    falling_edge_det <= '1' when (pwm_sync = '0' and pwm_d1 = '1') else '0';

    -- =========================================================
    -- 第 3 级：周期 & 高电平时间测量
    -- =========================================================
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            period_cnt  <= (others => '0');
            high_cnt    <= (others => '0');
            high_active <= '0';
            period_latch <= (others => '0');
            high_latch   <= (others => '0');
            valid_flag   <= '0';
        elsif rising_edge(clk) then
            -- 默认：valid_flag 只持续一个周期
            valid_flag <= '0';

            -- 周期计数器（自由运行，在上升沿时锁存并清零）
            period_cnt <= period_cnt + 1;

            if rising_edge_det = '1' then
                -- 上升沿 → 锁存上一周期的测量结果
                period_latch <= period_cnt;   -- 捕获完整周期计数值
                high_latch   <= high_cnt;     -- 捕获高电平计数值
                valid_flag   <= '1';          -- 数据有效脉冲

                -- 重置计数器，开始新一周期的测量
                period_cnt   <= (others => '0');
                high_cnt     <= (others => '0');
                high_active  <= '1';          -- 进入高电平区间
            elsif falling_edge_det = '1' then
                -- 下降沿 → 停止高电平计数
                high_active  <= '0';
            end if;

            -- 高电平计数器（仅在高电平区间计数）
            if high_active = '1' then
                high_cnt <= high_cnt + 1;
            end if;
        end if;
    end process;

    -- =========================================================
    -- 输出驱动
    -- =========================================================
    period     <= period_latch;
    high_time  <= high_latch;
    duty_valid <= valid_flag;
end architecture;
