#pragma once

#include <optional>

#include "ro/sim/companion_ai.h"
#include "ro/sim/session_settings.h"

namespace ro::sim {

// LES RÉGLAGES TELS QUE L'IA LES APPLIQUE, sous les règles que le serveur a
// dites. Rien si elles n'y changent rien — le cas courant, qui ne copie pas.
//
// 🔴 LES DISTANCES S'ARRÊTENT AU RAYON DE VUE : suivi, éloignements maximum et
// rayons d'engagement. Au-delà, le compagnon disparaît du client et aucune
// cible n'y est connue : une distance plus grande ne commanderait rien qu'on
// puisse voir. Le profil, lui, garde la valeur du joueur.
//
// ⚠ LE DÉLAI D'ARRIVÉE D'USINE SUIT CELUI DU SERVEUR : un réglage resté à sa
// valeur d'usine prend le délai que le serveur impose après la téléportation de
// l'homoncule. Un délai que le joueur a choisi autre reste le sien.
[[nodiscard]] std::optional<CompanionAiSettings> SettingsUnderServerRules(
    const CompanionAiSettings& settings, const SessionSettings& session);

}  // namespace ro::sim
