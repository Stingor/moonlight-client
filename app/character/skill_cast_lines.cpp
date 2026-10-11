#include "character/skill_cast_lines.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

constexpr std::string_view kSlot = "%s";

// Remplit un gabarit traduit : chaque `%s`, dans l'ordre, reçoit la valeur
// suivante ; un `%s` sans valeur reste vide.
//
// ⚠ PAS DE `snprintf` : le gabarit passe par une variable, et un nom de skill
// ou un motif d'erreur n'a pas de longueur bornée — un tampon fixe couperait la
// phrase en silence.
[[nodiscard]] std::string Filled(std::string_view format,
                                 std::initializer_list<std::string_view> values) {
  std::string text;
  const auto* value = values.begin();
  std::size_t from = 0;
  for (std::size_t slot = format.find(kSlot); slot != std::string_view::npos;
       slot = format.find(kSlot, from)) {
    text.append(format.substr(from, slot - from));
    if (value != values.end()) {
      text.append(*value);
      ++value;
    }
    from = slot + kSlot.size();
  }
  text.append(format.substr(from));
  return text;
}

}  // namespace

std::string SkillShownName(const ro::sim::SkillNode* node, const ro::sim::KnownSkill& skill) {
  if (node != nullptr && !node->display_name.empty()) {
    return node->display_name;
  }
  return skill.name.empty() ? std::to_string(skill.id) : skill.name;
}

std::string SkillArmedLine(std::string_view shown_name, const ro::sim::KnownSkill& skill) {
  if (ro::sim::SkillNeedsGround(skill)) {
    return Filled(ro::core::Tr("%s : choisissez une case."), {shown_name});
  }
  if (ro::sim::SkillTargetsTrap(skill)) {
    return Filled(ro::core::Tr("%s : choisissez un piège."), {shown_name});
  }
  return Filled(ro::core::Tr("%s : choisissez une cible."), {shown_name});
}

std::string SkillLaunchedLine(std::string_view shown_name) {
  return Filled(ro::core::Tr("%s lancé."), {shown_name});
}

std::string SkillNotSentLine(std::string_view shown_name, const ro::core::Error& why) {
  const std::string reason = ro::core::Describe(why);
  return Filled(ro::core::Tr("%s n'est pas parti : %s"), {shown_name, reason});
}

std::string SkillApproachNotSentLine(std::string_view shown_name, const ro::core::Error& why) {
  const std::string reason = ro::core::Describe(why);
  return Filled(ro::core::Tr("%s : l'approche n'est pas partie : %s"), {shown_name, reason});
}

void RecordSkillUse(ro::sim::World& world, std::string text, std::uint32_t now_ms) {
  world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kSkillUse, 0, std::move(text),
                   now_ms);
}

}  // namespace moonclient
