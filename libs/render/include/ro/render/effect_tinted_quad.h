#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE CARRÉ TEINTÉ QUI TOURNE : quatre coins à quatre-vingt-dix degrés l'un de
// l'autre sur un même cercle, tournés vers la caméra et teintés par une marque.
//
// Le dessinateur qui ouvre le plus après l'arc : TREIZE sous-types et
// soixante-huit identifiants. C'est le frère de l'arc de ruban — même boucle
// sur quatre pistes, même respiration du rayon, même rotation, même sortie —,
// et il en diffère par trois choses seulement.
//
// La première : ses quatre coins sont RÉGULIERS. L'arc choisit ses écarts dans
// trois jeux et laisse un côté plus large que les autres ; celui-ci pose
// simplement un carré, chaque coin quatre-vingt-dix degrés après le précédent.
//
// La deuxième : sa marque ne choisit PAS une pile de couches mais UNE couleur,
// et un seul quad sort. Là où l'arc empile jusqu'à six couches pour aller du
// discret à l'éclatant, celui-ci change de teinte.
//
// La troisième est la plus facile à manquer, et c'est pour cela qu'elle est
// dans la signature.

// ⚠ LA MARQUE VIENT DE LA PREMIÈRE PISTE, PAS DE CELLE QU'ON DESSINE. Le client
// lit ces deux marques à une place FIXE du nœud, que la boucle des quatre pistes
// ne décale pas, alors que tout le reste se lit dans la piste courante. Les
// quatre pistes d'un nœud partagent donc leur teinte et leur ordre de coins.
// Les lire piste par piste donnerait quatre couleurs là où le client en montre
// une, et c'est exactement le genre d'écart qu'on ne voit pas sur une capture.
struct EffectTintedQuadStyle {
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  // Les coins passent tels quels, ou par paires échangées — la texture s'y pose
  // alors retournée. Trois marques sur vingt — zéro, un et quinze — gardent
  // l'ordre d'origine.
  bool swaps_corners = true;
  // 🔴 LE MÉLANGE : ORDINAIRE, ou ADDITIF quand c'est faux. C'est le SEIZIÈME
  // paramètre du traceur de quads : zéro y choisit le facteur UN de Direct3D,
  // toute autre valeur le mélange ordinaire. Il varie d'une marque à l'autre —
  // un pour la marque zéro, zéro pour la marque un.
  //
  // ⚠ CE N'EST PAS « ABSOLU », bien que les deux se ressemblent dans la table.
  // Le paramètre qui décide d'ajouter l'origine du nœud est le DIX-HUITIÈME,
  // et toutes les marques qui dessinent le poussent à UN : aucune
  // ne l'ajoute.
  bool ordinary_blend = false;
  // Une seule marque n'emploie que la MOITIÉ de l'alpha de la piste.
  bool halves_alpha = false;
  // ⚠ DE COMBIEN LE CARRÉ MONTE au-dessus de son ancre, et le nom compte. Le
  // client l'écrit en RETRANCHANT cent dix à l'ordonnée des quatre coins — mais
  // son repère DESCEND, si bien qu'une soustraction y élève. Recopier le signe
  // au lieu du sens enfoncerait la figure de deux cent vingt unités.
  float lift_above_anchor = 0.0F;
  // FAUX quand la marque ne dessine rien. C'est une réponse : le switch du
  // client retombe sur un `default` muet, et poser un carré blanc à sa place
  // montrerait ce qu'il cache.
  bool draws = false;
};

// Ce que la marque demande, la sous-marque servant à la seule marque qui s'en
// sert. Une marque inconnue rend un style qui ne dessine pas.
[[nodiscard]] EffectTintedQuadStyle EffectTintedQuadStyleFor(std::uint8_t mark,
                                                             std::uint8_t sub_mark);

// LE CARRÉ D'UNE PISTE, ou rien.
//
// Rend `std::nullopt` quand la piste est éteinte, quand son alpha est nul, ou
// quand la marque ne dessine pas.
//
// ⚠ L'ANCRE DE LA PISTE EST UN POINT DU MONDE, et c'est la seule origine : le
// client pousse un au dix-huitième paramètre de son traceur pour toutes ses
// marques, si bien que la position du nœud n'entre jamais dans les coins. Chaque
// semis doit donc poser une ancre déjà dans le monde ; un écart relatif
// poserait le carré au coin de la carte.
//
// ⚠ LE RAYON RESPIRE, comme celui de l'arc : il s'écarte de lui-même au rythme
// de l'angle propre de la piste, de cinq centièmes. Les deux dessinateurs
// partagent ce battement au chiffre près.
//
// ⚠ ET UN ANGLE PROPRE HORS DU TOUR VAUT ZÉRO, il ne se ramène pas. Le client
// remet la case à zéro au lieu d'en prendre le reste : une piste dont le
// mouvement a débordé cesse de respirer au lieu de repartir au milieu.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeTintedRotatedQuad(
    const EffectTrack& track, std::uint8_t mark, std::uint8_t sub_mark,
    std::int16_t billboard_turn_degrees, std::int16_t billboard_tilt_degrees);

// LES FORMES QUE CETTE FIGURE DESSINE : le carré teinté et ses dix cousins, qui
// n'en diffèrent que par le mouvement. Ce sont celles que l'application compose
// par `ComposeTintedRotatedQuad`, une à une ou par `ComposeTintedQuadNodeShape`.
//
// ⚠ LA LISTE EST CELLE DU CLIENT, pas celle de nos fichiers : le répartiteur de
// dessin mène treize sous-types à ce dessinateur, et onze sont portés. Une
// forme oubliée ici échappe au plafond d'`ecl_fild01` sans que rien ne le dise.
[[nodiscard]] bool DrawnAsTintedQuad(PooledNodeShape shape) noexcept;

// LE PLAFOND D'ALPHA de la carte qui le demande — voir
// `EffectScatterWorld::caps_tinted_quad_alpha`.
inline constexpr std::int16_t kTintedQuadAlphaCapOnThatMap = 100;

// RAMÈNE AU PLAFOND toute piste allumée qui le dépasse. Le client le fait en
// dessinant, et ÉCRIT la valeur dans la piste : le mouvement suivant repart
// donc du plafond. Une piste qui monte à deux cent cinquante puis s'éteint de
// cinq par tic s'éteint ainsi en vingt tics au lieu de cinquante.
void CapTintedQuadAlpha(std::array<EffectTrack, kEffectTracksPerNode>& tracks) noexcept;

}  // namespace ro::render
