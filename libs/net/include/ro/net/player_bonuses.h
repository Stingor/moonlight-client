#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE DÉTAIL DE CE QUE L'ÉQUIPEMENT APPORTE, traduit dans le monde.
//
// `ZC_MOONLIGHT_STAT_BONUS 0x0F10`, qui n'arrive qu'à un client ayant donné sa
// poignée de main (ADR 0040). Même contrat qu'`ApplyFrame` : rien n'est inventé,
// et `sim` ne voit aucun octet de protocole.
[[nodiscard]] FrameOutcome ApplyStatBonusFrame(const Frame& frame, sim::World& world);

// ── CE QUE LA STRUCTURE ENGENDRÉE NE PORTE PAS ──────────────────────────────
//
// 🔴 LE GÉNÉRATEUR S'ARRÊTE AU BLOC FIXE, et c'est normal : la déclaration du
// serveur s'arrête là aussi. Ce qui suit — trois listes, chacune précédée de son
// compte — est écrit par son CODE, à coups de `reinterpret_cast` dans un tampon,
// et aucune structure ne le décrit. Les décalages se nomment donc ici.
//
// ⚠ ET LES TROIS LISTES SONT `#pragma pack(1)` : le serveur les écrit sous le
// `pack(push, 1)` qui couvre tout `packets_struct.hpp`. Une entrée de compétence
// fait DOUZE octets et non seize — l'aligner naturellement décalerait tout ce
// qui la suit, en silence et dès la première entrée.

// Le bloc fixe, où commence la première liste.
inline constexpr std::size_t kStatBonusFixedSize = 224;

// Le compte qui précède chaque liste.
inline constexpr std::size_t kStatBonusCountSize = 2;

// Une entrée de chaque liste, en octets sur le fil.
inline constexpr std::size_t kConditionalBonusSize = 8;
inline constexpr std::size_t kSkillBonusSize = 12;
inline constexpr std::size_t kItemBonusSize = 10;

}  // namespace ro::net
