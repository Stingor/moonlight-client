#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_screen_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA NAPPE AU SOL : soixante carreaux jointifs, posés à plat et tournés vers la
// caméra.
//
// Le dessinateur du sous-type 72. Il sort par la voie A, soixante fois par
// piste vive.
//
// ⚠ CE N'EST PAS UNE GRILLE DE TRAITS MAIS UNE NAPPE PLEINE. Chaque carreau a le
// côté du rayon de la piste — trente unités — et le pas des deux boucles vaut
// trente aussi : les carreaux se touchent exactement, et l'ensemble fait une
// surface continue de trois cents unités sur cent quatre-vingts. Les espacer
// d'autre chose que leur côté y ouvrirait des fentes.
//
// ⚠ SA HAUTEUR EST LA MÊME POUR LES QUATRE COINS, et elle ne vient pas de la
// piste : le client l'écrit en dur, à dix unités au-dessus de l'ancre dans son
// repère qui descend. La nappe est donc plate, et c'est la ROTATION qui la
// couche.
//
// ⚠ ET LA CLÉ DE LA PISTE EST UNE ORIGINE, non une hauteur (ADR 0017). Elle se
// retranche des deux axes du sol à la fois : c'est de quoi faire glisser la
// nappe entière d'un carreau, et son mouvement la reboucle à trente pour qu'elle
// ne s'en aille jamais plus loin.

// LE PAS DE LA NAPPE, en unités du monde, et les bornes des deux boucles. Dix
// carreaux dans un sens, six dans l'autre.
inline constexpr std::int32_t kGridPlaneStep = 30;
inline constexpr std::int32_t kGridPlaneFirstAxisFrom = -150;
inline constexpr std::int32_t kGridPlaneFirstAxisTo = 150;
inline constexpr std::int32_t kGridPlaneSecondAxisFrom = -90;
inline constexpr std::int32_t kGridPlaneSecondAxisTo = 90;

// LE MOT DE RENDU : celui du constructeur, auquel le bâtisseur ajoute le bit
// qui peint PAR-DESSUS TOUT. Voir `ComposeScreenGridPlaneQuads`, qui en tire la
// conséquence jusqu'au bout.
inline constexpr std::uint32_t kGridPlaneRenderWord = 0x201;

// LA HAUTEUR DES QUATRE COINS, écrite en dur par le client. NÉGATIVE : son
// repère descend, donc la nappe est au-dessus de son ancre.
inline constexpr float kGridPlaneHeight = -10.0F;

// LA MARQUE QUI CHOISIT LA TEINTE, et son sens appartient à cette figure
// (ADR 0017) — le mouvement la lit, lui, comme un RYTHME.
inline constexpr std::size_t kMarkOfGridPlaneTint = 10;

// LES QUATRE TEINTES, et deux marques donnent la même : zéro et trois peignent
// blanc, un peint rouge, tout le reste peint bleu. Les ranger sur une échelle
// ferait croire à une progression qui n'existe pas.
[[nodiscard]] std::array<std::uint8_t, 3> EffectGridPlaneTintFor(std::uint8_t mark);

// LES SOIXANTE CARREAUX D'UNE PISTE, ou un lot VIDE.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles que le mouvement a rangés
// dans les styles des deux premières pistes : la figure les y relit, comme toute
// la voie A.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeGridPlaneQuads(const EffectTrack& track,
                                                                 std::int16_t turn_degrees,
                                                                 std::int16_t tilt_degrees,
                                                                 glm::vec3 node_origin,
                                                                 const EffectOwnerTint& owner = {});

// LA NAPPE PEINTE À L'ÉCRAN — ce que ce client fait de la brume du poison.
//
// 🔴 C'EST UN CHOIX, CELUI DU VOILE DE TÉNÈBRES (`effect_dark_veil.h`). Posée
// dans le monde, la nappe suit le zoom : dézoomée, elle ne couvre plus l'écran
// et la scène reparaît nette autour d'elle ; et, redressée vers l'œil au pied
// du joueur, le sol et le décor la coupent. Ce client garde donc l'aspect du
// zoom par défaut QUEL QUE SOIT le zoom : chaque carreau a la taille en pixels
// qu'il aurait au recul du jeu, le carrelage se cale sur la place d'écran du
// porteur comme celui du jeu, et il se PROLONGE jusqu'aux bords — au zoom par
// défaut, les soixante carreaux du jeu couvrent déjà l'écran. Mêmes image,
// teinte, alpha et mélange que la nappe du monde.
//
// Un porteur sans place d'écran cale le carrelage au centre. Rend un lot VIDE
// pour une piste éteinte ou sans alpha.
[[nodiscard]] std::vector<EffectScreenQuad> ComposeScreenGridPlaneQuads(
    const EffectTrack& track, std::optional<glm::vec2> bearer_pixel, float viewport_width,
    float viewport_height);

}  // namespace ro::render
