#!/usr/bin/env python3
"""
Scan the repository for directories containing HTML files, create/update index.html
for each directory, and fix broken relative links by searching for matching filenames
in the repo.

Each index.html contains:
  - 模块说明：目录用途简介
  - 功能模块索引：按主题分组的可读链接（使用页面 <title> 作为链接文字）
  - 目录索引：完整文件/子目录列表（保留原有行为）

Usage: python3 tools/update_indices.py
"""
import fnmatch
import os
import re
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent.parent

HTML_EXTS = {'.html', '.htm'}

TITLE_RE = re.compile(r'<title[^>]*>(.*?)</title>', re.IGNORECASE | re.DOTALL)

# 目录简介（相对 ROOT 的路径；空字符串表示站点根目录）
DIR_INTROS = {
    '': '嵌入式与网络协议技术笔记库：蓝牙、WiFi、TLS、网络协议、程序设计、总线、驱动、Buildroot、工具等专题。',
    'program': 'C 语言标准、Linux 系统编程、并发与设计模式等程序设计笔记。',
    'bt': '蓝牙 Core 规范导读、信令索引、抓包流程与深度专题。',
    'bt/overview': 'Bluetooth Core 6.0 各协议层规范概览（Vol 1 / Part A–H）。',
    'bt/misc': '经典蓝牙与 BLE 深度专题：配对演进、报文解析、基带/射频、L2CAP/ATT/SDP 等。',
    'bt/packet': '经典蓝牙与 BLE 真实抓包流程对照（连接、配对、HID、SDP、L2CAP 等）。',
    'bt/principle': '从第一性原理理解蓝牙底层架构与 LMP 信令。',
    'bt/profile': '经典蓝牙应用层 Profile：RFCOMM、SPP 等。',
    'bt/signaling': 'HCI / LMP / LL / L2CAP / ATT 各层信令与 PDU 参考索引表。',
    'buildroot': 'Buildroot 嵌入式根文件系统：配置、编译框架、U-Boot 与 GNU Make。',
    'bus': '硬件总线与接口：USB 专题、串口等。',
    'bus/usb': 'USB 2.0 / Type-C 规范、描述符、事务、Linux Gadget 与调试。',
    'driver': 'Linux 内核设备模型与常见外设驱动（GPIO、UART、I2C、以太网、IRQ）。',
    'protocol': '应用层网络协议：MQTT、TCP、DHCP 等专题。',
    'protocol/dhcp': 'DHCP（RFC 2131）图解导读与规范原文。',
    'protocol/mqtt': 'MQTT v3.1 / v5.0 图解导读、抓包实战与 OASIS 规范原文。',
    'protocol/tcp': 'TCP 可靠传输、Wireshark 分析与 Linux 内核 IP/邻居/路由子系统。',
    'tls': 'TLS/SSL 协议、密码学基础、PKI、证书与握手抓包。',
    'tls/packet': 'TLS 1.2 / 1.3 解密抓包逐包分析。',
    'tls/rfc': 'TLS 相关 RFC 图解导读与规范原文存档。',
    'tools': '开发工具：Git、抓包（tcpdump/TShark）、网络配置（ip 命令）等。',
    'wifi': 'IEEE 802.11 标准导读、协议栈、关联/认证、调制技术与跨协议对比。',
}

# 功能分组：(分组标题, 分组说明, 文件名/目录名 glob 列表，* 匹配任意)
# 路径为相对 ROOT；未匹配项归入「其他」
MODULE_GROUPS = {
    'protocol': [
        ('MQTT', 'v3.1 / v5.0 导读与抓包', ['mqtt/']),
        ('TCP', '可靠传输与 Wireshark 分析', ['tcp/']),
        ('DHCP', 'RFC 2131 导读', ['dhcp/']),
    ],
    '': [
        ('蓝牙', 'Core 规范、信令、抓包与专题', ['bt/']),
        ('WiFi / 802.11', '标准、协议栈、认证与调制', ['wifi/']),
        ('TLS / 密码学', '协议演化、RFC、抓包与 PKI', ['tls/']),
        ('网络协议', 'MQTT、TCP、DHCP', ['protocol/']),
        ('程序设计', 'C 语言、系统编程、并发与设计模式', ['program/']),
        ('总线与接口', 'USB、串口等', ['bus/']),
        ('Linux 驱动', '设备模型与外设子系统', ['driver/']),
        ('Buildroot', '嵌入式构建系统', ['buildroot/']),
        ('工具', 'Git、抓包与网络命令', ['tools/']),
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
        ('802.11 标准导读', '各代 PHY/MAC 标准精读', ['80211*.html']),
        ('协议栈与连接', '栈结构、关联、发现', [
            'wifi_stack_guide.html', 'wifi_association_guide.html',
            'wifi_and_bluetooth_discovery_mechanism_comparison.html',
        ]),
        ('认证与安全', '802.11i 与跨协议对比', [
            'wifi_authentication_guide.html', 'wifi_authentication_comparison_with_tls_bt.html',
            '80211i_spec_overview.html',
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
    'protocol/mqtt': [
        ('图解导读', 'MQTT v3.1 / v5.0 协议导读', ['mqtt_v*_guide.html']),
        ('抓包实战', '全流程 pcap 分析', ['mqtt_full_flow.html']),
        ('规范原文', 'OASIS / 历史规范 HTML', ['mqtt-v*.html']),
    ],
    'protocol/tcp': [
        ('协议原理', '可靠传输机制', ['tcp_transmission.html']),
        ('抓包分析', 'Wireshark 实战', ['wireshark_*.html']),
        ('Linux 网络子系统', 'IP、邻居、路由内核实现', ['*_subsystem_guide.html']),
    ],
    'protocol/dhcp': [
        ('图解导读', 'RFC 2131 DHCP 导读', ['dhcp_rfc2131_guide.html']),
        ('规范原文', 'RFC 2131 HTML', ['rfc2131.html']),
    ],
    'bus': [
        ('USB', 'USB 2.0 / Type-C 等专题', ['usb/']),
        ('串口', 'Serial Port Complete 笔记', ['serial_port_complete.html']),
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
        ('串行总线', 'UART、I2C', ['uart_guide.html', 'i2c_guide.html']),
        ('网络', '以太网 MAC/PHY', ['eth_guide.html']),
    ],
    'buildroot': [
        ('配置', 'menuconfig 与 defconfig', ['buildroot_configuration.html']),
        ('编译系统', '框架与官方手册笔记', [
            'buildroot_build_frame.html', 'buildroot_build_system.html',
        ]),
        ('Bootloader', 'U-Boot 编译集成', ['buildroot_uboot_build.html']),
        ('构建工具', 'GNU Make 内部流程', ['make_internal.html']),
    ],
    'tools': [
        ('版本控制', 'Git 原理与常用命令', ['git_guide.html']),
        ('抓包与分析', 'tcpdump、TShark', ['tcpdump_guide.html', 'tshark_guide.html']),
        ('网络配置', 'iproute2 ip 命令', ['ip_command_guide.html']),
    ],
}


def is_local_link(href: str) -> bool:
    if not href:
        return False
    href = href.strip()
    if href.startswith('#'):
        return False
    if href.startswith('/'):
        return False
    p = urlparse(href)
    if p.scheme and p.scheme not in ('',):
        return False
    if href.startswith('//'):
        return False
    return True


def find_all_html_files():
    files = []
    for p in ROOT.rglob('*'):
        if p.is_file() and p.suffix.lower() in HTML_EXTS:
            files.append(p)
    return files


def build_name_index(files):
    by_name = {}
    for f in files:
        by_name.setdefault(f.name, []).append(f)
    return by_name


LINK_RE = re.compile(r'(href|src)=("|\')(?P<url>.*?)(\2)', re.IGNORECASE)


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
    """子目录链接显式指向 index.html，便于静态托管与本地打开。"""
    return f'{dirname.rstrip("/")}/index.html'


def dir_link_variants(name: str):
    """目录条目在匹配分组时同时识别 dir/ 与 dir/index.html。"""
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
    """Return list of (title, desc, [(href, label, kind), ...])."""
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
            name = sub.name or rel or '/'
            return f'{name} — {intro}'
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
    if not groups_spec:
        return []

    grouped = assign_to_groups(items, groups_spec)
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


def write_index_for_dir(d: Path):
    items = []
    skip_names = {'__pycache__', 'node_modules'}
    for p in sorted(d.iterdir(), key=lambda x: (x.is_file(), x.name.lower())):
        if p.name.lower().startswith('.') or p.name in skip_names:
            continue
        if p.is_dir():
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
    ]

    lines.append('<section>')
    lines.append('<h2>模块说明</h2>')
    if desc:
        lines.append(f'<p>{desc}</p>')
    else:
        lines.append('<p>（此处为模块说明占位，建议编辑以说明本目录功能/用途）</p>')
    lines.append('</section>')

    if items:
        lines.extend(render_functional_index(d, items))

    if items:
        lines.append('<nav>')
        lines.append('<h2>目录索引</h2>')
        lines.append('<ul>')
        for text, href, kind in items:
            lines.append(f'<li><a href="{href}">{text}</a></li>')
        lines.append('</ul>')
        lines.append('</nav>')

    lines += ['</body>', '</html>']
    index_path = d / 'index.html'
    index_path.write_text('\n'.join(lines), encoding='utf-8')
    return True


def scan_and_fix_links(all_files, name_index):
    broken = []
    multiple = []
    fixed_count = 0
    for src in all_files:
        text = src.read_text(encoding='utf-8', errors='ignore')
        changed = False

        def repl(m):
            nonlocal changed, fixed_count
            attr = m.group(1)
            url = m.group('url')
            if not is_local_link(url):
                return m.group(0)
            candidate = (src.parent / url).resolve()
            if candidate.exists():
                return m.group(0)
            url_clean = url.split('?', 1)[0].split('#', 1)[0]
            base = os.path.basename(url_clean)
            if not base:
                broken.append((src, url))
                return m.group(0)
            matches = name_index.get(base, [])
            if len(matches) == 1:
                target = matches[0]
                rel = os.path.relpath(target, start=src.parent)
                changed = True
                fixed_count += 1
                return f'{attr}={m.group(2)}{rel}{m.group(2)}'
            if len(matches) > 1:
                multiple.append((src, url, matches))
                return m.group(0)
            broken.append((src, url))
            return m.group(0)

        new_text = LINK_RE.sub(repl, text)
        if changed:
            src.write_text(new_text, encoding='utf-8')
    return fixed_count, broken, multiple


def collect_index_dirs(all_html):
    """Directories that need index.html: HTML parents and ancestors up to ROOT."""
    dirs = set(p.parent for p in all_html)
    extra = set()
    for d in dirs:
        p = d.parent
        while p >= ROOT:
            extra.add(p)
            if p == ROOT:
                break
            p = p.parent
    return dirs | extra


def main():
    all_html = find_all_html_files()
    name_index = build_name_index(all_html)

    dirs = collect_index_dirs(all_html)
    created = 0
    updated = 0
    for d in sorted(dirs):
        p = d / 'index.html'
        ok = write_index_for_dir(d)
        if ok:
            if p.exists():
                updated += 1
            else:
                created += 1

    fixed_count, broken, multiple = scan_and_fix_links(all_html, name_index)

    top = ROOT / 'index.htm'
    if top.exists():
        fixed2, broken2, multiple2 = scan_and_fix_links([top], name_index)
        fixed_count += fixed2
        broken += broken2
        multiple += multiple2

    print('Summary:')
    print('  directories processed:', len(dirs))
    print('  index.html created/updated: created=%d updated=%d' % (created, updated))
    print('  links fixed:', fixed_count)
    print('  unresolved broken links:', len(broken))
    if broken:
        for src, url in broken[:20]:
            print('    MISSING in %s -> %s' % (src.relative_to(ROOT), url))
    if multiple:
        print('  ambiguous links (multiple candidates):', len(multiple))
        for src, url, matches in multiple[:20]:
            print('    AMBIG %s -> %s candidates:' % (src.relative_to(ROOT), url))
            for m in matches[:5]:
                print('      - %s' % m.relative_to(ROOT))


if __name__ == '__main__':
    main()
