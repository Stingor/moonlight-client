#include "world/battle_log_rules.h"

#include "ro/render/floating_number.h"
#include "ro/render/second_hand.h"
#include "ro/render/skill_damage_timing.h"
#include "ro/render/skill_hit_extra.h"
#include "ro/vfs/battle_words.h"

#include "world/hit_popup_look.h"

namespace moonclient {
namespace {

// Ce qui, dans un nom du dictionnaire des noms, n'est pas à écrire.
constexpr char kNameCut = '#';

// Les deux couleurs du client. Il les passe en `COLORREF` — `0x00BBGGRR`, le
// rouge en octet de poids faible — : `0x00FFFF` (jaune) pour mes coups,
// `0xC8C8FF` (rose pâle) pour ceux du groupe. Ici en 0xRRGGBB.
constexpr std::uint32_t kMyBattleColor = 0xFFFF00;
constexpr std::uint32_t kPartyBattleColor = 0xFFC8C8;

}  // namespace

std::vector<BattleMessage> BattleMessagesOf(const ro::sim::Strike& strike, const StrikeSides& sides,
                                            std::uint32_t first_hit_ms, bool target_is_player,
                                            bool attacker_is_player) {
  std::vector<BattleMessage> messages;
  if (strike.damage <= 0 || strike.type == ro::render::kLuckyDodgeBlow) {
    return messages;
  }
  // LE DÉCOUPAGE DU CLIENT, celui de ses nombres, et non celui qu'un réglage
  // a choisi : un joueur qui montre le total une fois lit quand même une ligne
  // par frappe.
  HitPopupLook client;
  const StrikePlan plan = PlanStrike(strike, sides, client);
  const std::uint32_t part = StrikeTotalAt(strike, plan, 0);
  if (part == 0) {
    return messages;
  }
  const std::uint32_t step_ms = static_cast<std::uint32_t>(ro::render::kMultiHitIntervalMs) +
                                ro::render::SkillHitStepExtraMs(strike.skill_id);
  const std::uint16_t replays =
      sides.attacker_is_ground_unit
          ? std::uint16_t{0}
          : ro::render::SwingReplaysOf(strike.skill_id, strike.type, strike.hits);
  for (std::uint16_t hit = 0; hit < plan.count; ++hit) {
    const std::uint32_t due_ms = first_hit_ms + StrikeHitOffsetMs(plan, hit, step_ms);
    messages.push_back(BattleMessage{.target_id = strike.target,
                                     .attacker_id = strike.attacker,
                                     .damage = static_cast<std::int32_t>(part),
                                     .receiver_is_player = target_is_player,
                                     .receiver_id = strike.target,
                                     .due_ms = due_ms});
    // LA REPRISE D'ATTAQUE part au lanceur à la même frappe, avec les mêmes
    // faits : elle écrit donc aussi, quand la question posée la concerne.
    if (hit >= 1 && hit <= replays) {
      messages.push_back(BattleMessage{.target_id = strike.target,
                                       .attacker_id = strike.attacker,
                                       .damage = static_cast<std::int32_t>(part),
                                       .receiver_is_player = attacker_is_player,
                                       .receiver_id = strike.attacker,
                                       .due_ms = due_ms});
    }
  }
  // LA SECONDE MAIN, un message de plus : ses dégâts à elle.
  if (plan.second_hand) {
    messages.push_back(
        BattleMessage{.target_id = strike.target,
                      .attacker_id = strike.attacker,
                      .damage = strike.second_damage,
                      .receiver_is_player = target_is_player,
                      .receiver_id = strike.target,
                      .due_ms = first_hit_ms + ro::render::SecondHandOffsetMs(step_ms)});
  }
  return messages;
}

std::string BattleName(const ro::vfs::ItemWords& words, std::string_view name) {
  const std::string_view kept = name.substr(0, name.find(kNameCut));
  return kept.empty() ? std::string(ro::vfs::SomeoneText(words)) : std::string(kept);
}

std::optional<BattleLogLine> BattleLogLineFor(const ro::vfs::ItemWords& words,
                                              const BattleMessage& message, std::uint32_t self_id,
                                              const ro::sim::PartyRoster& party,
                                              std::string_view target_name,
                                              std::string_view attacker_name) {
  if (message.damage <= 0 || self_id == 0) {
    return std::nullopt;
  }
  using ro::vfs::BattleLine;
  const auto line = [&](BattleLine which, std::string_view first, std::string_view second,
                        bool mine) {
    // Sans phrase, la ligne ne s'écrit pas.
    std::string text = ro::vfs::BattleLineText(words, which, first, second, message.damage);
    if (text.empty()) {
      return std::optional<BattleLogLine>();
    }
    return std::optional<BattleLogLine>(
        BattleLogLine{.text = std::move(text),
                      .kind = mine ? ro::sim::ChatKind::kBattle : ro::sim::ChatKind::kPartyBattle,
                      .color = mine ? kMyBattleColor : kPartyBattleColor});
  };
  // UN JOUEUR QUI REÇOIT se demande qui ENCAISSE ; tout autre acteur, qui
  // FRAPPE. Les deux questions ne se posent jamais ensemble.
  if (message.receiver_is_player) {
    if (message.target_id == self_id) {
      return line(BattleLine::kIReceived, attacker_name, {}, true);
    }
    if (party.HasOnlineMember(message.target_id)) {
      return line(BattleLine::kPartyReceived, target_name, attacker_name, false);
    }
    return std::nullopt;
  }
  if (message.attacker_id == self_id) {
    return line(BattleLine::kIGave, target_name, {}, true);
  }
  if (party.HasOnlineMember(message.attacker_id)) {
    return line(BattleLine::kPartyGave, attacker_name, target_name, false);
  }
  return std::nullopt;
}

}  // namespace moonclient
