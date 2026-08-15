from __future__ import annotations

import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT, TA_RIGHT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Frame, HRFlowable, ListFlowable, ListItem, PageBreak,
    PageTemplate, Paragraph, Spacer, Table, TableStyle
)
from reportlab.platypus.tableofcontents import TableOfContents


ROOT = Path(r"E:\repos\resst")
SOURCE = ROOT / "docs" / "SST_CHB前级整流DSP工程设计与核查报告.md"
OUTPUT = ROOT / "output" / "pdf" / "SST_CHB前级整流DSP工程设计与核查报告.pdf"

NAVY = colors.HexColor("#12304A")
BLUE = colors.HexColor("#1677A6")
CYAN = colors.HexColor("#35A7C8")
PALE = colors.HexColor("#EAF4F8")
INK = colors.HexColor("#25323B")
MUTED = colors.HexColor("#657784")
LINE = colors.HexColor("#CAD8DF")
WARM = colors.HexColor("#FFF3DD")


def register_fonts():
    candidates = [
        ("CJK", r"C:\Windows\Fonts\msyh.ttc"),
        ("CJK", r"C:\Windows\Fonts\simhei.ttf"),
    ]
    bold_candidates = [
        ("CJKBold", r"C:\Windows\Fonts\msyhbd.ttc"),
        ("CJKBold", r"C:\Windows\Fonts\simhei.ttf"),
    ]
    for name, path in candidates:
        if Path(path).exists():
            pdfmetrics.registerFont(TTFont(name, path))
            break
    for name, path in bold_candidates:
        if Path(path).exists():
            pdfmetrics.registerFont(TTFont(name, path))
            break


def inline(text: str) -> str:
    text = html.escape(text.strip())
    text = re.sub(r"`([^`]+)`", r'<font name="CJK" color="#0B668A">\1</font>', text)
    text = re.sub(r"\*\*([^*]+)\*\*", r'<font name="CJKBold">\1</font>', text)
    return text


class ReportDoc(BaseDocTemplate):
    def __init__(self, filename, styles):
        super().__init__(filename, pagesize=A4, leftMargin=18*mm, rightMargin=18*mm,
                         topMargin=21*mm, bottomMargin=18*mm,
                         title="TMS320F28335 SST/CHB 前级整流 DSP 工程设计与核查报告",
                         author="工程核查报告")
        self.styles_ref = styles
        body = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="body")
        self.addPageTemplates(PageTemplate(id="normal", frames=body,
                                           onPage=self.draw_header_footer))

    def draw_header_footer(self, canvas, doc):
        page = canvas.getPageNumber()
        if page == 1:
            return
        canvas.saveState()
        canvas.setStrokeColor(LINE)
        canvas.setLineWidth(0.5)
        canvas.line(18*mm, A4[1]-14*mm, A4[0]-18*mm, A4[1]-14*mm)
        canvas.setFont("CJK", 7.5)
        canvas.setFillColor(MUTED)
        canvas.drawString(18*mm, A4[1]-11*mm, "RESST · SST/CHB 前级整流 DSP")
        canvas.drawRightString(A4[0]-18*mm, 10*mm, f"第 {page} 页")
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and flowable.style.name in ("H1", "H2"):
            level = 0 if flowable.style.name == "H1" else 1
            text = flowable.getPlainText()
            key = f"h{level}-{self.seq.nextf('heading')}"
            self.canv.bookmarkPage(key)
            self.notify("TOCEntry", (level, text, self.page, key))


def make_styles():
    base = getSampleStyleSheet()
    return {
        "body": ParagraphStyle("Body", parent=base["BodyText"], fontName="CJK", fontSize=9.2,
                               leading=15, textColor=INK, alignment=TA_LEFT, spaceAfter=6),
        "lead": ParagraphStyle("Lead", fontName="CJK", fontSize=10, leading=17,
                               textColor=INK, alignment=TA_LEFT, backColor=PALE,
                               borderColor=CYAN, borderWidth=0.8, borderPadding=9, spaceAfter=10),
        "H1": ParagraphStyle("H1", fontName="CJKBold", fontSize=16, leading=22,
                             textColor=NAVY, spaceBefore=12, spaceAfter=8, keepWithNext=True,
                             borderColor=CYAN, borderWidth=0, borderPadding=0),
        "H2": ParagraphStyle("H2", fontName="CJKBold", fontSize=11.5, leading=17,
                             textColor=BLUE, spaceBefore=9, spaceAfter=5, keepWithNext=True),
        "H3": ParagraphStyle("H3", fontName="CJKBold", fontSize=10, leading=15,
                             textColor=INK, spaceBefore=7, spaceAfter=4, keepWithNext=True),
        "code": ParagraphStyle("Code", fontName="CJK", fontSize=7.8, leading=12,
                               textColor=colors.HexColor("#DDEAF0"), backColor=NAVY,
                               borderPadding=8, leftIndent=0, rightIndent=0, spaceAfter=8),
        "small": ParagraphStyle("Small", fontName="CJK", fontSize=7.5, leading=10, textColor=MUTED),
        "toc0": ParagraphStyle("TOC0", fontName="CJKBold", fontSize=10, leading=17, textColor=NAVY,
                               leftIndent=0, firstLineIndent=0, spaceBefore=2),
        "toc1": ParagraphStyle("TOC1", fontName="CJK", fontSize=8.5, leading=14, textColor=INK,
                               leftIndent=12, firstLineIndent=0),
    }


def cover(story, styles):
    story += [Spacer(1, 32*mm)]
    story.append(Table([["ENGINEERING REVIEW  ·  V1.0"]], colWidths=[72*mm], rowHeights=[9*mm],
                       style=TableStyle([("BACKGROUND", (0,0), (-1,-1), CYAN),
                                         ("TEXTCOLOR", (0,0), (-1,-1), colors.white),
                                         ("FONTNAME", (0,0), (-1,-1), "CJKBold"),
                                         ("FONTSIZE", (0,0), (-1,-1), 8),
                                         ("ALIGN", (0,0), (-1,-1), "CENTER"),
                                         ("VALIGN", (0,0), (-1,-1), "MIDDLE")]), hAlign="LEFT"))
    story += [Spacer(1, 13*mm),
              Paragraph("TMS320F28335", ParagraphStyle("CoverA", fontName="CJKBold", fontSize=30,
                                                        leading=34, textColor=NAVY)),
              Paragraph("SST/CHB 前级整流 DSP", ParagraphStyle("CoverB", fontName="CJKBold", fontSize=25,
                                                               leading=34, textColor=BLUE)),
              Spacer(1, 4*mm),
              HRFlowable(width="38%", thickness=3, color=CYAN, hAlign="LEFT"),
              Spacer(1, 6*mm),
              Paragraph("工程设计与核查报告", ParagraphStyle("CoverC", fontName="CJK", fontSize=18,
                                                            leading=25, textColor=INK)),
              Spacer(1, 24*mm)]
    meta = [["工程路径", r"E:\repos\resst"], ["适用阶段", "单相逐相 · 低压 · 限流样机调试"],
            ["报告日期", "2026 年 8 月 14 日"], ["核查结论", "具备低压逐相上机验证条件"]]
    story.append(Table(meta, colWidths=[32*mm, 104*mm], rowHeights=[10*mm]*4,
                       style=TableStyle([("BACKGROUND", (0,0), (0,-1), NAVY),
                                         ("TEXTCOLOR", (0,0), (0,-1), colors.white),
                                         ("BACKGROUND", (1,0), (1,-1), PALE),
                                         ("TEXTCOLOR", (1,0), (1,-1), INK),
                                         ("FONTNAME", (0,0), (0,-1), "CJKBold"),
                                         ("FONTNAME", (1,0), (1,-1), "CJK"),
                                         ("FONTSIZE", (0,0), (-1,-1), 9),
                                         ("GRID", (0,0), (-1,-1), 0.4, colors.white),
                                         ("VALIGN", (0,0), (-1,-1), "MIDDLE"),
                                         ("LEFTPADDING", (0,0), (-1,-1), 8)])))
    story += [Spacer(1, 30*mm), Paragraph("安全边界", styles["H2"]),
              Paragraph("本报告确认的是代码结构、控制流程与低压逐相调试准备度；不替代高压绝缘、功率器件、驱动链路及整机额定工况验证。", styles["lead"]),
              PageBreak()]


def parse_table(lines, styles):
    rows = []
    for idx, line in enumerate(lines):
        protected = re.sub(r"`([^`]*)`", lambda m: "`" + m.group(1).replace("|", "│") + "`", line)
        cells = [c.strip() for c in protected.strip().strip("|").split("|")]
        if idx == 1 and all(re.fullmatch(r":?-{3,}:?", c) for c in cells):
            continue
        rows.append([Paragraph(inline(c), styles["small"]) for c in cells])
    n = max(len(r) for r in rows)
    for r in rows:
        r.extend([""] * (n-len(r)))
    usable = A4[0] - 36*mm
    weights = []
    for c in range(n):
        weights.append(max(6, min(28, max(len(lines[r].split("|")[c+1].strip()) if c+1 < len(lines[r].split("|")) else 0 for r in range(len(lines))))))
    widths = [usable*w/sum(weights) for w in weights]
    return Table(rows, colWidths=widths, repeatRows=1, splitByRow=1, hAlign="LEFT",
                 style=TableStyle([("BACKGROUND", (0,0), (-1,0), NAVY),
                                   ("TEXTCOLOR", (0,0), (-1,0), colors.white),
                                   ("FONTNAME", (0,0), (-1,0), "CJKBold"),
                                   ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, colors.HexColor("#F4F8FA")]),
                                   ("GRID", (0,0), (-1,-1), 0.45, LINE),
                                   ("VALIGN", (0,0), (-1,-1), "MIDDLE"),
                                   ("LEFTPADDING", (0,0), (-1,-1), 5),
                                   ("RIGHTPADDING", (0,0), (-1,-1), 5),
                                   ("TOPPADDING", (0,0), (-1,-1), 5),
                                   ("BOTTOMPADDING", (0,0), (-1,-1), 5)]))


def markdown_story(text, styles):
    lines = text.splitlines()[7:]  # cover owns title and metadata
    story = []
    i = 0
    para = []
    def flush():
        nonlocal para
        if para:
            content = " ".join(x.strip().rstrip("  ") for x in para)
            style = styles["lead"] if not any(isinstance(x, Paragraph) and x.style.name == "Body" for x in story[-2:]) and len(story) < 5 else styles["body"]
            story.append(Paragraph(inline(content), style))
            para = []
    while i < len(lines):
        line = lines[i]
        if line.startswith("```"):
            flush(); block=[]; i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                block.append(lines[i]); i += 1
            story.append(Paragraph("<br/>".join(html.escape(x).replace(" ", "&nbsp;") for x in block), styles["code"]))
        elif line.startswith("|"):
            flush(); block=[]
            while i < len(lines) and lines[i].startswith("|"):
                block.append(lines[i]); i += 1
            story += [parse_table(block, styles), Spacer(1, 7)]
            continue
        elif re.match(r"^#{2,4} ", line):
            flush(); level = len(line)-len(line.lstrip("#")); title=line[level+1:]
            story.append(Paragraph(inline(title), styles[{2:"H1",3:"H2",4:"H3"}[level]]))
        elif re.match(r"^[-*] ", line):
            flush(); items=[]
            while i < len(lines) and re.match(r"^[-*] ", lines[i]):
                items.append(ListItem(Paragraph(inline(lines[i][2:]), styles["body"]), leftIndent=8)); i += 1
            story.append(ListFlowable(items, bulletType="bullet", start="circle", leftIndent=14, bulletFontName="CJK", bulletFontSize=6))
            continue
        elif re.match(r"^\d+\. ", line):
            flush(); items=[]
            while i < len(lines) and re.match(r"^\d+\. ", lines[i]):
                items.append(ListItem(Paragraph(inline(re.sub(r"^\d+\. ", "", lines[i])), styles["body"]), leftIndent=8)); i += 1
            story.append(ListFlowable(items, bulletType="1", leftIndent=16, bulletFontName="CJK", bulletFontSize=8))
            continue
        elif line.strip() == "---":
            flush(); story += [Spacer(1, 4), HRFlowable(width="100%", thickness=0.6, color=LINE), Spacer(1, 4)]
        elif not line.strip():
            flush()
        else:
            para.append(line)
        i += 1
    flush()
    return story


def main():
    register_fonts()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    styles = make_styles()
    story = []
    cover(story, styles)
    story += [Paragraph("目录", styles["H1"])]
    toc = TableOfContents()
    toc.levelStyles = [styles["toc0"], styles["toc1"]]
    story += [toc, PageBreak()]
    story += markdown_story(SOURCE.read_text(encoding="utf-8"), styles)
    doc = ReportDoc(str(OUTPUT), styles)
    doc.multiBuild(story)
    print(OUTPUT)


if __name__ == "__main__":
    main()
