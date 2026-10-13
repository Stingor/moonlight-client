// LES BULLES D'ANNONCE, VUES DE LA BOUCLE : la place d'une poignée tirée à
// l'image d'avant, ce que le monde a déposé, puis ce que les deux bandeaux
// peindront. Voir `world/broadcast_balloons.h`.

#include <cstdint>

#include "game/game.h"
#include "game/setting_destination_open.h"
#include "world/broadcast_balloons.h"

namespace moonclient {

void Game::StageBalloons(std::uint32_t now_ms) {
  BalloonBoards& boards = windows_.balloons;
  // ⚠ LA PLACE PRISE À L'IMAGE D'AVANT S'ÉCRIT D'ABORD : le réglage qu'on va
  // relire doit déjà la porter, sinon la poignée sauterait en arrière.
  if (boards.announcement_ask.moved) {
    settings_->NoteBalloonAnchor(/*hunts=*/false, boards.announcement_ask);
  }
  if (boards.hunt_ask.moved) {
    settings_->NoteBalloonAnchor(/*hunts=*/true, boards.hunt_ask);
  }
  boards.announcement_ask = {};
  boards.hunt_ask = {};

  const ro::ui::BroadcastBalloonLook& look = settings_->options().pages().balloons.look();
  pieces_.balloons.Notice(world_, look);
  const SettingReach reach = SettingReachOf(pieces_);
  // UN ITEM FORGÉ LIÉ se nomme par son forgeron, comme dans le fil.
  const CreatorNames creators =
      item_description_.has_value() ? item_description_->creator_names() : CreatorNames();
  const BalloonDecor decor{
      .tables =
          ChatDecorTables{
              .items = pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
              .words = &pieces_.item_words,
              .creators = &creators,
              .maps = pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr,
              .settings = &reach},
      .item_icon = chat_tabs_.has_value() ? chat_tabs_->rich().item_icon : ItemIconLookup()};
  pieces_.balloons.Compose(now_ms, look, decor, !look.announcements.locked, !look.hunts.locked);
  boards.announcements = pieces_.balloons.announcements();
  boards.hunts = pieces_.balloons.hunts();
  boards.look = &look;
}

}  // namespace moonclient
