/*
OceanGram Desktop: OceanGram Premium and OceanSoft in Settings.
*/
#include "oceangram/og_settings.h"

#include "oceangram/og_api.h"
#include "oceangram/og_premium.h"
#include "oceangram/og_soft.h"
#include "oceangram/og_strings.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "base/unixtime.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "ui/basic_click_handlers.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/style/style_core_scale.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QPointer>

namespace Oceangram {
namespace {

constexpr auto kSwatches = int(kPalettes.size()) + 1; // + «Выкл.»

// Seven palette swatches and «Выкл.» in one row.
class PaletteRow final : public Ui::RpWidget {
public:
	PaletteRow(QWidget *parent, QString selected, Fn<void(QString)> chosen)
	: RpWidget(parent)
	, _selected(std::move(selected))
	, _chosen(std::move(chosen)) {
		setCursor(style::cur_pointer);
	}

	void setSelected(const QString &id) {
		_selected = id;
		update();
	}

protected:
	int resizeGetHeight(int) override {
		return style::ConvertScale(44);
	}

	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		p.setRenderHint(QPainter::Antialiasing);
		for (auto i = 0; i != kSwatches; ++i) {
			const auto id = idAt(i);
			const auto rect = swatchRect(i);
			if (id.isEmpty()) {
				auto pen = QPen(st::windowSubTextFg->c);
				pen.setWidthF(style::ConvertScale(1.5));
				p.setPen(pen);
				p.setBrush(Qt::NoBrush);
				p.drawEllipse(rect);
				p.drawLine(
					rect.topLeft() + QPoint(rect.width() / 5, rect.height() / 5),
					rect.bottomRight() - QPoint(rect.width() / 5, rect.height() / 5));
			} else {
				p.setPen(Qt::NoPen);
				p.setBrush(QBrush(ShimmerGradient(
					id,
					false,
					rect.x(),
					rect.width(),
					0)));
				p.drawEllipse(rect);
			}
			if (id == _selected) {
				auto pen = QPen(st::activeButtonBg->c);
				pen.setWidthF(style::ConvertScale(2.));
				p.setPen(pen);
				p.setBrush(Qt::NoBrush);
				const auto skip = style::ConvertScale(4);
				p.drawEllipse(rect.marginsAdded({ skip, skip, skip, skip }));
			}
		}
	}

	void mousePressEvent(QMouseEvent *e) override {
		for (auto i = 0; i != kSwatches; ++i) {
			const auto point = e->position().toPoint();
			if (swatchRect(i).marginsAdded({ 4, 4, 4, 4 }).contains(point)) {
				const auto id = idAt(i);
				if (id != _selected) {
					setSelected(id);
					if (_chosen) {
						_chosen(id);
					}
				}
				return;
			}
		}
	}

private:
	[[nodiscard]] static QString idAt(int index) {
		return (index < int(kPalettes.size()))
			? QString::fromLatin1(kPalettes[index])
			: QString();
	}

	[[nodiscard]] QRect swatchRect(int index) const {
		const auto size = style::ConvertScale(28);
		const auto padding = st::boxRowPadding.left();
		const auto available = width() - 2 * padding;
		const auto step = (kSwatches > 1)
			? (available - size) / (kSwatches - 1)
			: 0;
		return QRect(
			padding + index * step,
			(height() - size) / 2,
			size,
			size);
	}

	QString _selected;
	Fn<void(QString)> _chosen;

};

void AddHeader(not_null<Ui::GenericBox*> box, const QString &text) {
	box->addRow(
		object_ptr<Ui::FlatLabel>(box, text, st::boxLabel),
		style::margins(
			st::boxRowPadding.left(),
			st::boxRowPadding.left() / 2,
			st::boxRowPadding.right(),
			0));
}

void AddNote(not_null<Ui::GenericBox*> box, const QString &text) {
	box->addRow(
		object_ptr<Ui::FlatLabel>(box, text, st::boxDividerLabel),
		style::margins(
			st::boxRowPadding.left(),
			0,
			st::boxRowPadding.right(),
			st::boxRowPadding.left() / 2));
}

void PremiumBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	const auto user = session->user();
	const auto show = controller->uiShow();
	struct State {
		NameStyle style;
		bool active = false;
		bool loaded = false;
		QString buyUrl;
		base::Timer saveTimer;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto weakBox = QPointer<Ui::GenericBox>(box.get());

	box->setTitle(u"OceanGram Premium"_q);
	box->setWidth(st::boxWideWidth);

	const auto preview = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		user->name(),
		st::boxTitle));
	MarkPreview(
		preview,
		[=] { return state->style.nick; },
		[=] { return state->style.glint; });
	const auto status = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Tr(u"Загрузка…"_q, u"Loading…"_q),
		st::boxDividerLabel));

	const auto save = [=] {
		const auto payload = QJsonDocument(
			NameStyleJson(state->style)
		).toJson(QJsonDocument::Compact);
		RequestOcean(
			session,
			u"style"_q,
			QString(),
			QString::fromUtf8(payload),
			[=](QJsonObject json) {
				PutOwnStyle(session, ParseNameStyle(json));
			},
			[=](QString error) {
				show->showToast((error == u"OCEAN_PREMIUM_REQUIRED"_q)
					? Tr(
						u"Цвета сохранятся после покупки OceanGram Premium"_q,
						u"Colours are saved once you buy OceanGram Premium"_q)
					: (Tr(u"Не удалось сохранить: "_q, u"Could not save: "_q)
						+ error));
			});
	};
	state->saveTimer.setCallback(save);
	const auto changed = [=] {
		preview->update();
		if (state->loaded) {
			state->saveTimer.callOnce(400);
		}
	};

	AddHeader(box, Tr(u"Подсветка ника"_q, u"Name highlight"_q));
	const auto nick = box->addRow(
		object_ptr<PaletteRow>(box, QString(), [=](QString id) {
			state->style.nick = id;
			changed();
		}),
		style::margins());
	AddHeader(box, Tr(u"Подсветка номера"_q, u"Number highlight"_q));
	const auto number = box->addRow(
		object_ptr<PaletteRow>(box, QString(), [=](QString id) {
			state->style.number = id;
			changed();
		}),
		style::margins());
	AddHeader(box, Tr(u"Юзернеймы и NFT"_q, u"Usernames and NFTs"_q));
	const auto username = box->addRow(
		object_ptr<PaletteRow>(box, QString(), [=](QString id) {
			state->style.username = id;
			changed();
		}),
		style::margins());
	AddNote(box, Tr(
		u"Все твои @юзернеймы в профиле, коллекционные (NFT) тоже. Выключено, пока не выберешь цвет."_q,
		u"All your @usernames in the profile, collectible (NFT) ones too. Off until you pick a colour."_q));
	const auto glint = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		Tr(u"Блик"_q, u"Glint"_q),
		true,
		st::defaultBoxCheckbox));
	glint->checkedChanges() | rpl::on_next([=](bool checked) {
		state->style.glint = checked;
		changed();
	}, glint->lifetime());
	AddNote(box, Tr(
		u"Твой ник, номер и юзернеймы переливаются у всех в OceanGram: в профиле, в шапке чата и в списке чатов."_q,
		u"Your name, number and usernames shimmer for everyone in OceanGram: in the profile, the chat header and the chat list."_q));

	RequestOcean(session, u"premium"_q, QString(), QString(), crl::guard(box, [=](
			QJsonObject json) {
		state->active = json.value(u"active"_q).toBool();
		state->buyUrl = json.value(u"buy_url"_q).toString();
		state->style = ParseNameStyle(json.value(u"style"_q).toObject());
		nick->setSelected(state->style.nick);
		number->setSelected(state->style.number);
		username->setSelected(state->style.username);
		glint->setChecked(state->style.glint);
		state->loaded = true;
		preview->update();
		const auto free = json.value(u"free"_q).toBool();
		status->setText(state->active
			? (free
				? Tr(u"Активен навсегда — бесплатно"_q, u"Active forever — free"_q)
				: Tr(u"Куплен навсегда"_q, u"Bought forever"_q))
			: Tr(
				u"Не куплен. Цвета можно посмотреть, сохранятся после покупки."_q,
				u"Not bought. Try the colours; they are saved after the purchase."_q));
		box->clearButtons();
		if (!state->active && !state->buyUrl.isEmpty()) {
			box->addButton(rpl::single(Tr(u"Купить"_q, u"Buy"_q)), [=] {
				UrlClickHandler::Open(state->buyUrl);
			});
		}
		box->addButton(rpl::single(Tr(u"Готово"_q, u"Done"_q)), [=] {
			if (state->saveTimer.isActive()) {
				state->saveTimer.cancel();
				save();
			}
			if (weakBox) {
				weakBox->closeBox();
			}
		});
	}), crl::guard(box, [=](QString error) {
		status->setText(Tr(u"Не удалось загрузить: "_q, u"Could not load: "_q)
			+ error);
	}));
	box->addButton(rpl::single(Tr(u"Готово"_q, u"Done"_q)), [=] {
		box->closeBox();
	});
	box->boxClosing() | rpl::on_next([=] {
		if (state->saveTimer.isActive()) {
			state->saveTimer.cancel();
			save();
		}
	}, box->lifetime());
}

[[nodiscard]] QString SoftStatusText(const SoftState &state) {
	if (!state.active) {
		return Tr(u"Не подключён"_q, u"Not active"_q);
	} else if (state.source == u"premium"_q) {
		return Tr(
			u"Бесплатно навсегда с OceanGram Premium"_q,
			u"Free forever with OceanGram Premium"_q);
	} else if (!state.until) {
		return Tr(u"Активен"_q, u"Active"_q);
	}
	const auto date = base::unixtime::parse(TimeId(state.until));
	return Tr(u"Активен до "_q, u"Active until "_q)
		+ QLocale().toString(date, QLocale::ShortFormat)
		+ (state.autoRenew
			? Tr(u", продлится сам"_q, u", renews itself"_q)
			: QString());
}

void SoftBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	const auto show = controller->uiShow();
	box->setTitle(u"OceanSoft"_q);
	box->setWidth(st::boxWideWidth);

	const auto status = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		SoftStatusText(SoftStateFor(session)),
		st::boxLabel));
	AddNote(box, Tr(
		u"Режим призрака и сохранённые удалённые сообщения. 2500 ⭐ в неделю, один день бесплатно, бесплатно навсегда с OceanGram Premium."_q,
		u"Ghost mode and kept deleted messages. 2500 ⭐ a week, one free day, free forever with OceanGram Premium."_q));

	const auto addOption = [&](const QString &text, bool SoftOptions::*field) {
		const auto check = box->addRow(object_ptr<Ui::Checkbox>(
			box,
			text,
			Soft().*field,
			st::defaultBoxCheckbox));
		check->checkedChanges() | rpl::on_next([=](bool checked) {
			Soft().*field = checked;
			SaveSoft();
		}, check->lifetime());
	};
	AddHeader(box, Tr(u"Режим призрака"_q, u"Ghost mode"_q));
	addOption(
		Tr(u"Не отправлять «прочитано»"_q, u"Don't send «read»"_q),
		&SoftOptions::noRead);
	addOption(
		Tr(u"Смотреть истории незаметно"_q, u"View stories unseen"_q),
		&SoftOptions::noStories);
	addOption(
		Tr(u"Скрывать «в сети»"_q, u"Hide «online»"_q),
		&SoftOptions::hideOnline);
	AddHeader(box, Tr(u"Удалённые сообщения"_q, u"Deleted messages"_q));
	addOption(
		Tr(u"Сохранять удалённые собеседником"_q, u"Keep messages others delete"_q),
		&SoftOptions::keepDeleted);
	AddNote(box, Tr(
		u"Такие сообщения остаются с 🗑 перед временем, пока открыт OceanGram."_q,
		u"Such messages stay with 🗑 before the time while OceanGram is open."_q));

	const auto weakBox = QPointer<Ui::GenericBox>(box.get());
	const auto rebuild = std::make_shared<Fn<void()>>();
	const auto buy = [=](const QString &plan) {
		RequestOcean(session, u"soft_buy"_q, QString(), plan, [=](
				QJsonObject json) {
			ApplySoftState(session, json);
			show->showToast(Tr(u"OceanSoft подключён"_q, u"OceanSoft is on"_q));
			if (weakBox && *rebuild) {
				(*rebuild)();
			}
		}, [=](QString error) {
			show->showToast((error == u"BALANCE_TOO_LOW"_q)
				? Tr(u"Не хватает звёзд"_q, u"Not enough Stars"_q)
				: (error == u"OCEANSOFT_TRIAL_USED"_q)
				? Tr(u"Пробный день уже был"_q, u"The free day was already used"_q)
				: (Tr(u"Не удалось: "_q, u"Failed: "_q) + error));
		});
	};
	*rebuild = [=] {
		if (!weakBox) {
			return;
		}
		const auto state = SoftStateFor(session);
		status->setText(SoftStatusText(state));
		weakBox->clearButtons();
		if (!state.active) {
			weakBox->addButton(
				rpl::single(Tr(u"Неделя — "_q, u"Week — "_q)
					+ QString::number(state.price)
					+ u" ⭐"_q),
				[=] { buy(u"week"_q); });
			if (state.trialAvailable) {
				weakBox->addButton(
					rpl::single(Tr(u"День бесплатно"_q, u"Free day"_q)),
					[=] { buy(u"trial"_q); });
			}
		} else if (state.source == u"week"_q) {
			weakBox->addLeftButton(
				rpl::single(state.autoRenew
					? Tr(u"Не продлевать"_q, u"Stop renewing"_q)
					: Tr(u"Продлевать"_q, u"Renew"_q)),
				[=] {
					RequestOcean(
						session,
						u"soft_autorenew"_q,
						QString(),
						state.autoRenew ? u"false"_q : u"true"_q,
						[=](QJsonObject json) {
							ApplySoftState(session, json);
							if (*rebuild) {
								(*rebuild)();
							}
						},
						[=](QString error) {
							show->showToast(Tr(u"Не удалось: "_q, u"Failed: "_q)
								+ error);
						});
				});
		}
		weakBox->addButton(rpl::single(Tr(u"Закрыть"_q, u"Close"_q)), [=] {
			if (weakBox) {
				weakBox->closeBox();
			}
		});
	};
	box->lifetime().add([=] {
		*rebuild = nullptr;
	});
	(*rebuild)();
	RefreshSoft(session, crl::guard(box, [=] {
		if (*rebuild) {
			(*rebuild)();
		}
	}));
}

} // namespace

void ShowPremiumBox(not_null<Window::SessionController*> controller) {
	controller->uiShow()->showBox(Box(PremiumBox, controller));
}

void ShowSoftBox(not_null<Window::SessionController*> controller) {
	controller->uiShow()->showBox(Box(SoftBox, controller));
}

void BuildSettingsSection(Settings::Builder::SectionBuilder &builder) {
	const auto controller = builder.controller();
	auto weak = base::weak_ptr<Window::SessionController>();
	if (controller) {
		weak = base::make_weak(controller);
	}
	builder.addDivider();
	builder.addSkip();
	builder.addButton({
		.id = u"main/oceangram-premium"_q,
		.title = rpl::single(u"OceanGram Premium"_q),
		.icon = { &st::menuIconPremium },
		.onClick = [=] {
			if (const auto strong = weak.get()) {
				ShowPremiumBox(strong);
			}
		},
		.keywords = { u"oceangram"_q, u"premium"_q, u"shimmer"_q },
	});
	builder.addButton({
		.id = u"main/oceansoft"_q,
		.title = rpl::single(u"OceanSoft"_q),
		.icon = { &st::menuIconStealth },
		.onClick = [=] {
			if (const auto strong = weak.get()) {
				ShowSoftBox(strong);
			}
		},
		.keywords = { u"oceansoft"_q, u"ghost"_q, u"deleted"_q },
	});
	builder.addSkip();
}

} // namespace Oceangram
