# OceanGram Desktop

Builds OceanGram for Windows from [iamxvbaba/gramsrv-tdesktop](https://github.com/iamxvbaba/gramsrv-tdesktop)
(Telegram Desktop 7.2.9, layer 229). The upstream commit is pinned in `.github/workflows/build-exe.yml`.

`patch/apply.py` changes the upstream source:

- server: DC 2 at `31.77.251.231:2398`, no built-in IPv6 addresses;
- RSA: the server's current (test) key and the OceanGram key;
- name OceanGram: window title, file properties, data folder `%APPDATA%\OceanGram`;
- its own Windows ids (installer AppId, notification AppUserModelID, toast activator, shortcuts), so it
  installs next to Telegram Desktop without sharing settings or notifications.

Auto-update and crash reports are turned off at build time, so the app never replaces itself with Telegram.

## Build

Actions → Build OceanGram Desktop → Run workflow. The first run builds the libraries (Qt, WebRTC, FFmpeg...)
and takes several hours. If the Libraries job runs out of time, run the workflow again: it continues from
the saved progress. The result is attached to the run: `OceanGram-Setup-<version>.exe` and a portable zip.
