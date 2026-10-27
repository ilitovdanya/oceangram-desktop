/*
OceanGram Desktop: OceanSoft.
*/
#include "oceangram/og_soft.h"

#include "oceangram/og_api.h"
#include "base/flat_set.h"
#include "base/unixtime.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "settings.h"

#include <QtCore/QSettings>

#include <unordered_map>

namespace Oceangram {
namespace {

constexpr auto kRefreshEvery = 10 * 60 * crl::time(1000);

struct Cache {
	SoftOptions options;
	bool optionsLoaded = false;
	std::unordered_map<uint64, SoftState> states;
	std::unordered_map<uint64, crl::time> loadedAt;
	base::flat_set<uint64> loading;
	base::flat_set<FullMsgId> deleted;
};

[[nodiscard]] Cache &SoftCache() {
	static const auto instance = new Cache();
	return *instance;
}

[[nodiscard]] QString StorePath() {
	return cWorkingDir() + u"tdata/oceangram.ini"_q;
}

[[nodiscard]] QString StateKey(uint64 userId, const QString &field) {
	return u"soft_"_q + QString::number(userId) + '/' + field;
}

[[nodiscard]] SoftState LoadState(uint64 userId) {
	auto store = QSettings(StorePath(), QSettings::IniFormat);
	auto result = SoftState();
	result.active = store.value(StateKey(userId, u"active"_q), false).toBool();
	result.source = store.value(StateKey(userId, u"source"_q)).toString();
	result.until = store.value(StateKey(userId, u"until"_q), 0).toLongLong();
	return result;
}

void SaveState(uint64 userId, const SoftState &state) {
	auto store = QSettings(StorePath(), QSettings::IniFormat);
	store.setValue(StateKey(userId, u"active"_q), state.active);
	store.setValue(StateKey(userId, u"source"_q), state.source);
	store.setValue(StateKey(userId, u"until"_q), qint64(state.until));
}

[[nodiscard]] SoftState &StateRef(uint64 userId) {
	auto &states = SoftCache().states;
	auto i = states.find(userId);
	if (i == end(states)) {
		i = states.emplace(userId, LoadState(userId)).first;
	}
	return i->second;
}

} // namespace

SoftOptions &Soft() {
	auto &data = SoftCache();
	if (!data.optionsLoaded) {
		data.optionsLoaded = true;
		auto store = QSettings(StorePath(), QSettings::IniFormat);
		auto &o = data.options;
		o.noRead = store.value(u"soft/no_read"_q, o.noRead).toBool();
		o.noStories = store.value(u"soft/no_stories"_q, o.noStories).toBool();
		o.hideOnline = store.value(u"soft/hide_online"_q, o.hideOnline).toBool();
		o.keepDeleted = store.value(u"soft/keep_deleted"_q, o.keepDeleted).toBool();
	}
	return data.options;
}

void SaveSoft() {
	const auto &o = Soft();
	auto store = QSettings(StorePath(), QSettings::IniFormat);
	store.setValue(u"soft/no_read"_q, o.noRead);
	store.setValue(u"soft/no_stories"_q, o.noStories);
	store.setValue(u"soft/hide_online"_q, o.hideOnline);
	store.setValue(u"soft/keep_deleted"_q, o.keepDeleted);
}

SoftState SoftStateFor(not_null<Main::Session*> session) {
	return StateRef(session->userId().bare);
}

void ApplySoftState(
		not_null<Main::Session*> session,
		const QJsonObject &json) {
	const auto id = session->userId().bare;
	auto &state = StateRef(id);
	state.active = json.value(u"active"_q).toBool();
	state.source = json.value(u"source"_q).toString();
	state.until = json.value(u"until"_q).toInteger();
	state.autoRenew = json.value(u"auto_renew"_q).toBool();
	state.trialAvailable = json.value(u"trial_available"_q).toBool();
	state.price = json.value(u"price"_q).toInteger(2500);
	state.stars = json.value(u"stars"_q).toInteger();
	SoftCache().loadedAt[id] = crl::now();
	SaveState(id, state);
}

void RefreshSoft(not_null<Main::Session*> session, Fn<void()> done) {
	const auto id = session->userId().bare;
	auto &data = SoftCache();
	if (data.loading.contains(id) && !done) {
		return;
	}
	data.loading.emplace(id);
	RequestOcean(session, u"soft"_q, QString(), QString(), [=](
			QJsonObject json) {
		SoftCache().loading.remove(id);
		ApplySoftState(session, json);
		if (done) {
			done();
		}
	}, [=](QString) {
		auto &data = SoftCache();
		data.loading.remove(id);
		data.loadedAt[id] = crl::now();
		if (done) {
			done();
		}
	});
}

bool SoftActive(not_null<Main::Session*> session) {
	const auto id = session->userId().bare;
	auto &data = SoftCache();
	const auto i = data.loadedAt.find(id);
	if (i == end(data.loadedAt) || crl::now() - i->second > kRefreshEvery) {
		RefreshSoft(session);
	}
	const auto &state = StateRef(id);
	return state.active
		&& (!state.until || state.until > base::unixtime::now());
}

bool GhostRead(not_null<Main::Session*> session) {
	return Soft().noRead && SoftActive(session);
}

bool GhostStories(not_null<Main::Session*> session) {
	return Soft().noStories && SoftActive(session);
}

bool GhostOnline(not_null<Main::Session*> session) {
	return Soft().hideOnline && SoftActive(session);
}

bool KeepDeleted(not_null<Main::Session*> session) {
	return Soft().keepDeleted && SoftActive(session);
}

void MarkDeleted(not_null<HistoryItem*> item) {
	SoftCache().deleted.emplace(item->fullId());
	auto &owner = item->history()->owner();
	owner.notifyItemDataChange(item);
	owner.requestItemResize(item);
}

bool IsDeleted(not_null<const HistoryItem*> item) {
	return SoftCache().deleted.contains(item->fullId());
}

} // namespace Oceangram
