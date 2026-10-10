// LE JOURNAL DES QUÊTES ET LE PANNEAU DE SUIVI, dans la boucle de la partie :
// leur image, puis ce qu'ils demandent ailleurs — guider, ouvrir la fiche d'un
// monstre, décrire un item, suivre une adresse, signaler un bug.

#include <ctime>
#include <string>

#include "game/game.h"
#include "world/monster_sheet.h"

namespace moonclient {

void Game::AdvanceQuests(std::uint32_t now_ms) {
  // ⚠ APRÈS LES SUCCÈS : le suivi range les entrées qu'ils viennent de composer.
  const auto now_unix_s = static_cast<std::int64_t>(std::time(nullptr));
  pieces_.quests.Advance(windows_.quests, windows_.tracker, link_, world_, now_unix_s, now_ms);
  pieces_.tracker.Advance(windows_.tracker, pieces_.quests, windows_.quests, windows_.achievements);

  if (const std::optional<ro::ui::RichPlace> place = pieces_.quests.TakeGuide();
      place.has_value()) {
    // LE MÊME GUIDAGE QU'UN LIEN DE LIEU : le trajet sur la minimap, la
    // destination écrite dans la conversation.
    FollowDescriptionLink(
        ro::ui::DescriptionWindowAction{.followed = place->map + ',' + std::to_string(place->x) +
                                                    ',' + std::to_string(place->y),
                                        .followed_kind = ro::ui::RichLinkKind::kPlace},
        now_ms);
  }
  if (std::string address = pieces_.quests.TakeAddress(); !address.empty()) {
    FollowDescriptionLink(
        ro::ui::DescriptionWindowAction{.followed = std::move(address),
                                        .followed_kind = ro::ui::RichLinkKind::kAddress},
        now_ms);
  }
  if (const std::uint32_t monster = pieces_.quests.TakeMonster();
      monster != 0 && monster_sheet_.has_value()) {
    monster_sheet_->Open(windows_.monster_sheet.state, monster, /*by_view=*/false);
  }
  // UNE RÉCOMPENSE se décrit, se relaie et s'interroge comme un butin.
  FollowCatalogItem(pieces_.quests.TakeItemGesture(), now_ms);
  if (std::optional<BugReportContext> report = pieces_.quests.TakeBugReport(); report.has_value()) {
    AskBugReport(windows_.bug_report, std::move(*report));
  }
}

}  // namespace moonclient
