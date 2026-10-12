#!/usr/bin/env python3
"""Signe une commande MQTT pour un module N2 (banc ou exploitation).

La clé (32 octets, hexadécimal) est lue dans la variable d'environnement
N2_CMD_KEY ou dans un fichier passé par --key-file ; elle n'est jamais
passée en argument ni écrite sur disque par cet outil.

Exemple :
    N2_CMD_KEY=... n2_cmd.py --device n2-banc-01 --seq 42 disarm
    # -> v=1;seq=42;cmd=disarm;mac=<hex>  (à publier sur essensys/n2/n2-banc-01/cmd)
"""
import argparse
import hashlib
import hmac
import os
import sys

COMMANDS = ("arm", "disarm", "debug", "status", "events", "frames", "ota")


def sign(key: bytes, device: str, body: str) -> str:
    mac = hmac.new(key, (device + "|" + body).encode("ascii"), hashlib.sha256).hexdigest()
    return f"{body};mac={mac}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", required=True)
    ap.add_argument("--seq", required=True, type=int, help="strictement croissant (anti-rejeu)")
    ap.add_argument("--key-file")
    ap.add_argument("--arg")
    ap.add_argument("--ttl", type=int)
    ap.add_argument("cmd", choices=COMMANDS)
    a = ap.parse_args()

    if a.key_file:
        with open(a.key_file, encoding="ascii") as f:
            key_hex = f.read().strip()
    else:
        key_hex = os.environ.get("N2_CMD_KEY", "")
    try:
        key = bytes.fromhex(key_hex)
    except ValueError:
        key = b""
    if len(key) < 16:
        print("clé absente ou invalide (N2_CMD_KEY ou --key-file)", file=sys.stderr)
        return 2

    body = f"v=1;seq={a.seq};cmd={a.cmd}"
    if a.arg:
        body += f";arg={a.arg}"
    if a.ttl:
        body += f";ttl={a.ttl}"
    print(sign(key, a.device, body))
    return 0


if __name__ == "__main__":
    sys.exit(main())
