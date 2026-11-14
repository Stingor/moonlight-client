#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA FICHE D'UN OBJET, ÉDITÉE PAR LE STAFF ────────────────────────────────
//
// Le nom, la ressource, la teinte de l'icône et la description d'un objet,
// tels que `item_info.yml` les écrit. La fenêtre ne change RIEN : elle pose une
// intention, un « bon de travail », que `app` écrit dans `savedata\` et qu'un
// script porte ensuite dans la table du dépôt. Voir `app/items/item_info_delta.h`.
//
// La fenêtre ne possède rien du jeu : elle tient sa saisie ; `app` compose ce
// qu'elle montre et route ce qu'elle demande.

inline constexpr const char* kItemSheetEditorId = "item_sheet_editor";
inline constexpr std::size_t kItemSheetEditorTextBytes = 256;
inline constexpr std::size_t kItemSheetEditorDescriptionBytes = 16384;

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
};

// Ce que la saisie montre en s'ouvrant : le bon en attente s'il y en a un,
// sinon la table.
struct ItemSheetEditorFields {
  std::string_view name;
  std::string_view resource;
  int tint_rank = 0;
  std::span<const std::string> description;
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
