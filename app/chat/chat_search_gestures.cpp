// Ce que demandent les gestes qui cherchent. Voir l'en-tête.

#include "chat/chat_search_gestures.h"

#include <string_view>

namespace moonclient {
namespace {

// ⚠ DEUX COMMANDES DU SERVEUR, non traduites : le joueur les retape parfois à
// la main.
constexpr std::string_view kMobInfoCommand = "@mobinfo ";
constexpr std::string_view kWhereIsCommand = "@whereis ";

}  // namespace

std::string MonsterCommandLine(ro::ui::ChatLinkGesture gesture, std::uint32_t monster_id) {
  if (monster_id == 0) {
    return {};
  }
  std::string_view verb;
  if (gesture == ro::ui::ChatLinkGesture::kMobInfo) {
    verb = kMobInfoCommand;
  } else if (gesture == ro::ui::ChatLinkGesture::kWhereIs) {
    verb = kWhereIsCommand;
  } else {
    return {};
  }
  std::string line(verb);
  line += std::to_string(monster_id);
  return line;
}

std::uint32_t MonsterDatabaseId(const MonsterLink& monster, bool by_view) {
  return by_view ? 0 : monster.monster_id;
}

NaviSearchLink WhereToFindSearch(const MonsterLink& monster) {
  return NaviSearchLink{.family = NaviSearchFamily::kMonster, .map = {}, .term = monster.name};
}

std::string NaviSearchDestination(const NaviSearchLink& link) {
  return link.family == NaviSearchFamily::kMap ? link.term : link.map;
}

}  // namespace moonclient
