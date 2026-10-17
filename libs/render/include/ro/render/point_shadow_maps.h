#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/core/error.h"
#include "ro/render/point_shadow.h"

namespace ro::render {

// L'unité de texture où les nuanceurs lisent la carte cubique de rang zéro ;
// les suivantes prennent les unités suivantes. 0 porte la texture, 1 la
// lightmap, 2 et 3 l'ombre du soleil.
inline constexpr int kPointShadowFirstTextureUnit = 4;

// LES CARTES D'OMBRE DES LUMIÈRES PONCTUELLES : une texture cubique de
// profondeur par lumière ombrée, peinte face par face depuis la lumière, puis
// lue par les nuanceurs qui reçoivent ces lumières — voir `kPointLightGlsl`.
class PointShadowMaps {
 public:
  PointShadowMaps() = default;
  PointShadowMaps(const PointShadowMaps&) = delete;
  PointShadowMaps& operator=(const PointShadowMaps&) = delete;
  PointShadowMaps(PointShadowMaps&& other) noexcept;
  PointShadowMaps& operator=(PointShadowMaps&& other) noexcept;
  ~PointShadowMaps();

  [[nodiscard]] static core::Result<PointShadowMaps> Create(int side_texels);

  // Dirige le dessin vers la carte et efface les profondeurs, décalées pour les
  // faces peintes contre l'acné d'une face qui s'ombre elle-même.
  // ⚠ RETIENT la cible et la fenêtre en vigueur, que `EndDepthPass` rétablit.
  void BeginDepthPass();
  // Dirige le dessin vers la face `face` de la carte `rank`, et l'efface.
  void BeginFace(std::size_t rank, std::size_t face) const;
  // Le même recul que `SunShadowMap::SetCasterOffset`.
  void SetCasterOffset(bool on) const;
  void EndDepthPass() const;

  // Lie la carte `rank` à l'unité `unit`.
  void Bind(int unit, std::size_t rank) const;

  [[nodiscard]] bool valid() const noexcept { return framebuffer_ != 0; }

  // CE QUE CHAQUE CARTE PORTE DÉJÀ, et le compte des passes : voir
  // `PointShadowRepaints`.
  [[nodiscard]] const std::array<std::optional<PaintedPointLight>, kMaxShadowedPointLights>&
  painted() const noexcept {
    return painted_;
  }
  void NotePainted(std::size_t rank, const PaintedPointLight& light) { painted_.at(rank) = light; }
  [[nodiscard]] std::uint64_t TakeFrame() noexcept { return frames_++; }
  [[nodiscard]] int side_texels() const noexcept { return side_texels_; }

 private:
  static constexpr int kViewportValues = 4;

  unsigned int framebuffer_ = 0;
  std::array<unsigned int, kMaxShadowedPointLights> cubes_{};
  int side_texels_ = 0;
  int previous_framebuffer_ = 0;
  int previous_viewport_[kViewportValues] = {};
  std::array<std::optional<PaintedPointLight>, kMaxShadowedPointLights> painted_{};
  std::uint64_t frames_ = 0;
};

}  // namespace ro::render
