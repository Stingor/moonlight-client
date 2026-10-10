#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/image.h"
#include "ro/formats/spr.h"
#include "ro/render/battlefield_art.h"
#include "ro/render/screen_overlay.h"

namespace ro::render {

// ── LE SCORE DU CHAMP DE BATAILLE, EN HAUT À DROITE ─────────────────────────
//
// Deux rangées : l'emblème du lion et ses points, puis celui de l'aigle et les
// siens. Les chiffres sont ceux de la fonte du classement PVP (`kPvpRankFontPath`) :
// l'action N porte le chiffre N, en une image.
//
// Les cotes sont en pixels d'écran, comptées depuis le bord droit pour les
// abscisses et depuis le haut pour les ordonnées.

// Les deux emblèmes : même colonne, deux rangées.
inline constexpr float kBattleEmblemLeftFromRightPx = 280.0F;
inline constexpr float kBattleEmblemRightFromRightPx = 232.0F;
inline constexpr float kLionEmblemTopPx = 26.0F;
inline constexpr float kLionEmblemBottomPx = 74.0F;
inline constexpr float kEagleEmblemTopPx = 84.0F;
inline constexpr float kEagleEmblemBottomPx = 132.0F;
// LES UNITÉS se posent plus à droite dès qu'un des deux scores a deux chiffres ;
// les dizaines, un pas à leur gauche.
inline constexpr float kBattleUnitsFromRightPx = 207.0F;
inline constexpr float kBattleUnitsWideFromRightPx = 175.0F;
inline constexpr float kBattleDigitPitchPx = 32.0F;
inline constexpr float kLionDigitsTopPx = 50.0F;
inline constexpr float kEagleDigitsTopPx = 108.0F;
// Au-delà, le score se montre borné : deux chiffres, pas trois.
inline constexpr std::int32_t kBattleScoreMost = 99;

// LA MARQUE D'UN ENNEMI, au-dessus de sa tête : son coin haut-gauche part du
// point d'écran des pieds, décalé d'une fraction de l'écran mesurée sur un
// écran de référence, puis mise à l'échelle de la hauteur du sprite.
inline constexpr float kEnemyMarkerReferenceWidthPx = 640.0F;
inline constexpr float kEnemyMarkerReferenceHeightPx = 480.0F;
inline constexpr float kEnemyMarkerShiftXPx = -5.0F;
inline constexpr float kEnemyMarkerShiftYPx = -100.0F;

// Les deux emblèmes sont des images à fond blanc : elles se posent un peu
// transparentes, et le fond reste visible comme un cartouche pâle.
inline constexpr std::uint8_t kBattleEmblemAlpha = 205;

// Le camp que le HUD surligne : celui du joueur, s'il est connu et que le
// réglage le demande.
enum class BattleScoreHighlight : std::uint8_t { kNone, kLion, kEagle };

struct BattleScoreFigures {
  std::int32_t lion = 0;
  std::int32_t eagle = 0;
  BattleScoreHighlight highlight = BattleScoreHighlight::kNone;
};

// LES QUADS DU SCORE. Rien si l'un des deux est négatif — inconnu.
[[nodiscard]] std::vector<ScreenQuad> ComposeBattleScore(const formats::SprSprite& digits_sprite,
                                                         const formats::ActAnimation& digits,
                                                         const BattlefieldArt& art,
                                                         const BattleScoreFigures& figures,
                                                         glm::vec2 viewport_px);

// LA MARQUE D'UN ENNEMI, à sa taille d'image, coin haut-gauche en
// `feet_px` + décalage × `height_scale` (le facteur de hauteur du sprite, celui
// qui place aussi la bulle de parole). Un quad vide — d'aire nulle — si
// l'image manque.
[[nodiscard]] ScreenQuad ComposeEnemyMarker(const formats::Image& marker, glm::vec2 feet_px,
                                            glm::vec2 viewport_px, float height_scale);

}  // namespace ro::render
