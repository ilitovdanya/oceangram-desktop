/*
OceanGram Desktop: the profile «Баланс: 💎 X ›» pill under the profile's
actions, laid out like the saved-music pill. Visible to everyone unless the
owner hides it («Скрыть баланс в профиле» in their own balance box).
*/
#pragma once

#include "ui/widgets/buttons.h"
#include "ui/text/text.h"

class UserData;

namespace Ui {
class VerticalLayout;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Oceangram {

class BalanceButton final : public Ui::RippleButton {
public:
	BalanceButton(QWidget *parent, Fn<void()> handler);

	void setBalance(int64 nano);
	void setOverrideBg(std::optional<QColor> color);

private:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

	Ui::Text::String _label;
	Ui::Text::String _amount;
	std::optional<QColor> _overrideBg;

};

void AddBalancePill(
	not_null<Ui::VerticalLayout*> layout,
	not_null<Window::SessionController*> window,
	not_null<UserData*> user,
	rpl::producer<std::optional<QColor>> topBarColor);

} // namespace Oceangram
