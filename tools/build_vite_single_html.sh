#!/usr/bin/env bash
#
# build_vite_single_html.sh — 将 Vite 项目编译为「单文件、无外部依赖」的 HTML
#
# 方法概要（与 network_packet_loss_guide_no_single.zip 示例一致）：
#   1. 使用 Vite 管理前端（本示例为 React + TypeScript + Tailwind 4）
#   2. 在 devDependencies 中加入 vite-plugin-singlefile
#   3. vite.config.ts 的 plugins 中启用：viteSingleFile()
#   4. 执行 npm run build → dist/index.html 内联全部 JS/CSS，无独立 chunk
#   5. 正文勿引用 CDN、/assets/、arena 等外部渲染资源；样式脚本均走打包
#
# 示例项目要点：
#   - package.json: "build": "vite build"
#   - vite.config.ts: plugins: [react(), tailwindcss(), viteSingleFile()]
#   - 长文档可放在 src/App.tsx 的模板字符串中，由 iframe srcDoc 展示（可选）
#
# 用法：
#   ./tools/build_vite_single_html.sh /path/to/vite-project [输出.html]
#   ./tools/build_vite_single_html.sh --zip project.zip [输出.html]
#   ./tools/build_vite_single_html.sh --zip network_packet_loss_guide_no_single.zip protocol/network_packet_loss_guide.html
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="dir"
PROJECT_DIR=""
ZIP_FILE=""
OUTPUT=""

log() { printf '[build_vite_single_html] %s\n' "$*"; }
die() { printf '[build_vite_single_html] 错误: %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<'EOF'
用法:
  build_vite_single_html.sh <vite项目目录> [输出.html]
  build_vite_single_html.sh --zip <项目.zip> [输出.html]

说明:
  - 需在项目目录执行 npm ci 或 npm install 后 vite build
  - 项目须已配置 vite-plugin-singlefile（见示例 zip）
  - 未指定输出时：复制 dist/index.html 到项目上级目录，文件名为 <目录名>.html

示例:
  ./tools/build_vite_single_html.sh /tmp/network_packet_loss_build
  ./tools/build_vite_single_html.sh --zip network_packet_loss_guide_no_single.zip \
      protocol/network_packet_loss_guide.html
EOF
}

wsl_node_install_hint() {
  cat <<'EOF' >&2
WSL 下请安装「Linux 版」Node.js（不要用 Windows 的 npm 跑 Linux 路径，否则会报 UNC/路径错误）：

  方式 A — nvm（推荐，用户目录安装）:
    curl -fsSL https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.3/install.sh | bash
    # 重新打开终端，或执行:
    source ~/.nvm/nvm.sh
    nvm install 22
    nvm use 22

  方式 B — NodeSource（系统级 apt）:
    curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash -
    sudo apt-get install -y nodejs

安装后验证:
  which node npm          # 应在 /home/... 或 /usr/bin，而不是 /mnt/c/...
  node -v && npm -v

若已用 nvm，请把下面两行写入 ~/.bashrc:
  export NVM_DIR="$HOME/.nvm"
  [ -s "$NVM_DIR/nvm.sh" ] && . "$NVM_DIR/nvm.sh"
EOF
}

setup_node_env() {
  # 非交互 shell 常未加载 nvm/fnm，构建前尝试启用
  if [[ -s "${HOME}/.nvm/nvm.sh" ]]; then
    # shellcheck disable=SC1090
    source "${HOME}/.nvm/nvm.sh"
  fi
  if command -v fnm >/dev/null 2>&1; then
    eval "$(fnm env --shell bash 2>/dev/null)" || true
  fi
}

require_node_toolchain() {
  setup_node_env

  local node_path npm_path
  node_path="$(command -v node 2>/dev/null || true)"
  npm_path="$(command -v npm 2>/dev/null || true)"

  if [[ -z "${node_path}" || -z "${npm_path}" ]]; then
    wsl_node_install_hint
    die "未找到 node 和/或 npm"
  fi

  # Windows 侧 node/npm 在 WSL 中易导致 UNC 路径错误，要求使用 Linux 安装
  if [[ "${node_path}" == /mnt/* ]] || [[ "${npm_path}" == /mnt/* ]]; then
    wsl_node_install_hint
    die "当前使用的是 Windows 路径下的 node/npm，无法在 WSL 中可靠构建"
  fi

  log "node: ${node_path} ($(node -v 2>/dev/null || echo '?'))"
  log "npm:  ${npm_path} ($(npm -v 2>/dev/null || echo '?'))"
}

default_output_for_dir() {
  local dir="$1"
  local name
  name="$(basename "${dir%/}")"
  name="${name%_no_single}"
  printf '%s/%s.html' "$(dirname "${dir}")" "${name}"
}

default_output_for_zip() {
  local zip="$1"
  local base
  base="$(basename "${zip}" .zip)"
  base="${base%_no_single}"
  printf '%s/%s.html' "$(dirname "${zip}")" "${base}"
}

check_singlefile_plugin() {
  local dir="$1"
  if ! grep -q 'vite-plugin-singlefile' "${dir}/package.json" 2>/dev/null; then
    die "package.json 中未找到 vite-plugin-singlefile，请参考示例 zip 添加依赖"
  fi
  if ! grep -q 'viteSingleFile\|vite-plugin-singlefile' "${dir}/vite.config.ts" 2>/dev/null; then
    die "vite.config.ts 中未启用 viteSingleFile()，请参考示例 zip 配置 plugins"
  fi
}

run_build() {
  local dir="$1"
  cd "${dir}"

  if [[ -f package-lock.json ]]; then
    log "npm ci ..."
    npm ci
  else
    log "npm install ..."
    npm install
  fi

  log "npm run build ..."
  npm run build

  [[ -f dist/index.html ]] || die "构建失败：未生成 dist/index.html"
}

resolve_output_path() {
  local out="$1"
  if [[ "${out}" != /* ]]; then
    out="${ORIGINAL_PWD}/${out}"
  fi
  printf '%s' "${out}"
}

copy_output() {
  local dist_html="$1"
  local out
  out="$(resolve_output_path "$2")"
  mkdir -p "$(dirname "${out}")"
  cp -f "${dist_html}" "${out}"
  local size
  size="$(wc -c < "${out}" | tr -d ' ')"
  log "已生成单文件 HTML: ${out} (${size} 字节)"
}

parse_args() {
  if [[ $# -lt 1 ]]; then
    usage
    exit 1
  fi
  case "$1" in
    -h|--help|help)
      usage
      exit 0
      ;;
    --zip)
      MODE="zip"
      [[ $# -ge 2 ]] || die "--zip 需要指定 zip 文件路径"
      ZIP_FILE="$2"
      OUTPUT="${3:-}"
      ;;
    *)
      MODE="dir"
      PROJECT_DIR="$1"
      OUTPUT="${2:-}"
      ;;
  esac
}

main() {
  parse_args "$@"

  # run_build 会 cd 到项目目录；输出路径必须相对「调用脚本时的工作目录」解析
  ORIGINAL_PWD="$(pwd)"

  require_node_toolchain
  command -v unzip >/dev/null 2>&1 || die "未找到 unzip，请执行: sudo apt-get install -y unzip"

  local work_dir=""
  local cleanup="0"

  if [[ "${MODE}" == "zip" ]]; then
    [[ "${ZIP_FILE}" != /* ]] && ZIP_FILE="${ORIGINAL_PWD}/${ZIP_FILE}"
    [[ -f "${ZIP_FILE}" ]] || die "zip 不存在: ${ZIP_FILE}"
    work_dir="$(mktemp -d "${TMPDIR:-/tmp}/vite-single-build.XXXXXX")"
    cleanup="1"
    log "解压 ${ZIP_FILE} → ${work_dir}"
    unzip -q -o "${ZIP_FILE}" -d "${work_dir}"
    # zip 根目录可能有一层子目录，也可能直接是 package.json
    if [[ ! -f "${work_dir}/package.json" ]]; then
      local inner
      inner="$(find "${work_dir}" -mindepth 1 -maxdepth 2 -name package.json | head -1)"
      [[ -n "${inner}" ]] || die "zip 内未找到 package.json"
      work_dir="$(dirname "${inner}")"
    fi
    PROJECT_DIR="${work_dir}"
    [[ -n "${OUTPUT}" ]] || OUTPUT="$(default_output_for_zip "${ZIP_FILE}")"
  else
    [[ "${PROJECT_DIR}" != /* ]] && PROJECT_DIR="${ORIGINAL_PWD}/${PROJECT_DIR}"
    [[ -d "${PROJECT_DIR}" ]] || die "目录不存在: ${PROJECT_DIR}"
    [[ -f "${PROJECT_DIR}/package.json" ]] || die "不是有效的 Vite 项目（缺少 package.json）"
    [[ -n "${OUTPUT}" ]] || OUTPUT="$(default_output_for_dir "${PROJECT_DIR}")"
  fi
  PROJECT_DIR="$(cd "${PROJECT_DIR}" && pwd)"

  check_singlefile_plugin "${PROJECT_DIR}"
  run_build "${PROJECT_DIR}"
  copy_output "${PROJECT_DIR}/dist/index.html" "${OUTPUT}"

  if [[ "${cleanup}" == "1" ]]; then
    log "清理临时目录 ${work_dir}"
    rm -rf "${work_dir}"
  fi

  log "完成。输出目录: $(dirname "$(resolve_output_path "${OUTPUT}")")"
  log "可用浏览器直接打开（file:// 或静态服务器均可）。"
}

main "$@"
