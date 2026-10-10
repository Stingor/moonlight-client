#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ÉVENTAIL : DIX carrés emboîtés, du plus serré au plus large, et chacun a son
// propre éclat.
//
// C'est la figure du sous-type 147 — les compteurs de rotation, et deux effets de prêtre.
//
// ⚠ CE N'EST PAS UN QUAD RÉPÉTÉ DIX FOIS. Les dix lames partagent leur centre et
// leurs angles, mais chacune tire son alpha d'une case différente de la piste —
// que le mouvement fait monter à dix plafonds différents — et s'écarte du centre
// d'un pas qui lui est propre. C'est ce double dégradé, en éclat et en rayon,
// qui fait l'éventail ; dessiner un seul carré à l'alpha de la piste donnerait
// une plaque.
//
// ⚠ LA LAME LA PLUS BRILLANTE EST LA PLUS SERRÉE. Le client parcourt ses lames à
// rebours : la case la plus haute — celle que le mouvement pousse le plus haut —
// est celle du carré le plus petit. L'éventail est donc clair au centre et
// s'efface vers le bord, et inverser le parcours le retournerait.
//
// ⚠ LE CLIENT SE SERT DE LA PISTE COMME D'UN BROUILLON. Il range ses quatre
// coins dans l'ancre et la queue de la piste qu'il dessine, et les relit au
// tour suivant après les avoir multipliés. Rien n'en survit d'une image à
// l'autre — tout est recalculé depuis le rayon —, si bien que cette figure ne
// touche pas la piste. Prendre ces champs pour un état entretenu ferait chercher
// un mouvement qui les pose, et il n'y en a pas.

// CE QU'UN STYLE DEMANDE À L'ÉVENTAIL.
//
// ⚠ ICI, TOUT STYLE DESSINE, et c'est ce qui sépare cette figure de ses deux
// sœurs. L'arc de ruban et le carré teinté ont un `default` muet — dix-neuf
// marques sur trente-trois ne montrent rien —, si bien que la question « cette
// marque dessine-t-elle ? » s'y pose. Le `switch` de l'éventail, lui, n'a pas de
// branche muette : tout ce qu'il ne nomme pas retombe sur le carré blanc
// ordinaire. Lui ajouter un cas muet par symétrie ferait disparaître des lames
// que le client montre.
struct EffectSpiralFanStyle {
  // ⚠ LES QUATRE ANGLES NE FONT PAS TOUJOURS UN CARRÉ. Huit styles sur dix
  // posent leurs coins tous les quatre-vingt-dix degrés ; les deux qui
  // s'ouvrent à la fin les resserrent de dix degrés vers l'horizontale, ce qui
  // aplatit la lame. Les régulariser rendrait ces deux-là indiscernables des
  // autres.
  std::array<std::int32_t, 4> corner_degrees = {45, 135, 225, 315};

  // DE COMBIEN CHAQUE LAME S'ÉCARTE de la précédente. Trois styles avancent
  // d'une unité, les autres de trois : l'éventail est serré ou étalé, et c'est
  // la seule chose qui en règle l'envergure.
  float radius_step_per_blade = 3.0F;

  // ⚠ DE COMBIEN L'ÉVENTAIL MONTE au-dessus de l'origine du nœud, et le nom
  // compte. Le client l'écrit en RETRANCHANT ce nombre à l'ordonnée des quatre
  // coins — mais son repère DESCEND, si bien qu'une soustraction y élève.
  // Recopier le signe au lieu du sens enfoncerait l'éventail sous le sol.
  //
  // ⚠ ET IL S'APPLIQUE APRÈS LA ROTATION, contrairement au carré teinté. C'est
  // une élévation dans le MONDE, pas un coin posé plus haut : l'appliquer avant
  // la ferait basculer avec la figure et la rendrait à l'horizontale dès que la
  // caméra se couche.
  float lift_above_anchor = 30.0F;

  std::array<std::uint8_t, 3> color = {255, 255, 255};
  std::int16_t blend_mode = 1;

  // LE STYLE QUI DOUBLE SA LAME LA PLUS SERRÉE, à demi-alpha et par-dessous.
  // Un seul le fait, et c'est ce qui lui donne son cœur plus dense.
  bool doubles_innermost = false;

  // L'AURA : un second carré un dixième plus grand, en mélange additif, posé
  // par-dessus chaque lame. Trois styles la portent, et c'est elle qui donne à
  // l'éventail son halo coloré.
  bool has_aura = false;
  std::array<std::uint8_t, 3> aura_color = {5, 5, 255};
};

// Ce que le style de la piste demande. Un style qu'aucun `case` ne nomme rend la
// parure ordinaire — voir l'avertissement ci-dessus.
[[nodiscard]] EffectSpiralFanStyle EffectSpiralFanStyleFor(std::uint8_t style);

// LES QUADS D'UNE PISTE, dans l'ordre où le client les soumet.
//
// Rend une liste VIDE quand la piste est éteinte, ou quand aucune de ses dix
// lames n'a d'éclat.
//
// ⚠ UNE LAME SANS ÉCLAT NE SORT PAS, et c'est la FERMETURE que cela dessine.
// Les dix lames s'allument au même tic — le mouvement les fait monter ensemble —
// mais elles retombent de plafonds différents, si bien que l'éventail se défait
// lame par lame, du bord vers le centre. Soumettre les dix jusqu'au bout
// laisserait dix carrés noirs empilés là où le client n'en montre plus qu'un.
//
// ⚠ ET L'ORDRE EST CELUI DU CLIENT : la lame serrée d'abord, son aura juste
// après. Ces quads se mélangent à la destination, et les réordonner change
// l'image.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpiralFanQuads(
    const EffectTrack& track, std::int16_t billboard_turn_degrees,
    std::int16_t billboard_tilt_degrees, glm::vec3 node_origin);

}  // namespace ro::render
