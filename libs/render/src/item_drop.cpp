#include "ro/render/item_drop.h"

#include <algorithm>
#include <array>

namespace ro::render {
namespace {

// LES CINQ PILIERS, DANS L'ORDRE DES MODES 1 À 5 DU SERVEUR, et leurs fichiers
// tels que le client les écrit en dur.
constexpr std::array<std::uint16_t, 5> kPillarByMode = {kItemDropPillarBlue, kItemDropPillarYellow,
                                                        kItemDropPillarPurple, kItemDropPillarGreen,
                                                        kItemDropPillarRed};

// Le rose, qui n'est le pilier d'aucun mode : c'est le DÉFAUT de la classe du
// client, celui que prend tout identifiant qu'elle ne nomme pas.
constexpr ItemDropPillar kPinkPillar = {
    "new_dropitem\\dropitem_pink\\dropitem_pink\\dropitem_pink.str",
    "new_dropitem\\dropitem_pink\\dropitem_pink_bottom\\dropitem_pink_bottom.str",
    "effect\\drop_pink.wav"};

// L'âge de l'objet en TICS du client : c'est dans cette unité que la parabole
// et le scintillement sont écrits, et arrondir ailleurs les décalerait tous
// les deux.
[[nodiscard]] float TicksSince(std::uint32_t elapsed_ms) noexcept {
  return static_cast<float>(elapsed_ms) / kItemTickMs;
}

// La cloche d'un arc : zéro au départ, un à mi-course, zéro à l'arrivée.
[[nodiscard]] float Bell(float part) noexcept { return 4.0F * part * (1.0F - part); }

// La part d'un intervalle déjà parcourue, bornée à un.
[[nodiscard]] float PartOf(std::uint32_t elapsed_ms, std::uint32_t span_ms) noexcept {
  if (span_ms == 0) {
    return 1.0F;
  }
  return std::min(1.0F, static_cast<float>(elapsed_ms) / static_cast<float>(span_ms));
}

}  // namespace

float ItemFallUpUnits(std::uint32_t elapsed_ms) noexcept {
  const float ticks = TicksSince(elapsed_ms);
  // La parabole du client, retournée : il compte vers le bas depuis « sol
  // moins quinze », nous vers le haut depuis le sol.
  const float up = kItemFallBirthUnits +
                   (kItemFallRiseUnitsPerTick - kItemFallPullUnitsPerTick2 * ticks) * ticks;
  // Sous le sol, l'objet EST au sol : le client y pose la hauteur du terrain et
  // éteint sa chute.
  return std::max(0.0F, up);
}

bool ItemFallIsOver(std::uint32_t elapsed_ms) noexcept {
  return ItemFallUpUnits(elapsed_ms) <= 0.0F;
}

bool ItemSparklesAt(std::uint32_t elapsed_ms) noexcept {
  const auto ticks = static_cast<std::uint32_t>(TicksSince(elapsed_ms));
  return ticks % kItemSparklePeriodTicks >= kItemSparkleFirstLitTick;
}

namespace {

// La durée entière d'un arc : son vol et ses deux rebonds.
[[nodiscard]] std::uint32_t WholeArcMs(const ItemDropSettings& settings) noexcept {
  const std::uint32_t flight_ms = std::max(settings.duration_ms, kItemArcMinDurationMs);
  const auto span = static_cast<float>(flight_ms);
  return flight_ms + static_cast<std::uint32_t>(span * kItemBounce1Duration) +
         static_cast<std::uint32_t>(span * kItemBounce2Duration);
}

}  // namespace

bool ItemArcIsOver(const ItemDropSettings& settings, std::uint32_t elapsed_ms) noexcept {
  return elapsed_ms >= WholeArcMs(settings);
}

ItemArcStep ItemArcAt(const ItemDropSettings& settings, std::uint32_t elapsed_ms,
                      float source_up_units) noexcept {
  const std::uint32_t flight_ms = std::max(settings.duration_ms, kItemArcMinDurationMs);
  const auto bounce1_ms =
      static_cast<std::uint32_t>(static_cast<float>(flight_ms) * kItemBounce1Duration);
  const auto bounce2_ms =
      static_cast<std::uint32_t>(static_cast<float>(flight_ms) * kItemBounce2Duration);

  ItemArcStep step;
  if (elapsed_ms < flight_ms) {
    const float part = PartOf(elapsed_ms, flight_ms);
    // L'horizontale DÉCÉLÈRE : l'objet est éjecté vite, puis se pose. La
    // verticale suit une cloche.
    const float left = 1.0F - part;
    step.advance = 1.0F - left * left;
    // LA CLOCHE, ET LA HAUTEUR DU CORPS QUI S'EFFACE. La seconde part de la
    // taille de qui a lâché l'objet et tombe à rien en arrivant : c'est ce qui
    // fait sortir le butin d'un CORPS plutôt que du sol.
    step.up_units = settings.height_units * Bell(part) + source_up_units * (1.0F - step.advance);
    return step;
  }
  // Les rebonds se font SUR PLACE : la case d'arrivée est déjà atteinte.
  if (elapsed_ms < flight_ms + bounce1_ms) {
    step.up_units = settings.height_units * kItemBounce1Height *
                    Bell(PartOf(elapsed_ms - flight_ms, bounce1_ms));
    return step;
  }
  if (elapsed_ms < flight_ms + bounce1_ms + bounce2_ms) {
    step.up_units = settings.height_units * kItemBounce2Height *
                    Bell(PartOf(elapsed_ms - flight_ms - bounce1_ms, bounce2_ms));
    return step;
  }
  step.over = true;
  return step;
}

std::uint16_t ItemDropPillarEffectId(std::uint16_t mode) noexcept {
  if (mode == 0 || mode > kPillarByMode.size()) {
    return 0;
  }
  return kPillarByMode[mode - 1];
}

ItemDropPillar ItemDropPillarOf(std::uint16_t effect_id) noexcept {
  switch (effect_id) {
    case kItemDropPillarBlue:
      return {"new_dropitem\\dropitem_blue\\dropitem_blue\\dropitem_blue.str",
              "new_dropitem\\dropitem_blue\\dropitem_blue_bottom\\dropitem_blue_bottom.str",
              "effect\\drop_blue.wav"};
    case kItemDropPillarYellow:
      return {"new_dropitem\\dropitem_yellow\\dropitem_yellow\\dropitem_yellow.str",
              "new_dropitem\\dropitem_yellow\\dropitem_yellow_bottom\\dropitem_yellow_bottom.str",
              "effect\\drop_yellow.wav"};
    case kItemDropPillarPurple:
      return {"new_dropitem\\dropitem_purple\\dropitem_purple\\dropitem_purple.str",
              "new_dropitem\\dropitem_purple\\dropitem_purple_bottom\\dropitem_purple_bottom.str",
              "effect\\drop_purple.wav"};
    case kItemDropPillarGreen:
      return {"new_dropitem\\dropitem_green\\dropitem_green\\dropitem_green.str",
              "new_dropitem\\dropitem_green\\dropitem_green_bottom\\dropitem_green_bottom.str",
              "effect\\drop_green.wav"};
    case kItemDropPillarRed:
      return {"new_dropitem\\dropitem_red\\dropitem_red\\dropitem_red.str",
              "new_dropitem\\dropitem_red\\dropitem_red_bottom\\dropitem_red_bottom.str",
              "effect\\drop_red.wav"};
    default:
      return kPinkPillar;
  }
}

}  // namespace ro::render
