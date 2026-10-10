#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <utility>

#include "ro/core/error.h"
#include "ro/render/actor_model_scene.h"
#include "ro/render/camera.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/scene_fog.h"

namespace ro::render {

// Peint les acteurs en modèle que `ComposeActorModel` a posés.
//
// Leurs sommets arrivent déjà placés et teintés : il ne reste qu'à les peindre,
// texture multipliée par la teinte du sommet, comme le client le fait des
// primitives qu'il pousse dans sa file de dessin. La profondeur s'éprouve ET
// s'écrit — ce sont des volumes, que ce qui passe derrière doit voir couvrir.
//
// ⚠ AUCUNE FACE N'EST ÉCARTÉE : la file du client dessine les deux côtés.
class ActorModelRenderer {
 public:
  ActorModelRenderer() = default;
  ActorModelRenderer(const ActorModelRenderer&) = delete;
  ActorModelRenderer& operator=(const ActorModelRenderer&) = delete;
  ActorModelRenderer(ActorModelRenderer&& other) noexcept;
  ActorModelRenderer& operator=(ActorModelRenderer&& other) noexcept;
  ~ActorModelRenderer();

  [[nodiscard]] static core::Result<ActorModelRenderer> Create();

  void Draw(const Camera& camera, std::span<const DrawnActorModel> models, float aspect_ratio,
            const SceneFog& fog = SceneFog());

  // Oublie les textures d'une source qui s'en va — une ressource déchargée,
  // un emblème remplacé.
  void Forget(const void* owner);

 private:
  void Release() noexcept;
  [[nodiscard]] const GlTexture* TextureFor(const ActorModelTexture& texture);

  GlProgram program_;
  // ⚠ UNE TABLE ORDONNÉE, et ses clés sont des IDENTITÉS : l'adresse de la
  // source et le rang de la texture en elle.
  std::map<std::pair<const void*, std::uint64_t>, GlTexture> textures_;
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
};

}  // namespace ro::render
