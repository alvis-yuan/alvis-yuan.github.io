#!/usr/bin/env python3
"""Convert C architecture/pattern HTML docs to c-programming-design skill markdown."""

from __future__ import annotations

import html
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESEARCH_HTML = ROOT / "program" / "c-programming-design-research.html"
PATTERN_HTML = ROOT / "program" / "c-design-pattern-guide.html"

PATTERN_SECTIONS: list[tuple[str, str, str]] = [
    ("overview", "设计模式概述"),
    ("categories", "23 种设计模式总览"),
    ("singleton", "单例模式（Singleton）"),
    ("factory", "工厂方法模式（Factory Method）"),
    ("abstract-factory", "抽象工厂模式（Abstract Factory）"),
    ("builder", "建造者模式（Builder）"),
    ("prototype", "原型模式（Prototype）"),
    ("adapter", "适配器模式（Adapter）"),
    ("bridge", "桥接模式（Bridge）"),
    ("composite", "组合模式（Composite）"),
    ("decorator", "装饰器模式（Decorator）"),
    ("facade", "外观模式（Facade）"),
    ("flyweight", "享元模式（Flyweight）"),
    ("proxy", "代理模式（Proxy）"),
    ("observer", "观察者模式（Observer）"),
    ("strategy", "策略模式（Strategy）"),
    ("command", "命令模式（Command）"),
    ("chain", "责任链模式（Chain of Responsibility）"),
    ("state", "状态模式（State）"),
    ("template", "模板方法模式（Template Method）"),
    ("iterator", "迭代器模式（Iterator）"),
    ("mediator", "中介者模式（Mediator）"),
    ("visitor", "访问者模式（Visitor）"),
    ("memento", "备忘录模式（Memento）"),
    ("interpreter", "解释器模式（Interpreter）"),
    ("decision-tree", "设计模式决策树"),
]

ARCH_TITLES: dict[str, str] = {
    "ch1": "单一职责与函数短小原则",
    "ch2": "分层架构与模块化设计",
    "ch3": "抽象接口与信息隐藏",
    "ch4": "集中式错误处理 (goto 清理模式)",
    "ch5": "数据驱动设计",
    "ch6": "简单控制流原则",
    "ch7": "防御性编程与断言设计",
    "ch8": "可移植性设计原则",
    "ch9": "内存所有权语义",
    "ch10": "事件驱动架构与单线程模型",
}


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
        r"<a[^>]+href=\"([^\"]+)\"[^>]*>(.*?)</a>",
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


def pre_to_code(pre_html: str) -> str:
    inner = pre_html
    if "<pre" in pre_html:
        m = re.search(r"<pre[^>]*>(.*?)</pre>", pre_html, re.DOTALL | re.IGNORECASE)
        inner = m.group(1) if m else pre_html
    inner = re.sub(r"</?code>", "", inner, flags=re.IGNORECASE)
    inner = re.sub(r"<br\s*/?>", "\n", inner, flags=re.IGNORECASE)
    inner = strip_spans(inner)
    return html.unescape(inner).strip()


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


def extract_div_block(body: str, class_pattern: str) -> tuple[str, str, str] | None:
    m = re.search(rf'<div class="{class_pattern}', body)
    if not m:
        return None
    start = m.start()
    depth = 0
    i = start
    while i < len(body):
        if body[i : i + 4] == "<div":
            depth += 1
            gt = body.find(">", i)
            if gt == -1:
                break
            i = gt + 1
        elif body[i : i + 6] == "</div>":
            depth -= 1
            i += 6
            if depth == 0:
                return body[:start], body[start:i], body[i:]
        else:
            i += 1
    return None


def stash_replace(body: str, class_pattern: str, converter, stash: dict[str, str], counter: list[int]) -> str:
    while True:
        found = extract_div_block(body, class_pattern)
        if not found:
            break
        before, block, after = found
        key = f"\x00STASH{counter[0]}\x00"
        stash[key] = converter(block)
        counter[0] += 1
        body = before + key + after
    return body


def svg_to_md(block: str) -> str:
    cap_m = re.search(r'class="diagram-caption">(.*?)</div>', block, re.DOTALL)
    cap_m2 = re.search(r'class="svg-title">(.*?)</div>', block, re.DOTALL)
    caption = inner_text(cap_m.group(1)) if cap_m else inner_text(cap_m2.group(1)) if cap_m2 else ""
    texts = re.findall(r"<text[^>]*>([^<]+)</text>", block)
    bullets: list[str] = []
    seen: set[str] = set()
    for t in texts:
        t = t.strip()
        if len(t) < 2 or t in seen:
            continue
        seen.add(t)
        bullets.append(f"- {t}")
    lines = []
    if caption:
        lines.append(f"**{caption}**")
    if bullets:
        lines.extend(["", *bullets[:40]])
    return "\n".join(lines)


def quote_block_to_md(block: str) -> str:
    cite_m = re.search(r"<cite>(.*?)</cite>", block, re.DOTALL)
    cite = inner_text(cite_m.group(1)) if cite_m else ""
    body = re.sub(r"<cite>.*?</cite>", "", block, flags=re.DOTALL)
    text = md_inline(re.sub(r'<div class="quote-block">|</div>', "", body))
    lines = ["> " + ln for ln in text.splitlines()]
    if cite:
        lines.append(f"> — {cite}")
    return "\n".join(lines)


def info_grid_to_md(block: str) -> str:
    cards = re.findall(r'<div class="info-card[^"]*">([\s\S]*?)</div>\s*</div>', block, re.DOTALL)
    parts: list[str] = []
    for card in cards:
        label_m = re.search(r'<div class="card-label">(.*?)</div>', card, re.DOTALL)
        label = inner_text(label_m.group(1)) if label_m else ""
        items = re.findall(r"<li>([\s\S]*?)</li>", card, re.IGNORECASE)
        if label:
            parts.append(f"**{label}**")
        for item in items:
            parts.append(f"- {md_inline(item)}")
        parts.append("")
    return "\n".join(parts).strip()


def code_compare_to_md(block: str) -> str:
    blocks = re.findall(
        r'<div class="code-block (bad|good)"[^>]*>([\s\S]*?)</div>\s*(?=<div class="code-block|$)',
        block,
    )
    parts: list[str] = []
    for kind, body in blocks[:2]:
        header_m = re.search(r'class="code-header[^"]*">(.*?)</div>', body, re.DOTALL)
        header = inner_text(header_m.group(1)) if header_m else ""
        code = pre_to_code(body)
        if kind == "bad":
            title = header or "反面示例"
            parts.append(f"**{title}**\n\n```c\n{code}\n```")
        else:
            title = header or "正面示例"
            parts.append(f"**{title}**\n\n```c\n{code}\n```")
    return "\n\n".join(parts)


def pattern_code_block_to_md(block: str) -> str:
    label_m = re.search(r'<div class="code-label">(.*?)</div>', block, re.DOTALL)
    label = inner_text(label_m.group(1)) if label_m else ""
    code = pre_to_code(block)
    if label:
        return f"**示例：{label}**\n\n```c\n{code}\n```"
    return f"```c\n{code}\n```"


def info_box_to_md(block: str) -> str:
    content_m = re.search(r'class="info-box-content">([\s\S]*?)</div>', block, re.DOTALL)
    if not content_m:
        return md_inline(block)
    return "> " + "\n> ".join(md_inline(content_m.group(1)).splitlines())


def card_to_md(block: str) -> str:
    title_m = re.search(r'class="card-title">(.*?)</div>', block, re.DOTALL)
    sub_m = re.search(r'class="card-subtitle">(.*?)</div>', block, re.DOTALL)
    title = inner_text(title_m.group(1)) if title_m else ""
    sub = inner_text(sub_m.group(1)) if sub_m else ""
    body = block
    for pat in [
        r'<div class="card-header">[\s\S]*?</div>',
        r'<div class="card-icon[^"]*">[\s\S]*?</div>',
        r'<div class="code-block">[\s\S]*?</div>\s*</div>',
        r'<a class="source-link"[^>]+>[\s\S]*?</a>',
    ]:
        body = re.sub(pat, "", body, flags=re.DOTALL)
    body = re.sub(r'<div class="card"[^>]*>|</div>', "", body)
    text = md_inline(body).strip()
    lines = [f"### {title}"]
    if sub:
        lines.append(f"*{sub}*")
    if text:
        lines.extend(["", text])
    link_m = re.search(
        r'<a class="source-link"[^>]+href="([^"]+)"[^>]*>(.*?)</a>', block, re.DOTALL
    )
    if link_m:
        lines.extend(["", f"- [{inner_text(link_m.group(2))}]({link_m.group(1)})"])
    code_m = re.search(r'<div class="code-block">([\s\S]*?)</div>\s*(?=</div>|$)', block)
    if code_m:
        lines.extend(["", pattern_code_block_to_md(code_m.group(0))])
    return "\n".join(lines)


def parse_body_to_md(body: str) -> str:
    stash: dict[str, str] = {}
    counter = [0]

    def stash_regex(pattern: str, converter) -> None:
        nonlocal body
        while True:
            m = re.search(pattern, body, re.DOTALL | re.IGNORECASE)
            if not m:
                break
            key = f"\x00STASH{counter[0]}\x00"
            stash[key] = converter(m.group(0))
            counter[0] += 1
            body = body[: m.start()] + key + body[m.end() :]

    body = stash_replace(body, "code-compare", code_compare_to_md, stash, counter)
    body = stash_replace(body, "diagram-wrap", svg_to_md, stash, counter)
    body = stash_replace(body, "svg-container", svg_to_md, stash, counter)
    body = stash_replace(body, "info-grid", info_grid_to_md, stash, counter)
    body = stash_replace(body, "quote-block", quote_block_to_md, stash, counter)
    body = stash_replace(body, "grid-2", lambda b: "\n\n".join(
        info_box_to_md(x) for x in re.findall(r'<div class="info-box[^"]*">[\s\S]*?</div>\s*</div>', b)
    ), stash, counter)
    body = stash_replace(body, "info-box", info_box_to_md, stash, counter)
    body = stash_replace(body, "code-block", pattern_code_block_to_md, stash, counter)
    body = stash_replace(body, "card", card_to_md, stash, counter)
    stash_regex(r"<table>[\s\S]*?</table>", table_to_md)
    stash_regex(r'<div class="tbl-wrap">\s*<table>[\s\S]*?</table>\s*</div>', table_to_md)

    # Remove standalone source badges/links already in header
    body = re.sub(
        r'(?:^|\n)\s*<a class="source-badge"[^>]+>.*?</a>\s*(?=\n)',
        "\n",
        body,
    )
    body = re.sub(
        r'(?:^|\n)\s*<a class="source-link"[^>]+>.*?</a>\s*(?=\n)',
        "\n",
        body,
    )

    parts: list[str] = []
    tokens = re.split(
        r"(<div class=\"section-title\">[\s\S]*?</div>"
        r"|<h2>[\s\S]*?</h2>"
        r"|<h3>[\s\S]*?</h3>"
        r"|<p class=\"desc\">[\s\S]*?</p>"
        r"|<p>[\s\S]*?</p>"
        r"|<ul>[\s\S]*?</ul>"
        r"|<ol>[\s\S]*?</ol>"
        r"|\x00STASH\d+\x00)",
        body,
        flags=re.IGNORECASE,
    )

    for token in tokens:
        token = token.strip()
        if not token:
            continue
        if token.startswith("\x00STASH"):
            parts.append(stash[token])
            continue
        if "section-title" in token[:30]:
            parts.append(f"### {inner_text(token)}")
            continue
        if token.lower().startswith("<h2"):
            parts.append(f"### {inner_text(token)}")
            continue
        if token.lower().startswith("<h3"):
            parts.append(f"#### {inner_text(token)}")
            continue
        if token.lower().startswith("<ul") or token.lower().startswith("<ol"):
            for item in re.findall(r"<li>([\s\S]*?)</li>", token, re.IGNORECASE):
                parts.append(f"- {md_inline(item)}")
            continue
        if token.lower().startswith("<p"):
            text = md_inline(token)
            if text:
                parts.append(text)

    result = "\n\n".join(parts)
    for key, val in stash.items():
        if key in result:
            result = result.replace(key, val)
    return result


def parse_research_chapter(section_html: str, ch_id: str, title: str) -> str:
    subtitle_m = re.search(r'<div class="subtitle">(.*?)</div>', section_html, re.DOTALL)
    subtitle = inner_text(subtitle_m.group(1)) if subtitle_m else ""
    badge_m = re.search(r'<a class="source-badge"[^>]+href="([^"]+)"[^>]*>(.*?)</a>', section_html, re.DOTALL)
    refs: list[str] = []
    if badge_m:
        refs.append(f"- [{inner_text(badge_m.group(2))}]({badge_m.group(1)})")

    header = f"# {title}\n\n"
    if subtitle:
        header += f"*{subtitle}*\n\n"
    if refs:
        header += "参考来源：\n" + "\n".join(refs) + "\n\n"

    body = section_html
    body = re.sub(r'<div class="chapter-header">[\s\S]*?</div>', "", body, count=1)
    if ch_id == "intro":
        body = re.sub(r'<div class="hero">[\s\S]*?</div>', "", body, count=1)
    return header + parse_body_to_md(body)


def parse_pattern_section(section_html: str, title: str) -> str:
    hero_m = re.search(r'<div class="section-hero">([\s\S]*?)</div>', section_html, re.DOTALL)
    hero_p = ""
    if hero_m:
        p_m = re.search(r"<p>([\s\S]*?)</p>", hero_m.group(1), re.DOTALL)
        if p_m:
            hero_p = md_inline(p_m.group(1))
    refs = re.findall(
        r'<a class="source-link"[^>]+href="([^"]+)"[^>]*>(.*?)</a>',
        section_html,
        re.DOTALL,
    )
    header = f"# {title}\n\n"
    if hero_p:
        header += f"{hero_p}\n\n"
    if refs:
        header += "参考来源：\n"
        header += "\n".join(f"- [{inner_text(t)}]({u})" for u, t in refs)
        header += "\n\n"
    body = re.sub(r'<div class="section-hero">[\s\S]*?</div>', "", section_html, count=1)
    return header + parse_body_to_md(body)


def ch0_overview_md() -> str:
    return """# C 语言架构设计总览

*架构与模式*

参考来源：[Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html)、[SQLite Architecture](https://sqlite.org/arch.html)、[GoF Design Patterns](https://en.wikipedia.org/wiki/Design_Patterns)

> 本 skill 分两部分：**第 1–10 章**为架构与设计原则（函数职责、分层、错误处理、事件驱动等）；**第 11–36 章**为 GoF 设计模式在 C 中的落地与选型。编写或审查 C 模块/系统架构时，**先读相关 reference 章节**。

## 0.1 使用场景与选型

### [建议] 先确定架构层问题，再考虑具体设计模式

**两层决策：**

| 层次 | 典型问题 | 阅读章节 |
| --- | --- | --- |
| **架构原则** | 模块如何划分？错误如何传播？谁拥有内存？控制流是否过深？ | 第 1–10 章 |
| **设计模式** | 创建对象、组合结构、行为协作是否有成熟套路？ | 第 11–36 章 |

**优先顺序：**

1. **模块边界与接口**（分层、信息隐藏、所有权）未理清时，**不要**先套模式。
2. 架构原则满足后，按 [第 36 章 设计模式决策树](ch36.md) 选择模式。
3. C 中模式通常用 **函数指针 / 结构体 + 回调 / 表驱动** 实现，避免过度 OO 化。

**反面示例（未做架构直接套模式 — 过度设计）**

```c
/* ❌ 仅解析一行配置，却引入 Singleton + Factory + Observer */
typedef struct config_singleton config_singleton_t;
config_singleton_t *config_singleton_get(void);
/* ...200 行模式脚手架... */
int load_one_key(const char *line);  /* 实际需求 */
```

**正面示例（先架构后模式）**

```c
/* ✅ 小模块：分层 + 单一职责（第 1、2 章）*/
int config_parse_line(const char *line, struct config_entry *out);
int config_store_push(struct config_store *store, const struct config_entry *e);

/* ✅ 多种后端需运行时切换时，再用 Strategy（第 26 章）*/
struct io_backend {
    ssize_t (*read)(void *ctx, void *buf, size_t n);
    ssize_t (*write)(void *ctx, const void *buf, size_t n);
};
```

## 0.3 架构原则体系

**图1：C 语言设计原则总体架构体系**

- C 语言设计原则体系
- **架构层原则**：单一职责、分层模块化、抽象接口、数据驱动、事件驱动
- **控制流层原则**：简单控制流、集中错误处理、防御性编程
- **资源管理层原则**：内存所有权、可移植性设计、信息隐藏

---

"""


def references_md() -> str:
    return """# 权威参考来源

*附录*

## 架构与设计原则

- [Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- [SQLite Architecture](https://sqlite.org/arch.html)
- [GNU Coding Standards](https://www.gnu.org/prep/standards/standards.html)
- [NASA JPL Power of Ten Rules](https://spinroot.com/p10/)
- [SEI CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/spaces/c/pages/87152064/Introduction)

## 设计模式与开源实践

- [Design Patterns: Elements of Reusable OO Software (GoF)](https://en.wikipedia.org/wiki/Design_Patterns)
- [Redis Internals](https://redis.io/docs/latest/operate/oss_and_stack/reference/internals/)
- [libevent](https://libevent.org/)
- [Nginx HTTP Request Processing](https://nginx.org/en/docs/http/request_processing.html)

## 相关 Cursor Skill

- C 语言编程规范（编码标准、类型、错误处理、RAII 等）— 与本 skill 互补：本 skill 管**架构与模式**，规范 skill 管**编码细节**。
"""


def build_all_chapters() -> list[tuple[str, str, str]]:
    """Return list of (file_stem, index_title, markdown)."""
    research = RESEARCH_HTML.read_text(encoding="utf-8")
    pattern = PATTERN_HTML.read_text(encoding="utf-8")

    chapters: list[tuple[str, str, str]] = []
    intro_m = re.search(
        r'<section id="intro" class="chapter"[\s\S]*?</section>', research, re.DOTALL
    )
    ch0_md = ch0_overview_md()
    if intro_m:
        src_tags = re.findall(
            r'<a class="src-tag"[^>]+href="([^"]+)"[^>]*>(.*?)</a>',
            intro_m.group(0),
            re.DOTALL,
        )
        if src_tags:
            extra = "\n".join(f"- [{inner_text(t)}]({u})" for u, t in src_tags)
            ch0_md = ch0_md.rstrip() + "\n\n## 0.4 权威来源标签\n\n" + extra + "\n"
    chapters.append(("ch0", "总览与选型", ch0_md))

    for ch_id in [f"ch{i}" for i in range(1, 11)]:
        m = re.search(
            rf'<section id="{ch_id}" class="chapter"[\s\S]*?</section>',
            research,
            re.DOTALL,
        )
        if not m:
            continue
        title = ARCH_TITLES.get(ch_id, ch_id)
        md = parse_research_chapter(m.group(0), ch_id, title)
        num = ch_id.replace("ch", "")
        chapters.append((f"ch{num}", f"第{num}章 {title}", md))

    ch_num = 11
    for sec_id, title in PATTERN_SECTIONS:
        m = re.search(
            rf'<div class="section[^"]*" id="sec-{re.escape(sec_id)}"[\s\S]*?(?=<div class="section[^"]*" id="sec-|<footer|$)',
            pattern,
            re.DOTALL,
        )
        if not m:
            continue
        md = parse_pattern_section(m.group(0), title)
        chapters.append((f"ch{ch_num}", f"第{ch_num}章 {title}", md))
        ch_num += 1

    chapters.append(("ch37", "权威参考来源", references_md()))
    return chapters
