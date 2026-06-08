#!/usr/bin/env python3
"""Convert c-programming-specification-framework.html to Cursor skill markdown."""

from __future__ import annotations

import html
import re
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HTML_PATH = ROOT / "program" / "c-programming-specification-framework.html"
SKILL_DIR = ROOT / ".cursor" / "skills" / "c-programming-spec"


class ContentExtractor(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.in_main = False
        self.depth = 0
        self.chunks: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attr = dict(attrs)
        if tag == "div" and attr.get("id") == "main":
            self.in_main = True
            self.depth = 1
            return
        if not self.in_main:
            return
        if tag == "script":
            self.in_main = False
            return
        self.depth += 1
        self.chunks.append(self.get_starttag_text() or f"<{tag}>")

    def handle_endtag(self, tag: str) -> None:
        if not self.in_main:
            return
        self.chunks.append(f"</{tag}>")
        self.depth -= 1
        if self.depth <= 0:
            self.in_main = False

    def handle_data(self, data: str) -> None:
        if self.in_main:
            self.chunks.append(data)

    def handle_entityref(self, name: str) -> None:
        if self.in_main:
            self.chunks.append(f"&{name};")

    def handle_charref(self, name: str) -> None:
        if self.in_main:
            self.chunks.append(f"&#{name};")

    @property
    def html(self) -> str:
        return "".join(self.chunks)


def strip_highlight_spans(text: str) -> str:
    text = re.sub(r"<span[^>]*>", "", text)
    text = re.sub(r"</span>", "", text)
    return text


def pre_to_code(pre_html: str) -> str:
    m = re.search(r"<pre[^>]*>(.*?)</pre>", pre_html, re.DOTALL | re.IGNORECASE)
    if not m:
        raw = strip_highlight_spans(pre_html)
        return html.unescape(raw).strip()
    inner = m.group(1)
    inner = re.sub(r"<br\s*/?>", "\n", inner, flags=re.IGNORECASE)
    inner = strip_highlight_spans(inner)
    return html.unescape(inner).strip()


def inner_text(fragment: str) -> str:
    fragment = re.sub(r"<br\s*/?>", "\n", fragment, flags=re.IGNORECASE)
    fragment = re.sub(r"<svg[\s\S]*?</svg>", "", fragment, flags=re.IGNORECASE)
    text = re.sub(r"<[^>]+>", "", fragment)
    return html.unescape(re.sub(r"\s+", " ", text)).strip()


def md_inline(fragment: str) -> str:
    codes: list[str] = []

    def _save_code(m: re.Match[str]) -> str:
        codes.append(html.unescape(m.group(1)))
        return f"\x00CODE{len(codes) - 1}\x00"

    fragment = re.sub(r"<code>(.*?)</code>", _save_code, fragment, flags=re.DOTALL)
    fragment = re.sub(
        r"<strong>(.*?)</strong>",
        lambda m: f"**{inner_text(m.group(1))}**",
        fragment,
        flags=re.DOTALL,
    )
    fragment = re.sub(
        r"<a[^>]+href=[\"']([^\"']+)[\"'][^>]*>(.*?)</a>",
        lambda m: f"[{inner_text(m.group(2))}]({m.group(1)})",
        fragment,
        flags=re.DOTALL,
    )
    fragment = re.sub(r"<br\s*/?>", "\n", fragment, flags=re.IGNORECASE)
    fragment = re.sub(r"<[^>]+>", "", fragment)
    text = html.unescape(fragment)
    for i, code in enumerate(codes):
        text = text.replace(f"\x00CODE{i}\x00", f"`{code}`")
    text = re.sub(r"[ \t]+\n", "\n", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def table_to_md(table_html: str) -> str:
    rows = re.findall(r"<tr[^>]*>(.*?)</tr>", table_html, re.DOTALL | re.IGNORECASE)
    parsed: list[list[str]] = []
    for row in rows:
        cells = re.findall(
            r"<t[hd][^>]*>(.*?)</t[hd]>", row, re.DOTALL | re.IGNORECASE
        )
        parsed.append([md_inline(c).replace("|", "\\|") for c in cells])
    if not parsed:
        return ""
    header = parsed[0]
    lines = [
        "| " + " | ".join(header) + " |",
        "| " + " | ".join("---" for _ in header) + " |",
    ]
    for row in parsed[1:]:
        while len(row) < len(header):
            row.append("")
        lines.append("| " + " | ".join(row[: len(header)]) + " |")
    return "\n".join(lines)


def extract_code_label(block: str) -> str:
    m = re.search(r'<div class="code-label"[^>]*>(.*?)</div>', block, re.DOTALL)
    return inner_text(m.group(1)) if m else ""


def code_pair_to_md(pair_html: str) -> str:
    blocks = re.findall(
        r'<div class="code-block (bad|good)"[^>]*>([\s\S]*?)</div>\s*(?=<div class="code-block|$)',
        pair_html,
    )
    parts: list[str] = []
    for kind, body in blocks[:2]:
        label = extract_code_label(body)
        code = pre_to_code(body)
        if kind == "bad":
            heading = f"反面示例（{label}）" if label else "反面示例（不规范）"
        else:
            heading = f"正面示例（{label}）" if label else "正面示例（规范）"
        parts.append(f"**{heading}**\n\n```c\n{code}\n```")
    return "\n\n".join(parts)


def code_block_to_md(block_html: str) -> str:
    label = extract_code_label(block_html)
    code = pre_to_code(block_html)
    if label:
        return f"**示例：{label}**\n\n```c\n{code}\n```"
    return f"```c\n{code}\n```"


def split_mixed_examples(code: str) -> str:
    """Split inline ✅/❌ comments into labeled sections when both exist."""
    if "❌" not in code or "✅" not in code:
        return f"```c\n{code}\n```"
    parts = re.split(r"(?=/\* ✅|/\* ❌)", code)
    sections: list[str] = []
    for part in parts:
        part = part.strip()
        if not part:
            continue
        if part.startswith("/* ❌") or "❌" in part[:20]:
            sections.append(f"**反面示例**\n\n```c\n{part}\n```")
        elif part.startswith("/* ✅") or "✅" in part[:20]:
            sections.append(f"**正面示例**\n\n```c\n{part}\n```")
        else:
            sections.append(f"```c\n{part}\n```")
    return "\n\n".join(sections)


def rule_card_to_md(card_html: str) -> str:
    tag_m = re.search(r'<span class="rule-tag[^"]*">(.*?)</span>', card_html, re.DOTALL)
    title_m = re.search(r'<span class="rule-title">(.*?)</span>', card_html, re.DOTALL)
    body_m = re.search(
        r'<div class="rule-body">([\s\S]*?)</div>\s*</div>\s*$', card_html
    )

    tag = inner_text(tag_m.group(1)) if tag_m else ""
    title = inner_text(title_m.group(1)) if title_m else ""
    body = body_m.group(1) if body_m else ""

    lines = [f"### [{tag}] {title}" if tag else f"### {title}"]

    pres = list(re.finditer(r"<pre[\s\S]*?</pre>", body, re.IGNORECASE))
    tables = re.findall(r"<table[^>]*>[\s\S]*?</table>", body, re.IGNORECASE)

    text_only = body
    for pre in pres:
        text_only = text_only.replace(pre.group(0), "")
    for tbl in tables:
        text_only = text_only.replace(tbl, "")

    text_part = md_inline(text_only)
    if text_part:
        lines.extend(["", text_part])

    for tbl in tables:
        md_tbl = table_to_md(tbl)
        if md_tbl:
            lines.extend(["", md_tbl])

    for pre in pres:
        code = pre_to_code(pre.group(0))
        lines.extend(["", split_mixed_examples(code)])

    return "\n".join(lines)


def box_to_md(box_html: str, kind: str) -> str:
    content_m = re.search(
        r'<div class="(?:info|warn)-box">[\s\S]*?<div>([\s\S]*?)</div>\s*</div>',
        box_html,
    )
    if not content_m:
        return ""
    text = md_inline(content_m.group(1))
    prefix = "> 💡 " if kind == "info" else "> ⚠️ "
    return prefix + "\n> ".join(text.splitlines())


def ul_to_md(ul_html: str) -> str:
    items = re.findall(r"<li>([\s\S]*?)</li>", ul_html, re.IGNORECASE)
    return "\n".join(f"- {md_inline(item)}" for item in items)


def diagram_to_md(diagram_html: str) -> str:
    title_m = re.search(r'<div class="diagram-title">(.*?)</div>', diagram_html, re.DOTALL)
    if not title_m:
        return ""
    title = inner_text(title_m.group(1))
    texts = re.findall(r"<text[^>]*>([^<]+)</text>", diagram_html)
    bullets: list[str] = []
    seen: set[str] = set()
    for t in texts:
        t = t.strip()
        if len(t) < 2 or t in seen:
            continue
        seen.add(t)
        bullets.append(f"- {t}")
    lines = [f"**{title}**"]
    if bullets:
        lines.extend(["", *bullets[:40]])
    return "\n".join(lines)


def find_blocks(section_html: str) -> list[tuple[str, str]]:
    """Return ordered (kind, html) blocks in a section."""
    patterns = [
        ("rule-card", r'<div class="rule-card">[\s\S]*?</div>\s*</div>'),
        ("code-pair", r'<div class="code-pair">[\s\S]*?</div>\s*</div>'),
        (
            "code-block",
            r'<div class="code-block[^"]*"[^>]*>\s*<div class="code-label"[^>]*>[\s\S]*?</div>\s*<pre[\s\S]*?</pre>\s*</div>',
        ),
        ("table", r'<table class="data-table"[\s\S]*?</table>'),
        ("info-box", r'<div class="info-box">[\s\S]*?</div>\s*</div>'),
        ("warn-box", r'<div class="warn-box">[\s\S]*?</div>\s*</div>'),
        ("diagram", r'<div class="diagram"[^>]*>[\s\S]*?<div class="diagram-title">[\s\S]*?</div>\s*</div>'),
        ("ul", r'<ul class="source-list">[\s\S]*?</ul>'),
        ("p", r"<p>[\s\S]*?</p>"),
    ]

    matches: list[tuple[int, str, str]] = []
    for kind, pat in patterns:
        for m in re.finditer(pat, section_html, re.IGNORECASE):
            matches.append((m.start(), kind, m.group(0)))
    matches.sort(key=lambda x: x[0])

    # De-overlap: keep earliest match at each position
    used_ranges: list[tuple[int, int]] = []
    blocks: list[tuple[str, str]] = []
    for start, kind, chunk in matches:
        end = start + len(chunk)
        if any(not (end <= s or start >= e) for s, e in used_ranges):
            continue
        used_ranges.append((start, end))
        blocks.append((kind, chunk))
    return blocks


def section_to_md(section_html: str) -> str:
    title_m = re.search(r'<h2 class="section-title"[^>]*>(.*?)</h2>', section_html, re.DOTALL)
    title = inner_text(title_m.group(1)) if title_m else ""
    lines = [f"## {title}", ""]

    for kind, chunk in find_blocks(section_html):
        if kind == "rule-card":
            lines.append(rule_card_to_md(chunk))
        elif kind == "code-pair":
            lines.append(code_pair_to_md(chunk))
        elif kind == "code-block":
            lines.append(code_block_to_md(chunk))
        elif kind == "table":
            tbl = table_to_md(chunk)
            if tbl:
                lines.append(tbl)
        elif kind == "info-box":
            lines.append(box_to_md(chunk, "info"))
        elif kind == "warn-box":
            lines.append(box_to_md(chunk, "warn"))
        elif kind == "diagram":
            d = diagram_to_md(chunk)
            if d:
                lines.append(d)
        elif kind == "ul":
            lines.append(ul_to_md(chunk))
        elif kind == "p":
            t = md_inline(chunk)
            if t:
                lines.append(t)
        lines.append("")

    return "\n".join(lines).strip() + "\n"


def chapter_to_md(chapter_html: str) -> tuple[str, str, str]:
    id_m = re.search(r'id="(ch\d+)"', chapter_html)
    ch_id = id_m.group(1) if id_m else "ch0"
    badge_m = re.search(r'<div class="chapter-badge">(.*?)</div>', chapter_html, re.DOTALL)
    title_m = re.search(r'<h1 class="chapter-title">(.*?)</h1>', chapter_html, re.DOTALL)
    desc_m = re.search(r'<div class="chapter-desc">(.*?)</div>', chapter_html, re.DOTALL)

    badge = inner_text(badge_m.group(1)) if badge_m else ""
    title = inner_text(title_m.group(1)) if title_m else ch_id
    desc = md_inline(desc_m.group(1)) if desc_m else ""
    desc = re.sub(r"^参考来源：\s*", "", desc)

    lines = [f"# {title}", ""]
    if badge:
        lines.extend([f"*{badge}*", ""])
    if desc:
        lines.extend([f"参考来源：{desc}", ""])

    sections = re.findall(
        r'<h2 class="section-title"[^>]*>[\s\S]*?(?=<h2 class="section-title"|</section>)',
        chapter_html,
    )
    for sec in sections:
        lines.append(section_to_md(sec))
        lines.append("")

    return ch_id, title, "\n".join(lines).strip() + "\n"


def build_skill() -> None:
    from type_guide_to_md import type_guide_to_ch5_appendix

    raw = HTML_PATH.read_text(encoding="utf-8")
    extractor = ContentExtractor()
    extractor.feed(raw)
    main_html = extractor.html

    chapters = re.findall(
        r'<section class="chapter" id="ch\d+">[\s\S]*?</section>',
        main_html,
    )

    SKILL_DIR.mkdir(parents=True, exist_ok=True)
    ref_dir = SKILL_DIR / "reference"
    ref_dir.mkdir(exist_ok=True)

    index_lines: list[str] = []
    for ch_html in chapters:
        ch_id, title, md = chapter_to_md(ch_html)
        if ch_id == "ch5":
            md = md.replace(
                "[Barr Group — C99 Fixed-Width Integers](https://barrgroup.com/embedded-systems/how-to/c-fixed-width-integers-c99)",
                "[Barr Group — C99 Fixed-Width Integers](https://barrgroup.com/embedded-systems/how-to/c-fixed-width-integers-c99)、\n    [cppreference — 整数类型](https://en.cppreference.com/w/c/types/integer)（5.3–5.18 扩展）",
                1,
            )
            md = md.rstrip() + "\n\n" + type_guide_to_ch5_appendix()
        if ch_id == "ch10":
            md = md.rstrip() + """

> 函数存在**多处退出**且需释放 fd/内存/锁时，除 goto 清理链外，可优先使用 [第24章 RAII 风格资源管理](ch24.md) 与 [raii.h](raii.h)（`autofd`、`autofree`、`WITH_LOCK`、`REQUIRE`）；**仅单一 return** 且无资源泄漏风险时，使用原生 `close`/`free` 即可。
"""
        if ch_id == "ch14":
            md = md.rstrip() + """

> **多处 return** 下的堆内存释放可配合 [raii.h](raii.h) 的 `autofree`（见 [第24章](ch24.md)）；单 return 路径仍可直接 `free`。
"""
        if ch_id == "ch9":
            md = md.rstrip() + """

> 除 `_Static_assert` 外，GCC **`__attribute__` 契约检查**（nonnull、format、must_check 等）与 **BUILD_BUG_ON 宏族**见 [第23章 GCC 扩展与编译期检查](ch23.md)；运行期参数校验仍以 9.2 为主。
"""
        if ch_id == "ch18":
            md = md.replace(
                "| static 变量非线程安全 | 函数内 static 变量在多线程中需显式同步或改用 TLS（线程局部存储） |",
                "| static 变量非线程安全 | 函数内 static 变量在多线程中需显式同步或改用 TLS（线程局部存储） |\n"
                "| 单整型共享计数/标志 | 多线程下优先 `atomic_t` 与 `atomic_*`（见 [第22章](ch22.md)），勿用普通 `int` 或仅靠 `volatile` |",
                1,
            )
            md = md.rstrip() + """

> 共享 **单个整型**（引用计数、连接数、就绪标志）且操作可用一次原子 RMW 完成时，**优先**使用 [第22章 GCC 内置原子操作](ch22.md)；多字段或复杂结构仍用本节互斥锁方案。
"""
        (ref_dir / f"{ch_id}.md").write_text(md, encoding="utf-8")
        num = ch_id.replace("ch", "")
        index_lines.append(f"- [第{num}章 {title}](reference/{ch_id}.md)")

    from atomic_chapter_md import write_atomic_chapter
    from compile_checks_chapter_md import write_compile_checks_chapter
    from raii_chapter_md import write_raii_chapter

    write_atomic_chapter(ref_dir)
    index_lines.append("- [第22章 GCC 内置原子操作](reference/ch22.md)")
    write_compile_checks_chapter(ref_dir)
    index_lines.append("- [第23章 GCC 扩展与编译期检查](reference/ch23.md)")
    write_raii_chapter(ref_dir)
    index_lines.append("- [第24章 RAII 风格资源管理](reference/ch24.md)")

    skill_md = f"""---
name: c-programming-spec
description: Applies a self-contained C99 programming standard — file organization, naming, types, error handling, RAII resource cleanup (reference/raii.h), GCC atomics (reference/atomic.h), compile-time checks (reference/compile_checks.h), memory safety, and documentation. Use when writing or reviewing C code, managing resources with multiple exit paths, autofd/autofree/WITH_LOCK, or C programming standards.
---

# C 语言编程规范 (C99)

本 skill 为自包含的 C99 编程规范：核心章节见 `reference/ch1.md`–`ch21.md`；扩展含数据类型选择（第 5 章 5.3–5.18）、GCC 用户态原子操作（[atomic.h](reference/atomic.h)，第 22 章）、GCC 编译期检查（[compile_checks.h](reference/compile_checks.h)，第 23 章）、RAII 资源管理（[raii.h](reference/raii.h)，第 24 章）。权威外部资料见 [第 21 章](reference/ch21.md)。编写或审查 C 代码时，**必须先阅读相关 reference 文件**，并按规范执行。

## 使用方式

1. 根据任务类型，从下表定位对应章节并 **完整阅读** `reference/` 下的 markdown。
2. 每条规则包含：**级别标签**（必须/建议/可选）、**文字说明**、**正面示例**与**反面示例**（若源文档提供）。
3. 源文档仅有单一示例时，以该示例为准；同一代码块内含 ✅/❌ 标记时，分别作为正反面示例。
4. 规范冲突时优先级：**必须** > **建议** > **可选**。

## 规范级别

| 标签 | 含义 |
| --- | --- |
| **必须** | 强制遵守，违反可能导致 bug、安全漏洞或不可移植 |
| **建议** | 强烈推荐，偏离须有充分理由并文档化 |
| **可选** | 团队可自定，但须保持一致 |

## 章节索引

{chr(10).join(index_lines)}

## 配套头文件（reference/ 目录内）

- [atomic.h](reference/atomic.h) — GCC 用户态原子操作宏（第 22 章）
- [compile_checks.h](reference/compile_checks.h) — GCC 编译期检查宏（第 23 章）
- [raii.h](reference/raii.h) — RAII / cleanup 资源管理宏（第 24 章）

## 快速检查清单

- [ ] 文件名、头文件守卫、include 顺序符合第 1、12 章
- [ ] 命名带模块前缀，使用 snake_case（第 2 章）
- [ ] 数据类型选用 stdint/size_t，并参考第 5 章决策树与速查表（5.3–5.18）
- [ ] 变量声明即初始化，结构体成员按对齐降序（第 4、6 章）
- [ ] 公共 API 有参数校验与明确返回值语义（第 8、9、10 章）
- [ ] 宏安全或改用 static inline；内存分配/释放配对（第 11、14 章）
- [ ] 指针、整数、字符串操作无 UB（第 15、16、17 章）
- [ ] 多线程共享整型计数/标志优先 atomic（第 22 章），复杂状态用锁（第 18 章）
- [ ] 公共 API 声明 nonnull/format/must_check；布局不变量用 static_assert 或 BUILD_BUG_ON（第 23 章）
- [ ] 多 exit 路径的资源释放用 raii.h 或 goto 清理；单 return 可用原生 close/free（第 24 章）
- [ ] 注释说明「为什么」，公共接口有 Doxygen（第 20 章）

## 权威参考

完整参考链接见 [reference/ch21.md](reference/ch21.md)。
"""

    (SKILL_DIR / "SKILL.md").write_text(skill_md, encoding="utf-8")
    print(f"Generated {len(chapters)} + ch22 + ch23 + ch24 chapter files in {SKILL_DIR}")


if __name__ == "__main__":
    build_skill()
