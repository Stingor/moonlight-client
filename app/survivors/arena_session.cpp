#include "survivors/arena_session.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/terrain_mesh.h"

#include "survivors/arena_choice.h"
#include "survivors/arena_ground_follow.h"
#include "survivors/arena_ground_relief.h"
#include "survivors/arena_night_lights.h"
#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

// L'horloge de présentation, ramenée à celle des renderers.
[[nodiscard]] std::uint32_t RendererMs(std::uint64_t presentation_ms) {
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(presentation_ms, std::numeric_limits<std::uint32_t>::max()));
}

// Les orbites dont l'orbe est le chariot du héros.
constexpr std::array<std::uint16_t, 2> kSpinningCartSkills = {kCartRevolutionSkill,
                                                              kCartTornadoSkill};

}  // namespace

ArenaSession::ArenaSession(ro::vfs::Vfs& vfs, ro::render::ActorWardrobe& wardrobe,
                           const ro::sim::SurvivorsRules& rules)
    : composer_(wardrobe),
      effects_(vfs, wardrobe.catalog(), rules),
      carts_(vfs),
      falcons_(vfs),
      camera_(rules.spawn_frame),
      rules_(rules) {}

ro::core::Result<void> ArenaSession::Open(ro::vfs::Vfs& vfs, const ro::sim::SurvivorsRules& rules,
                                          const ro::sim::SurvivorsStart& start,
                                          const MapStageRead& read) {
  arena_id_.reset();
  const ro::sim::ArenaRule* arena = ro::sim::FindArena(rules, start.arena_id);
  if (arena == nullptr) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "arène " + std::to_string(start.arena_id) + " inconnue des règles");
  }
  if (auto opened = scene_.Open(vfs, rules, *arena, start.seed, read); !opened) return opened;
  const GroundWindowOrigin origin = scene_.window_origin();
  effects_.Open(scene_.stage(), glm::ivec2(origin.x_cells, origin.y_cells));
  effects_.set_weather(ArenaWeatherOf(rules, *arena));
  if (auto digits = numbers_.Open(vfs); !digits) {
    ro::core::LogWarning("arène : chiffres absents — {}", ro::core::Describe(digits.error()));
  }
  generated_ = scene_.ground_sources() != nullptr;
  // Le héros peut naître au sommet d'une colline, le bord de l'image au creux.
  camera_.BoundDescent(GroundDescentReachOf(generated_),
                       generated_ ? static_cast<float>(kGroundReliefSpanUnits) : 0.0F);
  field_track_ = ArenaFieldTrackOf(rules, *arena);
  arena_id_ = arena->arena_id;
  run_seed_ = start.seed;
  start_origin_ = origin;
  recentres_ = 0;
  most_mount_ms_ = 0.0;
  return {};
}

bool ArenaSession::Shows(const ro::sim::SurvivorsStart& start) const noexcept {
  return arena_id_.has_value() &&
         MountedArenaServes(MountedArena{.arena_id = *arena_id_,
                                         .generated = generated_,
                                         .run_seed = run_seed_,
                                         .start_origin = start_origin_,
                                         .shown_origin = scene_.window_origin()},
                            start.arena_id, start.seed);
}

void ArenaSession::Restart() {
  poses_.Reset();
  carts_.Reset();
  falcons_.Reset();
  camera_.Enter();
  effects_.Restart();
  numbers_.Restart();
  faded_.clear();
  veil_targets_.clear();
  scene_.ForgetVeil();
}

float ArenaSession::AspectOf(float width, float height) const noexcept {
  return height > 0.0F ? width / height : 1.0F;
}

void ArenaSession::Warm(std::span<const std::uint16_t> species,
                        std::span<const std::uint16_t> skill_ids, glm::vec2 hero_cell,
                        float viewport_width, float viewport_height, std::uint32_t now_ms) {
  const float aspect_ratio = AspectOf(viewport_width, viewport_height);
  const ro::render::Camera& camera =
      camera_.Pose(scene_.GroundAtCell(hero_cell), kArenaDescentMs, aspect_ratio, now_ms, nobody_);
  scene_.Warm(composer_, effects_, species, skill_ids, hero_cell, camera, aspect_ratio,
              viewport_width, viewport_height, now_ms);
}

void ArenaSession::FollowGround() {
  const std::optional<glm::vec2> hero = hero_cell();
  if (!hero.has_value()) return;
  const std::optional<GroundRecentre> moved =
      scene_.FollowHero(*hero, effects_.ShiftsWithoutTrace());
  if (!moved.has_value()) return;
  // 🔴 DANS LA MÊME IMAGE QUE L'ÉCHANGE, avant que rien ne se compose : tout
  // ce que la session tient en coordonnées du monde se déplace d'autant. Les
  // corps, la lanterne, les halos et l'image ouverte se refont depuis la
  // nouvelle fenêtre ; les poses et la réserve des chiffres comptent en cases
  // de la partie, que le recentrage ne touche pas.
  effects_.Shift(moved->world_delta, glm::ivec2(moved->to.x_cells, moved->to.y_cells));
  numbers_.Shift(moved->world_delta);
  camera_.Shift(moved->world_delta);
  ++recentres_;
  most_mount_ms_ = std::max(most_mount_ms_, moved->mount_ms);
}

std::vector<std::string> ArenaSession::TakeEffectSounds() {
  std::vector<std::string> heard = effects_.TakeSounds();
  std::ranges::move(hero_sounds_, std::back_inserter(heard));
  hero_sounds_.clear();
  return heard;
}

void ArenaSession::Compose(const ArenaFrameFacts& facts, const GraphicsOptions& graphics,
                           std::uint32_t now_ms) {
  const ro::sim::SurvivorsField& field = *facts.field;
  const std::uint32_t renderer_ms = RendererMs(facts.presentation_ms);
  FollowGround();
  const auto ground_of = [this](ro::sim::FieldPoint point) { return scene_.GroundAt(point); };
  const auto effects_started = std::chrono::steady_clock::now();
  // LE CADRE VU EST CELUI DE LA CAMÉRA POSÉE à l'image précédente : la marge
  // du cadre couvre le pas qu'elle fera à celle-ci.
  const ArenaSight sight = ArenaSightOf(
      camera_.camera(), AspectOf(facts.viewport_width, facts.viewport_height), field.hero.position);
  effects_.Advance(*facts.news, field, facts.orbs, ground_of, renderer_ms, sight);
  effects_ms_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - effects_started)
          .count();
  numbers_.Notice(*facts.news, field, facts.presentation_ms);

  frame_ = scene_.Begin(graphics, facts.presentation_ms);
  // LES FACES SUIVENT LE LACET que la caméra prendra à la fin de l'image.
  const float yaw_degrees = camera_.yaw_degrees();
  poses_.set_camera_yaw_degrees(yaw_degrees);
  carts_.set_camera_yaw_degrees(yaw_degrees);
  falcons_.set_camera_yaw_degrees(yaw_degrees);
  poses_.NoticeFalls(facts.news->falls, facts.presentation_ms);
  poses_.NoticeHits(facts.news->hits, field, facts.presentation_ms);
  poses_.NoticeCasts(facts.news->casts, field, facts.presentation_ms);
  poses_.Pose(field, facts.presentation_ms, facts.blend_per_mille, composer_.TimesSource());
  // LES CHARIOTS : le héros qui fait tourner le sien le regarde, ce qui
  // retouche sa pose.
  faded_.assign(poses_.poses().begin(), poses_.poses().end());
  const ArenaEffectSwitches switches = effects_.switches();
  carts_.Compose(
      field, facts.orbs, kSpinningCartSkills, faded_,
      [this](glm::vec2 cell) { return scene_.GroundAtCell(cell); },
      switches.shown && !switches.reduced);
  std::span<const HordePose> shown;
  // LA VICTOIRE EFFACE LA HORDE RESTANTE (Q7), le boss tombé compris : le
  // héros, le sol et ce qu'il porte restent.
  if (facts.victory_ms.has_value()) {
    const float left =
        1.0F - std::min(1.0F, static_cast<float>(*facts.victory_ms) / kVictoryFadeMs);
    for (HordePose& pose : faded_) {
      if (pose.kind == HordeBodyKind::kFoe || pose.kind == HordeBodyKind::kFalling) {
        pose.tint[3] = static_cast<std::uint8_t>(static_cast<float>(pose.tint[3]) * left);
      }
    }
  }
  if (facts.stranger.has_value()) faded_.push_back(*facts.stranger);
  shown = faded_;
  composer_.Compose(
      shown, [this](const HordePose& pose) { return scene_.GroundAtCell(pose.cell); },
      scene_.ShadowsOf(frame_));
  // LE HÉROS DU JOUEUR EST EN TÊTE des corps, quand il est posé.
  const bool hero_drawn =
      !composer_.tags().empty() && composer_.tags().front().kind == HordeBodyKind::kHero &&
      !poses_.poses().empty() && poses_.poses().front().kind == HordeBodyKind::kHero;
  if (std::optional<std::string> heard =
          hero_frame_sound_.Hear(hero_drawn ? composer_.drawn().front().actor : nullptr)) {
    hero_sounds_.push_back(std::move(*heard));
  }
  composer_.AppendCompanions(carts_.drawn());
  // LES FAUCONS planent au-dessus de leurs héros, et piquent sur leurs coups.
  falcons_.Compose(
      field, facts.news->casts, faded_,
      [this](glm::vec2 cell) { return scene_.GroundAtCell(cell); },
      [this](float x, float z) {
        const MapStage& stage = scene_.stage();
        const std::array<float, 2> cell = ro::render::FractionalCellAt(stage.scene.gnd, x, z);
        return GroundPositionAt(stage, cell[0], cell[1])[1];
      },
      renderer_ms);
  composer_.AppendCompanions(falcons_.drawn());
  ArenaVeilTargets(
      shown, rules_, [this](glm::vec2 cell) { return scene_.GroundAtCell(cell); }, veil_targets_);
  if (!poses_.poses().empty() && poses_.poses().front().kind == HordeBodyKind::kHero) {
    effects_.FollowHero(poses_.poses().front().cell);
  }
  numbers_.Compose(facts.presentation_ms, ground_of);
  const glm::vec3 hero_ground = hero_cell().has_value() ? scene_.GroundAtCell(*hero_cell())
                                                        : scene_.GroundAt(field.hero.position);
  static_cast<void>(camera_.Pose(hero_ground, facts.arrival_ms,
                                 AspectOf(facts.viewport_width, facts.viewport_height), now_ms,
                                 nobody_));
}

void ArenaSession::Paint(const ArenaFrameFacts& facts, const GraphicsOptions& graphics,
                         std::uint32_t now_ms) {
  const std::optional<glm::vec2> hero = hero_cell();
  scene_.Paint(
      frame_, camera_.camera(), AspectOf(facts.viewport_width, facts.viewport_height), graphics,
      facts.presentation_ms, now_ms, composer_,
      ArenaOverlays{
          .effects = &effects_,
          .numbers = &numbers_,
          .viewport_width = facts.viewport_width,
          .viewport_height = facts.viewport_height,
          .veil_targets = veil_targets_,
          .hero_feet = hero.has_value() ? std::optional(scene_.GroundAtCell(*hero)) : std::nullopt,
          .burst_pieces = facts.field != nullptr
                              ? std::span<const ro::sim::CellPosition>(facts.field->burst_tanks)
                              : std::span<const ro::sim::CellPosition>()});
}

std::optional<glm::vec2> ArenaSession::ScreenOf(glm::vec3 world, float viewport_width,
                                                float viewport_height) const {
  const ro::render::Camera& camera = camera_.camera();
  const glm::vec4 clip = camera.ProjectionMatrix(AspectOf(viewport_width, viewport_height)) *
                         camera.ViewMatrix() * glm::vec4(world, 1.0F);
  if (clip.w <= 0.0F) return std::nullopt;
  const glm::vec2 ndc = glm::vec2(clip.x, clip.y) / clip.w;
  // L'écran a son origine en haut à gauche, l'axe des y vers le bas.
  return glm::vec2((ndc.x + 1.0F) * 0.5F * viewport_width, (1.0F - ndc.y) * 0.5F * viewport_height);
}

std::optional<ro::sim::FieldPoint> ArenaSession::FieldPointUnder(glm::vec2 screen,
                                                                 ro::sim::FieldPoint hero,
                                                                 float viewport_width,
                                                                 float viewport_height) const {
  const glm::vec2 hero_cell =
      CellOfField(glm::vec2(static_cast<float>(hero.x), static_cast<float>(hero.y)));
  const std::optional<glm::vec2> cells = GroundCellsUnder(
      camera_.camera(), screen, viewport_width, viewport_height, scene_.GroundAtCell(hero_cell),
      scene_.GroundAtCell(hero_cell + glm::vec2(1.0F, 0.0F)),
      scene_.GroundAtCell(hero_cell + glm::vec2(0.0F, 1.0F)));
  if (!cells.has_value()) return std::nullopt;
  const auto subunits = static_cast<float>(ro::sim::kSubunitsPerCell);
  return ro::sim::FieldPoint{hero.x + static_cast<std::int32_t>(cells->x * subunits),
                             hero.y + static_cast<std::int32_t>(cells->y * subunits)};
}

std::optional<glm::vec2> ArenaSession::hero_cell() const noexcept {
  return ArenaHeroCell(poses_.poses());
}

}  // namespace moonclient
