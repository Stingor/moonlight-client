#include "survivors/arena_scene.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_night.h"
#include "survivors/arena_night_lights.h"

namespace moonclient {
namespace {

// LA CHAUFFE SE PEINT À CETTE DISTANCE DU HÉROS, en cases : hors du cadre de
// la caméra figée au nord à son plus grand recul, d'un côté comme de l'autre.
constexpr float kWarmAwayCells = 60.0F;

// L'horloge de présentation, ramenée à celle des renderers. Une partie ne dure
// pas quarante-neuf jours : la borne ne sert qu'à ne jamais boucler.
[[nodiscard]] std::uint32_t RendererMs(std::uint64_t presentation_ms) {
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(presentation_ms, std::numeric_limits<std::uint32_t>::max()));
}

}  // namespace

ro::core::Result<void> ArenaScene::Open(ro::vfs::Vfs& vfs, const ro::sim::ArenaRule& arena,
                                        const MapStageRead& read) {
  const std::string_view map_name = arena.map_name;
  if (map_name.empty()) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "arène " + std::string(arena.key) + " : aucune carte nommée");
  }
  if (auto opened = lit_.Open(vfs, map_name, read); !opened) {
    return ro::core::Fail(opened.error().code, "arène " + std::string(map_name) + " : " +
                                                   ro::core::Describe(opened.error()));
  }
  lit_.set_sky_hour_imposed(true);
  follow_.reset();
  ground_.reset();
  origin_ = GroundWindowOrigin();
  ro::core::LogInfo("arène : {} montée", lit_.stage().name);
  return {};
}

ro::core::Result<void> ArenaScene::Open(ro::vfs::Vfs& vfs, const ro::sim::SurvivorsRules& rules,
                                        const ro::sim::ArenaRule& arena, std::uint64_t run_seed,
                                        const MapStageRead& read) {
  if (arena.terrain_id == 0) return Open(vfs, arena, read);
  const ro::sim::TerrainRule* rule = ro::sim::FindTerrain(rules, arena.terrain_id);
  const BiomeLook* look = rule != nullptr ? FindBiomeLook(rule->key) : nullptr;
  if (look == nullptr) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "arène " + std::string(arena.key) + " : terrain sans aspect");
  }
  const GroundWindowOrigin origin =
      GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
  // 🔴 LE SUIVI S'ARRÊTE AVANT QUE LA GRAINE NE CHANGE : ses fils lisent les
  // ressources, et une fenêtre bâtie sous l'ancienne graine ne doit pas se
  // monter.
  if (ground_ != nullptr && ground_->rule == rule && ground_->look == look) {
    follow_.reset();
    ground_->run_seed = run_seed;
    return MountGround(vfs, origin, std::string(arena.key), read, GroundBuildThread::kWorker);
  }
  auto sources = OpenGroundSources(vfs, *rule, run_seed, *look, read.progress);
  if (!sources) {
    return ro::core::Fail(sources.error().code, "arène " + std::string(arena.key) + " : " +
                                                    ro::core::Describe(sources.error()));
  }
  return OpenGround(vfs, std::move(*sources), origin, std::string(arena.key), read);
}

ro::core::Result<void> ArenaScene::OpenGround(ro::vfs::Vfs& vfs, GroundSources sources,
                                              GroundWindowOrigin origin, std::string name,
                                              const MapStageRead& read, GroundBuildThread thread) {
  follow_.reset();
  ground_ = std::make_unique<GroundSources>(std::move(sources));
  return MountGround(vfs, origin, std::move(name), read, thread);
}

ro::core::Result<void> ArenaScene::MountGround(ro::vfs::Vfs& vfs, GroundWindowOrigin origin,
                                               std::string name, const MapStageRead& read,
                                               GroundBuildThread thread) {
  vfs_ = &vfs;
  origin_ = origin;
  ground_subdivisions_ = read.rounded_ground ? ro::render::kRoundedGroundSubdivisions
                                             : ro::render::kFlatGroundSubdivisions;
  auto window = BuildGroundWindow(vfs, *ground_, origin_, name, ground_subdivisions_);
  if (!window) return std::unexpected(window.error());
  if (auto opened = lit_.Open(vfs, std::move(*window), ground_->look->reference_map, read.progress);
      !opened) {
    return std::unexpected(opened.error());
  }
  lit_.set_sky_hour_imposed(true);
  // 🔴 UN FIL DE TRAVAIL NE LIT AUCUN FICHIER : une fenêtre ne cite que ce que
  // la bibliothèque a lu (`OpenGroundSources`). Une texture du sol absente de
  // l'installation se relirait à chaque fenêtre, sur le VFS que le fil
  // principal emploie : les fenêtres se bâtissent alors sur le fil principal.
  const GroundBuildThread builds_on =
      ground_->library.missing_terrain_textures == 0 ? thread : GroundBuildThread::kCaller;
  follow_ = std::make_unique<GroundFollow<MapStage>>(
      [this, name](GroundWindowOrigin next) {
        return BuildGroundWindow(*vfs_, *ground_, next, name, ground_subdivisions_);
      },
      builds_on);
  ro::core::LogInfo("arène : {} générée, fenêtre en ({}, {})", name, origin_.x_cells,
                    origin_.y_cells);
  return {};
}

std::optional<GroundRecentre> ArenaScene::FollowHero(glm::vec2 hero_cell,
                                                     bool shifts_without_trace) {
  if (follow_ == nullptr) return std::nullopt;
  const bool forced = GroundRecentreForced(origin_, hero_cell);
  // UNE PRÉPARATION QUI NE SERT PLUS s'abandonne d'abord : la fenêtre qu'il faut
  // se demande dans la même image. L'abandon ne dépend pas du téléversement.
  if (lit_.preparing() &&
      GroundPreparedFateOf(origin_, prepared_origin_, hero_cell,
                           /*uploaded=*/false, shifts_without_trace) == GroundPreparedFate::kDrop) {
    if (std::optional<MapStage> dropped = lit_.DropPrepared()) {
      follow_->Retire(std::move(*dropped));
    }
  }
  if (!lit_.preparing()) {
    if (std::optional<GroundFollow<MapStage>::Built> built =
            follow_->Next(origin_, hero_cell, forced)) {
      if (auto prepared = lit_.Prepare(std::move(built->window)); !prepared) {
        ro::core::LogWarning("arène : fenêtre ({}, {}) non préparée — {}", built->origin.x_cells,
                             built->origin.y_cells, ro::core::Describe(prepared.error()));
        return std::nullopt;
      }
      prepared_origin_ = built->origin;
      // Les textures et les tampons réservés font l'image de la préparation : la
      // première tranche attend la suivante, sauf au bord.
      if (!forced) return std::nullopt;
    }
  }
  if (!lit_.preparing()) return std::nullopt;
  // UNE TRANCHE DU SOL PAR IMAGE, tout d'un coup quand le bord approche.
  const bool uploaded =
      lit_.PrepareStep(forced ? std::numeric_limits<std::size_t>::max() : kGroundUploadSliceBytes);
  switch (
      GroundPreparedFateOf(origin_, prepared_origin_, hero_cell, uploaded, shifts_without_trace)) {
    case GroundPreparedFate::kKeep:
      return std::nullopt;
    case GroundPreparedFate::kDrop:
      if (std::optional<MapStage> dropped = lit_.DropPrepared()) {
        follow_->Retire(std::move(*dropped));
      }
      return std::nullopt;
    case GroundPreparedFate::kMount:
      break;
  }
  auto mounted = MountPrepared();
  if (!mounted) {
    ro::core::LogWarning("arène : fenêtre ({}, {}) non montée — {}", prepared_origin_.x_cells,
                         prepared_origin_.y_cells, ro::core::Describe(mounted.error()));
    return std::nullopt;
  }
  return *mounted;
}

void ArenaScene::ReleaseSpareGround() {
  if (std::optional<MapStage> dropped = lit_.ReleaseSpares(); dropped && follow_ != nullptr) {
    follow_->Retire(std::move(*dropped));
  }
}

ro::core::Result<GroundRecentre> ArenaScene::Recentre(GroundWindowOrigin origin) {
  if (ground_ == nullptr) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "arène : une carte ne se recentre pas");
  }
  auto window = BuildGroundWindow(*vfs_, *ground_, origin, std::string(lit_.map_name()),
                                  ground_subdivisions_);
  if (!window) return std::unexpected(window.error());
  if (std::optional<MapStage> dropped = lit_.DropPrepared(); dropped && follow_ != nullptr) {
    follow_->Retire(std::move(*dropped));
  }
  if (auto prepared = lit_.Prepare(std::move(*window)); !prepared) {
    return std::unexpected(prepared.error());
  }
  prepared_origin_ = origin;
  return MountPrepared();
}

ro::core::Result<GroundRecentre> ArenaScene::MountPrepared() {
  const auto started = std::chrono::steady_clock::now();
  auto outgoing = lit_.MountPrepared();
  const double mount_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  if (!outgoing) return std::unexpected(outgoing.error());
  if (follow_ != nullptr) follow_->Retire(std::move(*outgoing));
  const GroundRecentre recentre{.from = origin_,
                                .to = prepared_origin_,
                                .world_delta = GroundWorldShift(origin_, prepared_origin_),
                                .mount_ms = mount_ms};
  origin_ = prepared_origin_;
  return recentre;
}

LitFrame ArenaScene::Begin(const GraphicsOptions& graphics, std::uint64_t presentation_ms) {
  ground_subdivisions_ = graphics.rounded_ground() ? ro::render::kRoundedGroundSubdivisions
                                                   : ro::render::kFlatGroundSubdivisions;
  return lit_.Begin(graphics, graphics.fog(), RendererMs(presentation_ms));
}

void ArenaScene::Paint(LitFrame& frame, const ro::render::Camera& camera, float aspect_ratio,
                       const GraphicsOptions& graphics, std::uint64_t presentation_ms,
                       std::uint32_t now_ms, const HordeComposer& horde,
                       const ArenaOverlays& overlays) {
  // LA NUIT RESTE LISIBLE : la lanterne suit le héros, et chaque objet au sol
  // luit.
  carried_lights_.clear();
  if (const std::optional<glm::vec3> hero_feet = ArenaHeroFeet(horde.drawn(), horde.tags())) {
    if (const std::optional<ro::render::PointLight> lantern =
            ArenaLantern(presentation_ms, *hero_feet)) {
      carried_lights_.push_back(*lantern);
    }
  }
  ArenaItemHalos(horde.drawn(), horde.tags(), presentation_ms, item_halos_);
  const LitActors actors{.bodies = horde.drawn(),
                         .halos = item_halos_,
                         .sun_casters = horde.sun_casters(),
                         .carried_lights = carried_lights_};
  painting_effects_ = overlays.effects;
  lit_.Paint(frame, camera, aspect_ratio, graphics, ArenaSkyHour(presentation_ms),
             RendererMs(presentation_ms), now_ms, actors, *this);
  painting_effects_ = nullptr;
  if (overlays.effects != nullptr) {
    overlays.effects->DrawAfterBodies(lit_.sprites(), camera, aspect_ratio, overlays.viewport_width,
                                      overlays.viewport_height, frame.fog,
                                      RendererMs(presentation_ms));
  }
  if (overlays.numbers != nullptr) overlays.numbers->Draw(lit_.sprites(), camera, aspect_ratio);
}

void ArenaScene::Warm(HordeComposer& horde, ArenaEffects& effects,
                      std::span<const std::uint16_t> species,
                      std::span<const std::uint16_t> skill_ids, glm::vec2 hero_cell,
                      const ro::render::Camera& camera, float aspect_ratio, float viewport_width,
                      float viewport_height, std::uint32_t now_ms) {
  // HORS CHAMP : au nord du héros, au-delà du bord haut de l'image, ou au sud,
  // derrière la caméra, quand le nord sort de la carte.
  const float hero_north = hero_cell.y - static_cast<float>(origin_.y_cells);
  const bool north_fits =
      !lit_.stage().gat.has_value() ||
      hero_north + kWarmAwayCells < static_cast<float>(lit_.stage().gat->height());
  const glm::vec2 far_cell =
      hero_cell + glm::vec2(0.0F, north_fits ? kWarmAwayCells : -kWarmAwayCells);
  std::vector<HordePose> poses;
  poses.reserve(species.size());
  for (std::size_t index = 0; index < species.size(); ++index) {
    poses.push_back(HordePose{.kind = HordeBodyKind::kFoe,
                              .sim_id = static_cast<std::uint32_t>(index),
                              .look_id = species[index],
                              .motion = ro::render::Motion::kWalk,
                              .cell = far_cell});
  }
  horde.Compose(
      poses, [this](const HordePose& pose) { return GroundAtCell(pose.cell); }, HordeShadows{});
  if (!horde.drawn().empty()) lit_.sprites().Draw(camera, horde.drawn(), aspect_ratio);
  effects.Warm(skill_ids, GroundAtCell(far_cell), lit_.sprites(), camera, aspect_ratio,
               viewport_width, viewport_height, now_ms);
}

glm::vec3 ArenaScene::GroundAt(ro::sim::FieldPoint point) const {
  return GroundAtCell(
      CellOfField(glm::vec2(static_cast<float>(point.x), static_cast<float>(point.y))));
}

glm::vec3 ArenaScene::GroundAtCell(glm::vec2 cell) const {
  const std::array<float, 3> ground =
      GroundPositionAt(lit_.stage(), cell.x - static_cast<float>(origin_.x_cells),
                       cell.y - static_cast<float>(origin_.y_cells));
  return glm::vec3(ground[0], ground[1], ground[2]);
}

void ArenaScene::DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                                  const LitFrame& frame) {
  if (painting_effects_ != nullptr) {
    painting_effects_->DrawBehindBodies(camera, aspect_ratio, frame.fog, lit_.sprites());
  }
}

void ArenaScene::DrawOverWater(const ro::render::Camera& /*camera*/, float /*aspect_ratio*/,
                               const LitFrame& /*frame*/) {}

}  // namespace moonclient
