#include "world/staff_roster_menu.h"

#include <utility>
#include <vector>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::EntityMenuItem;

constexpr std::string_view kGoToCommand = "@jumpto ";
constexpr std::string_view kRecallCommand = "@recall ";

// Les lignes du menu d'entité qui valent pour un joueur hors de vue.
[[nodiscard]] bool WorksAway(int verb) {
  switch (static_cast<EntityVerb>(verb)) {
    case EntityVerb::kWhisper:
    case EntityVerb::kWhisperWindow:
    case EntityVerb::kMailSend:
    case EntityVerb::kCopyName:
    case EntityVerb::kInspect:
    case EntityVerb::kCopyId:
    case EntityVerb::kEventPoints:
    case EntityVerb::kMute:
    case EntityVerb::kUnmute:
    case EntityVerb::kMannerGood:
    case EntityVerb::kMannerBad:
    case EntityVerb::kChatBlockLog:
    case EntityVerb::kFullStrip:
    case EntityVerb::kNuke:
    case EntityVerb::kKick:
    case EntityVerb::kJail:
    case EntityVerb::kBlockAccount:
      return true;
    default:
      break;
  }
  return false;
}

// Les deux lignes de voyage, en un bloc de la section du staff.
void AddTravel(std::vector<EntityMenuItem>& items) {
  const auto add = [&items](const char* label, EntityVerb verb, const char* tip, bool separator) {
    EntityMenuItem item;
    item.label = label;
    item.verb = static_cast<int>(verb);
    item.separator = separator;
    item.staff = true;
    item.submenu = true;
    item.tip = tip;
    items.push_back(std::move(item));
  };
  add(Tr("Aller à lui"), EntityVerb::kStaffGoTo,
      Tr("@jumpto : vous êtes téléporté sur sa case, où qu'il soit."), /*separator=*/true);
  add(Tr("Le rappeler ici"), EntityVerb::kStaffRecall,
      Tr("@recall : il est téléporté sur votre case, d'où qu'il vienne."), /*separator=*/false);
}

}  // namespace

ro::ui::EntityMenuView BuildStaffRosterMenu(const EntityMenuFacts& facts) {
  ro::ui::EntityMenuView view = BuildEntityMenu(facts, EntityMenuSettings{.staff_extras = true});
  std::vector<EntityMenuItem> kept;
  // Le trait d'un bloc dont la tête est retirée revient à sa première ligne
  // gardée.
  bool separator_owed = false;
  for (EntityMenuItem& item : view.items) {
    if (!WorksAway(item.verb)) {
      separator_owed = separator_owed || item.separator;
      continue;
    }
    // Ni la première ligne du menu ni celle qui ouvre la section du staff ne
    // portent de trait : le dessin pose le sien.
    const bool opens_staff = item.staff && (kept.empty() || !kept.back().staff);
    item.separator = !kept.empty() && !opens_staff && (item.separator || separator_owed);
    separator_owed = false;
    const bool identity_done = item.staff && item.verb == static_cast<int>(EntityVerb::kCopyId);
    kept.push_back(std::move(item));
    if (identity_done && facts.family == EntityFamily::kPlayer) {
      AddTravel(kept);
    }
  }
  view.items = std::move(kept);
  return view;
}

std::string StaffTravelCommand(EntityVerb verb, std::string_view name) {
  if (name.empty()) {
    return {};
  }
  if (verb == EntityVerb::kStaffGoTo) {
    return std::string(kGoToCommand) + std::string(name);
  }
  if (verb == EntityVerb::kStaffRecall) {
    return std::string(kRecallCommand) + std::string(name);
  }
  return {};
}

}  // namespace moonclient
