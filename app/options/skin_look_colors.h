#pragma once

// LES COULEURS DE L'HABILLAGE, telles que look.yaml les range et qu'un code de
// thème les porte : leur ligne, leur clé, leur champ, et la version qui les a
// apportées.
//
// Catalogue : une entrée par couleur, que l'on cherche sans lire de bout en bout.

#include <array>
#include <cstdint>

#include <imgui.h>

#include "ro/ui/skin_look.h"

#include "options/skin_look_options.h"

namespace moonclient::skin_look {

// LA VERSION DE L'HABILLAGE QU'UN FICHIER PORTE, c'est-à-dire les couleurs qu'il
// connaît : la palette d'origine, puis les teintes et le thème, puis les textes,
// la sélection et les infobulles, puis les couleurs d'état, puis le papier
// sombre du thème Sombre.
inline constexpr int kOriginalVersion = 1;
inline constexpr int kTintsVersion = 2;
inline constexpr int kTextsVersion = 3;
inline constexpr int kStatesVersion = 4;
inline constexpr int kDarkPaperVersion = 5;
inline constexpr int kCurrentVersion = kDarkPaperVersion;

// CE QUE DEVIENT UNE COULEUR QU'UN FICHIER PLUS ANCIEN NE PORTE PAS.
enum class Absent : std::uint8_t {
  // Celle du thème le plus proche. Ce qu'elle peint est neuf aussi — un bouton
  // et son libellé, un lien —, rien de ce que le joueur a déjà choisi ne s'y
  // lit.
  kTakeTheme,
  // Son défaut, qui est l'aspect qu'avaient ces fichiers. ⚠ CELLE DES IMAGES DE
  // LA FENÊTRE : elle passe sous le texte du titre, que le joueur a choisi pour
  // l'image telle quelle ; l'assombrir pourrait le rendre illisible.
  kKeepDefault,
};

// UNE COULEUR DE LA PALETTE : sa ligne, sa clé, son champ, la version qui l'a
// apportée, et ce qu'elle devient dans un fichier plus ancien.
//
// ⚠ LA CLÉ ET LA VERSION SONT GRAVÉES : les changer rend à tous les joueurs
// l'habillage d'origine, et fausse les codes de thème déjà partagés.
struct ColorRow {
  SkinLookRow row;
  const char* key;
  ImU32 ro::ui::SkinLook::* member;
  int since = kOriginalVersion;
  Absent absent = Absent::kTakeTheme;
};

// ⚠ DANS L'ORDRE DE LA PAGE : c'est celui dans lequel les rangées se composent.
// Le fond des cartes garde la clé du papier de l'album, qui est la même couleur.
using ro::ui::SkinLook;
inline constexpr std::array<ColorRow, 31> kColorRows = {{
    {SkinLookRow::kBody, "body_color", &SkinLook::body},
    {SkinLookRow::kBorder, "border_color", &SkinLook::border},
    {SkinLookRow::kActiveTab, "active_tab_color", &SkinLook::active_tab},
    {SkinLookRow::kIdleTab, "idle_tab_color", &SkinLook::idle_tab},
    {SkinLookRow::kInput, "input_color", &SkinLook::input},
    {SkinLookRow::kTableHeader, "table_header_color", &SkinLook::table_header},
    {SkinLookRow::kPopup, "popup_color", &SkinLook::popup},
    {SkinLookRow::kSelection, "selection_color", &SkinLook::selection, kTextsVersion},
    {SkinLookRow::kArtTint, "art_tint_color", &SkinLook::art_tint, kTintsVersion,
     Absent::kKeepDefault},
    {SkinLookRow::kTitleText, "title_text_color", &SkinLook::title_text},
    {SkinLookRow::kText, "text_color", &SkinLook::text},
    {SkinLookRow::kFaintText, "faint_text_color", &SkinLook::faint_text, kTextsVersion},
    {SkinLookRow::kLink, "link_color", &SkinLook::link, kTextsVersion},
    {SkinLookRow::kGoodText, "good_text_color", &SkinLook::good_text, kStatesVersion},
    {SkinLookRow::kBadText, "bad_text_color", &SkinLook::bad_text, kStatesVersion},
    {SkinLookRow::kWarnText, "warn_text_color", &SkinLook::warn_text, kStatesVersion},
    {SkinLookRow::kInfoText, "info_text_color", &SkinLook::info_text, kStatesVersion},
    {SkinLookRow::kPaper, "tooltip_color", &SkinLook::paper, kTextsVersion},
    {SkinLookRow::kPaperText, "tooltip_text_color", &SkinLook::paper_text, kTextsVersion},
    {SkinLookRow::kButtonTint, "button_tint_color", &SkinLook::button_tint, kTintsVersion},
    {SkinLookRow::kButtonHoverTint, "button_hover_tint_color", &SkinLook::button_hover_tint,
     kTintsVersion},
    {SkinLookRow::kButtonText, "button_text_color", &SkinLook::button_text, kTintsVersion},
    {SkinLookRow::kCheckTint, "check_tint_color", &SkinLook::check_tint, kTintsVersion},
    {SkinLookRow::kSliderTint, "slider_tint_color", &SkinLook::slider_tint, kTintsVersion},
    {SkinLookRow::kItemTabTint, "item_tab_tint_color", &SkinLook::item_tab_tint, kTintsVersion},
    {SkinLookRow::kItemGridTint, "item_grid_tint_color", &SkinLook::item_grid_tint, kTintsVersion},
    {SkinLookRow::kSlot, "slot_color", &SkinLook::slot},
    {SkinLookRow::kDoll, "doll_color", &SkinLook::doll},
    {SkinLookRow::kCard, "album_paper_color", &SkinLook::card},
    {SkinLookRow::kCardBand, "card_band_color", &SkinLook::card_band},
    {SkinLookRow::kCardBandText, "card_band_text_color", &SkinLook::card_band_text},
}};

}  // namespace moonclient::skin_look
