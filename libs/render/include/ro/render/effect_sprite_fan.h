#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"
#include "ro/render/effect_trail_history.h"

namespace ro::render {

// L'ÉVENTAIL DE SPRITES : le sous-type 167.
//
// La figure, et la part du mouvement d'émission que le cas 167 atteint. TROIS
// identifiants — 901, 923 et 941.
//
// ⚠ CE N'EST PAS UN QUAD, C'EST UN SPRITE. La figure emprunte le chemin des
// acteurs, comme le sprite éclatant du sous-type 153, et non celui des quads du
// monde.
//
// ⚠ ET SON DESSINATEUR CROISE LES PISTES ET LES CRANS. Le client boucle sur ses
// quatre pistes, et dans chacune sur les crans de traînée du nœud ; la position
// d'une pose est la somme des deux. Les crans valent ici des POSES et non un
// historique — le constructeur en pose UN, si bien qu'un nœud rend quatre
// sprites et non seize.

// LE PAS D'ÉCARTEMENT que le bâtisseur pose, et où il vit.
//
// ⚠ C'EST TOUT LE MOUVEMENT DE LA FIGURE. Les autres branches du mouvement
// d'émission sont gardées par des bits de drapeaux que ce bâtisseur ne pose pas
// ; il ne pose que celui de la teinte.
inline constexpr std::size_t kSpriteFanValueOfSpread = 0;

// LA MARQUE QUI CHOISIT LA TEINTE, et les trois valeurs que les sites donnent.
inline constexpr std::size_t kSpriteFanMarkOfColor = 0;
inline constexpr std::uint8_t kSpriteFanLayerMark = 0;
inline constexpr std::uint8_t kSpriteFanBlueMark = 1;
inline constexpr std::uint8_t kSpriteFanUntintedMark = 4;

// DE COMBIEN L'ALPHA DES CALQUES EST RABATTU : quatre-vingt-cinq pour cent.
inline constexpr float kSpriteFanAlphaShare = 0.85F;

// LE BLEU VIF de la marque un, tel que le client l'écrit en un seul mot :
// rouge et vert à cinquante-cinq, bleu plein.
inline constexpr std::uint8_t kSpriteFanShade = 55;
inline constexpr std::array<std::uint8_t, 3> kSpriteFanBlue = {kSpriteFanShade, kSpriteFanShade,
                                                               255};

// LE HALO : chaque sprite est repeint une seconde fois, élargi de quatre pixels
// d'ÉCRAN sur chacun de ses côtés, à la moitié de l'alpha de son calque et
// sous le même mot de rendu. Quatre pixels d'écran ne sont pas un facteur
// d'échelle : un sprite lointain en gagne autant qu'un proche.
inline constexpr float kSpriteFanHaloScreenPixels = 4.0F;
inline constexpr float kSpriteFanHaloAlphaShare = 0.5F;

// LA TEINTE d'une piste, alpha compris.
//
// ⚠ LA MARQUE QUATRE NE POSE RIEN CHEZ LE CLIENT : son dessinateur n'a que deux
// branches — zéro et un —, si bien que la teinte du quad PRÉCÉDENT lui survit.
// C'est indéterminé, et nous rendons du blanc plein plutôt que du sable. Ce
// n'est pas le comportement du client : c'est le choix le moins surprenant
// devant un défaut du client.
[[nodiscard]] std::array<std::uint8_t, 4> EffectSpriteFanTint(std::uint8_t mark);

// LA TEINTE DU HALO d'une piste, alpha compris.
//
// ⚠ ELLE N'EST PAS CELLE DE LA PREMIÈRE PASSE. La marque zéro garde le ROUGE du
// calque et pose le vert et le bleu à cinquante-cinq : son halo rougeoie. La
// marque un garde son bleu. Toute autre marque ne pose rien, et le halo reprend
// alors la teinte de sa première passe.
[[nodiscard]] std::array<std::uint8_t, 4> EffectSpriteFanHaloTint(std::uint8_t mark);

// LES POSES D'UN NŒUD : une par piste vive et par cran.
//
// `history` porte les crans du nœud ; un historique VIDE vaut un cran à
// l'origine, ce qui est le cas de cette famille.
[[nodiscard]] std::vector<EffectTrailStep> ComposeSpriteFanPoses(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks,
    const std::vector<EffectTrailStep>& history, glm::vec3 anchor);

// LES POSES DU HALO, rang pour rang celles de `ComposeSpriteFanPoses` — même
// point, même échelle —, teintes par `EffectSpriteFanHaloTint`. La i-ème se
// peint juste après la i-ème pose, élargie de `kSpriteFanHaloScreenPixels`.
[[nodiscard]] std::vector<EffectTrailStep> ComposeSpriteFanHaloPoses(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks,
    const std::vector<EffectTrailStep>& history, glm::vec3 anchor);

// LE MOUVEMENT DU SOUS-TYPE 167 : chaque piste vive s'écarte du nœud, dans la
// direction de son azimut.
//
// ⚠ ET LE NŒUD MEURT PAR SON ANIMATION : quatre tics par image, sans boucle, et
// la dernière image le tue. Ce n'est pas ici que cela se décide — la troupe le
// fait pour toute la famille des sprites, sur le `frame_hold_ticks` et le
// `loops_frames` que le semis pose —, mais le noter évite de chercher une garde
// de vie qui n'existe pas.
void AdvanceSpriteFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
