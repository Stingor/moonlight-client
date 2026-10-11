// Les messages par numéro que le serveur envoie. Voir l'en-tête.

#include "ro/net/server_message.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"

#include "frame_apply.h"
#include "server_message_table.h"
#include "wire_color.h"

namespace ro::net {
namespace {

// LES COULEURS, en `0xRRGGBB`.
// Un message ordinaire : du blanc.
constexpr std::uint32_t kPlainMessageColor = 0xFFFFFF;
// Un refus qui se lit d'un coup d'œil : du rouge.
constexpr std::uint32_t kRedMessageColor = 0xFF0000;
// Un message qui porte une valeur : du jaune.
constexpr std::uint32_t kValueMessageColor = 0xFFFF00;
// L'issue d'une compétence : un lavande.
constexpr std::uint32_t kSkillMessageColor = 0xCDCDFF;

// LE TROU D'UN GABARIT, rempli par substitution et non par printf : une
// traduction qui changerait ses spécifications écrirait de travers, elle ne
// lirait jamais au-delà de ses arguments.
constexpr std::string_view kValueHole = "%d";
// Et celui d'un nom d'item.
constexpr std::string_view kNameHole = "%s";

// LE MESSAGE QUI NOMME UN ITEM interdit sur la carte : sa valeur est
// l'identifiant de l'item, et non un nombre à écrire.
constexpr std::uint16_t kItemBannedOnMapMessage = 3278;
// Le nom d'un item que la table ne connaît pas : une espace, et non le numéro.
constexpr std::string_view kUnknownItemName = " ";

// Ce qu'un gabarit reçoit : un nombre, ou un nom.
struct TemplateFill {
  std::optional<std::int32_t> value;
  std::optional<std::string> name;
};

// Remplit le PREMIER trou de ce genre, au-delà du préfixe.
void FillHole(std::string& line, std::size_t from, std::string_view hole, std::string_view filler) {
  const std::size_t at = line.find(hole, from);
  if (at != std::string::npos) {
    line.replace(at, hole.size(), filler);
  }
}

// LA CASE D'UN MESSAGE : l'issue d'une compétence sous « échecs de
// compétence », un plafond du tribut à la guilde sous « EXP de contribution »
// — mais seulement par `ZC_MSG`, le seul paquet que le client d'origine trie
// ainsi —, le reste sous la case générale.
[[nodiscard]] sim::ChatType TypeOf(std::uint16_t opcode, std::uint32_t message_id) {
  if (opcode == proto::ZcMsgSkill::kOpcode) {
    return sim::ChatType::kSkillFailure;
  }
  if (opcode == proto::ZcMsg::kOpcode && message_id <= std::numeric_limits<std::uint16_t>::max() &&
      ServerMessageIsGuildExpTribute(static_cast<std::uint16_t>(message_id))) {
    return sim::ChatType::kRepaymentExp;
  }
  return sim::ChatType::kPublic;
}

// Écrit la ligne. Un numéro absent de la table n'écrit rien, et le journal le
// dit.
FrameOutcome Say(sim::World& world, std::uint16_t opcode, std::uint32_t message_id,
                 std::string_view prefix, std::uint32_t color, std::uint32_t now_ms,
                 const TemplateFill& fill = {}) {
  const char* french = message_id <= std::numeric_limits<std::uint16_t>::max()
                           ? ServerMessageFrench(static_cast<std::uint16_t>(message_id))
                           : nullptr;
  if (french == nullptr) {
    core::LogInfo("message du serveur n°{} (opcode {:#06x}) absent de la table : rien d'écrit",
                  message_id, opcode);
    return FrameOutcome::kIgnored;
  }
  std::string line(prefix);
  line += core::Tr(french);
  if (fill.value.has_value()) {
    FillHole(line, prefix.size(), kValueHole, std::to_string(*fill.value));
  }
  if (fill.name.has_value()) {
    FillHole(line, prefix.size(), kNameHole, *fill.name);
  }
  world.RecordChat(sim::ChatKind::kServer, TypeOf(opcode, message_id), 0, std::move(line), now_ms,
                   color);
  return FrameOutcome::kApplied;
}

// La couleur d'un message par `ZC_MSG` ou `ZC_MSG_COLOR`, sauf pour un refus
// que la table veut rouge.
[[nodiscard]] std::uint32_t ColorOf(std::uint16_t message_id, std::uint32_t asked_rgb) {
  return ServerMessageIsRed(message_id) ? kRedMessageColor : asked_rgb;
}

// « [nom affiché] » ; à défaut le nom interne du serveur ; à défaut le numéro.
[[nodiscard]] std::string SkillPrefix(std::uint16_t skill_id, const sim::World& world,
                                      const sim::SkillTree* skills) {
  std::string name;
  const sim::SkillNode* node = skills != nullptr ? skills->Find(skill_id) : nullptr;
  if (node != nullptr && !node->display_name.empty()) {
    name = node->display_name;
  } else if (const sim::KnownSkill* known = world.skill_book().Find(skill_id);
             known != nullptr && !known->name.empty()) {
    name = known->name;
  } else {
    name = std::to_string(skill_id);
  }
  return "[" + name + "] ";
}

}  // namespace

FrameOutcome ApplyServerMessageFrame(const Frame& frame, sim::World& world,
                                     const sim::SkillTree* skills, std::uint32_t now_ms,
                                     const ItemNameLookup& item_names) {
  switch (frame.opcode) {
    case proto::ZcMsg::kOpcode:
      return Decoded<proto::ZcMsg>(frame, [&](const proto::ZcMsg& packet) {
        return Say(world, frame.opcode, packet.msgId, {}, ColorOf(packet.msgId, kPlainMessageColor),
                   now_ms);
      });
    case proto::ZcMsgColor::kOpcode:
      return Decoded<proto::ZcMsgColor>(frame, [&](const proto::ZcMsgColor& packet) {
        return Say(world, frame.opcode, packet.MessageId, {},
                   ColorOf(packet.MessageId, RgbFromBgr(packet.MessageColor)), now_ms);
      });
    case proto::ZcMsgValue::kOpcode:
      return Decoded<proto::ZcMsgValue>(frame, [&](const proto::ZcMsgValue& packet) {
        if (packet.message == kItemBannedOnMapMessage) {
          std::string name =
              item_names ? item_names(static_cast<std::uint32_t>(packet.value)) : std::string{};
          if (name.empty()) {
            name = kUnknownItemName;
          }
          return Say(world, frame.opcode, packet.message, {}, kValueMessageColor, now_ms,
                     {.name = std::move(name)});
        }
        return Say(world, frame.opcode, packet.message, {}, kValueMessageColor, now_ms,
                   {.value = packet.value});
      });
    case proto::ZcMsgSkill::kOpcode:
      return Decoded<proto::ZcMsgSkill>(frame, [&](const proto::ZcMsgSkill& packet) {
        if (packet.msgId < 0) {
          core::LogInfo("message de compétence n°{} négatif : rien d'écrit", packet.msgId);
          return FrameOutcome::kIgnored;
        }
        return Say(world, frame.opcode, static_cast<std::uint32_t>(packet.msgId),
                   SkillPrefix(packet.skillId, world, skills), kSkillMessageColor, now_ms);
      });
    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net
