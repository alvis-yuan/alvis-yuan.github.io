import { useEffect, useRef } from "react";

const htmlContent = `
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root {
    --bg: #0f1117; --bg2: #161b22; --bg3: #1c2230; --bg4: #21283a;
    --border: #30363d; --accent: #58a6ff; --accent2: #3fb950;
    --accent3: #f78166; --accent4: #d2a679; --accent5: #bc8cff;
    --text: #e6edf3; --text2: #8b949e; --text3: #c9d1d9; --warn: #e3b341;
    --nav-w: 280px; --progress-h: 3px;
  }
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  html { scroll-behavior: smooth; }
  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'PingFang SC', sans-serif; background: var(--bg); color: var(--text); line-height: 1.7; overflow-x: hidden; }
  #progress-bar { position: fixed; top: 0; left: 0; height: var(--progress-h); background: linear-gradient(90deg, var(--accent), var(--accent5)); z-index: 9999; transition: width 0.1s linear; width: 0%; }
  #sidebar { position: fixed; top: var(--progress-h); left: 0; width: var(--nav-w); height: calc(100vh - var(--progress-h)); background: var(--bg2); border-right: 1px solid var(--border); overflow-y: auto; z-index: 100; display: flex; flex-direction: column; transition: transform 0.3s ease; }
  #sidebar::-webkit-scrollbar { width: 4px; }
  #sidebar::-webkit-scrollbar-thumb { background: var(--border); border-radius: 2px; }
  .sidebar-header { padding: 16px; border-bottom: 1px solid var(--border); background: var(--bg2); position: sticky; top: 0; z-index: 1; }
  .sidebar-title { font-size: 13px; font-weight: 700; color: var(--accent); letter-spacing: 0.5px; text-transform: uppercase; }
  .sidebar-subtitle { font-size: 11px; color: var(--text2); margin-top: 2px; }
  .nav-group { border-bottom: 1px solid var(--border); }
  .nav-group-header { display: flex; align-items: center; justify-content: space-between; padding: 10px 16px; cursor: pointer; font-size: 12px; font-weight: 600; color: var(--text2); text-transform: uppercase; letter-spacing: 0.5px; user-select: none; transition: color 0.2s; }
  .nav-group-header:hover { color: var(--text); }
  .nav-group-header .arrow { transition: transform 0.2s; font-size: 10px; }
  .nav-group-header.collapsed .arrow { transform: rotate(-90deg); }
  .nav-group-items { overflow: hidden; transition: max-height 0.3s ease; }
  .nav-group-items.hidden { max-height: 0 !important; }
  .nav-item { display: flex; align-items: center; gap: 8px; padding: 7px 16px 7px 24px; font-size: 13px; color: var(--text2); cursor: pointer; transition: all 0.15s; border-left: 3px solid transparent; text-decoration: none; }
  .nav-item:hover { color: var(--text); background: var(--bg3); }
  .nav-item.active { color: var(--accent); background: rgba(88,166,255,0.1); border-left-color: var(--accent); }
  .nav-item .nav-dot { width: 6px; height: 6px; border-radius: 50%; background: var(--border); flex-shrink: 0; transition: background 0.15s; }
  .nav-item.active .nav-dot { background: var(--accent); }
  .nav-badge { margin-left: auto; font-size: 10px; padding: 1px 6px; border-radius: 10px; background: var(--bg4); color: var(--text2); }
  #main { margin-left: var(--nav-w); padding: 40px 0; min-height: 100vh; }
  .content-wrap { max-width: 860px; margin: 0 auto; padding: 0 32px; }
  #topbar { display: none; position: fixed; top: var(--progress-h); left: 0; right: 0; height: 52px; background: var(--bg2); border-bottom: 1px solid var(--border); z-index: 99; align-items: center; padding: 0 16px; gap: 12px; }
  #topbar .topbar-title { font-size: 14px; font-weight: 600; color: var(--text); flex: 1; }
  #hamburger { background: none; border: 1px solid var(--border); color: var(--text); padding: 6px 10px; border-radius: 6px; cursor: pointer; font-size: 16px; }
  #overlay { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.6); z-index: 98; }
  .page-hero { background: linear-gradient(135deg, var(--bg2) 0%, var(--bg3) 100%); border: 1px solid var(--border); border-radius: 12px; padding: 36px 40px; margin-bottom: 40px; position: relative; overflow: hidden; }
  .page-hero::before { content: ''; position: absolute; top: -40px; right: -40px; width: 180px; height: 180px; background: radial-gradient(circle, rgba(88,166,255,0.12), transparent 70%); pointer-events: none; }
  .hero-tag { display: inline-block; font-size: 11px; font-weight: 600; color: var(--accent); background: rgba(88,166,255,0.12); border: 1px solid rgba(88,166,255,0.3); padding: 3px 10px; border-radius: 20px; margin-bottom: 14px; text-transform: uppercase; letter-spacing: 0.5px; }
  .hero-h1 { font-size: 28px; font-weight: 700; line-height: 1.3; margin-bottom: 12px; background: linear-gradient(90deg, var(--text), var(--accent)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; background-clip: text; }
  .hero-desc { font-size: 14px; color: var(--text2); max-width: 600px; }
  .hero-refs { margin-top: 16px; display: flex; flex-wrap: wrap; gap: 8px; }
  .ref-chip { font-size: 11px; color: var(--accent); background: rgba(88,166,255,0.08); border: 1px solid rgba(88,166,255,0.2); padding: 3px 10px; border-radius: 20px; text-decoration: none; transition: background 0.2s; }
  .ref-chip:hover { background: rgba(88,166,255,0.18); }
  .section { margin-bottom: 60px; scroll-margin-top: 88px; }
  .section-header { display: flex; align-items: center; gap: 14px; margin-bottom: 24px; padding-bottom: 14px; border-bottom: 1px solid var(--border); }
  .section-icon { width: 40px; height: 40px; border-radius: 10px; display: flex; align-items: center; justify-content: center; font-size: 18px; flex-shrink: 0; }
  .section-num { font-size: 11px; font-weight: 700; color: var(--text2); text-transform: uppercase; letter-spacing: 1px; }
  .section-title { font-size: 22px; font-weight: 700; color: var(--text); }
  .card { background: var(--bg2); border: 1px solid var(--border); border-radius: 10px; padding: 20px; margin-bottom: 20px; transition: border-color 0.2s; }
  .card:hover { border-color: rgba(88,166,255,0.3); }
  .card-title { font-size: 15px; font-weight: 600; margin-bottom: 10px; display: flex; align-items: center; gap: 8px; color: var(--text); }
  .badge { font-size: 10px; font-weight: 600; padding: 2px 8px; border-radius: 12px; }
  .badge-nic { background: rgba(247,129,102,0.15); color: var(--accent3); border: 1px solid rgba(247,129,102,0.3); }
  .badge-arp { background: rgba(210,166,121,0.15); color: var(--accent4); border: 1px solid rgba(210,166,121,0.3); }
  .badge-ip  { background: rgba(63,185,80,0.15); color: var(--accent2); border: 1px solid rgba(63,185,80,0.3); }
  .badge-tcp { background: rgba(88,166,255,0.15); color: var(--accent); border: 1px solid rgba(88,166,255,0.3); }
  .badge-udp { background: rgba(188,140,255,0.15); color: var(--accent5); border: 1px solid rgba(188,140,255,0.3); }
  .badge-fw  { background: rgba(227,179,65,0.15); color: var(--warn); border: 1px solid rgba(227,179,65,0.3); }
  .badge-skb { background: rgba(88,166,255,0.12); color: var(--accent); border: 1px solid rgba(88,166,255,0.25); }
  .card p { font-size: 14px; color: var(--text3); margin-bottom: 10px; }
  .card p:last-child { margin-bottom: 0; }
  pre { background: #0d1117; border: 1px solid var(--border); border-radius: 8px; padding: 16px; overflow-x: auto; margin: 14px 0; }
  pre code { font-family: 'SF Mono','Fira Code',Consolas,monospace; font-size: 13px; color: #a5d6ff; line-height: 1.6; white-space: pre; }
  code { font-family: 'SF Mono','Fira Code',Consolas,monospace; font-size: 12.5px; background: rgba(88,166,255,0.1); color: var(--accent); padding: 2px 6px; border-radius: 4px; border: 1px solid rgba(88,166,255,0.2); }
  pre code { background: none; border: none; padding: 0; color: #a5d6ff; }
  .info-box { border-radius: 8px; padding: 14px 18px; margin: 16px 0; border-left: 3px solid; font-size: 14px; }
  .info-box p { margin: 0; }
  .info-note { background: rgba(88,166,255,0.08); border-color: var(--accent); color: #9ecbff; }
  .info-warn { background: rgba(227,179,65,0.08); border-color: var(--warn); color: #f0c674; }
  .info-tip  { background: rgba(63,185,80,0.08); border-color: var(--accent2); color: #85e89d; }
  .table-wrap { overflow-x: auto; margin: 14px 0; border-radius: 8px; border: 1px solid var(--border); }
  table { width: 100%; border-collapse: collapse; font-size: 13px; }
  th { background: var(--bg3); color: var(--text2); font-weight: 600; padding: 10px 14px; text-align: left; font-size: 12px; }
  td { padding: 9px 14px; border-top: 1px solid var(--border); color: var(--text3); vertical-align: top; }
  tr:hover td { background: var(--bg3); }
  td:first-child { font-family: 'SF Mono',Consolas,monospace; font-size: 12px; color: var(--accent); }
  .diagram { margin: 24px 0; }
  .diagram-title { font-size: 13px; color: var(--text2); text-align: center; margin-bottom: 8px; font-weight: 500; }
  svg { display: block; margin: 0 auto; }
  .summary-box { background: linear-gradient(135deg, var(--bg3), var(--bg4)); border: 1px solid rgba(88,166,255,0.25); border-radius: 10px; padding: 20px; margin-top: 32px; }
  .summary-box h4 { font-size: 14px; font-weight: 700; color: var(--accent); margin-bottom: 12px; display: flex; align-items: center; gap: 8px; }
  .summary-box ul { list-style: none; display: flex; flex-direction: column; gap: 6px; }
  .summary-box li { font-size: 13.5px; color: var(--text3); padding-left: 20px; position: relative; }
  .summary-box li::before { content: '\\2192'; position: absolute; left: 0; color: var(--accent); }
  .checklist { list-style: none; }
  .checklist li { padding: 6px 0 6px 28px; position: relative; font-size: 13.5px; color: var(--text3); border-bottom: 1px solid rgba(48,54,61,0.5); }
  .checklist li:last-child { border-bottom: none; }
  .checklist li::before { content: '\\2610'; position: absolute; left: 0; color: var(--accent); font-size: 14px; }
  .checklist code { font-size: 12px; }
  .layer-pills { display: flex; flex-wrap: wrap; gap: 8px; margin: 16px 0; }
  .layer-pill { padding: 5px 14px; border-radius: 20px; font-size: 12px; font-weight: 600; cursor: default; }
  h3 { font-size: 17px; font-weight: 600; color: var(--text); margin: 24px 0 12px; display: flex; align-items: center; gap: 10px; }
  h3::before { content: ''; display: inline-block; width: 4px; height: 16px; background: var(--accent); border-radius: 2px; flex-shrink: 0; }
  h4 { font-size: 14px; font-weight: 600; color: var(--text2); margin: 16px 0 8px; text-transform: uppercase; letter-spacing: 0.5px; }
  p { font-size: 14px; color: var(--text3); margin-bottom: 12px; }
  a { color: var(--accent); }
  @media (max-width: 768px) {
    #sidebar { transform: translateX(-100%); }
    #sidebar.open { transform: translateX(0); }
    #topbar { display: flex; }
    #main { margin-left: 0; padding-top: 60px; }
    .content-wrap { padding: 0 16px; }
    .page-hero { padding: 24px 20px; }
    .hero-h1 { font-size: 22px; }
    #overlay.show { display: block; }
  }
</style>
</head>
<body>
<div id="progress-bar"></div>
<div id="topbar">
  <button id="hamburger" aria-label="菜单">&#9776;</button>
  <span class="topbar-title">Linux 网络丢包排查</span>
</div>
<div id="overlay"></div>
<nav id="sidebar">
  <div class="sidebar-header">
    <div class="sidebar-title">&#128269; 丢包排查指南</div>
    <div class="sidebar-subtitle">Linux Kernel Network Drop</div>
  </div>
  <div class="nav-group">
    <div class="nav-group-header" onclick="toggleGroup(this)"><span>&#128203; 概述</span><span class="arrow">&#9662;</span></div>
    <div class="nav-group-items">
      <a class="nav-item" href="#overview"><span class="nav-dot"></span>数据包路径总览</a>
      <a class="nav-item" href="#interfaces"><span class="nav-dot"></span>核心接口速查</a>
    </div>
  </div>
  <div class="nav-group">
    <div class="nav-group-header" onclick="toggleGroup(this)"><span>&#128268; 链路层</span><span class="arrow">&#9662;</span></div>
    <div class="nav-group-items">
      <a class="nav-item" href="#nic"><span class="nav-dot"></span>网卡 &amp; Ring Buffer <span class="nav-badge">NIC</span></a>
      <a class="nav-item" href="#softirq"><span class="nav-dot"></span>SoftIRQ &amp; Backlog <span class="nav-badge">NAPI</span></a>
    </div>
  </div>
  <div class="nav-group">
    <div class="nav-group-header" onclick="toggleGroup(this)"><span>&#128225; 网络层</span><span class="arrow">&#9662;</span></div>
    <div class="nav-group-items">
      <a class="nav-item" href="#arp"><span class="nav-dot"></span>ARP 丢包 <span class="nav-badge">L2/3</span></a>
      <a class="nav-item" href="#ip"><span class="nav-dot"></span>IP 层丢包 <span class="nav-badge">L3</span></a>
      <a class="nav-item" href="#firewall"><span class="nav-dot"></span>Netfilter / iptables <span class="nav-badge">FW</span></a>
    </div>
  </div>
  <div class="nav-group">
    <div class="nav-group-header" onclick="toggleGroup(this)"><span>&#128279; 传输层</span><span class="arrow">&#9662;</span></div>
    <div class="nav-group-items">
      <a class="nav-item" href="#tcp"><span class="nav-dot"></span>TCP 丢包 <span class="nav-badge">L4</span></a>
      <a class="nav-item" href="#udp"><span class="nav-dot"></span>UDP 丢包 <span class="nav-badge">L4</span></a>
    </div>
  </div>
  <div class="nav-group">
    <div class="nav-group-header" onclick="toggleGroup(this)"><span>&#128300; 高级工具</span><span class="arrow">&#9662;</span></div>
    <div class="nav-group-items">
      <a class="nav-item" href="#skb"><span class="nav-dot"></span>SKB Drop Reason <span class="nav-badge">v5.17+</span></a>
      <a class="nav-item" href="#checklist"><span class="nav-dot"></span>排查速查表</a>
    </div>
  </div>
</nav>
<main id="main">
<div class="content-wrap">

<!-- Hero -->
<div class="page-hero">
  <div class="hero-tag">Linux Kernel Network Debugging</div>
  <h1 class="hero-h1">Linux 网络丢包全栈排查指南</h1>
  <p class="hero-desc">覆盖 NIC → ARP → IP → Netfilter → TCP/UDP 全协议栈，基于内核原生 /proc、/sys、ethtool 接口，结合 SNMP MIB 计数器与 SKB drop reason 机制。</p>
  <div class="hero-refs">
    <a class="ref-chip" href="https://docs.kernel.org/networking/snmp_counter.html" target="_blank">kernel.org SNMP Counter</a>
    <a class="ref-chip" href="https://www.kernel.org/doc/html/v6.3/networking/statistics.html" target="_blank">Interface Statistics</a>
    <a class="ref-chip" href="https://github.com/leandromoreira/linux-network-performance-parameters" target="_blank">Linux Network Perf Params</a>
    <a class="ref-chip" href="https://developers.redhat.com/articles/2023/07/19/how-retrieve-packet-drop-reasons-linux-kernel" target="_blank">Red Hat SKB Drop Reason</a>
    <a class="ref-chip" href="https://docs.rockylinux.org/10/guides/network/performance_tuning/network_performance_irq_tuning/" target="_blank">Rocky Linux IRQ Tuning</a>
  </div>
</div>

<!-- Section 1: Overview -->
<section class="section" id="overview">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(88,166,255,0.12)">&#128202;</div>
    <div><div class="section-num">Section 01</div><div class="section-title">数据包接收路径总览</div></div>
  </div>
  <p>理解丢包发生在哪一层，首先需要理解 Linux 内核从网卡到应用程序的完整数据包路径。以下接收路径来源：<a href="https://github.com/leandromoreira/linux-network-performance-parameters" target="_blank">linux-network-performance-parameters</a>。</p>

  <div class="diagram">
    <div class="diagram-title">图 1：Linux 网络协议栈接收路径与丢包点</div>
    <svg viewBox="0 0 780 520" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:780px;background:#0d1117;border:1px solid #30363d;border-radius:10px">
      <defs><marker id="arr" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0,0 L0,6 L8,3 z" fill="#30363d"/></marker></defs>
      <text x="390" y="26" text-anchor="middle" fill="#8b949e" font-size="12" font-family="monospace">Linux Ingress Path — Drop Points</text>
      <text x="120" y="54" text-anchor="middle" fill="#6a7889" font-size="11" font-family="monospace">硬件层</text>
      <text x="390" y="54" text-anchor="middle" fill="#6a7889" font-size="11" font-family="monospace">内核层</text>
      <text x="660" y="54" text-anchor="middle" fill="#6a7889" font-size="11" font-family="monospace">协议栈</text>
      <line x1="255" y1="40" x2="255" y2="510" stroke="#30363d" stroke-width="1" stroke-dasharray="4,4"/>
      <line x1="525" y1="40" x2="525" y2="510" stroke="#30363d" stroke-width="1" stroke-dasharray="4,4"/>
      <!-- NIC -->
      <rect x="50" y="70" width="140" height="44" rx="8" fill="#1c2230" stroke="#f78166" stroke-width="1.5"/>
      <text x="120" y="87" text-anchor="middle" fill="#f78166" font-size="12" font-weight="600">网卡 NIC</text>
      <text x="120" y="103" text-anchor="middle" fill="#8b949e" font-size="10" font-family="monospace">DMA → Ring Buffer</text>
      <circle cx="210" cy="92" r="14" fill="#f78166" fill-opacity="0.18" stroke="#f78166" stroke-width="1.5"/>
      <text x="210" y="88" text-anchor="middle" fill="#f78166" font-size="9" font-weight="700">DROP</text>
      <text x="210" y="99" text-anchor="middle" fill="#f78166" font-size="8">①</text>
      <line x1="190" y1="92" x2="265" y2="92" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr)"/>
      <!-- NAPI -->
      <rect x="270" y="70" width="150" height="44" rx="8" fill="#1c2230" stroke="#d2a679" stroke-width="1.5"/>
      <text x="345" y="87" text-anchor="middle" fill="#d2a679" font-size="12" font-weight="600">NAPI SoftIRQ</text>
      <text x="345" y="103" text-anchor="middle" fill="#8b949e" font-size="10" font-family="monospace">netdev_max_backlog</text>
      <circle cx="440" cy="92" r="14" fill="#d2a679" fill-opacity="0.18" stroke="#d2a679" stroke-width="1.5"/>
      <text x="440" y="88" text-anchor="middle" fill="#d2a679" font-size="9" font-weight="700">DROP</text>
      <text x="440" y="99" text-anchor="middle" fill="#d2a679" font-size="8">②</text>
      <line x1="420" y1="92" x2="540" y2="92" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr)"/>
      <!-- netif_receive_skb -->
      <rect x="545" y="70" width="180" height="44" rx="8" fill="#1c2230" stroke="#58a6ff" stroke-width="1.5"/>
      <text x="635" y="87" text-anchor="middle" fill="#58a6ff" font-size="11" font-weight="600">netif_receive_skb</text>
      <text x="635" y="103" text-anchor="middle" fill="#8b949e" font-size="10" font-family="monospace">tc ingress / XDP</text>
      <line x1="635" y1="114" x2="635" y2="150" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr)"/>
      <!-- ARP -->
      <rect x="545" y="155" width="180" height="44" rx="8" fill="#1c2230" stroke="#d2a679" stroke-width="1.5"/>
      <text x="635" y="172" text-anchor="middle" fill="#d2a679" font-size="12" font-weight="600">ARP 处理</text>
      <text x="635" y="188" text-anchor="middle" fill="#8b949e" font-size="10" font-family="monospace">/proc/net/stat/arp_cache</text>
      <circle cx="750" cy="177" r="14" fill="#d2a679" fill-opacity="0.18" stroke="#d2a679" stroke-width="1.5"/>
      <text x="750" y="173" text-anchor="middle" fill="#d2a679" font-size="9" font-weight="700">DROP</text>
      <text x="750" y="184" text-anchor="middle" fill="#d2a679" font-size="8">③</text>
      <line x1="635" y1="199" x2="635" y2="235" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr)"/>
      <!-- IP -->
      <rect x="545" y="240" width="180" height="44" rx="8" fill="#1c2230" stroke="#3fb950" stroke-width="1.5"/>
      <text x="635" y="257" text-anchor="middle" fill="#3fb950" font-size="12" font-weight="600">IP 层 (ip_rcv)</text>
      <text x="635" y="273" text-anchor="middle" fill="#8b949e" font-size="10" font-family="monospace">/proc/net/snmp Ip:*</text>
      <circle cx="750" cy="262" r="14" fill="#3fb950" fill-opacity="0.12" stroke="#3fb950" stroke-width="1.5"/>
      <text x="750" y="258" text-anchor="middle" fill="#3fb950" font-size="9" font-weight="700">DROP</text>
      <text x="750" y="269" text-anchor="middle" fill="#3fb950" font-size="8">④</text>
      <line x1="635" y1="284" x2="635" y2="320" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr)"/>
      <!-- Netfilter -->
      <rect x="545" y="325" width="180" height="44" rx="8" fill="#1c2230" stroke="#e3b341" stroke-width="1.5"/>
      <text x="635" y="342" text-anchor="middle" fill="#e3b341" font-size="12" font-weight="600">Netfilter (iptables)</text>
      <text x="635" y="358" text-anchor="middle" fill="#8b949e" font-size="10" font-family="monospace">nf_conntrack / rules</text>
      <circle cx="750" cy="347" r="14" fill="#e3b341" fill-opacity="0.12" stroke="#e3b341" stroke-width="1.5"/>
      <text x="750" y="343" text-anchor="middle" fill="#e3b341" font-size="9" font-weight="700">DROP</text>
      <text x="750" y="354" text-anchor="middle" fill="#e3b341" font-size="8">⑤</text>
      <line x1="635" y1="369" x2="635" y2="405" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr)"/>
      <!-- TCP/UDP -->
      <rect x="545" y="410" width="180" height="44" rx="8" fill="#1c2230" stroke="#bc8cff" stroke-width="1.5"/>
      <text x="635" y="427" text-anchor="middle" fill="#bc8cff" font-size="12" font-weight="600">TCP / UDP</text>
      <text x="635" y="443" text-anchor="middle" fill="#8b949e" font-size="10" font-family="monospace">/proc/net/snmp Tcp/Udp</text>
      <circle cx="750" cy="432" r="14" fill="#bc8cff" fill-opacity="0.12" stroke="#bc8cff" stroke-width="1.5"/>
      <text x="750" y="428" text-anchor="middle" fill="#bc8cff" font-size="9" font-weight="700">DROP</text>
      <text x="750" y="439" text-anchor="middle" fill="#bc8cff" font-size="8">⑥</text>
      <line x1="635" y1="454" x2="635" y2="490" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr)"/>
      <rect x="545" y="490" width="180" height="20" rx="6" fill="#21283a" stroke="#30363d" stroke-width="1"/>
      <text x="635" y="505" text-anchor="middle" fill="#6a7889" font-size="11" font-family="monospace">应用程序 Socket Buffer</text>
      <!-- Legend -->
      <rect x="30" y="160" width="210" height="310" rx="8" fill="#12161e" stroke="#21283a" stroke-width="1"/>
      <text x="135" y="182" text-anchor="middle" fill="#58a6ff" font-size="12" font-weight="600">丢包点说明</text>
      <circle cx="55" cy="205" r="8" fill="#f78166" fill-opacity="0.2" stroke="#f78166" stroke-width="1"/>
      <text x="55" y="209" text-anchor="middle" fill="#f78166" font-size="9">①</text>
      <text x="75" y="202" fill="#c9d1d9" font-size="11">Ring Buffer 溢出</text>
      <text x="75" y="215" fill="#6a7889" font-size="10" font-family="monospace">ethtool -S / rx_missed</text>
      <circle cx="55" cy="240" r="8" fill="#d2a679" fill-opacity="0.2" stroke="#d2a679" stroke-width="1"/>
      <text x="55" y="244" text-anchor="middle" fill="#d2a679" font-size="9">②</text>
      <text x="75" y="237" fill="#c9d1d9" font-size="11">Backlog 队列溢出</text>
      <text x="75" y="250" fill="#6a7889" font-size="10" font-family="monospace">softnet_stat col2</text>
      <circle cx="55" cy="275" r="8" fill="#d2a679" fill-opacity="0.2" stroke="#d2a679" stroke-width="1"/>
      <text x="55" y="279" text-anchor="middle" fill="#d2a679" font-size="9">③</text>
      <text x="75" y="272" fill="#c9d1d9" font-size="11">ARP 表满 / 队列丢弃</text>
      <text x="75" y="285" fill="#6a7889" font-size="10" font-family="monospace">arp_cache unres_discards</text>
      <circle cx="55" cy="310" r="8" fill="#3fb950" fill-opacity="0.15" stroke="#3fb950" stroke-width="1"/>
      <text x="55" y="314" text-anchor="middle" fill="#3fb950" font-size="9">④</text>
      <text x="75" y="307" fill="#c9d1d9" font-size="11">IP 头错误/路由缺失</text>
      <text x="75" y="320" fill="#6a7889" font-size="10" font-family="monospace">IpInHdrErrors etc.</text>
      <circle cx="55" cy="345" r="8" fill="#e3b341" fill-opacity="0.15" stroke="#e3b341" stroke-width="1"/>
      <text x="55" y="349" text-anchor="middle" fill="#e3b341" font-size="9">⑤</text>
      <text x="75" y="342" fill="#c9d1d9" font-size="11">防火墙规则/conntrack满</text>
      <text x="75" y="355" fill="#6a7889" font-size="10" font-family="monospace">nf_conntrack_count</text>
      <circle cx="55" cy="380" r="8" fill="#bc8cff" fill-opacity="0.15" stroke="#bc8cff" stroke-width="1"/>
      <text x="55" y="384" text-anchor="middle" fill="#bc8cff" font-size="9">⑥</text>
      <text x="75" y="377" fill="#c9d1d9" font-size="11">Socket Buffer 满</text>
      <text x="75" y="390" fill="#6a7889" font-size="10" font-family="monospace">ListenDrops/UdpRcvbuf</text>
    </svg>
  </div>

  <div class="summary-box">
    <h4>&#128204; 第一性原理概述</h4>
    <ul>
      <li>每个数据包都是一个 <strong>sk_buff (SKB)</strong> 内核结构，在协议栈中从底层向上传递，任何一层资源不足或规则匹配均可导致该 SKB 被释放（丢弃）</li>
      <li>丢包本质：<strong>生产速率 &gt; 消费速率</strong> 或 <strong>数据不合法</strong> 或 <strong>规则显式拒绝</strong></li>
      <li>内核通过 <code>/proc/net/snmp</code>、<code>/proc/net/netstat</code>、<code>/proc/net/softnet_stat</code> 等接口暴露各层计数器</li>
    </ul>
  </div>
</section>

<!-- Section 2: Interfaces -->
<section class="section" id="interfaces">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(63,185,80,0.12)">&#128450;</div>
    <div><div class="section-num">Section 02</div><div class="section-title">核心接口速查</div></div>
  </div>
  <p>以下为排查网络丢包时最常用的内核原生接口，来源：<a href="https://www.kernel.org/doc/html/v6.3/networking/statistics.html" target="_blank">Interface statistics — The Linux Kernel documentation</a> 及 <a href="https://docs.kernel.org/networking/snmp_counter.html" target="_blank">SNMP counter — The Linux Kernel documentation</a>。</p>
  <div class="table-wrap">
    <table>
      <thead><tr><th>接口路径</th><th>层次</th><th>主要用途</th></tr></thead>
      <tbody>
        <tr><td>/proc/net/dev</td><td>L2</td><td>接口级 RX/TX 包数、错误、丢弃统计</td></tr>
        <tr><td>/sys/class/net/&lt;if&gt;/statistics/</td><td>L2</td><td>sysfs 接口，同 rtnl_link_stats64 结构体成员</td></tr>
        <tr><td>/proc/net/softnet_stat</td><td>L2.5</td><td>每 CPU 的软中断收包统计、backlog 丢弃</td></tr>
        <tr><td>/proc/net/stat/arp_cache</td><td>L2/3</td><td>ARP 邻居表统计，包括未解析丢弃</td></tr>
        <tr><td>/proc/net/snmp</td><td>L3/4</td><td>RFC1213 SNMP MIB：Ip、Icmp、Tcp、Udp 计数器</td></tr>
        <tr><td>/proc/net/netstat</td><td>L4</td><td>扩展 TCP 统计：ListenOverflows、ListenDrops 等</td></tr>
        <tr><td>/proc/net/sockstat</td><td>L4</td><td>各协议 Socket 总量与内存使用</td></tr>
        <tr><td>/proc/sys/net/netfilter/nf_conntrack_count</td><td>FW</td><td>当前连接跟踪表条目数</td></tr>
        <tr><td>/proc/sys/net/netfilter/nf_conntrack_max</td><td>FW</td><td>连接跟踪表最大容量</td></tr>
        <tr><td>ethtool -S &lt;if&gt;</td><td>L1/2</td><td>网卡驱动级统计，含 Ring Buffer 溢出</td></tr>
        <tr><td>ethtool -g &lt;if&gt;</td><td>L1/2</td><td>查看/修改 RX/TX Ring Buffer 大小</td></tr>
        <tr><td>ip -s link show</td><td>L2</td><td>接口统计（同 /proc/net/dev）</td></tr>
        <tr><td>ss / netstat -s</td><td>L4</td><td>读取 /proc/net/snmp 的人类友好输出</td></tr>
        <tr><td>nstat -a</td><td>L3/4</td><td>SNMP 计数器差值显示，排查利器</td></tr>
      </tbody>
    </table>
  </div>
  <div class="info-note info-box">
    <p>&#128216; <strong>提示</strong>：<code>nstat</code> 命令相比 <code>netstat -s</code> 更适合排查，它默认显示自上次调用以来的<strong>差值</strong>（增量），而不是累计值，可以清晰观察到计数器是否在增长。</p>
  </div>
  <pre><code># 一行命令：将 /proc/net/netstat 格式化为 key=value，过滤非零值
cat /proc/net/netstat | awk '(f==0) { i=1; while (i&lt;=NF) {n[i]=$i; i++}; f=1; next} \
  (f==1){ i=2; while (i&lt;=NF){ printf "%s = %d\n", n[i], $i; i++}; f=0}' | grep -v "= 0"</code></pre>
  <div class="summary-box">
    <h4>&#128204; 本节小结</h4>
    <ul>
      <li>所有计数器数据最终来源于内核 <code>net/ipv4/proc.c</code>，随内核启动时初始化</li>
      <li><code>/proc/net/snmp</code> 是 <code>netstat -s</code> 的数据源，可直接 <code>awk</code> 解析</li>
      <li><code>/sys/class/net/&lt;if&gt;/statistics/</code> 是 <code>rtnl_link_stats64</code> 结构体的 sysfs 映射</li>
    </ul>
  </div>
</section>

<!-- Section 3: NIC -->
<section class="section" id="nic">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(247,129,102,0.12)">&#128268;</div>
    <div><div class="section-num">Section 03</div><div class="section-title">网卡（NIC）&amp; Ring Buffer 丢包</div></div>
  </div>
  <div class="layer-pills">
    <span class="layer-pill badge-nic">物理层 / 数据链路层</span>
    <span class="layer-pill badge-nic">驱动层</span>
  </div>
  <p>网卡接收数据包时，通过 DMA 将数据写入预分配的 <strong>Ring Buffer（环形缓冲区）</strong>。当 Ring Buffer 满而 CPU 来不及处理时，新到达的包将被网卡直接丢弃。来源：<a href="https://www.kernel.org/doc/html/v6.3/networking/statistics.html" target="_blank">Interface statistics — The Linux Kernel documentation</a>，关键字段 <code>rx_missed_errors</code>：<em>Count of packets missed by the host. Counts number of packets dropped by the device due to lack of buffer space.</em></p>
  <h3>排查命令</h3>
  <div class="card">
    <div class="card-title"><span class="badge badge-nic">ethtool</span> 查看网卡统计与 Ring Buffer</div>
    <pre><code># 查看网卡 RX/TX Ring Buffer 当前配置与最大值
ethtool -g eth0
# Pre-set maximums: RX: 4096
# Current hardware settings: RX: 256  &lt;-- 实际大小，可调大

# 查看驱动级别统计（过滤 drop/miss/err/over 等关键字）
ethtool -S eth0 | grep -E "(err|drop|over|miss|timeout|reset|collis)" | grep -v ": 0"

# 扩大 Ring Buffer（临时）
ethtool -G eth0 rx 4096 tx 4096</code></pre>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-nic">/proc &amp; ip</span> 接口层统计</div>
    <pre><code># 查看接口统计（RX missed = Ring Buffer 溢出）
ip -s -s link show eth0

# sysfs 查看单个指标
cat /sys/class/net/eth0/statistics/rx_missed_errors
cat /sys/class/net/eth0/statistics/rx_dropped
cat /sys/class/net/eth0/statistics/tx_dropped</code></pre>
  </div>
  <h3>关键字段说明（内核结构 rtnl_link_stats64）</h3>
  <div class="table-wrap">
    <table>
      <thead><tr><th>字段</th><th>含义</th><th>来源（内核文档）</th></tr></thead>
      <tbody>
        <tr><td>rx_missed_errors</td><td>因 Ring Buffer 无空间而被网卡丢弃的包数</td><td>rtnl_link_stats64</td></tr>
        <tr><td>rx_dropped</td><td>因无资源或不支持协议而丢弃，L2 地址过滤等</td><td>rtnl_link_stats64</td></tr>
        <tr><td>rx_over_errors</td><td>接收 FIFO 溢出事件计数（高速网卡 = 一般丢包计数器）</td><td>rtnl_link_stats64</td></tr>
        <tr><td>rx_errors</td><td>所有接收错误的汇总</td><td>rtnl_link_stats64</td></tr>
        <tr><td>rx_crc_errors</td><td>CRC 校验失败的包数（物理链路问题）</td><td>rtnl_link_stats64</td></tr>
        <tr><td>rx_nohandler</td><td>设备未启用/备用链路（如 Bond backup）时被丢弃</td><td>rtnl_link_stats64</td></tr>
      </tbody>
    </table>
  </div>
  <div class="diagram">
    <div class="diagram-title">图 2：Ring Buffer 工作原理与溢出丢包</div>
    <svg viewBox="0 0 680 180" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:680px;background:#0d1117;border:1px solid #30363d;border-radius:8px">
      <defs><marker id="arr2" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0,0 L0,6 L8,3 z" fill="#30363d"/></marker></defs>
      <text x="340" y="20" text-anchor="middle" fill="#8b949e" font-size="11" font-family="monospace">NIC Ring Buffer（环形队列）</text>
      <rect x="60" y="36" width="52" height="50" rx="4" fill="#1c2c1a" stroke="#3fb950" stroke-width="1.5"/>
      <text x="86" y="66" text-anchor="middle" fill="#3fb950" font-size="9" font-family="monospace">pkts</text>
      <rect x="118" y="36" width="52" height="50" rx="4" fill="#1c2c1a" stroke="#3fb950" stroke-width="1.5"/>
      <text x="144" y="66" text-anchor="middle" fill="#3fb950" font-size="9" font-family="monospace">pkts</text>
      <rect x="176" y="36" width="52" height="50" rx="4" fill="#1c2c1a" stroke="#3fb950" stroke-width="1.5"/>
      <text x="202" y="66" text-anchor="middle" fill="#3fb950" font-size="9" font-family="monospace">pkts</text>
      <rect x="234" y="36" width="52" height="50" rx="4" fill="#1c2c1a" stroke="#3fb950" stroke-width="1.5"/>
      <text x="260" y="66" text-anchor="middle" fill="#3fb950" font-size="9" font-family="monospace">pkts</text>
      <rect x="292" y="36" width="52" height="50" rx="4" fill="#161b22" stroke="#21283a" stroke-width="1"/>
      <text x="318" y="66" text-anchor="middle" fill="#21283a" font-size="9">free</text>
      <rect x="350" y="36" width="52" height="50" rx="4" fill="#161b22" stroke="#21283a" stroke-width="1"/>
      <text x="376" y="66" text-anchor="middle" fill="#21283a" font-size="9">free</text>
      <rect x="408" y="36" width="52" height="50" rx="4" fill="#2c1a1a" stroke="#f78166" stroke-width="1.5" stroke-dasharray="4,2"/>
      <text x="434" y="59" text-anchor="middle" fill="#f78166" font-size="9" font-weight="700">FULL</text>
      <text x="434" y="74" text-anchor="middle" fill="#f78166" font-size="8">溢出!</text>
      <text x="40" y="62" text-anchor="middle" fill="#6a7889" font-size="10">RX</text>
      <text x="86" y="106" text-anchor="middle" fill="#3fb950" font-size="10">已写入</text>
      <text x="260" y="106" text-anchor="middle" fill="#3fb950" font-size="10">待消费</text>
      <text x="376" y="106" text-anchor="middle" fill="#30363d" font-size="10">空闲</text>
      <text x="434" y="106" text-anchor="middle" fill="#f78166" font-size="10">新包丢弃</text>
      <circle cx="530" cy="61" r="22" fill="#f78166" fill-opacity="0.12" stroke="#f78166" stroke-width="1.5"/>
      <text x="530" y="58" text-anchor="middle" fill="#f78166" font-size="12">&#128230;</text>
      <text x="530" y="73" text-anchor="middle" fill="#f78166" font-size="9" font-weight="700">DROP</text>
      <line x1="460" y1="61" x2="506" y2="61" stroke="#f78166" stroke-width="1.5" stroke-dasharray="4,2" marker-end="url(#arr2)"/>
      <rect x="555" y="36" width="110" height="50" rx="6" fill="#1c2230" stroke="#30363d" stroke-width="1"/>
      <text x="610" y="56" text-anchor="middle" fill="#58a6ff" font-size="11" font-family="monospace">ethtool -S</text>
      <text x="610" y="70" text-anchor="middle" fill="#8b949e" font-size="10">rx_missed</text>
      <text x="610" y="82" text-anchor="middle" fill="#8b949e" font-size="10">rx_no_buffer</text>
      <text x="230" y="138" text-anchor="middle" fill="#8b949e" font-size="11">NAPI 消费 (SoftIRQ 定期排空)</text>
      <text x="230" y="153" text-anchor="middle" fill="#6a7889" font-size="10" font-family="monospace">ethtool -G eth0 rx 4096  # 调大可缓解</text>
      <line x1="120" y1="126" x2="340" y2="126" stroke="#21283a" stroke-width="1" marker-end="url(#arr2)"/>
    </svg>
  </div>
  <div class="summary-box">
    <h4>&#128204; 本层小结</h4>
    <ul>
      <li>Ring Buffer 丢包是最底层丢包，发生在网卡 DMA 写入时，此时数据包尚未进入内核协议栈</li>
      <li>主要指标：<code>ethtool -S</code> 中的 <code>rx_missed</code>、<code>rx_no_buffer</code>；以及 <code>/sys/class/net/eth0/statistics/rx_missed_errors</code></li>
      <li>调优手段：<code>ethtool -G eth0 rx 4096</code>（增大 Ring Buffer）；开启 RSS 多队列；绑定 IRQ 亲和性</li>
      <li>第一性原理：NIC 写速率 &gt; CPU 消费速率 → 队列满 → 硬件静默丢弃</li>
    </ul>
  </div>
</section>

<!-- Section 4: SoftIRQ -->
<section class="section" id="softirq">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(210,166,121,0.12)">&#9889;</div>
    <div><div class="section-num">Section 04</div><div class="section-title">SoftIRQ &amp; netdev Backlog 丢包</div></div>
  </div>
  <div class="layer-pills"><span class="layer-pill badge-nic">内核 NAPI 层</span></div>
  <p>NAPI（New API）是 Linux 内核的中断合并机制。NIC 触发硬件中断后，内核切换到轮询模式，通过软中断 <code>NET_RX_SOFTIRQ</code> 批量处理数据包。每个 CPU 维护一个 <strong>softnet backlog 队列</strong>，当队列满时发生丢包。</p>
  <p>来源：<a href="https://docs.rockylinux.org/10/guides/network/performance_tuning/network_performance_irq_tuning/" target="_blank">IRQs and kernel packet drops — Rocky Linux Documentation</a>，<a href="https://access.redhat.com/sites/default/files/attachments/20150325_network_performance_tuning.pdf" target="_blank">Red Hat Enterprise Linux Network Performance Tuning Guide</a>。</p>
  <h3>/proc/net/softnet_stat 详解</h3>
  <p>每行对应一个 CPU 核心（从 CPU0 开始），所有字段均为 16 进制：</p>
  <div class="table-wrap">
    <table>
      <thead><tr><th>列号</th><th>字段名</th><th>含义</th><th>丢包关联</th></tr></thead>
      <tbody>
        <tr><td>col 1</td><td>total</td><td>该 CPU 处理的总包数</td><td>—</td></tr>
        <tr><td>col 2</td><td>dropped</td><td>backlog 队列满而丢弃的包数 ⚠️</td><td>✅ 关键丢包指标</td></tr>
        <tr><td>col 3</td><td>time_squeeze</td><td>NAPI budget/时间耗尽而提前退出次数</td><td>⚠️ 丢包前兆</td></tr>
        <tr><td>col 9</td><td>cpu_collision</td><td>发送时获取设备锁冲突次数</td><td>—</td></tr>
        <tr><td>col 10</td><td>received_rps</td><td>CPU 被 RPS 唤醒的次数</td><td>—</td></tr>
        <tr><td>col 11</td><td>flow_limit_count</td><td>达到流量限制次数</td><td>⚠️</td></tr>
      </tbody>
    </table>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-nic">/proc</span> 监控 SoftIRQ 丢包</div>
    <pre><code># 查看原始输出（十六进制，每行=一个CPU）
cat /proc/net/softnet_stat

# 格式化为人类可读（显示 processed/dropped/time_squeeze）
awk '{printf "CPU%-4d processed=%-12d dropped=%-8d time_squeeze=%d\n", \
  NR-1, strtonum("0x"$1), strtonum("0x"$2), strtonum("0x"$3)}' \
  /proc/net/softnet_stat</code></pre>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-nic">sysctl</span> 调整 NAPI 参数</div>
    <pre><code># 查看 backlog 队列大小（当 col2 dropped 增长时需调大）
sysctl net.core.netdev_max_backlog
# 默认值 1000，高吞吐场景建议 5000~16384
sysctl -w net.core.netdev_max_backlog=5000

# NAPI 每次轮询最大包数（默认 300）
sysctl net.core.netdev_budget
sysctl -w net.core.netdev_budget=600

# NAPI 每次轮询最大时间（微秒，默认 2000）
sysctl net.core.netdev_budget_usecs

# 查看 IRQ 亲和性（防止所有中断打到同一 CPU）
for irq in $(grep eth0 /proc/interrupts | awk '{print $1}' | tr -d ':'); do
  echo "IRQ $irq -&gt; CPU $(cat /proc/irq/$irq/smp_affinity_list)"
done</code></pre>
  </div>
  <div class="info-warn info-box">
    <p>&#9888;&#65039; <strong>注意</strong>：<code>time_squeeze</code>（col 3）非零说明 CPU 在 budget 耗尽前未处理完所有包，是 Ring Buffer 溢出的前兆——应在增大 <code>netdev_budget</code> 同时检查 CPU 核心的 IRQ 亲和性配置，避免单核过载。</p>
  </div>
  <div class="summary-box">
    <h4>&#128204; 本层小结</h4>
    <ul>
      <li><code>/proc/net/softnet_stat</code> 第 2 列（dropped）持续增长 → <code>netdev_max_backlog</code> 队列满 → 增大该参数</li>
      <li>第 3 列（time_squeeze）持续增长 → NAPI 预算不足 → 增大 <code>netdev_budget</code> 或优化 IRQ 绑定</li>
      <li>通过 <code>/proc/irq/&lt;N&gt;/smp_affinity</code> 分散 IRQ 到多核，避免单核成为瓶颈</li>
      <li>第一性原理：NAPI 每次只能消费固定 budget 数量的包，突发流量超出 budget 时，剩余包留在 Ring Buffer，若 Ring Buffer 也满则从硬件级别丢弃</li>
    </ul>
  </div>
</section>

<!-- Section 5: ARP -->
<section class="section" id="arp">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(210,166,121,0.12)">&#128225;</div>
    <div><div class="section-num">Section 05</div><div class="section-title">ARP 层丢包</div></div>
  </div>
  <div class="layer-pills"><span class="layer-pill badge-arp">链路层 / 网络层边界</span></div>
  <p>ARP（地址解析协议）负责将 IP 地址映射为 MAC 地址。Linux 内核维护一个 <strong>Neighbor Table（邻居表）</strong>来缓存这些映射。当邻居表满或 ARP 请求等待超时时，会发生数据包丢弃。</p>
  <p>来源：<a href="https://www.kernel.org/doc/Documentation/networking/ip-sysctl.txt" target="_blank">ip-sysctl.txt — kernel.org</a>。gc_thresh 三档阈值文档：<em>gc_thresh1: Minimum number of entries; gc_thresh2: Soft max; gc_thresh3: Hard max (default 1024)。</em></p>
  <div class="card">
    <div class="card-title">① <span class="badge badge-arp">ARP 表溢出</span> 邻居表满导致新条目无法添加</div>
    <p>当需要与大量主机通信时（如大规模容器场景），ARP 表可能达到上限，内核日志出现 <code>neighbour: arp_cache: neighbor table overflow!</code>。</p>
    <pre><code># 查看 ARP 表溢出统计（table_fulls 列）
cat /proc/net/stat/arp_cache

# 查看内核日志
dmesg | grep -i 'neighbour'

# 查看当前 ARP 表条目数
ip neigh show | wc -l

# 查看 ARP 表容量阈值（gc_thresh1/2/3）
sysctl -a | grep net.ipv4.neigh.default.gc_thresh
# gc_thresh1=128：低于此值不触发 GC（默认128）
# gc_thresh2=512：软上限，超过 5 秒后开始回收（默认512）
# gc_thresh3=1024：硬上限，立即触发 GC（默认1024）

# 调大 ARP 表（用于大规模容器/VM 环境）
sysctl -w net.ipv4.neigh.default.gc_thresh1=1024
sysctl -w net.ipv4.neigh.default.gc_thresh2=2048
sysctl -w net.ipv4.neigh.default.gc_thresh3=4096</code></pre>
  </div>
  <div class="card">
    <div class="card-title">② <span class="badge badge-arp">未解析队列丢包</span> ARP 请求尚未返回时的数据包缓存溢出</div>
    <p>ARP 缓存有效期约 10 分钟。当缓存过期或首次访问新 IP 时，内核发出 ARP 请求并将用户数据包缓存到 <code>arp_queue</code>（默认最多 3 个），超出部分直接丢弃。</p>
    <pre><code># 查看未解析丢弃统计（unresolved_discards 列）
cat /proc/net/stat/arp_cache

# 调整未解析队列大小
sysctl net.ipv4.neigh.default.unres_qlen_bytes
sysctl -w net.ipv4.neigh.default.unres_qlen_bytes=65536

# 查看当前邻居表状态
ip neigh show
# INCOMPLETE = 正在等待 ARP 响应（此时用户包进入 arp_queue）
# REACHABLE  = 可达，正常
# STALE      = 需要验证</code></pre>
  </div>
  <div class="info-tip info-box">
    <p>&#128161; <code>arp_ignore</code> 和 <code>arp_filter</code> 两个参数在多网卡系统中可能导致 ARP 响应异常，间接引起丢包。<code>arp_ignore=1</code> 只响应到达接口的 ARP 请求；<code>arp_filter=1</code> 检查反向路径，多接口负载均衡时需要关注。</p>
  </div>
  <div class="summary-box">
    <h4>&#128204; 本层小结</h4>
    <ul>
      <li>监控 <code>/proc/net/stat/arp_cache</code> 的 <code>table_fulls</code>（表溢出）和 <code>unresolved_discards</code>（未解析丢弃）</li>
      <li>ARP 表满：调整 <code>gc_thresh1/2/3</code>；未解析丢弃：增大 <code>unres_qlen_bytes</code></li>
      <li><code>dmesg | grep neighbour</code> 是最直接的 ARP 异常告警方式</li>
      <li>第一性原理：ARP 本质是 IP→MAC 的缓存查询，表容量有限，缓存未命中时需等待网络 RTT，期间包暂存于队列中，队列有限则丢弃</li>
    </ul>
  </div>
</section>

<!-- Section 6: IP -->
<section class="section" id="ip">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(63,185,80,0.12)">&#127760;</div>
    <div><div class="section-num">Section 06</div><div class="section-title">IP 层丢包</div></div>
  </div>
  <div class="layer-pills"><span class="layer-pill badge-ip">网络层 L3</span></div>
  <p>IP 层是 Linux 网络栈的核心路由决策层。丢包发生在 IP 头校验、路由查找、分片重组等环节。所有 IP 层计数器通过 <code>/proc/net/snmp</code> 暴露，符合 <a href="https://tools.ietf.org/html/rfc1213" target="_blank">RFC 1213</a>。</p>
  <p>来源：<a href="https://docs.kernel.org/networking/snmp_counter.html" target="_blank">SNMP counter — The Linux Kernel documentation</a>。</p>
  <h3>IP 层关键计数器</h3>
  <div class="table-wrap">
    <table>
      <thead><tr><th>计数器</th><th>丢包场景</th><th>RFC 1213</th></tr></thead>
      <tbody>
        <tr><td>IpInReceives</td><td>IP 层收到的总包数（包括后续被丢弃的）</td><td>ipInReceives</td></tr>
        <tr><td>IpInHdrErrors</td><td>IP 头校验失败（TTL=0、校验和错误等）</td><td>ipInHdrErrors</td></tr>
        <tr><td>IpInAddrErrors</td><td>目标 IP 无效 或 IP 转发未启用时的路由丢弃</td><td>ipInAddrErrors</td></tr>
        <tr><td>IpExtInNoRoutes</td><td>IP 转发启用但路由表无匹配条目</td><td>Linux 扩展</td></tr>
        <tr><td>IpInUnknownProtos</td><td>不支持的 L4 协议</td><td>ipInUnknownProtos</td></tr>
        <tr><td>IpExtInTruncatedPkts</td><td>实际数据长度小于 IP 头部 Total Length 字段</td><td>Linux 扩展</td></tr>
        <tr><td>IpInDiscards</td><td>内核内部原因丢弃（如内存不足）</td><td>ipInDiscards</td></tr>
        <tr><td>IpOutDiscards</td><td>发送路径内核内部原因丢弃</td><td>ipOutDiscards</td></tr>
        <tr><td>IpOutNoRoutes</td><td>发送时找不到路由</td><td>ipOutNoRoutes</td></tr>
        <tr><td>IpInDelivers</td><td>成功交付给 L4 的包数</td><td>ipInDelivers</td></tr>
      </tbody>
    </table>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-ip">/proc/net/snmp</span> 排查命令</div>
    <pre><code># 查看 IP 层所有计数器
nstat -az | grep ^Ip

# 快速查看非零的 IP 丢包计数器
netstat -s | grep -A 30 "Ip:"

# 监控 IP 头错误（持续增加 = 物理或上游问题）
watch -d 'nstat | grep -E "IpIn(HdrErr|AddrErr|Discard|UnknownProto)"'</code></pre>
  </div>
  <h3>rp_filter（反向路径过滤）丢包</h3>
  <pre><code># 查看 rp_filter 设置（0=关闭，1=严格，2=宽松）
sysctl -a | grep rp_filter
# 若多路由场景需要宽松模式：
sysctl -w net.ipv4.conf.all.rp_filter=2
sysctl -w net.ipv4.conf.eth0.rp_filter=2

# rp_filter 丢包会体现在 IpExtInNoRoutes 计数器增长
nstat -az | grep IpExt</code></pre>
  <div class="summary-box">
    <h4>&#128204; 本层小结</h4>
    <ul>
      <li><code>IpInHdrErrors</code> 增长：数据包本身损坏，检查物理链路或上游设备</li>
      <li><code>IpInAddrErrors</code> 增长：路由配置问题，检查 IP 转发开关（<code>net.ipv4.ip_forward</code>）</li>
      <li><code>IpExtInNoRoutes</code> / <code>IpOutNoRoutes</code>：路由表缺少条目，<code>ip route show</code> 检查</li>
      <li><code>rp_filter</code> 在非对称路由场景下需设为 2（宽松模式）</li>
      <li>第一性原理：IP 层做两件事——头部合法性校验 和 路由决策，任一失败即丢包</li>
    </ul>
  </div>
</section>

<!-- Section 7: Firewall -->
<section class="section" id="firewall">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(227,179,65,0.12)">&#128293;</div>
    <div><div class="section-num">Section 07</div><div class="section-title">Netfilter / iptables / conntrack 丢包</div></div>
  </div>
  <div class="layer-pills"><span class="layer-pill badge-fw">防火墙层</span></div>
  <p>Netfilter 是 Linux 内核的包过滤框架，iptables/nftables 是其用户态接口。<strong>连接跟踪（conntrack）</strong>表满是生产环境中常见的丢包根因，会在内核日志中出现 <code>nf_conntrack: table full, dropping packet</code>。</p>
  <p>来源：<a href="https://support.huaweicloud.com/intl/en-us/trouble-ecs/ecs_trouble_0324.html" target="_blank">Huawei Cloud — nf_conntrack table full</a>，<a href="https://www.cyberciti.biz/faq/ip_conntrack-table-ful-dropping-packet-error/" target="_blank">nixCraft ip_conntrack table full</a>。</p>
  <div class="card">
    <div class="card-title"><span class="badge badge-fw">/proc/sys/net/netfilter</span> 连接跟踪监控</div>
    <pre><code># 实时监控 conntrack 表使用情况
watch -n 1 'echo "Used: $(cat /proc/sys/net/netfilter/nf_conntrack_count) \
  / Max: $(cat /proc/sys/net/netfilter/nf_conntrack_max)"'

# 查看内核日志中的 conntrack 告警
dmesg | grep -i 'nf_conntrack'

# CONNTRACK_MAX = RAM(bytes) / 16384 / 2
# 64GB → 64*1024*1024*1024/16384/2 = 2097152

# 增大 conntrack 表
sysctl -w net.netfilter.nf_conntrack_max=1048576

# 减小 TCP 已建立连接超时（默认 432000 = 5天！）
sysctl -w net.netfilter.nf_conntrack_tcp_timeout_established=600

# 减小 UDP 超时
sysctl -w net.netfilter.nf_conntrack_udp_timeout=30
sysctl -w net.netfilter.nf_conntrack_generic_timeout=120</code></pre>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-fw">iptables</span> 规则导致的丢包</div>
    <pre><code># 查看 DROP/REJECT 规则
iptables -L -n -v | grep -E "(DROP|REJECT)"

# 查看 INVALID 状态包是否被丢弃
iptables -L -n -v | grep INVALID

# 旧接口查看 conntrack 表条目数
wc -l /proc/net/ip_conntrack

# 对不需要跟踪的流量添加 NOTRACK（减少 conntrack 压力）
iptables -t raw -A PREROUTING -p udp --dport 53 -j NOTRACK
iptables -t raw -A PREROUTING -p tcp --dport 22 -j NOTRACK</code></pre>
  </div>
  <div class="info-warn info-box">
    <p>&#9888;&#65039; 增大 <code>nf_conntrack_max</code> 时需注意内存消耗：每个 conntrack 条目约占 <strong>350 字节</strong>的不可换出内核内存（non-swappable kernel memory）。同步增大 hash 桶数量：<code>echo 262144 &gt; /sys/module/nf_conntrack/parameters/hashsize</code></p>
  </div>
  <div class="summary-box">
    <h4>&#128204; 本层小结</h4>
    <ul>
      <li>监控 <code>/proc/sys/net/netfilter/nf_conntrack_count</code> 接近 <code>nf_conntrack_max</code> 时需立即告警</li>
      <li>默认 TCP 连接超时 432000 秒（5天）在高并发场景极不合理，建议调至 600 秒</li>
      <li>对 DNS、监控等不需要追踪的 UDP 流量使用 <code>-j NOTRACK</code> 绕过 conntrack</li>
      <li><code>dmesg | grep 'nf_conntrack: table full'</code> 是 conntrack 丢包最直接的证据</li>
      <li>第一性原理：conntrack 为每个新连接分配有限哈希表槽位，表满时新连接无法记录，内核选择丢弃</li>
    </ul>
  </div>
</section>

<!-- Section 8: TCP -->
<section class="section" id="tcp">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(88,166,255,0.12)">&#128279;</div>
    <div><div class="section-num">Section 08</div><div class="section-title">TCP 层丢包</div></div>
  </div>
  <div class="layer-pills"><span class="layer-pill badge-tcp">传输层 L4</span></div>
  <p>TCP 丢包分为两类：<strong>连接建立阶段</strong>（SYN 队列/Accept 队列溢出）和<strong>数据传输阶段</strong>（接收缓冲区满、内存压力等）。所有计数器来自 <code>/proc/net/snmp</code>（TcpExt 部分）和 <code>/proc/net/netstat</code>。</p>
  <p>来源：<a href="https://docs.kernel.org/networking/snmp_counter.html" target="_blank">SNMP counter — The Linux Kernel documentation</a>（TcpExtListenOverflows、TcpExtListenDrops 等详细说明）。</p>
  <h3>连接建立阶段丢包（SYN &amp; Accept 队列）</h3>
  <div class="diagram">
    <div class="diagram-title">图 3：TCP SYN 队列与 Accept 队列</div>
    <svg viewBox="0 0 700 220" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:700px;background:#0d1117;border:1px solid #30363d;border-radius:8px">
      <defs><marker id="arr3" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0,0 L0,6 L8,3 z" fill="#30363d"/></marker></defs>
      <rect x="30" y="80" width="80" height="60" rx="6" fill="#1c2230" stroke="#30363d" stroke-width="1"/>
      <text x="70" y="107" text-anchor="middle" fill="#8b949e" font-size="11">Client</text>
      <text x="70" y="122" text-anchor="middle" fill="#6a7889" font-size="10">SYN →</text>
      <rect x="135" y="60" width="140" height="100" rx="8" fill="#1c2230" stroke="#d2a679" stroke-width="1.5"/>
      <text x="205" y="82" text-anchor="middle" fill="#d2a679" font-size="12" font-weight="600">SYN 半连接队列</text>
      <text x="205" y="97" text-anchor="middle" fill="#6a7889" font-size="10" font-family="monospace">tcp_max_syn_backlog</text>
      <rect x="148" y="105" width="22" height="20" rx="3" fill="#2c2010" stroke="#d2a679" stroke-width="1"/>
      <rect x="175" y="105" width="22" height="20" rx="3" fill="#2c2010" stroke="#d2a679" stroke-width="1"/>
      <rect x="202" y="105" width="22" height="20" rx="3" fill="#2c2010" stroke="#d2a679" stroke-width="1"/>
      <rect x="229" y="105" width="22" height="20" rx="3" fill="#161b22" stroke="#21283a" stroke-width="1"/>
      <rect x="256" y="105" width="22" height="20" rx="3" fill="#161b22" stroke="#21283a" stroke-width="1"/>
      <text x="205" y="145" text-anchor="middle" fill="#6a7889" font-size="10">SYN+ACK 已发出，等待 ACK</text>
      <line x1="278" y1="110" x2="318" y2="110" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr3)"/>
      <text x="298" y="103" text-anchor="middle" fill="#3fb950" font-size="9">ACK</text>
      <rect x="323" y="60" width="145" height="100" rx="8" fill="#1c2230" stroke="#58a6ff" stroke-width="1.5"/>
      <text x="395" y="82" text-anchor="middle" fill="#58a6ff" font-size="12" font-weight="600">Accept 全连接队列</text>
      <text x="395" y="97" text-anchor="middle" fill="#6a7889" font-size="10" font-family="monospace">somaxconn</text>
      <rect x="335" y="105" width="22" height="20" rx="3" fill="#1a2235" stroke="#58a6ff" stroke-width="1"/>
      <rect x="362" y="105" width="22" height="20" rx="3" fill="#1a2235" stroke="#58a6ff" stroke-width="1"/>
      <rect x="389" y="105" width="22" height="20" rx="3" fill="#1a2235" stroke="#58a6ff" stroke-width="1"/>
      <rect x="416" y="105" width="22" height="20" rx="3" fill="#161b22" stroke="#21283a" stroke-width="1"/>
      <rect x="443" y="105" width="22" height="20" rx="3" fill="#161b22" stroke="#21283a" stroke-width="1"/>
      <text x="395" y="145" text-anchor="middle" fill="#6a7889" font-size="10">3次握手完成，等待 accept()</text>
      <line x1="470" y1="110" x2="510" y2="110" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr3)"/>
      <text x="490" y="103" text-anchor="middle" fill="#6a7889" font-size="9">accept()</text>
      <rect x="514" y="80" width="80" height="60" rx="6" fill="#1c2230" stroke="#30363d" stroke-width="1"/>
      <text x="554" y="107" text-anchor="middle" fill="#8b949e" font-size="11">Server</text>
      <text x="554" y="122" text-anchor="middle" fill="#6a7869" font-size="10">App</text>
      <circle cx="205" cy="175" r="16" fill="#e3b341" fill-opacity="0.12" stroke="#e3b341" stroke-width="1.5"/>
      <text x="205" y="172" text-anchor="middle" fill="#e3b341" font-size="8" font-weight="700">SYN</text>
      <text x="205" y="183" text-anchor="middle" fill="#e3b341" font-size="8">DROP</text>
      <text x="205" y="197" text-anchor="middle" fill="#6a7889" font-size="9">tcp_max_syn_backlog 满</text>
      <circle cx="395" cy="175" r="16" fill="#f78166" fill-opacity="0.12" stroke="#f78166" stroke-width="1.5"/>
      <text x="395" y="172" text-anchor="middle" fill="#f78166" font-size="8" font-weight="700">SYN</text>
      <text x="395" y="183" text-anchor="middle" fill="#f78166" font-size="8">DROP</text>
      <text x="395" y="197" text-anchor="middle" fill="#6a7889" font-size="9">somaxconn 满</text>
      <line x1="113" y1="110" x2="132" y2="110" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr3)"/>
    </svg>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-tcp">/proc/net/snmp</span> TCP 连接队列丢包排查</div>
    <pre><code># 关键计数器：ListenOverflows（Accept队列满）和 ListenDrops（总监听丢弃）
nstat -az | grep -E "Listen(Overflow|Drop)"

# 查看 SYN 半连接队列大小上限
sysctl net.ipv4.tcp_max_syn_backlog

# 查看 Accept 全连接队列上限
sysctl net.core.somaxconn

# 调大（高并发服务）
sysctl -w net.ipv4.tcp_max_syn_backlog=8192
sysctl -w net.core.somaxconn=8192

# 查看各监听 Socket 的 Accept 队列使用情况
ss -lnt
# Send-Q = 队列容量，Recv-Q = 当前排队数</code></pre>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-tcp">接收缓冲区</span> TCP Socket 缓冲区满与连接中止</div>
    <pre><code># 查看 TCP 接收缓冲区因满而丢包
nstat -az | grep TcpExtTCPRcvQDrop

# 查看 TCP 读写缓冲区配置
sysctl net.ipv4.tcp_rmem   # 接收缓冲区 min/default/max
sysctl net.ipv4.tcp_wmem   # 发送缓冲区

# 调大缓冲区上限
sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sysctl -w net.core.rmem_max=16777216

# 查看各种连接中止原因
nstat -az | grep TcpExtTCPAbort
# TcpExtTCPAbortOnMemory：孤儿 Socket 超限，发送 RST
# TcpExtTCPAbortOnTimeout：定时器超时强制关闭
# TcpExtTCPAbortOnData：有未发送数据时强制关闭

# 查看重传统计
nstat -az | grep TcpRetransSegs</code></pre>
  </div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>计数器</th><th>含义（来源：kernel.org SNMP doc）</th></tr></thead>
      <tbody>
        <tr><td>TcpExtListenOverflows</td><td>Accept 队列满，内核丢弃 SYN（kernel ≥ 4.10 行为）</td></tr>
        <tr><td>TcpExtListenDrops</td><td>监听 Socket 丢包总数（包含内存分配失败等）</td></tr>
        <tr><td>TcpExtTCPRcvQDrop</td><td>接收队列满而丢弃的数据包</td></tr>
        <tr><td>TcpExtTCPAbortOnMemory</td><td>孤儿 Socket 超限，发送 RST 强制关闭</td></tr>
        <tr><td>TcpExtTCPAbortOnTimeout</td><td>各类定时器超时强制关闭连接</td></tr>
        <tr><td>TcpRetransSegs</td><td>重传段数（非丢包但间接反映网络质量）</td></tr>
        <tr><td>TCPSynRetrans</td><td>SYN 及 SYN/ACK 重传次数</td></tr>
      </tbody>
    </table>
  </div>
  <div class="summary-box">
    <h4>&#128204; 本层小结</h4>
    <ul>
      <li>连接建立丢包：监控 <code>TcpExtListenOverflows</code>，调大 <code>somaxconn</code> 和 <code>tcp_max_syn_backlog</code>，同时确保应用程序快速 <code>accept()</code></li>
      <li>数据接收丢包：监控 <code>TcpExtTCPRcvQDrop</code>，调大 <code>tcp_rmem</code> 和 <code>rmem_max</code></li>
      <li>内存压力：监控 <code>TcpExtTCPAbortOnMemory</code>，调大 <code>tcp_max_orphans</code> 或优化应用关闭连接方式</li>
      <li>第一性原理：TCP 是有状态协议，每个连接需要内核维护状态机和缓冲区，资源（队列/内存/连接数）耗尽即丢包</li>
    </ul>
  </div>
</section>

<!-- Section 9: UDP -->
<section class="section" id="udp">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(188,140,255,0.12)">&#128232;</div>
    <div><div class="section-num">Section 09</div><div class="section-title">UDP 层丢包</div></div>
  </div>
  <div class="layer-pills"><span class="layer-pill badge-udp">传输层 L4 无状态</span></div>
  <p>UDP 是无连接协议，内核不为 UDP 维护重传机制，因此一旦接收缓冲区满，新到达的数据包被内核直接静默丢弃。UDP 丢包是生产系统中最隐蔽的问题之一。</p>
  <p>来源：<a href="https://docs.kernel.org/networking/snmp_counter.html" target="_blank">SNMP counter — The Linux Kernel documentation</a>，<a href="https://elmagnifico.tech/2022/04/22/UDP-Lost-Packages/" target="_blank">UDP丢包分析 — elmagnifico.tech</a>。</p>
  <div class="card">
    <div class="card-title"><span class="badge badge-udp">/proc/net/snmp</span> UDP 丢包排查</div>
    <pre><code># 查看 UDP 层所有统计
nstat -az | grep -E "^Udp"
# UdpRcvbufErrors：接收缓冲区满而丢弃（最关键）
# UdpSndbufErrors：发送缓冲区满
# UdpInErrors：所有 UDP 接收错误
# UdpNoPorts：端口无对应进程（ICMP port unreachable）

# 用 netstat -s 查看 UDP receive errors
netstat -s | grep -A 10 "Udp:"
netstat -s | grep "receive buffer errors"

# 查看 UDP Socket 缓冲区使用情况
ss -unp
# Recv-Q 若持续非零 = 缓冲区积压，应用消费过慢

# 查看 UDP 缓冲区默认/最大值
sysctl net.core.rmem_default    # 默认接收缓冲区
sysctl net.core.rmem_max        # 最大接收缓冲区

# 调大 UDP 接收缓冲区
sysctl -w net.core.rmem_default=26214400
sysctl -w net.core.rmem_max=26214400

# skmem 信息：d=丢弃的数据报数量
ss -nump
# 示例：skmem:(r0,rb212992,t0,tb212992,f0,w0,o640,bl0,d0)</code></pre>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-udp">ARP+UDP</span> ARP 解析期间的 UDP 丢包</div>
    <p>ARP 缓存有效期约 10 分钟。当 ARP 过期时，用户发送的 UDP 包会被内核缓存到 <code>arp_queue</code>（默认最多 3 个），超出的 UDP 包直接被丢弃，通过 <code>/proc/net/stat/arp_cache</code> 的 <code>unresolved_discards</code> 可以观察到。</p>
    <pre><code># 查看 ARP 未解析期间的 UDP 丢包
cat /proc/net/stat/arp_cache | head -2
# 重要列：unresolved_discards（十六进制）

# 增大 ARP 解析等待队列
sysctl net.ipv4.neigh.default.unres_qlen_bytes
sysctl -w net.ipv4.neigh.default.unres_qlen_bytes=65536</code></pre>
  </div>
  <div class="table-wrap">
    <table>
      <thead><tr><th>计数器</th><th>含义</th><th>排查动作</th></tr></thead>
      <tbody>
        <tr><td>UdpRcvbufErrors</td><td>接收缓冲区满，新包被丢弃 ⚠️</td><td>增大 rmem_max，优化应用消费速率</td></tr>
        <tr><td>UdpSndbufErrors</td><td>发送缓冲区满（发送被阻塞）</td><td>增大 wmem_max</td></tr>
        <tr><td>UdpInErrors</td><td>所有 UDP 接收错误汇总</td><td>结合 UdpRcvbufErrors 分析</td></tr>
        <tr><td>UdpNoPorts</td><td>目标端口无对应进程</td><td>检查服务是否运行</td></tr>
        <tr><td>UdpInCsumErrors</td><td>UDP 校验和错误</td><td>检查物理链路或 ECMP 哈希</td></tr>
      </tbody>
    </table>
  </div>
  <div class="info-note info-box">
    <p>&#128216; 对于 TCP：发送缓冲区满只会使 <code>send()</code> 阻塞，不会丢包。而对于 UDP：接收缓冲区满会导致内核直接丢弃新包，<strong>无任何通知给应用程序</strong>，应用看到的是数据莫名缺失。</p>
  </div>
  <div class="summary-box">
    <h4>&#128204; 本层小结</h4>
    <ul>
      <li>核心指标：<code>UdpRcvbufErrors</code> 增长 → 缓冲区满 → 调大 <code>net.core.rmem_max</code></li>
      <li><code>ss -nump</code> 的 <code>skmem</code> 中 <code>d=</code> 字段直接显示该 Socket 丢弃计数</li>
      <li>高频 UDP 应用（监控、音视频）建议应用层使用独立线程快速消费缓冲区，避免积压</li>
      <li>ARP 解析期间的 UDP 丢包：监控 <code>/proc/net/stat/arp_cache</code> 中的 <code>unresolved_discards</code></li>
      <li>第一性原理：UDP 无流控无重传，内核接收缓冲区是唯一缓冲屏障，一旦消费慢于到达即丢弃</li>
    </ul>
  </div>
</section>

<!-- Section 10: SKB -->
<section class="section" id="skb">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(88,166,255,0.1)">&#128300;</div>
    <div><div class="section-num">Section 10</div><div class="section-title">SKB Drop Reason（内核 v5.17+）</div></div>
  </div>
  <div class="layer-pills">
    <span class="layer-pill badge-skb">Linux ≥ 5.17</span>
    <span class="layer-pill badge-skb">RHEL 8.8+ / 9.2+</span>
  </div>
  <p>Linux 5.17 引入 <code>kfree_skb_reason()</code> 机制（<a href="https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=c504e5c2f9648a1e5c2be01e8c3f59d394192bd3" target="_blank">commit c504e5c2f964</a>），允许开发者在释放 SKB 时附带原因码。通过 <code>skb:kfree_skb</code> tracepoint 可以获取人类可读的丢包原因，目前已支持 <strong>70+ 种丢包原因</strong>。</p>
  <p>来源：<a href="https://developers.redhat.com/articles/2023/07/19/how-retrieve-packet-drop-reasons-linux-kernel" target="_blank">How to retrieve packet drop reasons in the Linux kernel — Red Hat Developer</a>。</p>
  <div class="card">
    <div class="card-title"><span class="badge badge-skb">perf</span> 通过 tracepoint 获取精确丢包原因</div>
    <pre><code># 使用 perf 记录 skb:kfree_skb 事件
perf record -e skb:kfree_skb -a -- curl https://localhost
perf script
# 输出示例：
# curl 883 [001] 340.799: skb:kfree_skb:
#   skbaddr=0xffff88811f6a7068 protocol=2048
#   location=tcp_v4_rcv+0x157 reason: NO_SOCKET

# 统计 10 秒内所有丢包原因频次
perf record -e skb:kfree_skb -a sleep 10
perf script | awk '{print $NF}' | sort | uniq -c | sort -rn | head -20</code></pre>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-skb">dropwatch</span> 交互式丢包监控工具</div>
    <pre><code># 注意：需要访问内核地址（kptr_restrict 需为 0）
cat /proc/sys/kernel/kptr_restrict
echo 0 &gt; /proc/sys/kernel/kptr_restrict   # 调试完记得恢复！

# 启动 dropwatch 并开启详细模式
dropwatch -l kas
# dropwatch&gt; set alertmode packet
# dropwatch&gt; start
# 示例输出：
# drop at: tcp_v4_rcv+0x157/0x1630
# origin: software, ifindex: 1
# protocol: 0x800, drop reason: NO_SOCKET

# 完成后恢复
echo 1 &gt; /proc/sys/kernel/kptr_restrict</code></pre>
  </div>
  <div class="card">
    <div class="card-title"><span class="badge badge-skb">bpftrace</span> 灵活的 eBPF 丢包追踪</div>
    <pre><code># 附加 skb:kfree_skb 并输出丢包进程和原因
bpftrace -e 'tracepoint:skb:kfree_skb { printf("%s pid=%d reason=%d\n", comm, pid, args-&gt;reason) }'

# 统计各丢包原因的频次（10秒内）
bpftrace -e 'tracepoint:skb:kfree_skb { @[args-&gt;reason] = count() } interval:s:10 { print(@); clear(@) }'</code></pre>
  </div>
  <h3>常见 SKB Drop Reason 含义</h3>
  <div class="table-wrap">
    <table>
      <thead><tr><th>Drop Reason</th><th>含义</th><th>对应层</th></tr></thead>
      <tbody>
        <tr><td>NO_SOCKET</td><td>无对应 Socket（端口未监听）</td><td>TCP/UDP</td></tr>
        <tr><td>PKT_TOO_SMALL</td><td>包太小，低于协议最小长度</td><td>L4</td></tr>
        <tr><td>TCP_CSUM</td><td>TCP 校验和错误</td><td>TCP</td></tr>
        <tr><td>SOCKET_FILTER</td><td>被 BPF Socket 过滤器丢弃</td><td>Socket</td></tr>
        <tr><td>UDP_CSUM</td><td>UDP 校验和错误</td><td>UDP</td></tr>
        <tr><td>NETFILTER_DROP</td><td>Netfilter 规则丢弃</td><td>FW</td></tr>
        <tr><td>QDISC_DROP</td><td>流量控制队列丢弃</td><td>Egress</td></tr>
        <tr><td>XDP_DROP</td><td>XDP 程序决策丢弃</td><td>NIC Driver</td></tr>
        <tr><td>IP_NOPROTO</td><td>IP 层不支持的协议</td><td>IP</td></tr>
      </tbody>
    </table>
  </div>
  <div class="info-note info-box">
    <p>&#128216; SKB Drop Reason 的 enum 定义位于 <code>include/net/dropreason-core.h</code>，原始数字值不保证跨版本稳定，应优先使用 <code>perf</code> 或 <code>dropwatch</code> 获取文本形式的原因。</p>
  </div>
  <div class="summary-box">
    <h4>&#128204; 本节小结</h4>
    <ul>
      <li>SKB Drop Reason 是内核 v5.17 引入的精准丢包诊断机制，通过 <code>skb:kfree_skb</code> tracepoint 获取</li>
      <li>三种工具：<code>perf</code>（内置）、<code>dropwatch</code>（需 kptr_restrict=0）、<code>bpftrace</code>（最灵活）</li>
      <li>可将具体丢包包精准定位到内核函数和偏移量，是传统 MIB 计数器无法做到的</li>
      <li>当 MIB 计数器指向某层后，用 SKB Drop Reason 缩小到精确的代码路径</li>
    </ul>
  </div>
</section>

<!-- Section 11: Checklist -->
<section class="section" id="checklist">
  <div class="section-header">
    <div class="section-icon" style="background:rgba(63,185,80,0.12)">&#9989;</div>
    <div><div class="section-num">Section 11</div><div class="section-title">分层排查速查表</div></div>
  </div>

  <div class="diagram">
    <div class="diagram-title">图 4：丢包排查决策树</div>
    <svg viewBox="0 0 740 380" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:740px;background:#0d1117;border:1px solid #30363d;border-radius:8px">
      <defs><marker id="arr4" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0,0 L0,6 L8,3 z" fill="#30363d"/></marker></defs>
      <rect x="295" y="14" width="150" height="38" rx="18" fill="#21283a" stroke="#58a6ff" stroke-width="1.5"/>
      <text x="370" y="38" text-anchor="middle" fill="#58a6ff" font-size="13" font-weight="600">发现丢包</text>
      <line x1="370" y1="52" x2="370" y2="78" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr4)"/>
      <rect x="230" y="82" width="280" height="40" rx="8" fill="#1c2230" stroke="#f78166" stroke-width="1.2"/>
      <text x="370" y="99" text-anchor="middle" fill="#f78166" font-size="12">ethtool -S / rx_missed_errors</text>
      <text x="370" y="113" text-anchor="middle" fill="#6a7889" font-size="10" font-family="monospace">是否有 ring buffer 溢出？</text>
      <line x1="230" y1="102" x2="140" y2="102" stroke="#30363d" stroke-width="1.2" marker-end="url(#arr4)"/>
      <text x="183" y="96" text-anchor="middle" fill="#3fb950" font-size="9">是</text>
      <rect x="50" y="82" width="88" height="38" rx="6" fill="#1c2c1a" stroke="#3fb950" stroke-width="1.2"/>
      <text x="94" y="99" text-anchor="middle" fill="#3fb950" font-size="11">Ring Buffer</text>
      <text x="94" y="112" text-anchor="middle" fill="#6a7889" font-size="10">ethtool -G</text>
      <line x1="370" y1="122" x2="370" y2="148" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr4)"/>
      <text x="378" y="138" text-anchor="middle" fill="#6a7889" font-size="9">否</text>
      <rect x="215" y="152" width="310" height="40" rx="8" fill="#1c2230" stroke="#d2a679" stroke-width="1.2"/>
      <text x="370" y="169" text-anchor="middle" fill="#d2a679" font-size="12">softnet_stat col2 dropped 增长？</text>
      <text x="370" y="183" text-anchor="middle" fill="#6a7889" font-size="10" font-family="monospace">netdev_max_backlog 满？</text>
      <line x1="215" y1="172" x2="140" y2="172" stroke="#30363d" stroke-width="1.2" marker-end="url(#arr4)"/>
      <text x="180" y="166" text-anchor="middle" fill="#3fb950" font-size="9">是</text>
      <rect x="50" y="152" width="88" height="38" rx="6" fill="#2c2010" stroke="#d2a679" stroke-width="1.2"/>
      <text x="94" y="169" text-anchor="middle" fill="#d2a679" font-size="11">SoftIRQ</text>
      <text x="94" y="182" text-anchor="middle" fill="#6a7889" font-size="10">调大 backlog</text>
      <line x1="370" y1="192" x2="370" y2="218" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr4)"/>
      <text x="378" y="208" text-anchor="middle" fill="#6a7889" font-size="9">否</text>
      <rect x="215" y="222" width="310" height="40" rx="8" fill="#1c2230" stroke="#3fb950" stroke-width="1.2"/>
      <text x="370" y="239" text-anchor="middle" fill="#3fb950" font-size="12">nstat | grep IpIn* 有非零？</text>
      <text x="370" y="253" text-anchor="middle" fill="#6a7889" font-size="10" font-family="monospace">IP 头错误/路由/rp_filter？</text>
      <line x1="215" y1="242" x2="140" y2="242" stroke="#30363d" stroke-width="1.2" marker-end="url(#arr4)"/>
      <text x="180" y="236" text-anchor="middle" fill="#3fb950" font-size="9">是</text>
      <rect x="50" y="222" width="88" height="38" rx="6" fill="#12201a" stroke="#3fb950" stroke-width="1.2"/>
      <text x="94" y="239" text-anchor="middle" fill="#3fb950" font-size="11">IP 层</text>
      <text x="94" y="252" text-anchor="middle" fill="#6a7889" font-size="10">路由/rp_filter</text>
      <line x1="370" y1="262" x2="370" y2="288" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr4)"/>
      <text x="378" y="278" text-anchor="middle" fill="#6a7889" font-size="9">否</text>
      <rect x="215" y="292" width="310" height="40" rx="8" fill="#1c2230" stroke="#e3b341" stroke-width="1.2"/>
      <text x="370" y="309" text-anchor="middle" fill="#e3b341" font-size="12">dmesg | grep nf_conntrack 满？</text>
      <text x="370" y="323" text-anchor="middle" fill="#6a7889" font-size="10" font-family="monospace">iptables -L -v | grep DROP</text>
      <line x1="215" y1="312" x2="140" y2="312" stroke="#30363d" stroke-width="1.2" marker-end="url(#arr4)"/>
      <text x="180" y="306" text-anchor="middle" fill="#3fb950" font-size="9">是</text>
      <rect x="50" y="292" width="88" height="38" rx="6" fill="#201e10" stroke="#e3b341" stroke-width="1.2"/>
      <text x="94" y="309" text-anchor="middle" fill="#e3b341" font-size="11">Netfilter</text>
      <text x="94" y="322" text-anchor="middle" fill="#6a7889" font-size="10">conntrack/rules</text>
      <line x1="370" y1="332" x2="370" y2="355" stroke="#30363d" stroke-width="1.5" marker-end="url(#arr4)"/>
      <text x="378" y="348" text-anchor="middle" fill="#6a7889" font-size="9">否</text>
      <rect x="240" y="358" width="260" height="20" rx="6" fill="#1c1a2c" stroke="#bc8cff" stroke-width="1.2"/>
      <text x="370" y="373" text-anchor="middle" fill="#bc8cff" font-size="11">&#8594; nstat ListenOverflows / UdpRcvbufErrors</text>
    </svg>
  </div>

  <h3>&#128203; NIC / Ring Buffer 层</h3>
  <ul class="checklist">
    <li><code>ethtool -S eth0 | grep -E "(miss|drop|err|over)" | grep -v ": 0"</code> — 网卡级别丢包统计</li>
    <li><code>ethtool -g eth0</code> — 查看 Ring Buffer 当前大小（Current hardware settings: RX）</li>
    <li><code>cat /sys/class/net/eth0/statistics/rx_missed_errors</code> — Ring Buffer 溢出计数</li>
    <li><code>ethtool -G eth0 rx 4096</code> — 调大 Ring Buffer（若上述计数持续增长）</li>
    <li><code>cat /proc/interrupts | grep eth0</code> — 查看中断分布（是否偏向单核）</li>
  </ul>

  <h3>&#128203; SoftIRQ / Backlog 层</h3>
  <ul class="checklist">
    <li><code>cat /proc/net/softnet_stat</code> — 检查第 2 列（dropped）和第 3 列（time_squeeze）</li>
    <li><code>awk '{print NR-1, strtonum("0x"$2)}' /proc/net/softnet_stat</code> — 格式化输出每 CPU 的 dropped</li>
    <li><code>sysctl net.core.netdev_max_backlog</code> — 当 dropped 增长时，适当调大</li>
    <li><code>mpstat -P ALL 1</code> — 检查软中断 CPU 使用率（%soft 列）</li>
    <li><code>cat /proc/irq/&lt;N&gt;/smp_affinity_list</code> — 检查 IRQ 亲和性配置</li>
  </ul>

  <h3>&#128203; ARP 层</h3>
  <ul class="checklist">
    <li><code>dmesg | grep -i neighbour</code> — 检查 ARP 表溢出告警</li>
    <li><code>cat /proc/net/stat/arp_cache | head -2</code> — 查看 table_fulls 和 unresolved_discards</li>
    <li><code>ip neigh show | wc -l</code> — 当前 ARP 条目数</li>
    <li><code>sysctl net.ipv4.neigh.default.gc_thresh3</code> — 查看 ARP 表硬上限（默认 1024）</li>
  </ul>

  <h3>&#128203; IP 层</h3>
  <ul class="checklist">
    <li><code>nstat -az | grep -E "IpIn(Hdr|Addr|Discard|Unknown)"</code> — 检查非零 IP 错误计数</li>
    <li><code>sysctl net.ipv4.conf.all.rp_filter</code> — 多网卡时检查反向路径过滤（0=关闭，1=严格，2=宽松）</li>
    <li><code>ip route show</code> — 验证路由表是否完整</li>
    <li><code>netstat -s | grep -A 20 "^Ip:"</code> — 全量 IP 统计</li>
  </ul>

  <h3>&#128203; Netfilter / conntrack 层</h3>
  <ul class="checklist">
    <li><code>dmesg | grep "nf_conntrack: table full"</code> — 连接跟踪表满告警</li>
    <li><code>cat /proc/sys/net/netfilter/nf_conntrack_count</code> vs <code>nf_conntrack_max</code></li>
    <li><code>iptables -L -n -v | grep -E "(DROP|REJECT)"</code> — 防火墙显式丢弃规则</li>
    <li><code>sysctl net.netfilter.nf_conntrack_tcp_timeout_established</code> — 默认 432000 秒需调小</li>
  </ul>

  <h3>&#128203; TCP 层</h3>
  <ul class="checklist">
    <li><code>nstat -az | grep -E "Listen(Overflow|Drop)"</code> — Accept 队列满导致 SYN 丢弃</li>
    <li><code>ss -lnt</code> — 查看各监听 Socket 的队列使用（Recv-Q/Send-Q）</li>
    <li><code>nstat -az | grep TcpExtTCPAbort</code> — 连接中止原因</li>
    <li><code>sysctl net.core.somaxconn</code>，<code>sysctl net.ipv4.tcp_max_syn_backlog</code></li>
    <li><code>nstat -az | grep TcpRetransSegs</code> — 重传段数（间接反映丢包）</li>
  </ul>

  <h3>&#128203; UDP 层</h3>
  <ul class="checklist">
    <li><code>nstat -az | grep -E "UdpRcvbuf|UdpSndbuf|UdpInErr"</code> — UDP 缓冲区丢包</li>
    <li><code>ss -unp</code> — 查看 UDP Socket 的接收队列（Recv-Q）积压</li>
    <li><code>netstat -s | grep -A 10 "Udp:"</code> — UDP 统计汇总</li>
    <li><code>sysctl net.core.rmem_max</code>，<code>sysctl net.core.rmem_default</code> — 缓冲区上限</li>
  </ul>

  <h3>&#128203; 精准定位（SKB Drop Reason）</h3>
  <ul class="checklist">
    <li><code>perf record -e skb:kfree_skb -a sleep 5 &amp;&amp; perf script</code> — 获取精确丢包原因与内核位置</li>
    <li><code>dropwatch -l kas</code>（需 echo 0 &gt; /proc/sys/kernel/kptr_restrict）</li>
    <li><code>bpftrace -e 'tracepoint:skb:kfree_skb { @[args-&gt;reason] = count() }'</code> — eBPF 灵活追踪</li>
  </ul>

  <div class="summary-box">
    <h4>&#128204; 总体排查策略</h4>
    <ul>
      <li>自底向上：从 NIC Ring Buffer → SoftIRQ → ARP → IP → Netfilter → TCP/UDP 逐层检查计数器</li>
      <li>关注增量：使用 <code>nstat</code>（而非 <code>netstat -s</code>）观察计数器增量，避免被历史累积值误导</li>
      <li>区分方向：接收丢包（Ingress）和发送丢包（Egress）排查路径不同，通过 <code>ip -s link</code> 区分 RX/TX</li>
      <li>结合日志：<code>dmesg</code> 中的内核日志往往是最直接的线索（ARP overflow、conntrack full 等）</li>
      <li>精准定位：当 MIB 计数器缩小范围后，用 <code>perf skb:kfree_skb</code> 获取精确的丢包原因和内核函数</li>
    </ul>
  </div>
</section>

</div>
</main>

<script>
function updateProgress() {
  var scrollTop = window.scrollY;
  var docHeight = document.documentElement.scrollHeight - window.innerHeight;
  var pct = docHeight > 0 ? (scrollTop / docHeight) * 100 : 0;
  document.getElementById('progress-bar').style.width = pct + '%';
}

function updateActiveNav() {
  var sections = document.querySelectorAll('.section');
  var navItems = document.querySelectorAll('.nav-item');
  var current = '';
  sections.forEach(function(s) {
    if (s.getBoundingClientRect().top <= 80) current = s.id;
  });
  navItems.forEach(function(item) {
    var href = item.getAttribute('href');
    if (href === '#' + current) item.classList.add('active');
    else item.classList.remove('active');
  });
}

window.addEventListener('scroll', function() {
  updateProgress();
  updateActiveNav();
}, { passive: true });

function toggleGroup(header) {
  var items = header.nextElementSibling;
  var isCollapsed = header.classList.toggle('collapsed');
  if (isCollapsed) {
    items.style.maxHeight = '0';
    items.classList.add('hidden');
  } else {
    items.style.maxHeight = items.scrollHeight + 'px';
    items.classList.remove('hidden');
  }
}

document.querySelectorAll('.nav-group-items').forEach(function(el) {
  el.style.maxHeight = el.scrollHeight + 'px';
});

var sidebar = document.getElementById('sidebar');
var overlay = document.getElementById('overlay');
var hamburger = document.getElementById('hamburger');
function closeMobile() {
  sidebar.classList.remove('open');
  overlay.classList.remove('show');
}
hamburger.addEventListener('click', function() {
  sidebar.classList.toggle('open');
  overlay.classList.toggle('show');
});
overlay.addEventListener('click', closeMobile);

function scrollToSection(id, behavior) {
  var target = document.getElementById(id);
  if (!target) return false;
  var offset = 20;
  var top = target.getBoundingClientRect().top + window.scrollY - offset;
  window.scrollTo({ top: Math.max(0, top), behavior: behavior || 'smooth' });
  try {
    if (history.replaceState) {
      history.replaceState(null, '', '#' + id);
    } else {
      location.hash = id;
    }
  } catch (err) {}
  return true;
}

document.querySelectorAll('.nav-item').forEach(function(item) {
  item.addEventListener('click', function(e) {
    var href = item.getAttribute('href');
    if (href && href.charAt(0) === '#') {
      if (scrollToSection(href.slice(1), 'smooth')) {
        e.preventDefault();
      }
    }
    closeMobile();
    window.setTimeout(updateActiveNav, 400);
  });
});

if (location.hash) {
  window.setTimeout(function() {
    scrollToSection(location.hash.slice(1), 'auto');
  }, 0);
}

var sectionEls = Array.from(document.querySelectorAll('.section'));
var currentIdx = 0;
function updateIdx() {
  sectionEls.forEach(function(s, i) {
    if (s.getBoundingClientRect().top <= 100) currentIdx = i;
  });
}
window.addEventListener('scroll', updateIdx, { passive: true });
document.addEventListener('keydown', function(e) {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
  if (e.key === 'ArrowRight' || e.key === 'ArrowDown') {
    currentIdx = Math.min(currentIdx + 1, sectionEls.length - 1);
    sectionEls[currentIdx].scrollIntoView({ behavior: 'smooth' });
    e.preventDefault();
  } else if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') {
    currentIdx = Math.max(currentIdx - 1, 0);
    sectionEls[currentIdx].scrollIntoView({ behavior: 'smooth' });
    e.preventDefault();
  }
});

updateProgress();
updateActiveNav();
</script>
</body>
</html>
`;

export default function App() {
  const containerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    document.title = "Linux 网络丢包排查指南";
    document.body.style.margin = "0";
    document.body.style.padding = "0";
    document.body.style.background = "#0f1117";
    document.body.style.overflow = "hidden";
  }, []);

  return (
    <div
      ref={containerRef}
      style={{ width: "100vw", height: "100vh", overflow: "hidden" }}
    >
      <iframe
        srcDoc={htmlContent}
        scrolling="yes"
        style={{
          width: "100%",
          height: "100%",
          border: "none",
          display: "block",
        }}
        title="Linux 网络丢包排查指南"
        onLoad={(event) => {
          const frame = event.currentTarget;
          const doc = frame.contentDocument;
          const hash = window.location.hash;
          if (!doc || !hash) return;
          const id = hash.replace(/^#/, "");
          const target = doc.getElementById(id);
          if (target) {
            const top =
              target.getBoundingClientRect().top +
              (doc.defaultView?.scrollY ?? 0) -
              20;
            doc.defaultView?.scrollTo({ top: Math.max(0, top), behavior: "auto" });
          }
        }}
      />
    </div>
  );
}
