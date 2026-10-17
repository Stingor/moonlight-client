#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// L'HABIT DU GM D'UN JOUEUR : `ZC_MOONLIGHT_GM_OUTFIT 0x0F44`.
//
//     0f44 <len>.W <joueur>.L <porté>.B
//
// Le serveur ne l'envoie qu'à qui a annoncé savoir le dessiner — voir
// `kUiCapGmOutfit` —, après l'apparition d'un joueur qui le porte, et à toute la
// zone quand il le prend ou le quitte. L'absence vaut « habit du métier ». Un
// paquet qui nomme un acteur inconnu est lu et ignoré : voir
// `sim::World::TellGmOutfit`.
[[nodiscard]] FrameOutcome ApplyGmOutfitFrame(const Frame& frame, sim::World& world);

// PORTER OU QUITTER L'HABIT : `CZ_MOONLIGHT_GM_OUTFIT 0x0F43`.
//
//     0f43 <len>.W <porté>.B
//
// 🔴 LE SERVEUR TRANCHE : il ne l'accepte que d'un administrateur, retient le
// choix avec le personnage, et le redit à la zone.
[[nodiscard]] core::Result<void> SendGmOutfit(Connection& link, bool worn);

}  // namespace ro::net
