# OceanGram — состояние работ (для следующей сессии), 2026-10-27

## Сервер (боевой, старый сервер 159.194.214.209)
- С 2026-10-26 16:56 UTC работает og13: `ghcr.io/iamxvbaba/gramsrv/server:og13u-20261026` (тег `main`),
  схема 20261026010001. Исходники: gramsrv ветка `og13` (на VPS 31.77.251.231: /root/og13/gramsrv, экспорт /root/og13/merge).
- Деплой на бой — только с явного «да» пользователя в чате. Перед деплоем сверить запущенный образ
  (`docker inspect gramsrv-main-server-1`), другие сессии тоже деплоят. Диск ~95 %, дампы держать на VPS.
- Бэкап базы до переключения: VPS /root/og13/db/pre-og13-20261026-165306.dump. Записка на сервере: /root/og13-deploy/README.txt.

## Android (ilitovdanya/oceangram-android, ветка og13)
- Финальный APK `com.oceangram.messenger` 13.0.3 (ставится поверх 12.10.0): Actions → build-apk-og13.yml → Run workflow, release = true.
- Готово: Деньги (GRAM в чатах), плашка «Баланс», Premium (ник/номер/юзернеймы и NFT), OceanSoft, Custom 1 и 2.

## ПК (этот репозиторий, ветка og13) — Telegram Desktop 7.3.1, без Custom
- Код: patch/files/Telegram/SourceFiles/oceangram/* + patch/apply.py шаги 4–10 (проверены на исходниках 7.3.1, но ещё не компилировались).
  Деньги (отправка GRAM через «OGT1», плашка баланса), OceanGram Premium (переливание + окно в настройках), OceanSoft (призрак, удалённые с 🗑).
- Сборка: Actions → «Build OceanGram Desktop» → Run workflow, ветка og13. Этап Libraries собирает Qt/WebRTC/FFmpeg/Rust с нуля
  (до 6 ч на прогон): если прогон кончился «Libraries are not finished» — просто запустить снова, прогресс сохраняется в кэше.
  Когда Libraries готовы, job «OceanGram.exe» собирает установщик (артефакт OceanGram-7.3.1-windows).
- Если EXE упал на компиляции — ошибки в логе job «OceanGram.exe» (`error C…`), чинить файлы в patch/files/…/oceangram.

## Дальше по плану пользователя
1. Довести EXE до установщика, отдать пользователю.
2. Потом iOS: база iamxvbaba/gramsrv-Telegram-iOS (Telegram iOS 12.8, слой 227). Нужно спросить, как ставить
   (Sideloadly/AltStore, аккаунт разработчика или TrollStore) и какой айфон/iOS.
