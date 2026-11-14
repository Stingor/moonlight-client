#include "options/setting_links.h"

#include <array>
#include <cstddef>
#include <set>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/ui/achievement_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/commands_page.h"

#include "options/game_option_ids.h"
#include "options/pet_options.h"

// LES DESTINATIONS D'UN LIEN DE RÉGLAGE.
//
// 🔴 UNE CLÉ INCONNUE NE FORME AUCUN LIEN, et c'est la règle qui tient tout le
// reste : l'affichage et le clic interrogent la MÊME table, si bien qu'un lien
// visible mène toujours quelque part.

using moonclient::FindSettingDestination;
using moonclient::OptionSection;
using moonclient::SettingDestination;
using moonclient::SettingDestinationKind;
using moonclient::SettingDestinationLabel;
using moonclient::SettingDestinations;
using moonclient::SettingReach;

namespace {

// LES CLÉS PUBLIÉES, et la page où chacune tombe chez nous. ⚠ CES CLÉS SONT
// DÉJÀ ÉCRITES DANS DES SCRIPTS ET DANS DES LIGNES DE CHAT RELAYÉES : en perdre
// une ne casse rien de visible — le lien cesse simplement de se former.
constexpr std::array<std::pair<std::string_view, OptionSection>, 36> kPublishedKeys = {{
    {"graphics", OptionSection::kGraphics},
    {"gs_graphics", OptionSection::kGraphics},
    {"display", OptionSection::kDisplay},
    {"sound", OptionSection::kSound},
    {"gameplay", OptionSection::kGameplay},
    {"damage", OptionSection::kDamage},
    {"zone_recorder", OptionSection::kCapture},
    {"capture", OptionSection::kCapture},
    {"skill_range", OptionSection::kGameplay},
    {"skill_area", OptionSection::kGameplay},
    {"interface", OptionSection::kInterface},
    {"npc", OptionSection::kInterface},
    {"skin", OptionSection::kSkin},
    {"desc", OptionSection::kDescriptions},
    {"skill_bar", OptionSection::kShortcutBars},
    {"stat_bars", OptionSection::kStatBars},
    {"minimap", OptionSection::kMinimap},
    {"status_icons", OptionSection::kStatuses},
    {"dps", OptionSection::kDamageMeter},
    {"target_frame", OptionSection::kTarget},
    {"monster_info", OptionSection::kMonsterSheet},
    {"mvp_tracker", OptionSection::kMvpTracker},
    {"context_menu", OptionSection::kContextMenu},
    {"chat", OptionSection::kChat},
    {"bubbles", OptionSection::kBubbles},
    {"inventory", OptionSection::kInventory},
    {"cart", OptionSection::kCart},
    {"storage", OptionSection::kStorage},
    {"craft_atlas", OptionSection::kCraftAtlas},
    {"make_item", OptionSection::kMakeItem},
    {"refine", OptionSection::kWeaponRefine},
    {"card_album", OptionSection::kCardAlbum},
    // La fiche du personnage et celle du pet : un bloc d'« Interface » chacune.
    {"character", OptionSection::kInterface},
    {"skills", OptionSection::kInterface},
    {"pet", OptionSection::kInterface},
    {"commands", OptionSection::kCommands},
}};

// L'onglet Aventure des succès, son sixième sous-onglet, et l'onglet Exploit,
// qui n'en a aucun.
constexpr int kAdventureTab = ro::ui::AchievementTabOfMajor(2);
constexpr int kDungeonSubtab = 5;
constexpr int kFeatTab = ro::ui::AchievementTabOfMajor(6);

}  // namespace

TEST_CASE("Réglage : chaque clé publiée mène encore à sa page", "[app][lien]") {
  for (const auto& [key, section] : kPublishedKeys) {
    const SettingDestination* const found = FindSettingDestination(key);
    REQUIRE(found != nullptr);
    CHECK(found->kind == SettingDestinationKind::kOptions);
    CHECK(found->section == section);
    // ⚠ UNE PAGE ENTIÈRE : une clé publiée ne connaît pas nos onglets.
    CHECK(found->tab == moonclient::kNoSettingTab);
  }
}

TEST_CASE("Réglage : une fiche sans page mène à son bloc d'Interface", "[app][lien]") {
  const SettingDestination* const pet = FindSettingDestination("pet");
  REQUIRE(pet != nullptr);
  CHECK(pet->row == moonclient::PetRowId(moonclient::PetRow::kConfirmReturnToEgg));
  const SettingDestination* const character = FindSettingDestination("character");
  REQUIRE(character != nullptr);
  CHECK(character->row == static_cast<int>(moonclient::GameOption::kCharacterHoverDescription));
  // ⚠ LE MAJ + CLIC SUR « INTERFACE » POSE TOUJOURS « interface » : les clés
  // des blocs ne sont là que pour être lues.
  const SettingDestination* const page = moonclient::SettingDestinationOf(
      SettingDestinationKind::kOptions, OptionSection::kInterface, moonclient::kNoSettingTab);
  REQUIRE(page != nullptr);
  CHECK(page->key == "interface");
  CHECK(page->row == moonclient::kNoSettingRow);
}

TEST_CASE("Réglage : une clé inconnue ne forme pas de lien", "[app][lien]") {
  CHECK(FindSettingDestination("pas_un_reglage") == nullptr);
  CHECK(FindSettingDestination("") == nullptr);
  // Et c'est ce que l'affichage lit : une chaîne vide, donc pas de lien.
  CHECK(SettingDestinationLabel("pas_un_reglage", SettingReach{}).empty());
  CHECK_FALSE(SettingDestinationLabel("chat", SettingReach{}).empty());
}

TEST_CASE("Réglage : les clés sont uniques et voyagent", "[app][lien]") {
  // ⚠ UNE CLÉ EN DOUBLE NE SE VERRAIT PAS : la recherche rendrait la première,
  // et la seconde dormirait. Un `:` ou un chevron couperait la balise chez qui
  // la relit : un lecteur coupe au premier `:`.
  std::set<std::string_view> seen;
  for (const SettingDestination& destination : SettingDestinations()) {
    INFO(destination.key);
    CHECK(seen.insert(destination.key).second);
    CHECK(moonclient::SettingKeyTravels(destination.key));
    CHECK(destination.key.find(':') == std::string_view::npos);
    CHECK(destination.key.find('<') == std::string_view::npos);
    CHECK(destination.key.find('>') == std::string_view::npos);
    CHECK(destination.label != nullptr);
    // Une page désigne une section qui existe ; un onglet de fenêtre n'en a pas.
    CHECK((destination.kind == SettingDestinationKind::kOptions) ==
          (destination.section != OptionSection::kCount));
  }
}

TEST_CASE("Réglage : chaque page du panneau a sa clé", "[app][lien]") {
  // ⚠ SANS EXCEPTION : une page sans clé ne pourrait ni se lier d'un Maj + clic,
  // ni s'ouvrir d'un lien reçu.
  for (std::size_t rank = 0; rank < moonclient::Rank(OptionSection::kCount); ++rank) {
    const auto section = static_cast<OptionSection>(rank);
    INFO(rank);
    CHECK(moonclient::SettingDestinationOf(SettingDestinationKind::kOptions, section,
                                           moonclient::kNoSettingTab) != nullptr);
  }
}

TEST_CASE("Réglage : un Maj + clic pose la première clé de sa destination", "[app][lien]") {
  using moonclient::SettingDestinationOf;
  // Notre clé pour la capture, et non `zone_recorder`, qui ne fait que se
  // lire.
  CHECK(SettingDestinationOf(SettingDestinationKind::kOptions, OptionSection::kCapture,
                             moonclient::kNoSettingTab)
            ->key == "capture");
  CHECK(SettingDestinationOf(SettingDestinationKind::kOptions, OptionSection::kGraphics,
                             moonclient::kNoSettingTab)
            ->key == "graphics");
  CHECK(SettingDestinationOf(SettingDestinationKind::kOptions, OptionSection::kDamage, 2)->key ==
        "damage_colors");
  CHECK(SettingDestinationOf(SettingDestinationKind::kSheet, OptionSection::kCount,
                             static_cast<int>(ro::ui::CharacterTab::kTitles))
            ->key == "sheet_titles");
  // ⚠ LE GENRE DÉPARTAGE : l'onglet 0 des succès n'est pas celui de la fiche.
  CHECK(
      SettingDestinationOf(SettingDestinationKind::kAchievements, OptionSection::kCount, 0)->key ==
      "achievements_summary");
}

TEST_CASE("Réglage : l'onglet d'un compagnon absent ne s'offre pas", "[app][lien]") {
  const SettingDestination* const homunculus = FindSettingDestination("sheet_homunculus");
  REQUIRE(homunculus != nullptr);
  CHECK_FALSE(moonclient::SettingDestinationReachable(*homunculus, SettingReach{}));
  CHECK(moonclient::SettingDestinationReachable(*homunculus, SettingReach{.homunculus = true}));
  CHECK(SettingDestinationLabel("sheet_homunculus", SettingReach{}).empty());
  // Un onglet ordinaire, lui, est toujours là.
  CHECK(moonclient::SettingDestinationReachable(*FindSettingDestination("sheet_titles"),
                                                SettingReach{}));
  // Les succès demandent leur catalogue.
  CHECK_FALSE(moonclient::SettingDestinationReachable(*FindSettingDestination("achievements_all"),
                                                      SettingReach{}));
}

TEST_CASE("Réglage : le chemin nomme la page ou la fenêtre", "[app][lien]") {
  // ⚠ SANS CATALOGUE CHARGÉ, LA TRADUCTION REND LE FRANÇAIS : c'est ce qu'on lit.
  ro::core::UseFrench();
  CHECK(moonclient::SettingDestinationFallback(*FindSettingDestination("interface")) ==
        "Interface");
  CHECK(moonclient::SettingDestinationFallback(*FindSettingDestination("damage_colors")) ==
        "Dégâts › Couleurs");
  CHECK(moonclient::SettingDestinationFallback(*FindSettingDestination("sheet_titles")) ==
        "Personnage › Titres");
  CHECK(moonclient::SettingDestinationFallback(*FindSettingDestination("skill_bar_3")) ==
        "Barres de raccourcis › Barre 3");
  CHECK(moonclient::SettingLinkLabel(*FindSettingDestination("damage_colors")) ==
        "[Réglage: Dégâts › Couleurs]");
  CHECK(moonclient::SettingLinkLabel(*FindSettingDestination("sheet_titles")) ==
        "[Fenêtre: Personnage › Titres]");
  CHECK(moonclient::SettingDestinationTip(*FindSettingDestination("interface")) ==
        "Réglages › UI › Interface");
  CHECK(moonclient::SettingDestinationTip(*FindSettingDestination("damage")) ==
        "Réglages › Core › Dégâts");
}

TEST_CASE("Réglage : chaque sous-onglet des succès a sa clé", "[app][lien]") {
  using moonclient::SettingDestinationOf;
  for (int tab = 0; tab < ro::ui::kAchievementTabCount; ++tab) {
    for (int subtab = 0; subtab < ro::ui::kAchievementSubtabCounts[static_cast<std::size_t>(tab)];
         ++subtab) {
      INFO(tab << " › " << subtab);
      const SettingDestination* const found = SettingDestinationOf(
          SettingDestinationKind::kAchievements, OptionSection::kCount, tab, subtab);
      REQUIRE(found != nullptr);
      // ⚠ CE QU'UN MAJ + CLIC POSE SE RELIT EN LA MÊME PLACE.
      CHECK(FindSettingDestination(found->key) == found);
    }
  }
  // Un sous-onglet que l'arbre n'a pas n'a pas de clé : le Maj + clic retombe
  // sur l'onglet.
  CHECK(SettingDestinationOf(SettingDestinationKind::kAchievements, OptionSection::kCount, kFeatTab,
                             ro::ui::kAchievementFeatMinigames + 1) == nullptr);
}

TEST_CASE("Réglage : un sous-onglet des succès se nomme sous son onglet", "[app][lien]") {
  ro::core::UseFrench();
  const SettingDestination* const dungeon = moonclient::SettingDestinationOf(
      SettingDestinationKind::kAchievements, OptionSection::kCount, kAdventureTab, kDungeonSubtab);
  REQUIRE(dungeon != nullptr);
  CHECK(dungeon->key == "achievements_adventure_dungeon");
  CHECK(moonclient::SettingDestinationFallback(*dungeon) == "Succès › Aventure › Donjon");
  CHECK(moonclient::SettingLinkLabel(*dungeon) == "[Fenêtre: Succès › Aventure › Donjon]");
  CHECK(moonclient::SettingDestinationTip(*dungeon) == "Succès › Aventure › Donjon");
  // L'onglet entier garde son chemin court.
  CHECK(moonclient::SettingDestinationFallback(*FindSettingDestination("achievements_adventure")) ==
        "Succès › Aventure");
}

TEST_CASE("Réglage : un sous-onglet des succès sans catalogue reste inerte", "[app][lien]") {
  CHECK(SettingDestinationLabel("achievements_battle_pvp", SettingReach{}).empty());
  CHECK_FALSE(SettingDestinationLabel("achievements_battle_pvp", SettingReach{.achievements = true})
                  .empty());
  // Une sous-catégorie que l'arbre n'a pas ne forme aucun lien.
  CHECK(FindSettingDestination("achievements_feat_other") == nullptr);
  CHECK(SettingDestinationLabel("achievements_feat_other", SettingReach{.achievements = true})
            .empty());
}

TEST_CASE("Réglage : les onglets que Commands Settings dessine ont leur clé", "[app][lien]") {
  using moonclient::SettingDestinationOf;
  ro::core::UseFrench();
  const SettingDestination* const general =
      SettingDestinationOf(SettingDestinationKind::kOptions, OptionSection::kCommands,
                           static_cast<int>(ro::ui::CommandsTab::kGeneral));
  const SettingDestination* const autoloot =
      SettingDestinationOf(SettingDestinationKind::kOptions, OptionSection::kCommands,
                           static_cast<int>(ro::ui::CommandsTab::kAutoloot));
  REQUIRE(general != nullptr);
  REQUIRE(autoloot != nullptr);
  CHECK(general->key == "commands_general");
  CHECK(autoloot->key == "commands_autoloot");
  CHECK(moonclient::SettingDestinationFallback(*autoloot) == "Commands Settings › Autoloots");
  CHECK(moonclient::SettingDestinationTip(*autoloot) ==
        "Réglages › Server › Commands Settings › Autoloots");
  // La page entière garde sa clé publiée.
  CHECK(SettingDestinationOf(SettingDestinationKind::kOptions, OptionSection::kCommands,
                             moonclient::kNoSettingTab)
            ->key == "commands");
  // Un onglet que la page n'a pas n'a pas de clé : le Maj + clic retombe sur
  // la page.
  CHECK(SettingDestinationOf(SettingDestinationKind::kOptions, OptionSection::kCommands,
                             ro::ui::kCommandsTabCount) == nullptr);
}
