#pragma once

#include <cstdint>
#include <vector>

#include "ro/core/error.h"
#include "ro/render/camera.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/scene_fog.h"
#include "ro/render/water_plane.h"

namespace ro::render {

// Dessine la nappe d'eau d'une carte.
//
// Il reçoit un maillage déjà construit et des images déjà décodées : il ne lit
// aucun fichier et ne connaît ni le VFS ni les formats de carte — comme le
// renderer de terrain, et pour la même raison.
//
// 🔴 IL SE DESSINE APRÈS LE TERRAIN ET LE DÉCOR, jamais avant : la nappe est
// TRANSPARENTE, et ce qu'elle voile doit déjà être là. Il n'écrit pas dans le
// tampon de profondeur — deux nappes qui se recouvrent doivent se mélanger,
// pas se masquer.
//
// 🔴 ET APRÈS LES CORPS ET LEURS OMBRES, dans une scène qui en porte. Le jeu
// peint ses corps en écrivant leur profondeur, PUIS ses cellules d'eau en
// l'éprouvant : la nappe passe devant la part d'un corps qui est sous sa
// surface, et c'est ce qui met les pieds dans l'eau. Un pixel de corps trop
// transparent pour écrire sa profondeur en prend la teinte entière.
class WaterRenderer {
 public:
  WaterRenderer() = default;
  WaterRenderer(const WaterRenderer&) = delete;
  WaterRenderer& operator=(const WaterRenderer&) = delete;
  WaterRenderer(WaterRenderer&& other) noexcept;
  WaterRenderer& operator=(WaterRenderer&& other) noexcept;
  ~WaterRenderer();

  [[nodiscard]] static core::Result<WaterRenderer> Create();

  // Téléverse la nappe et ses animations.
  //
  // Une animation dont il manque des images se dessine quand même, avec ce
  // qu'elle a : une eau figée vaut mieux qu'un trou dans la carte.
  [[nodiscard]] core::Result<void> Upload(const WaterPlane& plane,
                                          const std::vector<WaterAnimation>& animations);

  // `elapsed_ms` est l'âge de la scène : il décide de l'image montrée et de la
  // phase de la houle. Sans nappe, ne dessine rien.
  void Draw(const Camera& camera, float aspect_ratio, std::uint32_t elapsed_ms,
            const SceneFog& fog = SceneFog()) const;

  [[nodiscard]] bool empty() const noexcept { return zones_.empty(); }

 private:
  // Les images d'un type, déjà sur la carte graphique.
  struct UploadedAnimation {
    std::int32_t type = 0;
    std::vector<GlTexture> frames;
  };

  [[nodiscard]] const GlTexture* FrameOf(std::int32_t type, int frame) const;

  GlProgram program_;
  std::vector<UploadedAnimation> animations_;
  std::vector<WaterZoneMesh> zones_;
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
};

}  // namespace ro::render
