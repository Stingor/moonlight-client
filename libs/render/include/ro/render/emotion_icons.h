#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/gl_texture.h"

namespace ro::render {

// LES ÉMOTICÔNES EN VIGNETTES, pour une grille d'interface.
//
// ⚠ LE MÊME SPRITE QUE LES BULLES, et c'est tout l'intérêt : `emotion.spr` est
// déjà lu et déjà en mémoire pour ce qui flotte au-dessus des têtes. Une seconde
// lecture donnerait deux sources pour la même image, qui se mettraient à
// diverger le jour où l'une suivrait une palette et pas l'autre.
//
// ⚠ MAIS PAS LE MÊME CHEMIN VERS L'ÉCRAN. Une bulle se dessine par le renderer,
// en quads OpenGL ; une vignette d'interface veut des TEXTURES qu'ImGui sache
// poser. C'est la même conversion que pour les icônes de compétence — image
// décodée, puis téléversée.
//
// ⚠ ET À LA DEMANDE. Le sprite en porte près de cent ; les téléverser toutes au
// démarrage coûterait des centaines de textures pour une grille que le joueur
// n'ouvrira peut-être jamais.

// UNE COUCHE D'UNE IMAGE D'ÉMOTICÔNE.
//
// 🔴 UNE COUCHE SEULE NE VEUT RIEN DIRE. Une émoticône du jeu en superpose
// plusieurs : la première est la BULLE VIDE, et ce qu'elle contient arrive
// par-dessus. Prendre la première pour « celle qui représente » donne une
// grille de bulles vides, alors que la même émoticône s'affiche correctement
// au-dessus des têtes — le renderer, lui, les peint toutes.
struct EmotionLayer {
  // Le nom OpenGL de la texture, zéro quand elle n'a pas pu être téléversée.
  unsigned texture = 0;
  // Les quatre coins, dans l'ordre du sprite et dans SON espace : c'est
  // l'appelant qui met à l'échelle et centre. ⚠ QUATRE POINTS ET NON UN
  // RECTANGLE : un quad peut être tourné, et un rectangle aligné aux axes ne
  // saurait pas le représenter.
  std::array<glm::vec2, 4> corners{};
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
};

// La boîte qui contient une action ENTIÈRE, toutes ses images confondues.
struct EmotionBounds {
  glm::vec2 min{0.0F, 0.0F};
  glm::vec2 max{0.0F, 0.0F};

  [[nodiscard]] bool usable() const noexcept { return max.x > min.x && max.y > min.y; }
};

class EmotionIcons {
 public:
  // Le sprite et son animation sont passés par référence et doivent vivre plus
  // longtemps : les vignettes s'y lisent au fil du jeu, pas une fois pour
  // toutes.
  EmotionIcons(const formats::SprSprite& sprite, const formats::ActAnimation& animation)
      : sprite_(&sprite), animation_(&animation) {}

  // Les couches d'UNE image, dans l'ordre du dessin. Vide quand le sprite ne
  // porte pas cette émoticône-là.
  //
  // ⚠ L'ORDRE EST CELUI DU DESSIN, et il ne se trie pas : le changer mettrait
  // la bulle par-dessus ce qu'elle contient.
  [[nodiscard]] std::span<const EmotionLayer> Layers(std::uint16_t emotion, std::uint16_t frame);

  // LA BOÎTE DE TOUTE L'ACTION, jamais celle d'une image.
  //
  // 🔴 C'EST ELLE QUI FIXE L'ÉCHELLE ET LE CENTRAGE. Les prendre sur l'image
  // courante les ferait changer à chaque image : le sprite se dilaterait et se
  // contracterait au rythme de son animation, dans une case qui, elle, ne bouge
  // pas. C'est la même règle que `GenerateBoundingBox` de GRF Editor.
  [[nodiscard]] EmotionBounds ActionBounds(std::uint16_t emotion);

  // Combien d'images porte son animation, et ce que dure chacune. Zéro image
  // veut dire que le sprite ne porte pas cette émoticône-là.
  [[nodiscard]] std::uint16_t FrameCount(std::uint16_t emotion) const;
  [[nodiscard]] float FrameMs(std::uint16_t emotion) const;

 private:
  // Ce qu'une image coûte : ses couches, et les textures qui les portent.
  struct LoadedFrame {
    std::vector<EmotionLayer> layers;
    std::vector<GlTexture> textures;
  };

  // ⚠ UNE SEULE CLÉ POUR LES DEUX : l'émoticône en haut, l'image en bas.
  [[nodiscard]] static std::uint32_t KeyOf(std::uint16_t emotion, std::uint16_t frame) {
    return static_cast<std::uint32_t>(emotion) << 16U | frame;
  }

  // L'action qui dessine ce numéro du protocole, ou rien. Voir `EmotionAction`.
  [[nodiscard]] const formats::ActAction* ActionOf(std::uint16_t emotion) const;

  const formats::SprSprite* sprite_ = nullptr;
  const formats::ActAnimation* animation_ = nullptr;
  std::unordered_map<std::uint32_t, LoadedFrame> loaded_;
  std::unordered_map<std::uint16_t, EmotionBounds> boxes_;
};

}  // namespace ro::render
