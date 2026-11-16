#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/equip_window.h"
#include "ro/ui/skin_art.h"

#include "achievement_paint.h"

// LES MESURES ET LES GESTES DE LA FENÊTRE D'ÉQUIPEMENT DU JEU.
//
// En-tête PRIVÉ de `equip_window.cpp`, qui peint, et de
// `equip_window_gestures.cpp`, qui répond à la souris : les deux lisent les
// mêmes nombres du jeu, et un nombre écrit deux fois finit par différer.
namespace ro::ui::equip_parts {

using achievement::Painter;

// ── LA DISPOSITION DU JEU ───────────────────────────────────────────────────
//
// Le contenu se mesure depuis un haut placé vingt pixels sous le bord : la
// barre de titre, puis les onglets.
inline constexpr float kContentTop = 20.0F;
// Le fond peint commence sous les onglets ; le cadre du costume, trois pixels
// plus haut, sur leur filet.
inline constexpr float kBackgroundTop = kContentTop + 17.0F;
inline constexpr float kCostumeFrameTop = kContentTop + 14.0F;

// Les onglets : leur bande, la marge qu'ils ajoutent à leur libellé, et le
// filet qui prolonge leur bas jusqu'au bord droit.
inline constexpr float kTabTop = 17.0F;
inline constexpr float kTabHeight = 17.0F;
inline constexpr float kTabPadding = 15.0F;
inline constexpr float kTabLineTop = 34.0F;
// Le libellé d'un onglet fermé descend d'un pixel.
inline constexpr float kTabClosedDrop = 1.0F;

// LES RANGÉES. La première porte les places 8 et 9 ; les quatre suivantes,
// vingt-six pixels plus bas chacune, les places 0 à 3 à gauche et 4 à 7 à
// droite.
inline constexpr float kRowPitch = 26.0F;
inline constexpr float kFirstIconTop = kContentTop + 18.0F;
inline constexpr float kBodyIconTop = kContentTop + 44.0F;
inline constexpr std::size_t kRowsPerColumn = 4;
inline constexpr std::size_t kFirstLeftPlace = 8;
inline constexpr std::size_t kFirstRightPlace = 9;
// Le nom, le liseré de survol et l'emblème du grade se posent sous le haut de
// l'icône.
inline constexpr float kNameBelowIcon = 7.0F;
inline constexpr float kGradeBelowIcon = 12.0F;
inline constexpr float kIconPixels = 24.0F;

// La colonne de gauche, dans les deux largeurs.
inline constexpr float kLeftIcon = 6.0F;
inline constexpr float kLeftHighlight = 4.0F;
inline constexpr float kLeftName = 33.0F;
// La colonne de droite du jeu, que l'élargissement repousse.
inline constexpr float kGameRightIcon = 251.0F;
inline constexpr float kGameRightHighlight = 248.0F;
inline constexpr float kGameRightName = 172.0F;
inline constexpr float kGameNameRoom = 70.0F;
// Élargi, le nom de droite s'écarte du personnage, et tout nom gagne de la
// place avant de passer à la ligne.
inline constexpr float kWideRightNameShift = 33.0F;
inline constexpr float kWideNameRoom = 127.0F;

// LES ZONES DE CLIC : la bande des rangées, et la rangée entière à gauche
// jusqu'au milieu, à droite depuis lui.
inline constexpr float kZoneTop = kContentTop + 17.0F;
inline constexpr float kZoneBottom = kContentTop + 145.0F;
inline constexpr float kLeftEntryEnd = 106.0F;
inline constexpr float kRightEntryStart = 170.0F;
inline constexpr float kEntryEdge = 6.0F;

// LA MUNITION, sous le personnage, depuis le milieu de la fenêtre.
inline constexpr float kAmmoFromCenter = 25.0F;
inline constexpr float kAmmoZoneInset = 1.0F;
inline constexpr float kAmmoIconTop = kContentTop + 44.0F;
inline constexpr float kAmmoNameTop = kContentTop + 68.0F;
inline constexpr float kAmmoNameRoom = 40.0F;

// LES TEXTES. La police du jeu n'est pas celle de ce client : sa taille
// nominale de douze donne ce corps-ci à la même largeur, mesuré sur Basic Info,
// et un texte calé à gauche recule de l'approche de nos glyphes.
inline constexpr float kTextBody = 11.4F;
inline constexpr float kGlyphBearing = 2.0F;
inline constexpr float kLineHeight = 12.0F;
// Un nom ne prend jamais plus de deux lignes, et remonte de la moitié d'une
// ligne pour chacune qu'il ajoute.
inline constexpr std::size_t kNameLinesMost = 2;
inline constexpr float kNameLineLift = kLineHeight / 2.0F;
inline constexpr std::string_view kEllipsis = "...";
// L'ombre d'un nom, en gris clair.
inline constexpr ImU32 kNameShadow = IM_COL32(0xE0, 0xE0, 0xE0, 0xFF);
// Le gris des onglets et de leur filet.
inline constexpr ImU32 kTabBorder = IM_COL32(0xC3, 0xC3, 0xC3, 0xFF);

// LE BAS : la case à cocher et « tout retirer ».
inline constexpr float kCheckLeft = 10.0F;
inline constexpr float kCheckFromBottom = 15.0F;
inline constexpr float kCheckHeight = 12.0F;
inline constexpr float kCheckTextGap = 3.0F;
inline constexpr float kTakeOffAllLeft = 235.0F;
inline constexpr float kTakeOffAllTop = 167.0F;

// LES DEUX BOUTONS POSÉS SUR LE PERSONNAGE, depuis le milieu : la monture, et
// au-dessus le contenu du chariot.
inline constexpr float kRiderFromCenter = 18.0F;
inline constexpr float kRiderTop = 127.0F;
inline constexpr float kCartFromCenter = 14.0F;
inline constexpr float kCartTop = 107.0F;

// LA BULLE D'UNE PLACE, posée sur sa rangée : à gauche, au milieu ou à droite
// selon où est la souris. ⚠ LE PAS DES RANGÉES Y VAUT VINGT-QUATRE, et non
// vingt-six : c'est le calcul du jeu, et la bulle glisse vers le bas.
inline constexpr float kTipLeft = 6.0F;
inline constexpr float kTipFromCenter = 20.0F;
inline constexpr float kTipRight = 175.0F;
inline constexpr float kTipRowPitch = 24.0F;
inline constexpr float kTipLift = 3.0F;

// La couleur d'un nom, selon les cartes posées au-delà des emplacements.
inline constexpr std::array<ImU32, 5> kNameColors = {
    IM_COL32(0x00, 0x00, 0x00, 0xFF), IM_COL32(0x66, 0x99, 0x00, 0xFF),
    IM_COL32(0x00, 0xBF, 0xFF, 0xFF), IM_COL32(0xFF, 0xA5, 0x00, 0xFF),
    IM_COL32(0x94, 0x00, 0xD3, 0xFF)};

[[nodiscard]] inline bool IsLeftColumn(std::size_t place) {
  return place < kRowsPerColumn || place == kFirstLeftPlace;
}

// LE HAUT DE L'ICÔNE D'UNE PLACE.
[[nodiscard]] inline float IconTop(std::size_t place) {
  if (place >= kFirstLeftPlace) {
    return kFirstIconTop;
  }
  return kBodyIconTop + kRowPitch * static_cast<float>(place % kRowsPerColumn);
}

[[nodiscard]] inline float IconLeft(const EquipWindowLayout& layout, std::size_t place) {
  return IsLeftColumn(place) ? kLeftIcon : layout.right_icon;
}

[[nodiscard]] inline float CenterOf(const EquipWindowLayout& layout) {
  return std::floor(layout.width / 2.0F);
}

[[nodiscard]] inline const EquipWindowPiece& PieceAt(const EquipWindowView& view,
                                                     EquipWindowTab tab, std::size_t place) {
  if (place == kEquipWindowAmmoPlace) {
    return view.ammo;
  }
  return tab == EquipWindowTab::kCostume ? view.costume[place] : view.general[place];
}

// ── LES GESTES ET LES BOUTONS, écrits dans `equip_window_gestures.cpp` ──────

// Un bouton peint : son image au repos, une autre survolée ou enfoncée. Rend
// ce qu'ImGui en a vu ; il agit au relâché sur lui.
[[nodiscard]] achievement::HitResult PaintedButton(const Painter& paint, const char* id,
                                                   const SkinImage& out, const SkinImage& over,
                                                   float left, float top);
void DrawIconGestures(const Painter& paint, const EquipWindowLayout& layout,
                      const EquipWindowView& view, EquipWindowTab tab, EquipWindowAction& action);
void DrawEntryGestures(const Painter& paint, const EquipWindowLayout& layout,
                       const EquipWindowView& view, EquipWindowTab tab, EquipWindowAction& action);
void DrawDropTarget(const Painter& paint, const EquipWindowLayout& layout,
                    EquipWindowAction& action);
void DrawSettingBox(const Painter& paint, const SkinArt& art, const char* label, bool ticked,
                    std::optional<bool>& asked);
void DrawTakeOffAll(const Painter& paint, const SkinArt& art, const EquipWindowView& view,
                    EquipWindowTab tab, EquipWindowAction& action);
void DrawRiderButton(const Painter& paint, const EquipWindowLayout& layout, const SkinArt& art,
                     std::span<const EquipWindowRider> riders, EquipWindowAction& action);
void DrawCartButton(const Painter& paint, const EquipWindowLayout& layout, const SkinArt& art,
                    const EquipWindowView& view, EquipWindowAction& action);

}  // namespace ro::ui::equip_parts
