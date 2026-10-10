#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/sim/actor.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/entity_bars.h"

namespace moonclient {

// ── CE QUI SE PEINT AU-DESSUS DES ENTITÉS : LEUR VIE, LEUR MAGIE ────────────
//
// 🔴 RÉCOLTÉ ICI, PEINT PAR L'ÉTAGE D'INTERFACE, comme les bulles de parole. Le
// dessin est en ImGui parce que ces barres écrivent parfois leurs nombres, et
// que notre atlas de rendu est gravé en niveaux de gris : une jauge chiffrée
// veut la police du chat, pas celle des noms.
//
// 🔴 ET C'EST ICI QUE SE FAIT LA TRADUCTION DU MONDE VERS UNE ESPÈCE. `ui` ne
// sait pas ce qu'est un membre de groupe ; elle reçoit un rang d'onglet. Même
// frontière que `NameColorOf` pour la couleur d'une étiquette.

// L'ESPÈCE SOUS LAQUELLE SE RÈGLE CET ACTEUR-LÀ.
//
// 🔴 « DANS MON GROUPE » SE LIT DANS LA LISTE DU GROUPE, ET NON DANS LA VIE
// CONNUE. `ZC_NOTIFY_HP_TO_GROUPM 0x080E` porte la vie d'un autre joueur, mais
// le serveur l'envoie à deux publics : les membres du groupe, et le staff qui a
// le droit de voir la vie de tous (`clif_hpmeter`, dans `src/map/clif.cpp`).
// Chez ce dernier, « vie connue » vaudrait « du groupe » pour toute la zone.
// Sur un champ de bataille, un coéquipier se règle comme le groupe dès que son
// camp est dit.
//
// ⚠ ET LE JOUEUR PASSE AVANT TOUT : ma propre barre se règle à part, sans quoi
// elle hériterait des réglages faits pour les autres.
[[nodiscard]] ro::ui::EntityBarKind EntityBarKindOf(const ro::sim::Actor& actor,
                                                    const ro::sim::World& world,
                                                    const ro::sim::PartyRoster& party);

// VRAI SI CET ACTEUR NE PORTE AUCUNE BARRE, quel que soit le réglage de son
// espèce : un marchand derrière son échoppe, quand le joueur les fait taire.
//
// ⚠ JAMAIS MOI : ma barre se règle sur son onglet à elle, échoppe ouverte ou
// non.
[[nodiscard]] bool HidesEntityBarsOf(const ro::sim::Actor& actor, const ro::sim::World& world,
                                     const ro::ui::EntityBarsLook& look);

// CE QUE LES BARRES D'ÉCRAN MONTRENT, pour tous les acteurs dessinés.
//
// `hovered_id` est ce que la souris désigne, zéro sinon ; la cible vient du
// monde. Les deux ne servent qu'aux réglages « au survol » et « la cible » —
// le filtre lui-même est dans `ro::ui::ShowsEntityBar`.
//
// ⚠ LA MAGIE N'A QUE DEUX SOURCES, et c'est le protocole qui le veut : la
// MIENNE, que le serveur m'envoie en clair, et celle de la CIBLE, par le paquet
// privé `ZC 0x0F2A`. Aucun paquet du jeu ne porte la magie d'une entité tierce
// — ni pour un monstre, ni pour un compagnon de groupe. Une barre de magie
// allumée ailleurs ne paraît donc jamais, et c'est la vérité plutôt qu'une
// barre vide.
//
// ⚠ L'ÉCHELLE DU ZOOM EST PRISE ICI, une fois pour toute la carte, et posée sur
// chaque vue : voir `ro::render::ApparentScale`.

// LA HAUTEUR QUE LES BARRES DE CET ACTEUR RÉSERVENT AU-DESSUS DE SA TÊTE, en
// pixels — zéro quand elles se posent sous ses pieds, ou qu'elles sont éteintes.
//
// 🔴 CE QUI SE POSE PLUS HAUT DOIT SAVOIR CE QUI EST EN DESSOUS. L'enseigne d'un
// vendeur visait le même bord que la jauge, et les deux se chevauchaient.
//
// ⚠ ELLE RÉPOND D'APRÈS LE RÉGLAGE SEUL, sans le filtre d'affichage — pas de
// survol, pas de seuil d'entame, pas de valeur connue. C'est volontaire : cette
// réponse est demandée AVANT que l'on sache ce que la souris désigne, et une
// enseigne qui monte de cinq pixels de trop ne se voit pas, là où une enseigne
// posée SUR une jauge se voit tout de suite.
[[nodiscard]] float EntityBarsAbove(const ro::sim::Actor& actor, const ro::sim::World& world,
                                    const ro::sim::PartyRoster& party,
                                    const ro::ui::EntityBarsLook& look, float anchor_scale);

[[nodiscard]] std::vector<ro::ui::EntityBarView> EntityBarsOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::sim::PartyRoster& party, std::uint32_t hovered_id,
    float viewport_width, float viewport_height);

}  // namespace moonclient
