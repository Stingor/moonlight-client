// Les onze bits de la propriété de carte, traduits en `ro::sim::MapMode`.
// Voir l'en-tête pour la raison qui sépare ce paquet des faits d'acteur.

#include "ro/net/map_mode.h"

#include <cstddef>

#include "ro/proto/map_packets.h"
#include "ro/sim/map_mode.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

using sim::MapKind;
using sim::MapMode;

// ── LES ONZE RANGS DE BIT ───────────────────────────────────────────────────
//
// 🔴 CES RANGS SONT DU FIL, PAS DU MONDE, et c'est pourquoi ils vivent ici et
// non dans `MapMode`. Le serveur compose son mot en clair — onze décalages
// écrits à la main dans `clif_map_property` — et sa propre structure nommée,
// `packet_maptypeproperty2`, n'est employée nulle part : le générateur ne peut
// donc rien en tirer. Les rangs se recopient une fois, ici, et le monde n'en
// sait rien.
//
// ⚠ L'ORDRE EST CELUI DU SERVEUR, ET IL NE SE DEVINE PAS. Un décalage d'un seul
// rang échangerait « le curseur d'attaque paraît » contre « l'emblème de guilde
// paraît » sans qu'aucun contrôle ne le voie. C'est `tests/net/map_mode_test.cpp`
// qui l'attrape, en levant chaque bit SEUL.
constexpr std::uint32_t kMapBitAttackableBeyondParty = 0;
constexpr std::uint32_t kMapBitAttackableBeyondGuild = 1;
constexpr std::uint32_t kMapBitGuildEmblems = 2;
constexpr std::uint32_t kMapBitSimpleEffects = 3;
constexpr std::uint32_t kMapBitAttackableWithoutShift = 4;
constexpr std::uint32_t kMapBitPvpCounter = 5;
constexpr std::uint32_t kMapBitPartyChangesRefused = 6;
constexpr std::uint32_t kMapBitBattlefield = 7;
constexpr std::uint32_t kMapBitCostumesHidden = 8;
constexpr std::uint32_t kMapBitCartAllowed = 9;
constexpr std::uint32_t kMapBitSunMoonStarMiracle = 10;

// La dernière valeur que `enum map_property` nomme. Au-delà, le serveur a
// inventé quelque chose que nous ne savons pas lire.
constexpr std::int16_t kLastNamedMapKind = 6;

[[nodiscard]] bool MapBitSet(std::uint32_t flags, std::uint32_t rank) {
  return (flags >> rank & 1U) != 0U;
}

// ⚠ LE GÉNÉRATEUR REND QUATRE OCTETS ET NON UN ENTIER, parce que le serveur
// déclare ce champ comme un tableau. Ils sont dans l'ordre du fil, donc du plus
// faible au plus fort.
[[nodiscard]] MapMode MapModeFrom(const proto::PacketMaptypeproperty2& packet) {
  std::uint32_t flags = 0;
  for (std::size_t index = 0; index < packet.flag.size(); ++index) {
    flags |= static_cast<std::uint32_t>(packet.flag[index]) << (index * 8U);
  }
  MapMode mode;
  // Un type inconnu se lit « carte ordinaire » : c'est le seul défaut qui ne
  // promette rien. Les drapeaux, eux, restent lus — ils ne dépendent pas de lui.
  if (packet.type >= 0 && packet.type <= kLastNamedMapKind) {
    mode.kind = static_cast<MapKind>(packet.type);
  }
  mode.attackable_beyond_party = MapBitSet(flags, kMapBitAttackableBeyondParty);
  mode.attackable_beyond_guild = MapBitSet(flags, kMapBitAttackableBeyondGuild);
  mode.guild_emblems_shown = MapBitSet(flags, kMapBitGuildEmblems);
  mode.simple_effects_forced = MapBitSet(flags, kMapBitSimpleEffects);
  mode.attackable_without_shift = MapBitSet(flags, kMapBitAttackableWithoutShift);
  mode.pvp_counter_shown = MapBitSet(flags, kMapBitPvpCounter);
  mode.party_changes_refused = MapBitSet(flags, kMapBitPartyChangesRefused);
  mode.battlefield = MapBitSet(flags, kMapBitBattlefield);
  mode.costumes_hidden = MapBitSet(flags, kMapBitCostumesHidden);
  mode.cart_allowed = MapBitSet(flags, kMapBitCartAllowed);
  mode.sun_moon_star_miracle_allowed = MapBitSet(flags, kMapBitSunMoonStarMiracle);
  return mode;
}

}  // namespace

FrameOutcome ApplyMapModeFrame(const Frame& frame, sim::World& world) {
  // LE TYPE DE CARTE, l'autre déclaration : voir `sim::PartySeesConcealedEffects`.
  if (frame.opcode == proto::ZcNotifyMapproperty2::kOpcode) {
    return Decoded<proto::ZcNotifyMapproperty2>(frame, [&world](const auto& packet) {
      world.TellMapType(static_cast<std::uint16_t>(packet.type));
      return FrameOutcome::kApplied;
    });
  }
  if (frame.opcode != proto::PacketMaptypeproperty2::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::PacketMaptypeproperty2>(frame, [&world](const auto& packet) {
    world.TellMapMode(MapModeFrom(packet));
    return FrameOutcome::kApplied;
  });
}

}  // namespace ro::net
