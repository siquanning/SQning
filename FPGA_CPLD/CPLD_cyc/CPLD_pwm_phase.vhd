-- =========================================================
-- 2. 单相 H2 PWM 生成模块
-- =========================================================
-- 功能流水线（每时钟周期执行一次）：
--   ① DSP 活跃检测 → ② 调制量限幅 → ③ CMPA 计算 →
--   ④ PWM 比较      → ⑤ 单极性分配 → ⑥ 门极输出
--
-- 门极向量定义（4 bit / H桥）：
--   h2_gates(0) = A+（A 桥臂上管）
--   h2_gates(1) = A-（A 桥臂下管）
--   h2_gates(2) = B+（B 桥臂上管）
--   h2_gates(3) = B-（B 桥臂下管）
--
-- 调制量格式：
--   modulation 为 Q15 定点有符号数（signed 16-bit）
--   数值范围 [-32768, 32767] 对应归一化调制量 [-1.0, 1.0)
--   限幅后范围 [-32112, 32112] 对应 [-0.98, 0.98]
--
-- 单极性 PWM 策略（每相两个 H 桥，各桥臂互补导通）：
--   正半周（mod ≥ 0）：A 桥臂上管常通，B 桥臂按 PWM 斩波
--   负半周（mod < 0）：A 桥臂按 PWM 斩波，B 桥臂上管常通
--   目的：降低开关损耗（每半周期仅一个桥臂开关动作），
--         同时保持输出电流连续性和电压线性度。
--
-- DSP 闭锁逻辑：
--   当 H1 四路门极全部为低时，判定 DSP 处于预充电闭锁或
--   故障闭锁状态，H2 输出全部强制关断（全 0），防止功率
--   器件在异常状态下误导通。
-- =========================================================

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity CPLD_pwm_phase is
    port (
        clk        : in  std_logic;                       -- 150 MHz 系统时钟
        rst_n      : in  std_logic;                       -- 低有效异步复位
        modulation : in  signed(15 downto 0);             -- Q15 调制量 [-1.0, 1.0)，来自 DSP
        carrier    : in  integer range 0 to 3750;         -- H2 比较载波（已移相 180°）
        h1_gates   : in  std_logic_vector(3 downto 0);   -- 本相 H1 门极（A+ A- B+ B-），用于闭锁检测
        h2_gates   : out std_logic_vector(3 downto 0)    -- 本相 H2 门极（A+ A- B+ B-），送功率器件
    );
end entity;

architecture rtl of CPLD_pwm_phase is
    -- =========================================================
    -- 固定占空比宏定义（调试用）
    -- USE_FIXED_DUTY = true  时，忽略外部 modulation 输入，
    -- 使用 FIXED_DUTY_Q15 作为调制量。
    -- FIXED_DUTY_Q15 = duty × 32768
    --   0.50 → 16384
    --   0.25 →  8192
    --   0.75 → 24576
    -- =========================================================
    constant USE_FIXED_DUTY  : boolean := true;
    constant FIXED_DUTY_Q15  : integer := 8192;   -- 0.50 × 32768

    -- TBPRD：载波峰值，150M / (2 × 20k) = 3750
    constant TBPRD         : integer := 3750;

    -- 调制量限幅阈值（Q15 格式）
    -- 0.98 × 2^15 = 32112.64 ≈ 32112
    -- 留出 2% 裕量，防止比较值越界导致 PWM 占空比异常
    constant MOD_CLAMP_MAX : integer := 32112;            -- +0.98 in Q15
    constant MOD_CLAMP_MIN : integer := -32112;           -- -0.98 in Q15

    -- Q15 缩放因子：2^15 = 32768
    -- CMPA = |mod_int| × TBPRD / 32768
    -- 乘法在除法之前执行以保持精度
    constant Q15_SCALE     : integer := 32768;

begin
    process(clk, rst_n)
        -- mod_int ：限幅后的调制量（Q15 整数）
        -- mod_abs ：调制量绝对值，用于 CMPA 计算
        -- cmpa    ：比较寄存器值，范围 0 ~ 3675（0.98 × 3750）
        -- pwm_tmp ：PWM 比较结果原始值
        -- dsp_active：DSP 是否已使能 PWM 输出
        variable mod_int    : integer;
        variable mod_abs    : integer;
        variable cmpa       : integer;
        variable pwm_tmp    : std_logic;
        variable dsp_active : boolean;
    begin
        if rst_n = '0' then
            -- 异步复位：H2 全部关断，安全状态
            h2_gates <= (others => '0');
        elsif rising_edge(clk) then
            -- -------------------------------------------------------
            -- ① DSP 活跃检测
            -- 正常 PWM 运行时，上下管互补导通，至少 2 路为高
            -- 四路全低 → DSP 闭锁（预充电 / 故障 / 未初始化）
            -- -------------------------------------------------------
            dsp_active := (h1_gates /= "0000");

            if not dsp_active then
                -- DSP 未使能：H2 硬闭锁，四路全关断
                -- 防止功率器件在无控制状态下误导通
                h2_gates <= (others => '0');
            else
                -- -------------------------------------------------------
                -- ② 调制量读取与限幅
                -- 将 Q15 signed 转为 integer，钳位至 ±0.98
                -- USE_FIXED_DUTY=true 时使用固定占空比，忽略外部输入
                -- -------------------------------------------------------
                if USE_FIXED_DUTY then
                    mod_int := FIXED_DUTY_Q15;
                else
                    mod_int := to_integer(modulation);
                end if;
                if mod_int > MOD_CLAMP_MAX then
                    mod_int := MOD_CLAMP_MAX;             -- 钳位上限 +0.98
                elsif mod_int < MOD_CLAMP_MIN then
                    mod_int := MOD_CLAMP_MIN;             -- 钳位下限 -0.98
                end if;

                -- -------------------------------------------------------
                -- ③ CMPA 比较值计算
                -- CMPA = |m| × TBPRD / 2^15
                -- 将归一化调制量幅值换算为载波域的计数值
                -- 乘除顺序：先乘后除，保整数精度
                -- 最大 CMPA = 32112 × 3750 / 32768 ≈ 3675
                -- -------------------------------------------------------
                if mod_int < 0 then
                    mod_abs := -mod_int;                  -- 取绝对值
                else
                    mod_abs := mod_int;
                end if;
                cmpa := (mod_abs * TBPRD) / Q15_SCALE;    -- Q15 → 载波域

                -- -------------------------------------------------------
                -- ④ PWM 比较
                -- pwm_on = (cmpa ≥ carrier)
                -- 当载波计数值 ≤ CMPA 时 PWM 为高，产生中心对齐对称 PWM
                -- 调制量越大 → CMPA 越大 → PWM 占空比越大
                -- -------------------------------------------------------
                if cmpa >= carrier then
                    pwm_tmp := '1';                       -- PWM 有效电平
                else
                    pwm_tmp := '0';                       -- PWM 无效电平
                end if;

                -- -------------------------------------------------------
                -- ⑤ + ⑥ 单极性 PWM 分配 → 门极输出
                --
                -- 正半周（mod ≥ 0）：
                --   A+ = 1, A- = 0       → A 桥臂上管常通
                --   B+ = ~pwm, B- = pwm  → B 桥臂按 PWM 互补斩波
                --
                -- 负半周（mod < 0）：
                --   A+ = ~pwm, A- = pwm  → A 桥臂按 PWM 互补斩波
                --   B+ = 1, B- = 0       → B 桥臂上管常通
                --
                -- 每个半周期仅一个桥臂高频开关，总开关损耗降低约 50%
                -- -------------------------------------------------------
                if mod_int >= 0 then
                    -- 正半周：A 常通，B 斩波
                    h2_gates(0) <= '1';                   -- A+：常通
                    h2_gates(1) <= '0';                   -- A-：常断
                    h2_gates(2) <= not pwm_tmp;           -- B+：~pwm（pwm=1 时 B+ 断，pwm=0 时 B+ 通）
                    h2_gates(3) <= pwm_tmp;               -- B-：pwm（pwm=1 时 B- 通，pwm=0 时 B- 断）
                else
                    -- 负半周：A 斩波，B 常通
                    h2_gates(0) <= not pwm_tmp;           -- A+：~pwm
                    h2_gates(1) <= pwm_tmp;               -- A-：pwm
                    h2_gates(2) <= '1';                   -- B+：常通
                    h2_gates(3) <= '0';                   -- B-：常断
                end if;
            end if;
        end if;
    end process;
end architecture;
