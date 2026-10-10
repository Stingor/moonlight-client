#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/actor.h"
#include "ro/sim/status_effects.h"

namespace ro::sim {

// CE QUI CHANGE LA MOTION D'UN ACTEUR hors de tout geste : un arrêt que le
// serveur impose, un sommeil, un réveil. Le monde note l'instant ; c'est à qui
// dessine d'en tirer la pose et le repos.

// 🔴 L'ARRÊT QUE LE SERVEUR IMPOSE CHANGE LA MOTION, à cet instant : le client
// fait glisser l'acteur jusqu'à la case annoncée, par un pas de marche de
// quelques millisecondes, puis le rend au repos. Qui marchait encore, ou qui
// n'était pas sur cette case, glisse : son repos est celui qu'on prend EN
// QUITTANT LA MARCHE — l'attente, jamais la garde. Qui s'y tenait déjà ne
// glisse pas, et se repose tel qu'il était. À appeler AVANT de le reposer.
void NoteStop(Actor& actor, CellPosition where, std::uint32_t stopped_ms);

// 🔴 L'ENDORMISSEMENT ET LE RÉVEIL, que deux causes provoquent — l'état du
// corps et le sommeil profond — et qui ne se cumulent pas : le premier endort,
// avec ses bulles, tant que l'acteur ne dort pas déjà ; le premier qui cesse
// réveille, et rend à l'acteur sa posture d'avant. Le repos qu'il y prend est
// celui qu'on prend EN QUITTANT LE SOMMEIL — l'attente, jamais la garde.
void NoteSleep(Actor& actor, bool asleep, std::uint32_t now_ms);

// Pose l'état du corps, et note l'instant quand il change. Y entrer endormi ou
// en sortir passe par `NoteSleep`.
void NoteBodyState(Actor& actor, std::uint16_t body_state, std::uint32_t now_ms);

// Le sommeil que la pose ou la levée d'un état apporte : `NoteSleep` pour le
// sommeil profond, rien pour tout autre état.
void NoteStatusSleep(Actor& actor, StatusEffectId effect, bool active, std::uint32_t now_ms);

// LE SILENCE ET LA MALÉDICTION, lus sur l'état de santé déjà posé : chacun note
// l'instant où il commence — c'est de là que ses points de suspension, sa
// marque s'animent — et l'oublie quand il cesse.
void NoteHealthMarks(Actor& actor, std::uint32_t now_ms);

// 🔴 UNE RÉANNONCE N'EST PAS UN CHANGEMENT D'ÉTAT. Le serveur redit l'unité à
// chaque pas, et la fiche neuve partirait de comptes nuls : un lecteur qui
// retient le compte verrait un front qui n'a jamais eu lieu, ou en manquerait
// un vrai. Les fronts d'options, le compte des changements et le début du
// silence qui tient encore passent de la fiche connue à l'annoncée.
//
// ⚠ ET L'INSTANT DE L'ÉTAT DU CORPS QUI TIENT ENCORE, avec le sommeil qu'il
// porte : l'annonce le daterait de son arrivée, et les étoiles d'un étourdi, le
// bloc d'un gelé ou les bulles d'un dormeur repartiraient de leur première
// image à chaque redite.
void KeepStateEdges(Actor& announced, const Actor& known);

// DEPUIS QUAND L'ÉTAT DU CORPS `body_state` TIENT L'ACTEUR, ou rien quand il ne
// le tient pas. C'est de cet instant que s'anime ce que l'état fait paraître
// sur lui : le bloc de glace d'un gelé, les étoiles d'un étourdi.
[[nodiscard]] std::optional<std::uint32_t> BodyStateHeldSince(const Actor& actor,
                                                              std::uint16_t body_state);

}  // namespace ro::sim
