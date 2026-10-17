// L'ATLAS DES RECETTES, vu de la boucle : ce que la fenêtre a rendu à l'image
// précédente, puis la vue qu'elle dessinera.
//
// ⚠ À CÔTÉ DE LA BOUCLE : l'Atlas forme un sujet entier — un livre lu dans
// l'installation, ses gestes, et les portes qui y mènent depuis les autres
// fenêtres.

#include <string>
#include <utility>

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
  // ── LES GESTES SUR UNE LIGNE, les mêmes que partout où un item se montre ──
  //
  // ⚠ UNE LIGNE D'ATLAS EST UN ITEM DE CATALOGUE : ni raffinement, ni cartes. Et
  // le clic gauche, lui, est resté dans la fenêtre : il y navigue. L'aperçu au
  // survol, comme sur une case, seulement quand le réglage le veut.
  ro::ui::ChatLinkAsk item = answer.item;
  if (!options.preview()) {
    item.hovered_id = 0;
  }
  FollowCatalogItem(item, now_ms);

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
  craft_atlas_->NoteLinkFacts(ItemLinkFactsNow());
  craft_atlas_->NoteStaff(Rights().staff_tools, pieces_.item_properties.stacking(),
                          StaffFreeBagSlots(world_.inventory()));
  window.view = &craft_atlas_->view();
}

}  // namespace moonclient
