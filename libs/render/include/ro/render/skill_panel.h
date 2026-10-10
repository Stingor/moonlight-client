#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/font_atlas.h"
#include "ro/render/screen_overlay.h"

namespace ro::render {

// LE GRIMOIRE : la liste des compétences, en pixels de fenêtre.
//
// Ce panneau ne sait pas ce qu'il montre. `app` lui donne des libellés déjà
// écrits et lui demande quelle ligne un point désigne ; c'est ce qui laisse
// vérifier sa géométrie sans monde ni carte graphique, et ce qui garde `render`
// hors de `sim`.
//
// Il n'est pas dessiné à l'image du jeu, dont la fenêtre est faite de morceaux
// de sprites : ce sont des aplats, comme les jauges de vie. Une fenêtre fidèle
// viendra avec la couche d'interface ; celle-ci sert à LANCER, pas à décorer.

// La largeur du panneau et la hauteur d'une de ses lignes, en pixels.
inline constexpr float kSkillPanelWidthPixels = 210.0F;
inline constexpr float kSkillPanelRowPixels = 16.0F;

// La marge du panneau au bord de la fenêtre, et la marge intérieure d'une
// ligne.
inline constexpr float kSkillPanelMarginPixels = 8.0F;
inline constexpr float kSkillPanelPaddingPixels = 6.0F;

// La hauteur de son bandeau de titre.
inline constexpr float kSkillPanelTitlePixels = 18.0F;

// Le fond, le bandeau, la ligne ARMÉE — celle qui attend qu'on désigne une
// cible — et le cadre.
inline constexpr std::array<std::uint8_t, 4> kSkillPanelBackColor = {16, 16, 24, 216};
inline constexpr std::array<std::uint8_t, 4> kSkillPanelTitleColor = {44, 40, 72, 232};
inline constexpr std::array<std::uint8_t, 4> kSkillPanelArmedColor = {120, 90, 30, 232};
inline constexpr std::array<std::uint8_t, 4> kSkillPanelBorderColor = {0, 0, 0, 220};

// La couleur d'un libellé, et celle d'une compétence PASSIVE, qui ne se lance
// pas.
inline constexpr std::array<std::uint8_t, 4> kSkillPanelTextColor = {235, 235, 235, 255};
inline constexpr std::array<std::uint8_t, 4> kSkillPanelPassiveColor = {130, 130, 130, 255};

// Une ligne du panneau.
struct SkillPanelRow {
  // Le nom, à gauche, et ce qui l'accompagne — un rang, un coût — à droite.
  std::string_view label;
  std::string_view detail;
  // Vrai pour la ligne qui attend qu'on désigne sa cible.
  bool armed = false;
  // Vrai pour ce qui ne se lance pas : la ligne s'écrit alors en gris.
  bool passive = false;
};

// Où le panneau se pose, et combien de lignes il peut montrer.
//
// Il s'accroche en HAUT À DROITE et grandit vers le bas, jusqu'à ce que la
// fenêtre s'y oppose : une liste plus longue défile plutôt que de déborder.
struct SkillPanelLayout {
  // Le coin haut-gauche du panneau, et sa taille.
  glm::vec2 origin{0.0F, 0.0F};
  glm::vec2 size{0.0F, 0.0F};
  // Combien de lignes tiennent, au plus.
  std::size_t rows_shown = 0;
};

// La place du panneau dans une fenêtre de cette taille, pour cette liste.
[[nodiscard]] SkillPanelLayout SkillPanelPlacement(float viewport_width, float viewport_height,
                                                   std::size_t row_count);

// Vrai si ce point tombe DANS le panneau, bandeau de titre compris.
//
// C'est ce qui empêche un clic sur le grimoire d'atteindre le monde derrière
// lui : sans quoi, ouvrir le panneau ferait marcher le personnage sous les
// lignes qu'on lit.
[[nodiscard]] bool SkillPanelHolds(const SkillPanelLayout& layout, glm::vec2 point_px);

// La ligne VISIBLE qu'un point désigne, ou rien s'il tombe hors des lignes.
//
// Le rang rendu se compte à partir de la première ligne montrée, pas de la
// première de la liste : c'est à l'appelant qui fait défiler d'ajouter la
// sienne.
[[nodiscard]] std::optional<std::size_t> SkillPanelRowAt(const SkillPanelLayout& layout,
                                                         glm::vec2 point_px);

// Le premier rang à montrer après avoir tourné la molette de `wheel_steps`.
//
// Un cran vers le HAUT remonte la liste : la molette pousse le papier, elle ne
// pousse pas la fenêtre.
//
// Le résultat tient TOUJOURS dans ce que le panneau peut montrer, y compris
// avec un pas nul : c'est ce qui rattrape un rang devenu trop grand parce que
// la fenêtre a grandi ou que le grimoire a raccourci. Un rang gardé d'une
// image sur l'autre finit sinon par montrer du vide.
[[nodiscard]] std::size_t SkillPanelScrolled(const SkillPanelLayout& layout, std::size_t row_count,
                                             std::size_t first_row, float wheel_steps);

// Le panneau entier : le cadre, le titre, puis les lignes à partir de
// `first_row`.
[[nodiscard]] std::vector<ScreenQuad> ComposeSkillPanel(const FontAtlas& font,
                                                        const SkillPanelLayout& layout,
                                                        std::string_view title,
                                                        std::span<const SkillPanelRow> rows,
                                                        std::size_t first_row);

}  // namespace ro::render
