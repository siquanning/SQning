-- =========================================================
-- 异步双级触发器同步器 (Generic 2-FF Synchronizer)
-- =========================================================
-- 用途:
--   将异步输入信号安全地同步到本地时钟域，防止亚稳态传播。
--   两级 D 触发器级联，MTBF（平均无故障时间）呈指数级增长。
--
-- 工作原理:
--   ┌──────┐    ┌──────┐
--   │ FF0  │───→│ FF1  │───→ sync_out (同步到 clk 域)
--   └──┬───┘    └──┬───┘
--      ↑clk        ↑clk
--   async_in
--
-- 使用约束 (SDC):
--   set_false_path -to [get_registers *sync_reg*[0]*]
--   set_max_delay -from [get_ports async_in] -to [get_registers *sync_reg*[0]*] 2.0
--
-- ASYNC_REG 属性告知综合器/布局布线器:
--   - 第一级 FF 允许亚稳态（不作时序优化）
--   - FF0 和 FF1 尽可能靠近布局，缩短 MTBF
-- =========================================================

library ieee;
use ieee.std_logic_1164.all;

entity sync_2ff is
    generic (
        STAGES : positive := 2         -- 同步级数（默认为 2，可扩展为 3）
    );
    port (
        clk       : in  std_logic;     -- 目标时钟域
        rst_n     : in  std_logic;     -- 异步复位，低有效
        async_in  : in  std_logic;     -- 异步输入信号
        sync_out  : out std_logic      -- 同步后的输出信号
    );
end entity;

architecture rtl of sync_2ff is
    -- 同步寄存器链
    signal sync_chain : std_logic_vector(STAGES-1 downto 0);

    -- ASYNC_REG 属性：告知 Quartus 该寄存器为同步器链，
    -- 允许第一级出现亚稳态，并优化两级间的布局延迟
    attribute async_reg : string;
    attribute async_reg of sync_chain : signal is "true";

    -- PRESERVE 属性：防止综合器优化掉同步寄存器
    attribute preserve : boolean;
    attribute preserve of sync_chain : signal is true;
begin
    process(clk, rst_n)
    begin
        if rst_n = '0' then
            sync_chain <= (others => '0');
        elsif rising_edge(clk) then
            -- 移位同步：第 0 级捕获异步输入，后续级逐级传递
            sync_chain(0) <= async_in;
            for i in 1 to STAGES-1 loop
                sync_chain(i) <= sync_chain(i-1);
            end loop;
        end if;
    end process;

    -- 输出最后一级（已同步到 clk 域，亚稳态概率可忽略）
    sync_out <= sync_chain(STAGES-1);
end architecture;
