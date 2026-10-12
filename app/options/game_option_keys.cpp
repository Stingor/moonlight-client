// LES CLÉS DU FICHIER DE RÉGLAGES, ET CE QUI LES DÉCLARE.
//
// ⚠ SÉPARÉ DE CE QUI LES APPLIQUE, et pas seulement pour la longueur : ce
// fichier-ci ne se lit jamais de bout en bout. On y cherche UNE clé, on la copie
// sur sa voisine, on s'en va. Le fichier d'à côté, lui, se lit : il porte les
// règles qui disent ce qu'un réglage TOUCHE.
//
// 🔴 ET C'EST L'ADRESSE DU CHAMP QU'ON DÉCLARE. La lecture et l'écriture
// parcourent la même table, donc elles ne peuvent pas diverger — et la valeur
// présente à la DÉCLARATION du champ est le défaut, ce qui interdit une seconde
// source.

#include <algorithm>
#include <filesystem>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/ui/system_fonts.h"
#include "ro/ui/ui_context.h"

#include "chat/chat_history_file.h"
#include "options/display_options.h"
#include "options/game_options.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. ⚠ GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
// LES DEUX BASCULES D'EFFET DU JEU, et le client les garde toutes deux — mais
// PAR LE NOM DE LA COMMANDE, jamais par son numéro : son `SaveData\OptionInfo.lua`
// porte `CmdOnOffList["/effect"] = 1` et `CmdOnOffList["/mineffect"] = 0`.
//
// ⚠ UN CLIENT QUI NE CONNAÎT PAS LA CHAÎNE NE SAIT PAS RELIRE LE RÉGLAGE : c'est
// ainsi que `/frame` est resté inerte chez lui. Nos clés sont les nôtres, et
// c'est sans danger tant qu'elles ne bougent plus.
constexpr const char* kEffectsShownKey = "effects_shown";
constexpr const char* kMinimalEffectsKey = "effects_minimal";
constexpr const char* kLanguageKey = "language";
constexpr const char* kLoginAccountKey = "count_held";
constexpr const char* kScreenNameKey = "display_screen";
constexpr const char* kScreenRankKey = "display_screen_rank";
constexpr const char* kDisplayModeKey = "display_mode";
constexpr const char* kResolutionKey = "display_resolution";
constexpr const char* kLockWindowSizeKey = "display_size_locked";
constexpr const char* kVerticalSyncKey = "display_sync_vertical";
constexpr const char* kFrameLimitKey = "display_limit_frames";
constexpr const char* kFrameMeterKey = "display_meter";
constexpr const char* kFrameMeterLockedKey = "display_meter_locked";
constexpr const char* kFrameMeterGraphKey = "display_meter_curve";
constexpr const char* kFrameMeterPingKey = "display_meter_ping";
constexpr const char* kWindowPlaceKey = "display_position";
constexpr const char* kAlignGridKey = "grid_alignment";
constexpr const char* kAlignGridMagnetKey = "grid_magnet";
constexpr const char* kInventoryFilterKey = "bag_filter";
constexpr const char* kCartFilterKey = "cart_filter";
constexpr const char* kCartSizeKey = "cart_size_locked";
constexpr const char* kStorageFilterKey = "storage_filter";
constexpr const char* kInventoryStorageButtonKey = "bag_button_storage";
constexpr const char* kCartStorageButtonKey = "cart_button_storage";
// ⚠ LES DEUX CLÉS AU-DESSUS GARDENT LEUR NOM D'ORIGINE, alors qu'elles
// commandent maintenant DEUX raccourcis et non le seul storage : les renommer
// perdrait le réglage de qui l'avait déjà posé, pour un mot dans un fichier que
// personne ne lit.
constexpr const char* kStoragePeerButtonsKey = "storage_buttons_pairs";
constexpr const char* kInventoryTabsVerticalKey = "bag_tabs_vertical";
constexpr const char* kInventoryFreeLayoutKey = "bag_placement_free";
constexpr const char* kCartTabsVerticalKey = "cart_tabs_vertical";
constexpr const char* kStorageTabsVerticalKey = "storage_tabs_vertical";
constexpr const char* kStorageTypeTabsKey = "storage_tabs_categories";
constexpr const char* kStorageTabImagesKey = "storage_tabs_frames";
constexpr const char* kStorageShelfRowKey = "storage_row_storages";
constexpr const char* kStorageSlotColumnKey = "storage_column_slot";
constexpr const char* kStorageItemIdColumnKey = "storage_column_login";
constexpr const char* kStorageCardSlotsColumnKey = "storage_column_slots";
constexpr const char* kStorageValueColumnKey = "storage_column_value";
constexpr const char* kStorageTotalValueKey = "storage_value_total";
constexpr const char* kInventoryHoverDescriptionKey = "bag_preview_hover";
constexpr const char* kCartHoverDescriptionKey = "cart_preview_hover";
constexpr const char* kStorageHoverDescriptionKey = "storage_preview_hover";
constexpr const char* kCharacterHoverDescriptionKey = "sheet_preview_hover";
constexpr const char* kGrimoireSmoothIconsKey = "grimoire_smoothing_icons";
constexpr const char* kCharacterStaffPaneKey = "sheet_panel_staff";
constexpr const char* kStaffBattleIgnoreKey = "sheet_staff_ignores_of_monsters";
constexpr const char* kCharacterAvatarPoseKey = "sheet_avatar_placed";
constexpr const char* kCharacterAvatarAnimatedKey = "sheet_avatar_animated";
constexpr const char* kCharacterAvatarDirectionKey = "sheet_avatar_direction";
// LA BARRE D'ÉTATS. ⚠ SES CLÉS SONT PRÉFIXÉES `etats_` et non `statut_` : c'est
// le mot que le reste du client emploie déjà — `StatusTable`, `ActiveStatus` —,
// et une clé de fichier ne se renomme pas sans perdre le réglage de qui l'avait
// posée.
// LES SIX JAUGES LIBRES. ⚠ PRÉFIXE `gauges_` : « bars » est déjà pris par les
// barres de raccourcis, et deux familles au même préfixe se mêleraient dans un
// fichier qu'on relit à la main.
constexpr const char* kStatBarsShownKey = "gauges_shown";
constexpr const char* kStatBarsLockedKey = "gauges_locked";
constexpr const char* kStatBarsStickyKey = "gauges_snapped";
constexpr const char* kStatBarsCenterMarkKey = "gauges_marker_center";
constexpr const char* kStatBarsKeepMaxedExpKey = "gauges_exp_kept_at_max_level";
constexpr const char* kStatBarsWarnKey = "gauges_alert_overload";
constexpr const char* kStatBarsZenyKey = "gauges_purse_full";

// Les noms de chaque jauge dans le fichier. ⚠ DES MOTS ET NON DES NUMÉROS : une
// clé « gauges_2_x » ne se relit pas, et l'ordre de l'énumération changerait son
// sens sans que le fichier le dise.
constexpr std::array<const char*, ro::ui::kStatBarCount> kStatBarNames = {
    "life", "magic", "exp_base", "exp_job", "weight", "purse", "cast",
};

// La clé d'une macro de chat, numérotée à partir de UN.
[[nodiscard]] std::string MacroKey(std::size_t slot) { return "macro_" + std::to_string(slot + 1); }

[[nodiscard]] std::string StatBarKey(std::size_t bar, const char* what) {
  return std::string("gauge_") + kStatBarNames[bar] + "_" + what;
}

// LES BARRES AU-DESSUS DES ENTITÉS. ⚠ PRÉFIXE `entity_` : « gauges » désigne
// déjà les six libres, et deux familles au même préfixe se mêleraient dans un
// fichier qu'on relit à la main.
constexpr const char* kEntityBarsShownKey = "entities_bars_shown";
constexpr const char* kEntityBarsHideVendorsKey = "entities_bars_hide_vendors";

// Le nom de chaque espèce dans le fichier. ⚠ DES MOTS ET NON DES NUMÉROS, pour
// la raison des jauges : l'ordre de l'énumération changerait le sens d'un
// numéro sans que le fichier le dise.
constexpr std::array<const char*, ro::ui::kEntityBarKindCount> kEntityBarNames = {
    "self", "party", "players", "monsters", "boss", "others",
};

[[nodiscard]] std::string EntityBarKey(std::size_t kind, const char* what) {
  return std::string("entity_") + kEntityBarNames[kind] + "_" + what;
}

// LES ÉTIQUETTES DES ENTITÉS. ⚠ PRÉFIXE `name_` ET NON `entity_` : les espèces
// ne sont pas les mêmes que celles des barres — pas de groupe, mais un PNJ et
// un objet au sol — et deux familles au même préfixe se liraient comme une
// seule dans un fichier qu'on relit à la main.
constexpr const char* kEntityNamesShownKey = "names_shown";
constexpr const char* kEntityNamesFontKey = "names_font";
constexpr const char* kEntityNamesEmblemKey = "names_emblem_guild";

constexpr std::array<const char*, ro::render::kNamePlateKindCount> kEntityNameNames = {
    "self", "players", "npc", "monsters", "boss", "items",
};

[[nodiscard]] std::string EntityNameKey(std::size_t kind, const char* what) {
  return std::string("name_") + kEntityNameNames[kind] + "_" + what;
}

constexpr const char* kStatusBarShownKey = "statuses_shown";
constexpr const char* kNpcMenuSearchKey = "npc_menu_search";
constexpr const char* kUnlockNoticeKey = "unlock_notice_shown";
constexpr const char* kSkillFailShownKey = "skill_fail_shown";
constexpr const char* kStatusBarLockedKey = "statuses_locked";
constexpr const char* kStatusBarBorderKey = "statuses_rule";
constexpr const char* kStatusBarBackgroundKey = "statuses_color_background";
constexpr const char* kStatusBarSideKey = "statuses_side";
constexpr const char* kStatusBarGapKey = "statuses_spacing";
constexpr const char* kStatusBarMostKey = "statuses_ceiling";
constexpr const char* kStatusBarRowsKey = "statuses_lines";
constexpr const char* kStatusBarAnchorKey = "statuses_anchoring";
constexpr const char* kStatusBarSortKey = "statuses_sorting";
constexpr const char* kStatusBarCountdownKey = "statuses_count_to_down";
constexpr const char* kStatusBarCountdownSizeKey = "statuses_size_of_text";
constexpr const char* kStatusBarCountdownBackgroundKey = "statuses_background_of_text";
constexpr const char* kStatusBarSweepKey = "statuses_greying";
// LE BANDEAU « objet obtenu ». ⚠ PAS DE CLÉ D'ALLUMAGE : le bandeau EST la
// notification de ramassage, il n'y a rien à éteindre.
//
// Les deux opacités n'ont pas de clé non plus, et ce n'est pas un oubli :
// l'alpha voyage DANS la couleur, qui est un `ImU32`. Une clé de plus pour le
// même octet donnerait deux sources qui se contrediraient au premier
// chargement.
constexpr const char* kItemObtainLinesKey = "banner_item_lines_max";
constexpr const char* kItemObtainNewestOnTopKey = "banner_item_recent_in_top";
constexpr const char* kItemObtainMergeSameKey = "banner_item_group_up";
constexpr const char* kItemObtainDurationKey = "banner_item_duration_ms";
constexpr const char* kItemObtainLockedKey = "banner_item_lock";
constexpr const char* kItemObtainPosXKey = "banner_item_x";
constexpr const char* kItemObtainPosYKey = "banner_item_y";
constexpr const char* kItemObtainRowGapKey = "banner_item_line_spacing";
constexpr const char* kItemObtainPadVerticalKey = "banner_item_margin_v";
constexpr const char* kItemObtainFontScaleKey = "banner_item_font";
constexpr const char* kItemObtainIconKey = "banner_item_icon";
constexpr const char* kItemObtainFrameKey = "banner_item_frame";
constexpr const char* kItemObtainBackgroundKey = "banner_item_background";
constexpr const char* kItemObtainBackgroundColorKey = "banner_item_background_color";
constexpr const char* kItemObtainRoundingKey = "banner_item_background_rounding";
constexpr const char* kItemObtainBorderKey = "banner_item_border";
constexpr const char* kItemObtainBorderColorKey = "banner_item_border_color";
constexpr const char* kItemObtainBorderThicknessKey = "banner_item_border_thickness";
constexpr const char* kItemObtainTextColorKey = "banner_item_name_color";
constexpr const char* kItemObtainQuantityColorKey = "banner_item_quantity_color";

constexpr const char* kStatusBarSweepColorKey = "statuses_color_veil";
constexpr const char* kStatusBarOpacityKey = "statuses_opacity";
constexpr const char* kStatusBarIconBackgroundKey = "statuses_background_icon";
constexpr const char* kInventoryLockSizeKey = "bag_size_locked";
constexpr const char* kInventoryDropLockKey = "bag_lock_throw";
constexpr const char* kInventorySellLockKey = "bag_lock_sale";
constexpr const char* kInventorySortKey = "bag_favorites_sorted";
constexpr const char* kAlignGridCellKey = "grid_step";
constexpr const char* kAlignGridColorKey = "grid_color";
// LE SOL UNI DE L'ÉTABLI DU STAFF. ⚠ PERSISTÉ MÊME SI LE DROIT NE L'EST PAS :
// un compte devenu ordinaire relira la clé sans pouvoir ouvrir la fenêtre, et
// c'est pourquoi le DESSIN est gaté sur le même droit que l'établi — voir
// `Game::Rights`. Sans cela, un terrain repeint survivrait à la case qui le
// commande.
constexpr const char* kStaffFlatGroundKey = "staff_ground_solid";
constexpr const char* kStaffFlatGroundColorKey = "staff_ground_solid_color";
// L'ENREGISTREUR DE ZONE. ⚠ LA ZONE EST PERSISTÉE COMME LE RESTE : la tracer
// est le seul geste qui recouvre l'écran, et la perdre à chaque lancement
// obligerait à recommencer avant chaque prise.
constexpr const char* kCaptureZoneXKey = "capture_zone_x";
constexpr const char* kCaptureZoneYKey = "capture_zone_y";
constexpr const char* kCaptureZoneWidthKey = "capture_zone_width";
constexpr const char* kCaptureZoneHeightKey = "capture_zone_height";
constexpr const char* kCaptureFpsKey = "capture_frames_per_second";
constexpr const char* kCaptureSecondsKey = "capture_duration";
constexpr const char* kCaptureWidthKey = "capture_width_max";
constexpr const char* kCaptureDelayKey = "capture_delay";
constexpr const char* kFixedCameraKey = "camera_fixed";
constexpr const char* kExpandedZoomKey = "camera_zoom_extended";
constexpr const char* kStaffFreeCameraKey = "staff_camera_free";
constexpr const char* kStaffCameraHudKey = "staff_camera_raised";
constexpr const char* kStaffSoundZonesKey = "staff_zones_sound";
constexpr const char* kStaffPickBoxesKey = "staff_zones_clickable";
constexpr const char* kStaffPickFloorForcedKey = "staff_zones_floor_forced";
constexpr const char* kStaffPickFloorPixelsKey = "staff_zones_floor_px";
constexpr const char* kStaffPickNegativeKey = "staff_zones_negative";
constexpr const char* kStaffPickNegativePercentKey = "staff_zones_negative_share";
constexpr const char* kNoShiftKey = "without_shift";
constexpr const char* kNoCtrlKey = "without_ctrl";
constexpr const char* kSpeakScreamKey = "say_the_shout";
constexpr const char* kSpeakFrostJokeKey = "say_the_joke";
constexpr const char* kItemDropArcKey = "loot_bursts";
constexpr const char* kItemDropArcHeightKey = "loot_crest";
constexpr const char* kItemDropArcKickKey = "loot_gap";
// ⚠ UNE PART DE LA TAILLE DU CORPS, pas des unités du monde.
constexpr const char* kItemDropArcSourceHeightKey = "loot_height_body";
constexpr const char* kItemDropArcDurationKey = "loot_duration";
constexpr const char* kChatFontKey = "text_chat";
constexpr const char* kChatPaddingKey = "margins_chat";
constexpr const char* kChatLineGapKey = "line_spacing_chat";
constexpr const char* kBubbleShowOwnKey = "bubbles_the_mine";
constexpr const char* kBubbleFadeKey = "bubbles_fade";
constexpr const char* kBubbleClientTimingKey = "bubbles_duration_of_client";
constexpr const char* kBubbleBaseMsKey = "bubbles_duration_of_base";
constexpr const char* kBubblePerCharacterMsKey = "bubbles_extension";
constexpr const char* kBubbleRiseKey = "bubbles_offset";
constexpr const char* kBubbleWidthKey = "bubbles_width";
constexpr const char* kBubbleOpacityKey = "bubbles_opacity";
constexpr const char* kBubbleBackgroundKey = "bubbles_background";
constexpr const char* kBubbleCornerKey = "bubbles_rounding";
constexpr const char* kBubblePaddingXKey = "bubbles_margin_x";
constexpr const char* kBubblePaddingYKey = "bubbles_margin_y";
constexpr const char* kBubbleBackgroundColorKey = "bubbles_color_background";
constexpr const char* kBubbleBorderColorKey = "bubbles_color_trim";
constexpr const char* kChatGrowInputKey = "field_that_grows";
constexpr const char* kChatBattleModeKey = "battle_mode";
constexpr const char* kChatTimestampsKey = "timestamp_chat";
constexpr const char* kFontFamilyKey = "font";
constexpr const char* kBackdropKey = "scenery_alive";
constexpr const char* kChatKeepHistoryKey = "keep_chat";
constexpr const char* kChatKeptLinesKey = "lines_kept";
constexpr const char* kChatKeepTypedKey = "keep_inputs";
constexpr const char* kChatKeptTypedKey = "inputs_kept";
constexpr const char* kChatKeepLogKey = "log_chat";
constexpr const char* kChatDiagnosticKey = "diagnostic_chat";
constexpr const char* kChatKeptPerKindKey = "lines_kept_per_kind";
constexpr const char* kChatBodyKey = "background_chat";
constexpr const char* kChatBorderKey = "border_chat";
constexpr const char* kChatTabsKey = "tabs_chat";

// ⚠ LES NOMS DE LANGUE SONT DES ENDONYMES — chacune s'écrit dans sa propre
// langue, comme dans tout choix de langue : qui cherche « English » ne sait pas
// forcément lire le mot qui le désigne en français.
//
// ⚠ MARQUÉS QUAND MÊME, et traduits par eux-mêmes dans le catalogue : la liste
// déroulante traduit ce qu'on lui donne, et un nom absent du catalogue s'y
// compterait comme un oubli de traduction.
//
// ⚠ UN CATALOGUE INCOMPLET SE PROPOSE QUAND MÊME, et c'est le format qui le
// permet : une traduction VIDE laisse le français s'afficher, donc une langue
// à moitié faite ne montre jamais de trou. La refuser jusqu'à 100 %, ce serait
// n'en livrer aucune — un catalogue se remplit en servant, pas avant.
constexpr Language kLanguages[] = {
    {"fr", ro::core::TrText("Français")},
    {"en", ro::core::TrText("English")},
    {"es", ro::core::TrText("Español")},
};

// LES CLÉS DES BARRES DE RACCOURCIS, composées : « bar3_columns ».
//
// ⚠ LE NUMÉRO EST CELUI QUE LE JOUEUR VOIT, à partir de UN, comme dans le
// fichier des barres et dans celui des raccourcis. Trois fichiers qui
// compteraient différemment obligeraient à traduire de tête.
//
// ⚠ GRAVÉES comme toutes les autres : les changer rend à tous les joueurs la
// disposition d'origine, sans que rien ne le dise.
constexpr const char* kShortcutBarKeys = "bar%zu_%s";
constexpr const char* kShortcutShowKeysKey = "bars_keys";
constexpr const char* kShortcutStickyKey = "bars_magnetized";

[[nodiscard]] std::string BarKey(std::size_t bar, const char* what) {
  std::array<char, 48> text{};
  std::snprintf(text.data(), text.size(), kShortcutBarKeys, bar + 1, what);
  return std::string(text.data());
}

}  // namespace

std::span<const Language> KnownLanguages() { return kLanguages; }

void GameOptions::DeclareShortcutBars(SettingsFile& into) {
  // ⚠ UNE BOUCLE ET NON QUARANTE LIGNES. Les huit barres portent les mêmes cinq
  // réglages ; les écrire à la main serait quarante occasions de se tromper de
  // numéro, et la quarante-et-unième le jour où une sixième s'ajoute.
  //
  // ⚠ ET C'EST L'ADRESSE DU CHAMP QU'ON DÉCLARE, comme partout ailleurs : la
  // lecture et l'écriture parcourent la même table, donc elles ne peuvent pas
  // diverger. Voir `settings_file.h`.
  into.Declare(kShortcutShowKeysKey, &shortcut_bars_.show_keys);
  into.Declare(kShortcutStickyKey, &shortcut_bars_.sticky);
  for (std::size_t bar = 0; bar < ro::ui::kShortcutBarCount; ++bar) {
    ro::ui::ShortcutBarLayout& look = shortcut_bars_.bars[bar];
    into.Declare(BarKey(bar, "shown"), &look.shown);
    into.Declare(BarKey(bar, "passthrough"), &look.click_through);
    into.Declare(BarKey(bar, "columns"), &look.columns);
    into.Declare(BarKey(bar, "slots"), &look.shown_slots);
    into.Declare(BarKey(bar, "size"), &look.slot_side_px);
    into.Declare(BarKey(bar, "spacing"), &look.gap_px);
    // L'APPARENCE, PAR BARRE. ⚠ MÊME BOUCLE, et c'est ce qui la rend tenable :
    // treize réglages de plus écrits à la main pour huit barres, ce serait cent
    // quatre lignes et autant d'occasions de se tromper de numéro.
    into.Declare(BarKey(bar, "background_empty"), &look.empty_color);
    into.Declare(BarKey(bar, "background_skill"), &look.skill_color);
    into.Declare(BarKey(bar, "background_item"), &look.item_color);
    into.Declare(BarKey(bar, "border"), &look.border_color);
    into.Declare(BarKey(bar, "border_thickness"), &look.border_thickness);
    into.Declare(BarKey(bar, "key_color"), &look.key_color);
    into.Declare(BarKey(bar, "key_size"), &look.key_scale);
    into.Declare(BarKey(bar, "number_color"), &look.count_color);
    into.Declare(BarKey(bar, "number_size"), &look.count_scale);
    into.Declare(BarKey(bar, "countdown_color"), &look.countdown_color);
    into.Declare(BarKey(bar, "shadow_color"), &look.shadow_color);
    into.Declare(BarKey(bar, "shadow_kind"), &look.shadow_kind);
    into.Declare(BarKey(bar, "shadow_offset"), &look.shadow_offset);
  }
}

void GameOptions::DeclareEntityBars(SettingsFile& into) {
  into.Declare(kEntityBarsShownKey, &entity_bars_.shown);
  into.Declare(kEntityBarsHideVendorsKey, &entity_bars_.hide_vendors);

  // ⚠ UNE BOUCLE, COMME LES JAUGES : six espèces fois vingt-quatre clés écrites
  // à la main seraient cent quarante-quatre occasions de se tromper de champ, et
  // la faute ne se verrait qu'au chargement suivant.
  for (std::size_t kind = 0; kind < ro::ui::kEntityBarKindCount; ++kind) {
    ro::ui::EntityBarLook& look = entity_bars_.kinds[kind];
    into.Declare(EntityBarKey(kind, "shown"), &look.shown);
    into.Declare(EntityBarKey(kind, "life"), &look.health);
    into.Declare(EntityBarKey(kind, "magic"), &look.magic);
    // ⚠ UN RANG D'ÉNUMÉRATION SE DÉCLARE EN `int*`, et se relit borné : le
    // fichier s'ouvre dans un éditeur de texte, et un rang hors bornes rendu
    // tel quel serait un comportement indéfini. Voir `EntityBarWhenOf`.
    into.Declare(EntityBarKey(kind, "when"), reinterpret_cast<int*>(&look.when));
    into.Declare(EntityBarKey(kind, "hide_dead"), &look.hide_dead);
    into.Declare(EntityBarKey(kind, "under_the_feet"), &look.under_feet);
    into.Declare(EntityBarKey(kind, "gap"), &look.offset_pixels);
    into.Declare(EntityBarKey(kind, "width"), &look.width_pixels);
    into.Declare(EntityBarKey(kind, "height"), &look.height_pixels);
    into.Declare(EntityBarKey(kind, "height_magic"), &look.magic_height_pixels);
    into.Declare(EntityBarKey(kind, "inner_gap"), &look.gap_pixels);
    into.Declare(EntityBarKey(kind, "color"), &look.fill);
    into.Declare(EntityBarKey(kind, "color_alert"), &look.low_fill);
    into.Declare(EntityBarKey(kind, "threshold_alert"), &look.low_share);
    into.Declare(EntityBarKey(kind, "gradient"), &look.gradient);
    into.Declare(EntityBarKey(kind, "color_magic"), &look.magic_fill);
    into.Declare(EntityBarKey(kind, "background"), &look.background);
    into.Declare(EntityBarKey(kind, "frame"), &look.border);
    into.Declare(EntityBarKey(kind, "frame_color"), &look.border_color);
    into.Declare(EntityBarKey(kind, "rounding"), &look.rounding);
    into.Declare(EntityBarKey(kind, "opacity"), &look.opacity);
    into.Declare(EntityBarKey(kind, "text"), reinterpret_cast<int*>(&look.text));
    into.Declare(EntityBarKey(kind, "font"), &look.font_pixels);
    into.Declare(EntityBarKey(kind, "text_color"), &look.text_color);
  }
}

void GameOptions::DeclareSkillArea(SettingsFile& into) {
  into.Declare("zone_spell_cast", &skill_area_.cast_shown);
  into.Declare("zone_spell_in_hand", &skill_area_.armed_shown);
  // ⚠ UN RANG D'ÉNUMÉRATION SE DÉCLARE EN `int*`, jamais sur son type sous-
  // jacent : le fichier s'ouvre dans un éditeur de texte, et `Change` le relit
  // borné. Voir `kSkillAreaPattern`.
  into.Declare("zone_spell_drawing", reinterpret_cast<int*>(&skill_area_.pattern));
  into.Declare("zone_spell_joined", &skill_area_.gap_percent);
  into.Declare("zone_spell_color", &skill_area_.color);
}

void GameOptions::DeclareEntityNames(SettingsFile& into) {
  into.Declare(kEntityNamesShownKey, &entity_names_.shown);
  into.Declare(kEntityNamesFontKey, &entity_names_.font_pixels);
  into.Declare(kEntityNamesEmblemKey, &entity_names_.guild_emblem);

  // ⚠ UNE BOUCLE, COMME LES BARRES ET LES JAUGES : six espèces fois douze clés
  // écrites à la main seraient soixante-douze occasions de se tromper de champ,
  // et la faute ne se verrait qu'au chargement suivant.
  for (std::size_t kind = 0; kind < ro::render::kNamePlateKindCount; ++kind) {
    ro::render::EntityNameLook& look = entity_names_.kinds[kind];
    into.Declare(EntityNameKey(kind, "shown"), &look.shown);
    // ⚠ UN RANG D'ÉNUMÉRATION SE DÉCLARE EN `int*`, et se relit borné : le
    // fichier s'ouvre dans un éditeur de texte, et un rang hors bornes rendu tel
    // quel serait un comportement indéfini. Voir `NamePlateWhenOf`.
    into.Declare(EntityNameKey(kind, "when"), reinterpret_cast<int*>(&look.when));
    into.Declare(EntityNameKey(kind, "title"), &look.title);
    into.Declare(EntityNameKey(kind, "following"), &look.aside);
    into.Declare(EntityNameKey(kind, "pennant"), &look.banner);
    into.Declare(EntityNameKey(kind, "badge_boss"), &look.boss_badge);
    into.Declare(EntityNameKey(kind, "color"), &look.color);
    into.Declare(EntityNameKey(kind, "gap_level"), &look.level_gap);
    into.Declare(EntityNameKey(kind, "shadow"), &look.shadow);
    into.Declare(EntityNameKey(kind, "shadow_color"), &look.shadow_color);
    into.Declare(EntityNameKey(kind, "gap"), &look.offset_pixels);
    into.Declare(EntityNameKey(kind, "opacity"), &look.opacity);
  }
}

void GameOptions::DeclareStatBars(SettingsFile& into) {
  into.Declare(kStatBarsShownKey, &stat_bars_.shown);
  into.Declare(kStatBarsLockedKey, &stat_bars_.locked);
  into.Declare(kStatBarsStickyKey, &stat_bars_.sticky);
  into.Declare(kStatBarsCenterMarkKey, &stat_bars_.center_mark);
  into.Declare(kStatBarsKeepMaxedExpKey, &stat_bars_.keep_maxed_exp);
  into.Declare(kStatBarsWarnKey, &stat_bars_.warn_overweight);
  into.Declare(kStatBarsZenyKey, &stat_bars_.zeny_reference);

  // ⚠ UNE BOUCLE, ET LA GÉOMÉTRIE AVEC. Les quatre nombres d'un cadre ne
  // paraissent dans aucun panneau — on les pose à la souris — mais ils se
  // persistent comme le reste, sans quoi le joueur reposerait ses six jauges à
  // chaque lancement.
  for (std::size_t bar = 0; bar < ro::ui::kStatBarCount; ++bar) {
    ro::ui::StatBarLook& look = stat_bars_.bars[bar];
    into.Declare(StatBarKey(bar, "shown"), &look.shown);
    into.Declare(StatBarKey(bar, "color"), &look.fill);
    into.Declare(StatBarKey(bar, "shape"), reinterpret_cast<int*>(&look.shape));
    into.Declare(StatBarKey(bar, "inverted"), &look.reversed);
    // L'APPARENCE, PAR JAUGE. ⚠ MÊME BOUCLE, et c'est ce qui la rend tenable :
    // onze réglages de plus écrits à la main pour six jauges, ce serait
    // soixante-six lignes et autant d'occasions de se tromper de nom.
    into.Declare(StatBarKey(bar, "vertical"), &look.vertical);
    into.Declare(StatBarKey(bar, "relief"), &look.glossy);
    into.Declare(StatBarKey(bar, "text"), reinterpret_cast<int*>(&look.text));
    into.Declare(StatBarKey(bar, "font"), &look.font_pixels);
    into.Declare(StatBarKey(bar, "caption"), &look.label);
    into.Declare(StatBarKey(bar, "rounding"), &look.rounding);
    into.Declare(StatBarKey(bar, "thickness_ring"), &look.ring_thickness);
    into.Declare(StatBarKey(bar, "rule"), &look.border);
    into.Declare(StatBarKey(bar, "background"), &look.background);
    into.Declare(StatBarKey(bar, "text_color"), &look.text_color);
    into.Declare(StatBarKey(bar, "text_offset_x"), &look.text_shift_x);
    into.Declare(StatBarKey(bar, "text_offset_y"), &look.text_shift_y);
    into.Declare(StatBarKey(bar, "rule_color"), &look.border_color);
    into.Declare(StatBarKey(bar, "x"), &look.rect.x);
    into.Declare(StatBarKey(bar, "y"), &look.rect.y);
    into.Declare(StatBarKey(bar, "width"), &look.rect.width);
    into.Declare(StatBarKey(bar, "height"), &look.rect.height);
    into.Declare(StatBarKey(bar, "anchor_x"), &look.rect.across);
    into.Declare(StatBarKey(bar, "anchor_y"), &look.rect.down);
  }
}

void GameOptions::DeclarePvpHud(SettingsFile& into) {
  into.Declare("pvp_locked", &pvp_hud_.locked);
  into.Declare("pvp_magnetized", &pvp_hud_.sticky);
  into.Declare("pvp_marker_center", &pvp_hud_.center_mark);
  into.Declare("pvp_rule", &pvp_hud_.border);

  // ⚠ LA GÉOMÉTRIE SE PERSISTE SANS PARAÎTRE DANS AUCUN PANNEAU, comme celle des
  // jauges libres : on la pose à la souris. Sans cela le joueur reposerait ses
  // deux pièces à chaque lancement.
  //
  // ⚠ ET LES NOMS SONT ÉCRITS EN TOUTES LETTRES plutôt que bâtis par une boucle :
  // elles ne sont que deux, et un fichier de réglages se relit à la main.
  ro::ui::HudRect& rang = pvp_hud_.parts[ro::ui::Rank(ro::ui::PvpHudPart::kRank)];
  into.Declare("pvp_rank_x", &rang.x);
  into.Declare("pvp_rank_y", &rang.y);
  into.Declare("pvp_rank_width", &rang.width);
  into.Declare("pvp_rank_height", &rang.height);
  into.Declare("pvp_rank_anchor_x", &rang.across);
  into.Declare("pvp_rank_anchor_y", &rang.down);

  ro::ui::HudRect& chrono = pvp_hud_.parts[ro::ui::Rank(ro::ui::PvpHudPart::kTimer)];
  into.Declare("pvp_timer_x", &chrono.x);
  into.Declare("pvp_timer_y", &chrono.y);
  into.Declare("pvp_timer_width", &chrono.width);
  into.Declare("pvp_timer_height", &chrono.height);
  into.Declare("pvp_timer_anchor_x", &chrono.across);
  into.Declare("pvp_timer_anchor_y", &chrono.down);
}

void GameOptions::Declare(SettingsFile& into) {
  // LA PAGE DU PANNEAU QUI MONTRE les blocs qui suivent : voir
  // `SettingsFile::PageScope`. Rien de ce que ne montre aucune ligne n'en a.
  const auto page = [&into](OptionSection section) { into.Page(SettingsPage(section)); };
  const auto no_page = [&into] { into.Page(kNoSettingsPage); };
  // ⚠ L'ORDRE EST CELUI DU FICHIER : les réglages d'un même sujet se déclarent
  // ensemble pour se lire ensemble.
  page(OptionSection::kSound);
  into.Section("Le son");
  {
    // LE SON TIENT À LA MACHINE : un profil de réglages ne le change pas.
    const SettingsFile::MachineScope machine(into);
    DeclareSoundOptions(into, sound_options_);
  }
  page(OptionSection::kInterface);
  into.Section("Les effets");
  into.Declare(kEffectsShownKey, &effects_shown_);
  into.Declare(kMinimalEffectsKey, &minimal_effects_);
  no_page();
  into.Section("La langue et l'échelle de l'interface");
  {
    // LA LANGUE, LE COMPTE ET L'ÉCHELLE TIENNENT À LA MACHINE.
    const SettingsFile::MachineScope machine(into);
    into.Declare(kLanguageKey, &language_);
    // ⚠ LE COMPTE RETENU VIT ICI et non dans une fenêtre d'options : voir
    // `GameOptions::login_account`. Le mot de passe, lui, n'a aucune clé.
    into.Declare(kLoginAccountKey, &login_account_);
    page(OptionSection::kInterface);
    into.Declare(kInterfaceScaleKey, &interface_scale_);
  }
  page(OptionSection::kDisplay);
  into.Section("L'affichage");
  {
    // L'ÉCRAN TIENT À LA MACHINE : charger un profil ne le fait pas clignoter.
    const SettingsFile::MachineScope machine(into);
    into.Declare(kScreenNameKey, &screen_.name);
    into.Declare(kScreenRankKey, &screen_.rank);
    into.Declare(kDisplayModeKey, &display_mode_);
    into.Declare(kResolutionKey, &resolution_);
    into.Declare(kLockWindowSizeKey, &lock_window_size_);
    into.Declare(kVerticalSyncKey, &vertical_sync_);
    into.Declare(kFrameLimitKey, &frame_limit_);
  }
  into.Declare(kFrameMeterKey, &frame_meter_);
  into.Declare(kFrameMeterLockedKey, &frame_meter_locked_);
  into.Declare(kFrameMeterGraphKey, &frame_meter_graph_);
  into.Declare(kFrameMeterPingKey, &frame_meter_ping_);
  {
    const SettingsFile::MachineScope machine(into);
    no_page();
    into.Declare(kWindowPlaceKey, &window_place_);
  }
  page(OptionSection::kInterface);
  into.Section("La grille d'alignement");
  into.Declare(kAlignGridKey, &align_grid_.shown);
  into.Declare(kAlignGridMagnetKey, &align_grid_.magnetic);
  page(OptionSection::kShortcutBars);
  into.Section("Les huit barres de raccourcis");
  DeclareShortcutBars(into);
  page(OptionSection::kStatBars);
  into.Section("Les six jauges libres");
  DeclareStatBars(into);
  page(OptionSection::kInterface);
  into.Section("Le classement PVP");
  DeclarePvpHud(into);
  page(OptionSection::kEntityBars);
  into.Section("Les barres de vie et de magie des entités");
  DeclareEntityBars(into);
  into.Section("Les étiquettes des entités");
  DeclareEntityNames(into);
  page(OptionSection::kGameplay);
  into.Section("La zone des sorts au sol");
  DeclareSkillArea(into);
  page(OptionSection::kShortcutBars);
  into.Section("Le sort en main");
  DeclareArmedSkill(into);
  no_page();
  pages_.Declare(into);
  page(OptionSection::kDamage);
  into.Section("Les dégâts flottants");
  DeclareDamage(into);
  into.Section("Le sac, la charrette et le coffre");
  page(OptionSection::kInventory);
  into.Declare(kInventoryFilterKey, &inventory_filter_);
  page(OptionSection::kCart);
  into.Declare(kCartFilterKey, &cart_filter_);
  into.Declare(kCartSizeKey, &cart_locked_);
  page(OptionSection::kStorage);
  into.Declare(kStorageFilterKey, &storage_filter_);
  page(OptionSection::kInventory);
  into.Declare(kInventoryStorageButtonKey, &inventory_storage_button_);
  page(OptionSection::kCart);
  into.Declare(kCartStorageButtonKey, &cart_storage_button_);
  page(OptionSection::kStorage);
  into.Declare(kStoragePeerButtonsKey, &storage_peer_buttons_);
  page(OptionSection::kInventory);
  into.Declare(kInventoryTabsVerticalKey, &inventory_tabs_vertical_);
  into.Declare(kInventoryFreeLayoutKey, &inventory_free_layout_);
  page(OptionSection::kCart);
  into.Declare(kCartTabsVerticalKey, &cart_tabs_vertical_);
  page(OptionSection::kStorage);
  into.Declare(kStorageTabsVerticalKey, &storage_tabs_vertical_);
  into.Declare(kStorageTypeTabsKey, &storage_type_tabs_);
  into.Declare(kStorageTabImagesKey, &storage_tab_images_);
  into.Declare(kStorageShelfRowKey, &storage_shelf_row_);
  into.Declare(kStorageSlotColumnKey, &storage_slot_column_);
  into.Declare(kStorageItemIdColumnKey, &storage_item_id_column_);
  into.Declare(kStorageCardSlotsColumnKey, &storage_card_slots_column_);
  into.Declare(kStorageValueColumnKey, &storage_value_column_);
  into.Declare(kStorageTotalValueKey, &storage_total_value_);
  page(OptionSection::kInventory);
  into.Declare(kInventoryHoverDescriptionKey, &inventory_hover_description_);
  page(OptionSection::kCart);
  into.Declare(kCartHoverDescriptionKey, &cart_hover_description_);
  page(OptionSection::kStorage);
  into.Declare(kStorageHoverDescriptionKey, &storage_hover_description_);
  page(OptionSection::kInterface);
  into.Declare(kCharacterHoverDescriptionKey, &character_hover_description_);
  into.Section("La fiche de personnage");
  into.Declare(kGrimoireSmoothIconsKey, &grimoire_smooth_icons_);
  into.Declare(kCharacterStaffPaneKey, &character_staff_pane_);
  into.Declare(kStaffBattleIgnoreKey, &staff_battle_ignore_);
  into.Declare(kCharacterAvatarPoseKey, &character_avatar_pose_);
  into.Declare(kCharacterAvatarAnimatedKey, &character_avatar_animated_);
  into.Declare(kCharacterAvatarDirectionKey, &character_avatar_direction_);
  page(OptionSection::kInterface);
  into.Section("Les dialogues de PNJ");
  into.Declare(kNpcMenuSearchKey, &npc_menu_search_);
  into.Section("L'avis de pose");
  into.Declare(kUnlockNoticeKey, &unlock_notice_);
  page(OptionSection::kGameplay);
  into.Section("Les refus de skill");
  into.Declare(kSkillFailShownKey, &skill_fail_shown_);
  page(OptionSection::kStatuses);
  into.Section("La barre d'états");
  into.Declare(kStatusBarShownKey, &status_bar_.shown);
  into.Declare(kStatusBarLockedKey, &status_bar_.locked);
  into.Declare(kStatusBarBorderKey, &status_bar_.border);
  into.Declare(kStatusBarBackgroundKey, &status_bar_.background_color);
  into.Declare(kStatusBarSideKey, &status_bar_.side_px);
  into.Declare(kStatusBarGapKey, &status_bar_.gap_px);
  into.Declare(kStatusBarMostKey, &status_bar_.most);
  into.Declare(kStatusBarRowsKey, &status_bar_.rows);
  into.Declare(kStatusBarAnchorKey, &status_bar_.anchor);
  into.Declare(kStatusBarSortKey, &status_bar_.sort);
  into.Declare(kStatusBarCountdownKey, &status_bar_.show_countdown);
  into.Declare(kStatusBarCountdownSizeKey, &status_bar_.countdown_px);
  into.Declare(kStatusBarCountdownBackgroundKey, &status_bar_.countdown_background);
  into.Declare(kStatusBarSweepKey, &status_bar_.sweep);
  into.Declare(kStatusBarSweepColorKey, &status_bar_.sweep_color);
  into.Declare(kStatusBarOpacityKey, &status_bar_.opacity);
  into.Declare(kStatusBarIconBackgroundKey, &status_bar_.icon_background);
  page(OptionSection::kInventory);
  into.Declare(kInventoryLockSizeKey, &inventory_locked_);
  into.Declare(kInventoryDropLockKey, &inventory_drop_lock_);
  into.Declare(kInventorySellLockKey, &inventory_sell_lock_);
  into.Declare(kInventorySortKey, &inventory_sort_);
  page(OptionSection::kInterface);
  into.Declare(kAlignGridCellKey, &align_grid_.cell_pixels);
  into.Declare(kAlignGridColorKey, &align_grid_.color);
  page(OptionSection::kItemObtain);
  into.Section("Le bandeau d'objet obtenu");
  into.Declare(kItemObtainLinesKey, &item_obtain_.most_lines);
  into.Declare(kItemObtainNewestOnTopKey, &item_obtain_.newest_on_top);
  into.Declare(kItemObtainMergeSameKey, &item_obtain_.merge_same);
  into.Declare(kItemObtainDurationKey, &item_obtain_.duration_ms);
  into.Declare(kItemObtainLockedKey, &item_obtain_.locked);
  into.Declare(kItemObtainPosXKey, &item_obtain_.pos_x);
  into.Declare(kItemObtainPosYKey, &item_obtain_.pos_y);
  into.Declare(kItemObtainRowGapKey, &item_obtain_.row_gap_px);
  into.Declare(kItemObtainPadVerticalKey, &item_obtain_.pad_vertical_px);
  into.Declare(kItemObtainFontScaleKey, &item_obtain_.font_scale_percent);
  into.Declare(kItemObtainIconKey, &item_obtain_.show_icon);
  into.Declare(kItemObtainFrameKey, &item_obtain_.show_frame);
  into.Declare(kItemObtainBackgroundKey, &item_obtain_.background_shown);
  into.Declare(kItemObtainBackgroundColorKey, &item_obtain_.background);
  into.Declare(kItemObtainRoundingKey, &item_obtain_.rounding_px);
  into.Declare(kItemObtainBorderKey, &item_obtain_.border_shown);
  into.Declare(kItemObtainBorderColorKey, &item_obtain_.border);
  into.Declare(kItemObtainBorderThicknessKey, &item_obtain_.border_px);
  into.Declare(kItemObtainTextColorKey, &item_obtain_.text);
  into.Declare(kItemObtainQuantityColorKey, &item_obtain_.quantity);
  no_page();
  into.Section("Les outils du staff");
  into.Declare(kStaffFlatGroundKey, &staff_tools_.flat_ground.on);
  into.Declare(kStaffFlatGroundColorKey, &staff_tools_.flat_ground.color);
  into.Declare(kStaffFreeCameraKey, &staff_tools_.free_camera);
  into.Declare(kStaffCameraHudKey, &staff_tools_.camera_hud);
  into.Declare(kStaffSoundZonesKey, &staff_tools_.sound_zones);
  into.Declare(kStaffPickBoxesKey, &staff_tools_.picking.boxes_shown);
  into.Declare(kStaffPickFloorForcedKey, &staff_tools_.picking.actor_floor_forced);
  into.Declare(kStaffPickFloorPixelsKey, &staff_tools_.picking.actor_floor_pixels);
  into.Declare(kStaffPickNegativeKey, &staff_tools_.picking.shrink_negative_ids);
  into.Declare(kStaffPickNegativePercentKey, &staff_tools_.picking.negative_id_percent);
  into.Section("Les dix macros de chat");
  // LES DIX MACROS DE CHAT. ⚠ DES CLÉS NUMÉROTÉES À PARTIR DE UN, comme la
  // fenêtre les montre : « macro_1 » est la ligne d'Alt+1, et un fichier qu'on
  // ouvre à la main doit se lire sans compter à partir de zéro.
  for (std::size_t slot = 0; slot < chat_macros_.size(); ++slot) {
    into.Declare(MacroKey(slot), &chat_macros_[slot]);
  }
  page(OptionSection::kCapture);
  into.Section("L'enregistreur de zone");
  {
    // UNE ZONE EN PIXELS D'ÉCRAN TIENT À L'ÉCRAN, donc à la machine.
    const SettingsFile::MachineScope machine(into);
    into.Declare(kCaptureZoneXKey, &capture_.zone.left);
    into.Declare(kCaptureZoneYKey, &capture_.zone.top);
    into.Declare(kCaptureZoneWidthKey, &capture_.zone.width);
    into.Declare(kCaptureZoneHeightKey, &capture_.zone.height);
  }
  into.Declare(kCaptureFpsKey, &capture_.frames_per_second);
  into.Declare(kCaptureSecondsKey, &capture_.seconds);
  into.Declare(kCaptureWidthKey, &capture_.widest_output);
  into.Declare(kCaptureDelayKey, &capture_.start_delay_seconds);
  page(OptionSection::kGameplay);
  into.Section("Le jeu");
  into.Declare(kFixedCameraKey, &fixed_camera_);
  into.Declare(kExpandedZoomKey, &expanded_zoom_);
  into.Declare(kNoShiftKey, &no_shift_);
  into.Declare(kNoCtrlKey, &no_ctrl_);
  into.Declare(kSpeakScreamKey, &speak_scream_);
  into.Declare(kSpeakFrostJokeKey, &speak_frost_joke_);
  into.Declare(kItemDropArcKey, &item_drop_.arc);
  into.Declare(kItemDropArcHeightKey, &item_drop_.height_units);
  into.Declare(kItemDropArcKickKey, &item_drop_.kick_units);
  into.Declare(kItemDropArcSourceHeightKey, &item_drop_.source_height_fraction);
  into.Declare(kItemDropArcDurationKey, &item_drop_.duration_ms);
  page(OptionSection::kChat);
  into.Section("La conversation");
  into.Declare(kChatFontKey, &chat_look_.font_percent);
  into.Declare(kChatPaddingKey, &chat_look_.padding_pixels);
  into.Declare(kChatLineGapKey, &chat_look_.line_gap_pixels);
  into.Declare(kChatTimestampsKey, &chat_look_.timestamps);
  into.Declare(kChatGrowInputKey, &chat_look_.grow_input);
  into.Declare(kChatBattleModeKey, &chat_look_.battle_mode);
  page(OptionSection::kBubbles);
  into.Section("Les bulles de parole");
  into.Declare(kBubbleShowOwnKey, &bubbles_.show_own);
  into.Declare(kBubbleFadeKey, &bubbles_.fade);
  into.Declare(kBubbleClientTimingKey, &bubbles_.client_timing);
  into.Declare(kBubbleBaseMsKey, &bubbles_.base_ms);
  into.Declare(kBubblePerCharacterMsKey, &bubbles_.per_character_ms);
  into.Declare(kBubbleRiseKey, &bubbles_.rise_pixels);
  into.Declare(kBubbleWidthKey, &bubbles_.width_fraction);
  into.Declare(kBubbleOpacityKey, &bubbles_.opacity);
  into.Declare(kBubbleBackgroundKey, &bubbles_.background);
  into.Declare(kBubbleCornerKey, &bubbles_.corner_radius);
  into.Declare(kBubblePaddingXKey, &bubbles_.padding_x);
  into.Declare(kBubblePaddingYKey, &bubbles_.padding_y);
  into.Declare(kBubbleBackgroundColorKey, &bubbles_.background_color);
  into.Declare(kBubbleBorderColorKey, &bubbles_.border_color);
  page(OptionSection::kInterface);
  into.Declare(kFontFamilyKey, &font_family_);
  no_page();
  {
    // ⚠ LE FOND ANIMÉ SE RÈGLE À L'ÉCRAN DE CONNEXION, hors de toute partie :
    // suivi par un profil, l'entrée en jeu reposerait l'ancienne valeur.
    const SettingsFile::MachineScope machine(into);
    into.Declare(kBackdropKey, &backdrop_wanted_);
  }
  page(OptionSection::kChat);
  into.Declare(kChatKeepHistoryKey, &chat_look_.keep_history);
  into.Declare(kChatKeptLinesKey, &chat_look_.kept_lines);
  into.Declare(kChatKeepTypedKey, &chat_look_.keep_typed);
  into.Declare(kChatKeptTypedKey, &chat_look_.kept_typed);
  into.Declare(kChatKeepLogKey, &chat_look_.keep_log);
  into.Declare(kChatDiagnosticKey, &chat_look_.diagnostic);
  into.Declare(kChatKeptPerKindKey, &chat_kept_per_kind_);
  into.Declare(kChatBodyKey, &chat_look_.body);
  into.Declare(kChatBorderKey, &chat_look_.border);
  into.Declare(kChatTabsKey, &chat_look_.tabs);
  no_page();
  pages_.NoteRestorable(into);
}

}  // namespace moonclient
