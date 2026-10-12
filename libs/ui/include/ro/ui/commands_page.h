#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA PAGE « COMMANDS SETTINGS » DU PANNEAU D'OPTIONS : les réglages que le
// serveur tient pour le personnage — `@showexp`, `@noks`, `@autoloot` et les
// siens —, en cases à cocher plutôt qu'en commandes tapées.
//
// ⚠ ELLE NE POSSÈDE AUCUN RÉGLAGE. Elle montre ce qu'on lui passe et rend ce que
// le joueur a demandé ; c'est l'appelant qui l'envoie, et qui note d'avance ce
// que le serveur ne renverra pas.
//
// ⚠ DEUX ONGLETS, « Général » et « Autoloots », et c'est la page qui les dessine :
// les listes repliables, la liste de ramassage et ses presets ne sont pas des
// rangées de réglage.

// LES DEUX ONGLETS, dans l'ordre où la page les dessine. ⚠ LE RANG EST CELUI
// QU'UN LIEN DE RÉGLAGE DÉSIGNE : un onglet s'ajoute en fin.
enum class CommandsTab : std::uint8_t {
  kGeneral = 0,
  kAutoloot = 1,
};
inline constexpr int kCommandsTabCount = 2;

// Un item de la liste de ramassage.
struct CommandsAutolootEntry {
  std::uint32_t item_id = 0;
  // Son nom, ou nul quand la table des items ne le connaît pas : l'identifiant
  // seul s'affiche alors.
  const char* name = nullptr;
};

// Un preset de la liste, tel que le serveur l'a rangé.
struct CommandsPresetView {
  std::uint8_t slot = 0;
  // Vide pour un preset sans nom : il se montre alors par son numéro.
  const char* name = "";
  bool autoload = false;
};

// Le plafond d'un seuil de zeny, et son pas : celui du fil.
inline constexpr int kCommandsMinZenyHighest = 1000000;
inline constexpr int kCommandsMinZenyStep = 100;
// Le taux de ramassage le plus haut, en pour-cent.
inline constexpr int kCommandsAutolootRateHighest = 100;
// Les emplacements de preset, de un à ce nombre.
inline constexpr std::uint8_t kCommandsPresetSlots = 10;
// La liste de ramassage ne dépasse pas ce nombre d'items.
inline constexpr std::size_t kCommandsAutolootCapacity = 50;
// Le plus long nom de preset qu'on laisse taper, zéro compris.
inline constexpr std::size_t kCommandsPresetNameBytes = 64;

struct CommandsPageView {
  // ⚠ FAUX TANT QUE LE SERVEUR N'A RIEN DIT : la page se grise alors, plutôt que
  // d'offrir des cases dont un clic écraserait le vrai réglage par un défaut.
  bool told = false;

  bool show_exp = false;
  bool show_zeny = false;
  bool show_mob_info = false;
  bool separate_kills = false;
  bool block_exp = false;
  bool show_skill_delay = false;
  bool show_speed = false;
  bool sell_stuff = false;
  bool sell_item = false;
  bool no_ask = false;
  bool wings = false;
  bool fly_wing_marker = false;
  // De zéro à trois : aucune, soi, le groupe, la guilde.
  int kill_steal_guard = 0;

  int autoloot_rate_percent = 0;
  int autoloot_min_zeny = 0;
  std::uint16_t autoloot_type_mask = 0;
  bool autoloot_rares = false;
  bool autoloot_mvp = false;
  bool autoloot_mvp_rewards = false;

  std::span<const CommandsAutolootEntry> autoloot;
  // La liste diffère de celle du preset chargé.
  bool autoloot_unsaved = false;
  std::span<const CommandsPresetView> presets;
  std::uint8_t active_preset = 0;

  // LE DERNIER ITEM DONT ON A OUVERT LA DESCRIPTION, pour l'ajouter d'un clic —
  // zéro pour aucun.
  std::uint32_t last_viewed_item = 0;
  const char* last_viewed_name = nullptr;

  // Le Maj + clic d'un onglet pose son lien dans le chat. Faux là où il n'y a
  // pas de chat.
  bool offers_links = false;
};

// CE QUE LA PAGE RETIENT D'UNE IMAGE À L'AUTRE : ce qu'on tape, et le preset
// choisi dans la liste.
struct CommandsPageState {
  // LE SEUIL DE PRIX EN COURS DE FRAPPE, qui ne part qu'en quittant le champ.
  int min_zeny_draft = 0;
  bool min_zeny_editing = false;
  int autoloot_id_input = 0;
  std::array<char, kCommandsPresetNameBytes> preset_name{};
  std::array<char, kCommandsPresetNameBytes> rename{};
  bool rename_open = false;
  // Le preset choisi dans la liste déroulante, qui peut n'être pas le chargé.
  std::uint8_t selected_preset = 0;
  // L'ONGLET OUVERT, relevé à chaque image : c'est ImGui qui le tient.
  CommandsTab tab = CommandsTab::kGeneral;
  // L'ONGLET QU'ON DEMANDE D'OUVRIR — un lien qu'on suit —, consommé à l'image
  // qui l'applique. Voir `OptionsWindowState::section_tab_wanted`.
  std::optional<CommandsTab> tab_wanted;
};

// Le réglage qu'une case, une liste ou un curseur vient de changer.
enum class CommandsField : std::uint8_t {
  kNone,
  kShowExp,
  kShowZeny,
  kShowMobInfo,
  kSeparateKills,
  kBlockExp,
  kShowSkillDelay,
  kShowSpeed,
  kSellStuff,
  kSellItem,
  kNoAsk,
  kWings,
  kFlyWingMarker,
  kKillStealGuard,
  // En pour-cent.
  kAutolootRate,
  // En zeny entiers, multiple de `kCommandsMinZenyStep`.
  kAutolootMinZeny,
  kAutolootTypes,
  kAutolootRares,
  kAutolootMvp,
  kAutolootMvpRewards,
};

// Ce qu'on demande au serveur sur un preset.
enum class CommandsPresetGesture : std::uint8_t {
  kNone,
  kSave,
  kLoad,
  kDelete,
  // L'emplacement zéro éteint le chargement automatique.
  kAutoload,
  kRename,
};

// Ce que le joueur a demandé. ⚠ UN GESTE PAR IMAGE : il n'a qu'une souris.
struct CommandsPageAction {
  CommandsField field = CommandsField::kNone;
  // Un booléen vaut zéro ou un.
  int value = 0;

  // LA LISTE DE RAMASSAGE : un item ajouté, un item retiré, ou tout effacé.
  std::uint32_t autoloot_add = 0;
  std::uint32_t autoloot_remove = 0;
  bool autoloot_clear = false;

  CommandsPresetGesture preset = CommandsPresetGesture::kNone;
  std::uint8_t preset_slot = 0;
  std::string preset_name;

  // L'ONGLET dont on veut poser le lien dans le chat — Maj + clic sur sa
  // languette, qui le sélectionne au passage.
  std::optional<CommandsTab> link_tab;
};

[[nodiscard]] CommandsPageAction DrawCommandsPage(const CommandsPageView& view,
                                                  CommandsPageState& state, const SkinArt& art);

// CE QUE LA PAGE DONNE À LA RECHERCHE DU PANNEAU : ses libellés traduits et ses
// commandes, chacun avec son onglet. Voir `OptionSectionView::body_terms`.
// ⚠ À REFAIRE À CHAQUE IMAGE où l'on cherche : la langue a pu changer.
void AppendCommandsPageTerms(std::vector<OptionBodyTerm>& into);

// LE SEUIL DE ZENY TEL QUE LE SERVEUR LE RETIENT : borné, puis arrondi au pas du
// fil, vers le bas.
[[nodiscard]] int ClampAutolootMinZeny(int zeny);

// L'EMPLACEMENT OÙ SAUVEGARDER SOUS CE NOM : celui du preset qui le porte déjà,
// sinon le premier libre, sinon zéro — la liste est pleine.
[[nodiscard]] std::uint8_t PresetSlotForName(std::span<const CommandsPresetView> presets,
                                             const char* name);

}  // namespace ro::ui
