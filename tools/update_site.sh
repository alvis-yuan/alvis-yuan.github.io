#!/usr/bin/env bash
#
# 一键更新站点索引并清理 HTML 失效链接。
# 实际逻辑见 tools/update_indices.py
#
# 用法:
#   ./tools/update_site.sh              # 更新全部 index.html + 修复/删除坏链
#   ./tools/update_site.sh --dry-run    # 仅预览，不写文件
#   ./tools/update_site.sh --indices-only   # 只更新索引，不改正文
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

if ! command -v python3 >/dev/null 2>&1; then
  echo "[update_site] 错误: 未找到 python3" >&2
  exit 1
fi

echo "[update_site] 工作目录: ${ROOT}"
python3 tools/update_indices.py "$@"
