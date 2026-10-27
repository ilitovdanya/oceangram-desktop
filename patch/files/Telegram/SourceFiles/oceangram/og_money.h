/*
OceanGram Desktop: «Деньги». The server holds every wallet (custody) and has no
chain, so a transfer is not a signed TON message: the client sends the intent
("OGT1" + JSON) in wallet.sendTransfer and the server settles it on its ledger,
exactly like the OceanGram Android app does.
*/
#pragma once

class UserData;

namespace Main {
class SessionShow;
} // namespace Main

namespace Oceangram {

// Replaces the wallet's own send box for another user: no seed phrase, no
// local signing. |sent| runs after the server accepted the transfer.
void ShowSendGram(
	std::shared_ptr<Main::SessionShow> show,
	not_null<UserData*> user,
	Fn<void()> sent,
	int64 amountNano);

// "1.5" -> 1'500'000'000; 0 when the text is not a positive GRAM amount.
[[nodiscard]] int64 ParseGramAmount(const QString &text);

// 1'500'000'000 -> "1.5".
[[nodiscard]] QString FormatGramAmount(int64 nano);

} // namespace Oceangram
