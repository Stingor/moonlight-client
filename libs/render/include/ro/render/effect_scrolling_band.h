#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA BANDE QUI DÉFILE : cinq quads bout à bout qui glissent le long de leur axe
// sans jamais montrer de couture.
//
// C'est le dessinateur du sous-type 79, et il sort par la voie A, deux fois.
// Treize identifiants l'emploient, et tous peignent de l'eau qui
// tombe : c'est la cascade du client.
//
// ⚠ IL N'Y A AUCUN FONDU (ADR 0017). Le premier et
// le dernier quad de la bande sont RACCOURCIS, et leur image coupée d'autant :
// géométrie et texture reculent ensemble, si bien que la bande paraît glisser
// d'un bloc alors que ses cinq morceaux ne bougent pas. Prendre ce raccourci
// pour une transparence ferait clignoter les deux bouts au lieu de les faire
// couler.
//
// Ce que la figure fait, dans l'ordre :
//
//   - la piste porte une DURÉE DE CYCLE ; le compteur de tics, ramené dans ce
//     cycle, donne une phase qui parcourt une longueur de segment ;
//   - les six jointures de la bande se lisent dans les cases de la piste, et la
//     phase les décale toutes ensemble ;
//   - le premier et le dernier quad sont rognés de la fraction de phase, et
//     leur image avec ;
//   - l'image de chaque quad est prise dans un jeu de TROIS, et le jeu tourne
//     d'un cran à chaque cycle — c'est cela qui referme la boucle.

// COMBIEN DE QUADS la bande compte, et combien de jointures il faut pour les
// tenir. Le client déroule cinq fois sa boucle et lit une case de plus que de
// quads : les six jointures sont les bords, pas les morceaux.
inline constexpr std::size_t kScrollingBandSegments = 5;
inline constexpr std::size_t kScrollingBandJoints = kScrollingBandSegments + 1;

// COMBIEN D'IMAGES le jeu compte. Le client en alloue trois, les indexe modulo
// trois, et fait tourner l'index d'un cran par cycle.
inline constexpr std::size_t kScrollingBandTextures = 3;

// SUR QUELLE LONGUEUR la phase court avant de revenir à son début.
//
// Quarante, et deux mesures indépendantes tombent dessus : le dessinateur
// divise le cycle en quarante, et les deux bâtisseurs espacent leurs jointures
// de quarante. C'est ce qui fait que la bande se recolle exactement sur
// elle-même — un écart entre les deux montrerait une saccade par cycle.
inline constexpr float kScrollingBandPhaseSpan = 40.0F;

// ⚠ LE CLIENT MULTIPLIE, IL NE DIVISE PAS, et cette constante n'est pas
// l'inverse exact de la précédente en flottant. Remplacer la multiplication par
// une division par quarante décalerait le rognage des bouts d'un ulp à chaque
// tic — invisible sur une capture, et faux.
inline constexpr float kScrollingBandPhaseToFraction = 0.025F;

// AU-DELÀ DE QUOI la marque de tassement tasse. Le client compare à dix et ne
// tasse que STRICTEMENT au-dessus : un bâtisseur qui y écrit dix ne tasse rien,
// et c'est le cas de l'un des deux.
inline constexpr std::uint8_t kScrollingBandSqueezeFloor = 10;

// LES TROIS MARQUES QUE CETTE FIGURE LIT, et leur sens lui appartient.
//
// ⚠ ELLES SE LISENT PISTE PAR PISTE, à la différence du carré teinté qui va
// chercher les siennes dans la première. Le client décale bien ces trois-là de
// cent quatre-vingts octets par piste : les quatre rideaux d'une cascade ont
// donc chacun son plan, son style et son tassement.
inline constexpr std::size_t kMarkOfBandPlane = 0;
inline constexpr std::size_t kMarkOfBandStyle = 1;
inline constexpr std::size_t kMarkOfBandSqueeze = 2;

// LA MARQUE DE PLAN qui fait basculer la bande d'un quart de tour.
inline constexpr std::uint8_t kScrollingBandPlaneTurned = 1;

// CE QU'UN STYLE DONNE À LA BANDE.
//
// Trois cas, et le client les teste deux fois sous la même condition : une fois
// pour le sens de la phase, une fois pour celui de l'index d'image. Les deux
// vont donc toujours ensemble, et un seul champ les porte.
struct EffectScrollingBandStyle {
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  // Dans la numérotation du seizième paramètre de la voie A, pas dans celle de
  // Direct3D : zéro y veut dire ADDITION. Voir `EffectParticleBlend`.
  std::int16_t blend_mode = 1;
  // ⚠ LA BANDE REMONTE, et l'index d'image tourne à rebours avec elle. Les deux
  // styles qui l'emploient sont exactement ceux qui teintent.
  bool reversed = false;
};

// Ce que le style d'une piste demande. Tout style hors des deux connus rend le
// cas ordinaire : blanc, mélange par l'alpha, bande qui descend.
[[nodiscard]] EffectScrollingBandStyle EffectScrollingBandStyleFor(std::uint8_t style);

// LES QUADS D'UNE PISTE, dans l'ordre où le client les émet.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha est nul : c'est
// la garde du client, et elle porte sur les deux.
//
// ⚠ UNE DURÉE DE CYCLE NULLE NE DÉGÉNÈRE PAS LA BANDE, elle la fige : le client
// se rabat alors sur UN SEUL quad et met la phase au bout de sa course. Le seul
// identifiant dans ce cas est aussi le seul dont le style renverse la phase, si
// bien que la phase retombe à zéro et que le quad garde toute son aire. Les deux
// mesures se recoupent sans rien savoir l'une de l'autre.
//
// ⚠ LES COINS SONT RELATIFS À L'ORIGINE DU NŒUD : le client pousse zéro au
// dix-huitième paramètre de sa primitive. L'ancre de la PISTE n'entre que par sa
// profondeur, et c'est par là que les quatre rideaux d'une cascade s'échelonnent
// sans se battre pour le même plan.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeScrollingBandQuads(
    const EffectTrack& track, glm::vec3 node_origin, const EffectOwnerTint& owner = {});

}  // namespace ro::render
