#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/map_effects.h"
#include "ro/render/scene_fog.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "world/actor_footprints.h"
#include "world/actor_sounds.h"
#include "world/item_drops.h"
#include "world/map_stage.h"

namespace moonclient {

// LES EFFETS DU DÉCOR DE CONNEXION : ceux de la ville — émetteurs, fumées,
// anneaux de vent, ciel, météo — et ceux de ses passants.
//
// ⚠ LES MÊMES PIÈCES QUE LA PARTIE, dans le même ordre et par les mêmes portes :
// le catalogue, la troupe des nœuds, le moteur d'effets, leur renderer, et
// `DrawEffectLayers`. Deux chemins à part finiraient par ne plus montrer la
// même chose, et c'est le décor qui se tromperait.
//
// ⚠ SANS JOUEUR : la session spectateur n'en a pas. Ce que la partie mesure
// depuis lui — la portée des émetteurs, l'ancre du ciel et de la météo — se
// mesure depuis le point que la caméra vise.
//
// ⚠ AUCUN RÉGLAGE N'EST LU, comme pour le reste du décor : `/effect` et
// `/mineffect` gardent leur défaut, effets montrés et pleins.
//
// ⚠ NI DÉPLACÉE NI COPIÉE : le moteur d'effets garde l'adresse du catalogue et
// de la troupe qui vivent à côté de lui.
class BackdropEffects {
 public:
  explicit BackdropEffects(ro::vfs::Vfs& vfs);
  BackdropEffects(const BackdropEffects&) = delete;
  BackdropEffects& operator=(const BackdropEffects&) = delete;
  BackdropEffects(BackdropEffects&&) = delete;
  BackdropEffects& operator=(BackdropEffects&&) = delete;
  ~BackdropEffects() = default;

  // AVANT LES PASSANTS : les effets qui passent derrière les corps. Le lot est
  // celui que l'image précédente a fait vivre, comme dans la partie.
  void DrawBehindActors(const ro::render::Camera& camera, float aspect_ratio,
                        const ro::render::SceneFog& fog, ro::render::SpriteRenderer& sprites);

  // APRÈS LES PASSANTS ET L'EAU : les effets que la carte pose — émetteurs,
  // fumées, vent —, par-dessus la nappe comme dans la partie.
  void DrawMapEffects(const MapStage& stage, const ro::render::Camera& camera, float aspect_ratio,
                      const ro::render::SceneFog& fog);

  // APRÈS LES PASSANTS : lève le ciel et la météo au point visé, fait vivre les
  // effets de la carte, puis ceux des passants, et peint ce qu'ils montrent.
  //
  // `map_objects` sont les objets d'effet de la ville à cette image — ceux du
  // RSW, de son document et de ses modèles, à la nuit qui tombe : voir
  // `ro::render::SceneMapEffects`. Une liste qui change se relève.
  //
  // ⚠ `scene.audio` NE DOIT PAS ÊTRE NUL : la couche des effets fait entendre
  // ce qu'elle sème, et ne vérifie pas qu'il y a de quoi l'entendre.
  void DrawAfterActors(MapStage& stage, const SoundStage& scene, ActorSounds& sounds,
                       ItemDrops& drops, ro::render::SpriteRenderer& sprites, glm::vec3 anchor,
                       float aspect_ratio, float viewport_width, float viewport_height,
                       const ro::render::SceneFog& fog,
                       std::vector<ro::formats::RswEffect> map_objects);

  // La portée des effets de la ville, que la page « Graphics » règle : voir
  // `MapEffects::SetReachUnits`.
  void SetMapEffectReach(float reach_units) noexcept { map_effects_.SetReachUnits(reach_units); }

  // Ce que les effets ont demandé au monde des passants — une teinte, un
  // drapeau posé par une suite —, rendu HORS du dessin, qui ne le tient qu'en
  // lecture. Voir `EffectsInPlay::TakeActorEffects`.
  void Settle(ro::sim::World& world);

  [[nodiscard]] const EffectsInPlay& in_play() const noexcept { return effects_; }

  // LE SKIN A CHANGÉ : un même chemin désigne une autre image, et certaines
  // images d'effet sont des icônes d'objet.
  void ForgetTextures() {
    if (renderer_.has_value()) {
      renderer_->ForgetTextures();
    }
  }

 private:
  // Lève le ciel et la météo, une fois par ville. Voir la partie : elle les
  // attache au joueur, que le décor n'a pas.
  void RaiseSky(const MapStage& stage, glm::vec3 anchor);
  // Fait vivre les émetteurs, les fumées et le vent, et lance ce qu'ils sèment.
  void AdvanceMapEffects(MapStage& stage, const SoundStage& scene, ActorSounds& sounds,
                         glm::vec3 anchor, std::vector<ro::formats::RswEffect> map_objects);

  ro::vfs::Vfs* vfs_ = nullptr;
  ro::render::EffectCatalog catalog_;
  EffectNodesInPlay nodes_;
  EffectsInPlay effects_;
  // Sans renderer — un pilote qui refuse le programme —, la ville se montre
  // sans effets, comme la partie.
  std::optional<ro::render::EffectRenderer> renderer_;
  // Les cases de terrain des effets : la grille de visée, les flaques.
  std::optional<ro::render::GroundQuadRenderer> ground_;
  ro::render::MapEffects map_effects_;
  // Les objets d'effet levés, pour savoir quand la liste change.
  std::vector<ro::formats::RswEffect> raised_objects_;
  std::vector<ro::render::DrawnWorldQuads> map_quads_;
  ActorFootprints footprints_;
  std::vector<EffectsInPlay::ActorEffectToRecord> to_record_;
  bool map_raised_ = false;
  bool sky_raised_ = false;
};

}  // namespace moonclient
