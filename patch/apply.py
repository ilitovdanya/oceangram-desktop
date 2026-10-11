#!/usr/bin/env python3
"""Turns Telegram Desktop into OceanGram Desktop.

Usage: apply.py <path to tdesktop checkout>

Every edit checks that its anchor exists, so a changed upstream fails the
build here instead of producing an EXE that talks to the wrong server.
"""
import pathlib
import re
import sys

SERVER_IP = "31.77.251.231"
SERVER_PORT = 2398
APP_NAME = "OceanGram"
# Installer AppId; also read by the app to tell installed from portable.
APP_ID = "{C2D45D55-AD63-48A5-A9ED-B39E7329F48C}"
TOAST_ACTIVATOR_CLSID = "A664294F-1880-4788-BF41-1537E350615D"
APP_USER_MODEL_ID = "OceanGram.OceanGramDesktop"

# The server's current key (the published test key) and the OceanGram key
# (public half of /root/rsa_keys/oceangram-2026/server_rsa.pem on the server).
SERVER_KEYS = [
    [
        "MIIBCgKCAQEAxF//0M0+/5PzgdNagTX+J+dJgr75ZCTuiG8i4x7YwmJF+jiOGCjm",
        "7X7BLCaMc1+hOZYDL3+Gvle/AKykW1qouaCJMVx/H+2l8LFXLelZ2PLawTb8A7Bl",
        "TqWzL3db5BugMNWziL9TuhR8In1bwKY07QVpR9in5zjAsAGLBk+mGt0DnVyMf1Xo",
        "p2lLCFNmm0F4ykcAeaLCCIPbGWddliLY8xEEhI4GO2l1U3kZMwIOdOnAGJFtgUAo",
        "Te+FHR6F1s9adCVZB1teL/hf9R+WmekJwygVz0MYEH7y6U49T45+/W7OF6X6g0W0",
        "j1uSSrsY4qN7twxbTad9zdGZ7ys+9v+PuQIDAQAB",
    ],
    [
        "MIIBCgKCAQEAwSyt1s52Ad/NRNIEJyQX4n2sYgDMaDx8t9KsmnB7lFy51X6mYCEY",
        "VB6gah9qUzt5m/r4klQFLtvt5ws4ON7jkXBNClAVI7ZD9UoCTxA43FUlzIY90cpf",
        "y3uiFPdJlQxUiCV/9oTBS8oesTRXXZ73LTfBEM8+itcPwhsE0GCEKTaqAENX3kLp",
        "lQL0tvE2lYUs5Y+ZHWj1kpKBRDRN0g4QYYZ31e1im9r0yL95qQ/PSxrZ8x33nue1",
        "MBXwxHFmD+q7C7gVeOqSWmsU05mOIax2RDYj+p0p9KX8QvL7BSaPt6L0/50dCvEm",
        "g6AEOFGfJ6Ct9gOuhQJEbepK4Vf+tHJ9rwIDAQAB",
    ],
]

root = pathlib.Path(sys.argv[1]).resolve()
log = []


def read(path):
    # newline="" keeps each file's own line endings.
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def edit(rel, fn):
    path = root / rel
    text = read(path)
    new = fn(text)
    if new == text:
        sys.exit(f"patch: nothing changed in {rel}")
    write(path, new)


def replace(rel, old, new, count=None):
    def fn(text):
        n = text.count(old)
        if n == 0 or (count is not None and n != count):
            sys.exit(f"patch: {rel}: expected {count or 'some'} of {old!r}, found {n}")
        log.append(f"{rel}: {n}x {old!r}")
        return text.replace(old, new)
    edit(rel, fn)


def resub(rel, pattern, repl, count=1, flags=0):
    def fn(text):
        new, n = re.subn(pattern, lambda m: repl, text, flags=flags)
        if n != count:
            sys.exit(f"patch: {rel}: expected {count} match(es) of {pattern!r}, found {n}")
        log.append(f"{rel}: {n}x /{pattern}/")
        return new
    edit(rel, fn)


# 1. Server: one built-in DC (DC 2), no IPv6 addresses, and the server's RSA keys.
DC = "Telegram/SourceFiles/mtproto/mtproto_dc_options.cpp"
resub(DC, r"const BuiltInDc kBuiltInDcs\[\] = \{.*?\n\};",
      f'const BuiltInDc kBuiltInDcs[] = {{\n\t{{ 2, "{SERVER_IP}", {SERVER_PORT} }},\n}};', flags=re.S)
replace(DC, "gsl::make_span(kBuiltInDcsIPv6).subspan(0)", "gsl::make_span(kBuiltInDcsIPv6).subspan(0, 0)", 1)


def pem(lines):
    body = "".join(f"{line}\\n\\\n" for line in lines)
    return f'"\\\n-----BEGIN RSA PUBLIC KEY-----\\n\\\n{body}-----END RSA PUBLIC KEY-----"'


resub(DC, r'const char \*kPublicRSAKeys\[\] = \{ ".*?" \};',
      "const char *kPublicRSAKeys[] = { " + ", ".join(pem(k) for k in SERVER_KEYS) + " };", flags=re.S)

# 2. Name. AppName is also the data folder (%APPDATA%\OceanGram), so the app
#    never touches a real Telegram Desktop profile on the same PC.
V = "Telegram/SourceFiles/core/version.h"
resub(V, r'constexpr auto AppId = "\{[0-9A-F-]+\}"_cs;', f'constexpr auto AppId = "{APP_ID}"_cs;')
replace(V, 'constexpr auto AppNameOld = "Telegram Win (Unofficial)"_cs;',
        f'constexpr auto AppNameOld = "{APP_NAME} (Old)"_cs;', 1)
replace(V, 'constexpr auto AppName = "Telegram Desktop"_cs;', f'constexpr auto AppName = "{APP_NAME}"_cs;', 1)
replace(V, 'constexpr auto AppFile = "Telegram"_cs;', f'constexpr auto AppFile = "{APP_NAME}"_cs;', 1)

replace("Telegram/SourceFiles/window/main_window.cpp", 'u"Telegram"_q', f'u"{APP_NAME}"_q')
replace("Telegram/SourceFiles/window/window_restore_shell.cpp", 'u"Telegram"_q', f'u"{APP_NAME}"_q')

RC = "Telegram/Resources/winrc/Telegram.rc"
replace(RC, 'VALUE "CompanyName", "Telegram FZ-LLC"', f'VALUE "CompanyName", "{APP_NAME}"', 1)
replace(RC, 'VALUE "FileDescription", "Telegram Desktop"', f'VALUE "FileDescription", "{APP_NAME}"', 1)
replace(RC, 'VALUE "ProductName", "Telegram Desktop"', f'VALUE "ProductName", "{APP_NAME}"', 1)

# 3. Windows identity: own notification id, shortcuts and toast activator,
#    so notifications and pinned icons don't mix with Telegram Desktop.
AUM = "Telegram/SourceFiles/platform/win/windows_app_user_model_id.cpp"
replace(AUM, 'L"Telegram.TelegramDesktop', f'L"{APP_USER_MODEL_ID}', 2)
replace(AUM, 'u"Telegram Desktop/Telegram.lnk"_q', f'u"{APP_NAME}/{APP_NAME}.lnk"_q', 1)
replace(AUM, 'u"Telegram.lnk"_q', f'u"{APP_NAME}.lnk"_q', 2)
replace("Telegram/SourceFiles/platform/win/windows_toast_activator.h",
        "F11932D3-6110-4BBC-9B02-B2EC07A1BD19", TOAST_ACTIVATOR_CLSID, 2)

print("\n".join(log))
print("patch: OK")
