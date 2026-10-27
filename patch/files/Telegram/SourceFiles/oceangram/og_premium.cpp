/*
OceanGram Desktop: OceanGram Premium shimmer.
*/
#include "oceangram/og_premium.h"

#include "oceangram/og_api.h"
#include "oceangram/og_strings.h"
#include "base/algorithm.h"
#include "base/timer.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "ui/style/style_core_scale.h"
#include "ui/widgets/labels.h"
#include "window/themes/window_theme.h"

#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsEffect>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Oceangram {
namespace {

constexpr auto kPeriod = crl::time(1500);
constexpr auto kRefreshEvery = 5 * 60 * crl::time(1000);
constexpr auto kFrame = crl::time(33);
constexpr auto kSamples = 40;

class ShimmerEffect;

struct Cache {
	Cache();

	void tick();
	void schedule();
	void changed();

	std::unordered_map<uint64, NameStyle> styles;
	crl::time loadedAt = 0;
	bool loading = false;
	base::Timer ticker;
	std::vector<std::pair<QPointer<QWidget>, QRect>> repaints;
	std::vector<QPointer<ShimmerEffect>> effects;
};

[[nodiscard]] Cache &Styles() {
	// Never destroyed: widgets may still paint while the app shuts down.
	static const auto instance = new Cache();
	return *instance;
}

[[nodiscard]] std::vector<QColor> Stops(const QString &palette) {
	const auto rgb = [](std::initializer_list<uint32> list) {
		auto result = std::vector<QColor>();
		for (const auto value : list) {
			result.push_back(QColor::fromRgb(value));
		}
		return result;
	};
	if (palette == u"rainbow"_q) {
		return rgb({ 0xFF2D55, 0xFF9500, 0xFFD60A, 0x34C759, 0x00C7FF, 0x0A84FF, 0xBF5AF2, 0xFF2D55 });
	} else if (palette == u"red"_q) {
		return rgb({ 0xD40000, 0xFF2B2B, 0xFF9A9A, 0xFF2B2B, 0xA80000, 0xFF4D4D, 0xD40000 });
	} else if (palette == u"white"_q) {
		// White text disappears on a light theme; it is drawn as platinum there.
		return Window::Theme::IsNightMode()
			? rgb({ 0xFFFFFF, 0xD6E2F0, 0xFFFFFF, 0xB8C6D6, 0xFFFFFF })
			: rgb({ 0x353D49, 0x76829A, 0xBCC6D2, 0x76829A, 0x353D49 });
	} else if (palette == u"gold"_q) {
		return rgb({ 0xA8740A, 0xF5C518, 0xFFF1A8, 0xE0A800, 0xA8740A });
	} else if (palette == u"neon"_q) {
		return rgb({ 0x00D4FF, 0x7A5CFF, 0xFF2BD6, 0x7A5CFF, 0x00D4FF });
	} else if (palette == u"ocean"_q) {
		return rgb({ 0x0077FF, 0x00C6FF, 0x00E0B0, 0x00C6FF, 0x0077FF });
	} else if (palette == u"sunset"_q) {
		return rgb({ 0xFF5F6D, 0xFFB347, 0xFF4E88, 0xFF8A5C, 0xFF5F6D });
	}
	return {};
}

[[nodiscard]] QColor Blend(const QColor &a, const QColor &b, double t) {
	t = std::clamp(t, 0., 1.);
	return QColor::fromRgbF(
		a.redF() + (b.redF() - a.redF()) * t,
		a.greenF() + (b.greenF() - a.greenF()) * t,
		a.blueF() + (b.blueF() - a.blueF()) * t,
		a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

NameStyle ParseStyleJson(const QJsonObject &json) {
	return NameStyle{
		.nick = json.value(u"nick"_q).toString(),
		.number = json.value(u"number"_q).toString(),
		.username = json.value(u"username"_q).toString(),
		.glint = json.value(u"glint"_q).toBool(true),
		.glow = json.value(u"glow"_q).toBool(true),
	};
}

void Refresh(not_null<Main::Session*> session) {
	auto &cache = Styles();
	if (cache.loading
		|| (cache.loadedAt && crl::now() - cache.loadedAt < kRefreshEvery)) {
		return;
	}
	cache.loading = true;
	RequestOcean(session, u"styles"_q, QString(), QString(), [](
			QJsonObject json) {
		auto &cache = Styles();
		cache.loading = false;
		cache.loadedAt = crl::now();
		auto styles = std::unordered_map<uint64, NameStyle>();
		const auto map = json.value(u"styles"_q).toObject();
		for (auto i = map.begin(); i != map.end(); ++i) {
			auto ok = false;
			const auto id = i.key().toULongLong(&ok);
			if (ok && id) {
				styles.emplace(id, ParseStyleJson(i.value().toObject()));
			}
		}
		cache.styles = std::move(styles);
		cache.changed();
	}, [](QString) {
		auto &cache = Styles();
		cache.loading = false;
		// Try again on the next paint after a minute.
		cache.loadedAt = crl::now() - kRefreshEvery + 60 * crl::time(1000);
	});
}

[[nodiscard]] const NameStyle *Find(not_null<PeerData*> peer) {
	if (!peer->isUser()) {
		return nullptr;
	}
	Refresh(&peer->session());
	const auto &styles = Styles().styles;
	const auto i = styles.find(peerToUser(peer->id).bare);
	return (i != end(styles)) ? &i->second : nullptr;
}

class ShimmerEffect final : public QGraphicsEffect {
public:
	ShimmerEffect(
		not_null<QWidget*> widget,
		Fn<QString()> palette,
		Fn<bool()> glint)
	: QGraphicsEffect(nullptr)
	, _widget(widget)
	, _palette(std::move(palette))
	, _glint(std::move(glint)) {
	}

protected:
	void draw(QPainter *painter) override {
		const auto palette = _palette();
		if (palette.isEmpty()) {
			drawSource(painter);
			return;
		}
		auto offset = QPoint();
		const auto pixmap = sourcePixmap(
			Qt::LogicalCoordinates,
			&offset,
			QGraphicsEffect::NoPad);
		if (pixmap.isNull()) {
			return;
		}
		auto image = pixmap.toImage().convertToFormat(
			QImage::Format_ARGB32_Premultiplied);
		const auto ratio = image.devicePixelRatio();
		const auto width = int(std::ceil(image.width() / ratio));
		const auto height = int(std::ceil(image.height() / ratio));
		auto textWidth = width;
		if (const auto label = dynamic_cast<Ui::FlatLabel*>(_widget.data())) {
			textWidth = std::clamp(label->textMaxWidth(), 1, width);
		}
		{
			auto q = QPainter(&image);
			q.setCompositionMode(QPainter::CompositionMode_SourceIn);
			q.fillRect(
				QRect(0, 0, width, height),
				QBrush(ShimmerGradient(
					palette,
					_glint(),
					0,
					textWidth,
					crl::now())));
		}
		painter->drawImage(offset, image);
		auto &cache = Styles();
		cache.effects.push_back(this);
		cache.schedule();
	}

private:
	const QPointer<QWidget> _widget;
	const Fn<QString()> _palette;
	const Fn<bool()> _glint;

};

Cache::Cache() : ticker([=] { tick(); }) {
}

void Cache::schedule() {
	if (!ticker.isActive()) {
		ticker.callOnce(kFrame);
	}
}

void Cache::tick() {
	// Every shimmering paint registers itself again, so a name scrolled away
	// or a closed profile simply stops being repainted.
	auto repaintsNow = base::take(repaints);
	for (const auto &[widget, rect] : repaintsNow) {
		if (widget) {
			widget->update(rect);
		}
	}
	auto effectsNow = base::take(effects);
	for (const auto &effect : effectsNow) {
		if (effect) {
			effect->update();
		}
	}
}

void Cache::changed() {
	// Names painted before the styles arrived have nothing registered yet.
	for (const auto widget : QApplication::topLevelWidgets()) {
		widget->update();
	}
}

} // namespace

bool IsPalette(const QString &palette) {
	return !Stops(palette).empty();
}

QString PaletteName(const QString &palette) {
	if (palette == u"rainbow"_q) {
		return Tr(u"Радуга"_q, u"Rainbow"_q);
	} else if (palette == u"red"_q) {
		return Tr(u"Красный"_q, u"Red"_q);
	} else if (palette == u"white"_q) {
		return Tr(u"Белый"_q, u"White"_q);
	} else if (palette == u"gold"_q) {
		return Tr(u"Золото"_q, u"Gold"_q);
	} else if (palette == u"neon"_q) {
		return Tr(u"Неон"_q, u"Neon"_q);
	} else if (palette == u"ocean"_q) {
		return Tr(u"Океан"_q, u"Ocean"_q);
	} else if (palette == u"sunset"_q) {
		return Tr(u"Закат"_q, u"Sunset"_q);
	}
	return Tr(u"Выкл."_q, u"Off"_q);
}

QString PaletteFor(not_null<PeerData*> peer, Field field) {
	const auto style = Find(peer);
	if (!style) {
		return QString();
	}
	if (field == Field::UsernameAlso && peer->usernames().size() < 2) {
		return QString();
	}
	const auto &palette = (field == Field::Nick)
		? style->nick
		: (field == Field::Number)
		? style->number
		: style->username;
	return IsPalette(palette) ? palette : QString();
}

bool GlintFor(not_null<PeerData*> peer) {
	const auto style = Find(peer);
	return style ? style->glint : true;
}

void PutOwnStyle(not_null<Main::Session*> session, const NameStyle &style) {
	auto &cache = Styles();
	const auto id = session->userId().bare;
	if (style.nick.isEmpty()
		&& style.number.isEmpty()
		&& style.username.isEmpty()) {
		cache.styles.erase(id);
	} else {
		cache.styles[id] = style;
	}
	cache.changed();
}

QLinearGradient ShimmerGradient(
		const QString &palette,
		bool glint,
		int left,
		int width,
		crl::time now) {
	const auto stops = Stops(palette);
	auto result = QLinearGradient(left, 0, left + std::max(width, 1), 0);
	if (stops.empty()) {
		return result;
	}
	const auto w = double(std::max(width, 1));
	const auto tile = double(style::ConvertScale(200));
	const auto phase = double(now % kPeriod) / kPeriod;
	const auto offset = phase * tile;
	const auto bandCenter = -style::ConvertScale(40.)
		+ (w + style::ConvertScale(80.)) * phase;
	const auto bandWidth = style::ConvertScale(27.);
	const auto dark = Window::Theme::IsNightMode();
	const auto platinum = (palette == u"white"_q) && !dark;
	const auto glintStrength = glint
		? ((dark ? .85 : .45) * (platinum ? .65 : 1.))
		: 0.;
	const auto count = int(stops.size());
	for (auto i = 0; i != kSamples; ++i) {
		const auto t = i / double(kSamples - 1);
		const auto x = w * t;
		const auto p = std::fmod(std::fmod(x - offset, tile) + tile, tile)
			/ tile
			* (count - 1);
		const auto a = int(std::floor(p));
		const auto b = std::min(a + 1, count - 1);
		auto color = Blend(stops[a], stops[b], p - a);
		if (glintStrength > 0.) {
			const auto d = (x - bandCenter) / bandWidth;
			color = Blend(color, Qt::white, std::exp(-d * d) * glintStrength);
		}
		result.setColorAt(t, color);
	}
	return result;
}

bool SetShimmerPen(
		QPainter &p,
		PeerData *peer,
		Field field,
		QRect textRect) {
	if (!peer || peer->isSelf() || textRect.width() <= 0) {
		return false;
	}
	const auto palette = PaletteFor(peer, field);
	if (palette.isEmpty()) {
		return false;
	}
	p.setPen(QPen(QBrush(ShimmerGradient(
		palette,
		GlintFor(peer),
		textRect.x(),
		textRect.width(),
		crl::now())), 1));
	auto &cache = Styles();
	if (const auto widget = dynamic_cast<QWidget*>(p.device())) {
		const auto rect = p.transform().mapRect(textRect).adjusted(
			-1,
			-1,
			1,
			1);
		cache.repaints.emplace_back(widget, rect);
		cache.schedule();
	}
	return true;
}

void MarkLabel(
		not_null<QWidget*> widget,
		not_null<UserData*> user,
		Field field) {
	widget->setGraphicsEffect(new ShimmerEffect(
		widget,
		[=] { return PaletteFor(user, field); },
		[=] { return GlintFor(user); }));
	Refresh(&user->session());
}

void MarkPreview(
		not_null<QWidget*> widget,
		Fn<QString()> palette,
		Fn<bool()> glint) {
	widget->setGraphicsEffect(new ShimmerEffect(
		widget,
		std::move(palette),
		std::move(glint)));
}

NameStyle ParseNameStyle(const QJsonObject &json) {
	return ParseStyleJson(json);
}

QJsonObject NameStyleJson(const NameStyle &style) {
	auto result = QJsonObject();
	result.insert(u"nick"_q, style.nick);
	result.insert(u"number"_q, style.number);
	result.insert(u"username"_q, style.username);
	result.insert(u"glint"_q, style.glint);
	result.insert(u"glow"_q, style.glow);
	return result;
}

} // namespace Oceangram
