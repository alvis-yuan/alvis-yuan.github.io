#!/usr/bin/env python3
"""从 Wireshark PDU Export 或含明文 TLS 握手的 pcap/pcapng 中提取 Certificate 消息里的 DER 链。"""
import argparse
import os
import sys


def find_certificate_chains(buf: bytes):
    chains = []
    i = 0
    while True:
        j = buf.find(b"\x16\x03", i)
        if j < 0:
            break
        if j + 5 > len(buf):
            break
        rec_len = (buf[j + 3] << 8) | buf[j + 4]
        if j + 5 + rec_len > len(buf):
            i = j + 1
            continue
        payload = buf[j + 5 : j + 5 + rec_len]
        off = 0
        while off + 4 <= len(payload):
            htype = payload[off]
            hlen = (payload[off + 1] << 16) | (payload[off + 2] << 8) | payload[off + 3]
            if off + 4 + hlen > len(payload):
                break
            hbody = payload[off + 4 : off + 4 + hlen]
            if htype == 0x0B and len(hbody) >= 3:
                clen = (hbody[0] << 16) | (hbody[1] << 8) | hbody[2]
                pos, chain = 3, []
                end = min(3 + clen, len(hbody))
                while pos + 3 <= end:
                    cert_len = (hbody[pos] << 16) | (hbody[pos + 1] << 8) | hbody[pos + 2]
                    pos += 3
                    chain.append(hbody[pos : pos + cert_len])
                    pos += cert_len
                if chain:
                    chains.append((j, chain))
            off += 4 + hlen
        i = j + 5 + rec_len
    return chains


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pcap", help="pcap/pcapng 路径")
    ap.add_argument("-o", "--out-dir", default="certs_from_pcap", help="输出目录")
    args = ap.parse_args()
    with open(args.pcap, "rb") as f:
        data = f.read()
    chains = find_certificate_chains(data)
    if not chains:
        print("未找到 Certificate 握手消息", file=sys.stderr)
        sys.exit(1)
    os.makedirs(args.out_dir, exist_ok=True)
    labels = ["server", "client"]
    for idx, (_, chain) in enumerate(chains):
        label = labels[idx] if idx < len(labels) else f"certmsg{idx}"
        for ci, der in enumerate(chain):
            path = os.path.join(args.out_dir, f"{label}_chain_{ci}.der")
            with open(path, "wb") as f:
                f.write(der)
            print(f"wrote {path} ({len(der)} bytes)")


if __name__ == "__main__":
    main()
