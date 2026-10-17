// Le refus d'office des échanges. Voir l'en-tête.

#include "options/deal_refusal_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// La clé du fichier. ⚠ GRAVÉE, et en anglais : la changer rend à tous les
// joueurs le réglage d'origine, sans que rien ne le dise.
constexpr const char* kRefuseDealsKey = "deal_refuse_requests";

}  // namespace

void DealRefusalOptions::Declare(SettingsFile& into) {
  into.Declare(kRefuseDealsKey, &refuses_deals_);
}

void DealRefusalOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(ro::ui::OptionRowView{
      .id = DealRefusalRowId(DealRefusalRow::kRefuseDeals),
      .label = Tr("Refuser d'office les échanges"),
      .help = Tr("Toute demande d'échange reçue se refuse d'elle-même, sans que la boîte ne "
                 "paraisse. Celui qui la propose l'apprend comme un refus."),
      .command = "/notrade",
      .kind = ro::ui::OptionKind::kToggle,
      .on = refuses_deals_});
  rows.back().heading = Tr("Échanges");
}

bool DealRefusalOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed != DealRefusalRowId(DealRefusalRow::kRefuseDeals)) {
    return false;
  }
  refuses_deals_ = asked.on;
  return true;
}

}  // namespace moonclient
