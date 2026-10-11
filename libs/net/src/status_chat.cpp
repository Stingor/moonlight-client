// Ce qu'un état écrit dans la conversation. Voir l'en-tête.

#include "ro/net/status_chat.h"

#include <array>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "status_chat_texts.h"

namespace ro::net {
namespace {

using status_chat::kLiftColor;
using status_chat::kPoseColor;

// Les états que les phrases chiffrées lisent, et celui que le paquet renomme
// (les EFST du serveur).
constexpr sim::StatusEffectId kEfstWeaponProperty = 64;
constexpr sim::StatusEffectId kEfstCashPlusExp = 250;
constexpr sim::StatusEffectId kEfstCashReceiveItem = 252;
constexpr sim::StatusEffectId kEfstCashPlusOnlyJobExp = 312;
constexpr sim::StatusEffectId kEfstRewardPlusOnlyJobExp = 636;
constexpr sim::StatusEffectId kEfstAllStatDown = 1013;

// L'ÉLÉMENT D'UNE ARME ENCHANTÉE SE LIT DANS L'ÉTAT : la forme à durée écrit
// `EFST_WEAPONPROPERTY` sous l'état de son élément, `EFST_ATTACK_PROPERTY_*`,
// le premier d'entre eux plus `val1`. Aucun n'a de phrase.
constexpr sim::StatusEffectId kEfstAttackPropertyFirst = 897;

// LES ÉTATS QUE LA FORME ANCIENNE N'ÉCRIT PAS : elle les écarte avant toute
// phrase (EFST_UNLIMITED_HUMMING_VOICE, EFST_MUSICAL_INTERLUDE,
// EFST_JAWAII_SERENADE, EFST_PRON_MARCH).
constexpr std::array<sim::StatusEffectId, 4> kSilentInOldTimedForm = {454, 1261, 1262, 1263};

constexpr std::uint32_t kMsPerMinute = 60'000;
constexpr std::uint32_t kMinutesPerHour = 60;
// `val1` d'une récompense est un pourcentage, que la phrase écrit en facteur.
constexpr double kPercent = 100.0;

// Le gain que la phrase fixe de la forme nue annonce, faute de `val1`.
constexpr int kBareJobExpBonusPercent = 50;

// Les gains de la boutique se disent en minutes restantes ; un état qui en a
// moins d'une se tait.
[[nodiscard]] std::optional<StatusChatPhrase> TimedShopPhrase(const StatusChatFacts& facts) {
  const auto minutes = static_cast<int>(facts.remaining_ms / kMsPerMinute);
  switch (facts.effect) {
    case kEfstCashPlusExp:
      if (minutes == 0) {
        return std::nullopt;
      }
      return StatusChatPhrase{
          core::Printed(core::Tr("Pendant %d minutes, votre EXP augmente de %d %%."), minutes,
                        facts.val1),
          kPoseColor};
    case kEfstCashReceiveItem: {
      if (minutes == 0) {
        return std::nullopt;
      }
      const int hours = minutes / static_cast<int>(kMinutesPerHour);
      if (hours > 0) {
        return StatusChatPhrase{
            core::Printed(core::Tr("Pendant %d heures %.2d minutes, le drop d'items des monstres "
                                   "augmente de %d %%."),
                          hours, minutes % static_cast<int>(kMinutesPerHour), facts.val1),
            kPoseColor};
      }
      return StatusChatPhrase{
          core::Printed(
              core::Tr("Pendant %d minutes, le drop d'items des monstres augmente de %d %%."),
              minutes, facts.val1),
          kPoseColor};
    }
    case kEfstCashPlusOnlyJobExp:
      if (minutes == 0) {
        return std::nullopt;
      }
      return StatusChatPhrase{
          core::Printed(
              core::Tr("Pendant %d minutes, l'EXP de job tirée des monstres augmente de %d %%."),
              minutes, facts.val1),
          kPoseColor};
    case kEfstRewardPlusOnlyJobExp:
      return StatusChatPhrase{
          core::Printed(
              core::Tr("L'EXP de job de chasse que le doublement de %d peut rapporter est de "
                       "%.2f minutes."),
              facts.val2, facts.val1 / kPercent + 1.0),
          kPoseColor};
    default:
      return std::nullopt;
  }
}

[[nodiscard]] std::optional<StatusChatPhrase> PosePhrase(const StatusChatFacts& facts) {
  if (facts.effect == kEfstAllStatDown) {
    return StatusChatPhrase{core::Tr("Toutes les caractéristiques diminuent."), kLiftColor};
  }
  if (facts.form == StatusChatForm::kBare) {
    // La forme nue n'a pas de durée : les gains de la boutique y ont leur
    // phrase fixe, celle de la durée qu'ils avaient à l'origine.
    if (facts.effect == kEfstCashReceiveItem) {
      return StatusChatPhrase{core::Tr("Le drop des items ordinaires des monstres est doublé "
                                       "pendant les 30 prochaines minutes."),
                              kPoseColor};
    }
    if (facts.effect == kEfstCashPlusOnlyJobExp) {
      return StatusChatPhrase{
          core::Printed(
              core::Tr("L'EXP de job tirée des monstres augmente de %d %% pendant 30 minutes."),
              kBareJobExpBonusPercent),
          kPoseColor};
    }
  }
  if (const char* shared = status_chat::SharedPoseText(facts.effect); shared != nullptr) {
    return StatusChatPhrase{shared, kPoseColor};
  }
  if (facts.form == StatusChatForm::kBare) {
    return std::nullopt;
  }
  if (std::optional<StatusChatPhrase> shop = TimedShopPhrase(facts); shop.has_value()) {
    return shop;
  }
  if (const status_chat::Phrase phrase = status_chat::TimedPosePhrase(facts.effect);
      phrase.text != nullptr) {
    return StatusChatPhrase{phrase.text, phrase.color_rgb};
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<StatusChatPhrase> LiftPhrase(const StatusChatFacts& facts) {
  // ⚠ LA LEVÉE D'UN MALUS S'ÉCRIT EN CYAN, et sa phrase dépend de la forme.
  if (facts.effect == kEfstAllStatDown) {
    return StatusChatPhrase{facts.form == StatusChatForm::kTimed
                                ? core::Tr("Toutes les caractéristiques diminuent.")
                                : core::Tr("La baisse de toutes les caractéristiques est levée."),
                            kPoseColor};
  }
  if (const char* shared = status_chat::SharedLiftText(facts.effect); shared != nullptr) {
    return StatusChatPhrase{shared, kLiftColor};
  }
  if (facts.form == StatusChatForm::kTimed) {
    return std::nullopt;
  }
  if (const status_chat::Phrase phrase = status_chat::BareLiftPhrase(facts.effect);
      phrase.text != nullptr) {
    return StatusChatPhrase{phrase.text, phrase.color_rgb};
  }
  return std::nullopt;
}

// Qui porte l'état, et la transition elle-même ; rien pour une trame illisible
// ou une transition que la forme écarte.
struct Transition {
  std::uint32_t account_id = 0;
  StatusChatFacts facts;
};

[[nodiscard]] StatusChatFacts TimedFacts(sim::StatusEffectId effect, bool active,
                                         std::uint32_t remaining_ms, std::int32_t val1,
                                         std::int32_t val2) {
  return StatusChatFacts{.form = StatusChatForm::kTimed,
                         .effect = effect,
                         .active = active,
                         .remaining_ms = remaining_ms,
                         .val1 = val1,
                         .val2 = val2};
}

[[nodiscard]] std::optional<Transition> TransitionOf(const Frame& frame) {
  switch (frame.opcode) {
    case proto::PacketStatusChange::kOpcode: {
      proto::PacketStatusChange packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      return Transition{
          packet.AID, TimedFacts(static_cast<sim::StatusEffectId>(packet.index), packet.state != 0,
                                 packet.Left, packet.val1, packet.val2)};
    }
    case proto::PacketStatusChange2::kOpcode: {
      proto::PacketStatusChange2 packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      const auto effect = static_cast<sim::StatusEffectId>(packet.index);
      for (const sim::StatusEffectId silent : kSilentInOldTimedForm) {
        if (effect == silent) {
          return std::nullopt;
        }
      }
      return Transition{
          packet.AID, TimedFacts(effect, packet.state != 0, packet.Left, packet.val1, packet.val2)};
    }
    // L'état d'un acteur qui entre en vue : toujours une pose.
    case proto::ZcEfstSetEnter::kOpcode: {
      proto::ZcEfstSetEnter packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      return Transition{packet.targetID,
                        TimedFacts(static_cast<sim::StatusEffectId>(packet.type), true,
                                   packet.duration2, packet.val1, packet.val2)};
    }
    case proto::PacketStatusChangeEnd::kOpcode: {
      proto::PacketStatusChangeEnd packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return std::nullopt;
      }
      return Transition{packet.AID,
                        StatusChatFacts{.form = StatusChatForm::kBare,
                                        .effect = static_cast<sim::StatusEffectId>(packet.index),
                                        .active = packet.state != 0}};
    }
    default:
      return std::nullopt;
  }
}

[[nodiscard]] bool IsStatusOpcode(std::uint16_t opcode) {
  return opcode == proto::PacketStatusChange::kOpcode ||
         opcode == proto::PacketStatusChange2::kOpcode ||
         opcode == proto::ZcEfstSetEnter::kOpcode ||
         opcode == proto::PacketStatusChangeEnd::kOpcode;
}

}  // namespace

std::optional<StatusChatPhrase> StatusChatPhraseOf(const StatusChatFacts& facts) {
  StatusChatFacts read = facts;
  if (read.form == StatusChatForm::kTimed && read.effect == kEfstWeaponProperty) {
    read.effect = static_cast<sim::StatusEffectId>(kEfstAttackPropertyFirst + read.val1);
  }
  return read.active ? PosePhrase(read) : LiftPhrase(read);
}

FrameOutcome ApplyStatusChatFrame(const Frame& frame, sim::World& world,
                                  const sim::PartyRoster& party, std::uint32_t now_ms) {
  if (!IsStatusOpcode(frame.opcode)) {
    return FrameOutcome::kUnhandled;
  }
  const std::optional<Transition> transition = TransitionOf(frame);
  if (!transition.has_value() || transition->account_id == 0) {
    return FrameOutcome::kIgnored;
  }
  const bool mine = transition->account_id == world.player_id();
  if (!mine && !party.HasOnlineMember(transition->account_id)) {
    return FrameOutcome::kIgnored;
  }
  std::optional<StatusChatPhrase> phrase = StatusChatPhraseOf(transition->facts);
  if (!phrase.has_value() || phrase->text.empty()) {
    return FrameOutcome::kIgnored;
  }
  if (mine) {
    world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kAbnormalStatus, 0,
                     std::move(phrase->text), now_ms, phrase->color_rgb);
    return FrameOutcome::kApplied;
  }
  const sim::PartyMember* member = party.Find(transition->account_id);
  std::string line =
      core::Printed(core::Tr("Sur %s : %s"), member->name.c_str(), phrase->text.c_str());
  world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kPartyStatus, 0, std::move(line), now_ms,
                   phrase->color_rgb);
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
