-- =========================================================
-- 顶层模块 —— CPLD PWM 三相完整版本
-- =========================================================
-- 当前状态：
--   ① ABC 三相 H1 门极透传 —— 各 4 路经异步双级触发器同步器 → 输出
--   ② ABC 三相 H2 PWM 生成 —— 载波发生器 + 单极性 PWM 调制
--   ③ PWM 捕获模块带异步双级触发器同步器
--   ④ PLL 将 50 MHz 晶振倍频至 150 MHz 系统时钟
--
-- 时钟方案：
--   外部晶振 50 MHz → PLL (×3) → 内部 150 MHz 系统时钟
--   PLL locked 信号可用于系统就绪指示
--
-- 门极向量定义（24 bit = 3 相 × 2 H桥 × 4 开关管）：
--   gates_in(3..0)   = A 相 H1（A+ A- B+ B-），来自 DSP，透传
--   gates_in(7..4)   = B 相 H1（A+ A- B+ B-），来自 DSP，透传
--   gates_in(11..8)  = C 相 H1（A+ A- B+ B-），来自 DSP，透传
--   gates_in(23..12) = 保留（未使用）
--
--   gates_out(3..0)   = A 相 H1（同步透传）
--   gates_out(7..4)   = B 相 H1（同步透传）
--   gates_out(11..8)  = C 相 H1（同步透传）
--   gates_out(15..12) = A 相 H2（CPLD PWM 生成）
--   gates_out(19..16) = B 相 H2（CPLD PWM 生成）
--   gates_out(23..20) = C 相 H2（CPLD PWM 生成）
--
-- H1 透传数据路径（异步双级同步，每路独立 sync_2ff）：
--   延迟 = 2 clk ≈ 13.3 ns，输出抖动 ±1 clk (6.7 ns)
--
-- H2 PWM 策略（单极性、中心对齐、180° 移相）：
--   - 载波频率 20 kHz，中心对齐上下计数，TBPRD = 3750
--   - H1/H2 交错并联，等效开关频率 40 kHz
--   - 调制量 Q15 格式，限幅 ±0.98
--   - DSP 闭锁检测：H1 四路全低 → H2 强制关断
--
-- PWM 捕获通道：
--   pwm_cap_in → 双级同步器 → 周期/占空比测量
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
        -- H1 透传: gates_in(11..0)；gates_in(23..12) 保留
        gates_in   : in  std_logic_vector(23 downto 0);

        -- 调制量输入（Q15 有符号定点数，来自 DSP）
        mod_a      : in  signed(15 downto 0);              -- A 相调制量 [-1.0, 1.0)
        mod_b      : in  signed(15 downto 0);              -- B 相调制量
        mod_c      : in  signed(15 downto 0);              -- C 相调制量

        -- 门极输出（24 路，送隔离驱动）
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

    -- H1 同步门极信号（12 bit = 3 相 × 4 路，PLL 锁定前强制关断）
    signal sync_h1       : std_logic_vector(11 downto 0);

    -- 载波信号
    signal carrier       : integer range 0 to 3750;        -- H1 参考载波（预留）
    signal carrier_h2    : integer range 0 to 3750;        -- H2 比较载波（移相 180°）

    -- H2 PWM 门极信号（每相 4 bit，由 CPLD 根据调制量生成）
    signal h2_gates_a    : std_logic_vector(3 downto 0);   -- A 相 H2
    signal h2_gates_b    : std_logic_vector(3 downto 0);   -- B 相 H2
    signal h2_gates_c    : std_logic_vector(3 downto 0);   -- C 相 H2

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
            inclk0  => clk_50m,
            c0      => clk_150m,
            locked  => pll_locked_i
        );

    pll_locked <= pll_locked_i;

    -- =========================================================
    -- A 相 H1 透传（异步双级触发器同步器）
    -- gates_in(3..0) → sync_2ff × 4 → sync_h1(3..0)
    --   3 = B-（B 桥臂下管）   2 = B+（B 桥臂上管）
    --   1 = A-（A 桥臂下管）   0 = A+（A 桥臂上管）
    -- =========================================================
    u_sync_a_h1_0: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(0), sync_out => sync_h1(0));
    u_sync_a_h1_1: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(1), sync_out => sync_h1(1));
    u_sync_a_h1_2: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(2), sync_out => sync_h1(2));
    u_sync_a_h1_3: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(3), sync_out => sync_h1(3));

    -- =========================================================
    -- B 相 H1 透传（异步双级触发器同步器）
    -- gates_in(7..4) → sync_2ff × 4 → sync_h1(7..4)
    -- =========================================================
    u_sync_b_h1_0: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(4), sync_out => sync_h1(4));
    u_sync_b_h1_1: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(5), sync_out => sync_h1(5));
    u_sync_b_h1_2: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(6), sync_out => sync_h1(6));
    u_sync_b_h1_3: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(7), sync_out => sync_h1(7));

    -- =========================================================
    -- C 相 H1 透传（异步双级触发器同步器）
    -- gates_in(11..8) → sync_2ff × 4 → sync_h1(11..8)
    -- =========================================================
    u_sync_c_h1_0: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(8),  sync_out => sync_h1(8));
    u_sync_c_h1_1: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(9),  sync_out => sync_h1(9));
    u_sync_c_h1_2: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(10), sync_out => sync_h1(10));
    u_sync_c_h1_3: entity work.sync_2ff(rtl)
        port map (clk => clk_150m, rst_n => rst_n,
                  async_in => gates_in(11), sync_out => sync_h1(11));

    -- =========================================================
    -- H1 门极输出（PLL 锁定安全闭锁）
    -- 未锁定时全部关断，防止不稳定时钟导致误输出
    -- =========================================================
    gates_out(3 downto 0)   <= sync_h1(3 downto 0)   when pll_locked_i = '1' else (others => '0');
    gates_out(7 downto 4)   <= sync_h1(7 downto 4)   when pll_locked_i = '1' else (others => '0');
    gates_out(11 downto 8)  <= sync_h1(11 downto 8)  when pll_locked_i = '1' else (others => '0');

    -- =========================================================
    -- 载波发生器 —— 中心对齐三角波 + H2 180° 移相输出
    --
    -- TBPRD = 150M / (2 × 20k) = 3750
    -- carrier    = H1 参考（预留，供 DSP 侧参照）
    -- carrier_h2 = H2 比较载波（180° 移相），送各相 PWM 模块
    --
    -- H1/H2 交错并联，等效开关频率 40 kHz
    -- =========================================================
    u_carrier: entity work.CPLD_pwm_carrier(rtl)
        port map (
            clk         => clk_150m,
            rst_n       => rst_n,
            carrier     => carrier,
            carrier_h2  => carrier_h2
        );

    -- =========================================================
    -- A 相 H2 PWM 生成
    --
    -- 根据调制量 mod_a 和移相载波 carrier_h2 生成 A 相 H2 门极信号。
    -- h1_gates 输入用于 DSP 闭锁检测：当 H1 四路全低时，
    -- 判定 DSP 处于预充电/故障闭锁状态，H2 输出强制关断。
    -- =========================================================
    u_phase_a: entity work.CPLD_pwm_phase(rtl)
        port map (
            clk         => clk_150m,
            rst_n       => rst_n,
            modulation  => mod_a,
            carrier     => carrier_h2,
            h1_gates    => sync_h1(3 downto 0),
            h2_gates    => h2_gates_a
        );

    -- =========================================================
    -- B 相 H2 PWM 生成
    -- =========================================================
    u_phase_b: entity work.CPLD_pwm_phase(rtl)
        port map (
            clk         => clk_150m,
            rst_n       => rst_n,
            modulation  => mod_b,
            carrier     => carrier_h2,
            h1_gates    => sync_h1(7 downto 4),
            h2_gates    => h2_gates_b
        );

    -- =========================================================
    -- C 相 H2 PWM 生成
    -- =========================================================
    u_phase_c: entity work.CPLD_pwm_phase(rtl)
        port map (
            clk         => clk_150m,
            rst_n       => rst_n,
            modulation  => mod_c,
            carrier     => carrier_h2,
            h1_gates    => sync_h1(11 downto 8),
            h2_gates    => h2_gates_c
        );

    -- =========================================================
    -- H2 门极输出（PLL 锁定安全闭锁）
    -- =========================================================
    gates_out(15 downto 12) <= h2_gates_a when pll_locked_i = '1' else (others => '0');
    gates_out(19 downto 16) <= h2_gates_b when pll_locked_i = '1' else (others => '0');
    gates_out(23 downto 20) <= h2_gates_c when pll_locked_i = '1' else (others => '0');

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
