#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES PISTES : ce qu'un bâtisseur pose dans un nœud à sa naissance.
//
// Une piste est un état à trois mains — le bâtisseur la sème, le mouvement la
// fait vivre, le dessinateur la lit (voir `effect_track.h`). Ce fichier porte la
// première, et c'est elle qui manquait : sans semis, le mouvement fait croître
// un rayon nul et la figure n'a rien à montrer.
//
// ⚠ LE BÂTISSEUR EST LE PATRON, PAS L'IDENTIFIANT. Les vingt-trois identifiants
// du sous-type 30 passent par le même bâtisseur et ne se distinguent que par
// deux nombres : un alpha de départ et un STYLE. Il n'y a donc pas vingt-trois
// patrons à porter, mais un seul et vingt-trois couples d'arguments — ceux de
// ses sites d'appel.

// Ce qu'un numéro de style choisit dans le bâtisseur des anneaux à quatre
// pistes. Tout le reste du semis est écrit en dur et vaut pour tous les styles.
struct EffectRingStyle {
  std::uint8_t style = 0;

  // Le numéro que la primitive de ruban lit dans le nœud pour y prendre sa
  // couleur — voir `effect_trail_palette.h`. Le style ne le choisit pas par une
  // formule : vingt et une branches le posent, et le reste tombe au défaut.
  std::uint8_t trail_variant = 0;

  // La clé de décalage de chaque piste, dans l'ordre des pistes.
  //
  // ⚠ CE N'EST PAS UNE HAUTEUR : la figure la compare à deux flottants exacts,
  // et aucune des douze valeurs du bâtisseur n'en est un — l'anneau du
  // sous-type 30 ne se soulève jamais. Le client range d'ailleurs 24.0 à côté
  // de la clé 24.000999, ce qui ne serait pas une précaution utile si la valeur
  // était lue comme une mesure.
  std::array<float, kEffectTracksPerNode> lift_key = {};
};

// Ce que le bâtisseur laisse en place quand aucune branche ne nomme le style.
[[nodiscard]] EffectRingStyle DefaultEffectRingStyle();

// La table engendrée, triée par numéro de style.
[[nodiscard]] std::span<const EffectRingStyle> EffectRingStyleTable();

// Le choix d'un style, ou le défaut du bâtisseur si aucune branche ne le nomme.
[[nodiscard]] EffectRingStyle EffectRingStyleFor(std::uint8_t style);

// LE SEMIS DES QUATRE PISTES D'UN ANNEAU.
//
// Le bâtisseur du sous-type 30. Les quatre pistes naissent allumées, sur le
// tour entier, au même
// rayon et à la même inclinaison ; elles ne diffèrent que par leur azimut, leur
// alpha et leur clé.
//
// `start_alpha` est le premier argument du bâtisseur. C'est un ALPHA et non un
// angle : il se pose en tête de chaque piste, et la primitive de ruban en fait
// l'octet de transparence de sa couleur.
//
// ⚠ UNE VIE COURTE ÉTEINT LE SEMIS. Quand l'instance ne vit pas plus que le
// seuil du bâtisseur, les quatre pistes naissent à alpha nul et avec un retard
// échelonné — un compteur NÉGATIF, que le mouvement laisse remonter avant de
// faire quoi que ce soit. Sans cette règle les quatre pistes partiraient
// ensemble et à pleine teinte, ce qui se voit.
//
// Les longueurs de segment naissent nulles : c'est le mouvement qui donne au
// ruban son aire.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRingTracks(
    std::int16_t start_alpha, std::uint8_t style, std::int32_t effect_life_ticks);

// LE PLANCHER DE VIE que le bâtisseur impose à son instance, à son premier
// tic : une vie plus courte est relevée à ce nombre, une plus longue reste.
//
// ⚠ IL EST SOUS LE SEUIL QUI ALLUME LE SEMIS, mais la vie réelle peut le
// dépasser — celle que la table donne à l'effet, ou la durée de l'incantation
// qu'il accompagne. C'est elle, relevée, qui décide si les pistes naissent
// éteintes et échelonnées.
inline constexpr std::int32_t kEffectRingInstanceLifeTicks = 56;

// LA VIE DE L'INSTANCE après le plancher du bâtisseur.
[[nodiscard]] constexpr std::int32_t EffectRingInstanceLifeTicks(std::int32_t instance_life_ticks) {
  return instance_life_ticks < kEffectRingInstanceLifeTicks ? kEffectRingInstanceLifeTicks
                                                            : instance_life_ticks;
}

// UN ANNEAU QU'UN EFFET SÈME : ce que son site d'appel passe au bâtisseur.
//
// ⚠ UN EFFET EN SÈME SOUVENT DEUX. Vingt et un des vingt-trois identifiants
// appellent le bâtisseur deux fois, à deux alphas de départ — et l'effet 1556 à
// deux styles, donc deux teintes. N'en poser qu'un montrerait la moitié de la
// figure sans que rien ne le dise.
struct EffectRingNodePattern {
  std::uint32_t effect_id = 0;
  // LE NOM DU BÂTISSEUR qui pose cet anneau chez le client. Il TRACE, comme
  // celui d'un patron de nœud : d'une figure qui se montre mal on remonte à la
  // description de son bâtisseur.
  std::string_view client_function;
  // Le chemin complet de la texture dans l'archive.
  std::string_view texture;
  // Le premier argument du bâtisseur, d'où les quatre pistes tirent le leur.
  std::int16_t start_alpha = 0;
  std::uint8_t style = 0;
};

// Tous les anneaux connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectRingNodePattern> EffectRingNodePatternTable();

// TOUS les anneaux d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectRingNodePattern> EffectRingNodePatternsFor(
    std::uint32_t effect_id);

// LE NŒUD qu'un anneau décrit, pistes semées, prêt à rejoindre la troupe.
//
// Ici plutôt que chez l'appelant, et pour la même raison qu'`EffectNodeFromPattern`
// : la table entière se parcourt au banc, et l'on vérifie que chaque rangée
// donne un nœud qui finit par montrer quelque chose. Laissée dans la fenêtre,
// cette traduction ne se vérifierait qu'à l'œil.
//
// Le nœud ne porte AUCUNE image de figure : ses quads viennent de ses pistes, et
// la texture qu'il emporte ne sert qu'à les peindre.
//
// `instance_life_ticks` est la vie de l'instance AVANT le plancher du
// bâtisseur, que ce semis applique.
[[nodiscard]] PooledEffectNode SowEffectRingNode(const EffectRingNodePattern& pattern,
                                                 glm::vec3 ground_position,
                                                 std::int32_t instance_life_ticks);

}  // namespace ro::render
