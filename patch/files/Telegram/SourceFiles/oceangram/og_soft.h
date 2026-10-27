/*
OceanGram Desktop: OceanSoft — ghost mode (no «read», no story views, no
«online») and messages others delete kept with 🗑 before the time. The
subscription is the server's (oceangram/soft*: 2500 Stars a week, one free
day, free with OceanGram Premium); the switches are this computer's.
*/
#pragma once

#include <QtCore/QJsonObject>

class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace Oceangram {

struct SoftOptions {
	bool noRead = true;
	bool noStories = true;
	bool hideOnline = false;
	bool keepDeleted = true;
};

[[nodiscard]] SoftOptions &Soft();
void SaveSoft();

struct SoftState {
	bool active = false;
	QString source;
	int64 until = 0;
	bool autoRenew = false;
	bool trialAvailable = false;
	int64 price = 2500;
	int64 stars = 0;
};

[[nodiscard]] SoftState SoftStateFor(not_null<Main::Session*> session);
void ApplySoftState(not_null<Main::Session*> session, const QJsonObject &json);
void RefreshSoft(not_null<Main::Session*> session, Fn<void()> done = nullptr);
[[nodiscard]] bool SoftActive(not_null<Main::Session*> session);

[[nodiscard]] bool GhostRead(not_null<Main::Session*> session);
[[nodiscard]] bool GhostStories(not_null<Main::Session*> session);
[[nodiscard]] bool GhostOnline(not_null<Main::Session*> session);
[[nodiscard]] bool KeepDeleted(not_null<Main::Session*> session);

void MarkDeleted(not_null<HistoryItem*> item);
[[nodiscard]] bool IsDeleted(not_null<const HistoryItem*> item);

} // namespace Oceangram
