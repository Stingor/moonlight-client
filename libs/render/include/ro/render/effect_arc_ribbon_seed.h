#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ARC DE RUBAN, et les trente-six effets qui l'emploient.
//
// Un seul paramètre décide de tout : un STYLE, que dix-huit valeurs parcourent et qui
// choisit le rayon, l'angle de départ, la parure et la hauteur.
//
// ⚠ CE BÂTISSEUR N'ALLUME QU'UNE PISTE SUR QUATRE. Il pose la première et ÉTEINT
// les trois autres, explicitement, à chaque style. Les quatre autres bâtisseurs
// de la famille sèment leurs quatre pistes ; celui-ci non, et le croire
// montrerait quatre arcs superposés là où le client n'en montre qu'un.

// LA CASE où le bâtisseur range l'élévation de la piste au-dessus de son
// instance. Le mouvement la relit à chaque tic pour reposer l'ancre de la piste
// sur l'instance, qui a pu suivre son porteur.
inline constexpr std::size_t kArcRibbonSlotOfHeightGap = 15;

// CE QU'UN STYLE SÈME dans l'unique piste vive du nœud.
struct EffectArcRibbonStyleSeed {
  std::uint8_t style = 0;
  // Le rayon où les quatre coins se posent, quand l'instance ne porte aucune
  // échelle.
  float radius_at_anchor = 0.0F;
  // ⚠ NEUF STYLES SUR DIX-HUIT PRÉFÈRENT L'ÉCHELLE DE LEUR INSTANCE au rayon
  // ci-dessus, et ce facteur est ce par quoi ils la multiplient. À zéro, le
  // style ne regarde pas l'instance et son rayon est le seul qu'il ait.
  //
  // 🔴 CETTE ÉCHELLE EST CELLE DE L'INSTANCE, ET C'EST LE PREMIER DES QUATRE
  // PARAMÈTRES D'UN OBJET D'EFFET DU RSW — un semeur de carte les pose par le
  // message 46. Le client n'emploie le rayon constant ci-dessus que si elle
  // n'est PAS POSITIVE, ce qui est le cas de tout effet lancé par une
  // compétence. Les halos de minerai de manuk, eux, déclarent 8, 20, 25 ou 30 :
  // les peindre tous au rayon constant de 30 les rend jusqu'à près de quatre
  // fois trop gros.
  float radius_per_instance_scale = 0.0F;
  // L'angle du premier coin sur son cercle. Neuf styles le TIRENT AU SORT sur
  // le tour entier au lieu de le poser.
  std::int16_t degrees = 0;
  bool degrees_drawn = false;
  // La marque que la FIGURE lit pour choisir sa pile de couleurs : ce n'est pas
  // le style, et neuf styles sur dix-huit en portent une autre que la leur.
  std::uint8_t draw_mark = 0;
  // La marque que le MOUVEMENT lit : le style, sauf le 4 qui se donne celle du
  // 1. Deux lecteurs, deux marques — voir ADR 0017.
  std::uint8_t style_mark = 0;
  // ⚠ DE COMBIEN LA PISTE SE TIENT AU-DESSUS du point de l'effet, et le nom dit
  // le SENS et non le signe. Le client l'écrit en RETRANCHANT trente à la
  // hauteur de son effet — mais son repère DESCEND, si bien qu'une soustraction
  // ÉLÈVE. Recopier le signe au lieu du sens enfoncerait l'arc sous le sol.
  float lift_above_effect = 0.0F;
  // ⚠ UN STYLE PREND SA HAUTEUR SUR LE SPRITE DE L'ACTEUR, pas sur une
  // constante : le 22 — la marque de la brûlure — la lit dans le premier
  // calque du corps. Voir `ArcRibbonLiftFromBody`.
  bool lift_from_actor_height = false;
  // Le jeu d'écarts angulaires entre les quatre coins : carré, cerf-volant ou
  // cerf-volant aplati. Voir `effect_arc_ribbon.h`.
  float shape_key = 0.0F;
};

// Tous les styles connus, par valeur croissante.
[[nodiscard]] std::span<const EffectArcRibbonStyleSeed> EffectArcRibbonStyleSeedTable();

// Ce qu'un style sème, ou `nullptr` quand aucun site d'appel ne l'emploie. Un
// `nullptr` est une réponse : le bâtisseur porte des branches que personne
// n'appelle, et leur inventer un semis montrerait ce que le client ne montre pas.
[[nodiscard]] const EffectArcRibbonStyleSeed* EffectArcRibbonStyleSeedFor(std::uint8_t style);

// UN ARC QU'UN EFFET SÈME : ce que son site d'appel passe au bâtisseur.
//
// ⚠ UN EFFET EN SÈME PARFOIS QUATRE. Le 681 appelle le bâtisseur quatre fois, à
// quatre styles et quatre textures, et les quatre arcs se répondent. N'en poser
// qu'un montrerait le quart de la figure sans que rien ne le dise.
struct EffectArcRibbonNodePattern {
  std::uint32_t effect_id = 0;
  // Le nom de la fonction du client qui pose cet arc : elle trace, d'une figure
  // qui se montre mal jusqu'au code qui la décrit.
  std::string_view client_function;
  // Le chemin complet de la texture dans l'archive.
  std::string_view texture;
  std::uint8_t style = 0;
};

// Tous les arcs connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectArcRibbonNodePattern> EffectArcRibbonNodePatternTable();

// TOUS les arcs d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectArcRibbonNodePattern> EffectArcRibbonNodePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN NŒUD D'ARC, dont une seule est vive.
//
// `anchor` est le point de l'effet dans NOTRE repère, celui qui monte. Le semis
// y ajoute l'écart du style. `instance_scale` est l'échelle de l'instance : zéro pour
// tout effet qui n'en pose pas, et le style prend alors son rayon constant.
// LA HAUTEUR D'UN ARC QUI SE LÈVE SUR LE SPRITE DE SON PORTEUR, en unités du
// monde au-dessus du point de l'effet.
//
// 🔴 LE CLIENT LA LIT DANS LE CORPS : l'ordonnée EN MÉMOIRE du premier calque
// de la première image de la première action — celle du fichier moins la
// demi-hauteur de son image —, passée en unités à 0,29, et relevée de quatre.
// Sans corps lisible, ou pour une ordonnée nulle, trente. La marque de la
// brûlure s'y pose, au-dessus de la tête ; sans elle, elle tombait aux pieds,
// sur le sprite, comme une icône.
inline constexpr float kArcRibbonLiftWithoutBody = 30.0F;
[[nodiscard]] float ArcRibbonLiftFromBody(std::optional<float> body_first_layer_y);

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectArcRibbonTracks(
    const EffectArcRibbonStyleSeed& seed, glm::vec3 anchor, std::uint32_t& generator,
    float instance_scale = 0.0F, float body_lift = kArcRibbonLiftWithoutBody);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
//
// ⚠ LA VIE VIENT DE L'INSTANCE, pas du bâtisseur. Celui-ci recopie dans le nœud
// la vie de l'effet qui l'appelle, sans jamais la poser ni la borner — c'est le
// seul de la famille à faire ainsi. L'appelant doit donc la donner, et passer le
// défaut d'un nœud quand il ne la connaît pas.
[[nodiscard]] PooledEffectNode SowEffectArcRibbonNode(const EffectArcRibbonNodePattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks,
                                                      std::uint32_t& generator,
                                                      float instance_scale = 0.0F,
                                                      float body_lift = kArcRibbonLiftWithoutBody);

// 🔴 L'ARC SE REPOSE SUR SON INSTANCE À CHAQUE TIC, à son écart près : le nœud
// reprend la place de l'instance, et l'ancre de la piste vive la sienne plus
// l'élévation que le semis a rangée. Semé une fois, l'arc d'un état resterait
// au sol quand le personnage marche.
void FollowArcRibbonOwner(PooledEffectNode& pooled);

}  // namespace ro::render
