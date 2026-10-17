#include "ro/formats/monster_skill_info.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"

namespace ro::formats {
namespace {

constexpr std::string_view kRootTag = "Monster_Action_Table";
constexpr std::string_view kActionTag = "action";
constexpr std::string_view kLoopTypeTag = "loopType";
constexpr std::string_view kSkillTagPrefix = "_";
// La profondeur de chaque niveau, la racine comptant pour un.
constexpr std::size_t kMonsterDepth = 2;
constexpr std::size_t kSkillDepth = 3;
constexpr std::size_t kLeafDepth = 4;

// LE NOMBRE D'UNE FEUILLE, lu comme `atoi` : les blancs de tête sautés, un signe,
// des chiffres, et zéro quand il n'y en a pas.
std::int32_t LeadingInteger(std::string_view text) {
  const auto first_non_blank = std::ranges::find_if_not(
      text, [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
  text.remove_prefix(static_cast<std::size_t>(first_non_blank - text.begin()));
  if (!text.empty() && text.front() == '+') {
    text.remove_prefix(1);
  }
  std::int32_t value = 0;
  std::from_chars(text.data(), text.data() + text.size(), value);
  return value;
}

// Le nom d'une balise ouvrante, sans ses attributs.
std::string_view TagName(std::string_view inside) {
  const auto name_end = std::ranges::find_if(
      inside, [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0 || c == '/'; });
  return inside.substr(0, static_cast<std::size_t>(name_end - inside.begin()));
}

}  // namespace

MonsterSkillInfo MonsterSkillInfo::Parse(std::span<const std::uint8_t> bytes) {
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  MonsterSkillInfo info;
  std::vector<std::string_view> open_tags;
  // Ce que le bloc de compétence en cours a lu.
  std::optional<std::int32_t> action;
  std::int32_t loop_type = 0;
  // Faux dès que la racine n'est pas la bonne : rien ne se lit alors.
  bool inside_root = false;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t tag_start = text.find('<', pos);
    if (tag_start == std::string_view::npos) {
      break;
    }
    // Le texte avant la balise appartient à la feuille ouverte.
    if (inside_root && open_tags.size() == kLeafDepth) {
      const std::string_view leaf_text = text.substr(pos, tag_start - pos);
      if (open_tags.back() == kActionTag && !action.has_value()) {
        action = LeadingInteger(leaf_text);
      } else if (open_tags.back() == kLoopTypeTag) {
        loop_type = LeadingInteger(leaf_text);
      }
    }
    if (text.substr(tag_start).starts_with("<!--")) {
      const std::size_t name_end = text.find("-->", tag_start);
      pos = name_end == std::string_view::npos ? text.size() : name_end + 3;
      continue;
    }
    const std::size_t tag_end = text.find('>', tag_start);
    if (tag_end == std::string_view::npos) {
      break;
    }
    const std::string_view tag_body = text.substr(tag_start + 1, tag_end - tag_start - 1);
    pos = tag_end + 1;
    if (tag_body.empty() || tag_body.front() == '?' || tag_body.front() == '!') {
      continue;
    }
    if (tag_body.front() == '/') {
      if (open_tags.empty()) {
        continue;
      }
      if (inside_root && open_tags.size() == kSkillDepth && !info.monsters_.empty()) {
        Entry entry{info.monsters_.size() - 1, std::string(open_tags[kSkillDepth - 1]),
                    std::nullopt};
        if (action.has_value()) {
          entry.action = MonsterSkillAction{*action, loop_type};
        }
        info.entries_.push_back(std::move(entry));
        action.reset();
        loop_type = 0;
      }
      open_tags.pop_back();
      if (open_tags.empty()) {
        inside_root = false;
      }
      continue;
    }
    const std::string_view tag_name = TagName(tag_body);
    const bool self_closing = tag_body.back() == '/';
    if (open_tags.empty()) {
      inside_root = tag_name == kRootTag;
    }
    if (self_closing) {
      continue;
    }
    open_tags.push_back(tag_name);
    if (inside_root && open_tags.size() == kMonsterDepth) {
      info.monsters_.emplace_back(tag_name);
    }
  }
  return info;
}

std::optional<MonsterSkillAction> MonsterSkillInfo::Find(std::string_view resource_name,
                                                         std::uint32_t skill_id) const {
  const std::string espece = core::AsciiLower(resource_name);
  const auto bloc = std::ranges::find(monsters_, espece);
  if (bloc == monsters_.end()) {
    return std::nullopt;
  }
  const auto rang = static_cast<std::size_t>(bloc - monsters_.begin());
  const std::string balise = std::string(kSkillTagPrefix) + std::to_string(skill_id);
  const auto trouvee = std::ranges::find_if(entries_, [&](const Entry& entry) {
    return entry.block == rang && entry.skill_tag == balise;
  });
  if (trouvee == entries_.end()) {
    return std::nullopt;
  }
  return trouvee->action;
}

}  // namespace ro::formats
