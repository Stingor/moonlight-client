// Les réglages du champ de bataille. Voir l'en-tête.

#include "options/battleground_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Les clés du fichier. ⚠ GRAVÉES, et en anglais : les changer rend à tous les
// joueurs les réglages d'origine, sans que rien ne le dise.
constexpr const char* kScoreShownKey = "battleground_score_shown";
constexpr const char* kOwnSideHighlightedKey = "battleground_own_side_highlighted";
constexpr const char* kMinimapMatesKey = "battleground_minimap_mates";
constexpr const char* kEnemyMarkerKey = "battleground_enemy_marker";

[[nodiscard]] ro::ui::OptionRowView Toggle(BattlegroundRow row, const char* label, const char* help,
                                           bool on) {
  return ro::ui::OptionRowView{.id = BattlegroundRowId(row),
                               .label = label,
                               .help = help,
                               .kind = ro::ui::OptionKind::kToggle,
                               .on = on};
}

}  // namespace

void BattlegroundOptions::Declare(SettingsFile& into) {
  into.Declare(kScoreShownKey, &settings_.score_shown);
  into.Declare(kOwnSideHighlightedKey, &settings_.own_side_highlighted);
  into.Declare(kMinimapMatesKey, &settings_.minimap_mates);
  into.Declare(kEnemyMarkerKey, &settings_.enemy_marker);
}

void BattlegroundOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(Toggle(BattlegroundRow::kScoreShown, Tr("Score en haut à droite"),
                        Tr("Les points du lion et de l'aigle, sur une carte de champ de "
                           "bataille seulement."),
                        settings_.score_shown));
  rows.back().heading = Tr("Champ de bataille");
  // LE SURLIGNAGE PRÉCISE LE SCORE : sous lui, et grisé quand il est masqué.
  ro::ui::OptionRowView own_side = Toggle(
      BattlegroundRow::kOwnSideHighlighted, Tr("Surligner mon camp"),
      Tr("Un cartouche derrière la rangée de ton camp. Il paraît dès qu'un joueur de l'autre "
         "camp a été vu : c'est ce qui dit de quel côté tu es."),
      settings_.own_side_highlighted);
  own_side.indented = true;
  own_side.disabled = !settings_.score_shown;
  rows.push_back(own_side);
  rows.push_back(Toggle(BattlegroundRow::kMinimapMates, Tr("Coéquipiers sur la minimap"),
                        Tr("Un losange à la couleur de ton camp pour chaque coéquipier de la "
                           "carte ; son nom au survol."),
                        settings_.minimap_mates));
  rows.push_back(Toggle(BattlegroundRow::kEnemyMarker, Tr("Marque au-dessus des ennemis"),
                        Tr("Une marque au-dessus de la tête de chaque joueur du camp adverse. "
                           "Un ennemi caché n'en porte pas."),
                        settings_.enemy_marker));
}

bool BattlegroundOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kBattlegroundRowBase ||
      asked.changed >= BattlegroundRowId(BattlegroundRow::kCount)) {
    return false;
  }
  switch (static_cast<BattlegroundRow>(asked.changed - kBattlegroundRowBase)) {
    case BattlegroundRow::kScoreShown:
      settings_.score_shown = asked.on;
      break;
    case BattlegroundRow::kOwnSideHighlighted:
      settings_.own_side_highlighted = asked.on;
      break;
    case BattlegroundRow::kMinimapMates:
      settings_.minimap_mates = asked.on;
      break;
    case BattlegroundRow::kEnemyMarker:
      settings_.enemy_marker = asked.on;
      break;
    case BattlegroundRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
