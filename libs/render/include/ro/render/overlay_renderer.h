#pragma once

#include <cstddef>
#include <map>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/screen_overlay.h"

namespace ro::render {

// Dessine ce qui se pose PAR-DESSUS la scène, en pixels de fenêtre.
//
// Séparé du renderer de sprites, et pas seulement par commodité : celui-ci ne
// projette rien, ne trie rien et n'écrit aucune profondeur. Il peint les quads
// dans l'ordre où on les lui donne, le dernier par-dessus, comme une interface
// se dessine.
//
// ⚠ À appeler EN DERNIER, une fois la scène peinte : rien de ce qu'il dessine
// ne doit être masqué par le monde.
class OverlayRenderer {
 public:
  // Défini dans le fichier source, où le type des sommets est complet.
  OverlayRenderer();
  OverlayRenderer(const OverlayRenderer&) = delete;
  OverlayRenderer& operator=(const OverlayRenderer&) = delete;
  OverlayRenderer(OverlayRenderer&& other) noexcept;
  OverlayRenderer& operator=(OverlayRenderer&& other) noexcept;
  ~OverlayRenderer();

  [[nodiscard]] static core::Result<OverlayRenderer> Create();

  // Peint les quads dans une fenêtre de cette taille, en pixels.
  //
  // L'origine est le coin HAUT-GAUCHE et l'ordonnée descend : c'est le repère
  // de la souris et celui d'un sprite, donc celui dans lequel l'appelant a
  // déjà tout calculé.
  void Draw(std::span<const ScreenQuad> quads, float viewport_width, float viewport_height);

  // Oublie les textures dont plus personne ne se sert.
  void ForgetUnusedTextures();

  // OUBLIE LA TEXTURE D'UN ATLAS, parce qu'il va CHANGER ou disparaître.
  //
  // 🔴 SANS CET APPEL, UN ATLAS REGRAVÉ SORT EN CHARABIA. Le cache reconnaît un
  // atlas par son ADRESSE, sur l'invariante qu'une image gravée ne change plus ;
  // or une police regravée dans le même `optional` retombe à la MÊME adresse.
  // Le cache rend alors l'ancienne texture, que les nouvelles coordonnées de
  // glyphe découpent n'importe où — c'est ce qu'a montré un changement de taille
  // des noms d'entité.
  //
  // ⚠ À APPELER AVANT DE DÉTRUIRE L'IMAGE, tant que son adresse a encore un
  // sens.
  void ForgetAtlas(const formats::Image& atlas);

  [[nodiscard]] std::size_t cached_texture_count() const noexcept { return textures_.size(); }

 private:
  void Release() noexcept;
  [[nodiscard]] const GlTexture* TextureFor(const ScreenQuad& quad);

  struct CachedTexture {
    GlTexture texture;
    bool used = false;
  };

  struct OverlayVertex;

  GlProgram program_;
  // Le pixel blanc des quads unis, téléversé à la première demande.
  GlTexture white_;
  std::map<SpriteImageKey, CachedTexture> textures_;
  // Les atlas, reconnus par leur adresse : ils vivent aussi longtemps que ce
  // qui les a chargés, et ne changent pas une fois gravés.
  std::map<const formats::Image*, GlTexture> atlases_;
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
  // LES TAMPONS D'UN DESSIN, gardés d'un appel à l'autre : chaque `Draw` les
  // vide sans rendre leur mémoire, si bien qu'une interface qui ne grandit pas
  // ne fait plus aucune allocation.
  std::vector<const GlTexture*> quad_textures_;
  std::vector<OverlayVertex> vertices_;
};

}  // namespace ro::render
