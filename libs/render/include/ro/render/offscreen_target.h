#pragma once

#include "ro/core/error.h"
#include "ro/formats/bmp.h"

namespace ro::render {

// Cible de rendu hors écran, avec profondeur.
//
// C'est la pièce qui rend le renderer vérifiable : on dessine une scène connue,
// on relit l'image, on la compare. Sans elle, la seule vérification possible
// serait de regarder l'écran.
class OffscreenTarget {
 public:
  OffscreenTarget() = default;
  OffscreenTarget(const OffscreenTarget&) = delete;
  OffscreenTarget& operator=(const OffscreenTarget&) = delete;
  OffscreenTarget(OffscreenTarget&& other) noexcept;
  OffscreenTarget& operator=(OffscreenTarget&& other) noexcept;
  ~OffscreenTarget();

  [[nodiscard]] static core::Result<OffscreenTarget> Create(int width, int height);

  void Bind() const;

  // Dirige le DESSIN vers cette cible sans toucher à la source de LECTURE.
  //
  // ⚠ C'EST CE QUI PERMET UN TRANSFERT D'IMAGE À IMAGE DEPUIS L'ÉCRAN : `Bind`
  // lie les deux bouts à la fois, et la source qu'on voulait lire deviendrait
  // alors la cible elle-même. Voir `screen_grab.h`.
  void BindAsDrawTarget() const;

  // Relit le tampon de couleur en RGBA8, rangée 0 en HAUT — OpenGL rend les
  // rangées dans l'autre sens, et la remise à l'endroit se fait ici plutôt que
  // chez chaque appelant.
  [[nodiscard]] core::Result<formats::Image> ReadPixels() const;

  [[nodiscard]] int width() const noexcept { return width_; }
  [[nodiscard]] int height() const noexcept { return height_; }

 private:
  unsigned int framebuffer_ = 0;
  unsigned int color_ = 0;
  unsigned int depth_ = 0;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace ro::render
