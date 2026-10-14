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
// AU-DELÀ DE 99, SEULS LES DEUX DERNIERS CHIFFRES se montrent : 150 s'affiche
// « 50 », 105 s'affiche « 05 ». Le score n'a que deux places.
inline constexpr std::int32_t kBattleScoreShownModulus = 100;

// LA MARQUE D'UN ENNEMI, au-dessus de sa tête : son coin haut-gauche part du
// point d'écran des pieds, décalé d'une fraction ENTIÈRE de l'écran mesurée sur
// un écran de référence, puis mise à l'échelle de la hauteur du sprite.
//
// 🔴 LA FRACTION SE CALCULE EN ENTIERS, tronquée : `5 × largeur / 640` vaut 10
// sur 1366 pixels, et non 10,67. Les pieds et le coin final se tronquent au
// pixel de même.
inline constexpr std::int32_t kEnemyMarkerReferenceWidthPx = 640;
inline constexpr std::int32_t kEnemyMarkerReferenceHeightPx = 480;
inline constexpr std::int32_t kEnemyMarkerShiftXPx = -5;
inline constexpr std::int32_t kEnemyMarkerShiftYPx = -100;

// LES DEUX EMBLÈMES : des images de 36 pixels à fond blanc, ÉTIRÉES sur 48, et
// teintées de blanc à l'alpha 205 sur 255 (80 %) : le fond blanc reste visible
// comme un cartouche pâle.
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

// LE DÉCALAGE DE LA MARQUE à cette taille d'écran, avant la hauteur du sprite :
// des pixels entiers, la fraction tronquée.
[[nodiscard]] glm::vec2 EnemyMarkerShiftPx(glm::vec2 viewport_px);

// LA MARQUE D'UN ENNEMI, à sa taille d'image, coin haut-gauche en
// `feet_px` + décalage × `height_scale` (le facteur de hauteur du sprite, celui
// qui place aussi la bulle de parole), tronqué au pixel comme les pieds. Un
// quad vide — d'aire nulle — si l'image manque.
[[nodiscard]] ScreenQuad ComposeEnemyMarker(const formats::Image& marker, glm::vec2 feet_px,
                                            glm::vec2 viewport_px, float height_scale);

}  // namespace ro::render
