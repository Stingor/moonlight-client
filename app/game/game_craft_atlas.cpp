// L'ATLAS DES RECETTES, vu de la boucle : ce que la fenêtre a rendu à l'image
// précédente, puis la vue qu'elle dessinera.
//
// ⚠ À CÔTÉ DE LA BOUCLE : l'Atlas forme un sujet entier — un livre lu dans
// l'installation, ses gestes, et les portes qui y mènent depuis les autres
// fenêtres.

#include <string>
#include <utility>

#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"

#include "game/game.h"

namespace moonclient {

void Game::AdvanceCraftAtlas(std::uint32_t now_ms) {
  UiStage::Windows::CraftAtlas& window = windows_.craft_atlas;
  const ro::ui::CraftAtlasAnswer answer = std::exchange(window.answer, {});
  if (!craft_atlas_.has_value()) {
    window.view = nullptr;
    window.preview = nullptr;
    return;
  }
  // L'APERÇU D'UN LIEN DE RECETTE, que les surfaces ont noté à l'image
  // précédente. ⚠ AVANT TOUT RETOUR : il ne dépend pas de la fenêtre, et un
  // lien se survole Atlas fermé.
  window.preview = craft_atlas_->TakePreview();
  CraftAtlasOptions& options = settings_->options().craft_atlas();

  // ── CE QUE LA FENÊTRE A RENDU ─────────────────────────────────────────────
  craft_atlas_->NoteDrawn(answer.drawn);
  if (answer.open_settings) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kCraftAtlas);
  }
  if (answer.only_craftable.has_value()) {
    options.SetOnlyCraftable(*answer.only_craftable, &settings_->file());
  }
  // L'APERÇU AU SURVOL, comme sur une case — quand le réglage le veut.
  if (answer.item.hovered_id != 0 && options.preview()) {
    hovered_item_ = answer.item.hovered_id;
    hovered_slot_ = 0;
    hovered_holder_ = ro::ui::ItemHolder::kInventory;
  }

  // ── LES GESTES SUR UNE LIGNE, les mêmes que partout où un item se montre ──
  //
  // ⚠ UNE LIGNE D'ATLAS EST UN ITEM DE CATALOGUE : ni raffinement, ni cartes.
  // Il se décrit, se relaie et s'interroge par son seul identifiant — et le
  // clic gauche, lui, est resté dans la fenêtre : il y navigue.
  const std::uint32_t item_id = answer.item.item_id;
  switch (item_id == 0 ? ro::ui::ChatLinkGesture::kNothing : answer.item.gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      windows_.description.item_id = item_id;
      windows_.description.from_slot = 0;
      windows_.description.from_holder = ro::ui::ItemHolder::kInventory;
      windows_.description.open = true;
      windows_.description.place_at_mouse = true;
      windows_.description.raise = true;
      break;
    case ro::ui::ChatLinkGesture::kRepost:
      PostChatLink(ro::vfs::ItemLink{.item_id = item_id});
      break;
    case ro::ui::ChatLinkGesture::kOpenDatabase:
      FollowDescriptionLink(
          ro::ui::DescriptionWindowAction{.followed = ro::vfs::ItemDatabaseUrl(item_id)}, now_ms);
      break;
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops: {
      const char* const verb =
          answer.item.gesture == ro::ui::ChatLinkGesture::kItemInfo ? "@iteminfo " : "@whodrops ";
      if (chat_dispatch_.has_value()) {
        SubmitChat(*chat_dispatch_, verb + std::to_string(item_id), now_ms);
      }
      break;
    }
    case ro::ui::ChatLinkGesture::kVoteShop:
      // Le vote shop : la boutique s'ouvre, l'item au panier.
      static_cast<void>(pieces_.cash_shop.OpenWithItem(item_id, windows_.cash_shop.state, link_));
      break;
    case ro::ui::ChatLinkGesture::kNothing:
    case ro::ui::ChatLinkGesture::kMenu:
    case ro::ui::ChatLinkGesture::kTryStyle:
    case ro::ui::ChatLinkGesture::kCopyCode:
    case ro::ui::ChatLinkGesture::kOpenBestiary:
    case ro::ui::ChatLinkGesture::kOpenInNavigation:
    case ro::ui::ChatLinkGesture::kPlaceContents:
    case ro::ui::ChatLinkGesture::kStaffWarp:
    case ro::ui::ChatLinkGesture::kWhisperWindow:
    case ro::ui::ChatLinkGesture::kInviteParty:
    case ro::ui::ChatLinkGesture::kInviteGuild:
    case ro::ui::ChatLinkGesture::kAddFriend:
    case ro::ui::ChatLinkGesture::kMonsterSheet:
    case ro::ui::ChatLinkGesture::kShareRespawn:
    case ro::ui::ChatLinkGesture::kGuideToTomb:
    case ro::ui::ChatLinkGesture::kWhereToFind:
    case ro::ui::ChatLinkGesture::kMobInfo:
    case ro::ui::ChatLinkGesture::kWhereIs:
    case ro::ui::ChatLinkGesture::kAddToMvpLog:
    case ro::ui::ChatLinkGesture::kInviteMvp:
    case ro::ui::ChatLinkGesture::kShowImageOnce:
    case ro::ui::ChatLinkGesture::kAllowImageHost:
    case ro::ui::ChatLinkGesture::kViewImage:
    case ro::ui::ChatLinkGesture::kPostWhereToFind:
    case ro::ui::ChatLinkGesture::kGoThere:
      break;
  }

  // « Obtenir tout » et « Obtenir le manque » du staff : le droit se relit ici,
  // au geste.
  SendStaffItems(answer.staff_items, Rights(), link_, world_);

  // PARTAGER LA RECETTE de l'item montré : « [Recette : …] » dans la saisie. Le
  // nom est celui que l'Atlas montre, et c'est lui qui voyage dans la balise.
  if (answer.share_recipe && window.state.selected_id != 0) {
    PostChatLink(RecipeLink{.item_id = window.state.selected_id,
                            .name = std::string(craft_atlas_->NameOf(window.state.selected_id))});
  }

  if (!window.state.open) {
    window.view = nullptr;
    return;
  }
  // ⚠ LE PARTAGE DEMANDE LA FENÊTRE DE CHAT OUVERTE : le lien se pose dans sa
  // barre de saisie, et une saisie qu'on ne voit pas enverrait la ligne sans
  // qu'on l'ait relue.
  craft_atlas_->Refresh(window.state, world_.inventory(),
                        CraftAtlasFilters{.show_unlearnable = options.show_unlearnable(),
                                          .only_craftable = options.only_craftable(),
                                          .can_share = windows_.chat.main.open});
  craft_atlas_->NoteVoteShop(pieces_.cash_shop.offers());
  craft_atlas_->NoteStaff(Rights().staff_tools, pieces_.item_stacking,
                          StaffFreeBagSlots(world_.inventory()));
  window.view = &craft_atlas_->view();
}

}  // namespace moonclient
