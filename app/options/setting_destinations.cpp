// LA TABLE DES DESTINATIONS D'UN LIEN DE RÉGLAGE. Voir `setting_links.h` : ses
// règles de clés, et pourquoi il n'y en a qu'une.
//
// ⚠ L'ORDRE COMPTE À UN ENDROIT : la première clé d'une destination est celle
// qu'un Maj+clic pose. Les suivantes ne sont là que pour être LUES — ce sont
// d'autres noms qui tombent sur la même page chez nous.

#include <array>

#include "ro/core/i18n.h"
#include "ro/ui/achievement_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/commands_page.h"
#include "ro/ui/hotkey_window.h"
#include "ro/ui/party_friend_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/stat_bars.h"

#include "input/hotkeys.h"
#include "options/damage_page_names.h"
#include "options/entity_page_tabs.h"
#include "options/game_option_ids.h"
#include "options/pet_options.h"
#include "options/setting_links.h"

namespace moonclient {
namespace {

using ro::core::TrText;
using Kind = SettingDestinationKind;

// UNE PAGE DU PANNEAU, entière.
[[nodiscard]] constexpr SettingDestination Page(std::string_view key, const char* label,
                                                OptionSection section) {
  return SettingDestination{.key = key, .label = label, .section = section};
}

// UN BLOC D'UNE PAGE : la page s'ouvre sur la rangée qui porte son intertitre.
[[nodiscard]] constexpr SettingDestination PageRow(std::string_view key, const char* label,
                                                   OptionSection section, int row) {
  return SettingDestination{.key = key, .label = label, .section = section, .row = row};
}

// UN ONGLET D'UNE PAGE, par son rang dans la page.
template <typename Tab>
[[nodiscard]] constexpr SettingDestination PageTab(std::string_view key, const char* label,
                                                   OptionSection section, Tab tab) {
  return SettingDestination{
      .key = key, .label = label, .section = section, .tab = static_cast<int>(tab)};
}

// UNE BARRE DE RACCOURCIS : « Barre » et son numéro, compté à partir de un.
[[nodiscard]] constexpr SettingDestination ShortcutBar(std::string_view key, int rank) {
  return SettingDestination{.key = key,
                            .label = TrText("Barre"),
                            .section = OptionSection::kShortcutBars,
                            .tab = rank,
                            .ordinal = rank + 1};
}

// UN ONGLET DE FENÊTRE, par son rang dans la fenêtre.
template <typename Tab>
[[nodiscard]] constexpr SettingDestination WindowTab(std::string_view key, const char* label,
                                                     Kind kind, Tab tab) {
  return SettingDestination{.key = key, .label = label, .kind = kind, .tab = static_cast<int>(tab)};
}

// UN SOUS-ONGLET DES SUCCÈS : la sous-catégorie, par son rang sous son onglet.
[[nodiscard]] constexpr SettingDestination AchievementSubtab(std::string_view key,
                                                             const char* label, int tab,
                                                             int subtab) {
  return SettingDestination{
      .key = key, .label = label, .kind = Kind::kAchievements, .tab = tab, .subtab = subtab};
}

// LES ONGLETS DES CATÉGORIES DE SUCCÈS. ⚠ LE NUMÉRO EST CELUI DE LA CATÉGORIE
// DANS LE CATALOGUE, compté à partir de un, dans l'ordre de l'arbre.
constexpr int kAchievementGeneralTab = ro::ui::AchievementTabOfMajor(1);
constexpr int kAchievementAdventureTab = ro::ui::AchievementTabOfMajor(2);
constexpr int kAchievementBattleTab = ro::ui::AchievementTabOfMajor(3);
constexpr int kAchievementQuestTab = ro::ui::AchievementTabOfMajor(4);
constexpr int kAchievementMemorialTab = ro::ui::AchievementTabOfMajor(5);
constexpr int kAchievementFeatTab = ro::ui::AchievementTabOfMajor(6);

// LES PREMIÈRES RANGÉES DES BLOCS qu'un lien désigne : celles qui portent leur
// intertitre.
constexpr int kCharacterSheetRow = static_cast<int>(GameOption::kCharacterHoverDescription);
constexpr int kPetSheetRow = PetRowId(PetRow::kConfirmReturnToEgg);

using ro::ui::CharacterTab;
using ro::ui::CommandsTab;
using ro::ui::PartyFriendTab;
using ro::ui::StatBar;

constexpr auto kDestinations = std::to_array<SettingDestination>({
    // ── CE QUE LE PROGRAMME FAIT ── les libellés sont ceux de la navigation.
    Page("display", TrText("Display"), OptionSection::kDisplay),
    Page("graphics", TrText("Graphics"), OptionSection::kGraphics),
    Page("gs_graphics", TrText("Graphics"), OptionSection::kGraphics),
    Page("sound", TrText("Sound"), OptionSection::kSound),
    // Les sons des monstres : chez nous, un bloc de « Sound ».
    Page("sprite_sounds", TrText("Sound"), OptionSection::kSound),
    Page("gameplay", TrText("Gameplay"), OptionSection::kGameplay),
    // Les sections que notre « Gameplay » rassemble.
    Page("skill_range", TrText("Gameplay"), OptionSection::kGameplay),
    Page("skill_area", TrText("Gameplay"), OptionSection::kGameplay),
    Page("quick_cast", TrText("Gameplay"), OptionSection::kGameplay),
    Page("greyworld", TrText("Gameplay"), OptionSection::kGameplay),
    Page("afk_screen", TrText("Écran de veille"), OptionSection::kAfkScreen),
    Page("damage", TrText("Dégâts"), OptionSection::kDamage),
    PageTab("damage_shown", TrText("Ce qui se montre"), OptionSection::kDamage, DamageTab::kShown),
    PageTab("damage_motion", TrText("Allure"), OptionSection::kDamage, DamageTab::kMotion),
    PageTab("damage_colors", TrText("Couleurs"), OptionSection::kDamage, DamageTab::kColors),
    Page("entities", TrText("Entités"), OptionSection::kEntityBars),
    // Les barres de cast : chez nous, elles se règlent avec les autres barres
    // posées sur les entités.
    Page("cast_bar", TrText("Entités"), OptionSection::kEntityBars),
    PageTab("entities_own", TrText("Moi"), OptionSection::kEntityBars, EntityPageTab::kOwn),
    PageTab("entities_party", TrText("Groupe"), OptionSection::kEntityBars, EntityPageTab::kParty),
    PageTab("entities_players", TrText("Joueurs"), OptionSection::kEntityBars,
            EntityPageTab::kPlayer),
    PageTab("entities_monsters", TrText("Monstres"), OptionSection::kEntityBars,
            EntityPageTab::kMonster),
    PageTab("entities_boss", TrText("Boss"), OptionSection::kEntityBars, EntityPageTab::kBoss),
    PageTab("entities_npc", TrText("PNJ"), OptionSection::kEntityBars, EntityPageTab::kOther),
    PageTab("entities_ground_items", TrText("Items au sol"), OptionSection::kEntityBars,
            EntityPageTab::kGroundItem),
    PageTab("entities_signboards", TrText("Enseignes"), OptionSection::kEntityBars,
            EntityPageTab::kSignboards),
    Page("capture", TrText("Capture d'écran"), OptionSection::kCapture),
    // L'enregistreur de zone : chez nous, c'est la section qui règle ce que
    // l'écran rend.
    Page("zone_recorder", TrText("Capture d'écran"), OptionSection::kCapture),

    // ── CE QUE CHAQUE FENÊTRE FAIT ──
    Page("interface", TrText("Interface"), OptionSection::kInterface),
    // ⚠ « npc » tombe dans « Interface », qui porte ses réglages.
    Page("npc", TrText("Interface"), OptionSection::kInterface),
    Page("skin", TrText("Skin"), OptionSection::kSkin),
    Page("minimap", TrText("Minimap"), OptionSection::kMinimap),
    Page("menu_icons", TrText("Icônes du menu"), OptionSection::kMenuIcons),
    Page("skill_bar", TrText("Barres de raccourcis"), OptionSection::kShortcutBars),
    ShortcutBar("skill_bar_1", 0),
    ShortcutBar("skill_bar_2", 1),
    ShortcutBar("skill_bar_3", 2),
    ShortcutBar("skill_bar_4", 3),
    ShortcutBar("skill_bar_5", 4),
    ShortcutBar("skill_bar_6", 5),
    ShortcutBar("skill_bar_7", 6),
    ShortcutBar("skill_bar_8", 7),
    Page("stat_bars", TrText("Basic Info"), OptionSection::kStatBars),
    PageTab("stat_bars_hp", TrText("Vie"), OptionSection::kStatBars, StatBar::kHealth),
    PageTab("stat_bars_sp", TrText("Magie"), OptionSection::kStatBars, StatBar::kMagic),
    PageTab("stat_bars_base_exp", TrText("Exp. base"), OptionSection::kStatBars, StatBar::kBaseExp),
    PageTab("stat_bars_job_exp", TrText("Exp. job"), OptionSection::kStatBars, StatBar::kJobExp),
    PageTab("stat_bars_weight", TrText("Poids"), OptionSection::kStatBars, StatBar::kWeight),
    PageTab("stat_bars_zeny", TrText("Bourse"), OptionSection::kStatBars, StatBar::kZeny),
    PageTab("stat_bars_cast", TrText("Cast"), OptionSection::kStatBars, StatBar::kCast),
    Page("status_icons", TrText("États"), OptionSection::kStatuses),
    Page("dps", TrText("Compteur de dégâts"), OptionSection::kDamageMeter),
    Page("target_frame", TrText("Fenêtre de cible"), OptionSection::kTarget),
    Page("targeting", TrText("Fenêtre de cible"), OptionSection::kTarget),
    Page("monster_info", TrText("Fiche de monstre"), OptionSection::kMonsterSheet),
    Page("mvp_tracker", TrText("Carnet de chasse MVP"), OptionSection::kMvpTracker),
    Page("context_menu", TrText("Menu contextuel"), OptionSection::kContextMenu),
    Page("party_frames", TrText("Groupe (grille)"), OptionSection::kPartyGrid),
    Page("party_friend", TrText("Groupe / Amis"), OptionSection::kPartyFriend),
    Page("chat", TrText("Conversation"), OptionSection::kChat),
    Page("bubbles", TrText("Bulles de chat"), OptionSection::kBubbles),
    Page("broadcast_balloons", TrText("Bulles d'annonces"), OptionSection::kBroadcastBalloons),
    Page("item_toast", TrText("Item obtenu"), OptionSection::kItemObtain),
    Page("inventory", TrText("Inventory"), OptionSection::kInventory),
    Page("cart", TrText("Cart"), OptionSection::kCart),
    Page("storage", TrText("Storage"), OptionSection::kStorage),
    Page("craft_atlas", TrText("Recettes"), OptionSection::kCraftAtlas),
    Page("make_item", TrText("Fabrication"), OptionSection::kMakeItem),
    Page("refine", TrText("Refine"), OptionSection::kWeaponRefine),
    Page("card_album", TrText("Album de cartes"), OptionSection::kCardAlbum),
    Page("desc", TrText("Descriptions"), OptionSection::kDescriptions),
    // La fiche du personnage et celle du pet : un bloc d'« Interface » chacune.
    PageRow("character", TrText("Fiche du personnage"), OptionSection::kInterface,
            kCharacterSheetRow),
    // Le grimoire : la fiche du personnage chez nous.
    PageRow("skills", TrText("Fiche du personnage"), OptionSection::kInterface, kCharacterSheetRow),
    PageRow("pet", TrText("Fiche de pet"), OptionSection::kInterface, kPetSheetRow),
    Page("quest_tracker", TrText("Quêtes et suivi"), OptionSection::kQuestTracker),
    Page("commands", TrText("Commands Settings"), OptionSection::kCommands),
    // Les onglets que la page dessine elle-même.
    PageTab("commands_general", TrText("Général"), OptionSection::kCommands, CommandsTab::kGeneral),
    PageTab("commands_autoloot", TrText("Autoloots"), OptionSection::kCommands,
            CommandsTab::kAutoloot),

    // ── LES ONGLETS DE LA FICHE DU PERSONNAGE ──
    WindowTab("sheet_equipment", TrText("Équipement"), Kind::kSheet, CharacterTab::kEquipment),
    WindowTab("sheet_costume", TrText("Costume"), Kind::kSheet, CharacterTab::kCostume),
    WindowTab("sheet_presets", TrText("Presets"), Kind::kSheet, CharacterTab::kPresets),
    WindowTab("sheet_titles", TrText("Titres"), Kind::kSheet, CharacterTab::kTitles),
    WindowTab("sheet_guild", TrText("Guilde"), Kind::kSheet, CharacterTab::kGuild),
    WindowTab("sheet_grimoire", TrText("Grimoire"), Kind::kSheet, CharacterTab::kGrimoire),
    WindowTab("sheet_homunculus", TrText("Homoncule"), Kind::kSheet, CharacterTab::kHomunculus),
    WindowTab("sheet_mercenary", TrText("Mercenaire"), Kind::kSheet, CharacterTab::kMercenary),

    // ── LES ONGLETS DES SUCCÈS ── les catégories dans l'ordre du catalogue.
    WindowTab("achievements_summary", TrText("Résumé"), Kind::kAchievements,
              ro::ui::kAchievementSummaryTab),
    WindowTab("achievements_all", TrText("Tous"), Kind::kAchievements, ro::ui::kAchievementAllTab),
    WindowTab("achievements_general", TrText("Général"), Kind::kAchievements,
              kAchievementGeneralTab),
    WindowTab("achievements_adventure", TrText("Aventure"), Kind::kAchievements,
              kAchievementAdventureTab),
    WindowTab("achievements_battle", TrText("Combat"), Kind::kAchievements, kAchievementBattleTab),
    WindowTab("achievements_quest", TrText("Quête"), Kind::kAchievements, kAchievementQuestTab),
    WindowTab("achievements_memorial", TrText("Memorial"), Kind::kAchievements,
              kAchievementMemorialTab),
    WindowTab("achievements_feat", TrText("Exploit"), Kind::kAchievements, kAchievementFeatTab),

    // ── LES SOUS-ONGLETS DES SUCCÈS ── catégorie › sous-catégorie, dans l'ordre
    // de l'arbre. Les libellés sont ceux de l'arbre.
    AchievementSubtab("achievements_general_character", TrText("Personnage"),
                      kAchievementGeneralTab, 0),
    AchievementSubtab("achievements_general_action", TrText("Action"), kAchievementGeneralTab, 1),
    AchievementSubtab("achievements_general_other", TrText("Autre"), kAchievementGeneralTab, 2),
    AchievementSubtab("achievements_adventure_rune_midgarts", TrText("Rune Midgarts"),
                      kAchievementAdventureTab, 0),
    AchievementSubtab("achievements_adventure_schwarzwald", TrText("Schwartzvald"),
                      kAchievementAdventureTab, 1),
    AchievementSubtab("achievements_adventure_arunafeltz", TrText("Arunafeltz"),
                      kAchievementAdventureTab, 2),
    AchievementSubtab("achievements_adventure_new_world", TrText("Nouveau Monde"),
                      kAchievementAdventureTab, 3),
    AchievementSubtab("achievements_adventure_localizing", TrText("Localisation"),
                      kAchievementAdventureTab, 4),
    AchievementSubtab("achievements_adventure_dungeon", TrText("Donjon"), kAchievementAdventureTab,
                      5),
    AchievementSubtab("achievements_battle_pvp", TrText("Duel"), kAchievementBattleTab, 0),
    AchievementSubtab("achievements_battle_training", TrText("Entraînement"), kAchievementBattleTab,
                      1),
    AchievementSubtab("achievements_quest_episode", TrText("Épisode"), kAchievementQuestTab, 0),
    AchievementSubtab("achievements_quest_general", TrText("Général"), kAchievementQuestTab, 1),
    AchievementSubtab("achievements_memorial_midgard", TrText("Midgard"), kAchievementMemorialTab,
                      0),
    AchievementSubtab("achievements_memorial_new_world", TrText("Nouveau Monde"),
                      kAchievementMemorialTab, 1),
    AchievementSubtab("achievements_memorial_other", TrText("Autre"), kAchievementMemorialTab, 2),
    AchievementSubtab("achievements_feat_card_album", TrText("Album de cartes"),
                      kAchievementFeatTab, ro::ui::kAchievementFeatCardAlbum),
    AchievementSubtab("achievements_feat_minigames", TrText("Mini-jeux"), kAchievementFeatTab,
                      ro::ui::kAchievementFeatMinigames),

    // ── LES ONGLETS DE GROUPE / AMIS ──
    WindowTab("party_window_party", TrText("Groupe"), Kind::kPartyFriend, PartyFriendTab::kParty),
    WindowTab("party_window_friends", TrText("Amis"), Kind::kPartyFriend, PartyFriendTab::kFriends),

    // ── LES ONGLETS DES RACCOURCIS CLAVIER ── « Tout » précède les autres.
    WindowTab("hotkeys_all", TrText("Tout"), Kind::kHotkeys, ro::ui::kAllHotkeyTabs),
    WindowTab("hotkeys_bars", TrText("Barres"), Kind::kHotkeys, HotkeyTab::kBars),
    WindowTab("hotkeys_interface", TrText("Interface"), Kind::kHotkeys, HotkeyTab::kInterface),
    WindowTab("hotkeys_macros", TrText("Macros"), Kind::kHotkeys, HotkeyTab::kMacros),
    WindowTab("hotkeys_play", TrText("Jeu"), Kind::kHotkeys, HotkeyTab::kPlay),
    WindowTab("hotkeys_camera", TrText("Caméra"), Kind::kHotkeys, HotkeyTab::kCamera),
    WindowTab("hotkeys_tools", TrText("Outils"), Kind::kHotkeys, HotkeyTab::kTools),
});

// ⚠ UNE BARRE PAR CLÉ, NI PLUS NI MOINS : une barre ajoutée sans sa clé ne
// pourrait se lier, et une clé de trop ouvrirait un onglet absent.
static_assert(ro::ui::kShortcutBarCount == 8);
static_assert(ro::ui::kStatBarCount == 7);
static_assert(kEntityPageTabCount == 8);
static_assert(static_cast<std::size_t>(DamageTab::kCount) == 3);
static_assert(kHotkeyTabCount == 6);
static_assert(ro::ui::kAchievementTabCount == kAchievementFeatTab + 1);
static_assert(ro::ui::kCommandsTabCount == 2);

// CHAQUE SOUS-ONGLET DE L'ARBRE A UNE CLÉ ET UNE SEULE, et aucune clé ne
// désigne un sous-onglet que l'arbre n'a pas.
[[nodiscard]] constexpr bool SubtabsMatchTree() {
  for (const SettingDestination& destination : kDestinations) {
    if (destination.subtab == kNoSettingTab) {
      continue;
    }
    if (destination.kind != Kind::kAchievements || destination.tab < 0 ||
        destination.tab >= ro::ui::kAchievementTabCount || destination.subtab < 0 ||
        destination.subtab >=
            ro::ui::kAchievementSubtabCounts[static_cast<std::size_t>(destination.tab)]) {
      return false;
    }
  }
  for (int tab = 0; tab < ro::ui::kAchievementTabCount; ++tab) {
    for (int subtab = 0; subtab < ro::ui::kAchievementSubtabCounts[static_cast<std::size_t>(tab)];
         ++subtab) {
      int keys = 0;
      for (const SettingDestination& destination : kDestinations) {
        if (destination.kind == Kind::kAchievements && destination.tab == tab &&
            destination.subtab == subtab) {
          ++keys;
        }
      }
      if (keys != 1) {
        return false;
      }
    }
  }
  return true;
}
static_assert(SubtabsMatchTree());

}  // namespace

std::span<const SettingDestination> SettingDestinations() { return kDestinations; }

}  // namespace moonclient
