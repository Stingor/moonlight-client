// Le détail de ce que l'équipement apporte. Voir l'en-tête pour ce que le
// générateur ne porte pas, et pourquoi les décalages se nomment ici.

#include "ro/net/player_bonuses.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/player_bonuses.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Lit une liste : son compte, puis ses entrées.
//
// ⚠ LE COMPTE EST CRU, MAIS PAS SUR PAROLE. Une trame tronquée annoncerait plus
// d'entrées qu'elle n'en porte ; on s'arrête sur ce que le tampon contient
// vraiment, et le lecteur dit le reste.
template <typename Entry, typename Read>
[[nodiscard]] std::vector<Entry> ReadList(core::ByteReader& reader, std::size_t entry_size,
                                          Read read) {
  std::vector<Entry> entries;
  const std::int16_t announced = reader.I16();
  if (announced <= 0) {
    return entries;
  }
  const std::size_t fits = reader.remaining() / entry_size;
  const std::size_t count = std::min(static_cast<std::size_t>(announced), fits);
  entries.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    entries.push_back(read(reader));
  }
  return entries;
}

// Le bloc fixe, recopié champ par champ. ⚠ L'ORDRE EST CELUI DU SERVEUR et non
// un ordre d'affichage : il se relit contre l'écriture de
// `ZC_MOONLIGHT_STAT_BONUS` par le serveur.
void CopyFixedBlock(const proto::ZcMoonlightStatBonus& packet, sim::PlayerBonuses& into) {
  into.param_equip = packet.param_equip;
  into.param_bonus = packet.param_bonus;
  into.eatk = packet.eatk;
  into.ematk = packet.ematk;
  into.melee_pct = packet.melee_pct;
  into.ranged_pct = packet.ranged_pct;
  into.crit_dmg_pct = packet.crit_dmg_pct;
  into.hp_add = packet.hp_add;
  into.sp_add = packet.sp_add;
  into.aspd_add = packet.aspd_add;
  into.vcast_pct = packet.vcast_pct;
  into.fcast_pct = packet.fcast_pct;
  into.atk_pct = packet.atk_pct;
  into.matk_pct = packet.matk_pct;
  into.dmg_ret_melee = packet.dmg_ret_melee;
  into.dmg_ret_ranged = packet.dmg_ret_ranged;
  into.dmg_ret_magic = packet.dmg_ret_magic;
  into.double_pct = packet.double_pct;
  into.perfect_hit = packet.perfect_hit;
  into.hp_pct = packet.hp_pct;
  into.sp_pct = packet.sp_pct;
  into.hp_regen_pct = packet.hp_regen_pct;
  into.sp_regen_pct = packet.sp_regen_pct;
  into.crit_def_pct = packet.crit_def_pct;
  into.hp_on_kill = packet.hp_on_kill;
  into.sp_on_kill = packet.sp_on_kill;
  into.unbreak_pct = packet.unbreak_pct;
  into.pot_hp_pct = packet.pot_hp_pct;
  into.pot_sp_pct = packet.pot_sp_pct;
  into.heal_up_pct = packet.heal_up_pct;
  into.delay_pct = packet.delay_pct;
  into.add_vcast_ms = packet.add_vcast_ms;
  into.add_fcast_ms = packet.add_fcast_ms;
  into.steal_pct = packet.steal_pct;
  into.def_melee_pct = packet.def_melee_pct;
  into.def_ranged_pct = packet.def_ranged_pct;
  into.def_magic_pct = packet.def_magic_pct;
  into.def_misc_pct = packet.def_misc_pct;
  into.splash = packet.splash;
  into.splash_add = packet.splash_add;
  into.hp_drain_pct = packet.hp_drain_pct;
  into.sp_drain_pct = packet.sp_drain_pct;
  into.break_weapon_pct = packet.break_weapon_pct;
  into.break_armor_pct = packet.break_armor_pct;
  into.zeny_bonus_pct = packet.zeny_bonus_pct;
  into.classchange_pct = packet.classchange_pct;
  into.dmg_ret_reduce = packet.dmg_ret_reduce;
  into.magic_hp_gain = packet.magic_hp_gain;
  into.magic_sp_gain = packet.magic_sp_gain;
  into.refine_atk = packet.refine_atk;
  into.refine_def = packet.refine_def;
}

}  // namespace

FrameOutcome ApplyStatBonusFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightStatBonus::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightStatBonus packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }

  sim::PlayerBonuses bonuses;
  bonuses.told = true;
  CopyFixedBlock(packet, bonuses);

  // 🔴 LES TROIS LISTES SONT FACULTATIVES, et pas seulement vides : un serveur
  // d'une version antérieure s'arrête au bloc fixe. Se placer après lui et lire
  // ce qui reste rend zéro liste sans rien casser — là où exiger les comptes
  // ferait passer la trame entière pour indéchiffrable.
  core::ByteReader reader(frame.bytes);
  reader.Seek(kStatBonusFixedSize);

  if (reader.remaining() >= kStatBonusCountSize) {
    bonuses.conditional =
        ReadList<sim::ConditionalBonus>(reader, kConditionalBonusSize, [](core::ByteReader& from) {
          sim::ConditionalBonus entry;
          entry.code = from.U16();
          entry.index = from.I16();
          entry.value = from.I32();
          return entry;
        });
  }
  if (reader.remaining() >= kStatBonusCountSize) {
    bonuses.skills = ReadList<sim::SkillBonus>(reader, kSkillBonusSize, [](core::ByteReader& from) {
      sim::SkillBonus entry;
      entry.code = from.U16();
      entry.skill_id = from.U16();
      entry.level = from.I16();
      entry.value = from.I32();
      entry.triggered_by_skill = from.U16();
      return entry;
    });
  }
  if (reader.remaining() >= kStatBonusCountSize) {
    bonuses.items = ReadList<sim::ItemBonus>(reader, kItemBonusSize, [](core::ByteReader& from) {
      sim::ItemBonus entry;
      entry.code = from.U16();
      entry.item_id = from.U32();
      entry.rate = from.I32();
      return entry;
    });
  }

  world.SetPlayerBonuses(std::move(bonuses));
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
