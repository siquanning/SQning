#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
注释语言检查脚本 (check_comments.py)

用途：检查工程中 .c/.h 文件的注释是否为简体中文，禁止英文描述性注释。
规则（启发式）：
  - 允许：中文
  - 允许：代码引用（标识符/寄存器/宏名，如 PieCtrlRegs、EALLOW、CPU_CLK、GPIO5）
  - 允许：注释内嵌的代码片段（含 = ; ( ) # 等代码特征的行）
  - 允许：Doxygen 标签（@file/@brief 等）、预处理指令（#define/#if 等）、C 关键字
  - 允许：常见技术缩写白名单（如 ADC、PWM、ISR、Trip-Zone）
  - 禁止：普通英文单词（如 enable、output、the、is）
用法：
  python tools/check_comments.py                 # 默认扫描 SRC/ 和 INCLUDE/
  python tools/check_comments.py --scan SRC      # 只扫描指定目录
  python tools/check_comments.py --allow usb     # 追加白名单单词
返回码：0 = 通过；1 = 存在英文注释
"""

import argparse
import re
import sys
from pathlib import Path

# 工程根目录（脚本位于 <工程根>/tools/ 下）
PROJECT_ROOT = Path(__file__).resolve().parent.parent

# 默认扫描目录（相对工程根）
DEFAULT_DIRS = ["SRC", "INCLUDE"]

# 跳过不检查的文件名前缀（TI 外设库/第三方库，本身是英文注释且不允许修改）
SKIP_PREFIXES = ("DSP2833x_", "IQmathLib", "SFO")

# 允许出现在中文注释中的技术缩写 / C 关键字（小写比较）
TECH_WORDS = {
    # C 关键字（中文注释里提到这些术语很正常）
    "void", "volatile", "define", "if", "for", "while", "return", "int", "static",
    "const", "struct", "union", "enum", "signed", "unsigned", "short", "long",
    "char", "float", "double", "bool", "true", "false", "null", "typedef", "extern",
    "inline", "register", "goto", "switch", "case", "default", "do", "else",
    "break", "continue", "sizeof", "include", "ifdef", "ifndef", "endif", "pragma",
    "undef", "todo",
    # 常见技术缩写 / 单位 / 外设名
    "adc", "pwm", "gpio", "isr", "cpu", "timer", "ram", "flash", "debug", "release",
    "step", "led", "dsp", "pie", "ier", "ifr", "edis", "eallow", "sci", "spi", "can",
    "ecan", "epwm", "fpu", "iq", "pll", "xtal", "jtag", "coff", "ccs", "xintf", "saram",
    "emif", "fifo", "uart", "usb", "i2c", "eeprom", "dma", "cpld", "fpga", "rtos", "bios",
    "crc", "ecc", "wdt", "clk", "boot", "rom", "mcu", "c28x", "f28335", "delfino", "xds",
    "hrpwm", "hrtim", "eqep", "ecap", "mcbsp", "group", "channel", "int", "bit", "byte",
    "word", "port", "io", "hz", "khz", "mhz", "us", "ns", "ms", "vcc", "gnd", "ok", "id",
    "api", "os", "zone", "tdo", "tdi", "tck", "tms", "canopen", "modbus", "all", "half",
    "value", "high", "low", "on", "off", "sign", "period", "config", "init", "version",
    "trip", "hspclkdiv", "clkdiv", "ctrmode", "tbprd", "tbctl", "wake", "reset", "fault",
}

# 注释文本（/* */ 或 //）
COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)

# 英文单词候选（含下划线/数字的整段）
WORD_RE = re.compile(r"[A-Za-z][A-Za-z0-9_]*")

# 代码片段特征（该行出现这些字符，视为内嵌代码，整行英文词放行）
CODE_CHARS_RE = re.compile(r"[;()=*{}\[\]&#]")

# 需要从注释中剥离的模式：URL、Windows 路径、Doxygen 标签、预处理指令
STRIP_PATTERNS = [
    re.compile(r"[a-zA-Z]+://\S+"),     # URL
    re.compile(r"[A-Za-z]:\\\S+"),      # Windows 路径
    re.compile(r"@[a-zA-Z]+"),          # Doxygen 标签: @file @brief @param ...
    re.compile(r"#[a-zA-Z]+"),          # 预处理指令: #define #if #include ...
]


def extract_comments(src):
    """返回 [(行号, 注释正文)] 列表"""
    out = []
    for m in COMMENT_RE.finditer(src):
        line = src[: m.start()].count("\n") + 1
        text = m.group()
        if text.startswith("//"):
            body = text[2:]
        else:
            body = text[2:-2]
        out.append((line, body))
    return out


def clean_body(body):
    for pat in STRIP_PATTERNS:
        body = pat.sub(" ", body)
    return body


def is_identifier_like(word):
    """判断是否为代码标识符/寄存器/宏（允许出现）"""
    core = word.replace("_", "")
    if not core:
        return True
    if len(word) == 1:
        return True                      # 单字母（MAIN.c 的 c、占位符 x 等）
    if any(ch.isdigit() for ch in core):
        return True                      # 含数字：GPIO5, F28335, CPU_CLK
    if "_" in word:
        return True                      # 含下划线：宏/变量名
    if core.isupper() and len(core) > 1:
        return True                      # 全大写：宏/寄存器/缩写 (EALLOW, PIEACK)
    if re.match(r"^[A-Z]{2,}[A-Z0-9_]*[a-z]", word):
        return True                      # 大写前缀+小写后缀：GPIOx, DMAx
    if re.search(r"[a-z][A-Z]", word):
        return True                      # 驼峰：PieCtrlRegs, AppConfig
    if re.search(r"[A-Z][a-z][A-Z]", word):
        return True                      # IQMath 之类
    return False


def check_body(body, allow):
    """返回该注释中的违规英文词列表 [(词, 所在行片段), ...]"""
    bad = []
    body = clean_body(body)
    for m in WORD_RE.finditer(body):
        w = m.group()
        if is_identifier_like(w):
            continue
        # 取该词所在的行片段，判断是否为内嵌代码
        line_start = body.rfind("\n", 0, m.start()) + 1
        line_end = body.find("\n", m.end())
        if line_end == -1:
            line_end = len(body)
        line_frag = body[line_start:line_end]
        if CODE_CHARS_RE.search(line_frag):
            continue
        if w.lower() in allow:
            continue
        bad.append((w, line_frag.strip()))
    return bad


def scan_file(path, allow):
    try:
        src = path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        print(f"[错误] 无法读取 {path}: {e}", file=sys.stderr)
        return []
    hits = []
    for line, body in extract_comments(src):
        for w, frag in check_body(body, allow):
            snippet = frag
            if len(snippet) > 60:
                snippet = snippet[:60] + "..."
            hits.append((path, line, w, snippet))
    return hits


def main():
    ap = argparse.ArgumentParser(description="检查 C 注释是否为简体中文")
    ap.add_argument("--scan", action="append", default=None, metavar="DIR",
                    help="要扫描的目录（可多次指定，相对工程根）")
    ap.add_argument("--allow", action="append", default=[], metavar="WORD",
                    help="追加白名单单词（小写）")
    args = ap.parse_args()

    allow = TECH_WORDS | {w.lower() for w in args.allow}
    dirs = args.scan if args.scan else DEFAULT_DIRS

    all_hits = []
    scanned = 0
    for d in dirs:
        root = PROJECT_ROOT / d
        if not root.is_dir():
            print(f"[警告] 目录不存在，跳过: {root}", file=sys.stderr)
            continue
        for f in sorted(root.rglob("*")):
            if f.suffix.lower() not in (".c", ".h"):
                continue
            if f.name.startswith(SKIP_PREFIXES):
                continue
            scanned += 1
            all_hits.extend(scan_file(f, allow))

    print(f"已扫描 {scanned} 个用户文件（跳过 TI 库文件）")
    if not all_hits:
        print("[通过] 所有注释均为简体中文（或仅含允许的技术缩写/代码引用）")
        return 0

    print("[失败] 发现英文注释，请改为简体中文（技术缩写/寄存器名/代码引用除外）：")
    for path, line, word, snippet in all_hits:
        rel = path.relative_to(PROJECT_ROOT)
        print(f"  {rel}:{line}  英文词 \"{word}\"  <- {snippet}")
    print("\n提示：若该词是必须保留的技术缩写，可运行：")
    print("  python tools/check_comments.py --allow <单词>")
    return 1


if __name__ == "__main__":
    sys.exit(main())