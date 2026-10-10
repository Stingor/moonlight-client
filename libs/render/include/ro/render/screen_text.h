#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/font_atlas.h"
#include "ro/render/screen_overlay.h"

namespace ro::render {

// Écrire une ligne de texte sur la couche d'écran.
//
// Le texte est de l'UTF-8. Ce que la police n'a pas gravé ne se dessine pas :
// une ligne peut donc revenir plus courte qu'elle n'est longue, et c'est
// préférable à une file de rectangles vides.

// La couleur d'un nom, et celle de son ombre.
inline constexpr std::array<std::uint8_t, 4> kNameColor = {255, 255, 255, 255};
inline constexpr std::array<std::uint8_t, 4> kNameShadowColor = {0, 0, 0, 200};

// De combien l'ombre d'un nom se décale, en pixels.
inline constexpr float kTextShadowPixels = 1.0F;

// La largeur qu'occupera ce texte, en pixels.
[[nodiscard]] float MeasureText(const FontAtlas& atlas, std::string_view utf8);

// Les quads d'une ligne, le point donné étant le coin GAUCHE de sa ligne de
// base.
[[nodiscard]] std::vector<ScreenQuad> ComposeText(const FontAtlas& atlas, std::string_view utf8,
                                                  glm::vec2 baseline_px,
                                                  const std::array<std::uint8_t, 4>& color);

// La même ligne, DOUBLÉE d'une ombre, le point donné étant le coin gauche de
// sa ligne de base.
//
// L'ombre est ce qui rend une ligne lisible sur n'importe quel fond, décor
// clair comme sol sombre.
[[nodiscard]] std::vector<ScreenQuad> ComposeLabel(
    const FontAtlas& atlas, std::string_view utf8, glm::vec2 baseline_px,
    const std::array<std::uint8_t, 4>& color = kNameColor);

// La même, avec une ombre CHOISIE : voir la variante centrée qui suit.
[[nodiscard]] std::vector<ScreenQuad> ComposeLabel(const FontAtlas& atlas, std::string_view utf8,
                                                   glm::vec2 baseline_px,
                                                   const std::array<std::uint8_t, 4>& color,
                                                   const std::array<std::uint8_t, 4>& shadow_color);

// La même ligne, CENTRÉE sur une abscisse et doublée d'une ombre.
//
// L'ombre est ce qui rend un nom lisible sur n'importe quel sol : le client en
// pose une sous chacun des siens.
[[nodiscard]] std::vector<ScreenQuad> ComposeCenteredLabel(
    const FontAtlas& atlas, std::string_view utf8, glm::vec2 center_baseline_px,
    const std::array<std::uint8_t, 4>& color = kNameColor);

// La même, avec une ombre CHOISIE — sa couleur et sa transparence.
//
// ⚠ UN ALPHA NUL N'EN POSE AUCUNE, et ce n'est pas une optimisation : des quads
// entièrement transparents coûtent le même dessin que les autres, et un nom
// s'écrit une fois par entité en vue. C'est ainsi que se règle « pas d'ombre ».
[[nodiscard]] std::vector<ScreenQuad> ComposeCenteredLabel(
    const FontAtlas& atlas, std::string_view utf8, glm::vec2 center_baseline_px,
    const std::array<std::uint8_t, 4>& color, const std::array<std::uint8_t, 4>& shadow_color);

// OÙ COUPER UN TEXTE pour qu'aucune ligne ne dépasse `most_width_px`.
//
// Rend les morceaux dans l'ordre, chacun une VUE sur `utf8` : rien n'est
// recopié, et la chaîne doit donc vivre aussi longtemps que le résultat.
//
// ⚠ LA COUPE SE FAIT AUX ESPACES, et à DÉFAUT au milieu d'un mot : un mot plus
// large que la bulle existe — une adresse collée, un nom à rallonge — et le
// laisser déborder annulerait la borne qu'on vient de poser.
//
// Une largeur nulle ou négative rend le texte d'un seul tenant : c'est ce que
// veut dire « pas de borne », et cela évite à l'appelant un cas à traiter.
[[nodiscard]] std::vector<std::string_view> WrapTextLines(const FontAtlas& atlas,
                                                          std::string_view utf8,
                                                          float most_width_px);

// UN PARAGRAPHE CENTRÉ, chaque ligne doublée de son ombre.
//
// ⚠ LE POINT DONNÉ EST LA LIGNE DE BASE DE LA DERNIÈRE LIGNE, et non de la
// première : une bulle POUSSE VERS LE HAUT depuis la tête de qui parle. Prendre
// la première pour référence ferait descendre le texte sur le personnage à
// mesure que la phrase s'allonge.
[[nodiscard]] std::vector<ScreenQuad> ComposeCenteredParagraph(
    const FontAtlas& atlas, std::string_view utf8, glm::vec2 center_baseline_px,
    float most_width_px, const std::array<std::uint8_t, 4>& color = kNameColor);

}  // namespace ro::render
