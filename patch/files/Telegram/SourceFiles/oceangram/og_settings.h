/*
OceanGram Desktop: the «OceanGram Premium» and «OceanSoft» rows in Settings
and their boxes.
*/
#pragma once

namespace Settings::Builder {
class SectionBuilder;
} // namespace Settings::Builder

namespace Window {
class SessionController;
} // namespace Window

namespace Oceangram {

void BuildSettingsSection(Settings::Builder::SectionBuilder &builder);

void ShowPremiumBox(not_null<Window::SessionController*> controller);
void ShowSoftBox(not_null<Window::SessionController*> controller);

} // namespace Oceangram
