/*
OceanGram Desktop: OceanGram Premium — shimmering names (chat list, chat
header, profile), the phone number and, if the owner turns it on, their
usernames (collectible NFT ones too). The server keeps everyone's choice
(oceangram/styles); the palettes are the Android app's ones.
*/
#pragma once

#include <QtCore/QJsonObject>

#include <array>

class PeerData;
class UserData;

namespace Main {
class Session;
} // namespace Main

namespace Oceangram {

enum class Field {
	Nick,
	Number,
	Username,
	// The «also @a, @b» line under the username: only with more than one.
	UsernameAlso,
};

struct NameStyle {
	QString nick;
	QString number;
	QString username;
	bool glint = true;
	bool glow = true;
};

inline constexpr auto kPalettes = std::array<const char*, 7>{
	"rainbow", "red", "white", "gold", "neon", "ocean", "sunset"
};

[[nodiscard]] bool IsPalette(const QString &palette);
[[nodiscard]] NameStyle ParseNameStyle(const QJsonObject &json);
[[nodiscard]] QJsonObject NameStyleJson(const NameStyle &style);
[[nodiscard]] QString PaletteName(const QString &palette);

// The palette drawn for |peer|'s field, empty when off. Asks the server for
// everyone's styles when they are older than a few minutes.
[[nodiscard]] QString PaletteFor(not_null<PeerData*> peer, Field field);
[[nodiscard]] bool GlintFor(not_null<PeerData*> peer);

// The owner's own new style, shown at once.
void PutOwnStyle(not_null<Main::Session*> session, const NameStyle &style);

// A horizontal palette gradient for text from |left| over |width| at |now|.
[[nodiscard]] QLinearGradient ShimmerGradient(
	const QString &palette,
	bool glint,
	int left,
	int width,
	crl::time now);

// Chat list / chat header: replaces the pen before a name is drawn into
// |textRect| and repaints that rect while it shimmers. False when |peer| has
// no palette for |field| (the pen is left as it was).
bool SetShimmerPen(
	QPainter &p,
	PeerData *peer,
	Field field,
	QRect textRect);

// Profile labels: tints whatever |widget| draws with |user|'s palette for
// |field| (links too), animated while visible.
void MarkLabel(
	not_null<QWidget*> widget,
	not_null<UserData*> user,
	Field field);

// The Premium box preview: tints |widget| with whatever |palette| returns.
void MarkPreview(
	not_null<QWidget*> widget,
	Fn<QString()> palette,
	Fn<bool()> glint);

} // namespace Oceangram
