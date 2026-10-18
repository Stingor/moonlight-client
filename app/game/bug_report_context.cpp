#include "game/bug_report_context.h"

#include <cstdio>
#include <format>
#include <utility>

#include "ro/core/build_stamp.h"
#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/net/bug_report.h"

namespace moonclient {
namespace {

using ro::net::BugReportCategory;
// Au-dessous, un caractère de contrôle : il devient une espace.
constexpr unsigned char kFirstPrintable = 0x20;

// La révision, qui ferme chaque contexte.
[[nodiscard]] std::string Revision() {
  return std::format(R"("client_version":"{}","client_commit":"{}","client_tree":"{}")",
                     JsonEscaped(ro::core::kBuildVersion), JsonEscaped(ro::core::kBuildCommit),
                     JsonEscaped(ro::core::kBuildTree));
}

// Le nom tel que le libellé l'écrit. ⚠ UN NOM VIDE DEVIENT « ? » : sinon le
// joueur lirait « PNJ :  (GID 42) ».
[[nodiscard]] std::string Shown(std::string_view name) {
  return name.empty() ? std::string("?") : std::string(name);
}

[[nodiscard]] BugReportContext Entity(BugReportCategory category, std::string label,
                                      std::string_view key, std::uint32_t id, std::string_view name,
                                      std::string_view extra) {
  return BugReportContext{.category = static_cast<std::uint8_t>(category),
                          .label = std::move(label),
                          .json = std::format(R"({{"{}":{},"name":"{}"{},{}}})", key, id,
                                              JsonEscaped(name), extra, Revision())};
}

}  // namespace

std::string JsonEscaped(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char letter : text) {
    switch (letter) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += static_cast<unsigned char>(letter) < kFirstPrintable ? ' ' : letter;
        break;
    }
  }
  return out;
}

std::string JsonClipped(std::string_view text, std::size_t most_bytes) {
  return JsonEscaped(ro::core::WithoutDanglingUtf8(text.substr(0, most_bytes)));
}

BugReportContext ItemBugReport(std::uint32_t item_id, std::string_view name, int refine,
                               const std::array<std::uint32_t, 4>& cards) {
  std::string extra;
  if (refine >= 0) {
    extra += std::format(R"(,"refine":{})", refine);
  }
  if (cards[0] != 0 || cards[1] != 0 || cards[2] != 0 || cards[3] != 0) {
    extra += std::format(R"(,"cards":[{},{},{},{}])", cards[0], cards[1], cards[2], cards[3]);
  }
  // ⚠ LE FORMAT EST ÉCRIT À CHAQUE APPEL, et c'est voulu : un littéral traduit
  // se vérifie contre ses arguments, un format passé en paramètre non.
  std::array<char, kBugReportLabelBytes> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Item : %s (#%u)"), Shown(name).c_str(),
                static_cast<unsigned>(item_id));
  return Entity(BugReportCategory::kItem, label.data(), "item_id", item_id, name, extra);
}

BugReportContext SkillBugReport(std::uint16_t skill_id, std::string_view name, int level) {
  const std::string extra = level > 0 ? std::format(R"(,"level":{})", level) : std::string();
  std::array<char, kBugReportLabelBytes> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Skill : %s (#%u)"), Shown(name).c_str(),
                static_cast<unsigned>(skill_id));
  return Entity(BugReportCategory::kSkill, label.data(), "skill_id", skill_id, name, extra);
}

BugReportContext NpcBugReport(std::uint32_t npc_gid, std::string_view name) {
  std::array<char, kBugReportLabelBytes> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("PNJ : %s (GID %u)"), Shown(name).c_str(),
                static_cast<unsigned>(npc_gid));
  return Entity(BugReportCategory::kNpc, label.data(), "npc_gid", npc_gid, name, "");
}

BugReportContext MonsterBugReport(const MonsterBugScene& scene) {
  std::string extra;
  if (!scene.aegis_name.empty()) {
    extra += std::format(R"(,"aegis_name":"{}")", JsonEscaped(scene.aegis_name));
  }
  extra += std::format(R"(,"boss_rank":{})", scene.boss_rank);
  if (!scene.seen_map.empty()) {
    extra += std::format(R"(,"seen_map":"{}")", JsonEscaped(scene.seen_map));
  }
  std::array<char, kBugReportLabelBytes> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Monstre : %s (#%u)"),
                Shown(scene.name).c_str(), static_cast<unsigned>(scene.monster_id));
  return Entity(BugReportCategory::kGeneric, label.data(),
                scene.by_sprite_class ? "sprite_class" : "mob_id", scene.monster_id, scene.name,
                extra);
}

BugReportContext StyleBugReport(std::string_view style_code) {
  return BugReportContext{
      .category = static_cast<std::uint8_t>(BugReportCategory::kStyle),
      .label = std::string(ro::core::Tr("Éditeur de style")),
      .json = std::format(R"({{"style_code":"{}",{}}})", JsonEscaped(style_code), Revision())};
}

BugReportContext GenericBugReport() {
  return BugReportContext{
      .category = static_cast<std::uint8_t>(BugReportCategory::kGeneric),
      .label = std::string(ro::core::Tr("Rapport général (aucun contexte spécifique)")),
      .json = std::format("{{{}}}", Revision())};
}

BugReportContext FieldsBugReport(std::string label, std::string_view fields_json) {
  return BugReportContext{.category = static_cast<std::uint8_t>(BugReportCategory::kGeneric),
                          .label = std::move(label),
                          .json = std::format("{{{},{}}}", fields_json, Revision())};
}

void AskBugReport(ro::ui::BugReportWindowState& window, BugReportContext context) {
  if (window.open) {
    return;
  }
  window.category = context.category;
  window.context_label = std::move(context.label);
  window.context_json = std::move(context.json);
  window.asked = true;
}

void AskBugReport(ro::ui::BugReportWindowState& window, std::optional<BugReportContext> context) {
  if (context.has_value()) {
    AskBugReport(window, std::move(*context));
  }
}

}  // namespace moonclient
