#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LE SERVEUR SAIT DE L'ENTITÉ DÉSIGNÉE.
//
// Deux couples, tous deux réservés aux clients ayant donné leur poignée de main
// (ADR 0040) :
//
//   · `CZ 0x0F29` / `ZC 0x0F2A` — vie, magie, niveau, race, élément, taille ;
//   · `CZ 0x0F2C` / `ZC 0x0F2D` — la liste COMPLÈTE de ses états.
//
// 🔴 AUCUN ABONNEMENT : c'est le client qui REDEMANDE. Le serveur ne garde rien
// — pas de minuterie à nettoyer, pas de fuite à la déconnexion, et une fenêtre
// fermée ne coûte rien. La contrepartie est que le dossier est toujours la
// DERNIÈRE réponse, jamais un flux.
//
// 🔴 ET LA PORTE PVP EST DU JEU, PAS UN DÉFAUT. Sur un autre joueur, vie et
// magie ne partent que s'il est de mon groupe, de ma guilde, ou moi-même. Un
// adversaire renvoie son espèce et rien d'autre : le masque `known` le dit, et
// `sim` le porte en champs facultatifs. Ne pas « compléter » ce silence.

[[nodiscard]] FrameOutcome ApplyTargetInfoFrame(const Frame& frame, sim::World& world);

// ⚠ `now_ms` EST L'HORLOGE DE L'APPELANT, comme partout ailleurs : le paquet
// porte un RESTE, le monde garde une ÉCHÉANCE. Le monde ne lit aucune horloge,
// ce qui est ce qui permet de le rejouer à l'identique.
[[nodiscard]] FrameOutcome ApplyTargetStatusFrame(const Frame& frame, sim::World& world,
                                                  std::uint32_t now_ms);

[[nodiscard]] core::Result<void> RequestTargetInfo(Connection& link, std::uint32_t gid);
[[nodiscard]] core::Result<void> RequestTargetStatuses(Connection& link, std::uint32_t gid);

// ── CE QUE LA STRUCTURE ENGENDRÉE NE PORTE PAS ──────────────────────────────
//
// 🔴 LE GÉNÉRATEUR S'ARRÊTE AU BLOC FIXE, comme pour `0x0F10`. `ZC 0x0F2D`
// déclare un `count` et pas ses entrées : la liste est écrite par le CODE du
// serveur, et aucune structure ne la décrit. Les décalages se nomment donc ici.
//
// ⚠ ET L'ENTRÉE EST `#pragma pack(1)` : `{ efst:u16, remain:u32, total:u32 }`
// fait DIX octets et non douze. L'aligner naturellement décalerait toute la
// liste, en silence et dès la seconde entrée.
inline constexpr std::size_t kStatusListFixedSize = 10;
inline constexpr std::size_t kStatusListEntrySize = 10;

// Le plafond d'entrées que le serveur s'impose. Un joueur
// très chargé dépasse rarement la vingtaine ; au-delà il TRONQUE plutôt que de
// faire enfler un paquet qui part en rafale.
inline constexpr std::size_t kStatusListMostEntries = 40;

// Les bits de `known` : ce que le serveur a RENSEIGNÉ.
inline constexpr std::uint8_t kTargetKnowsHealth = 1;
inline constexpr std::uint8_t kTargetKnowsMagic = 2;
inline constexpr std::uint8_t kTargetKnowsLevel = 4;
inline constexpr std::uint8_t kTargetKnowsNature = 8;

// `status` de la réponse : zéro est la seule valeur qui décrive une entité.
inline constexpr std::uint8_t kTargetAnswerOk = 0;

}  // namespace ro::net
