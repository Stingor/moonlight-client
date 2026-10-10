#pragma once

#include <cstdint>
#include <span>

#include "ro/render/effect_node_fwd.h"

namespace ro::render {

// COMBIEN DE TICS UN NŒUD VIT, quand c'est l'EFFET qui le lui dit.
//
// ⚠ IL N'Y A PAS UNE BONNE RÉPONSE POUR TOUS, et c'est pour cela que cette
// table existe plutôt qu'une constante. Le client a trois espèces de
// bâtisseurs (`client_effect_builder_life.json`) :
//
//   copie → il écrit dans son nœud la DURÉE DE L'EFFET, qu'il lit dans la
//           table des durées du client (`client_effect_durations.json`).
//           190 bâtisseurs.
//   rien  → il ne touche pas à la vie du nœud, et le défaut du constructeur
//           tient — `kEffectNodeDefaultLifeTicks`. 33 bâtisseurs.
//   pose  → il y écrit une constante, que le PATRON de la figure porte déjà.
//           78 bâtisseurs.
//
// Cette table porte la PREMIÈRE espèce — et, quelle que soit l'espèce, tout
// identifiant dont le bâtisseur ÉCRIT LUI-MÊME la vie de son instance —, et
// `EffectNodeLifeFor` rend le défaut pour tout le reste : juste pour « rien »,
// sans effet pour « pose », dont les semis ne l'interrogent pas.
//
// 🔴 CE QUE LE BÂTISSEUR ÉCRIT DANS SON INSTANCE GAGNE SUR LA TABLE DU CLIENT.
// Trente bâtisseurs réécrivent la vie de leur instance à leur premier tic — là
// même où la durée de la table est entrée à la création —, en la posant ou en
// la relevant à un plancher, et c'est cette valeur-là que copient les nœuds qui
// copient. Le triple anneau de Heal écrit cent là où la table dit cinquante :
// borner sur cinquante coupait sa colonne à mi-course. Voir
// `scripts/vie_de_l_instance.py` et `client_effect_instance_life.json`.
//
// ⚠ CINQ IDENTIFIANTS ONT DEUX BÂTISSEURS D'ESPÈCES DIFFÉRENTES, et la table
// les ÉCARTE plutôt que de choisir pour eux : 248, 618, 669 et 718 mêlent copie
// et rien, 786 mêle copie et pose. Un nœud mal daté ne se voit pas et se
// cherche longtemps ; mieux vaut le défaut, qui est au moins explicable.
//
// ⚠ DONNER LA DURÉE DE L'EFFET À TOUS SERAIT FAUX, et la mesure l'a montré
// avant qu'on l'écrive : les identifiants que le client laisse au défaut y
// auraient changé de vie sans que rien ne le dise. Voir ADR 0042.
struct EffectLifeEntry {
  std::uint32_t effect_id = 0;
  std::int32_t life_ticks = 0;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectLifeEntry> EffectLifeTable();

// La vie à donner au nœud de cet effet.
//
// Rend `kEffectNodeDefaultLifeTicks` pour un identifiant que la table ne porte
// pas — voir plus haut pourquoi c'est la bonne réponse et non un repli.
[[nodiscard]] std::int32_t EffectNodeLifeFor(std::uint32_t effect_id);

// LA VIE DE CET EFFET EST-ELLE CONNUE, ou rend-on le défaut du constructeur ?
//
// 🔴 LA DIFFÉRENCE DÉCIDE DE CE QU'ON A LE DROIT DE BORNER. Un nœud ne survit pas
// à son effet — le client détruit l'instance et ses enfants ensemble —, et c'est
// ce qui fait que les rayons d'EF_ENHANCE s'éteignent AVEC leur colonne. Mais
// borner contre un DÉFAUT reviendrait à couper tous les nœuds des six cent
// soixante-seize effets dont la durée n'est pas connue, sur un nombre qui ne
// veut rien dire.
[[nodiscard]] bool EffectNodeLifeIsKnown(std::uint32_t effect_id);

// LES INSTANCES QUI NE FONT QUE BORNER, triées par identifiant : celles que leurs
// bâtisseurs laissent à la durée de la table du client, alors qu'ils posent la
// vie de leurs nœuds ou n'y touchent pas.
//
// 🔴 UN NŒUD EST BORNÉ PAR LA VIE DE SON INSTANCE, QUELLE QUE SOIT SON ESPÈCE.
// Sans elles, les plans croisés que BD_ADAPTATION sème dans la seconde moitié de
// son effet lui survivaient d'une demi-vie, là où le client les fait naître
// invisibles.
//
// ⚠ À PART, parce qu'elles ne se DONNENT pas : `EffectNodeLifeFor` ne les lit
// jamais, et un nœud « rien » garde le défaut du constructeur. Voir
// `VIES_D_INSTANCE_LAISSEES_A_LA_TABLE`.
[[nodiscard]] std::span<const EffectLifeEntry> EffectInstanceOnlyLifeTable();

// LA VIE DE L'INSTANCE DE CET EFFET EST-ELLE CONNUE ? Celle de la table des vies,
// ou celle d'une instance qui ne fait que borner. C'est la question que posent
// les bornes : un nœud ne survit pas à son effet.
[[nodiscard]] bool EffectInstanceLifeIsKnown(std::uint32_t effect_id);

// La vie de l'instance de cet effet, quand elle est connue ; le défaut du
// constructeur sinon, que l'appelant ne doit pas prendre pour une borne.
[[nodiscard]] std::int32_t EffectInstanceLifeFor(std::uint32_t effect_id);

// 🔴 LA VIE QUE REÇOIT L'INSTANCE D'UN EFFET D'INCANTATION : le client lui pose
// la durée de l'incantation, en tics, avant son premier passage, puis son
// bâtisseur la relève à son plancher et la recopie dans ses nœuds. Elle ne se
// relance donc pas : elle dure l'incantation d'un seul tenant. Zéro pour un
// effet dont le bâtisseur n'est pas de ceux-là — la vie de la table vaut, et la
// relance aussi.
[[nodiscard]] std::int32_t EffectLifeUnderCast(std::uint32_t effect_id, std::uint32_t cast_ms);

}  // namespace ro::render
