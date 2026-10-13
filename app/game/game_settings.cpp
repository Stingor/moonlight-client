#include "game/game_settings.h"

#include <utility>

#include "ro/core/i18n.h"

#include "game/hotkey_bug_report.h"
#include "game/options_bug_report.h"
#include "input/hotkey_file.h"
#include "items/item_layout_file.h"

namespace moonclient {

GameSettings::GameSettings(std::filesystem::path client_root)
    : client_root_(std::move(client_root)),
      file_(client_root_ / kSaveFolder / kSettingsFileName),
      profiles_(client_root_, file_, options_, bindings_) {
  // ⚠ LES TOUCHES D'AVANT L'ENTRÉE EN JEU : celles de `hotkeys.yml`. Le profil
  // du personnage pose ensuite les siennes. Voir `SettingsProfilePort`.
  LoadHotkeys(client_root_, bindings_);
  // ⚠ LUE MÊME QUAND LE PLACEMENT LIBRE EST ÉTEINT : le réglage se rallume sans
  // redémarrer, et la disposition doit être là quand il le fait.
  LoadItemLayout(client_root_, item_layout_);
  LoadStorageFavorites(client_root_, storage_favorites_);
  LoadStorageTabNames(client_root_, storage_tab_names_);

  // ⚠ DÉCLARER PUIS LIRE, DANS CET ORDRE : la valeur présente au moment de la
  // déclaration est ce qui sert de défaut.
  options_.Declare(file_);
  LoadBlockedNpcs(client_root_, options_.context_menu().blocked());
  file_.Section("La caméra du joueur");
  file_.Declare("camera_outside_distance", &camera_memory_.outdoor.zoom);
  file_.Declare("camera_outside_altitude", &camera_memory_.outdoor.altitude_degrees);
  file_.Declare("camera_inside_distance", &camera_memory_.indoor.zoom);
  file_.Declare("camera_inside_altitude", &camera_memory_.indoor.altitude_degrees);
  file_.Load();
  profiles_.Start();
}

void GameSettings::NoteCameraMemory(const CameraMemory& seen) {
  // Le lacet n'est pas gardé : il ne doit pas relancer l'écriture.
  const auto same = [](const NativeView& kept, const NativeView& now) {
    return kept.zoom == now.zoom && kept.altitude_degrees == now.altitude_degrees;
  };
  if (same(camera_memory_.outdoor, seen.outdoor) && same(camera_memory_.indoor, seen.indoor)) {
    return;
  }
  camera_memory_ = seen;
  file_.Touch();
}

void GameSettings::ApplyOptions(const OptionTargets& to) { options_.ApplyAll(to); }

void GameSettings::PlaceItem(std::uint32_t item_id, int cell, std::uint32_t sitting_there) {
  item_layout_.Place(item_id, cell, sitting_there);
  // ⚠ ÉCRIT TOUT DE SUITE, à la différence des réglages : un lâcher est un geste
  // isolé — il n'y a pas de rafale à amortir, et l'anti-rebond du fichier de
  // réglages ne commande pas celui-ci.
  static_cast<void>(SaveItemLayout(client_root_, item_layout_));
}

void GameSettings::ToggleStorageFavorite(std::uint32_t item_id) {
  storage_favorites_.Toggle(item_id);
  static_cast<void>(SaveStorageFavorites(client_root_, storage_favorites_));
}

bool GameSettings::ToggleBlockedNpc(std::uint32_t id, std::string_view name) {
  const bool blocked = options_.context_menu().blocked().Toggle(id, name);
  static_cast<void>(SaveBlockedNpcs(client_root_, options_.context_menu().blocked()));
  return blocked;
}

void GameSettings::NameStorageShelf(std::uint8_t storage_id, std::string_view name) {
  storage_tab_names_.Name(storage_id, name);
  static_cast<void>(SaveStorageTabNames(client_root_, storage_tab_names_));
}

void GameSettings::DecorateStorageShelf(std::uint8_t storage_id, std::uint32_t item_id) {
  storage_tab_names_.Decorate(storage_id, item_id);
  static_cast<void>(SaveStorageTabNames(client_root_, storage_tab_names_));
}

// 🔴 UN SEUL ENREGISTREMENT POUR LES DEUX, et c'est la raison d'être de cette
// fonction : nommer puis décorer aurait écrit le fichier deux fois pour un seul
// geste, et laissé une fenêtre — entre les deux — où le coffre a perdu son nom
// mais garde son icône.
void GameSettings::ForgetStorageShelf(std::uint8_t storage_id) {
  storage_tab_names_.Name(storage_id, "");
  storage_tab_names_.Decorate(storage_id, 0);
  static_cast<void>(SaveStorageTabNames(client_root_, storage_tab_names_));
}

void GameSettings::ForgetItemPlaces() {
  item_layout_.Clear();
  static_cast<void>(SaveItemLayout(client_root_, item_layout_));
}

void GameSettings::Refresh(UiStage::Windows& windows, std::uint32_t now_ms) {
  // ⚠ AVANT LA COMPOSITION : la page de l'album écrit la touche qui l'ouvre, et
  // elle ne connaît pas les raccourcis.
  options_.card_album().NoteOpenKey(LabelOfBinding(bindings_.Of(GameAction::kOpenCardAlbum)));
  options_.pages().mvp_tracker.NoteOpenKey(
      LabelOfBinding(bindings_.Of(GameAction::kOpenMvpTracker)));
  options_.Refresh(windows.options.open, windows.options.section_tab,
                   windows.options.filter.front() != '\0');
  windows.card_album.look = options_.card_album().look();
  windows.mvp_tracker.window.look = options_.pages().mvp_tracker.look();
  windows.menu_icons.look = &options_.pages().menu_icons.look();
  // ⚠ LES RÉGLAGES DESCENDENT DANS L'ÉTAT DE LA FENÊTRE, jamais l'inverse :
  // `ui` ne lit aucun fichier et ne connaît pas les options.
  windows.inventory.filter_shown = options_.inventory_filter_shown();
  windows.inventory.size_locked = options_.inventory_size_locked();
  windows.inventory.drop_locked = options_.inventory_drop_locked();
  windows.inventory.sell_locked = options_.inventory_sell_locked();
  windows.inventory.sorted = options_.inventory_sorted();
  windows.trade.npc_shop.state.confirm_sell_all = options_.shops().confirm_sell_all();
  windows.cart.filter_shown = options_.cart_filter_shown();
  windows.cart.size_locked = options_.cart_size_locked();
  windows.storage.filter_shown = options_.storage_filter_shown();
  // LA DISPOSITION DES ONGLETS, ET CE QUE LE COFFRE MONTRE AUTOUR DE SA TABLE.
  windows.inventory.tabs_vertical = options_.inventory_tabs_vertical();
  // 🔴 TROIS CONDITIONS, ET ELLES SE DISENT ICI, UNE FOIS. La fenêtre ne connaît
  // ni le verrou de taille — le lecteur des options le lui a déjà retranché — ni
  // la règle de l'onglet « Tout », qui mêle les familles : une case n'y désigne
  // pas la même place que dans l'onglet d'origine de l'objet, donc y ranger
  // reviendrait à défaire ce qu'on a rangé ailleurs.
  //
  // ⚠ ET C'EST CE DRAPEAU QUI OUVRE LES CIBLES DE LÂCHER de la grille : le
  // laisser vrai sur « Tout » aurait écrit dans la table des cases calculées sur
  // un affichage qui n'est pas le sien.
  windows.inventory.free_layout =
      options_.inventory_free_layout() && windows.inventory.tab != ro::ui::ItemTab::kAll;
  windows.cart.tabs_vertical = options_.cart_tabs_vertical();
  windows.storage.tabs_vertical = options_.storage_tabs_vertical();
  windows.storage.type_tabs = options_.storage_type_tabs();
  windows.storage.tab_images = options_.storage_tab_images();
  windows.storage.shelf_row = options_.storage_shelf_row();
  windows.storage.slot_column = options_.storage_slot_column();
  windows.storage.item_id_column = options_.storage_item_id_column();
  windows.storage.card_slots_column = options_.storage_card_slots_column();
  windows.storage.value_column = options_.storage_value_column();
  windows.storage.total_value = options_.storage_total_value();
  windows.inventory.storage_button = options_.inventory_storage_button();
  windows.cart.storage_button = options_.cart_storage_button();
  // ⚠ CHAQUE FENÊTRE SAIT SI L'AUTRE EST OUVERTE, et c'est l'étage qui le lui
  // dit : une fenêtre ne connaît pas ses sœurs, et son raccourci doit pourtant
  // annoncer si le clic ouvre ou referme.
  windows.inventory_view.cart_open = windows.cart.open;
  windows.cart_view.inventory_open = windows.inventory.open;
  windows.storage.peer_buttons = options_.storage_peer_buttons();
  // ⚠ UN RÉGLAGE PAR FENÊTRE : on RANGE son sac et l'on CHERCHE dans son coffre,
  // et ces deux gestes ne veulent pas la même infobulle.
  windows.inventory.hover_preview = options_.inventory_hover_description();
  windows.cart.hover_preview = options_.cart_hover_description();
  windows.storage.hover_preview = options_.storage_hover_description();
  windows.character.hover_preview = options_.character_hover_description();
  // ⚠ DESCENDU AVANT LE DESSIN, remonté après par `NoteGrimoireIcons` : la copie
  // posée ici est celle que l'en-tête du grimoire cochera dans la même image.
  windows.character.grimoire.smooth_icons = options_.grimoire_smooth_icons();
  windows.character.grimoire.keyboard_layout = options_.pages().grimoire_keys.keyboard_layout();
  windows.character.grimoire.grid = options_.pages().skill_views.grimoire_grid();
  windows.character.guild.skill_grid = options_.pages().skill_views.guild_grid();
  // 🔴 LE SOUVENIR DU VOLET DU STAFF, et il ne peut PAS vivre dans `imgui.ini`.
  // Le droit n'arrive qu'après l'entrée sur la carte : les premières images
  // posent un plafond de largeur SANS volet, ImGui y rabote la fenêtre
  // restaurée, puis réécrit la largeur rabotée. Le souvenir doit donc être à
  // nous, et reposer le cran quand le droit paraît. Voir `staff_wanted`.
  windows.character.staff_wanted = options_.character_staff_pane();
  // ⚠ CE QU'ON REGARDAIT LA DERNIÈRE FOIS : la pose de l'avatar et sa
  // direction descendent ici et remontent après le dessin, comme les icônes du
  // grimoire. C'est la molette et la liste qui les changent, pas un panneau.
  windows.character.avatar.pose = options_.character_avatar_pose();
  windows.character.avatar.animated = options_.character_avatar_animated();
  windows.character.avatar.direction = options_.character_avatar_direction();
  windows.character.avatar.gif_scale = options_.character_avatar_gif_scale();
  // ⚠ LE SOL UNI DESCEND AVANT LE DESSIN, et il remonte après : voir
  // `NoteStaffTools`. L'ordre est ce qui rend les deux sens compatibles — la
  // copie posée ici est celle que l'établi éditera dans la même image.
  windows.staff_tools.look = options_.staff_tools();
  windows.staff_tools.quick_cast = options_.quick_cast().look();
  // LES LIBELLÉS DE TOUCHE DES MACROS, composés seulement quand leur fenêtre
  // est ouverte : dix compositions par image pour une fenêtre fermée seraient
  // du temps donné à rien.
  if (windows.macros.open) {
    for (std::size_t slot = 0; slot < windows.macro_keys.size(); ++slot) {
      windows.macro_keys[slot] = LabelOfBinding(bindings_.Of(ActionRef::OfMacro(slot)));
    }
  }
  // ⚠ ET LE CATALOGUE DU CLIC DROIT, à la même condition et pour la même
  // raison : composer une trentaine de phrases pour une fenêtre fermée serait
  // du temps donné à rien. Il ne se refait de toute façon qu'au changement de
  // langue.
  if (windows.macros.open) {
    macro_commands_.Refresh();
  }
  windows.storage_view.inventory_open = windows.inventory.open;
  windows.storage_view.cart_open = windows.cart.open;
  windows.meter.shown = options_.frame_meter();
  windows.meter.locked = options_.frame_meter_locked();
  windows.meter.graph = options_.frame_meter_graph();
  windows.meter.ping = options_.frame_meter_ping();
  // ⚠ CE QUE LE FILET MONTRE, posé comme les autres réglages : `ui` ne tient
  // aucune horloge et ne sait pas défaire.
  windows.options.revert_seconds = options_.revert_seconds(now_ms);
  if (!windows.hotkeys.open) {
    return;
  }
  const std::span<const ActionRef> actions = AllBindableActions();
  keys_.clear();
  keys_.reserve(actions.size());
  names_.clear();
  names_.reserve(actions.size());
  groups_.clear();
  groups_.reserve(actions.size());
  for (const ActionRef action : actions) {
    keys_.push_back(LabelOfBinding(bindings_.Of(action)));
    // ⚠ LE NOM SE COMPOSE MAINTENANT — « Barre 3, case 7 » — donc il se range
    // dans un tampon, comme le libellé de touche. Quatre-vingt-seize littéraux
    // n'auraient été ni traduisibles ni tenables.
    names_.push_back(NameOfAction(action));
    groups_.push_back(GroupOfAction(action));
  }
  // ⚠ LES VUES SE POSENT APRÈS, quand les tampons ne bougent plus : une vue
  // prise en cours de remplissage pendrait à la première réallocation.
  rows_.clear();
  rows_.reserve(actions.size());
  for (std::size_t rank = 0; rank < actions.size(); ++rank) {
    rows_.push_back(ro::ui::HotkeyRowView{.action = names_[rank],
                                          .key = keys_[rank],
                                          .group = groups_[rank],
                                          .tab = static_cast<int>(TabOfAction(actions[rank])),
                                          .hidden = !ActionInThisBuild(actions[rank]),
                                          .capture_hint = CaptureHintOfAction(actions[rank])});
  }
  for (std::size_t tab = 0; tab < kHotkeyTabCount; ++tab) {
    tab_names_[tab] = NameOfTab(static_cast<HotkeyTab>(tab));
  }
}

void GameSettings::OpenOptionsAt(UiStage::Windows& windows, OptionSection section, int row) {
  windows.options.open = true;
  windows.options.row_wanted = row;
  // ⚠ LE FILTRE SE VIDE : sous un mot tapé plus tôt, la page de la fenêtre d'où
  // l'on vient ne se montrerait peut-être pas.
  windows.options.filter.front() = '\0';
  // ⚠ UNE AUTRE PAGE REPART DE SON PREMIER ONGLET, comme par la navigation :
  // le rang d'onglet de la page qu'on quitte ne désigne rien dans celle-ci.
  if (windows.options.section != static_cast<int>(Rank(section))) {
    windows.options.section_tab = 0;
    windows.options.section_tab_wanted = 0;
  }
  windows.options.section = static_cast<int>(Rank(section));
}

void GameSettings::NoteWindowPlace(const OptionTargets& to) {
  if (to.display == nullptr) {
    return;
  }
  // ⚠ RIEN EN PLEIN ÉCRAN : la position y est celle que le système impose, et
  // la retenir ferait ressortir la fenêtre collée dans un coin.
  if (options_.display_mode() != 0) {
    return;
  }
  if (options_.NoteWindowMoved(*to.display)) {
    file_.Touch();
  }
}

void GameSettings::RestoreChatMacros(UiStage::Windows& windows) const {
  const std::span<const std::string> saved = options_.chat_macros();
  for (std::size_t slot = 0; slot < windows.macros.rows.size() && slot < saved.size(); ++slot) {
    // ⚠ TRONQUÉE PLUTÔT QUE REFUSÉE : un fichier retouché à la main peut porter
    // plus long que le tampon, et perdre la ligne entière serait pire que d'en
    // perdre la fin. `std::snprintf` termine toujours par un zéro.
    std::snprintf(windows.macros.rows[slot].data(), windows.macros.rows[slot].size(), "%s",
                  saved[slot].c_str());
  }
}

void GameSettings::NoteChatMacros(const UiStage::Windows& windows) {
  // ⚠ LES TAMPONS SONT LA SOURCE PENDANT LA PARTIE : on les remonte, on ne les
  // redescend pas. Les redescendre à chaque image écraserait la frappe en
  // cours, parce qu'un `InputText` édite le tampon qu'on lui a donné.
  std::array<std::string, ro::ui::kChatMacroCount> seen;
  for (std::size_t slot = 0; slot < seen.size(); ++slot) {
    seen[slot] = windows.macros.rows[slot].data();
  }
  if (options_.NoteChatMacros(seen)) {
    file_.Touch();
  }
}

void GameSettings::NoteGrimoireIcons(const UiStage::Windows& windows) {
  if (options_.NoteGrimoireSmoothIcons(windows.character.grimoire.smooth_icons)) {
    file_.Touch();
  }
  if (options_.pages().grimoire_keys.NoteKeyboardLayout(
          windows.character.grimoire.keyboard_layout)) {
    file_.Touch();
  }
  if (options_.pages().skill_views.NoteGrimoireGrid(windows.character.grimoire.grid)) {
    file_.Touch();
  }
  if (options_.pages().skill_views.NoteGuildGrid(windows.character.guild.skill_grid)) {
    file_.Touch();
  }
}

void GameSettings::NoteCharacterStaffPane(const UiStage::Windows& windows) {
  // ⚠ SEULEMENT QUAND LE DROIT EST LÀ. Sans lui le volet est caché par force, et
  // enregistrer ce « caché » effacerait le souvenir de qui l'avait ouvert — ce
  // qui arrive à CHAQUE démarrage, avant que le serveur n'ait dit le niveau.
  if (!windows.character_view.staff_allowed) {
    return;
  }
  if (options_.NoteCharacterStaffPane(windows.character.staff_shown)) {
    file_.Touch();
  }
}

void GameSettings::NoteStaffBattleIgnore(bool wanted) {
  if (options_.NoteStaffBattleIgnore(wanted)) {
    file_.Touch();
  }
}

void GameSettings::NoteCharacterAvatar(const UiStage::Windows& windows) {
  if (options_.NoteCharacterAvatar(windows.character.avatar)) {
    file_.Touch();
  }
}

void GameSettings::NoteStaffTools(const UiStage::Windows& windows) {
  if (options_.NoteStaffTools(windows.staff_tools.look)) {
    file_.Touch();
  }
  // Quick cast, que l'établi montre aussi : ses réglages vivent dans le panneau.
  if (options_.quick_cast().Note(windows.staff_tools.quick_cast)) {
    file_.Touch();
  }
}

void GameSettings::NoteItemObtainAnchor(int pos_x, int pos_y) {
  options_.NoteItemObtainAnchor(pos_x, pos_y);
  file_.Touch();
}

void GameSettings::NoteBalloonAnchor(bool hunts, const ro::ui::BalloonAnchorAsk& asked) {
  options_.pages().balloons.NoteAnchor(hunts, asked);
  file_.Touch();
}

void GameSettings::TickDisplayRevert(const OptionTargets& to, std::uint32_t now_ms) {
  if (options_.revert_seconds(now_ms) == 0) {
    options_.RevertDisplay(to);
  }
}

void GameSettings::Apply(const UiStage::Answers& answers, const OptionTargets& to,
                         UiStage::Windows& windows, bool* running, std::uint32_t now_ms) {
  switch (answers.menu) {
    case ro::ui::GameMenuChoice::kNothing:
      break;
    case ro::ui::GameMenuChoice::kBackToGame:
      windows.menu.open = false;
      break;
    case ro::ui::GameMenuChoice::kOptions:
      windows.options.open = true;
      // Le menu se referme : on l'a ouvert pour aller ailleurs.
      windows.menu.open = false;
      break;
    case ro::ui::GameMenuChoice::kShortcuts:
      windows.hotkeys.open = true;
      windows.menu.open = false;
      break;
    case ro::ui::GameMenuChoice::kChatMacros:
      windows.macros.open = true;
      windows.menu.open = false;
      break;
    case ro::ui::GameMenuChoice::kStyleEditor:
      windows.style.open = true;
      windows.menu.open = false;
      break;
    case ro::ui::GameMenuChoice::kStaffTools:
      windows.staff_tools.open = true;
      windows.menu.open = false;
      break;
    // ⚠ LES TROIS GESTES QUI PARLENT AU SERVEUR NE SONT PAS ICI. Ils demandent
    // la connexion ou le routage de la conversation, que les réglages ne
    // possèdent pas — c'est la boucle qui les joue, juste après.
    case ro::ui::GameMenuChoice::kSavePointCommand:
    case ro::ui::GameMenuChoice::kRespawn:
    case ro::ui::GameMenuChoice::kResurrect:
    case ro::ui::GameMenuChoice::kCharacterSelect:
      windows.menu.open = false;
      break;
    case ro::ui::GameMenuChoice::kQuit:
      if (running != nullptr) {
        *running = false;
      }
      break;
  }

  const std::span<const ActionRef> actions = AllBindableActions();
  // « SIGNALER UN BUG » ⚠ AVANT TOUT CHANGEMENT : le rapport décrit ce que la
  // fenêtre montrait, pas ce que les demandes de cette image en font.
  if (answers.hotkeys.report_bug) {
    std::vector<std::string> action_keys;
    action_keys.reserve(actions.size());
    for (const ActionRef action : actions) {
      action_keys.push_back(SavedKeyOfAction(action));
    }
    AskBugReport(windows.bug_report, HotkeyBugReport(HotkeyBugScene{.window = &windows.hotkeys,
                                                                    .view = hotkey_view(),
                                                                    .action_keys = action_keys}));
  }
  if (answers.options.report_bug) {
    AskBugReport(windows.bug_report,
                 OptionsBugReport(
                     OptionsBugScene{.window = &windows.options, .sections = options_.sections()}));
  }
  if (answers.hotkeys.cleared >= 0 &&
      static_cast<std::size_t>(answers.hotkeys.cleared) < actions.size()) {
    // Retirer une touche ne peut la prendre à personne : rien à annoncer.
    static_cast<void>(
        bindings_.Bind(actions[static_cast<std::size_t>(answers.hotkeys.cleared)], Binding{}));
    windows.hotkeys.robbed[0] = '\0';
  }
  if (answers.hotkeys.restore_defaults) {
    bindings_.RestoreDefaults();
    windows.hotkeys.robbed[0] = '\0';
  }
  // ⚠ RIEN À ÉCRIRE ICI : la table fait partie du profil de réglages, dont le
  // gardien voit grandir la révision. Voir `SettingsProfilePort`.

  // Les options s'enregistrent elles-mêmes : le fichier fait partie de ce
  // qu'elles touchent. Voir `OptionTargets`.
  //
  // ⚠ LES DEUX PANNEAUX PASSENT PAR LE MÊME CHEMIN, et c'est toute la raison
  // du partage en deux GROUPES plutôt qu'en deux jeux de réglages : un
  // réglage n'existe qu'à un seul endroit, quel que soit le panneau qui le
  // montre.
  if (answers.options.keep_display) {
    options_.KeepDisplay();
  }
  if (answers.options.revert_display) {
    options_.RevertDisplay(to);
  }
  options_.Change(answers.options, to, now_ms);
  // ⚠ UN PNJ DÉBLOQUÉ DEPUIS LA PAGE S'ÉCRIT TOUT DE SUITE, comme un favori du
  // coffre : c'est une liste, dans son propre fichier.
  if (options_.context_menu().TakeBlockedChanged()) {
    static_cast<void>(SaveBlockedNpcs(client_root_, options_.context_menu().blocked()));
  }
}

void GameSettings::SetSwitch(GameOption which, bool on, const OptionTargets& to,
                             std::uint32_t now_ms) {
  options_.Change(ro::ui::OptionsWindowAction{.changed = static_cast<int>(which), .on = on}, to,
                  now_ms);
}

void GameSettings::SetRowSwitch(int row_id, bool on, const OptionTargets& to,
                                std::uint32_t now_ms) {
  options_.Change(ro::ui::OptionsWindowAction{.changed = row_id, .on = on}, to, now_ms);
}

void GameSettings::SetValue(GameOption which, float value, const OptionTargets& to,
                            std::uint32_t now_ms) {
  options_.Change(ro::ui::OptionsWindowAction{.changed = static_cast<int>(which), .value = value},
                  to, now_ms);
}

void GameSettings::Tick(std::uint32_t now_ms) {
  file_.Tick(now_ms);
  options_.pages().skin_look.Tick(now_ms);
}

void GameSettings::Flush() {
  profiles_.Flush();
  file_.Flush();
  options_.pages().skin_look.Flush();
}

}  // namespace moonclient
