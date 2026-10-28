#!/usr/bin/env python3
"""Turns Telegram Desktop into OceanGram Desktop.

Usage: apply.py <path to tdesktop checkout>

Every edit checks that its anchor exists, so a changed upstream fails the
build here instead of producing an EXE that talks to the wrong server.
"""
import pathlib
import os
import re
import sys

# workflow_dispatch input server_ip; the old server forwards to the new one after the move, so it is the safe default
SERVER_IP = os.environ.get("OG_SERVER_IP") or "159.194.214.209"
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
# Two production keys vs one test key: give the production span a dynamic extent,
# as upstream already does for kBuiltInDcs, so both branches of the ternary agree.
replace(DC, "\t\t: gsl::make_span(kPublicRSAKeys);", "\t\t: gsl::make_span(kPublicRSAKeys).subspan(0);", 1)

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


def insert_after(rel, pattern, make, count=1, flags=0):
    """After each match of pattern add make(match, newline); newline is the file's own line ending."""
    def fn(text):
        nl = "\r\n" if "\r\n" in text else "\n"
        new, n = re.subn(pattern, lambda m: m.group(0) + make(m, nl), text, flags=flags)
        if n != count:
            sys.exit(f"patch: {rel}: expected {count} match(es) of {pattern!r}, found {n}")
        log.append(f"{rel}: {n}x +after /{pattern}/")
        return new
    edit(rel, fn)


# 4. OceanGram's own sources (patch/files, same layout as the tdesktop tree) and their CMake entries.
files = pathlib.Path(__file__).resolve().parent / "files"
added = []
for src in sorted(files.rglob("*")):
    if src.is_file():
        rel = src.relative_to(files)
        dst = root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(src.read_bytes())
        added.append(rel.as_posix())
log.append("added " + ", ".join(added))
OG_SOURCES = sorted(p[len("Telegram/SourceFiles/"):] for p in added
                    if p.startswith("Telegram/SourceFiles/oceangram/") and p.endswith((".cpp", ".h")))
insert_after("Telegram/CMakeLists.txt", r"\n    wallet/wallet_panel\.h\r?\n",
             lambda m, nl: "".join(f"    {s}{nl}" for s in OG_SOURCES))

# 5. «Деньги»: sending GRAM to another account goes through the server's ledger (Oceangram::ShowSendGram,
#    "OGT1" intent in wallet.sendTransfer) instead of the wallet's local signing, which needs a seed phrase
#    this server never hands out. Balance, history and the chat messages stay the wallet's own.
WC = "Telegram/SourceFiles/wallet/wallet_content.cpp"
insert_after(WC, r'#include "wallet/wallet_content\.h"\r?\n', lambda m, nl: f'#include "oceangram/og_money.h"{nl}')
insert_after(WC, r"void ShowSendToUser\([^{;]*?\) \{\r?\n",
             lambda m, nl: (f"\tif (!user->isSelf()) {{{nl}"
                            f"\t\tOceangram::ShowSendGram(show, user, std::move(sent), amountNano);{nl}"
                            f"\t\treturn;{nl}"
                            f"\t}}{nl}"), flags=re.S)

# 6. Profile «Баланс: 💎 X ›» pill (Oceangram::AddBalancePill) right under the saved-music pill, as on Android.
PIW = "Telegram/SourceFiles/info/profile/info_profile_inner_widget.cpp"
insert_after(PIW, r'#include "info/profile/info_profile_inner_widget\.h"\r?\n',
             lambda m, nl: f'#include "oceangram/og_balance.h"{nl}')
insert_after(PIW, r"\t\tAddSavedMusic\(\r?\n\t\t\tresult\.data\(\),\r?\n\t\t\t_controller,\r?\n\t\t\tmusicPeer,\r?\n"
                  r"\t\t\t_topBarColor\.value\(\)\);\r?\n",
             lambda m, nl: (f"\t\tif (const auto user = _peer->asUser()) {{{nl}"
                            f"\t\t\tOceangram::AddBalancePill({nl}"
                            f"\t\t\t\tresult.data(),{nl}"
                            f"\t\t\t\t_controller->parentController(),{nl}"
                            f"\t\t\t\tuser,{nl}"
                            f"\t\t\t\t_topBarColor.value());{nl}"
                            f"\t\t}}{nl}"))

# 7. OceanGram Premium: shimmering names in the chat list and the chat header (gradient pen before the
#    name is drawn), and in the profile the name, the phone number and the usernames (oceangram/og_premium).
DL = "Telegram/SourceFiles/dialogs/ui/dialogs_layout.cpp"
insert_after(DL, r'#include "dialogs/ui/dialogs_layout\.h"\r?\n', lambda m, nl: f'#include "oceangram/og_premium.h"{nl}')
insert_after(DL, r"\t\t\t: st::dialogsNameFg\);\r?\n(?=\t\trowName\.draw\(p, \{\r?\n\t\t\t\.position = rectForName\.topLeft\(\),\r?\n"
                 r"\t\t\t\.availableWidth = rectForName\.width\(\),\r?\n\t\t\t\.elisionLines = 1,\r?\n\t\t\}\);\r?\n"
                 r"\t\tif \(drawMuteIcon\))",
             lambda m, nl: (f"\t\tOceangram::SetShimmerPen(p, from, Oceangram::Field::Nick, QRect({nl}"
                            f"\t\t\trectForName.topLeft(),{nl}"
                            f"\t\t\tQSize({nl}"
                            f"\t\t\t\tstd::min(rowName.maxWidth(), rectForName.width()),{nl}"
                            f"\t\t\t\trectForName.height())));{nl}"))
TB = "Telegram/SourceFiles/history/view/history_view_top_bar_widget.cpp"
insert_after(TB, r'#include "history/view/history_view_top_bar_widget\.h"\r?\n',
             lambda m, nl: f'#include "oceangram/og_premium.h"{nl}')
insert_after(TB, r"\t\tp\.setPen\(st::dialogsNameFg\);\r?\n(?=\t\t_title\.draw\(p, \{\r?\n\t\t\t\.position = \{ nameleft, nametop \},)",
             lambda m, nl: (f"\t\tOceangram::SetShimmerPen(p, namePeer, Oceangram::Field::Nick, QRect({nl}"
                            f"\t\t\tnameleft,{nl}"
                            f"\t\t\tnametop,{nl}"
                            f"\t\t\tstd::min(namewidth, _title.maxWidth()),{nl}"
                            f"\t\t\tst::msgNameStyle.font->height));{nl}"))
PTB = "Telegram/SourceFiles/info/profile/info_profile_top_bar.cpp"
insert_after(PTB, r'#include "info/profile/info_profile_top_bar\.h"\r?\n',
             lambda m, nl: f'#include "oceangram/og_premium.h"{nl}')
insert_after(PTB, r"\t_title->setSelectable\(true\);\r?\n",
             lambda m, nl: (f"\tif (const auto user = _peer->asUser()) {{{nl}"
                            f"\t\tOceangram::MarkLabel(_title.data(), user, Oceangram::Field::Nick);{nl}"
                            f"\t}}{nl}"))
PA = "Telegram/SourceFiles/info/profile/info_profile_actions.cpp"
insert_after(PA, r'#include "info/profile/info_profile_actions\.h"\r?\n',
             lambda m, nl: f'#include "oceangram/og_premium.h"{nl}')
insert_after(PA, r"\t\t\tphoneLabel->setContextMenuHook\(hook\);\r?\n",
             lambda m, nl: f"\t\t\tOceangram::MarkLabel(phoneLabel, user, Oceangram::Field::Number);{nl}")
insert_after(PA, r"\t\tusernameLine\.subtext->setContextMenuHook\(lnkHook\);\r?\n",
             lambda m, nl: (f"\t\tOceangram::MarkLabel(usernameLine.text, user, Oceangram::Field::Username);{nl}"
                            f"\t\tOceangram::MarkLabel(usernameLine.subtext, user, Oceangram::Field::UsernameAlso);{nl}"))


def include_og(rel, own_header, og_header):
    insert_after(rel, r'#include "' + re.escape(own_header) + r'"\r?\n',
                 lambda m, nl: f'#include "{og_header}"{nl}')


# 8. Settings rows «OceanGram Premium» and «OceanSoft» (oceangram/og_settings) before the Premium section,
#    in the settings page and in its search index.
SM = "Telegram/SourceFiles/settings/sections/settings_main.cpp"
include_og(SM, "settings/sections/settings_main.h", "oceangram/og_settings.h")
insert_after(SM, r"\r?\n(?=\t+BuildPremiumSection\(builder\);)",
             lambda m, nl: m.string[m.end():m.end() + 8].split("B")[0] + "Oceangram::BuildSettingsSection(builder);" + nl,
             count=2)

# 9. OceanSoft ghost mode: «read» (chats, comments), story views and «online» stay on this computer while
#    the subscription is on and the switch is set (oceangram/og_soft).
DH = "Telegram/SourceFiles/data/data_histories.cpp"
include_og(DH, "data/data_histories.h", "oceangram/og_soft.h")
insert_after(DH, r"\r?\n(?=\t\tif \(const auto channel = history->peer->asChannel\(\)\) \{\r?\n"
                 r"\t\t\treturn session\(\)\.api\(\)\.request\(MTPchannels_ReadHistory\()",
             lambda m, nl: (f"\t\tif (Oceangram::GhostRead(&session())) {{{nl}"
                            f"\t\t\t// OceanSoft ghost mode: read here, tell nobody.{nl}"
                            f"\t\t\tcrl::on_main(&session(), finished);{nl}"
                            f"\t\t\treturn mtpRequestId(0);{nl}"
                            f"\t\t}}{nl}"))
RL = "Telegram/SourceFiles/data/data_replies_list.cpp"
include_og(RL, "data/data_replies_list.h", "oceangram/og_soft.h")
insert_after(RL, r"\r?\n(?=\t_readRequestId = api->request\(MTPmessages_ReadDiscussion\()",
             lambda m, nl: (f"\tif (Oceangram::GhostRead(&_history->session())) {{{nl}"
                            f"\t\treturn;{nl}"
                            f"\t}}{nl}"))
DS = "Telegram/SourceFiles/data/data_stories.cpp"
include_og(DS, "data/data_stories.h", "oceangram/og_soft.h")
insert_after(DS, r"\r?\n(?=\tconst auto api = &_owner->session\(\)\.api\(\);\r?\n\tapi->request\(MTPstories_ReadStories\()",
             lambda m, nl: (f"\tif (Oceangram::GhostStories(&_owner->session())) {{{nl}"
                            f"\t\tcrl::on_main(&_owner->session(), finish);{nl}"
                            f"\t\treturn;{nl}"
                            f"\t}}{nl}"))
insert_after(DS, r"\r?\n(?=\t\tapi->request\(MTPstories_IncrementStoryViews\()",
             lambda m, nl: (f"\t\tif (Oceangram::GhostStories(&_owner->session())) {{{nl}"
                            f"\t\t\t_incrementViewsPending.remove(peer);{nl}"
                            f"\t\t\tcrl::on_main(&_owner->session(), finish);{nl}"
                            f"\t\t\tcontinue;{nl}"
                            f"\t\t}}{nl}"))
AU = "Telegram/SourceFiles/api/api_updates.cpp"
include_og(AU, "api/api_updates.h", "oceangram/og_soft.h")
insert_after(AU, r"\tbool isOnline = Core::App\(\)\.hasActiveWindow\(&session\(\)\);\r?\n",
             lambda m, nl: (f"\tif (Oceangram::GhostOnline(&session())) {{{nl}"
                            f"\t\tisOnline = false;{nl}"
                            f"\t}}{nl}"))

# 10. OceanSoft: messages the other side deletes stay, with 🗑 before the time (BottomInfo).
DSN = "Telegram/SourceFiles/data/data_session.cpp"
include_og(DSN, "data/data_session.h", "oceangram/og_soft.h")
insert_after(DSN, r"\r?\n(?=\tif \(!toDestroy\.empty\(\)\) \{\r?\n\t\tnotifyItemsAboutToBeDestroyed\(toDestroy\);)",
             lambda m, nl: (f"\tif (!toDestroy.empty() && Oceangram::KeepDeleted(&session())) {{{nl}"
                            f"\t\t// OceanSoft: what others delete stays here, marked.{nl}"
                            f"\t\tauto own = std::vector<not_null<HistoryItem*>>();{nl}"
                            f"\t\tfor (const auto &item : toDestroy) {{{nl}"
                            f"\t\t\tif (item->out()) {{{nl}"
                            f"\t\t\t\town.push_back(item);{nl}"
                            f"\t\t\t}} else {{{nl}"
                            f"\t\t\t\tOceangram::MarkDeleted(item);{nl}"
                            f"\t\t\t}}{nl}"
                            f"\t\t}}{nl}"
                            f"\t\ttoDestroy = std::move(own);{nl}"
                            f"\t}}{nl}"), count=2)
BIH = "Telegram/SourceFiles/history/view/history_view_bottom_info.h"
insert_after(BIH, r"\t\tEffectId effectId = 0;\r?\n", lambda m, nl: f"\t\tbool ogDeleted = false;{nl}")
BI = "Telegram/SourceFiles/history/view/history_view_bottom_info.cpp"
include_og(BI, "history/view/history_view_bottom_info.h", "oceangram/og_soft.h")
insert_after(BI, r"\tresult\.effectId = item->effectId\(\);\r?\n",
             lambda m, nl: f"\tresult.ogDeleted = Oceangram::IsDeleted(item);{nl}")
insert_after(BI, r"\tauto marked = TextWithEntities\(\);\r?\n",
             lambda m, nl: (f"\tif (_data.ogDeleted) {{{nl}"
                            f"\t\tif (const auto trash = Ui::Emoji::Find(QString::fromUtf8(\"\\xf0\\x9f\\x97\\x91\"))) {{{nl}"
                            f"\t\t\tmarked.append(helper.image({{{nl}"
                            f"\t\t\t\t.image = Ui::Emoji::SinglePixmap({nl}"
                            f"\t\t\t\t\ttrash,{nl}"
                            f"\t\t\t\t\tUi::Emoji::GetSizeNormal()).toImage().scaledToHeight({nl}"
                            f"\t\t\t\t\t\tst::stakeIconEmojiSize * style::DevicePixelRatio(),{nl}"
                            f"\t\t\t\t\t\tQt::SmoothTransformation),{nl}"
                            f"\t\t\t\t.margin = QMargins(0, st::stakeIconEmojiTop, 0, 0),{nl}"
                            f"\t\t\t\t.textColor = false,{nl}"
                            f"\t\t\t}})).append(u\" \"_q);{nl}"
                            f"\t\t}}{nl}"
                            f"\t}}{nl}"))

print("\n".join(log))
print("patch: OK")
