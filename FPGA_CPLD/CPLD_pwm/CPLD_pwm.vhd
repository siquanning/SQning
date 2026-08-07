-- =========================================================
-- 顶层模块 —— CPLD PWM 调试版本
-- =========================================================
-- 当前状态（调试阶段）：
--   ① A 相 H1 门极透传 —— 经异步双级触发器同步器 → 输出
--   ② PWM 捕获模块带异步双级触发器同步器
--   ③ PLL 将 50 MHz 晶振倍频至 150 MHz 系统时钟
--
-- 调试完成后恢复三相完整功能时，参考原版注释中的架构。
--
-- 时钟方案：
--   外部晶振 50 MHz → PLL (×3) → 内部 150 MHz 系统时钟
--   PLL locked 信号可用于系统就绪指示
--
-- A 相 H1 透传数据路径（异步双级同步）：
--   gates_in(0) ─→ [FF0] ─→ [FF1] ─→ gates_out(0)   A+
--   gates_in(1) ─→ [FF0] ─→ [FF1] ─→ gates_out(1)   A-
--   gates_in(2) ─→ [FF0] ─→ [FF1] ─→ gates_out(2)   B+
--   gates_in(3) ─→ [FF0] ─→ [FF1] ─→ gates_out(3)   B-
--       ↑ clk_150m 每路独立 sync_2ff 实例
--   延迟 = 2 clk ≈ 13.3 ns，输出抖动 ±1 clk (6.7 ns)
--   gates_out(23..4) → '0'                 其他通道全部关断
--
-- PWM 捕获通道：
--   pwm_cap_in       → 双级同步器 → 周期/占空比测量
--   period / high_time / duty_valid 输出供调试观测
-- =========================================================

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity CPLD_pwm is
    port (
        -- 时钟 & 复位
        clk_50m    : in  std_logic;                        -- 50 MHz 外部晶振输入 → PLL
        rst_n      : in  std_logic;                        -- 低有效异步复位

        -- DSP 并行总线输入（24 路门极向量）
        -- 调试阶段仅使用 A相 H1: gates_in(3..0)
        gates_in   : in  std_logic_vector(23 downto 0);

        -- 调制量输入（调试阶段暂不使用，保留接口）
        mod_a      : in  signed(15 downto 0);
        mod_b      : in  signed(15 downto 0);
        mod_c      : in  signed(15 downto 0);

        -- 门极输出（24 路，送隔离驱动）
        -- 调试阶段: gates_out(3..0) = A相 H1 透传; 其余 = '0'
        gates_out  : out std_logic_vector(23 downto 0);

        -- PWM 捕获输入（外部异步 PWM 信号，经双级同步后测量）
        pwm_cap_in : in  std_logic;                        -- 待测 PWM 输入

        -- PWM 捕获测量结果
        cap_period    : out unsigned(15 downto 0);         -- 测得周期（150 MHz clk 数）
        cap_high_time : out unsigned(15 downto 0);         -- 测得高电平时间
        cap_valid     : out std_logic;                     -- 测量有效脉冲

        -- PLL 状态
        pll_locked : out std_logic                         -- PLL 已锁定指示
    );
end entity;

architecture structural of CPLD_pwm is
    -- =========================================================
    -- 内部信号
    -- =========================================================
    signal clk_150m      : std_logic;                      -- PLL 输出 150 MHz 系统时钟
    signal pll_locked_i  : std_logic;                      -- PLL 锁定信号（内部）
    signal pll_areset    : std_logic;                      -- PLL 异步复位（高有效 = ~rst_n）
    signal sync_gates    : std_logic_vector(3 downto 0);   -- 同步后的门极信号（PLL 锁定前强制关断）

    -- 未使用调制量输入（调试阶段保留，不连接，Quartus 会产生 info 级警告可忽略）
begin
    -- =========================================================
    -- PLL: 50 MHz → 150 MHz
    -- 上电后 PLL 锁定前，系统时钟不稳定，locked=0 期间
    -- 逻辑处于复位状态以确保安全
    -- =========================================================
    pll_areset <= not rst_n;                               -- 低有效 → 高有效

    u_pll: entity work.pll_50m_to_150m(syn)
        port map (
            areset  => pll_areset,
            inclk0  => clk_50m,                             -- 已修改为顶层端口 clk_50m
            c0      => clk_150m,
            locked  => pll_locked_i
        );

    pll_locked <= pll_locked_i;

    -- =========================================================
    -- A 相 H1 透传（异步双级触发器同步器）
    --
    -- gates_out(3) = B-  (A 相 H1 B 桥臂下管)
    -- gates_out(2) = B+  (A 相 H1 B 桥臂上管)
    -- gates_out(1) = A-  (A 相 H1 A 桥臂下管)
    -- gates_out(0) = A+  (A 相 H1 A 桥臂上管)
    --
    -- 每路 gates_in 经过独立 sync_2ff 同步到 150 MHz 域后再输出，
    -- 延迟 = 2 clk ≈ 13.3 ns，输出抖动 ±1 clk (6.7 ns)
    -- =========================================================
    u_sync_aplus  : entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(0), sync_out => sync_gates(0));
    u_sync_aminus : entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(1), sync_out => sync_gates(1));
    u_sync_bplus  : entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(2), sync_out => sync_gates(2));
    u_sync_bminus : entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(3), sync_out => sync_gates(3));

    -- PLL 锁定安全闭锁：未锁定时门极全部关断，防止不稳定时钟导致误输出
    gates_out(3 downto 0) <= sync_gates when pll_locked_i = '1' else (others => '0');

    -- B 相 H1 + H2、C 相 H1 + H2、A 相 H2 全部关断
    gates_out(23 downto 4) <= (others => '0');

    -- =========================================================
    -- PWM 捕获模块（含异步双级触发器同步器）
    --
    -- 外部异步 PWM 信号 pwm_cap_in →
    --   双级 FF 同步器（亚稳态防护）→
    --   上升/下降沿检测 →
    --   周期计数器 + 高电平计数器 →
    --   结果锁存输出
    --
    -- cap_valid 在每个 PWM 周期结束（上升沿）时产生单周期脉冲，
    -- 同时更新 cap_period 和 cap_high_time
    -- =========================================================
    u_pwm_cap: entity work.pwm_capture(rtl)
        port map (
            clk         => clk_150m,
            rst_n       => rst_n,
            pwm_in      => pwm_cap_in,
            period      => cap_period,
            high_time   => cap_high_time,
            duty_valid  => cap_valid
        );

end architecture;