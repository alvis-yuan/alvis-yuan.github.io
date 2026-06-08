#!/usr/bin/env python3
"""Build c-programming-design skill from architecture/pattern HTML docs."""

from __future__ import annotations

from pathlib import Path

from c_design_skill_md import build_all_chapters

ROOT = Path(__file__).resolve().parents[1]
PROJECT_SKILL_DIR = ROOT / ".cursor/skills/c-programming-design"
GLOBAL_SKILL_DIR = Path.home() / ".cursor/skills/c-programming-design"


def write_skill(skill_dir: Path) -> None:
    ref_dir = skill_dir / "reference"
    ref_dir.mkdir(parents=True, exist_ok=True)

    chapters = build_all_chapters()
    stems = {stem for stem, _, _ in chapters}
    for old in ref_dir.glob("*.md"):
        if old.stem not in stems:
            old.unlink()

    index_lines: list[str] = []
    for stem, index_title, md in chapters:
        (ref_dir / f"{stem}.md").write_text(md, encoding="utf-8")
        index_lines.append(f"- [{index_title}](reference/{stem}.md)")

    skill_md = f"""---
name: c-programming-design
description: Guides C language software architecture and GoF design patterns in user-space/system code — layered modules, error handling, ownership, event-driven design, and pattern selection (Singleton, Factory, Observer, Strategy, etc.). Use when designing C modules, choosing architecture vs patterns, reviewing structure, or when the user mentions C architecture, design patterns, layering, or module boundaries.
---

# C 语言架构设计

自包含的 C 语言**架构原则**（第 0–10 章）与 **GoF 设计模式**（第 11–36 章）指南。参考 Linux 内核、SQLite、Redis 等实践。编写或审查 C 架构时，**必须先阅读相关 reference 章节**。

## 使用方式

1. **先架构、后模式**：模块划分、接口、所有权、错误策略未定时，读第 1–10 章；具体协作/创建/结构问题再读第 11–36 章。
2. 模式选型使用 [第 36 章 设计模式决策树](reference/ch36.md)。
3. 编码细节（命名、类型、RAII、编译期检查）见 **c-programming-spec** skill；本 skill 聚焦**结构与模式**。
4. 每条含 **文字说明** 与 **正/反面代码示例**（源文档提供时保留）。

## 章节索引

{chr(10).join(index_lines)}

## 快速检查清单

- [ ] 模块职责单一、函数短小（第 1 章）
- [ ] 分层清晰，层间仅通过头文件接口通信（第 2 章）
- [ ] 对外暴露 opaque 类型 / 最小 API（第 3 章）
- [ ] 多 exit 路径有集中错误处理或 RAII（第 4 章；编码见 c-programming-spec）
- [ ] 表驱动 / 数据与逻辑分离（第 5 章）
- [ ] 控制流扁平，圈复杂度可控（第 6 章）
- [ ] 内存所有权在 API 文档与实现中一致（第 9 章）
- [ ] 需要模式时对照决策树，避免过度设计（第 36 章）

## 权威参考

完整外部链接见 [reference/ch37.md](reference/ch37.md)。
"""

    (skill_dir / "SKILL.md").write_text(skill_md, encoding="utf-8")
    print(f"Wrote {len(chapters)} chapters to {skill_dir}")


def main() -> None:
    write_skill(PROJECT_SKILL_DIR)
    write_skill(GLOBAL_SKILL_DIR)


if __name__ == "__main__":
    main()
