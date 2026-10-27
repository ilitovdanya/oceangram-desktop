/*
OceanGram Desktop: «Деньги» — sending GRAM to another account.
*/
#include "oceangram/og_money.h"

#include "oceangram/og_strings.h"
#include "apiwrap.h"
#include "base/random.h"
#include "base/weak_ptr.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>

namespace Oceangram {
namespace {

constexpr auto kNanoPerGram = int64(1'000'000'000);
constexpr auto kMaxGram = int64(1'000'000'000); // Far above any ledger balance.
constexpr auto kCommentMax = 500;
constexpr auto kTransferPrefix = "OGT1";

[[nodiscard]] QString SendErrorText(const QString &type) {
	if (type == u"BALANCE_TOO_LOW"_q || type == u"WALLET_BALANCE_LOW"_q) {
		return Tr(u"Недостаточно GRAM на балансе"_q, u"Not enough GRAM"_q);
	} else if (type == u"USER_IS_BLOCKED"_q) {
		return Tr(
			u"Этот пользователь вас заблокировал"_q,
			u"This user has blocked you"_q);
	} else if (type == u"WALLET_ADDRESS_INVALID"_q) {
		return Tr(
			u"Адрес получателя изменился, попробуйте ещё раз"_q,
			u"The recipient's address changed, try again"_q);
	} else if (type == u"WALLET_UNAVAILABLE"_q
		|| type == u"WALLET_USER_UNAVAILABLE"_q) {
		return Tr(
			u"Кошелёк сейчас недоступен"_q,
			u"The wallet is unavailable right now"_q);
	} else if (type == u"USER_ID_INVALID"_q) {
		return Tr(
			u"Этому пользователю нельзя отправить GRAM"_q,
			u"GRAM can't be sent to this user"_q);
	} else if (type.startsWith(u"FLOOD_WAIT_"_q)) {
		return Tr(
			u"Слишком часто, подождите немного"_q,
			u"Too many attempts, wait a little"_q);
	}
	return Tr(u"Не удалось отправить: "_q, u"Could not send: "_q) + type;
}

struct SendState {
	QString address;
	int64 balance = -1;
	bool addressLoading = false;
	bool sending = false;
};

void SendGramBox(
		not_null<Ui::GenericBox*> box,
		std::shared_ptr<Main::SessionShow> show,
		not_null<UserData*> user,
		Fn<void()> sent,
		int64 amountNano) {
	const auto session = &show->session();
	const auto weakSession = base::make_weak(session);
	const auto userId = peerToUser(user->id);
	const auto state = box->lifetime().make_state<SendState>();
	const auto weakBox = QPointer<Ui::GenericBox>(box.get());

	box->setTitle(Tr(u"Отправить GRAM"_q, u"Send GRAM"_q));
	box->setWidth(st::boxWideWidth);
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			Tr(u"Получатель: "_q, u"To: "_q) + user->name(),
			st::boxLabel),
		style::margins(
			st::boxRowPadding.left(),
			0,
			st::boxRowPadding.right(),
			st::boxRowPadding.left() / 2));
	const auto balance = box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			Tr(u"Баланс: …"_q, u"Balance: …"_q),
			st::boxDividerLabel),
		style::margins(
			st::boxRowPadding.left(),
			0,
			st::boxRowPadding.right(),
			st::boxRowPadding.left() / 2));
	const auto amount = box->addRow(
		object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			rpl::single(Tr(u"Сумма, GRAM"_q, u"Amount, GRAM"_q)),
			(amountNano > 0) ? FormatGramAmount(amountNano) : QString()));
	const auto comment = box->addRow(
		object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			rpl::single(Tr(
				u"Комментарий (необязательно)"_q,
				u"Comment (optional)"_q)),
			QString()));
	comment->setMaxLength(kCommentMax);

	session->api().request(MTPwallet_GetState(
	)).done(crl::guard(box, [=](const MTPWalletState &result) {
		result.match([&](const MTPDwalletState &data) {
			state->balance = int64(data.vbalance().v);
			balance->setText(Tr(u"Баланс: "_q, u"Balance: "_q)
				+ FormatGramAmount(state->balance)
				+ u" GRAM"_q);
		}, [&](const MTPDwalletStateEmpty &) {
			state->balance = 0;
			balance->setText(Tr(
				u"Кошелёк ещё создаётся, попробуйте чуть позже"_q,
				u"The wallet is still being created, try a bit later"_q));
		});
	})).fail(crl::guard(box, [=](const MTP::Error &error) {
		balance->setText(SendErrorText(error.type()));
	})).send();

	const auto requestAddress = [=](Fn<void()> then) {
		if (state->addressLoading) {
			return;
		}
		state->addressLoading = true;
		session->api().request(MTPwallet_GetUserAddresses(
			MTP_flags(MTPwallet_GetUserAddresses::Flag::f_force),
			MTP_vector<MTPInputUser>(1, user->inputUser()),
			MTP_vector<MTPstring>()
		)).done(crl::guard(box, [=](const MTPwallet_UserAddresses &result) {
			state->addressLoading = false;
			const auto &data = result.data();
			session->data().processUsers(data.vusers());
			for (const auto &entry : data.vaddresses().v) {
				const auto &address = entry.data();
				const auto id = UserId(address.vuser_id().value_or_empty());
				if (id == userId) {
					// Exactly the server's string: it checks it as sent.
					state->address = qs(address.vaddress());
				}
			}
			if (state->address.isEmpty()) {
				show->showToast(SendErrorText(u"WALLET_USER_UNAVAILABLE"_q));
			} else if (then) {
				then();
			}
		})).fail(crl::guard(box, [=](const MTP::Error &error) {
			state->addressLoading = false;
			show->showToast(SendErrorText(error.type()));
		})).send();
	};
	requestAddress(nullptr);

	const auto submit = [=] {
		if (state->sending) {
			return;
		}
		const auto nano = ParseGramAmount(amount->getLastText());
		if (nano <= 0) {
			amount->showError();
			return;
		} else if (state->balance >= 0 && nano > state->balance) {
			amount->showError();
			show->showToast(SendErrorText(u"BALANCE_TOO_LOW"_q));
			return;
		} else if (state->address.isEmpty()) {
			requestAddress(nullptr);
			show->showToast(Tr(
				u"Получаем адрес получателя, нажмите ещё раз"_q,
				u"Getting the recipient's address, press again"_q));
			return;
		}
		auto json = QJsonObject();
		json.insert(u"address"_q, state->address);
		json.insert(u"amount"_q, QJsonValue(qint64(nano)));
		const auto text = comment->getLastText().trimmed();
		if (!text.isEmpty()) {
			json.insert(u"comment"_q, text);
		}
		const auto payload = QByteArray(kTransferPrefix)
			+ QJsonDocument(json).toJson(QJsonDocument::Compact);
		auto randomId = base::RandomValue<uint64>();
		while (!randomId) {
			randomId = base::RandomValue<uint64>();
		}
		state->sending = true;
		using Flag = MTPwallet_SendTransfer::Flag;
		session->api().request(MTPwallet_SendTransfer(
			MTP_flags(Flag(0)),
			MTP_bytes(payload),
			MTPbytes(),
			user->inputUser(),
			MTP_long(randomId)
		)).done([=](const MTPUpdates &result) {
			if (const auto strong = weakSession.get()) {
				strong->api().applyUpdates(result);
			}
			show->showToast(Tr(u"Отправлено "_q, u"Sent "_q)
				+ FormatGramAmount(nano)
				+ u" GRAM"_q);
			if (weakBox) {
				weakBox->closeBox();
			}
			if (sent) {
				sent();
			}
		}).fail(crl::guard(box, [=](const MTP::Error &error) {
			state->sending = false;
			if (error.type() == u"WALLET_ADDRESS_INVALID"_q) {
				state->address = QString();
				requestAddress(nullptr);
			}
			show->showToast(SendErrorText(error.type()));
		})).send();
	};
	amount->submits() | rpl::on_next([=] {
		comment->setFocus();
	}, amount->lifetime());
	comment->submits() | rpl::on_next(submit, comment->lifetime());
	box->setFocusCallback([=] {
		amount->setFocusFast();
	});
	box->addButton(rpl::single(Tr(u"Отправить"_q, u"Send"_q)), submit);
	box->addButton(rpl::single(Tr(u"Отмена"_q, u"Cancel"_q)), [=] {
		box->closeBox();
	});
}

} // namespace

int64 ParseGramAmount(const QString &text) {
	auto value = text.trimmed();
	value.replace(QChar(','), QChar('.'));
	value.remove(QChar(' '));
	if (value.isEmpty() || value.count(QChar('.')) > 1) {
		return 0;
	}
	const auto dot = value.indexOf(QChar('.'));
	const auto whole = (dot < 0) ? value : value.mid(0, dot);
	auto fraction = (dot < 0) ? QString() : value.mid(dot + 1);
	if (fraction.size() > 9) {
		return 0;
	}
	for (const auto &ch : whole + fraction) {
		if (!ch.isDigit()) {
			return 0;
		}
	}
	auto ok = true;
	const auto grams = whole.isEmpty() ? int64(0) : whole.toLongLong(&ok);
	if (!ok || grams < 0 || grams > kMaxGram) {
		return 0;
	}
	while (fraction.size() < 9) {
		fraction.append(QChar('0'));
	}
	const auto nanos = fraction.toLongLong(&ok);
	if (!ok) {
		return 0;
	}
	const auto result = grams * kNanoPerGram + nanos;
	return (result > 0) ? result : 0;
}

QString FormatGramAmount(int64 nano) {
	const auto negative = (nano < 0);
	const auto abs = negative ? -nano : nano;
	auto result = QString::number(abs / kNanoPerGram);
	auto fraction = QString::number(abs % kNanoPerGram).rightJustified(
		9,
		QChar('0'));
	while (!fraction.isEmpty() && fraction.endsWith(QChar('0'))) {
		fraction.chop(1);
	}
	if (!fraction.isEmpty()) {
		result += QChar('.') + fraction;
	}
	return negative ? (QChar('-') + result) : result;
}

void ShowSendGram(
		std::shared_ptr<Main::SessionShow> show,
		not_null<UserData*> user,
		Fn<void()> sent,
		int64 amountNano) {
	if (!show || !show->valid() || &show->session() != &user->session()) {
		return;
	}
	show->showBox(Box(SendGramBox, show, user, std::move(sent), amountNano));
}

} // namespace Oceangram
