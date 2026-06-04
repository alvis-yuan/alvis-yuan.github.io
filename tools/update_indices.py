#!/usr/bin/env python3
"""
更新站点内所有目录的 index.html，并清理 HTML 中失效的本地引用链接。

功能：
  1. 为每个含 HTML 的目录（及上级目录）生成/更新 index.html
     - 模块说明、功能模块索引、目录索引
     2. 子目录链接指向 xxx/index.html
  3. 扫描 HTML 中的 href/src：
     - 唯一匹配 → 修正为正确相对路径
     - 无法解析 → 删除引用（<a> 保留文字；link/script/img 移除标签）
  4. 跳过构建产物、模板噪声链接（如 minified JS 中的 +n+）

用法:
  python3 tools/update_indices.py
  python3 tools/update_indices.py --dry-run
  ./tools/update_site.sh

可选维护: 编辑本文件中的 DIR_INTROS、MODULE_GROUPS；未配置分组的目录将自动按文件名归类。
"""
from __future__ import annotations

import argparse
import fnmatch
import os
import re
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent.parent

HTML_EXTS = {'.html', '.htm'}

# 不参与索引/链接扫描的路径片段
EXCLUDE_DIR_NAMES = {
    '__pycache__', 'node_modules', '.git', 'network_packet_loss_guide_src',
    'assets', 'tmp', 'dist',
}
# 不在目录索引中列出的子目录（如仅含维护脚本）
LISTING_SKIP_DIRS = {'tools'}
EXCLUDE_FILE_NAMES = {'.DS_Store'}

TITLE_RE = re.compile(r'<title[^>]*>(.*?)</title>', re.IGNORECASE | re.DOTALL)
LINK_RE = re.compile(r'(href|src)=("|\')(?P<url>.*?)(\2)', re.IGNORECASE)
A_TAG_RE = re.compile(
    r'<a(\s+[^>]*?\bhref\s*=\s*["\'])(?P<url>[^"\']+)(["\'][^>]*?)>(?P<text>.*?)</a>',
    re.IGNORECASE | re.DOTALL,
)
VOID_TAG_RE = re.compile(
    r'<(link|script|img|source|video|audio)\b[^>]*\b(href|src)\s*=\s*["\'](?P<url>[^"\']+)["\'][^>]*/?>',
    re.IGNORECASE,
)

# 明显不是文件路径的 href（minified 模板、占位符等）
SKIP_URL_RE = re.compile(
    r'^(\+|\.{0,2}/?$|javascript:|data:|mailto:|#|/favicon|irc:|\{|\$\()',
    re.IGNORECASE,
)

DIR_INTROS = {
    '': '嵌入式与网络协议技术笔记库：蓝牙、WiFi、TLS、网络协议、内核、文件系统、程序设计、总线、驱动、Buildroot、命令行工具等专题。',
    'program': 'C 语言标准、Linux 系统编程、并发与设计模式等程序设计笔记。',
    'bt': '蓝牙 Core 规范导读、信令索引、抓包流程与深度专题。',
    'bt/overview': 'Bluetooth Core 6.0 各协议层规范概览（Vol 1 / Part A–H）。',
    'bt/misc': '经典蓝牙与 BLE 深度专题：配对演进、报文解析、基带/射频、L2CAP/ATT/SDP 等。',
    'bt/packet': '经典蓝牙与 BLE 真实抓包流程对照（连接、配对、HID、SDP、L2CAP 等）。',
    'bt/principle': '从第一性原理理解蓝牙底层架构与 LMP 信令。',
    'bt/profile': '经典蓝牙应用层 Profile：RFCOMM、SPP 等。',
    'bt/signaling': 'HCI / LMP / LL / L2CAP / ATT 各层信令与 PDU 参考索引表。',
    'buildroot': 'Buildroot 嵌入式根文件系统：配置、内核镜像、编译框架、U-Boot、包编译与 GNU Make。',
    'bus': '硬件总线与接口：USB、串口、UART 调试等。',
    'bus/usb': 'USB 2.0 / Type-C 规范、描述符、事务、Linux Gadget 与调试。',
    'driver': 'Linux 内核设备模型与常见外设驱动（GPIO、UART、I2C、以太网、IRQ）。',
    'protocol': '应用层网络协议：MQTT、TCP、DHCP、DNS 等专题。',
    'protocol/dhcp': 'DHCP（RFC 2131）图解导读与规范原文。',
    'protocol/dns': 'DNS 域名解析流程与实现要点。',
    'protocol/mqtt': 'MQTT v3.1 / v5.0 图解导读、抓包实战与 OASIS 规范原文。',
    'protocol/tcp': 'TCP 可靠传输、握手异常分析、Wireshark 与 Linux 网络子系统。',
    'tls': 'TLS/SSL 协议、密码学基础、PKI、证书与握手抓包。',
    'tls/packet': 'TLS 1.2 / 1.3 解密抓包逐包分析。',
    'tls/rfc': 'TLS 相关 RFC 图解导读与规范原文存档。',
    'wifi': 'IEEE 802.11 标准导读、无线组网、wpa_supplicant/iw、协议栈、关联/认证、调制技术。',
    'commands': 'Linux 命令行工具：Git、Shell 工具链、抓包、网络诊断、文件传输与性能分析等。',
    'kernel': 'Linux 内核专题：内存分析、进程地址空间、MIPS 启动与 SoC 内存生命周期。',
    'fs': '嵌入式文件系统：SquashFS、UBIFS 等只读/闪存文件系统概览。',
}

MODULE_GROUPS = {
    '': [
        ('蓝牙', 'Core 规范、信令、抓包与专题', ['bt/']),
        ('WiFi / 802.11', '标准、协议栈、认证与调制', ['wifi/']),
        ('TLS / 密码学', '协议演化、RFC、抓包与 PKI', ['tls/']),
        ('网络协议', 'MQTT、TCP、DHCP、DNS', ['protocol/']),
        ('程序设计', 'C 语言、系统编程、并发与设计模式', ['program/']),
        ('总线与接口', 'USB、串口、UART', ['bus/']),
        ('Linux 驱动', '设备模型与外设子系统', ['driver/']),
        ('Buildroot', '嵌入式构建系统', ['buildroot/']),
        ('命令行工具', 'Git、抓包、网络诊断、文件传输', ['commands/']),
        ('Linux 内核', '内存分析、MIPS 启动', ['kernel/']),
        ('文件系统', 'SquashFS、UBIFS', ['fs/']),
    ],
    'program': [
        ('C 语言标准与安全', 'C89/C99/C11 与头文件安全分析', [
            'c_standards_guide.html', 'c_functions_safe_overview.html',
            'c_header_safe_func_guide.html',
        ]),
        ('系统编程导读', 'Linux 系统编程经典书籍笔记', [
            'linux_system_programming_2nd_guide.html', 'system_program_with_linux_guide.html',
        ]),
        ('GCC 扩展', 'GCC 语言与编译器扩展', ['gcc_extension_guide.html']),
        ('并发与设计', 'pthread、状态机、设计模式', [
            'pthread_guide.html', 'fsm_guide.html', 'design_pattern_guide.html',
        ]),
    ],
    'bt': [
        ('规范概览', 'Core 6.0 各层协议导读', ['overview/']),
        ('信令索引', 'HCI / LMP / LL / L2CAP / ATT 信令表', ['signaling/']),
        ('抓包流程', '连接、配对、业务场景抓包对照', ['packet/']),
        ('深度专题', '配对、解析、基带、射频等', ['misc/']),
        ('架构原理', '第一性原理与底层架构', ['principle/']),
        ('应用 Profile', 'RFCOMM、SPP 等', ['profile/']),
    ],
    'bt/overview': [
        ('架构与约定', 'Vol 1 架构、历史与术语', ['architecture_overview.html']),
        ('经典 BR/EDR', '基带、LMP、HCI、L2CAP、SDP', [
            'bt_baseband_overview.html', 'bt_lmp_overview.html', 'hci_overview.html',
            'l2cap_overview.html', 'sdp_overview.html',
        ]),
        ('BLE 控制器', 'PHY、链路层、控制器与安全', [
            'ble_phy_overview.html', 'ble_ll_overview.html', 'ble_controller_overview.html',
            'ble_ll_security_overview.html',
        ]),
        ('应用与安全', 'GAP、GATT、ATT、SMP', [
            'gap_overview.html', 'gatt_overview.html', 'att_overview.html', 'smp_overview.html',
        ]),
    ],
    'bt/misc': [
        ('配对与安全演进', '经典蓝牙与 BLE 配对机制', [
            'bt_pairing_security_evolution.html', 'ble_pairing_security_evolution.html',
            'lmp_ssp_deepdive.html',
        ]),
        ('报文解析流程', '自底向上分流与过滤', [
            'bt_packet_parse_flow.html', 'baseband_packet_parse_flow.html',
            'ble_stack_parser.html', 'ble_ll_controller_parse.html',
        ]),
        ('基带与射频', 'BR/EDR 基带、时序、RF 工作流', [
            'bt_baseband_packet_types.html', 'bt_bb_flow.html', 'bt_timing.html',
            'bluetooth_rf_flow.html', 'rf_workflow.html',
        ]),
        ('链路层协议', 'L2CAP、ATT、ACL、SDP', [
            'l2cap_classic_part.html', 'att_protocol.html', 'bt_acl_create_process.html',
            'sdp_database_structure.html',
        ]),
        ('参考数据', 'Assigned Numbers 等', ['assigned_numbers.html']),
    ],
    'bt/packet': [
        ('BLE 连接', '首次/再次连接 OTA 抓包', [
            'ble_first_connection_packet_flow.html', 'ble_second_connection_packet_flow.html',
        ]),
        ('经典蓝牙建链', 'Paging 至 Setup Complete', [
            'paging_to_setup_complete_packet_flow.html',
            'setup_complete_to_io_capability_packet_flow.html',
        ]),
        ('配对与安全', 'SSP 安全配对', ['bt_secure_pairing_packet_flow.html', 'bt_all_packet_flow.html']),
        ('服务与业务', 'SDP、HID、L2CAP 会话', [
            'sdp_packet_flow.html', 'hid_connection_packet_flow.html',
            'l2cap_session_packet_flow.html',
        ]),
    ],
    'bt/signaling': [
        ('主机接口 HCI', '命令与事件索引', ['HCI_Signaling_Index.html']),
        ('链路管理 LMP', '经典蓝牙 LMP 信令', ['LMP_Signaling_Index.html']),
        ('链路层 LL', 'BLE Control PDU', ['LL_Signaling_Index.html']),
        ('L2CAP 控制', '信令与参数配置', ['L2CAP_Signaling_Index.html']),
        ('属性协议 ATT', 'ATT 信令与操作码', ['ATT_Signaling_Index.html']),
    ],
    'bt/profile': [
        ('串口仿真', 'RFCOMM 与 SPP Profile', ['rfcomm.html', 'spp.html']),
    ],
    'bt/principle': [
        ('底层架构', '从第一性原理到 LMP 信令', ['bt_base_architecture_principle.html']),
    ],
    'wifi': [
        ('802.11 标准导读', '各代 PHY/MAC 标准精读', ['80211*_spec_overview.html', '80211*_guide.html']),
        ('无线组网', '802.11 无线网络架构', ['80211_wireless_networks_guide.html']),
        ('协议栈与连接', '栈结构、关联、发现', [
            'wifi_stack_guide.html', 'wifi_association_guide.html',
            'wifi_and_bluetooth_discovery_mechanism_comparison.html',
        ]),
        ('认证与安全', '802.11i 与跨协议对比', [
            'wifi_authentication_guide.html', 'wifi_authentication_comparison_with_tls_bt.html',
            '80211i_spec_overview.html',
        ]),
        ('用户空间工具', 'wpa_supplicant、iw 命令', [
            'wpa_supplicant_guide.html', 'wpa_supplicant_command.html', 'iw_guide.html',
        ]),
        ('物理层与调制', '调制解调技术', [
            'wireless_modulation_technology_guide.html',
            'wireless_modulation_technology_guide_2nd.html',
        ]),
    ],
    'tls': [
        ('密码学基础', '对称/非对称、哈希、分组模式', [
            'symmetric_encrypt.html', 'asymmetric_encryption_guide.html',
            'hash_digest_guide.html', 'crypt_operation_modes.html',
        ]),
        ('PKI 与证书', '公钥基础设施与 X.509', ['pki_framework.html', 'tls_certificate_guide.html']),
        ('TLS 协议', '演化、密钥交换、签名与全流程', [
            'ssl_tls_evolution.html', 'tls_key_exchange_guide.html',
            'tls_signature_guide.html', 'tls_full_flow.html',
        ]),
        ('握手与认证原理', 'TLS 握手逻辑与跨协议认证本质', [
            'tls_negotiatation_principle.html', 'auth_principle.html',
        ]),
        ('RFC 导读', 'TLS 相关 RFC 图解', ['rfc/']),
        ('抓包分析', 'TLS 1.2/1.3 解密抓包', ['packet/']),
        ('抓包素材', '从 pcap 导出的证书链 PEM/DER', ['certs_from_pcap/']),
    ],
    'tls/rfc': [
        ('图解导读', 'RFC 4492 / 5246 / 5705 / 6066 / 8446 等', ['tls_rfc*_guide.html']),
        ('规范原文', 'RFC HTML 存档', ['rfc*.html']),
    ],
    'tls/packet': [
        ('TLS 1.2', 'tls12_decode 抓包分析', ['tls12_decode_analysis.html']),
        ('TLS 1.3', 'tls13_decode 抓包分析', ['tls13_decode_analysis.html']),
    ],
    'protocol': [
        ('MQTT', 'v3.1 / v5.0 导读与抓包', ['mqtt/']),
        ('TCP / IP', '可靠传输、内核子系统、抓包', ['tcp/']),
        ('DHCP', 'RFC 2131 导读', ['dhcp/']),
        ('DNS', '域名解析', ['dns/']),
    ],
    'protocol/mqtt': [
        ('图解导读', 'MQTT v3.1 / v5.0 协议导读', ['mqtt_v*_guide.html']),
        ('抓包实战', '全流程 pcap 分析', ['mqtt_full_flow.html']),
        ('规范原文', 'OASIS / 历史规范 HTML', ['mqtt-v*.html']),
    ],
    'protocol/tcp': [
        ('协议原理', '可靠传输、握手异常', [
            'tcp_transmission.html', 'tcp_handshake_error_analysis.html',
        ]),
        ('抓包分析', 'Wireshark 实战', ['wireshark_*.html']),
        ('Linux 网络子系统', 'IP、邻居、路由内核实现', ['*_subsystem_guide.html']),
    ],
    'protocol/dhcp': [
        ('图解导读', 'RFC 2131 DHCP 导读', ['dhcp_rfc2131_guide.html']),
        ('规范原文', 'RFC 2131 HTML', ['rfc2131.html']),
    ],
    'protocol/dns': [
        ('DNS 解析', '域名解析流程与实现', ['dns_resolution_guide.html']),
    ],
    'bus': [
        ('USB', 'USB 2.0 / Type-C 等专题', ['usb/']),
        ('串口', 'Serial Port Complete 笔记', ['serial_port_complete.html']),
        ('UART 调试', '串口调试概览', ['uart_debug_overview.html']),
    ],
    'bus/usb': [
        ('规范与接口', 'USB 2.0、Type-C、Micro-USB、接口类型', [
            'usb20_spec.html', 'usb_typec_spec.html', 'micro_usb.html',
            'usb_interface_type.html', 'usb_complete_5th.html',
        ]),
        ('协议机制', '事务、描述符、标准请求', [
            'usb_packets.html', 'usb_descriptors.html', 'standard_requests.html',
        ]),
        ('物理层', '电特性与上下拉', ['usb_phy_electric_feature.html', 'usb_pin_pulldown.html']),
        ('Linux 与调试', 'Gadget 子系统、调试技巧', [
            'usb_linux_kernel_gadget.html', 'usb_debug.html',
        ]),
    ],
    'driver': [
        ('设备模型', 'Linux 设备模型', ['device_model_guide.html']),
        ('GPIO / 中断', '通用 IO 与 IRQ 子系统', ['gpio_guide.html', 'irq_guide.html']),
        ('串行总线', 'UART 子系统、I2C', [
            'uart_guide.html', 'uart_subsystem_guide.html', 'i2c_guide.html',
        ]),
        ('调试专题', 'I2C 等外设调试', ['i2c_debug_overview.html']),
        ('网络', '以太网 MAC/PHY', ['eth_guide.html']),
    ],
    'buildroot': [
        ('配置', 'menuconfig 与 defconfig', ['buildroot_configuration.html']),
        ('编译系统', '框架与官方手册笔记', [
            'buildroot_build_frame.html', 'buildroot_build_system.html',
        ]),
        ('包编译', '单包编译流程分析', ['buildroot_package_compilation_analysis.html']),
        ('内核构建', 'kernel image 与 configuration', [
            'kernel_image_guide.html', 'kernel_configuration_guide.html',
        ]),
        ('开发调试', 'GCC 头文件搜索', ['gcc_search_header_guide.html']),
        ('Bootloader', 'U-Boot 编译集成', ['buildroot_uboot_build.html']),
        ('构建工具', 'GNU Make 内部流程', ['make_internal.html']),
    ],
    'commands': [
        ('版本控制', 'Git 原理与常用命令', ['git_guide.html']),
        ('抓包与分析', 'tcpdump、TShark', ['tcpdump_guide.html', 'tshark_guide.html']),
        ('网络诊断', 'ip、ss、curl、丢包排查', [
            'ip_command_guide.html', 'ss_command_guide.html', 'curl_http_guide.html',
            'network_packet_loss_guide.html',
        ]),
        ('文本与查找', 'grep、sed、awk、find', [
            'grep_command_guide.html', 'sed_command_guide.html',
            'awk_command_guide.html', 'find_command_guide.html',
        ]),
        ('文件与传输', 'tar、rsync、ssh、unzip', [
            'tar_guide.html', 'rsync_guide.html', 'ssh_guide.html', 'unzip_guide.html',
        ]),
        ('系统与性能', 'mount、perf、squash', [
            'mount_guide.html', 'perf_guide.html', 'squash_guide.html',
        ]),
        ('调试与二进制', 'strace、ELF', ['strace_command_guide.html', 'elf_overview.html']),
    ],
    'kernel': [
        ('内存分析', '内核与用户态内存', [
            'memory_analysis_guide.html', 'process_memory_analysis.html',
        ]),
        ('MIPS / X2600', '启动流程与 SoC 内存生命周期', [
            'mips_kernel_boot_guide.html', 'mips_x2600_memory_lifecycle_analysis.html',
        ]),
    ],
    'fs': [
        ('闪存文件系统', 'SquashFS、UBIFS 概览', [
            'squashfs_overview.html', 'ubifs_overview.html',
        ]),
    ],
}


def should_skip_path(path: Path) -> bool:
    parts = path.relative_to(ROOT).parts
    return any(p in EXCLUDE_DIR_NAMES for p in parts)


def is_local_link(href: str) -> bool:
    if not href:
        return False
    href = href.strip()
    if href.startswith('#'):
        return False
    if href.startswith('/'):
        return False
    if SKIP_URL_RE.search(href):
        return False
    p = urlparse(href)
    if p.scheme and p.scheme not in ('',):
        return False
    if href.startswith('//'):
        return False
    return True


def looks_like_file_reference(url: str) -> bool:
    """仅处理像文件/目录路径的引用，忽略 minified 代码里的噪声。"""
    clean = url.split('?', 1)[0].split('#', 1)[0].strip()
    if not clean or clean in ('.', '..'):
        return False
    if '/' in clean or '.' in clean:
        return True
    # 无扩展名的短名可能是目录
    return len(clean) > 2 and re.match(r'^[\w.\-]+$', clean) is not None


def find_all_html_files():
    files = []
    for p in ROOT.rglob('*'):
        if not p.is_file() or p.suffix.lower() not in HTML_EXTS:
            continue
        if should_skip_path(p):
            continue
        files.append(p)
    return files


def build_name_index(files):
    by_name = {}
    for f in files:
        by_name.setdefault(f.name, []).append(f)
    return by_name


def resolve_link_target(src: Path, url: str) -> Path | None:
    clean = url.split('?', 1)[0].split('#', 1)[0].strip()
    if not clean:
        return None
    candidate = (src.parent / clean).resolve()
    if candidate.exists():
        return candidate
    base = Path(clean).name
    if base and '.' not in base:
        for ext in ('.html', '.htm', '.txt'):
            c = (src.parent / f'{base}{ext}').resolve()
            if c.exists():
                return c
    return None


def pick_fix_target(src: Path, url: str, name_index: dict) -> Path | None:
    resolved = resolve_link_target(src, url)
    if resolved:
        return resolved
    base = os.path.basename(url.split('?', 1)[0].split('#', 1)[0])
    if not base:
        return None
    matches = name_index.get(base, [])
    if len(matches) == 1:
        return matches[0]
    if '.' not in base:
        for ext in ('.html', '.htm'):
            matches = name_index.get(f'{base}{ext}', [])
            if len(matches) == 1:
                return matches[0]
    return None


def extract_page_title(path: Path) -> str:
    try:
        text = path.read_text(encoding='utf-8', errors='ignore')
    except OSError:
        return path.name
    m = TITLE_RE.search(text)
    if not m:
        return path.name
    t = re.sub(r'\s+', ' ', m.group(1)).strip()
    if path.name.lower().startswith('index.') and t.lower().startswith('index of '):
        return ''
    return t or path.name


def dir_index_href(dirname: str) -> str:
    return f'{dirname.rstrip("/")}/index.html'


def dir_link_variants(name: str):
    variants = {name}
    if name.endswith('/index.html'):
        variants.add(name[: -len('index.html')] + '/')
    elif name.endswith('/'):
        variants.add(name + 'index.html')
    return variants


def matches_pattern(name: str, pattern: str) -> bool:
    for variant in dir_link_variants(name):
        if pattern.endswith('/'):
            if variant == pattern or variant.rstrip('/') + '/' == pattern:
                return True
        elif fnmatch.fnmatch(variant, pattern):
            return True
    return False


def assign_to_groups(items, groups_spec):
    remaining = list(items)
    result = []
    for title, desc, patterns in groups_spec:
        matched = []
        still = []
        for text, href, kind in remaining:
            hit = any(matches_pattern(text, pat) or matches_pattern(href, pat) for pat in patterns)
            if hit:
                matched.append((text, href, kind))
            else:
                still.append((text, href, kind))
        if matched:
            result.append((title, desc, matched))
        remaining = still
    if remaining:
        result.append(('其他', '未归入上述分类的条目', remaining))
    return result


def auto_module_groups(items):
    """未在 MODULE_GROUPS 中配置的目录：按文件名启发式分组。"""
    files = [(t, h, k) for t, h, k in items if k == 'file']
    dirs = [(t, h, k) for t, h, k in items if k == 'dir']
    buckets = {
        '导读 / 指南': [],
        '概览': [],
        '抓包 / 流程': [],
        '规范 / RFC': [],
        '子目录': dirs,
    }
    rest = []
    for entry in files:
        name = entry[0].lower()
        if 'overview' in name or 'architecture' in name:
            buckets['概览'].append(entry)
        elif 'flow' in name or 'packet' in name or 'pcap' in name:
            buckets['抓包 / 流程'].append(entry)
        elif name.startswith('rfc') or 'rfc' in name:
            buckets['规范 / RFC'].append(entry)
        elif 'guide' in name or name.endswith('_guide.html'):
            buckets['导读 / 指南'].append(entry)
        else:
            rest.append(entry)
    if rest:
        buckets['其他文档'] = rest
    groups = []
    for title, members in buckets.items():
        if members:
            groups.append((title, '', members))
    return groups


def dir_rel_path(d: Path) -> str:
    if d == ROOT:
        return ''
    return str(d.relative_to(ROOT)).replace('\\', '/')


def extract_description(dirpath: Path) -> str:
    rel = dir_rel_path(dirpath)
    if rel in DIR_INTROS:
        return DIR_INTROS[rel]
    candidates = []
    for p in dirpath.iterdir():
        if not p.is_file():
            continue
        name = p.name.lower()
        if 'overview' in name or name in ('readme.md', 'readme.html', 'readme.htm'):
            candidates.append(p)
    if not candidates:
        return ''
    c = sorted(candidates, key=lambda x: x.name)[0]
    try:
        text = c.read_text(encoding='utf-8', errors='ignore')
    except OSError:
        return ''
    if c.suffix.lower() in ('.html', '.htm'):
        t = re.sub(r'<script[\s\S]*?</script>', '', text, flags=re.I)
        t = re.sub(r'<style[\s\S]*?</style>', '', t, flags=re.I)
        t = re.sub(r'<[^>]+>', '', t)
        t = re.sub(r'\s+', ' ', t).strip()
        if not t:
            return ''
        end = t.find('.')
        if end != -1 and end < 300:
            return t[: end + 1]
        return t[:300] + ('...' if len(t) > 300 else '')
    t = re.sub(r'\s+', ' ', text).strip()
    if not t:
        return ''
    end = t.find('.')
    if end != -1 and end < 300:
        return t[: end + 1]
    return t[:300] + ('...' if len(t) > 300 else '')


def resolve_subdir(d: Path, href: str) -> Path:
    if href.endswith('/index.html'):
        return d / href[: -len('/index.html')]
    return d / href.rstrip('/')


def link_label(d: Path, href: str, kind: str) -> str:
    if kind == 'dir':
        sub = resolve_subdir(d, href)
        rel = dir_rel_path(sub)
        intro = DIR_INTROS.get(rel, '')
        if intro:
            return f'{sub.name or rel or "/"} — {intro}'
        idx = sub / 'index.html'
        if idx.exists():
            t = extract_page_title(idx)
            if t:
                return t
        return href.rstrip('/') + '/' if href.endswith('/') else href
    fp = d / href
    if fp.exists():
        return extract_page_title(fp)
    return href


def render_functional_index(d: Path, items):
    rel = dir_rel_path(d)
    groups_spec = MODULE_GROUPS.get(rel)
    grouped = assign_to_groups(items, groups_spec) if groups_spec else auto_module_groups(items)
    lines = ['<section>', '<h2>功能模块索引</h2>']
    for gtitle, gdesc, members in grouped:
        if gtitle == '其他' and len(members) == len(items):
            continue
        lines.append(f'<h3>{gtitle}</h3>')
        if gdesc:
            lines.append(f'<p>{gdesc}</p>')
        lines.append('<ul>')
        for text, href, kind in sorted(members, key=lambda x: x[0].lower()):
            label = link_label(d, href, kind)
            lines.append(f'<li><a href="{href}">{label}</a></li>')
        lines.append('</ul>')
    lines.append('</section>')
    return lines


def write_index_for_dir(d: Path, dry_run: bool = False) -> bool:
    items = []
    skip_names = EXCLUDE_DIR_NAMES
    for p in sorted(d.iterdir(), key=lambda x: (x.is_file(), x.name.lower())):
        if p.name.lower().startswith('.') or p.name in skip_names:
            continue
        if should_skip_path(p):
            continue
        if p.is_dir():
            if p.name in LISTING_SKIP_DIRS:
                continue
            if (p / 'index.html').exists():
                href = dir_index_href(p.name)
            elif (p / 'index.htm').exists():
                href = f'{p.name}/index.htm'
            else:
                href = p.name + '/'
            items.append((p.name + '/', href, 'dir'))
        elif p.suffix.lower() in HTML_EXTS and p.name.lower() not in ('index.html', 'index.htm'):
            items.append((p.name, p.name, 'file'))

    desc = extract_description(d)
    lines = [
        '<!doctype html>',
        '<html>',
        '<head><meta charset="utf-8"><title>Index of %s</title></head>' % (d.name or '/'),
        '<body>',
        '<h1>Index of %s</h1>' % (d.name or '/'),
        '<section>',
        '<h2>模块说明</h2>',
        f'<p>{desc}</p>' if desc else '<p>（此处为模块说明占位，建议编辑以说明本目录功能/用途）</p>',
        '</section>',
    ]
    if items:
        lines.extend(render_functional_index(d, items))
    if items:
        lines.extend([
            '<nav>',
            '<h2>目录索引</h2>',
            '<ul>',
        ])
        for text, href, kind in items:
            lines.append(f'<li><a href="{href}">{text}</a></li>')
        lines.extend(['</ul>', '</nav>'])
    lines += ['</body>', '</html>']
    index_path = d / 'index.html'
    if not dry_run:
        index_path.write_text('\n'.join(lines), encoding='utf-8')
    return True


def classify_url(src: Path, url: str, name_index: dict) -> tuple[str | None, str]:
    """返回 (新路径或 None, 状态: skip|ok|fix|remove)。"""
    if not is_local_link(url) or not looks_like_file_reference(url):
        return None, 'skip'
    resolved = resolve_link_target(src, url)
    if resolved:
        rel = os.path.relpath(resolved, start=src.parent)
        if rel == url.split('?', 1)[0].split('#', 1)[0]:
            return None, 'ok'
        return rel, 'fix'
    target = pick_fix_target(src, url, name_index)
    if target:
        return os.path.relpath(target, start=src.parent), 'fix'
    return None, 'remove'


def repair_html_links(all_files, name_index: dict, dry_run: bool = False):
    """修复或删除失效本地链接。返回 (fixed, removed, broken)。"""
    fixed = removed = 0
    broken = []

    for src in all_files:
        if src.name.lower().startswith('index.'):
            continue
        try:
            text = src.read_text(encoding='utf-8', errors='ignore')
        except OSError:
            continue
        changed = False

        def repl_a(m):
            nonlocal changed, fixed, removed
            url = m.group('url')
            new_path, status = classify_url(src, url, name_index)
            if status in ('skip', 'ok'):
                return m.group(0)
            if status == 'fix':
                changed = True
                fixed += 1
                return f'<a{m.group(1)}{new_path}{m.group(3)}>{m.group("text")}</a>'
            changed = True
            removed += 1
            return m.group('text')

        text = A_TAG_RE.sub(repl_a, text)

        def repl_void(m):
            nonlocal changed, fixed, removed
            url = m.group('url')
            new_path, status = classify_url(src, url, name_index)
            if status in ('skip', 'ok'):
                return m.group(0)
            if status == 'fix':
                changed = True
                fixed += 1
                return re.sub(
                    r'(href|src)\s*=\s*["\'][^"\']+["\']',
                    f'{m.group(2)}="{new_path}"',
                    m.group(0),
                    count=1,
                    flags=re.I,
                )
            changed = True
            removed += 1
            return ''

        text = VOID_TAG_RE.sub(repl_void, text)

        def repl_attr(m):
            nonlocal changed, fixed, removed
            attr, url = m.group(1), m.group('url')
            new_path, status = classify_url(src, url, name_index)
            if status in ('skip', 'ok'):
                return m.group(0)
            if status == 'fix':
                changed = True
                fixed += 1
                return f'{attr}={m.group(2)}{new_path}{m.group(2)}'
            changed = True
            removed += 1
            return ''

        text = LINK_RE.sub(repl_attr, text)

        if changed and not dry_run:
            src.write_text(text, encoding='utf-8')

    return fixed, removed


def collect_index_dirs(all_html):
    dirs = set()
    for p in all_html:
        if should_skip_path(p):
            continue
        dirs.add(p.parent)
    extra = set()
    for d in dirs:
        p = d.parent
        while p >= ROOT:
            if any(part in EXCLUDE_DIR_NAMES for part in p.relative_to(ROOT).parts):
                break
            extra.add(p)
            if p == ROOT:
                break
            p = p.parent
    return dirs | extra


def main():
    parser = argparse.ArgumentParser(description='更新目录索引并清理失效 HTML 链接')
    parser.add_argument('--dry-run', action='store_true', help='只报告，不写文件')
    parser.add_argument('--indices-only', action='store_true', help='仅更新 index.html，不修改正文链接')
    args = parser.parse_args()

    all_html = find_all_html_files()
    name_index = build_name_index(all_html)

    dirs = collect_index_dirs(all_html)
    index_count = 0
    # 先写子目录 index，再写根目录，确保父级链接能指向 xxx/index.html
    ordered = sorted(d for d in dirs if d != ROOT) + ([ROOT] if ROOT in dirs else [])
    for d in ordered:
        if should_skip_path(d) and d != ROOT:
            continue
        if write_index_for_dir(d, dry_run=args.dry_run):
            index_count += 1

    fixed = removed = 0
    if not args.indices_only:
        fixed, removed = repair_html_links(
            all_html, name_index, dry_run=args.dry_run
        )

    print('Summary:')
    print('  HTML files scanned:', len(all_html))
    print('  index.html dirs processed:', index_count)
    if args.dry_run:
        print('  (dry-run: no files written)')
    if not args.indices_only:
        print('  links fixed (unique filename match):', fixed)
        print('  links removed (unresolvable):', removed)
    print('Done.')


if __name__ == '__main__':
    main()
