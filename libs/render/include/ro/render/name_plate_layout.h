#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/formats/emblem.h"
#include "ro/render/boss_badge.h"
#include "ro/render/name_plate.h"

namespace ro::render {

// ── LA DISPOSITION D'UNE ÉTIQUETTE ──────────────────────────────────────────
//
// Le client pose chaque étiquette dans un WIDGET centré sous les pieds, dont le
// coin haut-gauche commande tout : une image de tête à gauche — l'emblème de
// guilde, ou à défaut l'insigne de boss —, puis les lignes.
//
// 🔴 TOUTES LES LIGNES COMMENCENT AU MÊME X. Le client ne centre pas chaque
// ligne : il centre le WIDGET, dont la largeur est celle de la plus large. Une
// seconde ligne plus courte s'aligne donc à gauche sur la première — « Moonlight
// [Recrue] » sous « Stingor (Groupe) ».
//
// ⚠ ET LE WIDGET A UNE LARGEUR MINIMALE : un nom très court ne se centre plus
// exactement, il commence à quatre pixels du bord d'un widget de trente-deux.

// L'IMAGE QUI PRÉCÈDE LES LIGNES. ⚠ L'EMBLÈME PASSE DEVANT L'INSIGNE : le
// client ne dessine l'insigne de boss que lorsqu'aucun emblème n'est posé.
enum class NamePlateLead : std::uint8_t { kNone, kBossBadge, kEmblem };

// La largeur minimale du widget, que le texte remplit avec ses deux marges
// (`kNamePlateMarginPixels`).
inline constexpr float kNamePlateLeastWidthPixels = 32.0F;
// Le texte commence à quatre pixels du HAUT du widget, et le widget finit deux
// pixels sous la dernière ligne.
inline constexpr float kNamePlateTopPixels = 4.0F;
inline constexpr float kNamePlateBottomPixels = 2.0F;

// L'EMBLÈME : blitté à quatre pixels du bord gauche, centré en hauteur sur une
// case de vingt-quatre, le texte vingt-huit pixels plus loin — et le widget
// s'élargit de vingt-cinq pour lui.
inline constexpr float kEmblemLeftPixels = 4.0F;
inline constexpr float kEmblemSlotPixels = static_cast<float>(formats::kEmblemSide);
inline constexpr float kEmblemTextShiftPixels = 28.0F;
inline constexpr float kEmblemWidgetGrowPixels = 25.0F;

// L'INSIGNE DE BOSS, lui, est blitté au coin même du widget, le texte à
// `kBossBadgeSizePixels` plus une marge. Le widget vaut alors la largeur
// minimale du texte — ses deux marges comprises — plus l'insigne.

struct NamePlateLayout {
  // L'abscisse où commencent TOUTES les lignes.
  float text_left_x = 0.0F;
  // Le haut de la première ligne, ramené au pixel : c'est de lui que la ligne
  // de base se tire, pour que texte et image partagent la même grille.
  float text_top_y = 0.0F;
  // Le coin haut-gauche de l'image de tête, quand il y en a une.
  glm::vec2 lead_min{0.0F, 0.0F};
};

// `anchor_x` est l'abscisse sous les pieds, sur laquelle le widget se centre ;
// `text_top_y` le HAUT de la première ligne ; `widest_line_px` la largeur de
// la plus large ; `line_step_px` l'écart d'une ligne à la suivante.
//
// ⚠ TOUT TOMBE SUR DES PIXELS ENTIERS, colonnes et rangées : une image de tête
// posée à cheval sur deux se flouterait, et le client ne connaît que des
// pixels entiers — il tronque l'ancre et divise la largeur en entiers.
[[nodiscard]] NamePlateLayout PlaceNamePlate(float anchor_x, float text_top_y, float widest_line_px,
                                             std::size_t line_count, float line_step_px,
                                             NamePlateLead lead);

}  // namespace ro::render
