"""Check the actual protocol and connection settings before compiling."""
import ipaddress
import json
import os
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
ip = str(ipaddress.ip_address(os.environ.get("OG_SERVER_IP", "2.26.197.16")))
expected_layer = int(os.environ.get("API_LAYER", "232"))
java = root / "TMessagesProj/src/main/java/org/telegram/tgnet/TLRPC.java"
if java.exists():
    layer = int(re.search(r"\bLAYER\s*=\s*(\d+)", java.read_text(encoding="utf-8"))[1])
    cpp = (root / "TMessagesProj/jni/tgnet/ConnectionsManager.cpp").read_text(encoding="utf-8")
    bootstrap = cpp.split("void ConnectionsManager::initDatacenters() {", 1)[1].split("void ConnectionsManager::attachConnection", 1)[0]
    addresses = re.findall(r'TcpAddress\("([^"]+)",\s*(\d+)', bootstrap)
    if addresses != [(ip, "2398")]:
        raise SystemExit(f"Unexpected bootstrap DCs: {addresses!r}")
    handshake = (root / "TMessagesProj/jni/tgnet/Handshake.cpp").read_text(encoding="utf-8")
    for fingerprint in ["0x9dbe5a4c45febeff", "0x74cddb5c5f509b24"]:
        if fingerprint not in handshake:
            raise SystemExit(f"Missing server RSA key: {fingerprint}")
    package = re.search(r"(?m)^APP_PACKAGE=(.+)$", (root / "gradle.properties").read_text())[1].strip()
    if package != os.environ.get("OG_PACKAGE", "com.oceangram.messenger"):
        raise SystemExit(f"Unexpected Android package: {package}")
    platform = "android"
else:
    schema = (root / "Telegram/SourceFiles/mtproto/scheme/api.tl").read_text(encoding="utf-8")
    layer = int(re.search(r"// LAYER (\d+)", schema)[1])
    cpp = (root / "Telegram/SourceFiles/mtproto/mtproto_dc_options.cpp").read_text(encoding="utf-8")
    bootstrap = re.search(r"const BuiltInDc kBuiltInDcs\[\] = \{(.*?)\n\};", cpp, re.S)[1]
    addresses = re.findall(r'\{\s*2,\s*"([^"]+)",\s*(\d+)\s*\}', bootstrap)
    if addresses != [(ip, "2398")]:
        raise SystemExit(f"Unexpected bootstrap DCs: {addresses!r}")
    platform = "windows"
if layer != expected_layer:
    raise SystemExit(f"Protocol layer {layer}, expected {expected_layer}")
manifest = dict(platform=platform, layer=layer, server_ip=ip, server_port=2398,
                app_version=os.environ.get("APP_VERSION"),
                upstream_repository=os.environ.get("UPSTREAM_REPO"),
                upstream_commit=os.environ.get("UPSTREAM_SHA"),
                oceangram_commit=os.environ.get("GITHUB_SHA"))
(root / "oceangram-build.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(json.dumps(manifest, indent=2))
