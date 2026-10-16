#pragma once

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/gl_program.h"
#include "ro/render/sky_body.h"

namespace ro::render {

// LE VIDE DERRIÈRE LE MONDE, assombri par l'heure : le fond uni et les nuages
// que certaines cartes — Gonryun, les cartes flottantes — montrent sous le
// terrain.
//
// Ce vide est ce que ni le sol ni le décor n'ont couvert : la profondeur y est
// restée à celle de l'effacement. Un triangle plein écran, posé tout au fond et
// éprouvé contre elle, ne touche donc que ces pixels-là, et MULTIPLIE leur
// couleur par `daylight` : fond et nuages ensemble, sans rien savoir de l'effet
// qui peint ces derniers. Un sort lancé au-dessus du sol reste intact ; un sort
// lancé au-dessus du vide s'assombrit avec lui, comme la nuit le voudrait.
//
// ⚠ À DESSINER APRÈS CE QUI PEINT LE VIDE, et dans la cible de la scène : la
// profondeur qu'il éprouve est la sienne.
class VoidShade {
 public:
  VoidShade() = default;
  VoidShade(const VoidShade&) = delete;
  VoidShade& operator=(const VoidShade&) = delete;
  VoidShade(VoidShade&& other) noexcept;
  VoidShade& operator=(VoidShade&& other) noexcept;
  ~VoidShade();

  [[nodiscard]] static core::Result<VoidShade> Create();

  // Rien à un : la lumière de midi ne retire rien.
  void Draw(const glm::vec3& daylight) const;

  // L'ASTRE SUR LE FOND, là où rien ne le cache : un disque et son halo,
  // ajoutés à ce qui est peint. Le soleil chaud, la lune pâle et bleutée. Rien
  // quand il ne se voit pas. ⚠ APRÈS `Draw` : la nuit n'éteint pas la lune.
  void DrawBody(const SkyBodyOnScreen& body, bool moon, float aspect_ratio) const;

  [[nodiscard]] bool valid() const noexcept { return vertex_array_ != 0; }

 private:
  GlProgram program_;
  GlProgram body_program_;
  unsigned int vertex_array_ = 0;
};

}  // namespace ro::render
