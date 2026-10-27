/*
OceanGram Desktop: the server's own endpoints (oceangram/balance, premium,
styles, soft...). They ride toncenter.performApiRequest, like on Android, and
answer JSON.
*/
#pragma once

#include <QtCore/QJsonObject>

namespace Main {
class Session;
} // namespace Main

namespace Oceangram {

// endpoint without the "oceangram/" prefix; an empty query or payload is not sent.
void RequestOcean(
	not_null<Main::Session*> session,
	const QString &endpoint,
	const QString &query,
	const QString &payload,
	Fn<void(QJsonObject)> done,
	Fn<void(QString)> fail = nullptr);

} // namespace Oceangram
