#!/usr/bin/env python3
"""Authenticate only peers seen connecting to the host's existing SSH listener."""
import argparse
import ipaddress
import json
from pathlib import Path
import secrets
import subprocess
import time

base = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--seconds', type=int, default=600)
parser.add_argument('--address', help='Exact endpoint read from a verified device capture; no LAN scan')
args = parser.parse_args()
if not 1 <= args.seconds <= 3600:
    parser.error('seconds must be 1..3600')
config = json.loads((base / 'host-config.json').read_text())
host = config['host_ipv4']
network = ipaddress.ip_network(host + '/24', strict=False)
explicit = str(ipaddress.IPv4Address(args.address)) if args.address else None
if explicit and (ipaddress.ip_address(explicit) not in network or explicit == host):
    parser.error('captured endpoint must be another address in the configured /24')
deadline = time.monotonic() + args.seconds
tried = set()
next_attempt = 0.0
nonce = secrets.token_hex(16)
command = 'printf "GLEWORKS_CONTROL ' + nonce + '\\n"; id -u; uname -r; readlink /proc/1/exe'
while time.monotonic() < deadline:
    result = subprocess.run(['ss', '-Htn', 'state', 'established', '( sport = :2222 )'],
                            capture_output=True, text=True, check=True)
    lines = result.stdout.splitlines()
    if explicit and time.monotonic() >= next_attempt:
        lines.append('0 0 ' + host + ':2222 ' + explicit + ':0')
        tried.discard(explicit)
        next_attempt = time.monotonic() + 5
    for line in lines:
        fields = line.split()
        if len(fields) != 4 or fields[2].rsplit(':', 1)[0] != host:
            continue
        try:
            address = str(ipaddress.IPv4Address(fields[3].rsplit(':', 1)[0]))
        except ValueError:
            continue
        if ipaddress.ip_address(address) not in network or address == host or address in tried:
            continue
        tried.add(address)
        ssh = ['ssh', '-F', '/dev/null', '-i', str(base / '.private/client_ed25519'),
               '-o', 'IdentitiesOnly=yes', '-o', 'BatchMode=yes',
               '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=gleworks-temporary-control',
               '-o', 'UserKnownHostsFile=' + str(base / '.private/known_hosts'),
               '-o', 'ConnectTimeout=4', '-p', '2222', 'root@' + address]
        try:
            proof = subprocess.run(ssh + [command], capture_output=True, text=True, timeout=8)
        except subprocess.TimeoutExpired:
            continue
        expected = ['GLEWORKS_CONTROL ' + nonce, '0', '3.0.35-lab126', '/sbin/init.exe']
        if proof.returncode == 0 and proof.stdout.splitlines() == expected:
            record = {'kindle_ipv4': address, 'ssh_argv': ssh, 'proof': expected,
                      'native_boot_confirmed': False, 'observed_unix_time': time.time()}
            (base / '.private/session-ready.json').write_text(json.dumps(record, indent=2) + '\n')
            print('Authenticated Kindle root shell ready; native Alpine boot has not occurred.', flush=True)
            raise SystemExit(0)
    time.sleep(0.5)
print('No authenticated Kindle control connection observed; device launch still required.', flush=True)
raise SystemExit(2)
