#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"

namespace ro::render {

// LA SORTIE LA PLUS EMPRUNTÉE DE LA FAMILLE PROCÉDURALE : le quad projeté.
//
// Quatre-vingts dessinateurs l'appellent, sur trois cent cinquante-huit sites,
// et ils portent à eux seuls cinq cent quarante-cinq nœuds sans image. Ce
// n'est pas une figure
// mais une SORTIE : elle ne décide ni de la forme ni du mouvement, seulement de
// la façon dont quatre points du monde deviennent deux triangles teintés.
//
// ⚠ PORTER LA SORTIE N'EST PAS PORTER LES FIGURES. Chaque dessinateur bâtit ses
// quatre coins à sa manière ; ce fichier ne porte que ce qu'ils ont en commun.
//
// Ce que le client fait ici et que nous laissons au renderer : la projection de
// chaque sommet, le test de découpe par triangle, et le budget de cinq cent dix
// triangles par file. Notre `EffectWorldQuad` garde ses coins dans le monde,
// et c'est le renderer qui projette — voir `effect_node_world.h`.
//
// ⚠ LA VARIANTE DE PROFONDEUR A QUATRE VALEURS : garder la profondeur projetée,
// l'aplatir pour peindre devant tout, l'avancer d'un souffle, ou recentrer le
// quad à l'écran avant de l'aplatir. La plupart des sites prennent la première.
// La demande la porte jusqu'au quad, et c'est le renderer qui l'applique — voir
// `EffectQuadDepth`.

// Ce qu'un champ d'écrasement porte quand le propriétaire n'écrase RIEN.
//
// Moins un, et non zéro : zéro est un canal noir parfaitement légitime, et
// c'est bien pourquoi le client a choisi une autre valeur pour dire l'absence.
inline constexpr std::int32_t kEffectOwnerTintAbsent = -1;

// LA TEINTE QUE LE PROPRIÉTAIRE D'UN NŒUD IMPOSE À SES QUADS.
//
// L'effet porte quatre nombres que chacun de ses nœuds consulte au moment de
// sortir un quad, et chacun écrase INDÉPENDAMMENT ce que le dessinateur avait
// choisi. C'est par là qu'un même effet se rejoue en rouge ou en bleu sans
// qu'aucun dessinateur ne connaisse la variante.
struct EffectOwnerTint {
  std::int32_t red = kEffectOwnerTintAbsent;
  std::int32_t green = kEffectOwnerTintAbsent;
  std::int32_t blue = kEffectOwnerTintAbsent;
  // ⚠ N'ÉCRASE QUE LE FACTEUR DE DESTINATION, et seulement quand le mode n'en
  // impose pas un. La source, elle, reste au mode.
  std::int32_t blend_destination = kEffectOwnerTintAbsent;
};

// Un couple de facteurs de mélange, dans la numérotation de Direct3D.
struct EffectBlendPair {
  std::int32_t source = 0;
  std::int32_t destination = 0;
};

// LE MÉLANGE QU'UN MODE CHOISIT, l'écrasement du propriétaire compris.
//
// Trois règles, et la troisième est celle qui sert :
//
//   - le mode dix peint SANS mélange — un sur un, la destination est effacée ;
//   - le mode quatre-vingt-dix-neuf force le mélange ordinaire, et ignore alors
//     ce que le propriétaire aurait imposé ;
//   - tout autre mode mélange par l'alpha de la source, et sa destination est
//     celle du propriétaire quand il en pose une, sinon l'ADDITION pour le mode
//     zéro et le mélange ordinaire pour les autres.
//
// ⚠ LE MODE ZÉRO N'EST PAS « PAS DE MODE » : c'est l'addition, et c'est le plus
// fréquent — deux cent cinquante-six sites sur trois cent cinquante-huit. Le
// prendre pour une absence peindrait un carré noir autour de chaque éclair.
[[nodiscard]] EffectBlendPair EffectParticleBlend(std::int16_t blend_mode,
                                                  std::int32_t owner_destination);

// UN POINT DU REPÈRE DU CLIENT, ramené au nôtre : `S = diag(1, −1, −1)`.
//
// 🔴 LES COINS D'UNE DEMANDE SONT DANS NOTRE REPÈRE, et le client calcule les
// siens dans le sien, où la hauteur compte vers le BAS. Une hauteur qu'il
// écrit négative élève la figure ; recopiée telle quelle, elle l'enterre, et
// l'épreuve de profondeur l'efface sans un mot. Un dessinateur qui reprend les
// formules du client les calcule donc dans son repère, puis passe chaque coin
// par ici avant de le demander.
[[nodiscard]] constexpr glm::vec3 EffectPointFromClient(glm::vec3 client) {
  return {client.x, -client.y, -client.z};
}

// CE QU'UN DESSINATEUR DE CETTE VOIE DEMANDE.
struct EffectParticleQuadRequest {
  // Les quatre coins, dans l'ordre où le client les passe : A, B, C puis D. Le
  // client en fait deux triangles, ABC et DCA.
  std::array<glm::vec3, 4> corners = {};

  // ⚠ RELATIFS À L'ORIGINE DU NŒUD PAR DÉFAUT, et c'est le cas le plus fréquent
  // — deux cent soixante-quatorze sites sur trois cent cinquante-huit. Les
  // traiter comme absolus poserait toute la famille à l'origine du monde.
  bool absolute = false;

  std::uint8_t alpha = 255;
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  std::int16_t blend_mode = 0;
  // Le numéro de la texture dans la liste du nœud.
  std::size_t texture = 0;

  // LES QUATRE NOMBRES DE TEXTURE, nommés par les coins qui les reçoivent.
  //
  // ⚠ CE N'EST PAS UN RECTANGLE ORIENTÉ : rien ne garantit que le premier soit
  // plus petit que le second, et un dessinateur RETOURNE son image en les
  // échangeant. Les ranger en minimum et maximum effacerait ce retournement.
  //
  // 🔴 PAR DÉFAUT, LE BAS DE L'IMAGE VA AUX DEUX PREMIERS COINS : v = 1 sur A et
  // B, v = 0 sur C et D, ce que passent les dessinateurs du client presque sans
  // exception. L'inverse retournait l'image de toutes les figures qui s'en
  // remettent au défaut.
  float u_at_a_and_d = 0.0F;
  float u_at_b_and_c = 1.0F;
  float v_at_a_and_b = 1.0F;
  float v_at_c_and_d = 0.0F;

  // LA VARIANTE DE PROFONDEUR, dernier paramètre de la sortie : voir
  // `EffectQuadDepth`. Le dessinateur la choisit par marque ou par style.
  EffectQuadDepth depth = EffectQuadDepth::kProjected;
};

// LE QUAD qu'une demande donne, prêt pour le renderer.
//
// `node_origin` est l'origine du nœud dans le monde. Elle n'entre que si les
// coins sont relatifs.
[[nodiscard]] EffectWorldQuad ComposeEffectParticleQuad(const EffectParticleQuadRequest& request,
                                                        glm::vec3 node_origin,
                                                        const EffectOwnerTint& owner = {});

}  // namespace ro::render
