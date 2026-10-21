#include "ro/render/point_shadow_pass.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/point_shadow.h"
#include "ro/render/world_frustum.h"

namespace ro::render {
namespace {

// LA SPHÈRE OÙ SE TIENT UN CORPS EN SPRITE, pour savoir quelles faces le
// voient : son centre à cette hauteur au-dessus de ses pieds, ce rayon à
// l'échelle un. Large : un corps compté de trop ne coûte qu'une face peinte.
constexpr float kActorShadowCenterHeightWorld = 15.0F;
constexpr float kActorShadowReachWorld = 25.0F;

// Un corps de l'image, tel que les faces le voient.
struct ShadowActor {
  std::size_t lot = 0;
  const DrawnActor* drawn = nullptr;
  ActorShadowKey key;
  glm::vec3 center{0.0F};
  float reach = 0.0F;
};

[[nodiscard]] std::vector<ShadowActor> ShadowActorsOf(const SunShadowCasters& casters) {
  std::vector<ShadowActor> actors;
  if (casters.sprites == nullptr) {
    return actors;
  }
  for (std::size_t lot = 0; lot < casters.actor_lots.size(); ++lot) {
    for (const DrawnActor& drawn : casters.actor_lots[lot]) {
      if (drawn.actor == nullptr || drawn.actor->quads.empty()) {
        continue;
      }
      const ComposedActor& composed = *drawn.actor;
      const float scale = std::max(drawn.scale, 0.0F);
      actors.push_back(ShadowActor{
          .lot = lot,
          .drawn = &drawn,
          .key = {.composition = drawn.actor,
                  .action_index = composed.action_index,
                  .frame_index = composed.frame_index,
                  .quad_count = composed.quads.size(),
                  .lead_corner = composed.quads.front().corners[0],
                  .ground_position = drawn.ground_position,
                  .scale = drawn.scale,
                  .height_scale = drawn.height_scale},
          .center =
              drawn.ground_position + glm::vec3(0.0F, kActorShadowCenterHeightWorld * scale, 0.0F),
          .reach = kActorShadowReachWorld * scale});
    }
  }
  return actors;
}

// L'emprise des acteurs en modèle, dont les sommets sont déjà dans le monde.
[[nodiscard]] std::optional<WorldBox> ActorModelsBox(std::span<const DrawnActorModel> models) {
  std::optional<WorldBox> box;
  for (const DrawnActorModel& model : models) {
    for (const DrawnActorModelMesh& mesh : model.meshes) {
      for (const ActorModelVertex& vertex : mesh.vertices) {
        const glm::vec3& position = vertex.position;
        if (!box.has_value()) {
          box = WorldBox{.lowest = position, .highest = position};
        }
        box->lowest = glm::min(box->lowest, position);
        box->highest = glm::max(box->highest, position);
      }
    }
  }
  return box;
}

// Ce que voit la face `face` de la carte de `light`.
[[nodiscard]] ShadowSight SightOf(const PointLight& light, std::size_t face) {
  const float far_world = PointShadowFar(light.range_world);
  return ShadowSight{.frustum = WorldFrustum::Of(
                         PointShadowFaceViewProjections(light.world_position, far_world).at(face)),
                     .center = light.world_position,
                     .radius = far_world};
}

// UNE FACE À REPEINDRE, et les corps qu'elle voit, par leur rang.
struct FaceJob {
  std::size_t rank = 0;
  std::size_t face = 0;
  std::vector<std::size_t> actors;
  bool actor_models_seen = false;
  // Elle ne se repeint que parce qu'un modèle s'anime dans son champ.
  bool animated = false;
};

// LE PEINTRE DES FACES D'UNE IMAGE : il ouvre la passe de profondeur à la
// première face, et la referme à la fin.
class FacePainter {
 public:
  FacePainter(PointLightStage& stage, const SunShadowCasters& casters, const Camera& camera)
      : stage_(stage), casters_(casters), camera_(camera) {}

  void Begin() {
    if (!begun_) {
      stage_.BeginDepthPass();
      begun_ = true;
    }
  }

  void End() const {
    if (begun_) {
      stage_.EndDepthPass();
    }
  }

  // Peint dans la face `job.face` de la carte `slot` ce qu'elle voit de la
  // lumière `light` : le sol, le décor, puis les corps.
  void Paint(PointShadowSlot slot, const PointLight& light, const FaceJob& job,
             std::span<const ShadowActor> actors, ModelOpacities& sighted) {
    Begin();
    const glm::mat4 view_projection =
        PointShadowFaceViewProjections(light.world_position, PointShadowFar(light.range_world))
            .at(job.face);
    const ShadowSight sight = SightOf(light, job.face);
    stage_.BeginFace(static_cast<std::size_t>(slot.tier), slot.layer, job.face);
    if (casters_.terrain != nullptr) {
      casters_.terrain->DrawDepth(view_projection, &sight);
    }
    if (casters_.models != nullptr) {
      static_cast<void>(casters_.models->Sighted(sight, casters_.model_casting, sighted));
      casters_.models->DrawDepth(view_projection, casters_.clock, &sighted);
    }
    // UN CORPS PLAT se dresse selon la direction de la lumière vue de ses
    // pieds, la même dans toutes les faces qui le voient.
    if (casters_.sprites != nullptr && !job.actors.empty()) {
      for (std::vector<DrawnActor>& lot : lots_) {
        lot.clear();
      }
      for (const std::size_t actor : job.actors) {
        lots_.at(actors[actor].lot).push_back(*actors[actor].drawn);
      }
      stage_.SetCasterOffset(false);
      for (const std::vector<DrawnActor>& lot : lots_) {
        if (!lot.empty()) {
          casters_.sprites->DrawDepth(view_projection, -CubeFaceForward(job.face), camera_, lot,
                                      &light.world_position);
        }
      }
      stage_.SetCasterOffset(true);
    }
    if (job.actor_models_seen && casters_.actor_model_renderer != nullptr) {
      casters_.actor_model_renderer->DrawDepth(view_projection, casters_.actor_models);
    }
  }

 private:
  PointLightStage& stage_;
  const SunShadowCasters& casters_;
  const Camera& camera_;
  bool begun_ = false;
  std::array<std::vector<DrawnActor>, kSunShadowActorLots> lots_{};
};

}  // namespace

PointLightSet DrawPointShadows(PointLightStage& stage, const SunShadowCasters* casters,
                               const PointLightSet& points, const Camera& camera,
                               float aspect_ratio, PointShadowTally* tally) {
  PointLightSet visible =
      VisiblePointLights(points, camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix());
  visible.shadows.clear();
  visible.stage = nullptr;
  PointShadowTally counted{.visible_lights = visible.lights.size()};
  if (!stage.valid()) {
    return visible;
  }
  if (casters != nullptr && !visible.lights.empty()) {
    const std::vector<std::optional<PointShadowSlots::Assignment>> assigned =
        stage.slots().Assign(visible.lights, camera.look_at);
    stage.EnsureLayers();
    visible.shadows.resize(visible.lights.size());
    const std::vector<ShadowActor> actors = ShadowActorsOf(*casters);
    const std::optional<WorldBox> actor_models_box = casters->actor_model_renderer != nullptr
                                                         ? ActorModelsBox(casters->actor_models)
                                                         : std::nullopt;
    const std::uint64_t generation = stage.NoteScene(PointShadowScene{
        .terrain = casters->terrain,
        .terrain_uploads = casters->terrain != nullptr ? casters->terrain->uploads() : 0,
        .models = casters->models,
        .model_uploads = casters->models != nullptr ? casters->models->uploads() : 0,
        .casting_given = casters->model_casting != nullptr,
        .casting = casters->model_casting != nullptr ? *casters->model_casting : ModelOpacities()});
    FacePainter painter(stage, *casters, camera);

    // LES FACES À REPEINDRE, relevées d'abord : celles dont ce qui se tient
    // devant a changé passent avant celles où seul un modèle s'anime.
    const std::uint64_t frame = stage.TakeFrame();
    std::vector<FaceJob> jobs;
    std::vector<FaceJob> animated_jobs;
    ModelOpacities sighted;
    std::vector<ActorShadowKey> seen_keys;
    for (std::size_t rank = 0; rank < visible.lights.size(); ++rank) {
      if (!assigned[rank].has_value()) {
        continue;
      }
      const PointShadowSlot slot = assigned[rank]->slot;
      visible.shadows[rank] = slot;
      ++counted.shadowed_lights;
      PointShadowCube& cube = stage.cube(static_cast<std::size_t>(slot.tier), slot.layer);
      // UNE CARTE NEUVE, OU UN DÉCOR CHANGÉ : tout est à repeindre, et ce qui
      // attend le budget ne doit rien ombrer d'ici là.
      if (assigned[rank]->fresh || cube.static_generation != generation) {
        cube = PointShadowCube{.faces = {}, .static_generation = generation};
        painter.Begin();
        for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
          stage.BeginFace(static_cast<std::size_t>(slot.tier), slot.layer, face);
        }
      }
      for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
        const ShadowSight sight = SightOf(visible.lights[rank], face);
        FaceJob job{.rank = rank, .face = face};
        seen_keys.clear();
        for (std::size_t actor = 0; actor < actors.size(); ++actor) {
          if (sight.SeesSphere(actors[actor].center, actors[actor].reach)) {
            job.actors.push_back(actor);
            seen_keys.push_back(actors[actor].key);
          }
        }
        job.actor_models_seen = actor_models_box.has_value() && sight.Sees(*actor_models_box);
        const PointShadowFace& painted = cube.faces[face];
        if (!painted.painted || painted.actors != seen_keys) {
          jobs.push_back(std::move(job));
          continue;
        }
        const bool moving = job.actor_models_seen ||
                            (casters->models != nullptr &&
                             casters->models->Sighted(sight, casters->model_casting, sighted));
        if (moving && frame - painted.painted_frame >= kPointShadowAnimatedPeriod) {
          job.animated = true;
          animated_jobs.push_back(std::move(job));
        }
      }
    }
    jobs.insert(jobs.end(), std::make_move_iterator(animated_jobs.begin()),
                std::make_move_iterator(animated_jobs.end()));

    std::size_t budget = kPointShadowFacesPerFrame;
    for (const FaceJob& job : jobs) {
      if (budget == 0) {
        counted.deferred_faces += job.animated ? 0U : 1U;
        continue;
      }
      --budget;
      ++counted.painted_faces;
      counted.moving_faces += job.animated ? 1U : 0U;
      const PointShadowSlot slot = visible.shadows[job.rank];
      PointShadowFace& painted =
          stage.cube(static_cast<std::size_t>(slot.tier), slot.layer).faces[job.face];
      painter.Paint(slot, visible.lights[job.rank], job, actors, sighted);
      painted.painted = true;
      painted.painted_frame = frame;
      painted.actors.clear();
      for (const std::size_t actor : job.actors) {
        painted.actors.push_back(actors[actor].key);
      }
    }
    painter.End();
  }
  stage.Upload(visible);
  visible.stage = &stage;
  if (tally != nullptr) {
    *tally = counted;
  }
  return visible;
}

}  // namespace ro::render
