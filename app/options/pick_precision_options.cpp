#include "options/pick_precision_options.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include <imgui.h>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// La clé du fichier.
constexpr const char* kShiftKey = "targeting_precision_notch";

// La place d'une phrase formatée, et d'un cran écrit avec sa taille.
constexpr std::size_t kIntroRoom = 640;
constexpr std::size_t kChoiceRoom = 96;

}  // namespace

int PickPrecisionOptions::floor_shift() const noexcept {
  return std::clamp(floor_shift_, 0, ro::render::kLargestPickFloorShift);
}

void PickPrecisionOptions::Declare(SettingsFile& into) { into.Declare(kShiftKey, &floor_shift_); }

void PickPrecisionOptions::Compose(float viewport_width) {
  rows_.clear();
  const auto game_actor = static_cast<int>(
      ro::render::PickFloorPixels(ro::render::kActorPickFloorFactor, viewport_width, 0));
  const auto game_ground = static_cast<int>(
      ro::render::PickFloorPixels(ro::render::kGroundPickFloorFactor, viewport_width, 0));

  // 🔴 UNE SEULE RANGÉE : la liste. Le préambule et la légende passent dans son
  // infobulle — trois paragraphes au-dessus des onglets repousseraient les
  // réglages de chaque espèce hors de l'écran.
  //
  // ⚠ L'INFOBULLE DONNE LES CHIFFRES DE CET ÉCRAN, pas une généralité : le
  // minimum dépend de la fenêtre, et c'est justement ce que le joueur n'a aucun
  // moyen de deviner.
  std::array<char, kIntroRoom> intro{};
  std::snprintf(intro.data(), intro.size(),
                Tr("Le jeu ÉLARGIT la surface qui répond au clic de chaque entité jusqu'à un "
                   "minimum qui dépend de la largeur de la fenêtre — sur ton écran : %d px pour "
                   "les monstres, les joueurs et les PNJ, %d px pour les items au sol et les "
                   "unités de skill. Un petit monstre se défend donc sur une surface bien "
                   "plus grande que lui, et deux entités proches se disputent le même clic."),
                game_actor, game_ground);
  help_ = std::string(intro.data()) + "\n\n" +
          Tr("Réduit ce minimum : le ciblage devient plus précis — deux entités voisines "
             "cessent de se disputer le même clic — mais les petits monstres deviennent "
             "plus exigeants à viser.\n\n"
             "« Moitié » est un bon départ. La proportion à la fenêtre est conservée, et le "
             "réglage s'applique aussi aux items au sol et aux unités de skill.") +
          "\n\n" +
          Tr("Dans le cadre du mode réglage : en jaune, la surface qui répond au clic avec "
             "ton réglage ; en gris, celle d'origine.");

  // ⚠ UN LITTÉRAL PAR CRAN : le relevé des traductions ne lit que les
  // littéraux passés à `Tr`.
  const std::array<const char*, kPickShiftCount> names = {
      Tr("Réglage d'origine du jeu"), Tr("Moitié"), Tr("Quart"), Tr("Huitième"), Tr("Seizième")};
  for (std::size_t shift = 0; shift < kPickShiftCount; ++shift) {
    std::array<char, kChoiceRoom> text{};
    std::snprintf(text.data(), text.size(), "%s (%d px)", names[shift],
                  static_cast<int>(ro::render::PickFloorPixels(
                      ro::render::kActorPickFloorFactor, viewport_width, static_cast<int>(shift))));
    choice_texts_[shift] = text.data();
    choices_[shift] = choice_texts_[shift].c_str();
  }
  rows_.push_back(OptionRowView{.id = PickPrecisionRowId(PickPrecisionRow::kShift),
                                .label = Tr("Zone cliquable minimale"),
                                .help = help_.c_str(),
                                .help_warning = Tr("Sans effet sur un GROS sprite : sa surface "
                                                   "vient de son dessin, pas de ce minimum."),
                                .kind = OptionKind::kChoice,
                                .chosen = floor_shift(),
                                .choices = choices_});
}

ro::ui::EntityPickZones PickPrecisionOptions::ZonesFor(float factor, float viewport_width) const {
  return ro::ui::EntityPickZones{
      .game_floor_pixels = ro::render::PickFloorPixels(factor, viewport_width, 0),
      .floor_pixels = ro::render::PickFloorPixels(factor, viewport_width, floor_shift())};
}

bool PickPrecisionOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kPickPrecisionRowBase ||
      asked.changed >= kPickPrecisionRowBase + static_cast<int>(PickPrecisionRow::kCount)) {
    return false;
  }
  if (asked.changed == PickPrecisionRowId(PickPrecisionRow::kShift)) {
    floor_shift_ = std::clamp(asked.chosen, 0, ro::render::kLargestPickFloorShift);
  }
  return true;
}

}  // namespace moonclient
