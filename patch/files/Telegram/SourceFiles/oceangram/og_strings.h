/*
OceanGram Desktop: our own texts. The server language pack has none of them,
so each is written in Russian (also for uk/be) and English, like the Android app.
*/
#pragma once

#include "lang/lang_instance.h"

namespace Oceangram {

[[nodiscard]] inline bool Ru() {
	const auto id = Lang::GetInstance().id();
	return id.startsWith(u"ru"_q)
		|| id.startsWith(u"uk"_q)
		|| id.startsWith(u"be"_q);
}

[[nodiscard]] inline QString Tr(const QString &ru, const QString &en) {
	return Ru() ? ru : en;
}

} // namespace Oceangram
