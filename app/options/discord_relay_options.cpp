#include "options/discord_relay_options.h"

#include <utility>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

[[nodiscard]] int IdOf(DiscordRelayRow row) { return kDiscordRelayRowBase + static_cast<int>(row); }

}  // namespace

void DiscordRelayOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  // ⚠ L'AIDE DIT CE QUE LA CASE NE FAIT PAS : décochée, la parole publique de
  // Gonryun part quand même vers Discord, et le joueur doit le savoir avant de
  // s'y fier.
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(DiscordRelayRow::kRelay),
      .label = Tr("Chat Discord (Gonryun seulement)"),
      .help = Tr("Montre dans le fil, sur Gonryun, ce qui se dit sur le canal Discord du "
                 "serveur.\nCe que vous dites à voix haute à Gonryun part sur Discord, case "
                 "cochée ou non : elle ne change que la mention qui l'accompagne.\nC'est le "
                 "serveur qui retient ce réglage, pour ce personnage."),
      .kind = ro::ui::OptionKind::kToggle,
      // ⚠ GRISÉE HORS LIGNE, et là seulement : en jeu, la valeur est celle que
      // le serveur a dite, ou son défaut — vrai — tant qu'il ne l'a pas dite.
      // Hors ligne, elle montre ce même défaut.
      .on = relay_.value_or(true),
      .disabled = !relay_.has_value()});
  rows.back().heading = Tr("Discord");
}

bool DiscordRelayOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed != IdOf(DiscordRelayRow::kRelay)) {
    return false;
  }
  // ⚠ RIEN N'EST DEMANDÉ HORS LIGNE : il n'y a personne à qui le demander.
  if (relay_.has_value()) {
    requested_ = asked.on;
  }
  return true;
}

std::optional<bool> DiscordRelayOptions::TakeRequest() noexcept {
  return std::exchange(requested_, {});
}

}  // namespace moonclient
