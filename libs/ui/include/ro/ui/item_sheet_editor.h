#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA FICHE D'UN OBJET, ÉDITÉE PAR LE STAFF ────────────────────────────────
//
// Le nom, la ressource, la teinte de l'icône et la description d'un objet,
// tels que `item_info.yml` les écrit. La fenêtre ne change RIEN : elle pose une
// intention, un « bon de travail », que `app` écrit dans `savedata\` et qu'un
// script porte ensuite dans la table du dépôt. Voir `app/items/item_info_delta.h`.
//
// L'onglet « Serveur » fait de même pour l'item_db du serveur : ses bons vont
// dans un second fichier, qu'un second script porte dans les imports du
// serveur. Voir `app/items/item_db_delta.h`.
//
// La fenêtre ne possède rien du jeu : elle tient sa saisie ; `app` compose ce
// qu'elle montre et route ce qu'elle demande.

inline constexpr const char* kItemSheetEditorId = "item_sheet_editor";
inline constexpr std::size_t kItemSheetEditorTextBytes = 256;
inline constexpr std::size_t kItemSheetEditorDescriptionBytes = 16384;
inline constexpr std::size_t kItemSheetEditorScriptBytes = 8192;

// ── LES CHAMPS DU SERVEUR ───────────────────────────────────────────────────
//
// Ce que l'onglet « Serveur » édite de l'item_db : les nombres, `Refineable`
// et les trois scripts. Les tables (`Jobs`, `Locations`, `Flags`…) n'y sont pas.
//
// ⚠ LES CLÉS SONT CELLES DE L'ITEM_DB, ET UN CONTRAT : le bon les écrit telles
// quelles, et `scripts/yaml_tables/apply_item_db_delta.py` les relit DANS CE
// FICHIER pour savoir ce qu'un bon a le droit de demander.

// Les bornes du serveur en pré-renouveau (`MAX_SLOTS`, `MAX_WEAPON_LEVEL`,
// `MAX_ARMOR_LEVEL` de `mmo.hpp`) ; les autres nombres ne sont bornés que par
// leur type.
inline constexpr int kItemSheetMostSlots = 4;
inline constexpr int kItemSheetMostWeaponLevel = 4;
inline constexpr int kItemSheetMostArmorLevel = 1;
inline constexpr int kItemSheetMostNumber = 2147483647;

struct ItemSheetServerNumber {
  std::string_view key;
  const char* label = "";
  int most = kItemSheetMostNumber;
};
inline constexpr std::array<ItemSheetServerNumber, 12> kItemSheetServerNumbers = {{
    {"Buy", core::TrText("Achat (zeny)")},
    {"Sell", core::TrText("Revente (zeny)")},
    {"Weight", core::TrText("Poids (dixièmes)")},
    {"Attack", core::TrText("ATK")},
    {"MagicAttack", core::TrText("MATK")},
    {"Defense", core::TrText("DEF")},
    {"Range", core::TrText("Portée (cases)")},
    {"Slots", core::TrText("Emplacements de card"), kItemSheetMostSlots},
    {"WeaponLevel", core::TrText("Niveau d'arme"), kItemSheetMostWeaponLevel},
    {"ArmorLevel", core::TrText("Niveau d'armure"), kItemSheetMostArmorLevel},
    {"EquipLevelMin", core::TrText("Niveau requis")},
    {"EquipLevelMax", core::TrText("Niveau maximal")},
}};
inline constexpr std::string_view kItemSheetRefineableKey = "Refineable";

struct ItemSheetServerScript {
  std::string_view key;
  const char* label = "";
};
inline constexpr std::array<ItemSheetServerScript, 3> kItemSheetServerScripts = {{
    {"Script", core::TrText("Script")},
    {"EquipScript", core::TrText("Script au port")},
    {"UnEquipScript", core::TrText("Script au retrait")},
}};

// Le libellé TRADUIT d'un nombre ou d'un script de l'onglet : les catalogues
// sont constants, et les traduire à leur définition les figerait en français.
[[nodiscard]] const char* ItemSheetServerNumberLabel(const ItemSheetServerNumber& number);
[[nodiscard]] const char* ItemSheetServerScriptLabel(const ItemSheetServerScript& script);

enum class ItemSheetEditorTab : std::uint8_t { kClient, kServer };

struct ItemSheetEditorState {
  bool open = false;
  // Mise au premier plan à la prochaine image.
  bool raise = false;
  std::uint32_t item_id = 0;
  // LA SAISIE EST REMPLIE depuis la vue tant que ce drapeau est baissé : à
  // l'ouverture, à « Rétablir », et après un bon posé ou retiré.
  bool filled = false;
  std::array<char, kItemSheetEditorTextBytes> name{};
  std::array<char, kItemSheetEditorTextBytes> resource{};
  // Le rang de la teinte dans `ItemSheetEditorView::tint_labels`.
  int tint_rank = 0;
  // Une ligne de la table par ligne de texte.
  std::array<char, kItemSheetEditorDescriptionBytes> description{};

  ItemSheetEditorTab tab = ItemSheetEditorTab::kClient;
  // L'ONGLET « SERVEUR » : un nombre par entrée de `kItemSheetServerNumbers`,
  // dans son ordre, et un texte par script de `kItemSheetServerScripts`. Il se
  // remplit À PART : la base du serveur se lit après l'ouverture.
  bool server_filled = false;
  std::array<int, kItemSheetServerNumbers.size()> numbers{};
  bool refineable = false;
  std::array<std::array<char, kItemSheetEditorScriptBytes>, kItemSheetServerScripts.size()>
      scripts{};
};

// Ce que la saisie montre en s'ouvrant : le bon en attente s'il y en a un,
// sinon la table.
struct ItemSheetEditorFields {
  std::string_view name;
  std::string_view resource;
  int tint_rank = 0;
  std::span<const std::string> description;
};

// De même pour l'onglet « Serveur » : le bon en attente par-dessus la base
// exportée du serveur.
struct ItemSheetServerFields {
  std::array<int, kItemSheetServerNumbers.size()> numbers{};
  bool refineable = false;
  std::array<std::string_view, kItemSheetServerScripts.size()> scripts{};
};

struct ItemSheetEditorView {
  std::uint32_t item_id = 0;
  // L'objet est dans la table ; sinon le bon créera sa fiche.
  bool known = false;
  ItemSheetEditorFields start;
  // Les libellés des teintes, en français et marqués `core::TrText` : la liste
  // les traduit à la lecture (`SkinCombo`). « Aucune » au rang zéro.
  std::span<const char* const> tint_labels;
  // L'icône de la table, et celle que donneraient la ressource et la teinte
  // saisies.
  SkinImage table_icon;
  SkinImage preview_icon;
  // Un bon attend le script pour CET objet ; et combien en tout.
  bool pending = false;
  std::size_t pending_count = 0;

  // L'ONGLET « SERVEUR » : l'item est dans la base exportée du serveur ; la
  // base se lit encore ; ce que la saisie montre ; et ses bons, de même.
  bool server_known = false;
  bool server_loading = false;
  ItemSheetServerFields server_start;
  bool server_pending = false;
  std::size_t server_pending_count = 0;
  // Ce que le dernier geste a donné, et s'il a réussi ; vide : rien à dire.
  std::string_view notice;
  bool notice_ok = true;
};

struct ItemSheetEditorAnswer {
  // « Enregistrer le bon » : `app` relit la saisie dans l'état.
  bool save = false;
  // « Annuler le bon » de cet objet.
  bool clear_pending = false;
};

// Ouvre la fenêtre au premier plan sur cet objet, sa saisie remplie de nouveau.
void OpenItemSheetEditor(ItemSheetEditorState& state, std::uint32_t item_id);

// LES LIGNES D'UNE DESCRIPTION SAISIE : une par ligne de texte, le retour
// chariot d'une fin de ligne Windows retiré, et la ligne vide qu'un saut de
// ligne final laisse, ôtée. Un texte vide n'a aucune ligne.
[[nodiscard]] std::vector<std::string> ItemSheetEditorLines(std::string_view text);

// Le texte qui montre ces lignes : jointes par des sauts de ligne.
[[nodiscard]] std::string ItemSheetEditorText(std::span<const std::string> lines);

[[nodiscard]] ItemSheetEditorAnswer DrawItemSheetEditor(ItemSheetEditorState& state,
                                                        const ItemSheetEditorView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
