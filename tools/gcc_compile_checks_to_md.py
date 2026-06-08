#!/usr/bin/env python3
"""Convert program/gcc-compile-time-checks.html to markdown sections for ch23."""

from __future__ import annotations

import html
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HTML_PATH = ROOT / "program" / "gcc-compile-time-checks.html"


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
        r'<a[^>]+class="src-link"[^>]+href="([^"]+)"[^>]*>(.*?)</a>',
        lambda m: f"[{inner_text(m.group(2))}]({m.group(1)})",
        fragment,
        flags=re.DOTALL,
    )
    fragment = re.sub(
        r'<a[^>]+href="([^"]+)"[^>]*>(.*?)</a>',
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


def pre_to_code(pre_html: str) -> tuple[str, str]:
    lang_m = re.search(r'data-lang="([^"]+)"', pre_html, re.IGNORECASE)
    lang = (lang_m.group(1).lower() if lang_m else "c").replace("shell", "bash")
    m = re.search(r"<pre[^>]*>(.*?)</pre>", pre_html, re.DOTALL | re.IGNORECASE)
    if not m:
        return lang, ""
    inner = m.group(1)
    inner = re.sub(r"</?code>", "", inner, flags=re.IGNORECASE)
    inner = re.sub(r"<br\s*/?>", "\n", inner, flags=re.IGNORECASE)
    inner = strip_spans(inner)
    return lang, html.unescape(inner).strip()


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


def split_mixed_examples(code: str, lang: str = "c") -> str:
    fence = lang if lang != "bash" else "bash"

    marker = re.search(r"/\*\s*[✓✅]", code)
    if marker:
        before = code[: marker.start()].rstrip()
        chunks = re.split(r"\n\s*\n", before)
        if len(chunks) >= 2 and chunks[-1].strip():
            bad_part = "\n\n".join(chunks[:-1]).strip()
            good_part = chunks[-1].strip() + "\n" + code[marker.start() :].strip()
            return (
                f"**反面示例**\n\n```{fence}\n{bad_part}\n```\n\n"
                f"**正面示例**\n\n```{fence}\n{good_part}\n```"
            )

    if re.search(r"warning:", code, re.IGNORECASE) and not re.search(
        r"/\*\s*[✓✅]", code
    ):
        return f"**反面示例（编译器将发出警告/错误）**\n\n```{fence}\n{code}\n```"

    if "❌" in code and "✅" in code:
        parts = re.split(r"(?=/\* ✅|/\* ❌)", code)
        sections: list[str] = []
        for part in parts:
            part = part.strip()
            if not part:
                continue
            if part.startswith("/* ❌") or "❌" in part[:30]:
                sections.append(f"**反面示例**\n\n```{fence}\n{part}\n```")
            elif part.startswith("/* ✅") or "✅" in part[:30]:
                sections.append(f"**正面示例**\n\n```{fence}\n{part}\n```")
            else:
                sections.append(f"```{fence}\n{part}\n```")
        return "\n\n".join(sections)

    return f"```{fence}\n{code}\n```"


def diagram_to_md(diagram_html: str) -> str:
    texts = re.findall(r"<text[^>]*>([^<]+)</text>", diagram_html)
    bullets: list[str] = []
    seen: set[str] = set()
    for t in texts:
        t = t.strip()
        if len(t) < 2 or t in seen:
            continue
        seen.add(t)
        bullets.append(f"- {t}")
    if not bullets:
        return ""
    return "**示意图要点**\n\n" + "\n".join(bullets[:50])


def box_to_md(box_html: str) -> str:
    label_m = re.search(r'<div class="box-label">(.*?)</div>', box_html, re.DOTALL)
    label = inner_text(label_m.group(1)) if label_m else ""
    body = re.sub(r'<div class="box-label">.*?</div>', "", box_html, flags=re.DOTALL)
    body = re.sub(r'^<div class="box[^"]*">', "", body.strip())
    body = re.sub(r"</div>\s*$", "", body)

    ul_m = re.search(r"<ul>([\s\S]*?)</ul>", body, re.IGNORECASE)
    if ul_m:
        items = re.findall(r"<li>([\s\S]*?)</li>", ul_m.group(1), re.IGNORECASE)
        lines = [f"> **{label}**", ">"] if label else [">"]
        for item in items:
            lines.append(f"> - {md_inline(item)}")
        return "\n".join(lines)

    p_m = re.search(r"<p>([\s\S]*?)</p>", body, re.IGNORECASE)
    if p_m:
        text = md_inline(p_m.group(0))
        if label:
            return f"> **{label}**\n>\n> " + "\n> ".join(text.splitlines())
        return "> " + "\n> ".join(text.splitlines())

    text = md_inline(body)
    if label:
        return f"> **{label}**\n>\n> " + "\n> ".join(text.splitlines())
    return "> " + "\n> ".join(text.splitlines())


def summary_to_md(summary_html: str) -> str:
    h4_m = re.search(r"<h4>(.*?)</h4>", summary_html, re.DOTALL)
    title = inner_text(h4_m.group(1)) if h4_m else "小结"
    ul_m = re.search(r"<ul>([\s\S]*?)</ul>", summary_html, re.IGNORECASE)
    items = re.findall(r"<li>([\s\S]*?)</li>", ul_m.group(1) if ul_m else "", re.IGNORECASE)
    lines = [f"> **{title}**", ">"]
    for item in items:
        lines.append(f"> - {md_inline(item)}")
    return "\n".join(lines)


def blockquote_to_md(bq_html: str) -> str:
    text = md_inline(bq_html)
    return "> " + "\n> ".join(text.splitlines())


def src_links_to_md(section_html: str) -> str:
    links = re.findall(
        r'<a class="src-link"[^>]+href="([^"]+)"[^>]*>(.*?)</a>',
        section_html,
        re.DOTALL,
    )
    if not links:
        return ""
    return "\n".join(f"- [{inner_text(text)}]({href})" for href, text in links)


def extract_div_block(body: str, class_prefix: str) -> tuple[str, str, str] | None:
    """Extract first <div class="PREFIX...">...</div> block. Returns (before, block, after)."""
    m = re.search(rf'<div class="{class_prefix}[^"]*">', body)
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


def stash_replace_div(body: str, class_prefix: str, converter, stash: dict[str, str], counter: list[int]) -> str:
    while True:
        found = extract_div_block(body, class_prefix)
        if not found:
            break
        before, block, after = found
        key = f"\x00STASH{counter[0]}\x00"
        stash[key] = converter(block)
        counter[0] += 1
        body = before + key + after
    return body


def _pre_block_to_md(pre_html: str) -> str:
    lang, code = pre_to_code(pre_html)
    return split_mixed_examples(code, lang)


def parse_section_content(section_html: str) -> str:
    """Parse chapter body (after title) into markdown."""
    body = re.sub(r'<div class="chapter-num">.*?</div>', "", section_html, flags=re.DOTALL)
    body = re.sub(r'<h2 class="chapter-title">.*?</h2>', "", body, flags=re.DOTALL)

    src_links = src_links_to_md(body)
    # Remove standalone block-level source links; keep inline links inside <p>
    body = re.sub(
        r"(?:^|\n)\s*<a class=\"src-link\"[^>]+href=\"[^\"]+\"[^>]*>.*?</a>\s*(?=\n)",
        "\n",
        body,
    )

    stash: dict[str, str] = {}
    counter = [0]

    def stash_replace(pattern: str, converter) -> None:
        nonlocal body

        while True:
            m = re.search(pattern, body, re.DOTALL | re.IGNORECASE)
            if not m:
                break
            key = f"\x00STASH{counter[0]}\x00"
            stash[key] = converter(m.group(0))
            counter[0] += 1
            body = body[: m.start()] + key + body[m.end() :]

    stash_replace(r'<div class="tbl-wrap">\s*<table>[\s\S]*?</table>\s*</div>', table_to_md)
    stash_replace(r"<table>[\s\S]*?</table>", table_to_md)
    stash_replace(r'<div class="diagram-wrap">[\s\S]*?</div>', diagram_to_md)
    body = stash_replace_div(body, "summary", summary_to_md, stash, counter)
    body = stash_replace_div(body, "box box-", box_to_md, stash, counter)
    stash_replace(r"<blockquote[^>]*>[\s\S]*?</blockquote>", blockquote_to_md)
    stash_replace(r"<pre[^>]*>[\s\S]*?</pre>", _pre_block_to_md)

    parts: list[str] = []
    if src_links:
        parts.append(src_links)

    tokens = re.split(
        r"(<h3>[\s\S]*?</h3>|<h4>[\s\S]*?</h4>|<p>[\s\S]*?</p>|<ul>[\s\S]*?</ul>|<ol>[\s\S]*?</ol>|\x00STASH\d+\x00)",
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
        if token.lower().startswith("<h3"):
            parts.append(f"### {inner_text(token)}")
            continue
        if token.lower().startswith("<h4"):
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

    return "\n\n".join(parts)


def extract_chapters(html_text: str) -> list[tuple[str, str, str]]:
    """Return (id, title, body_html) for each chapter section."""
    sections = re.findall(
        r'<section class="chapter" id="(ch\d+)"[^>]*>([\s\S]*?)</section>',
        html_text,
        re.IGNORECASE,
    )
    result: list[tuple[str, str, str]] = []
    for ch_id, content in sections:
        title_m = re.search(r'<h2 class="chapter-title">(.*?)</h2>', content, re.DOTALL)
        title = inner_text(title_m.group(1)) if title_m else ch_id
        result.append((ch_id, title, content))
    return result


def gcc_checks_body_md(start_section: int = 2) -> str:
    """Convert HTML chapters to markdown sections numbered 23.x."""
    html_text = HTML_PATH.read_text(encoding="utf-8")
    chapters = extract_chapters(html_text)
    parts: list[str] = []
    sec_num = start_section
    for _ch_id, title, content in chapters:
        body = parse_section_content(content)
        parts.append(f"## 23.{sec_num} {title}\n\n{body}")
        sec_num += 1
    return "\n\n---\n\n".join(parts)


if __name__ == "__main__":
    print(gcc_checks_body_md())
