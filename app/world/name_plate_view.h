#pragma once

#include <array>
#include <cstdint>

#include "ro/render/entity_names.h"
#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// ── CE QUI TRADUIT LE MONDE EN RÉGLAGE D'ÉTIQUETTE ──────────────────────────
//
// 🔴 EN BIBLIOTHÈQUE À PART POUR LA RAISON DES BARRES D'ENTITÉ : ces deux
// règles — sous quelle espèce se range un acteur, et de quelle couleur s'écrit
// son nom — doivent se vérifier sans fenêtre, sans carte et sans serveur.
// Laissées dans la boucle de dessin, personne ne les vérifie.
//
// ⚠ `render` NE SAIT PAS CE QU'EST UN BOSS : il reçoit un rang d'onglet et une
// couleur. C'est ici, et nulle part ailleurs, que le monde devient l'un et
// l'autre.

// CET ACTEUR PORTE-T-IL UNE ÉTIQUETTE, TOUT COURT ?
//
// 🔴 UN PORTAIL N'EN A JAMAIS, ET C'EST UNE GARDE DU CLIENT, pas un goût :
// le client compare le métier d'affichage à **45** et sort AVANT de construire
// quoi que ce soit. Le serveur, lui, nomme
// ses warps — « ggprowarp » — et ce nom n'a jamais été fait pour être lu.
//
// ⚠ ELLE PASSE DEVANT TOUT RÉGLAGE : aucun onglet ne doit pouvoir faire
// apparaître ces noms-là, parce qu'ils ne veulent rien dire pour le joueur.
[[nodiscard]] bool HasNamePlate(const ro::sim::Actor& actor);

// L'ESPÈCE SOUS LAQUELLE SE RÈGLE L'ÉTIQUETTE DE CET ACTEUR-LÀ.
[[nodiscard]] ro::render::NamePlateKind NamePlateKindOf(const ro::sim::Actor& actor,
                                                        const ro::sim::World& world);

// SON NOM SE COUPE-T-IL AU CROISILLON ?
//
// 🔴 TOUT CE QUI N'EST PAS UN JOUEUR, et c'est l'inverse de l'intuition. Le
// client coupe le nom quand le métier n'est PAS celui d'un joueur : la coupe
// est acquise d'entrée, et seul un métier de joueur la retire.
//
// ⚠ ET CELA CONCERNE SEPT MILLE CENT QUARANTE ET UN PNJ de ce serveur, dont le
// nom de script porte un croisillon — « Fisk#a2t », « Kafra Employee#alde ».
// Sans cette coupe, le joueur lit le nom UNIQUE que l'auteur du script a écrit
// pour lui-même.
[[nodiscard]] bool NameCutAtHash(const ro::sim::Actor& actor);

// LA COULEUR DE SON NOM, une fois l'écart de niveau appliqué.
//
// 🔴 L'ÉCART DE NIVEAU NE VAUT QUE SUR UN MONSTRE, et cette fonction le refuse
// ailleurs plutôt que de s'en remettre au réglage. Le panneau ne propose la case
// que sous les deux onglets qui la méritent — voir `entity_name_fields.h` —,
// mais le fichier de réglages s'ouvre dans un éditeur de texte, et un drapeau
// posé à la main sur les joueurs peindrait en rouge tout ce qui nous dépasse.
//
// ⚠ NOUS TRANCHONS PAR L'ESPÈCE, LÀ OÙ LE CLIENT TRANCHE PAR LE MÉTIER.
// Le client peint en rose clair tout métier
// de 1001 à 3998 ; le serveur, lui, nous dit l'espèce en clair dans l'octet de
// type de chaque paquet d'unité. Deviner par une plage de métiers ce qu'on nous
// a dit en toutes lettres serait reprendre un détour dont nous n'avons pas
// besoin.
[[nodiscard]] std::array<std::uint8_t, 4> NameColorOf(const ro::sim::Actor& actor,
                                                      const ro::render::EntityNameLook& how,
                                                      const ro::sim::World& world);

}  // namespace moonclient
