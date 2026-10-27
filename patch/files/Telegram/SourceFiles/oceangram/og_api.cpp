/*
OceanGram Desktop: the server's own endpoints.
*/
#include "oceangram/og_api.h"

#include "apiwrap.h"
#include "main/main_session.h"

#include <QtCore/QJsonDocument>

namespace Oceangram {

void RequestOcean(
		not_null<Main::Session*> session,
		const QString &endpoint,
		const QString &query,
		const QString &payload,
		Fn<void(QJsonObject)> done,
		Fn<void(QString)> fail) {
	using Flag = MTPtoncenter_performApiRequest::Flag;
	session->api().request(MTPtoncenter_PerformApiRequest(
		MTP_flags((query.isEmpty() ? Flag() : Flag::f_query)
			| (payload.isEmpty() ? Flag() : Flag::f_payload)),
		MTP_string(u"oceangram/"_q + endpoint),
		MTP_string(query),
		MTP_string(payload)
	)).done([=](const MTPtoncenter_ApiResponse &result) {
		const auto bytes = result.data().vresponse().data().vdata().v;
		const auto document = QJsonDocument::fromJson(bytes);
		if (!document.isObject()) {
			if (fail) {
				fail(u"BAD_RESPONSE"_q);
			}
		} else if (done) {
			done(document.object());
		}
	}).fail([=](const MTP::Error &error) {
		if (fail) {
			fail(error.type());
		}
	}).send();
}

} // namespace Oceangram
