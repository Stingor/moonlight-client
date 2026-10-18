#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/model_veil.h"
#include "ro/render/point_lights.h"
#include "ro/render/point_shadow.h"

namespace ro::render {

// Les unités de texture des lumières ponctuelles : le tableau de cubes de
// chaque palier à partir de la première, puis la texture-tampon des lumières.
// 0 porte la texture, 1 la lightmap, 2 et 3 l'ombre du soleil.
inline constexpr int kPointShadowFirstTextureUnit = 4;
inline constexpr int kPointLightBufferTextureUnit =
    kPointShadowFirstTextureUnit + static_cast<int>(kPointShadowTierCount);

// CE QU'UN ACTEUR DONNE À VOIR À UNE FACE : s'il change, la face se repeint.
// La composition est désignée par son adresse, qu'on ne relit pas.
struct ActorShadowKey {
  const void* composition = nullptr;
  std::size_t action_index = 0;
  std::size_t frame_index = 0;
  std::size_t quad_count = 0;
  glm::vec3 ground_position{0.0F};
  float scale = 0.0F;
  float height_scale = 0.0F;

  friend bool operator==(const ActorShadowKey&, const ActorShadowKey&) = default;
};

// CE QU'UNE FACE D'UNE CARTE PORTE DÉJÀ.
struct PointShadowFace {
  bool painted = false;
  // L'image où elle a été peinte.
  std::uint64_t painted_frame = 0;
  // Les acteurs qu'elle a peints, dans l'ordre où ils sont venus.
  std::vector<ActorShadowKey> actors;
};

// UNE CARTE CUBIQUE, et la génération du sol et du décor qu'elle a peints.
struct PointShadowCube {
  std::array<PointShadowFace, kCubeFaceCount> faces{};
  std::uint64_t static_generation = 0;
};

// LE SOL ET LE DÉCOR QUE PEIGNENT LES CARTES : leurs renderers, combien de
// fois chacun a été téléversé, et les poses du décor qui portent ombre. Qu'un
// seul change, et toutes les cartes se repeignent.
struct PointShadowScene {
  const void* terrain = nullptr;
  std::uint64_t terrain_uploads = 0;
  const void* models = nullptr;
  std::uint64_t model_uploads = 0;
  bool casting_given = false;
  ModelOpacities casting;

  friend bool operator==(const PointShadowScene&, const PointShadowScene&) = default;
};

// LES LUMIÈRES PONCTUELLES CÔTÉ CARTE GRAPHIQUE : la texture-tampon que lisent
// les nuanceurs, et un tableau de cartes cubiques de profondeur par palier,
// qui grandit à la demande jusqu'à `kPointShadowTierMaxLayers`. Voir
// `kPointLightGlsl` et docs/adr/0123.
class PointLightStage {
 public:
  PointLightStage() = default;
  PointLightStage(const PointLightStage&) = delete;
  PointLightStage& operator=(const PointLightStage&) = delete;
  PointLightStage(PointLightStage&& other) noexcept;
  PointLightStage& operator=(PointLightStage&& other) noexcept;
  ~PointLightStage();

  [[nodiscard]] static core::Result<PointLightStage> Create();

  [[nodiscard]] bool valid() const noexcept { return framebuffer_ != 0; }

  // Téléverse les lumières de `set`, leurs cartes comprises.
  void Upload(const PointLightSet& set);
  // Vrai quand ce qui est téléversé est `set` : autant de lumières, et au moins
  // une.
  [[nodiscard]] bool Holds(const PointLightSet& set) const noexcept {
    return valid() && !set.lights.empty() && uploaded_lights_ == set.lights.size();
  }
  // Lie la texture-tampon et les tableaux à leurs unités.
  void Bind() const;

  // LA RÉPARTITION DES CARTES, et ce que chacune porte.
  [[nodiscard]] PointShadowSlots& slots() noexcept { return slots_; }
  [[nodiscard]] PointShadowCube& cube(std::size_t tier, std::size_t layer) {
    return cubes_.at(tier).at(layer);
  }
  // Fait tenir au tableau de chaque palier les couches que `slots` emploie,
  // en gardant ce qu'elles portent.
  void EnsureLayers();
  // LA GÉNÉRATION DU SOL ET DU DÉCOR que peint l'image : elle avance quand
  // `scene` diffère de celle de l'image précédente.
  // Le rang de l'image qui commence.
  [[nodiscard]] std::uint64_t TakeFrame() noexcept { return ++frame_; }
  [[nodiscard]] std::uint64_t NoteScene(const PointShadowScene& scene) {
    if (!(scene == scene_)) {
      scene_ = scene;
      ++generation_;
    }
    return generation_;
  }

  // Dirige le dessin vers les cartes, les faces peintes reculées contre l'acné.
  // ⚠ RETIENT la cible et la fenêtre en vigueur, que `EndDepthPass` rétablit.
  void BeginDepthPass();
  // Dirige le dessin vers la face `face` de la carte `layer` du palier `tier`,
  // et l'efface : rien n'y ombre plus.
  void BeginFace(std::size_t tier, std::size_t layer, std::size_t face) const;
  // Le même recul que `SunShadowMap::SetCasterOffset`.
  void SetCasterOffset(bool on) const;
  void EndDepthPass() const;

  // Les octets que pèsent les cartes : le budget de `kPointShadowTierMaxLayers`.
  [[nodiscard]] std::size_t shadow_bytes() const noexcept;

 private:
  static constexpr int kViewportValues = 4;

  unsigned int framebuffer_ = 0;
  unsigned int light_buffer_ = 0;
  unsigned int light_texture_ = 0;
  std::size_t uploaded_lights_ = 0;
  std::array<unsigned int, kPointShadowTierCount> tier_textures_{};
  // Les couches que chaque tableau tient.
  std::array<std::size_t, kPointShadowTierCount> tier_layers_{};
  std::array<std::vector<PointShadowCube>, kPointShadowTierCount> cubes_{};
  PointShadowSlots slots_;
  PointShadowScene scene_;
  // Une carte neuve porte la génération zéro : elle se peint à la première.
  std::uint64_t generation_ = 1;
  std::uint64_t frame_ = 0;
  int previous_framebuffer_ = 0;
  int previous_viewport_[kViewportValues] = {};
};

}  // namespace ro::render
