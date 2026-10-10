#include "world/skill_refusal_report.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/net/server_message.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_link.h"

#include "world/skill_refusal_texts.h"

namespace moonclient {
namespace {

// Les deux trous d'une phrase. ⚠ REMPLIS PAR SUBSTITUTION, jamais par printf :
// la phrase est traduite, donc variable, et un nom d'objet qui porterait un
// `%` ne doit rien consommer.
constexpr std::string_view kTextHole = "%s";
constexpr std::string_view kNumberHole = "%d";

void Fill(std::string& text, std::string_view hole, std::string_view value) {
  if (const std::size_t at = text.find(hole); at != std::string::npos) {
    text.replace(at, hole.size(), value);
  }
}

// Le nom AFFICHÉ d'une compétence, puis le nom interne que le grimoire tient du
// serveur, puis son numéro.
std::string SkillNameOf(std::uint16_t skill_id, const ro::sim::World& world,
                        const ro::sim::SkillTree* skills) {
  if (const ro::sim::SkillNode* node = skills != nullptr ? skills->Find(skill_id) : nullptr;
      node != nullptr && !node->display_name.empty()) {
    return node->display_name;
  }
  if (const ro::sim::KnownSkill* known = world.skill_book().Find(skill_id);
      known != nullptr && !known->name.empty()) {
    return known->name;
  }
  std::array<char, 32> numbered{};
  std::snprintf(numbered.data(), numbered.size(), ro::core::Tr("Skill n°%u"),
                static_cast<unsigned>(skill_id));
  return numbered.data();
}

// L'objet requis, en LIEN D'ITEM : la conversation le montre sous son nom, et
// il se clique comme tout autre lien. Sans table des objets, son numéro.
std::string ItemLinkOf(std::uint32_t item_id, const ro::vfs::ItemTable* items) {
  if (items == nullptr || item_id == 0) {
    return "#" + std::to_string(item_id);
  }
  const ro::vfs::ItemEntry* entry = items->Find(item_id);
  return ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = item_id},
                                entry != nullptr ? entry->card_slots : std::uint8_t{0});
}

// Le texte d'un message de la table du serveur, traduit, ou vide.
std::string ServerMessageText(std::uint32_t message_id) {
  const auto table = ro::net::ServerMessageTable();
  const auto found =
      std::ranges::lower_bound(table, message_id, {}, &ro::net::ServerMessageEntry::message_id);
  if (found == table.end() || found->message_id != message_id) {
    return {};
  }
  return ro::core::Tr(found->french);
}

}  // namespace

std::string SkillRefusalLine(const ro::sim::SkillRefusal& refusal, const ro::sim::World& world,
                             const ro::sim::SkillTree* skills, const ro::vfs::ItemTable* items) {
  const RefusalPhrase phrase = RefusalPhraseOf(refusal);
  std::string said;
  if (phrase.french != nullptr) {
    said = ro::core::Tr(phrase.french);
  } else if (const std::uint32_t message_id = RefusalMessageId(refusal); message_id != 0) {
    said = ServerMessageText(message_id);
  }
  if (said.empty()) {
    return {};
  }
  bool names_skill = false;
  switch (phrase.hole) {
    case RefusalHole::kNone:
      break;
    case RefusalHole::kSkillName:
      Fill(said, kTextHole, SkillNameOf(refusal.skill_id, world, skills));
      names_skill = true;
      break;
    case RefusalHole::kItemAndCount:
      // ⚠ LE NOMBRE D'ABORD : le nom de l'objet, posé avant, pourrait porter un
      // `%d` qu'on prendrait pour le trou.
      Fill(said, kNumberHole, std::to_string(refusal.count));
      Fill(said, kTextHole, ItemLinkOf(refusal.item_id, items));
      break;
    case RefusalHole::kItem:
      Fill(said, kTextHole, ItemLinkOf(refusal.item_id, items));
      break;
    case RefusalHole::kCount:
      Fill(said, kNumberHole, std::to_string(refusal.count));
      break;
    case RefusalHole::kSkillOfCount:
      Fill(said, kTextHole, SkillNameOf(static_cast<std::uint16_t>(refusal.count), world, skills));
      break;
    case RefusalHole::kCause:
      Fill(said, kNumberHole, std::to_string(refusal.cause));
      break;
  }
  if (phrase.then_french != nullptr) {
    said += ' ';
    said += ro::core::Tr(phrase.then_french);
  }
  if (names_skill) {
    return said;
  }
  std::string line = ro::core::Tr(RefusalLineFrench());
  Fill(line, kTextHole, SkillNameOf(refusal.skill_id, world, skills));
  Fill(line, kTextHole, said);
  return line;
}

void SkillRefusalReport::Notice(ro::sim::World& world, const ro::sim::SkillTree* skills,
                                const ro::vfs::ItemTable* items, bool skill_fail_shown) {
  const ro::sim::SkillBook& book = world.skill_book();
  if (book.last_refusal_seq() < seen_seq_) {
    // Le rang a reculé : c'est une autre session.
    seen_seq_ = 0;
  }
  // 🔴 ON COMPOSE D'ABORD, ON ÉCRIT ENSUITE : les refus sont lus par une vue
  // sur le grimoire, que le monde porte comme la conversation.
  std::vector<std::pair<std::string, std::uint32_t>> lines;
  for (const ro::sim::SkillRefusal& refusal : book.refusals()) {
    if (refusal.seq <= seen_seq_ || (!skill_fail_shown && RefusalFollowsSkillFailSwitch(refusal))) {
      continue;
    }
    if (std::string line = SkillRefusalLine(refusal, world, skills, items); !line.empty()) {
      lines.emplace_back(std::move(line), refusal.refused_ms);
    }
  }
  seen_seq_ = book.last_refusal_seq();
  for (auto& [text, said_ms] : lines) {
    world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kSkillFailure, 0,
                     std::move(text), said_ms, kSkillRefusalColor);
  }
}

}  // namespace moonclient
