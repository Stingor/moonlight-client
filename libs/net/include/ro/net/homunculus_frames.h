#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/homunculus.h"

namespace ro::net {

// ── LES TRAMES DE L'HOMONCULE ───────────────────────────────────────────────
//
//     0230 changement d'état : l'accusé (0), l'intimité (1), la satiété (2)
//     0BA4 la fiche entière          0BA5 l'expérience, les PV ou les SP
//     0235 la liste des compétences  0239 une compétence qui change
//     022F l'accusé d'un nourrissage 02D9 l'écho du réglage d'auto-alimentation

// CE QU'UNE TRAME DEMANDE D'ÉCRIRE DANS LA CONVERSATION. Le texte est l'affaire
// d'`app`, qui a les noms d'objets et les traductions.
enum class HomunculusEchoKind : std::uint8_t {
  kNone,
  // La satiété vient d'arriver à un palier d'alerte.
  kStarving,
  // Nourri : `item_id` est la nourriture prise dans le sac.
  kFed,
  // Pas nourri : le sac n'a pas de `item_id`.
  kNotFed,
  kAutoFeedOn,
  kAutoFeedOff,
};

struct HomunculusEcho {
  HomunculusEchoKind kind = HomunculusEchoKind::kNone;
  std::uint32_t item_id = 0;
};

// Lit une trame ; `kUnhandled` si elle ne regarde pas l'homoncule.
//
// ⚠ L'ÉCHO DE RÉGLAGE EN PORTE D'AUTRES — l'équipement montré, le pet — : seul
// celui de l'auto-alimentation de l'homoncule est pris ici.
[[nodiscard]] FrameOutcome ApplyHomunculusFrame(const Frame& frame, sim::Homunculus& homunculus,
                                                HomunculusEcho& echo);

// Nourrir, et abandonner. ⚠ ÉMETTENT ET S'ARRÊTENT LÀ : le serveur répond, ou
// se tait quand l'homoncule n'est pas actif.
[[nodiscard]] core::Result<void> RequestHomunculusFeed(Connection& link);
[[nodiscard]] core::Result<void> RequestHomunculusDelete(Connection& link);

// Le nouveau nom, en UTF-8. ⚠ C'EST UN NOM, NON UNE PHRASE : il part dans le
// jeu de caractères du jeu, comme celui d'un personnage.
[[nodiscard]] core::Result<void> RequestHomunculusRename(Connection& link, std::string_view name);

// L'auto-alimentation. ⚠ LA CASE NE CHANGE QU'À L'ÉCHO : le serveur peut la
// refuser, et il se tait alors.
[[nodiscard]] core::Result<void> RequestHomunculusAutoFeed(Connection& link, bool on);

}  // namespace ro::net
