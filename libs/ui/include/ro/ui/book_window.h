#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/description_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {

// ── LE LIVRE QU'ON LIT ──────────────────────────────────────────────────────
//
// La fenêtre que « Lire » ouvre depuis la description d'un livre qu'on
// possède. Comme au client : le texte se replie à la largeur de la fenêtre,
// puis se découpe en pages de dix-neuf lignes ; la couleur ouverte par un
// « ^RRGGBB » suit d'une ligne à l'autre, et d'une page à la suivante.
inline constexpr std::size_t kBookLinesPerPage = 19;

struct BookWindowState {
  bool open = false;
  bool raise = false;
  // Ouverte sous la souris, comme une description — et par le même réglage.
  bool place_at_mouse = false;
  DescriptionPlacement placement;
  bool was_open = false;
  std::uint32_t item_id = 0;
  // La page, à partir de UN.
  int page = 1;

  // LES LIGNES REPLIÉES, et ce pour quoi elles valent : le livre et la largeur.
  // ⚠ REFAITES SEULEMENT QUAND L'UN DES DEUX CHANGE : mesurer chaque mot à
  // chaque image coûterait pour un texte qui ne bouge pas.
  std::vector<std::string> rows;
  std::uint32_t rows_for = 0;
  float rows_width = 0.0F;

  WheelGateState wheel;
};

struct BookView {
  std::uint32_t item_id = 0;
  // Le nom de l'item, qui est le titre du livre.
  std::string_view title;
  std::span<const std::string> lines;
  // Le fond que le livre demande ; zéro, celui des descriptions.
  ImU32 background = 0;
};

// Ce que le joueur a fait : la page où il ferme, pour le signet.
struct BookWindowAction {
  bool closed = false;
  int page = 0;
};

[[nodiscard]] BookWindowAction DrawBookWindow(BookWindowState& state, const BookView& view,
                                              const SkinArt& art);

// Replie les lignes d'un livre à `width` pixels, avec la police courante :
// chaque ligne rendue porte en tête la couleur qui s'y applique. Publique pour
// les essais.
[[nodiscard]] std::vector<std::string> WrapBookLines(std::span<const std::string> lines,
                                                     float width);

// Une ligne de livre sans ses marques de couleur : ce qui se dit.
[[nodiscard]] std::string BookLinePlain(std::string_view line);

// LA LECTURE À VOIX HAUTE : la ligne dite, dans une bulle en haut de l'écran,
// comme le client. Rien quand la ligne est vide.
void DrawBookRecital(std::string_view line);

}  // namespace ro::ui
