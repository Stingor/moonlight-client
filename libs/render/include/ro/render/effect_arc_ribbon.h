#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ARC DE RUBAN : un quad posé sur un ARC de cercle, empilé autant de fois que
// sa marque le demande.
//
// Le plus gros dessinateur d'effet du client, et celui qui ouvre le plus :
// trente-six identifiants par le sous-type 82.
//
// On le dirait « ruban coloré », et c'est à moitié vrai. Ce n'est pas un ruban
// mais un QUAD dont les quatre coins se posent sur un même cercle, à des angles
// que la piste choisit ; et la couleur n'est pas une, mais une PILE de couleurs
// que l'addition mêle.

// UNE COUCHE : une couleur, et le mélange par lequel elle se pose.
struct EffectArcRibbonLayer {
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  std::int16_t blend_mode = 0;
};

// CE QU'UNE MARQUE DEMANDE.
//
// ⚠ LE NOMBRE DE COUCHES EST LE PROPOS. Le client enchaîne ses cas SANS
// rupture : la marque 15 pose une couche, la 12 en pose deux, la 2 en pose six,
// et chacune ajoute la sienne par-dessus toutes les suivantes. C'est ainsi
// qu'une même figure va du discret à l'éclatant sans qu'aucun alpha ne change.
// Lire ce switch comme un choix d'une couleur parmi six donnerait six figures
// pâles au lieu d'une progression.
struct EffectArcRibbonStyle {
  std::span<const EffectArcRibbonLayer> layers;
  // ⚠ DEUX MARQUES SUR TRENTE-TROIS GARDENT L'ORDRE DES COINS ; toutes les
  // autres échangent les deux paires. Le quad reste le même, mais sa texture s'y
  // pose retournée — et un quad retourné n'est pas une figure ratée, c'est une
  // autre figure.
  bool swaps_corners = true;
  // LA SEULE MARQUE QUI SE RECENTRE À L'ÉCRAN sur l'origine du nœud, puis passe
  // devant tout. Voir `EffectQuadDepth::kScreenRecentred`.
  bool recentres_depth = false;
};

// Ce que la marque demande, ou une pile VIDE. Une pile vide est une réponse :
// dix-neuf marques sur trente-trois ne dessinent rien du tout, et poser un quad
// blanc à leur place montrerait ce que le client cache.
[[nodiscard]] EffectArcRibbonStyle EffectArcRibbonStyleFor(std::uint8_t mark);

// LES QUATRE COINS SUR LEUR ARC, et ce que la piste y décide.
//
// Le rayon RESPIRE : il s'écarte de lui-même au rythme de l'angle propre que le
// mouvement fait tourner, de cinq centièmes. C'est ce qui donne à la figure son
// battement, et le retirer la fige sans rien casser d'autre.
//
// Les quatre coins se posent sur ce cercle, séparés par trois écarts que la
// piste choisit parmi trois jeux : un carré, un cerf-volant, et un cerf-volant
// aplati. Le quatrième côté est donc un ARC plus large que les trois autres —
// c'est de là que vient le nom, et c'est ce qu'un quad régulier ne rendrait pas.
//
// Rend une liste VIDE quand la piste est éteinte, quand son alpha est nul, ou
// quand sa marque ne demande aucune couche.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeArcRibbonQuads(
    const EffectTrack& track, std::int16_t billboard_turn_degrees,
    std::int16_t billboard_tilt_degrees, glm::vec3 node_origin = glm::vec3{0.0F, 0.0F, 0.0F});

}  // namespace ro::render
