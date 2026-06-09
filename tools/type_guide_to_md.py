#!/usr/bin/env python3
"""Convert program/spec/c-type-guide.html to markdown sections for ch5 integration."""

from __future__ import annotations

import html
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPE_GUIDE_PATH = ROOT / "program" / "spec" / "c-type-guide.html"

# Section id -> (5.x number, title without emoji for anchor)
TYPE_GUIDE_SECTIONS: list[tuple[str, str, str]] = [
    ("s-decision", "5.3", "日常编程类型选择决策树"),
    ("s-basic", "5.4", "C 语言基本算术类型"),
    ("s-exact", "5.5", "精确宽度整数类型（Exact-width）"),
    ("s-least", "5.6", "最小宽度类型（Minimum-width）"),
    ("s-fast", "5.7", "最快宽度类型（Fastest）"),
    ("s-maxptr", "5.8", "最大宽度与指针整数类型"),
    ("s-stddef", "5.9", "size_t 与 ptrdiff_t"),
    ("s-other", "5.10", "其他重要标准库类型"),
    ("s-factors", "5.11", "全面考量因素"),
    ("s-overflow", "5.12", "溢出与未定义行为"),
    ("s-promotion", "5.13", "整型提升（Integer Promotion）规则"),
    ("s-platform", "5.14", "平台可移植性与数据模型"),
    ("s-perf", "5.15", "性能考量"),
    ("s-api", "5.16", "API 兼容性与系统调用类型"),
    ("s-flowchart", "5.17", "完整类型选型流程图"),
    ("s-summary", "5.18", "类型速查总结表"),
]


def strip_spans(text: str) -> str:
    text = re.sub(r"<span[^>]*>", "", text)
    text = re.sub(r"</span>", "", text)
    return text


def inner_text(fragment: str) -> str:
    fragment = re.sub(r"<br\s*/?>", "\n", fragment, flags=re.IGNORECASE)
    fragment = re.sub(r"<svg[\s\S]*?</svg>", "", fragment, flags=re.IGNORECASE)
    text = re.sub(r"<[^>]+>", "", fragment)
    return html.unescape(re.sub(r"[ \t]+", " ", text)).strip()


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
        r"<em>(.*?)</em>",
        lambda m: f"*{inner_text(m.group(1))}*",
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
    fragment = re.sub(r"<li[^>]*>", "\n- ", fragment, flags=re.IGNORECASE)
    fragment = re.sub(r"</li>", "", fragment, flags=re.IGNORECASE)
    fragment = re.sub(r"<[^>]+>", "", fragment)
    text = html.unescape(fragment)
    for i, code in enumerate(codes):
        text = text.replace(f"\x00CODE{i}\x00", f"`{code}`")
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def pre_to_code(block: str) -> str:
    m = re.search(r"<pre[^>]*>(.*?)</pre>", block, re.DOTALL | re.IGNORECASE)
    if not m:
        return ""
    inner = m.group(1)
    inner = re.sub(r"</?code>", "", inner, flags=re.IGNORECASE)
    inner = re.sub(r"<br\s*/?>", "\n", inner, flags=re.IGNORECASE)
    inner = strip_spans(inner)
    return html.unescape(inner).strip()


def split_mixed_examples(code: str) -> str:
    if "❌" not in code and "✅" not in code:
        if "// 危险" in code and "// 安全" in code:
            parts = re.split(r"(?=// 危险|// 安全)", code)
            sections: list[str] = []
            for part in parts:
                part = part.strip()
                if not part:
                    continue
                if part.startswith("// 危险"):
                    sections.append(f"**反面示例**\n\n```c\n{part}\n```")
                elif part.startswith("// 安全"):
                    sections.append(f"**正面示例**\n\n```c\n{part}\n```")
                else:
                    sections.append(f"```c\n{part}\n```")
            return "\n\n".join(sections)
        return f"```c\n{code}\n```"

    parts = re.split(r"(?=/\* ✅|/\* ❌|// ✅|// ❌)", code)
    sections = []
    for part in parts:
        part = part.strip()
        if not part:
            continue
        head = part[:30]
        if "❌" in head:
            sections.append(f"**反面示例**\n\n```c\n{part}\n```")
        elif "✅" in head:
            sections.append(f"**正面示例**\n\n```c\n{part}\n```")
        else:
            sections.append(f"```c\n{part}\n```")
    return "\n\n".join(sections)


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


def decision_tree_to_md(tree_html: str) -> str:
    items = re.findall(
        r'<div class="dt-item">([\s\S]*?)</div>\s*(?=<div class="dt-item"|</div>\s*</div>|$)',
        tree_html,
    )
    lines: list[str] = []
    for item in items:
        q_m = re.search(r'<div class="dt-q">(.*?)</div>', item, re.DOTALL)
        a_m = re.search(r'<div class="dt-a">(.*?)</div>', item, re.DOTALL)
        if not q_m or not a_m:
            continue
        q = md_inline(q_m.group(1))
        tags = re.findall(
            r'<span class="type-tag">(.*?)</span>', a_m.group(1), re.DOTALL
        )
        tag_part = " / ".join(f"`{inner_text(t)}`" for t in tags) if tags else ""
        note_html = re.sub(
            r'<span class="type-tag">[\s\S]*?</span>',
            "",
            a_m.group(1),
            flags=re.DOTALL,
        )
        note_html = re.sub(
            r'<span class="dt-arrow">[\s\S]*?</span>',
            "",
            note_html,
            flags=re.DOTALL,
        )
        note = md_inline(note_html).strip().strip("（）")
        answer = tag_part
        if note:
            answer = f"{tag_part}（{note}）" if tag_part else note
        lines.append(f"- **{q}** → {answer}")
    return "\n".join(lines)


def chart_to_md(chart_html: str) -> str:
    texts = re.findall(r"<text[^>]*>([^<]+)</text>", chart_html)
    bullets: list[str] = []
    seen: set[str] = set()
    for t in texts:
        t = t.strip()
        if len(t) < 2 or t in seen:
            continue
        seen.add(t)
        bullets.append(f"- {t}")
    return "\n".join(bullets[:50])


def callout_to_md(block: str) -> str:
    body_m = re.search(r'<div class="callout-body">([\s\S]*?)</div>', block)
    icon_m = re.search(r'<div class="callout-icon">(.*?)</div>', block, re.DOTALL)
    if not body_m:
        return ""
    icon = inner_text(icon_m.group(1)) if icon_m else "ℹ️"
    text = md_inline(body_m.group(1))
    return "> " + icon + " " + "\n> ".join(text.splitlines())


def card_to_md(block: str) -> str:
    title_m = re.search(r'<div class="card-title">(.*?)</div>', block, re.DOTALL)
    title = inner_text(title_m.group(1)) if title_m else ""
    lines = [f"**{title}**"] if title else []
    pres = re.findall(r"<pre[\s\S]*?</pre>", block, re.IGNORECASE)
    body = block
    for pre in pres:
        body = body.replace(pre, "")
    if title_m:
        body = body.replace(title_m.group(0), "")
    text = md_inline(body)
    if text:
        lines.extend(["", text])
    for pre in pres:
        code = pre_to_code(pre)
        if code:
            lines.extend(["", split_mixed_examples(code)])
    return "\n".join(lines)


def factor_grid_to_md(block: str) -> str:
    cards = re.findall(
        r'<div class="factor-card[^"]*">([\s\S]*?)</div>',
        block,
    )
    lines: list[str] = []
    for card in cards:
        h4 = re.search(r"<h4>(.*?)</h4>", card, re.DOTALL)
        p = re.search(r"<p>(.*?)</p>", card, re.DOTALL)
        if h4:
            lines.append(f"### {inner_text(h4.group(1))}")
        if p:
            lines.append("")
            lines.append(md_inline(p.group(1)))
        lines.append("")
    return "\n".join(lines).strip()


def factor_grid_cards_to_md(block: str) -> str:
    """Performance section uses .card inside .factor-grid."""
    cards = re.findall(
        r'<div class="card"[^>]*>([\s\S]*?)</div>\s*(?=<div class="card"|</div>\s*</div>|$)',
        block,
    )
    lines: list[str] = []
    for card in cards:
        lines.append(card_to_md(f'<div class="card">{card}</div>'))
        lines.append("")
    return "\n".join(lines).strip()


def source_links_to_md(block: str) -> str:
    links = re.findall(
        r'<a class="source-link"[^>]+href="([^"]+)"[^>]*>(.*?)</a>',
        block,
        re.DOTALL,
    )
    return "\n".join(f"- [{inner_text(text)}]({url})" for url, text in links)


def parse_type_guide_section(section_html: str) -> str:
    """Convert one type-guide section body to markdown blocks."""
    placeholders: dict[str, str] = {}

    def _stash(key: str, content: str, html_block: str) -> str:
        placeholders[key] = content
        return f"\x00{key}\x00"

    grid_m = re.search(
        r'<div class="factor-grid">([\s\S]*)</div>\s*(?=<h2|</section>|$)',
        section_html,
    )
    if grid_m:
        section_html = section_html.replace(
            grid_m.group(0),
            _stash(
                f"GRID{len(placeholders)}",
                factor_grid_to_md(grid_m.group(0)),
                grid_m.group(0),
            ),
            1,
        )
    perf_m = re.search(
        r'<div class="factor-grid"[^>]*>((?:<div class="card[\s\S]*?</div>\s*)+)</div>',
        section_html,
    )
    if perf_m:
        section_html = section_html.replace(
            perf_m.group(0),
            _stash(
                f"PERF{len(placeholders)}",
                factor_grid_cards_to_md(perf_m.group(0)),
                perf_m.group(0),
            ),
            1,
        )

    lines: list[str] = []
    patterns = [
        ("h3", r'<h3[^>]*>.*?</h3>'),
        ("decision-tree", r'<div class="decision-tree">[\s\S]*?</div>\s*(?=<a class="source-link"|</section>|$)'),
        ("tbl-wrap", r'<div class="tbl-wrap">\s*<table>[\s\S]*?</table>\s*</div>'),
        ("callout", r'<div class="callout[^"]*">[\s\S]*?</div>\s*</div>'),
        ("card", r'<div class="card[^"]*">[\s\S]*?</div>\s*(?=<pre|<div class="|<h3|<p|<a class="source-link"|</section>|$)'),
        ("chart-box", r'<div class="chart-box"[^>]*>[\s\S]*?</div>\s*(?=\s*(?:</section>|$))'),
        ("pre", r"<pre[\s\S]*?</pre>"),
        ("p", r"<p>([\s\S]*?)</p>"),
        ("tag-list", r'<div class="tag-list"[\s\S]*?</div>'),
        ("source-links", r'(<a class="source-link"[\s\S]*?</a>(?:\s*<a class="source-link"[\s\S]*?</a>)*)'),
        ("placeholder", r"\x00(?:GRID|PERF)\d+\x00"),
        ("hr", r"<hr\s*/?>"),
    ]

    matches: list[tuple[int, str, str]] = []
    for kind, pat in patterns:
        for m in re.finditer(pat, section_html, re.IGNORECASE):
            matches.append((m.start(), kind, m.group(0)))

    matches.sort(key=lambda x: x[0])
    used: list[tuple[int, int]] = []
    blocks: list[tuple[str, str]] = []
    for start, kind, chunk in matches:
        end = start + len(chunk)
        if any(not (end <= s or start >= e) for s, e in used):
            continue
        used.append((start, end))
        blocks.append((kind, chunk))

    for kind, chunk in blocks:
        if kind == "placeholder":
            key = chunk.strip("\x00")
            if key in placeholders:
                lines.extend([placeholders[key], ""])
        elif kind == "h3":
            title = inner_text(re.search(r"<h3[^>]*>(.*?)</h3>", chunk, re.DOTALL).group(1))
            lines.extend(["", f"### {title}", ""])
        elif kind == "p":
            t = md_inline(chunk)
            if t:
                lines.extend([t, ""])
        elif kind == "decision-tree":
            dt = decision_tree_to_md(chunk)
            if dt:
                lines.extend([dt, ""])
        elif kind == "tbl-wrap":
            tbl = table_to_md(chunk)
            if tbl:
                lines.extend([tbl, ""])
        elif kind == "callout":
            lines.extend([callout_to_md(chunk), ""])
        elif kind == "card":
            c = card_to_md(chunk)
            if c:
                lines.extend([c, ""])
        elif kind == "chart-box":
            chart = chart_to_md(chunk)
            if chart:
                lines.extend([chart, ""])
        elif kind == "pre":
            code = pre_to_code(chunk)
            if code:
                lines.extend([split_mixed_examples(code), ""])
        elif kind == "tag-list":
            tags = re.findall(r'<span class="tag[^"]*">(.*?)</span>', chunk)
            if tags:
                lines.append("标签：" + " · ".join(inner_text(t) for t in tags))
                lines.append("")
        elif kind == "source-links":
            sl = source_links_to_md(chunk)
            if sl:
                lines.extend(["**参考链接**", "", sl, ""])
        elif kind == "hr":
            lines.extend(["---", ""])

    return "\n".join(lines).strip()


def type_guide_to_ch5_appendix() -> str:
    raw = TYPE_GUIDE_PATH.read_text(encoding="utf-8")
    sections_html = {
        m.group(1): m.group(2)
        for m in re.finditer(
            r'<section class="section" id="(s-[^"]+)">([\s\S]*?)</section>',
            raw,
        )
    }

    parts = [
        "---",
        "",
        "> 以下为 **C 语言数据类型选择完全指南** 扩展内容（5.3–5.18），参考 [cppreference — 整数类型](https://en.cppreference.com/w/c/types/integer) 与 [Barr Group — Fixed-Width Integers](https://barrgroup.com/embedded-systems/how-to/c-fixed-width-integers-c99)。",
        "",
    ]

    for sec_id, num, title in TYPE_GUIDE_SECTIONS:
        body = sections_html.get(sec_id, "")
        if not body:
            continue
        # Remove leading h2 (we replace with numbered heading)
        body = re.sub(r"<h2[^>]*>[\s\S]*?</h2>", "", body, count=1)
        body = re.sub(
            r'<div class="section-badge">[\s\S]*?</div>',
            "",
            body,
            count=1,
        )
        parts.extend([f"## {num} {title}", ""])
        content = parse_type_guide_section(body)
        if content:
            parts.append(content)
        parts.append("")

    return "\n".join(parts).strip() + "\n"


if __name__ == "__main__":
    print(type_guide_to_ch5_appendix())
