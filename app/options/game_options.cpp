// CE QU'UN RÉGLAGE TOUCHE : ce qu'il pousse dans le moteur, et ce qu'il écrit.
//
// 🔴 CE FICHIER SE LIT DE BOUT EN BOUT, contrairement au catalogue des clés d'à
// côté : un `switch` dont un cas tombe dans le suivant s'y voit. Les clés et les
// rangées, qui ne se lisent qu'une ligne à la fois, vivent ailleurs.

#include "options/game_options.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/ui/system_fonts.h"
#include "ro/ui/ui_context.h"

#include "chat/chat_history_file.h"
#include "options/bubble_text_options.h"
#include "options/display_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {

void GameOptions::Record(const OptionTargets& to) {
  if (to.file != nullptr) {
    to.file->Touch();
  }
}

std::span<const ro::ui::OptionSectionView> GameOptions::sections() const noexcept {
  return sections_;
}

void GameOptions::Change(const ro::ui::OptionsWindowAction& asked, const OptionTargets& to,
                         std::uint32_t now_ms) {
  if (asked.restore_defaults) {
    // ⚠ LE FICHIER TIENT LES DÉFAUTS, pas nous : ce sont les valeurs qu'avaient
    // ces champs à la déclaration. Les recopier ici en ferait une seconde
    // source, et les deux finiraient par diverger.
    if (to.file != nullptr) {
      to.file->RestoreDefaults();
    }
    ApplyAll(to);
    Record(to);
    return;
  }
  // UNE PAGE SEULE. ⚠ LA MACHINE (écran, son) NE SE REPOUSSE QUE SI ELLE EN PORTE.
  if (asked.restore_section >= 0 && to.file != nullptr) {
    pages_.RestorePage(asked.restore_section, *to.file, PageRestore::kWhole);
    ApplyAll(to, to.file->PageHoldsMachine(asked.restore_section));
    Record(to);
    return;
  }
  // LES BARRES DE RACCOURCIS AVANT L'AIGUILLAGE : leurs lignes portent un
  // identifiant COMPOSÉ, hors de l'énumération, et c'est la fiche 0038 qui le
  // veut — quarante entrées nommées pour huit fois le même réglage.
  // ⚠ ET CHACUNE ENREGISTRE. Une couleur de jauge changée ne touchait pas le
  // fichier : elle ne survivait à la session que si un AUTRE réglage l'avait
  // touché entre-temps — ce qui arrive souvent, et masque exactement ce
  // genre d'oubli.
  if (ChangeStatBar(asked)) {
    Record(to);
    return;
  }
  if (pages_.ChangeServerHeld(asked)) {  // tenu par le serveur : rien à écrire, une demande
    return;
  }
  // CHAQUE PAGE PREND CE QUI EST À ELLE, dans cet ordre, et la première qui
  // répond arrête la recherche.
  if (ChangeEntityBar(asked) || ChangeEntityName(asked) || ChangeShortcutBar(asked) ||
      ChangeArmedSkill(asked) || pages_.targeting.Change(asked) ||
      pages_.context_menu.Change(asked) || pages_.graphics.Change(asked) ||
      pages_.monster_sheet.Change(asked) || pages_.craft_atlas.Change(asked, to.craft_atlas) ||
      pages_.make_item.Change(asked) || pages_.weapon_refine.Change(asked) ||
      pages_.quick_cast.Change(asked) || pages_.card_album.Change(asked) ||
      pages_.descriptions.Change(asked) || pages_.bug_report.Change(asked) ||
      pages_.ChangeSetting(asked)) {
    Record(to);
    return;
  }
  // ⚠ LA MÊME TRADUCTION QUE LE ROUTAGE DES GESTES, et par la même fonction :
  // c'est le `if` qu'on oublie d'écrire qui coûte, et il a coûté une fois.
  const std::optional<GameOption> named = NamedOption(asked.changed);
  if (!named.has_value()) {
    return;
  }
  const GameOption which = *named;

  // ── CE QUE LE GESTE VA REMPLACER ──────────────────────────────────────────
  //
  // 🔴 AVANT L'AIGUILLAGE : c'est LUI qui écrit le champ. Relevée après,
  // l'ancienne valeur serait déjà la neuve, et « Revenir » ne reviendrait nulle
  // part.
  const DisplayRevert before{.resolution = resolution_,
                             .display_mode = display_mode_,
                             .interface_scale = interface_scale_,
                             .screen = screen_};

  const bool display_gesture = which == GameOption::kScreen || which == GameOption::kDisplayMode ||
                               which == GameOption::kResolution ||
                               which == GameOption::kInterfaceScale;

  switch (which) {
    case GameOption::kMusic:
      sound_options_.music = asked.on;
      break;
    case GameOption::kSound:
      sound_options_.sound = asked.on;
      break;
    case GameOption::kSpatialSound:
      sound_options_.spatial_sound = asked.on;
      break;
    case GameOption::kFixedCamera:
      fixed_camera_ = asked.on;
      break;
    case GameOption::kExpandedZoom:
      expanded_zoom_ = asked.on;
      break;
    case GameOption::kNoShift:
      no_shift_ = asked.on;
      break;
    case GameOption::kNoCtrl:
      no_ctrl_ = asked.on;
      break;
    case GameOption::kSpeakScream:
      speak_scream_ = asked.on;
      break;
    case GameOption::kSpeakFrostJoke:
      speak_frost_joke_ = asked.on;
      break;
    case GameOption::kItemDropArc:
      item_drop_.arc = asked.on;
      break;
    case GameOption::kItemDropArcHeight:
      item_drop_.height_units = asked.value;
      break;
    case GameOption::kItemDropArcKick:
      item_drop_.kick_units = asked.value;
      break;
    case GameOption::kItemDropArcSourceHeight:
      item_drop_.source_height_fraction = asked.value / kPercent;
      break;
    case GameOption::kItemDropArcDuration:
      item_drop_.duration_ms = static_cast<std::uint32_t>(asked.value);
      break;
    case GameOption::kMusicVolume:
      sound_options_.music_volume = asked.value / kPercent;
      break;
    case GameOption::kSoundVolume:
      sound_options_.sound_volume = asked.value / kPercent;
      break;
    case GameOption::kSpriteSoundVolume:
      sound_options_.sprite_sound_volume = asked.value / kPercent;
      break;
    case GameOption::kEffectSoundVolume:
      sound_options_.effect_sound_volume = asked.value / kPercent;
      break;
    case GameOption::kAmbienceVolume:
      sound_options_.ambience_volume = asked.value / kPercent;
      break;
    case GameOption::kInterfaceScale:
      // ⚠ `chosen` ET NON `value` : c'est une liste de paliers, et `value` y
      // reste à zéro. Le fichier garde le FACTEUR et non le rang — la liste
      // gagne une entrée dès que `/echelle` pose une valeur hors palier, et un
      // rang enregistré ne voudrait alors plus dire la même chose.
      interface_scale_ = InterfaceScaleAt(asked.chosen);
      break;
    case GameOption::kScreen:
      screen_ = ScreenChoiceAt(screens_, asked.chosen, screen_);
      break;
    case GameOption::kDisplayMode:
      // ⚠ `chosen` ET NON `value` : c'est une liste déroulante, et `value` y
      // reste à zéro. Voir `OptionsWindowAction`, qui dit quel champ va avec
      // quel genre — les trois listes de cette section ont commencé par lire le
      // mauvais, et choisissaient donc toujours leur première entrée.
      display_mode_ = asked.chosen;
      break;
    case GameOption::kResolution:
      // ⚠ LE RANG N'EST PAS LA VALEUR : la fenêtre rend un indice DANS LA LISTE
      // qu'on lui a donnée, et c'est la TAILLE qu'on garde. Voir `resolution_`.
      resolution_ = ResolutionTextAt(asked.chosen);
      break;
    case GameOption::kLockWindowSize:
      lock_window_size_ = asked.on;
      break;
    case GameOption::kVerticalSync:
      vertical_sync_ = asked.on;
      break;
    case GameOption::kFrameLimit:
      frame_limit_ = FrameLimitAt(asked.chosen);
      break;
    case GameOption::kFrameMeter:
      frame_meter_ = asked.on;
      break;
    case GameOption::kFrameMeterLocked:
      frame_meter_locked_ = asked.on;
      break;
    case GameOption::kFrameMeterGraph:
      frame_meter_graph_ = asked.on;
      break;
    case GameOption::kFrameMeterPing:
      frame_meter_ping_ = asked.on;
      break;
    case GameOption::kChatFont:
      chat_look_.font_percent = static_cast<int>(asked.value);
      break;
    case GameOption::kInventoryFilter:
      inventory_filter_ = asked.on;
      break;
    case GameOption::kCartFilter:
      cart_filter_ = asked.on;
      break;
    case GameOption::kCartLockSize:
      cart_locked_ = asked.on;
      break;
    case GameOption::kStorageFilter:
      storage_filter_ = asked.on;
      break;
    case GameOption::kInventoryTabsVertical:
      inventory_tabs_vertical_ = asked.on;
      break;
    case GameOption::kInventoryFreeLayout:
      inventory_free_layout_ = asked.on;
      break;
    case GameOption::kInventoryForgetPlaces:
      // 🔴 UN GESTE, PAS UN RÉGLAGE : la table vit dans son fichier, et c'est
      // `GameSettings` qui l'efface — lui seul sait où elle s'écrit. Ici, il n'y
      // a rien à retenir.
      break;
    case GameOption::kCartTabsVertical:
      cart_tabs_vertical_ = asked.on;
      break;
    case GameOption::kStorageTabsVertical:
      storage_tabs_vertical_ = asked.on;
      break;
    case GameOption::kStorageTypeTabs:
      storage_type_tabs_ = asked.on;
      break;
    case GameOption::kStorageTabImages:
      storage_tab_images_ = asked.on;
      break;
    case GameOption::kStorageShelfRow:
      storage_shelf_row_ = asked.on;
      break;
    case GameOption::kStorageSlotColumn:
      storage_slot_column_ = asked.on;
      break;
    case GameOption::kStorageItemIdColumn:
      storage_item_id_column_ = asked.on;
      break;
    case GameOption::kStorageCardSlotsColumn:
      storage_card_slots_column_ = asked.on;
      break;
    case GameOption::kStorageValueColumn:
      storage_value_column_ = asked.on;
      break;
    case GameOption::kStorageTotalValue:
      storage_total_value_ = asked.on;
      break;
    case GameOption::kStorageResetColumns:
      // Voir `kInventoryForgetPlaces` : un geste dont l'état ne vit pas ici.
      break;
    case GameOption::kInventoryStorageButton:
      inventory_storage_button_ = asked.on;
      break;
    case GameOption::kCartStorageButton:
      cart_storage_button_ = asked.on;
      break;
    case GameOption::kStoragePeerButtons:
      storage_peer_buttons_ = asked.on;
      break;
    case GameOption::kInventoryHoverDescription:
      inventory_hover_description_ = asked.on;
      break;
    case GameOption::kCartHoverDescription:
      cart_hover_description_ = asked.on;
      break;
    case GameOption::kStorageHoverDescription:
      storage_hover_description_ = asked.on;
      break;
    case GameOption::kCharacterHoverDescription:
      character_hover_description_ = asked.on;
      break;
    case GameOption::kStatBarsShown:
      stat_bars_.shown = asked.on;
      break;
    case GameOption::kBasicInfoWindowShown:
      stat_bars_.basic_info_window = asked.on;
      break;
    case GameOption::kStatBarsLocked:
      stat_bars_.locked = asked.on;
      break;
    case GameOption::kStatBarsSticky:
      stat_bars_.sticky = asked.on;
      break;
    case GameOption::kStatBarsCenterMark:
      stat_bars_.center_mark = asked.on;
      break;
    case GameOption::kStatBarsTuning:
      stat_bars_.tuning = asked.on;
      break;
    case GameOption::kStatBarsKeepMaxedExp:
      stat_bars_.keep_maxed_exp = asked.on;
      break;
    case GameOption::kPvpHudLocked:
      pvp_hud_.locked = asked.on;
      break;
    case GameOption::kPvpHudSticky:
      pvp_hud_.sticky = asked.on;
      break;
    case GameOption::kPvpHudCenterMark:
      pvp_hud_.center_mark = asked.on;
      break;
    case GameOption::kPvpHudBorder:
      pvp_hud_.border = asked.on;
      break;
    case GameOption::kStatBarsWarnOverweight:
      stat_bars_.warn_overweight = asked.on;
      break;
    case GameOption::kStatBarsZenyReference:
      stat_bars_.zeny_reference = ro::ui::ZenyReferenceOf(asked.value);
      break;
    case GameOption::kEntityBarsShown:
    case GameOption::kEntityBarsHideVendors:
    case GameOption::kEntityBarsTuning:
      return;  // pris plus haut par `ChangeEntityBar`
    case GameOption::kEntityNamesShown:
      entity_names_.shown = asked.on;
      break;
    case GameOption::kEntityNamesFont:
      entity_names_.font_pixels = asked.value;
      break;
    case GameOption::kEntityNamesEmblem:
      entity_names_.guild_emblem = asked.on;
      break;
    case GameOption::kStatusBarShown:
      status_bar_.shown = asked.on;
      break;
    case GameOption::kStatusBarLocked:
      status_bar_.locked = asked.on;
      break;
    case GameOption::kStatusBarBorder:
      status_bar_.border = asked.on;
      break;
    case GameOption::kStatusBarBackground:
      status_bar_.background_color = asked.color;
      break;
    case GameOption::kStatusBarPreview:
      status_preview_ = asked.on;
      break;
    case GameOption::kNpcMenuSearch:
      npc_menu_search_ = asked.on;
      break;
    case GameOption::kUnlockNotice:
      unlock_notice_ = asked.on;
      break;
    case GameOption::kSkillFailShown:
      skill_fail_shown_ = asked.on;
      break;
    case GameOption::kEffectsShown:
      effects_shown_ = asked.on;
      break;
    case GameOption::kMinimalEffects:
      minimal_effects_ = asked.on;
      break;
    case GameOption::kSkyEffectsShown:
      sky_effects_shown_ = asked.on;
      break;
    case GameOption::kStatusBarSide:
      status_bar_.side_px = asked.value;
      break;
    case GameOption::kStatusBarGap:
      status_bar_.gap_px = asked.value;
      break;
    case GameOption::kStatusBarMost:
      status_bar_.most = static_cast<int>(asked.value);
      break;
    case GameOption::kStatusBarRows:
      status_bar_.rows = static_cast<int>(asked.value);
      break;
    case GameOption::kStatusBarAnchor:
      status_bar_.anchor = asked.chosen;
      break;
    case GameOption::kStatusBarSort:
      status_bar_.sort = asked.chosen;
      break;
    case GameOption::kStatusBarCountdown:
      status_bar_.show_countdown = asked.on;
      break;
    case GameOption::kStatusBarCountdownSize:
      status_bar_.countdown_px = asked.value;
      break;
    case GameOption::kStatusBarCountdownBackground:
      status_bar_.countdown_background = asked.color;
      break;
    case GameOption::kStatusBarSweep:
      status_bar_.sweep = asked.chosen;
      break;
    case GameOption::kStatusBarSweepColor:
      status_bar_.sweep_color = asked.color;
      break;
    case GameOption::kStatusBarIconBackground:
      status_bar_.icon_background = asked.color;
      break;
    case GameOption::kStatusBarOpacity:
      // ⚠ MONTRÉE EN POUR CENT, GARDÉE DE ZÉRO À UN — l'unité du dessin, comme
      // pour les volumes.
      status_bar_.opacity = asked.value / kPercent;
      break;
    case GameOption::kItemObtainLines:
      item_obtain_.most_lines = static_cast<int>(asked.value);
      break;
    case GameOption::kItemObtainNewestOnTop:
      item_obtain_.newest_on_top = asked.on;
      break;
    case GameOption::kItemObtainMergeSame:
      item_obtain_.merge_same = asked.on;
      break;
    case GameOption::kItemObtainDurationMs:
      item_obtain_.duration_ms = static_cast<int>(asked.value);
      break;
    case GameOption::kItemObtainUnlocked:
      // La case dit « déverrouiller » : elle est l'INVERSE du verrou. ⚠ ET ELLE
      // REPLACE LA POIGNÉE : sans cela, ImGui garderait la position que la
      // fenêtre avait à sa création, et rouvrir le verrou la ferait reparaître
      // où elle n'est plus.
      item_obtain_.locked = !asked.on;
      break;
    case GameOption::kItemObtainCentered:
      // On ne peut pas à la fois centrer et placer : décocher repose le bandeau
      // là où le client l'aurait mis.
      item_obtain_.pos_x = asked.on ? -1 : ro::ui::kNativeToastLeftPixels;
      break;
    case GameOption::kItemObtainRowGap:
      item_obtain_.row_gap_px = static_cast<int>(asked.value);
      break;
    case GameOption::kItemObtainPadVertical:
      item_obtain_.pad_vertical_px = static_cast<int>(asked.value);
      break;
    case GameOption::kItemObtainFontScale:
      item_obtain_.font_scale_percent = static_cast<int>(asked.value);
      break;
    case GameOption::kItemObtainIcon:
      item_obtain_.show_icon = asked.on;
      break;
    case GameOption::kItemObtainFrame:
      item_obtain_.show_frame = asked.on;
      break;
    case GameOption::kItemObtainBackground:
      item_obtain_.background_shown = asked.on;
      break;
    case GameOption::kItemObtainBackgroundColor:
      item_obtain_.background = asked.color;
      break;
    case GameOption::kItemObtainBackgroundAlpha:
      // ⚠ L'OPACITÉ EST L'ALPHA DE LA COULEUR, pas un champ à part : le curseur
      // et le nuancier règlent le même octet, et ne peuvent donc pas se
      // contredire.
      item_obtain_.background =
          ro::ui::WithAlphaPercent(item_obtain_.background, static_cast<int>(asked.value));
      break;
    case GameOption::kItemObtainRounding:
      item_obtain_.rounding_px = static_cast<int>(asked.value);
      break;
    case GameOption::kItemObtainBorder:
      item_obtain_.border_shown = asked.on;
      break;
    case GameOption::kItemObtainBorderColor:
      item_obtain_.border = asked.color;
      break;
    case GameOption::kItemObtainBorderAlpha:
      item_obtain_.border =
          ro::ui::WithAlphaPercent(item_obtain_.border, static_cast<int>(asked.value));
      break;
    case GameOption::kItemObtainBorderThickness:
      item_obtain_.border_px = static_cast<int>(asked.value);
      break;
    case GameOption::kItemObtainTextColor:
      item_obtain_.text = asked.color;
      break;
    case GameOption::kItemObtainQuantityColor:
      item_obtain_.quantity = asked.color;
      break;
    case GameOption::kInventoryLockSize:
      inventory_locked_ = asked.on;
      break;
    case GameOption::kInventoryDropLock:
      inventory_drop_lock_ = asked.on;
      break;
    case GameOption::kInventorySellLock:
      inventory_sell_lock_ = asked.on;
      break;
    case GameOption::kInventorySort:
      inventory_sort_ = asked.on;
      break;
    case GameOption::kCaptureFramesPerSecond:
      capture_.frames_per_second = static_cast<int>(asked.value);
      break;
    case GameOption::kSkillAreaCastShown:
      skill_area_.cast_shown = asked.on;
      break;
    case GameOption::kSkillAreaArmedShown:
      skill_area_.armed_shown = asked.on;
      break;
    case GameOption::kSkillAreaPattern:
      // ⚠ BORNÉ À LA LECTURE : le rang vient d'une liste déroulante, mais aussi
      // d'un fichier de réglages qu'on peut avoir écrit à la main.
      skill_area_.pattern = static_cast<ro::render::SkillAreaPattern>(
          std::clamp(asked.chosen, 0, static_cast<int>(ro::render::SkillAreaPattern::kSolid)));
      break;
    case GameOption::kSkillAreaGap:
      skill_area_.gap_percent = asked.value;
      break;
    case GameOption::kSkillAreaColor:
      skill_area_.color = asked.color;
      break;
    case GameOption::kCaptureSeconds:
      capture_.seconds = static_cast<int>(asked.value);
      break;
    case GameOption::kCaptureWidth:
      capture_.widest_output = static_cast<int>(asked.value);
      break;
    case GameOption::kCaptureDelay:
      capture_.start_delay_seconds = static_cast<int>(asked.value);
      break;
    // ⚠ LES TROIS GESTES DE LA CAPTURE NE SONT PAS ICI : ils demandent l'écran
    // et l'horloge, que ces réglages ne possèdent pas. C'est la boucle qui les
    // joue, juste après — exactement comme les trois gestes du menu d'Échap.
    case GameOption::kCaptureZone:
    case GameOption::kCaptureRecord:
    case GameOption::kCapturePicture:
      break;
    case GameOption::kAlignGrid:
      align_grid_.shown = asked.on;
      break;
    case GameOption::kAlignGridMagnet:
      align_grid_.magnetic = asked.on;
      break;
    case GameOption::kWindowMagnet:
      align_grid_.windows_magnetic = asked.on;
      break;
    case GameOption::kAlignGridCell:
      align_grid_.cell_pixels = static_cast<int>(asked.value);
      break;
    case GameOption::kAlignGridColor:
      align_grid_.color = asked.color;
      break;
    case GameOption::kChatPadding:
      chat_look_.padding_pixels = static_cast<int>(asked.value);
      break;
    case GameOption::kChatLineGap:
      chat_look_.line_gap_pixels = static_cast<int>(asked.value);
      break;
    case GameOption::kChatTimestamps:
      chat_look_.timestamps = asked.on;
      break;
    case GameOption::kChatGrowInput:
      chat_look_.grow_input = asked.on;
      break;
    case GameOption::kChatBattleMode:
      chat_look_.battle_mode = asked.on;
      break;
    case GameOption::kChatKeepHistory:
      chat_look_.keep_history = asked.on;
      break;
    case GameOption::kChatKeptLines:
      chat_look_.kept_lines = static_cast<int>(asked.value);
      break;
    case GameOption::kChatKeepTyped:
      chat_look_.keep_typed = asked.on;
      break;
    case GameOption::kChatKeptTyped:
      chat_look_.kept_typed = static_cast<int>(asked.value);
      break;
    case GameOption::kChatKeptPerKind:
      chat_kept_per_kind_ = static_cast<int>(asked.value);
      break;
    case GameOption::kChatKeepLog:
      chat_look_.keep_log = asked.on;
      break;
    case GameOption::kChatDiagnostic:
      chat_look_.diagnostic = asked.on;
      break;
    case GameOption::kChatBody:
      chat_look_.body = asked.color;
      break;
    case GameOption::kChatBorder:
      chat_look_.border = asked.color;
      break;
    case GameOption::kChatTabs:
      chat_look_.tabs = asked.color;
      break;
    case GameOption::kBubbleShowOwn:
      bubbles_.show_own = asked.on;
      break;
    case GameOption::kBubbleFade:
      bubbles_.fade = asked.on;
      break;
    case GameOption::kBubbleClientTiming:
      bubbles_.client_timing = asked.on;
      break;
    case GameOption::kBubbleBaseMs:
      bubbles_.base_ms = asked.value;
      break;
    case GameOption::kBubblePerCharacterMs:
      bubbles_.per_character_ms = asked.value;
      break;
    case GameOption::kBubbleTextScale:
      ChangeBubbleTextScale(bubbles_, asked.value);
      break;
    case GameOption::kBubbleRise:
      bubbles_.rise_pixels = asked.value;
      break;
    case GameOption::kBubbleWidth:
      bubbles_.width_fraction = asked.value;
      break;
    case GameOption::kBubbleOpacity:
      bubbles_.opacity = asked.value;
      break;
    case GameOption::kBubbleBackground:
      bubbles_.background = asked.on;
      break;
    case GameOption::kBubbleCorner:
      bubbles_.corner_radius = asked.value;
      break;
    case GameOption::kBubblePaddingX:
      bubbles_.padding_x = asked.value;
      break;
    case GameOption::kBubblePaddingY:
      bubbles_.padding_y = asked.value;
      break;
    case GameOption::kBubbleBackgroundColor:
      bubbles_.background_color = asked.color;
      break;
    case GameOption::kBubbleBorderColor:
      bubbles_.border_color = asked.color;
      break;
    case GameOption::kChatForgetHistory:
      // Rien à écrire : c'est un GESTE, pas une valeur. Il part au moteur par
      // le chemin qui POUSSE, comme la musique qu'on relance.
      break;
    case GameOption::kChatClientColors:
      // ⚠ LES TROIS D'UN COUP, et seulement les trois : ce bouton rend les
      // COULEURS du client, pas les réglages d'origine. Vider au passage
      // l'horodatage ou l'historique serait un geste qu'on n'a pas demandé.
      chat_look_.body = ro::ui::kChatBodyColor;
      chat_look_.border = ro::ui::kChatBorderColor;
      chat_look_.tabs = ro::ui::kChatTabColor;
      break;
    case GameOption::kFontFamily:
      // La place dans le menu, qui ne montre que les familles présentes.
      font_family_ = ro::ui::FontFamilyAtMenuRank(asked.chosen);
      break;
    case GameOption::kLanguage: {
      const std::span<const Language> languages = KnownLanguages();
      if (asked.chosen < 0 || asked.chosen >= static_cast<int>(languages.size())) {
        return;
      }
      std::string why;
      static_cast<void>(
          SetLanguage(languages[static_cast<std::size_t>(asked.chosen)].code, to, &why));
      return;
    }
    case GameOption::kDamageShown:
    case GameOption::kDamageOwnDealt:
    case GameOption::kDamageOwnTaken:
    case GameOption::kDamageOthers:
    case GameOption::kDamageMiss:
    case GameOption::kDamageLucky:
    case GameOption::kDamageCriticalStar:
    case GameOption::kDamageHeals:
    case GameOption::kDamageEffectNumbers:
    case GameOption::kDamageSplit:
    case GameOption::kDamageHitInterval:
    case GameOption::kDamageScale:
    case GameOption::kDamageDigitPitch:
    case GameOption::kDamageOpacity:
    case GameOption::kDamageAbbreviate:
    case GameOption::kDamageColorOwnDealt:
    case GameOption::kDamageColorMonster:
    case GameOption::kDamageColorPlayer:
    case GameOption::kDamageColorSelf:
    case GameOption::kDamageColorCritical:
    case GameOption::kDamageColorSp:
    case GameOption::kDamageColorHeal:
    case GameOption::kDamageColorSpRecovery:
    case GameOption::kDamageArrivalX:
    case GameOption::kDamageBendOne:
    case GameOption::kDamageBendTwo:
    case GameOption::kDamageScatter:
    case GameOption::kDamageScatterX:
    case GameOption::kDamageScatterY:
    case GameOption::kDamageColorTotal:
    case GameOption::kDamagePreview:
    case GameOption::kDamageTotalCarries:
    case GameOption::kDamageTotalForget:
    case GameOption::kDamageStartX:
    case GameOption::kDamagePathShown:
    case GameOption::kDamagePathCopy:
    case GameOption::kDamageRehearsalStep:
    case GameOption::kDamagePreviewFloor:
    case GameOption::kDamagePreviewGrid:
    case GameOption::kDamageBirthScale:
    case GameOption::kDamageBirthSettle:
    case GameOption::kDamageFadeStart:
    case GameOption::kDamageLifeSpan:
    case GameOption::kDamageFacingDrift:
    case GameOption::kDamagePathClient:
    case GameOption::kDamageFontText:
    case GameOption::kDamageFontOutline:
    case GameOption::kDamageFontName:
    case GameOption::kDamageFontBold:
    case GameOption::kDamageClientLook:
      // LES DÉGÂTS FLOTTANTS, retour au client compris : voir `ChangeDamage`.
      ChangeDamage(which, asked);
      break;
    case GameOption::kAfkEnabled:
    case GameOption::kAfkDelay:
    case GameOption::kAfkWakeOn:
    case GameOption::kAfkSpin:
    case GameOption::kAfkTilt:
    case GameOption::kAfkZoom:
    case GameOption::kAfkEase:
    case GameOption::kAfkHideUi:
    case GameOption::kAfkHideCursor:
    case GameOption::kAfkVignette:
    case GameOption::kAfkDesaturate:
    case GameOption::kAfkGrain:
    case GameOption::kAfkShowClock:
    case GameOption::kAfkShowAway:
    case GameOption::kAfkClockAnchor:
    case GameOption::kAfkClockMargin:
    case GameOption::kAfkClockScale:
    case GameOption::kAfkClockColor:
    case GameOption::kAfkClockShadow:
    case GameOption::kAfkTryNow:
    case GameOption::kAfkAnnounce:
    case GameOption::kAfkAnnounceDelay:
    case GameOption::kAfkAnnounceSleepSign:
    case GameOption::kAfkAnnounceTagSign:
      // L'ÉCRAN DE VEILLE, dans son fichier, pour la même raison que les
      // dégâts : vingt-trois réglages d'un seul objet, et l'aiguillage qui les
      // écrit vit avec leurs lignes. Voir `AfkOptions::Change` — le bouton
      // d'essai y passe aussi, en armant une demande.
      pages_.afk.Change(which, asked, to);
      break;
    case GameOption::kCount:
      return;
  }
  Apply(which, to);

  if (display_gesture) {
    ArmDisplayRevert(now_ms, before, to);
  }
  Record(to);
}

}  // namespace moonclient
