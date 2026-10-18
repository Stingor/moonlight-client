// LES CONTENANTS, VUS DE LA BOUCLE : le sac, la charrette, le coffre et la
// banque — leurs vues composées avant le dessin, puis ce que leurs fenêtres ont
// demandé.
//
// ⚠ ILS VONT ENSEMBLE PARCE QU'ILS SE RÉPONDENT : le coffre ouvert change la
// destination du sac, le raccourci de l'un ouvre l'autre, et le même geste
// peut venir de deux fenêtres. Les objets tirés de l'un à l'autre vivent dans
// `game_item_drag.cpp`.

#include <cstdint>
#include <initializer_list>

#include "ro/core/i18n.h"
#include "ro/vfs/item_table.h"

#include "game/game.h"
#include "game/message_language.h"
#include "game/warp_list_stage.h"
#include "items/transfer_hold.h"
#include "world/click_action.h"

namespace moonclient {

void Game::StageContainers() {
  // LE SAC SE REFAIT AVANT LE DESSIN : l'onglet et le filtre décident de ce
  // qu'il porte, et les deux peuvent avoir changé à l'image précédente.
  // ⚠ LE TRI NE SE DEMANDE QUE TAILLE DÉVERROUILLÉE : c'est la condition sous
  // laquelle son bouton existe, et un tri qui survivrait à la disparition de son
  // bouton écraserait l'ordre du serveur sans que rien ne l'explique.
  //
  // ⚠ LA TABLE DES PLACES NE DESCEND QUE SI LE PLACEMENT LIBRE EST EN VIGUEUR, et
  // c'est le drapeau DE LA FENÊTRE qui le dit : il porte déjà les trois
  // conditions — le réglage, le verrou de taille et l'onglet. Relire le réglage
  // ici en oublierait deux.
  bag_->Refresh(windows_.inventory.open, world_, windows_.inventory.tab, windows_.inventory.filter,
                settings_->options().inventory_drop_locked(),
                settings_->options().inventory_sorted() && !windows_.inventory.size_locked,
                windows_.inventory.free_layout ? &settings_->item_layout() : nullptr);
  windows_.inventory_view.cart_available = bag_->cart_available();
  StageCardInsert();
  // Les mots tenus par `ItemWords` suivent la langue avant que les fenêtres qui
  // les écrivent ne se composent.
  FollowMessageLanguage(pieces_.item_words, pieces_.item_words_language);
  StageWarpList(world_, pieces_.item_words,
                pieces_.map_display_names.has_value() ? &*pieces_.map_display_names : nullptr,
                windows_);
  // LA LISTE DES CHARRETTES QU'OUVRE `MC_CHANGECART`, dressée au niveau de base
  // du moment où la compétence part.
  if (grimoire_.TakeCartChoiceAsked()) {
    pieces_.cart_choice.choice().OfferChange(world_.player_status().base_level);
  }
  pieces_.cart_choice.Stage(vfs_, pieces_.item_words, windows_.cart_choice,
                            windows_.cart_choice_view);
  pieces_.spell_list.Stage(pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                           pieces_.skill_icons.has_value() ? &*pieces_.skill_icons : nullptr,
                           world_, windows_.spell_list, windows_.spell_list_view);
  pieces_.repair.Stage(world_, windows_.repair, windows_.repair_view);
  // LES TROIS COMPAGNONS QUI ONT UNE FICHE.
  pieces_.pet.Stage(*vfs_, world_, pieces_, windows_.pet, windows_.pet_view, windows_.pet_hatch,
                    windows_.pet_hatch_view);
  pieces_.homunculus.Stage(world_, pieces_);
  pieces_.mercenary.Stage(world_, pieces_);
  // LA SAISIE QU'OUVRE LA POSE DE TALKIE BOX : la compétence attend son texte.
  if (const std::optional<GroundCast> cast = grimoire_.TakeTalkboxAsked()) {
    pieces_.talkbox.Ask(*cast);
  }
  pieces_.talkbox.Stage(pieces_.item_words, windows_.talkbox, windows_.talkbox_view);
  // LES RÉGLAGES AVANCÉS D'UNE TACTIQUE DE COMPAGNON.
  pieces_.companion_ai.StageTacticWindow(
      pieces_.homunculus, pieces_.mercenary,
      pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr, windows_.companion_tactic,
      windows_.companion_tactic_view);
  // Ses distances s'arrêtent à la portée du jeu que le serveur annonce.
  windows_.companion_tactic_view.view_cells = world_.session().GameAreaCells();
  // LA FENÊTRE « IA DU COMPAGNON ».
  pieces_.companion_ai.StageAiWindow(
      pieces_.homunculus, pieces_.mercenary,
      CompanionAiWindowBooks{
          .world = world_,
          .skill_tree = pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
          .statuses = &pieces_.status_table,
          .words = &pieces_.item_words,
          .monster_sheets = monster_sheet_.has_value() ? &monster_sheet_->book() : nullptr,
          .monster_list = &pieces_.monster_list,
          .job_names = &pieces_.job_names},
      windows_.companion_ai, windows_.companion_ai_view);
  // Le droit du staff, relu à chaque image : l'identifiant d'un skill au survol.
  windows_.companion_ai_view.staff_allowed = Rights().staff_tools;
  // La case « Montrer les zones au sol », tenue par les réglages.
  windows_.companion_ai_view.ground_zones_shown =
      settings_->options().pages().companion_zones.kept_shown;
  // 🔴 LE COFFRE MANGE L'ÉCHANGE SAC <-> CHARRETTE, et le serveur le refuse
  // sans un mot (`pc_cant_act2` compte `storage_flag`). Les deux fenêtres
  // doivent donc le savoir : l'une grise son entrée, l'autre change de
  // destination.
  windows_.inventory_view.storage_open = world_.storage_open();
  windows_.cart_view.storage_open = world_.storage_open();
  // UNE ÉCHOPPE EN PRÉPARATION GÈLE LES TRANSFERTS : les trois fenêtres le
  // montrent, et `HoldTransfers` retient ce qui passerait quand même.
  const bool frozen = pieces_.trade.PreparingStore();
  windows_.inventory_view.transfers_frozen = frozen;
  windows_.cart_view.transfers_frozen = frozen;
  windows_.storage_view.transfers_frozen = frozen;
  cart_->Refresh(windows_.cart.open, world_, windows_.cart.tab, windows_.cart.filter);
  // 🔴 SA FENÊTRE SE REFERME QUAND LA CHARRETTE PART. Le serveur la reprend d'un
  // paquet -- une compétence qu'on retire, une monture qu'on descend --, et le
  // joueur la retrouverait sinon rouverte à la prochaine charrette.
  //
  // ⚠ AU DÉPART SEULEMENT, pas tant qu'il n'y en a pas : à l'entrée en jeu, la
  // charrette n'est pas encore annoncée, et l'ouverture que le profil de
  // disposition vient de poser doit l'attendre — le dessin la tient cachée.
  if (cart_->departed()) {
    windows_.cart.open = false;
  }
  // ⚠ CE N'EST PAS LE JOUEUR QUI OUVRE SON COFFRE : la fenêtre recopie ce que le
  // serveur dit de la session, et son nom avec — c'est la seule chose qui
  // sépare le coffre personnel de celui de la guilde.
  // ⚠ SES FAVORIS DESCENDENT AVEC : ils vivent dans les réglages, qui savent
  // les écrire, et la vue ne fait que les lire.
  storage_->Refresh(world_, windows_.storage.tab, windows_.storage.filter,
                    &settings_->storage_favorites(), windows_.storage.sub_category,
                    &settings_->storage_tab_names());
  windows_.storage.open = world_.storage_open();
  windows_.storage_view.name = storage_->name();
  // LES COFFRES QU'ON PEUT OUVRIR, et lequel l'est : la fenêtre en fait une
  // rangée, et c'est le monde qui les tient.
  windows_.storage_view.shelves = storage_->shelves();
  windows_.storage_view.sub_categories = storage_->sub_categories();
  windows_.storage_view.open_shelf = world_.open_storage_id();
  windows_.storage_view.cart_available = bag_->cart_available();
  // ⚠ NI CELLE-CI : la banque s'ouvre sur la RÉPONSE du serveur, qui bascule.
  // Voir `ro/net/bank.h` — il n'y a pas de « rafraîchir », redemander le solde
  // refermerait la fenêtre.
  windows_.bank.open = world_.bank().open();
  // L'ALBUM DE CARTES. ⚠ SA VUE NE SE COMPOSE QUE FENÊTRE OUVERTE : nommer neuf
  // cents cartes pour personne serait du gaspillage. Fermée, la fenêtre garde
  // la dernière — elle n'en lit rien, elle rend seulement son verrou.
  windows_.inventory_view.album_open = windows_.card_album.open;
  // « VOIR DANS L'ALBUM » au menu d'une carte, dans les trois contenants.
  const ro::ui::CardAlbumLinkFacts album_links = card_album_.LinkFacts(link_.is_open());
  windows_.inventory_view.album_links = album_links;
  windows_.cart_view.album_links = album_links;
  windows_.storage_view.album_links = album_links;
  // LE COURRIER EN COURS D'ÉCRITURE : une pièce jointe peut partir du menu.
  windows_.inventory_view.mail_writing = pieces_.mail.writing();
  // LE BOUTON « Vider » DE LA BARRE DE TITRE, pour le staff seulement.
  windows_.inventory_view.staff_tools = Rights().staff_tools;
  if (windows_.card_album.open) {
    card_album_.Compose(
        CardAlbumPieces{
            .items = pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
            .icons = pieces_.item_icons.has_value() ? &*pieces_.item_icons : nullptr,
            .thumbnails = pieces_.card_thumbnails.has_value() ? &*pieces_.card_thumbnails : nullptr,
            .packages = &pieces_.package_items},
        world_, windows_.card_album, Rights().staff_tools);
  }
  windows_.card_album_view = &card_album_.view();
}

void Game::ApplyContainerAnswers(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // UNE ÉCHOPPE EN PRÉPARATION GÈLE LES TRANSFERTS : voir `HoldTransfers`. Le
  // refus se dit, sans quoi le geste passerait pour une fenêtre cassée.
  ro::ui::InventoryWindowAction bag_asked = answers.inventory;
  ro::ui::CartWindowAction cart_asked = answers.cart;
  ro::ui::StorageWindowAction storage_asked = answers.storage;
  if (pieces_.trade.PreparingStore() && HoldTransfers(bag_asked, cart_asked, storage_asked)) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0,
                      ro::core::Tr("Échoppe en préparation : les transferts sont gelés."), now_ms);
  }
  ApplyItemTransfer(ReceivedTransfer(bag_asked, cart_asked, storage_asked));
  bag_->Apply(bag_asked, link_, world_);
  PlaceDraggedItem(answers.inventory);
  // ── LÂCHER UN OBJET SUR LE MONDE, C'EST LE JETER ────────────────────────────
  //
  // C'est le geste du client, et c'est l'autre bout de celui qui range : on tire
  // un objet vers une fenêtre pour l'y mettre, vers le SOL pour s'en défaire.
  //
  // ⚠ CE QU'ON PORTE NE SE JETTE PAS : le serveur refuse — `pc_dropitem` regarde
  // le drapeau d'équipement —, et le menu ne le propose pas non plus.
  //
  // ⚠ ET UNE PILE DEMANDE COMBIEN, comme au menu. La question se pose à l'image
  // suivante, par le même souvenir : `app` ne dessine rien, il arme.
  //
  // ⚠ SOUS UNE SOURIS FIGÉE, LE SOL REFUSE L'OBJET : voir `WorldTakesClicks`.
  if (answers.dropped_on_nothing && !answers.dropped_item.worn && !windows_.inventory.drop_locked &&
      WorldTakesClicks(MouseRouteOf(world_, grimoire_, world_mouse_.engagement, now_ms))) {
    const std::uint16_t slot = answers.dropped_item.client_slot;
    const ro::sim::InventoryItem* const item = world_.inventory().Find(slot);
    if (item != nullptr && item->amount > 1) {
      windows_.inventory.asking_slot = slot;
      windows_.inventory.asked_amount = 1;
      windows_.inventory.asking_verb = ro::ui::ItemVerb::kDrop;
    } else if (item != nullptr) {
      ro::ui::InventoryWindowAction thrown;
      thrown.verb = ro::ui::ItemVerb::kDrop;
      thrown.slot = slot;
      thrown.amount = 1;
      bag_->Apply(thrown, link_, world_);
    }
  }
  cart_->Apply(cart_asked, link_, world_);
  // UN COFFRE REFERMÉ PAR ÉCHAP répond comme par sa croix : la pile n'a fait
  // que baisser l'interrupteur, que la session du serveur relèvera à l'image
  // suivante tant qu'on ne lui a rien dit.
  if (world_.storage_open() && !windows_.storage.open &&
      storage_asked.verb == ro::ui::StorageVerb::kNothing) {
    storage_asked.verb = ro::ui::StorageVerb::kClose;
  }
  storage_->Apply(storage_asked, link_, world_);
  // LE FAVORI D'UN OBJET DE COFFRE : un geste qui ne parle pas au serveur — il
  // n'en connaît aucun ici — et que les réglages écrivent dans leur fichier.
  if (answers.storage.toggle_favorite_id != 0) {
    settings_->ToggleStorageFavorite(answers.storage.toggle_favorite_id);
  }
  // LE NOM ET L'ICÔNE D'UN COFFRE : deux gestes qui ne parlent pas au serveur.
  // ⚠ LE NOM SE LIT DANS LE TAMPON DE LA FENÊTRE, où la saisie vit : l'action
  // ne dit que LEQUEL des coffres vient d'être renommé.
  if (answers.storage.renamed_shelf >= 0) {
    settings_->NameStorageShelf(static_cast<std::uint8_t>(answers.storage.renamed_shelf),
                                windows_.storage.shelf_name);
  }
  if (answers.storage.decorated_shelf >= 0) {
    settings_->DecorateStorageShelf(static_cast<std::uint8_t>(answers.storage.decorated_shelf),
                                    answers.storage.decorate_item_id);
  }
  if (answers.storage.reset_shelf >= 0) {
    settings_->ForgetStorageShelf(static_cast<std::uint8_t>(answers.storage.reset_shelf));
  }
  // LE RETRAIT N'AVAIT NULLE PART OÙ ALLER. ⚠ DIT DANS LA CONVERSATION, là où le
  // joueur lit déjà tous les refus : un clic qui ne fait rien et ne dit rien
  // passe pour une fenêtre cassée.
  if (answers.storage.nowhere_to_take) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0,
                      ro::core::Tr("Ouvrez l'inventory ou le cart pour y retirer un item."),
                      now_ms);
  }
  // ⚠ LE MÊME GESTE DEPUIS DEUX FENÊTRES, et il ne part qu'une fois : les deux
  // raccourcis disent la même chose, et le joueur n'a qu'une souris.
  if (answers.inventory.toggle_storage || answers.cart.toggle_storage) {
    storage_->ToggleSession(link_, world_);
  }
  // LES RACCOURCIS VERS L'AUTRE FENÊTRE. ⚠ CEUX-LÀ NE PARLENT PAS AU SERVEUR :
  // le sac et la charrette sont des vues de ce que le monde porte déjà, et le
  // bouton ne fait qu'ouvrir ou refermer la vue. Seul le coffre est une session.
  //
  // ⚠ ET LE MÊME GESTE VIENT DE DEUX FENÊTRES, mais ne bascule qu'une fois :
  // deux raccourcis mènent au même endroit, et deux bascules dans la même image
  // se seraient annulées.
  if (answers.inventory.toggle_cart || answers.storage.toggle_cart) {
    windows_.cart.open = !windows_.cart.open;
  }
  // Le « Vider » du staff : le droit se relit ici, au geste.
  if (answers.inventory.staff_wipe) {
    SendStaffItemReset(Rights(), link_, world_);
  }
  if (answers.cart.toggle_inventory || answers.storage.toggle_inventory) {
    windows_.inventory.open = !windows_.inventory.open;
  }
  // L'ALBUM DE CARTES : ce qu'il a demandé au serveur et au fichier du staff,
  // ce que le sac lui offre, et ce qui reste à la partie — décrire, la base de
  // données du site, ses réglages.
  const ro::ui::CardAlbumWindowAction& asked = answers.card_album;
  card_album_.Apply(asked, link_, now_ms);
  // ⚠ LE SAC OFFRE, L'ALBUM DÉCIDE — dépôt ou sacrifice — à l'image suivante :
  // lui seul sait si la pochette est ouverte, et lui seul demande combien.
  if (answers.inventory.to_album_slot != 0) {
    windows_.card_album.offered_slot = answers.inventory.to_album_slot;
  }
  if (answers.inventory.album_withdraw != 0) {
    windows_.card_album.withdraw_asked = answers.inventory.album_withdraw;
  }
  for (const std::uint32_t card_id :
       {answers.inventory.see_in_album_id, answers.cart.see_in_album_id,
        answers.storage.see_in_album_id}) {
    ShowCardInAlbum(card_id, now_ms);
  }
  if (answers.inventory.market_item_id != 0) {
    pieces_.trade.market().AskOpen(answers.inventory.market_item_id);
  }
  if (answers.inventory.database_item_id != 0) {
    database_.AskOpen(ro::ui::DatabaseTarget{.kind = ro::ui::DatabaseKind::kItem,
                                             .id = answers.inventory.database_item_id});
  }
  if (asked.describe_id != 0) {
    ro::ui::OpenItemDescription(windows_.description, asked.describe_id, 0,
                                ro::ui::ItemHolder::kInventory);
  }
  if (asked.database_id != 0) {
    FollowDescriptionLink(
        ro::ui::DescriptionWindowAction{.followed = ro::vfs::ItemDatabaseUrl(asked.database_id)},
        now_ms);
  }
  if (asked.settings_asked) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kCardAlbum);
  }
}

void Game::ApplyContainerSwitches(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // LA PUCE D'UNE BARRE DE TITRE : elle mène aux réglages de SA fenêtre.
  //
  // ⚠ UNE SEULE PAR IMAGE, et l'ordre ne décide de rien : le joueur n'a qu'une
  // souris, et deux fenêtres ne reçoivent pas deux clics dans la même image.
  if (answers.inventory.settings_asked) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kInventory);
  }
  // LE VERROU DE JET DU PIED. ⚠ L'ÉTAT DE LA FENÊTRE EST LA VALEUR COURANTE :
  // les réglages viennent d'y descendre à `Refresh`, et c'est donc elle qu'on
  // renverse.
  if (answers.inventory.toggle_drop_lock) {
    settings_->SetSwitch(GameOption::kInventoryDropLock, !windows_.inventory.drop_locked,
                         OptionsTargets(), now_ms);
  }
  if (answers.inventory.toggle_sell_lock) {
    settings_->SetSwitch(GameOption::kInventorySellLock, !windows_.inventory.sell_locked,
                         OptionsTargets(), now_ms);
  }
  if (answers.inventory.toggle_sort) {
    settings_->SetSwitch(GameOption::kInventorySort, !windows_.inventory.sorted, OptionsTargets(),
                         now_ms);
  }
  if (answers.cart.settings_asked) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kCart);
  }
  if (answers.storage.settings_asked) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kStorage);
  }
}

void Game::ApplyBankAnswers(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // LE SAC DE ZENY DU PIED : la même bascule que le raccourci.
  if (answers.inventory.toggle_bank) {
    bank_.ToggleSession(link_, world_, now_ms);
  }
  // CE QUE LA BANQUE A DEMANDÉ, puis ce que le serveur a répondu.
  //
  // ⚠ LE VERDICT SE LIT À CHAQUE IMAGE, banque fermée comprise : un refus
  // arrive aussi sur une demande d'ouverture qui n'ouvre rien.
  bank_.Apply(answers.bank, windows_.bank, link_, world_);
  bank_.NoteVerdict(world_, now_ms);
}

}  // namespace moonclient
