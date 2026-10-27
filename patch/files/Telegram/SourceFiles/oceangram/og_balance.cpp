/*
OceanGram Desktop: the profile «Баланс» pill.
*/
#include "oceangram/og_balance.h"

#include "oceangram/og_api.h"
#include "oceangram/og_money.h"
#include "oceangram/og_strings.h"
#include "base/weak_ptr.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "ui/color_contrast.h"
#include "ui/effects/animation_value.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/ui_utility.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "wallet/wallet_panel.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_credits.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"

namespace Oceangram {
namespace {

void ShowUserBalance(
		not_null<Window::SessionController*> window,
		not_null<UserData*> user,
		int64 balance) {
	const auto weak = base::make_weak(window);
	window->uiShow()->showBox(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(user->name());
		const auto amount = FormatGramAmount(balance);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			Ru()
				? (u"У "_q + user->name() + QChar(' ') + amount
					+ u" GRAM в блокчейне TON."_q)
				: (user->name() + u" has "_q + amount
					+ u" GRAM on the TON blockchain."_q),
			st::boxLabel));
		box->addButton(rpl::single(Tr(u"Отправить GRAM"_q, u"Send GRAM"_q)), [=] {
			box->closeBox();
			if (const auto strong = weak.get()) {
				ShowSendGram(strong->uiShow(), user, nullptr, 0);
			}
		});
		box->addButton(rpl::single(Tr(u"Закрыть"_q, u"Close"_q)), [=] {
			box->closeBox();
		});
	}));
}

void ShowOwnBalance(
		not_null<Window::SessionController*> window,
		not_null<UserData*> user,
		int64 balance,
		bool hidden,
		Fn<void(bool)> hiddenChanged) {
	const auto session = &user->session();
	const auto show = window->uiShow();
	show->showBox(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(Tr(u"Мой баланс"_q, u"My balance"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			FormatGramAmount(balance) + u" GRAM"_q,
			st::boxLabel));
		const auto hide = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				Tr(u"Скрыть баланс в профиле"_q, u"Hide balance in profile"_q),
				hidden,
				st::defaultBoxCheckbox),
			style::margins(
				st::boxRowPadding.left(),
				st::boxRowPadding.left(),
				st::boxRowPadding.right(),
				st::boxRowPadding.left() / 2));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			Tr(
				u"Пока баланс не скрыт, его видят все, кто открывает твой профиль."_q,
				u"Until you hide it, everyone who opens your profile sees it."_q),
			st::boxDividerLabel));
		hide->checkedChanges() | rpl::on_next([=](bool checked) {
			RequestOcean(
				session,
				u"balance_hidden"_q,
				QString(),
				checked ? u"true"_q : u"false"_q,
				[=](QJsonObject json) {
					if (hiddenChanged) {
						hiddenChanged(json.value(u"hidden"_q).toBool());
					}
				},
				[=](QString error) {
					show->showToast(Tr(
						u"Не удалось сохранить: "_q,
						u"Could not save: "_q) + error);
				});
		}, hide->lifetime());
		box->addButton(rpl::single(Tr(u"Открыть кошелёк"_q, u"Open wallet"_q)), [=] {
			box->closeBox();
			Wallet::ShowWallet(session);
		});
		box->addButton(rpl::single(Tr(u"Закрыть"_q, u"Close"_q)), [=] {
			box->closeBox();
		});
	}));
}

} // namespace

BalanceButton::BalanceButton(QWidget *parent, Fn<void()> handler)
: RippleButton(parent, st::infoMusicButtonRipple) {
	setBalance(0);
	setClickedCallback(std::move(handler));
}

void BalanceButton::setBalance(int64 nano) {
	_label.setText(st::semiboldTextStyle, Tr(u"Баланс:"_q, u"Balance:"_q));
	_amount.setText(st::defaultTextStyle, FormatGramAmount(nano));
	setAccessibleName(Tr(u"Баланс: "_q, u"Balance: "_q)
		+ FormatGramAmount(nano)
		+ u" GRAM"_q);
	update();
}

void BalanceButton::setOverrideBg(std::optional<QColor> color) {
	_overrideBg = color;
	update();
}

void BalanceButton::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	if (_overrideBg) {
		p.fillRect(e->rect(), Ui::BlendColors(
			*_overrideBg,
			Qt::black,
			st::infoProfileTopBarActionButtonBgOpacity));
	} else {
		p.fillRect(e->rect(), st::shadowFg);
	}
	const auto lightBackground = _overrideBg
		&& Ui::IsLightBackground(*_overrideBg);
	const auto rippleColor = !_overrideBg
		? std::nullopt
		: std::make_optional(anim::with_alpha(
			(lightBackground
				? QColor(Qt::black)
				: st::groupCallMembersFg->c),
			st::infoProfileTopBarBackdropRippleOpacity));
	paintRipple(p, QPoint(), rippleColor ? &*rippleColor : nullptr);

	const auto fg = !_overrideBg
		? st::windowBoldFg->c
		: lightBackground
		? QColor(Qt::black)
		: st::groupCallMembersFg->c;
	const auto &gem = st::tonIconEmoji.icon;
	const auto &arrow = st::topicButtonArrow;
	const auto skip = st::normalFont->spacew;
	const auto labelWidth = _label.maxWidth();
	const auto amountWidth = _amount.maxWidth();
	const auto total = labelWidth
		+ skip
		+ gem.width()
		+ amountWidth
		+ skip
		+ arrow.width();
	auto left = std::max((width() - total) / 2, 0);
	const auto textTop = (height() - st::normalFont->height) / 2;

	p.setPen(fg);
	_label.draw(p, {
		.position = { left, textTop },
		.availableWidth = labelWidth,
		.now = crl::now(),
		.elisionLines = 1,
	});
	left += labelWidth + skip;
	gem.paint(p, left, (height() - gem.height()) / 2, width(), fg);
	left += gem.width();
	_amount.draw(p, {
		.position = { left, textTop },
		.availableWidth = amountWidth,
		.now = crl::now(),
		.elisionLines = 1,
	});
	left += amountWidth + skip;
	arrow.paint(p, left, (height() - arrow.height()) / 2, width(), fg);
}

int BalanceButton::resizeGetHeight(int newWidth) {
	const auto padding = st::infoMusicButtonPadding;
	const auto &font = st::defaultTextStyle.font;
	return padding.top() + font->height + padding.bottom();
}

void AddBalancePill(
		not_null<Ui::VerticalLayout*> layout,
		not_null<Window::SessionController*> window,
		not_null<UserData*> user,
		rpl::producer<std::optional<QColor>> topBarColor) {
	if (user->isBot()) {
		return;
	}
	const auto session = &user->session();
	const auto wrap = layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			layout,
			object_ptr<Ui::VerticalLayout>(layout)));
	wrap->hide(anim::type::instant);

	struct State {
		int64 balance = 0;
		bool hidden = false;
	};
	const auto state = wrap->lifetime().make_state<State>();
	const auto weakWindow = base::make_weak(window);
	const auto button = wrap->entity()->add(object_ptr<BalanceButton>(
		wrap->entity(),
		[=] {
			const auto strong = weakWindow.get();
			if (!strong) {
				return;
			} else if (user->isSelf()) {
				ShowOwnBalance(strong, user, state->balance, state->hidden, [=](
						bool hidden) {
					state->hidden = hidden;
				});
			} else {
				ShowUserBalance(strong, user, state->balance);
			}
		}));
	std::move(
		topBarColor
	) | rpl::on_next([=](std::optional<QColor> color) {
		button->setOverrideBg(color);
	}, button->lifetime());

	auto query = u"user_id="_q + QString::number(peerToUser(user->id).bare);
	if (!user->isSelf()) {
		query += u"&access_hash="_q + QString::number(int64(user->accessHash()));
	}
	RequestOcean(session, u"balance"_q, query, QString(), crl::guard(wrap, [=](
			QJsonObject json) {
		if (!json.value(u"visible"_q).toBool()) {
			return;
		}
		state->balance = json.value(u"balance"_q).toInteger();
		state->hidden = json.value(u"hidden"_q).toBool();
		button->setBalance(state->balance);
		wrap->toggle(true, anim::type::normal);
	}));
}

} // namespace Oceangram
