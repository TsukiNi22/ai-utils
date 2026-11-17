#!/usr/bin/env python3
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  17/11/2026 by Tsukini

File Name:
##  link.py

File Description:
##  An encrypted link between Claude sessions on several computers: one hosts, the others join; messages and files
##  of any size, a stream of events to watch, wake-up hooks
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

"""
Usage: link.py <command> [--link NAME] ...
  host  [--method auto|tailscale|lan|public|local] [--port N] [--bind IP,...] [--passphrase P] [--as NAME]
        [--cluster SESSION] [--notify]          host a link (a daemon in the background), print how to join it
  join  CODE|ADDRESS:PORT [--passphrase P] [--as NAME] [--cluster SESSION] [--notify]   join a hosted link
  send  [--to PEER|all] TEXT...                 a message (default: all)
  file  PATH [--to PEER|all]                    a file of any size (encrypted, checked with sha256 on arrival)
  status                                        role, addresses, peers, transfers
  info  [--secret]                              how to join (the code; the passphrase only with --secret)
  events [--follow] [--new] [--json]            what happened (one line each); --follow --new: for a Monitor
  read  ID                                      the full text of a message event
  stop                                          stop the daemon (and its UPnP mapping)

The passphrase gives a key (scrypt); both sides prove they know it without sending it (HMAC challenge), then every
frame is encrypted and authenticated (ChaCha20-Poly1305, a counter per direction: no replay). The passphrase can also
come from SESSION_LINK_PASSPHRASE; without one, host generates a strong one. State: $SESSION_LINK_DIR or
~/.local/state/session-link/<link>/ (config, secret 0600, events.jsonl, inbox/).
"""

##### Imports #####

import argparse # Used for ArgumentParser
import asyncio # Used for start_server, open_connection, Lock
import base64 # Used for urlsafe_b64encode, urlsafe_b64decode
import hashlib # Used for scrypt, sha256
import hmac # Used for new, compare_digest
import ipaddress # Used for ip_address
import json # Used for loads, dumps
import os # Used for path, chmod, urandom
import re # Used for sub, match
import secrets # Used for token_urlsafe, token_hex
import shutil # Used for which, disk_usage
import socket # Used for gethostname, socket
import subprocess # Used for run, Popen
import sys # Used for exit, argv, executable
import time # Used for time, strftime, sleep

try:
    from cryptography.exceptions import InvalidTag # Used for a forged / replayed frame
    from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305 # Used for the frames
except ImportError:
    ChaCha20Poly1305, InvalidTag = None, ValueError

##### Constants #####

VERSION = 1
ROOT = os.environ.get("SESSION_LINK_DIR", os.path.expanduser("~/.local/state/session-link"))
PORTS = range(47200, 47300)        # tried in order when no port is given
CHUNK = 256 * 1024                 # file chunk
MAX_FRAME = CHUNK + 64 * 1024      # encrypted frame limit (a chunk + its header)
MAX_PLAIN = 4096                   # handshake frame limit
MAX_PEERS = 16
SCRYPT = {"n": 2 ** 15, "r": 8, "p": 1, "maxmem": 64 * 1024 * 1024, "dklen": 32}
PING, DEAD = 20, 70                # seconds: keepalive, silence that closes a link
BAD_LIMIT, BAN = 5, 600            # wrong passphrases per minute from one address, then banned (seconds)
HANDSHAKE_TIMEOUT = 10
J, F = 1, 2                        # frame kinds: JSON, file chunk

##### Functions #####

def die(text: str) -> None:
    sys.stderr.write(f"Error: {text}\n")
    sys.exit(1)

def link_dir(link: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9._-]{1,40}", link):
        die(f"bad link name {link!r} (letters, digits, . _ -)")
    return os.path.join(ROOT, link)

def private_write(path: str, text: str) -> None:
    """A file only the user can read (the secret, the control token)"""
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(text)

def load_json(path: str) -> dict:
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}

def human(size: float) -> str:
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if size < 1024 or unit == "TB":
            return f"{size:.0f} {unit}" if unit == "B" else f"{size:.1f} {unit}"
        size /= 1024
    return ""

def derive(passphrase: str, salt: bytes) -> bytes:
    return hashlib.scrypt(passphrase.encode(), salt=salt, **SCRYPT)

def mac(key: bytes, *parts: bytes) -> bytes:
    return hmac.new(key, b"|".join(parts), hashlib.sha256).digest()

def hkdf(key: bytes, salt: bytes, info: bytes) -> bytes:
    prk = hmac.new(salt, key, hashlib.sha256).digest()
    return hmac.new(prk, info + b"\x01", hashlib.sha256).digest()

def safe_name(name: str) -> str:
    """A file name received from a peer, made harmless: no folder, no hidden file, no odd character"""
    base = os.path.basename(name.replace("\\", "/")).strip()
    base = re.sub(r"[^A-Za-z0-9._ +()\[\]-]", "_", base).lstrip(".")
    return base[:120] or "file"

def unique(path: str) -> str:
    stem, ext = os.path.splitext(path)
    n = 1
    while os.path.exists(path):
        path = f"{stem}-{n}{ext}"
        n += 1
    return path

##### Network discovery #####

def tailscale_ip() -> str:
    if not shutil.which("tailscale"):
        return ""
    try:
        out = subprocess.run(["tailscale", "ip", "-4"], capture_output=True, text=True, timeout=5).stdout.split()
    except (OSError, subprocess.SubprocessError):
        return ""
    return out[0] if out else ""

def lan_ip() -> str:
    """The address of the interface that carries the default route (no packet is sent)"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))
        ip = s.getsockname()[0]
    except OSError:
        return ""
    finally:
        s.close()
    return ip if ipaddress.ip_address(ip).is_private else ""

def free_port(addresses: list, wanted: int) -> int:
    for port in ([wanted] if wanted else PORTS):
        try:
            for addr in addresses:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) # as the server does: old connections in TIME_WAIT do not block it
                    s.bind((addr, port))
            return port
        except OSError:
            if wanted:
                die(f"port {wanted} is already used on {addresses}")
    die("no free port in 47200-47299")
    return 0

def upnp_map(lan: str, port: int) -> str:
    """Opens the port on the router (UPnP) and returns the public address, empty when the router refuses"""
    if not shutil.which("upnpc"):
        return ""
    run = lambda *a: subprocess.run(["upnpc", *a], capture_output=True, text=True, timeout=20).stdout
    try:
        run("-e", "session-link", "-a", lan, str(port), str(port), "TCP")
        status = run("-s")
    except (OSError, subprocess.SubprocessError):
        return ""
    m = re.search(r"ExternalIPAddress = ([0-9.]+)", status)
    return m.group(1) if m else ""

def firewall_hint(port: int) -> str:
    """firewalld active and the port closed: the command to open it (root, asked first)"""
    if not shutil.which("firewall-cmd"):
        return ""
    try:
        state = subprocess.run(["firewall-cmd", "--state"], capture_output=True, text=True, timeout=5).stdout.strip()
        if state != "running":
            return ""
        query = subprocess.run(["firewall-cmd", f"--query-port={port}/tcp"], capture_output=True, text=True, timeout=5)
    except (OSError, subprocess.SubprocessError):
        return ""
    if query.stdout.strip() == "yes":
        return ""
    return f"firewall-cmd --add-port={port}/tcp (runtime: closed again at reboot; --permanent to keep it)"

def encode_code(addresses: list, port: int, link: str) -> str:
    raw = json.dumps({"v": VERSION, "a": addresses, "p": port, "l": link}, separators=(",", ":")).encode()
    return "sl1." + base64.urlsafe_b64encode(raw).decode().rstrip("=")

def decode_target(target: str) -> tuple:
    """A join code (sl1....) or ADDRESS:PORT -> (addresses, port, link or '')"""
    if target.startswith("sl1."):
        raw = target[4:] + "=" * (-len(target[4:]) % 4)
        try:
            data = json.loads(base64.urlsafe_b64decode(raw))
            return data["a"], int(data["p"]), data.get("l", "")
        except (ValueError, KeyError, TypeError):
            die("this join code is damaged")
    m = re.fullmatch(r"\[?([^\]]+?)\]?:(\d+)", target)
    if not m:
        die("give a join code (sl1....) or ADDRESS:PORT")
    return [m.group(1)], int(m.group(2)), ""

##### Encrypted channel #####

async def send_plain(writer: asyncio.StreamWriter, data: dict) -> None:
    raw = json.dumps(data).encode()
    writer.write(len(raw).to_bytes(4, "big") + raw)
    await writer.drain()

async def recv_plain(reader: asyncio.StreamReader) -> dict:
    size = int.from_bytes(await reader.readexactly(4), "big")
    if size > MAX_PLAIN:
        raise ValueError("handshake frame too big")
    return json.loads(await reader.readexactly(size))

class Channel:
    """One authenticated TCP link: a key and a frame counter per direction"""

    def __init__(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter, k_out: bytes, k_in: bytes, name: str) -> None:
        self.reader, self.writer, self.name = reader, writer, name
        self.out, self.inn = ChaCha20Poly1305(k_out), ChaCha20Poly1305(k_in)
        self.n_out = self.n_in = 0
        self.lock = asyncio.Lock()
        self.last = time.time()

    async def send(self, kind: int, payload: bytes) -> None:
        async with self.lock:
            ct = self.out.encrypt(self.n_out.to_bytes(12, "big"), bytes([kind]) + payload, None)
            self.n_out += 1
            self.writer.write(len(ct).to_bytes(4, "big") + ct)
            await self.writer.drain()

    async def send_json(self, data: dict) -> None:
        await self.send(J, json.dumps(data).encode())

    async def recv(self) -> tuple:
        size = int.from_bytes(await asyncio.wait_for(self.reader.readexactly(4), DEAD), "big")
        if size > MAX_FRAME:
            raise ValueError("frame too big")
        ct = await asyncio.wait_for(self.reader.readexactly(size), DEAD)
        pt = self.inn.decrypt(self.n_in.to_bytes(12, "big"), ct, None) # raises on a forged or replayed frame
        self.n_in += 1
        self.last = time.time()
        return pt[0], pt[1:]

    def close(self) -> None:
        try:
            self.writer.close()
        except OSError:
            pass

##### Daemon #####

class Node:
    """The daemon of one link: host (accepts the others, relays between them) or member (joined to a host)"""

    def __init__(self, folder: str) -> None:
        self.dir = folder
        self.cfg = load_json(os.path.join(folder, "config.json"))
        self.name = self.cfg["as"]
        with open(os.path.join(folder, "secret"), encoding="utf-8") as f:
            self.passphrase = f.read().strip()
        self.peers = {}            # host: name -> Channel
        self.since = {}            # host: name -> (address, time)
        self.up = None             # member: the Channel to the host
        self.incoming = {}         # transfer id -> {fh, path, sha, size, got, from, name}
        self.relays = {}           # transfer id -> [Channel] (host forwarding a file)
        self.sending = {}          # transfer id -> {name, size, sent, to}
        self.bad = {}              # address -> [times of wrong passphrases]
        self.banned = {}           # address -> until
        self.event_id = self.last_event_id()
        self.stopping = asyncio.Event()
        self.salt = os.urandom(16)
        self.key = derive(self.passphrase, self.salt) if self.cfg["role"] == "host" else b""
        self.keys = {}             # member: salt -> key

    ##### Events #####

    def last_event_id(self) -> int:
        path = os.path.join(self.dir, "events.jsonl")
        last = 0
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                for line in f:
                    try:
                        last = json.loads(line)["id"]
                    except (ValueError, KeyError):
                        pass
        return last

    def emit(self, kind: str, **data: object) -> None:
        self.event_id += 1
        event = {"id": self.event_id, "time": time.strftime("%Y-%m-%d %H:%M:%S"), "kind": kind, **data}
        with open(os.path.join(self.dir, "events.jsonl"), "a", encoding="utf-8") as f:
            f.write(json.dumps(event, ensure_ascii=False) + "\n")
        if kind in ("message", "file"):
            self.wake(event)

    def wake(self, event: dict) -> None:
        """The wake-up hooks: a prompt to a claude-cluster session, a desktop notification"""
        line = format_event(event, self.cfg["link"])
        session = self.cfg.get("cluster")
        if session and shutil.which("claude-cluster"):
            subprocess.Popen(["claude-cluster", "send", session, line + " (session-link: run `link.py events --new` to see it)"],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if self.cfg.get("notify") and shutil.which("notify-send"):
            subprocess.Popen(["notify-send", "-a", "session-link", f"session-link {self.cfg['link']}", line[:300]],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    ##### Routing #####

    def targets(self, to: str, src: str) -> tuple:
        """(the local node receives it, the channels to forward to)"""
        if self.cfg["role"] != "host": # a member: what it sends goes to the host, what comes is for it
            return src != self.name and to in ("all", self.name), []
        if to == "all":
            return src != self.name, [c for n, c in self.peers.items() if n != src]
        if to == self.name:
            return True, []
        return False, [self.peers[to]] if to in self.peers else []

    async def route_json(self, data: dict, src: str) -> None:
        to = data.get("to", "all")
        local, forward = self.targets(to, src)
        if self.cfg["role"] == "member" and src == self.name and self.up:
            forward = [self.up]
        if self.cfg["role"] == "host" and src != self.name and to not in ("all", self.name) and to not in self.peers:
            if data["t"] in ("msg", "file-begin"):
                await self.peers[src].send_json({"t": "ack", "id": secrets.token_hex(8), "from": to, "to": src, "ref": data["id"],
                                                 "what": "message" if data["t"] == "msg" else "file", "ok": False, "why": f"{to} is not on the link"})
            return
        for ch in forward:
            await ch.send_json(data)
        if data["t"] == "file-begin":
            self.relays[data["id"]] = forward
        if data["t"] == "file-end" and not local:
            self.relays.pop(data["id"], None)
        if local:
            await self.receive_json(data)

    async def route_chunk(self, payload: bytes, src: str) -> None:
        tid = payload[:16].decode()
        for ch in self.relays.get(tid, []):
            await ch.send(F, payload)
        entry = self.incoming.get(tid)
        if entry:
            data = payload[16:]
            entry["fh"].write(data)
            entry["sha"].update(data)
            entry["got"] += len(data)

    async def receive_json(self, data: dict) -> None:
        kind = data["t"]
        if kind == "msg":
            self.emit("message", source=data["from"], to=data["to"], text=data["text"], ref=data["id"])
            await self.reply_ack(data, "message", True, "")
        elif kind == "file-begin":
            folder = os.path.join(self.dir, "inbox", safe_name(data["from"]))
            os.makedirs(folder, exist_ok=True)
            if shutil.disk_usage(folder).free < data["size"] + 512 * 1024 * 1024:
                self.emit("error", text=f"no room for {data['name']} ({human(data['size'])}) from {data['from']}")
                await self.reply_ack(data, "file", False, "no room on the receiving disk")
                return
            path = unique(os.path.join(folder, safe_name(data["name"])))
            self.incoming[data["id"]] = {"fh": open(path, "wb"), "path": path, "sha": hashlib.sha256(), "size": data["size"],
                                         "got": 0, "from": data["from"], "name": data["name"]}
        elif kind == "file-end":
            self.relays.pop(data["id"], None)
            entry = self.incoming.pop(data["id"], None)
            if not entry:
                return
            entry["fh"].close()
            ok = entry["got"] == entry["size"] and entry["sha"].hexdigest() == data["sha256"]
            if not ok:
                os.replace(entry["path"], entry["path"] + ".corrupt")
            self.emit("file", source=entry["from"], path=entry["path"] + ("" if ok else ".corrupt"), size=entry["size"],
                      ok=ok, name=entry["name"])
            await self.reply_ack({**data, "from": entry["from"]}, "file", ok, "" if ok else "size or sha256 mismatch")
        elif kind == "ack":
            self.emit("delivered", source=data["from"], what=data["what"], ok=data["ok"], ref=data["ref"], why=data.get("why", ""))
        elif kind == "peers":
            self.emit("peers", names=data["names"])

    async def reply_ack(self, data: dict, what: str, ok: bool, why: str) -> None:
        if data["from"] == self.name:
            return
        await self.route_json({"t": "ack", "id": secrets.token_hex(8), "from": self.name, "to": data["from"], "ref": data["id"],
                               "what": what, "ok": ok, "why": why}, self.name)

    async def broadcast_peers(self) -> None:
        names = [self.name] + sorted(self.peers)
        self.emit("peers", names=names)
        for ch in list(self.peers.values()):
            await ch.send_json({"t": "peers", "names": names, "to": ch.name, "from": self.name, "id": secrets.token_hex(8)})

    ##### Host side #####

    def refused(self, addr: str) -> bool:
        return self.banned.get(addr, 0) > time.time()

    def count_bad(self, addr: str) -> None:
        now = time.time()
        hits = [t for t in self.bad.get(addr, []) if now - t < 60] + [now]
        self.bad[addr] = hits
        if len(hits) >= BAD_LIMIT:
            self.banned[addr] = now + BAN
            self.emit("error", text=f"{addr} banned for {BAN // 60} min after {BAD_LIMIT} wrong passphrases")

    async def accept(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        addr = (writer.get_extra_info("peername") or ("?",))[0]
        if self.refused(addr) or len(self.peers) >= MAX_PEERS:
            writer.close()
            return
        try:
            ch = await asyncio.wait_for(self.handshake_host(reader, writer, addr), HANDSHAKE_TIMEOUT)
        except (OSError, ValueError, KeyError, asyncio.IncompleteReadError, asyncio.TimeoutError):
            writer.close()
            return
        if not ch:
            return
        self.peers[ch.name] = ch
        self.since[ch.name] = (addr, time.strftime("%H:%M:%S"))
        self.emit("joined", source=ch.name, address=addr)
        await self.broadcast_peers()
        await self.serve(ch)
        self.peers.pop(ch.name, None)
        self.since.pop(ch.name, None)
        self.emit("left", source=ch.name)
        await self.broadcast_peers()

    async def handshake_host(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter, addr: str) -> Channel:
        nonce_s = os.urandom(16)
        await send_plain(writer, {"v": VERSION, "salt": self.salt.hex(), "nonce": nonce_s.hex(), "host": self.name})
        hello = await recv_plain(reader)
        nonce_c, name = bytes.fromhex(hello["nonce"]), str(hello["name"])[:40]
        if not hmac.compare_digest(mac(self.key, b"c", nonce_s, nonce_c, name.encode()), bytes.fromhex(hello["mac"])):
            self.count_bad(addr)
            self.emit("refused", address=addr, why="wrong passphrase")
            await asyncio.sleep(1)
            await send_plain(writer, {"ok": False, "error": "wrong passphrase"})
            writer.close()
            return None
        if name == self.name or name in self.peers or not re.fullmatch(r"[A-Za-z0-9._-]{1,40}", name):
            await send_plain(writer, {"ok": False, "error": f"the name {name!r} is taken or invalid: join with another --as"})
            writer.close()
            return None
        await send_plain(writer, {"ok": True, "mac": mac(self.key, b"s", nonce_c, nonce_s).hex()})
        salt = nonce_s + nonce_c
        return Channel(reader, writer, hkdf(self.key, salt, b"s2c"), hkdf(self.key, salt, b"c2s"), name)

    ##### Member side #####

    async def handshake_member(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> Channel:
        hello = await recv_plain(reader)
        if hello.get("v") != VERSION:
            raise ValueError(f"the host speaks version {hello.get('v')}, this one {VERSION}")
        salt = bytes.fromhex(hello["salt"])
        if salt not in self.keys:
            self.keys[salt] = derive(self.passphrase, salt)
        key, nonce_s, nonce_c = self.keys[salt], bytes.fromhex(hello["nonce"]), os.urandom(16)
        await send_plain(writer, {"nonce": nonce_c.hex(), "name": self.name, "mac": mac(key, b"c", nonce_s, nonce_c, self.name.encode()).hex()})
        answer = await recv_plain(reader)
        if not answer.get("ok"):
            raise PermissionError(answer.get("error", "refused"))
        if not hmac.compare_digest(mac(key, b"s", nonce_c, nonce_s), bytes.fromhex(answer["mac"])):
            raise PermissionError("the host did not prove it knows the passphrase")
        self.host_name = hello["host"]
        both = nonce_s + nonce_c
        return Channel(reader, writer, hkdf(key, both, b"c2s"), hkdf(key, both, b"s2c"), hello["host"])

    async def member_loop(self) -> None:
        delay = 1
        while not self.stopping.is_set():
            for addr in self.cfg["addresses"]:
                try:
                    reader, writer = await asyncio.wait_for(asyncio.open_connection(addr, self.cfg["port"]), 8)
                    self.up = await asyncio.wait_for(self.handshake_member(reader, writer), HANDSHAKE_TIMEOUT)
                except PermissionError as err:
                    self.emit("error", text=f"refused by the host: {err} (the daemon stops)")
                    self.stopping.set()
                    return
                except (OSError, ValueError, KeyError, asyncio.IncompleteReadError, asyncio.TimeoutError):
                    continue
                self.emit("connected", source=self.up.name, address=f"{addr}:{self.cfg['port']}")
                delay = 1
                await self.serve(self.up)
                self.up = None
                self.emit("disconnected", text="connection lost, retrying")
                break
            await asyncio.sleep(delay)
            delay = min(delay * 2, 30)

    ##### Both #####

    async def serve(self, ch: Channel) -> None:
        pinger = asyncio.create_task(self.ping(ch))
        try:
            while not self.stopping.is_set():
                kind, payload = await ch.recv()
                if kind == F:
                    await self.route_chunk(payload, ch.name)
                    continue
                data = json.loads(payload)
                if data.get("t") == "ping":
                    continue
                if self.cfg["role"] == "host":
                    data["from"] = ch.name # a member cannot speak for another
                await self.route_json(data, ch.name)
        except (OSError, ValueError, KeyError, asyncio.IncompleteReadError, asyncio.TimeoutError, InvalidTag) as err:
            if not isinstance(err, (asyncio.IncompleteReadError, asyncio.TimeoutError, ConnectionError)):
                self.emit("error", text=f"link with {ch.name} closed: {type(err).__name__} {err}"[:300])
        finally:
            pinger.cancel()
            ch.close()
            for tid, entry in list(self.incoming.items()):
                if entry["from"] == ch.name:
                    entry["fh"].close()
                    self.incoming.pop(tid)
                    self.emit("error", text=f"transfer of {entry['name']} from {ch.name} cut at {human(entry['got'])}")

    async def ping(self, ch: Channel) -> None:
        try:
            while True:
                await asyncio.sleep(PING)
                await ch.send_json({"t": "ping"})
        except (OSError, ConnectionError):
            pass # the reading side sees the link closed

    async def send_message(self, to: str, text: str) -> dict:
        if not self.reachable(to):
            return {"ok": False, "error": f"no peer {to!r} connected (status lists them)"}
        data = {"t": "msg", "id": secrets.token_hex(8), "from": self.name, "to": to, "text": text}
        await self.route_json(data, self.name)
        return {"ok": True, "id": data["id"]}

    def reachable(self, to: str) -> bool:
        if self.cfg["role"] == "member":
            return self.up is not None
        return to == "all" or to in self.peers

    async def send_file(self, path: str, to: str) -> None:
        tid = secrets.token_hex(8)
        size = os.path.getsize(path)
        self.sending[tid] = {"name": os.path.basename(path), "size": size, "sent": 0, "to": to}
        await self.route_json({"t": "file-begin", "id": tid, "from": self.name, "to": to, "name": os.path.basename(path), "size": size}, self.name)
        chans = self.relays.pop(tid, [])
        sha = hashlib.sha256()
        try:
            with open(path, "rb") as f:
                while chunk := f.read(CHUNK):
                    sha.update(chunk)
                    for ch in chans:
                        await ch.send(F, tid.encode() + chunk)
                    self.sending[tid]["sent"] += len(chunk)
            for ch in chans:
                await ch.send_json({"t": "file-end", "id": tid, "from": self.name, "to": to, "sha256": sha.hexdigest()})
            self.emit("sent", what="file", name=os.path.basename(path), size=size, to=to)
        except (OSError, ConnectionError) as err:
            self.emit("error", text=f"sending {path} failed: {err}")
        finally:
            self.sending.pop(tid, None)

    ##### Local control #####

    async def control(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            req = json.loads(await asyncio.wait_for(reader.readline(), 10))
            if not hmac.compare_digest(str(req.get("token", "")), self.token):
                raise ValueError("bad token")
            answer = await self.command(req)
        except (ValueError, KeyError, asyncio.TimeoutError) as err:
            answer = {"ok": False, "error": str(err)}
        writer.write((json.dumps(answer) + "\n").encode())
        await writer.drain()
        writer.close()

    async def command(self, req: dict) -> dict:
        cmd = req["cmd"]
        if cmd == "send":
            return await self.send_message(req.get("to", "all"), req["text"])
        if cmd == "file":
            if not os.path.isfile(req["path"]):
                return {"ok": False, "error": f"no file {req['path']}"}
            if not self.reachable(req.get("to", "all")):
                return {"ok": False, "error": "nobody to send it to (status lists the peers)"}
            asyncio.create_task(self.send_file(req["path"], req.get("to", "all")))
            return {"ok": True, "text": f"sending {human(os.path.getsize(req['path']))}, an event says when it is delivered"}
        if cmd == "status":
            return {"ok": True, "role": self.cfg["role"], "as": self.name, "link": self.cfg["link"], "method": self.cfg.get("method"),
                    "addresses": self.cfg.get("addresses"), "port": self.cfg.get("port"),
                    "peers": {n: list(v) for n, v in self.since.items()} if self.cfg["role"] == "host" else
                    ({getattr(self, "host_name", "host"): "connected"} if self.up else {}),
                    "sending": {v["name"]: f"{v['sent'] * 100 // max(1, v['size'])}% to {v['to']}" for v in self.sending.values()},
                    "receiving": {v["name"]: f"{v['got'] * 100 // max(1, v['size'])}% from {v['from']}" for v in self.incoming.values()},
                    "wake": {"cluster": self.cfg.get("cluster"), "notify": self.cfg.get("notify", False)}}
        if cmd == "stop":
            self.stopping.set()
            return {"ok": True}
        return {"ok": False, "error": f"unknown command {cmd}"}

    async def run(self) -> None:
        self.token = secrets.token_hex(16)
        ctl = await asyncio.start_server(self.control, "127.0.0.1", 0)
        private_write(os.path.join(self.dir, "control"), json.dumps({"port": ctl.sockets[0].getsockname()[1], "token": self.token, "pid": os.getpid()}))
        servers = [ctl]
        if self.cfg["role"] == "host":
            servers.append(await asyncio.start_server(self.accept, self.cfg["bind"], self.cfg["port"]))
            self.emit("hosting", text=f"{self.cfg['method']} {', '.join(self.cfg['addresses'])} port {self.cfg['port']}")
            task = None
        else:
            task = asyncio.create_task(self.member_loop())
        await self.stopping.wait()
        for s in servers:
            s.close()
        for ch in list(self.peers.values()) + ([self.up] if self.up else []):
            ch.close()
        if task:
            task.cancel()
        if self.cfg.get("upnp") and shutil.which("upnpc"):
            subprocess.run(["upnpc", "-d", str(self.cfg["port"]), "TCP"], capture_output=True, timeout=20)
        self.emit("stopped", text="daemon stopped")
        try:
            os.remove(os.path.join(self.dir, "control"))
        except OSError:
            pass

##### Commands #####

def format_event(e: dict, link: str) -> str:
    head = f"[session-link {link}] #{e['id']}"
    kind = e["kind"]
    if kind == "message":
        text = e["text"].replace("\n", " ⏎ ")
        more = f" [cut: link.py read {e['id']}]" if len(text) > 600 else ""
        return f"{head} message from {e['source']}" + ("" if e["to"] == "all" else f" (to {e['to']})") + f": {text[:600]}{more}"
    if kind == "file":
        state = "checked" if e["ok"] else "CORRUPT (size or sha256 mismatch)"
        return f"{head} file from {e['source']}: {e['path']} ({human(e['size'])}, {state})"
    if kind == "delivered":
        state = "ok" if e["ok"] else f"FAILED: {e.get('why', '')}"
        return f"{head} {e['what']} delivered to {e['source']}: {state}"
    if kind in ("joined", "left", "connected"):
        return f"{head} {e['source']} {kind}" + (f" from {e['address']}" if e.get("address") else "")
    if kind == "peers":
        return f"{head} on the link: {', '.join(e['names'])}"
    if kind == "refused":
        return f"{head} refused a connection from {e['address']}: {e['why']}"
    if kind == "sent":
        return f"{head} sent {e['what']} {e['name']} ({human(e['size'])}) to {e['to']}"
    return f"{head} {kind}: {e.get('text', '')}"

def control(folder: str, req: dict) -> dict:
    info = load_json(os.path.join(folder, "control"))
    if not info:
        die("no daemon running for this link (host or join first)")
    try:
        with socket.create_connection(("127.0.0.1", info["port"]), timeout=10) as s:
            s.sendall((json.dumps({**req, "token": info["token"]}) + "\n").encode())
            data = b""
            while not data.endswith(b"\n"):
                part = s.recv(65536)
                if not part:
                    break
                data += part
    except OSError:
        die("the daemon does not answer (stopped?): host or join again")
    return json.loads(data or b"{}")

def start_daemon(folder: str) -> None:
    log = open(os.path.join(folder, "daemon.log"), "a", encoding="utf-8")
    kwargs = {"creationflags": 0x00000008} if os.name == "nt" else {"start_new_session": True}
    subprocess.Popen([sys.executable, os.path.abspath(__file__), "_run", folder], stdout=log, stderr=log, stdin=subprocess.DEVNULL, **kwargs)
    for _ in range(100):
        if os.path.exists(os.path.join(folder, "control")):
            return
        time.sleep(0.1)
    die(f"the daemon did not start: see {os.path.join(folder, 'daemon.log')}")

def alive(folder: str) -> bool:
    info = load_json(os.path.join(folder, "control"))
    if not info:
        return False
    try:
        with socket.create_connection(("127.0.0.1", info["port"]), timeout=3):
            return True
    except OSError:
        return False

def prepare(folder: str, passphrase: str) -> None:
    if alive(folder):
        die(f"this link already runs: link.py --link {os.path.basename(folder)} stop first, or use another --link")
    os.makedirs(folder, mode=0o700, exist_ok=True)
    os.chmod(folder, 0o700)
    private_write(os.path.join(folder, "secret"), passphrase)

def cmd_host(a: argparse.Namespace, folder: str) -> None:
    passphrase = a.passphrase or os.environ.get("SESSION_LINK_PASSPHRASE") or "-".join(secrets.token_urlsafe(6) for _ in range(4))
    if len(passphrase) < 12:
        die("a passphrase of at least 12 characters (or none: a strong one is generated)")
    ts, lan = tailscale_ip(), lan_ip()
    method = a.method
    if method == "auto":
        method = "tailscale" if ts else "lan" if lan else "local"
    bind = {"tailscale": [ts], "lan": [lan], "public": [lan], "local": ["127.0.0.1"]}[method]
    if a.bind:
        bind = a.bind.split(",")
    elif method == "tailscale" and lan and a.method == "auto":
        bind = [ts, lan] # auto: the tailnet and the local network
    if not all(bind):
        die(f"no {method} address here (tailscale up? a local network?): choose another --method")
    port = free_port(bind, a.port)
    addresses = list(bind)
    public = ""
    if method == "public":
        public = upnp_map(lan, port)
        if public:
            addresses.insert(0, public)
    prepare(folder, passphrase)
    cfg = {"role": "host", "link": os.path.basename(folder), "as": a.as_name or socket.gethostname().split(".")[0],
           "method": method, "bind": bind, "addresses": addresses, "port": port, "cluster": a.cluster, "notify": a.notify,
           "upnp": bool(public)}
    private_write(os.path.join(folder, "config.json"), json.dumps(cfg, indent=2))
    start_daemon(folder)
    print(f"hosting link {cfg['link']!r} as {cfg['as']!r} ({method}), port {port}")
    for addr in addresses:
        kind = "public" if addr == public else "tailscale" if addr == ts else "lan" if addr == lan else "local"
        print(f"  {kind:9} {addr}:{port}")
    if method == "public" and not public:
        print("  public: the router refused UPnP - forward the port by hand, then join with PUBLIC-IP:" + str(port))
    print(f"join code  {encode_code(addresses, port, cfg['link'])}")
    print(f"passphrase {passphrase}   (give it apart from the code; `info --secret` shows it again)")
    hint = firewall_hint(port)
    if hint and method != "local":
        print(f"firewall   the port is closed: open it with root: {hint}")

def cmd_join(a: argparse.Namespace, folder: str) -> None:
    passphrase = a.passphrase or os.environ.get("SESSION_LINK_PASSPHRASE")
    if not passphrase:
        die("the passphrase of the host is needed (--passphrase or SESSION_LINK_PASSPHRASE)")
    addresses, port, _ = decode_target(a.target)
    prepare(folder, passphrase)
    cfg = {"role": "member", "link": os.path.basename(folder), "as": a.as_name or socket.gethostname().split(".")[0],
           "method": "join", "addresses": addresses, "port": port, "cluster": a.cluster, "notify": a.notify}
    private_write(os.path.join(folder, "config.json"), json.dumps(cfg, indent=2))
    before = len(read_events(folder))
    start_daemon(folder)
    for _ in range(60): # wait for the handshake (only the events of this run)
        done = [e for e in read_events(folder)[before:] if e["kind"] in ("connected", "error")]
        if done:
            print(format_event(done[-1], cfg["link"]))
            if done[-1]["kind"] == "error":
                sys.exit(1)
            return
        time.sleep(0.5)
    print(f"not connected yet: the daemon keeps trying {', '.join(addresses)} port {port} (status / events)")

def read_events(folder: str) -> list:
    path = os.path.join(folder, "events.jsonl")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return [json.loads(l) for l in f if l.strip()]

def cmd_events(a: argparse.Namespace, folder: str) -> None:
    path = os.path.join(folder, "events.jsonl")
    link = os.path.basename(folder)
    show = lambda e: print(json.dumps(e, ensure_ascii=False) if a.json else format_event(e, link), flush=True)
    events = read_events(folder)
    if not a.new:
        for e in events:
            if e["kind"] not in ("peers",) or a.json:
                show(e)
    if not a.follow:
        return
    open(path, "a", encoding="utf-8").close()
    with open(path, encoding="utf-8") as f:
        f.seek(0, os.SEEK_END)
        while True:
            line = f.readline()
            if not line:
                time.sleep(0.5)
                continue
            try:
                e = json.loads(line)
            except ValueError:
                continue
            if e["kind"] not in ("peers", "sent") or a.json:
                show(e)

def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "_run":
        if ChaCha20Poly1305 is None:
            die("python3-cryptography is needed")
        asyncio.run(Node(sys.argv[2]).run())
        return 0
    ap = argparse.ArgumentParser(description="An encrypted link between Claude sessions on several computers")
    ap.add_argument("--link", default="default", help="the link's name (several links can run at once)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    h = sub.add_parser("host")
    h.add_argument("--method", default="auto", choices=["auto", "tailscale", "lan", "public", "local"])
    h.add_argument("--port", type=int, default=0)
    h.add_argument("--bind", default="")
    j = sub.add_parser("join")
    j.add_argument("target")
    for p in (h, j):
        p.add_argument("--passphrase", default="")
        p.add_argument("--as", dest="as_name", default="")
        p.add_argument("--cluster", default="", help="claude-cluster session woken up by each message / file")
        p.add_argument("--notify", action="store_true", help="a desktop notification for each message / file")
    s = sub.add_parser("send")
    s.add_argument("--to", default="all")
    s.add_argument("text", nargs="+")
    fl = sub.add_parser("file")
    fl.add_argument("path")
    fl.add_argument("--to", default="all")
    sub.add_parser("status")
    i = sub.add_parser("info")
    i.add_argument("--secret", action="store_true")
    ev = sub.add_parser("events")
    ev.add_argument("--follow", action="store_true")
    ev.add_argument("--new", action="store_true")
    ev.add_argument("--json", action="store_true")
    r = sub.add_parser("read")
    r.add_argument("id", type=int)
    sub.add_parser("stop")
    a = ap.parse_args()
    if ChaCha20Poly1305 is None and a.cmd in ("host", "join"):
        die("python3-cryptography is needed (dnf install python3-cryptography, or pip install --user cryptography)")
    folder = link_dir(a.link)
    if a.cmd == "host":
        cmd_host(a, folder)
    elif a.cmd == "join":
        cmd_join(a, folder)
    elif a.cmd == "send":
        res = control(folder, {"cmd": "send", "to": a.to, "text": " ".join(a.text)})
        print("sent" if res.get("ok") else f"Error: {res.get('error')}")
        return 0 if res.get("ok") else 1
    elif a.cmd == "file":
        res = control(folder, {"cmd": "file", "to": a.to, "path": os.path.abspath(a.path)})
        print(res.get("text", "") if res.get("ok") else f"Error: {res.get('error')}")
        return 0 if res.get("ok") else 1
    elif a.cmd == "status":
        print(json.dumps(control(folder, {"cmd": "status"}), indent=2, ensure_ascii=False))
    elif a.cmd == "info":
        cfg = load_json(os.path.join(folder, "config.json")) or die("no such link")
        print(f"link {cfg['link']!r}, {cfg['role']} as {cfg['as']!r}; join code {encode_code(cfg['addresses'], cfg['port'], cfg['link'])}")
        if a.secret:
            with open(os.path.join(folder, "secret"), encoding="utf-8") as f:
                print(f"passphrase {f.read().strip()}")
    elif a.cmd == "events":
        try:
            cmd_events(a, folder)
        except KeyboardInterrupt:
            pass
    elif a.cmd == "read":
        found = [e for e in read_events(folder) if e["id"] == a.id]
        print(found[0].get("text", json.dumps(found[0])) if found else f"no event #{a.id}")
    elif a.cmd == "stop":
        res = control(folder, {"cmd": "stop"})
        print("stopped" if res.get("ok") else f"Error: {res.get('error')}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
