#include "options/map_sign_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// La clé du fichier. GRAVÉE : la changer rend à tous les joueurs le réglage
// d'origine, sans que rien ne le dise.
constexpr const char* kModeKey = "banner_name_of_map_maps";

}  // namespace

MapSignMode MapSignOptions::mode() const noexcept {
  if (mode_ < 0 || mode_ >= static_cast<int>(MapSignMode::kCount)) {
    return MapSignMode::kEveryTitled;
  }
  return static_cast<MapSignMode>(mode_);
}

void MapSignOptions::Declare(SettingsFile& into) { into.Declare(kModeKey, &mode_); }

void MapSignOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) {
  // ⚠ UN LITTÉRAL PAR ENTRÉE : le relevé des traductions ne lit que les
  // littéraux passés à `Tr`.
  mode_names_[static_cast<std::size_t>(MapSignMode::kEveryTitled)] =
      Tr("Toutes les cartes qui ont un titre");
  mode_names_[static_cast<std::size_t>(MapSignMode::kLikeGame)] = Tr("Celles que le jeu annonce");
  mode_names_[static_cast<std::size_t>(MapSignMode::kNever)] = Tr("Aucune");
  rows.push_back(ro::ui::OptionRowView{
      .id = kMapSignRowBase + static_cast<int>(MapSignRow::kMode),
      .label = Tr("Bandeau du nom de la carte"),
      .help = Tr("À l'arrivée sur une carte, son nom s'affiche en haut de l'écran pendant cinq "
                 "secondes.\n"
                 "Le jeu ne le montre que sur les cartes que sa table annonce — deux cent "
                 "dix-neuf, presque toutes récentes, et aucune ville. Presque toutes les autres "
                 "ont pourtant un titre : ce client peut le montrer aussi."),
      .kind = ro::ui::OptionKind::kChoice,
      .chosen = static_cast<int>(mode()),
      .choices = mode_names_});
}

bool MapSignOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kMapSignRowBase ||
      asked.changed >= kMapSignRowBase + static_cast<int>(MapSignRow::kCount)) {
    return false;
  }
  if (asked.chosen >= 0 && asked.chosen < static_cast<int>(MapSignMode::kCount)) {
    mode_ = asked.chosen;
  }
  return true;
}

}  // namespace moonclient
