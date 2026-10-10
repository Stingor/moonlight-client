#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES RÈGLES DU SERVEUR : `ZC_MOONLIGHT_SERVER_RULES 0x0F38`.
//
//     0f38 <len>.W { <clé>.W <valeur>.L }*
//
// Des paires clé / valeur, rangées dans `sim::SessionSettings`. Le serveur les
// envoie dès que le client annonce savoir les lire (`kUiCapServerRules`), donc à
// chaque connexion à un serveur de carte, puis de nouveau à tous les clients qui
// l'ont annoncé quand le staff recharge sa configuration de combat
// (`@reloadbattleconf`).
//
// ⚠ UNE CLÉ INCONNUE EST IGNORÉE, et c'est ce qui laisse le serveur en ajouter
// sans casser ce client. Une longueur qui ne tombe pas sur un nombre entier de
// paires, ou qui contredit la trame, rend la trame indécodable : rien n'en est
// retenu. Une valeur hors de son domaine (un rayon nul, un niveau nul, un délai
// ou un seuil négatif, une taxe hors de 0 à 10 000 dix-millièmes) est ignorée,
// et le défaut ou la valeur précédente reste.
enum class ServerRuleKey : std::uint16_t {
  kViewCells = 1,
  kHomunculusMaxLevel = 2,
  kHomunculusSMaxLevel = 3,
  kHomunculusTeleportHoldMs = 4,
  kVendingTaxPermyriad = 5,
  kVendingTaxMinZeny = 6,
};

[[nodiscard]] FrameOutcome ApplyServerRulesFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
