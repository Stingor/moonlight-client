// La vue des joueurs bloqués. Voir l'en-tête.

#include "chat/block_list_view.h"

#include <functional>
#include <map>
#include <utility>

namespace moonclient {

ro::ui::BlockListView ComposeBlockList(const BlockListFacts& facts) {
  // Une table ordonnée : les lignes sortent triées et sans doublon.
  std::map<std::string, ro::ui::BlockedPlayerView, std::less<>> by_name;
  const auto row = [&](const std::string& name) -> ro::ui::BlockedPlayerView& {
    ro::ui::BlockedPlayerView& player = by_name[name];
    player.name = name;
    return player;
  };
  for (const std::span<const std::string> source : {facts.whispers, facts.kept_whispers}) {
    for (const std::string& name : source) {
      row(name).whispers = true;
    }
  }
  for (const std::string& name : facts.voice_blocked) {
    ro::ui::BlockedPlayerView& player = row(name);
    player.voice = true;
    player.voice_reachable = true;
  }
  ro::ui::BlockListView view{.whispers_from_everyone = facts.whispers_from_everyone,
                             .connected = facts.connected,
                             .voice_online = facts.voice_online,
                             .whispers_kept = facts.whispers_kept};
  view.players.reserve(by_name.size());
  for (auto& [name, player] : by_name) {
    if (!player.voice_reachable && facts.voice_hears) {
      player.voice_reachable = facts.voice_hears(name);
    }
    view.players.push_back(std::move(player));
  }
  return view;
}

}  // namespace moonclient
