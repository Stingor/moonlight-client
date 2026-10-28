#include "survivors/arena_effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/effect_table.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/survivors_geometry.h"

#include "effects/effect_actor_change.h"
#include "effects/effect_paint.h"
#include "survivors/horde_poses.h"
#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

// LE HÉROS DU MONDE PRIVÉ, puis ses orbes, une par acteur à partir de la
// suivante.
constexpr std::uint32_t kArenaHeroActorId = 1;
constexpr std::uint32_t kFirstOrbActorId = kArenaHeroActorId + 1;

[[nodiscard]] std::uint32_t OrbActorId(std::size_t orb_index) {
  return kFirstOrbActorId + static_cast<std::uint32_t>(orb_index);
}

// L'orientation qu'un effet de l'arène reçoit de son porteur : celle qu'il a
// reçue en naissant, que le suivi ne change pas.
constexpr float kArenaBearerFacingDegrees = 0.0F;

// Une case et la sous-case où l'on se tient dans elle, d'une coordonnée en
// cases continues dont l'entier est le centre.
struct CellAndSub {
  std::uint16_t cell = 0;
  std::uint8_t sub = ro::sim::kCellCenterSubCell;
};

[[nodiscard]] CellAndSub CellAndSubOf(float cells) {
  const float whole = std::max(std::round(cells), 0.0F);
  const float sub =
      std::round(ro::sim::kSubCellCenter + (cells - whole) * ro::sim::kSubCellsPerCell);
  return CellAndSub{
      .cell = static_cast<std::uint16_t>(whole),
      .sub = static_cast<std::uint8_t>(std::clamp(sub, 0.0F, ro::sim::kSubCellsPerCell - 1.0F))};
}

// La vie d'un effet dont rien ne dit la durée, pour le compte des vivants.
constexpr std::uint32_t kUnknownEffectLifeMs = 1000;

// LES MARQUES AU SOL : les annonces en rouge, plus franches à mesure que le
// coup approche ; les aires selon leur sorte.
constexpr std::uint8_t kWarningAlphaFirst = 60;
constexpr std::uint8_t kWarningAlphaLast = 170;
constexpr std::array<std::uint8_t, 3> kWarningRgb = {255, 32, 32};
constexpr std::array<std::uint8_t, 4> kPuddleColor = {120, 40, 160, 110};
constexpr std::array<std::uint8_t, 4> kQuakeColor = {150, 100, 40, 110};
// Une annonce sans rayon couvre au moins sa case.
constexpr std::int32_t kLeastMarkRadius = ro::sim::kSubunitsPerCell / 2;

// Le pas d'un mur, d'une flamme à la suivante : une case.
constexpr std::int32_t kWallFlameSpacing = ro::sim::kSubunitsPerCell;

[[nodiscard]] std::int64_t Squared(std::int64_t value) { return value * value; }

// Si le point `p` est à moins de `radius` du segment [`from`, `to`].
[[nodiscard]] bool Covers(ro::sim::FieldPoint from, ro::sim::FieldPoint to, std::int32_t radius,
                          ro::sim::FieldPoint p) {
  const std::int64_t dx = std::int64_t{to.x} - from.x;
  const std::int64_t dy = std::int64_t{to.y} - from.y;
  const std::int64_t px = std::int64_t{p.x} - from.x;
  const std::int64_t py = std::int64_t{p.y} - from.y;
  const std::int64_t length_squared = Squared(dx) + Squared(dy);
  std::int64_t nearest_x = 0;
  std::int64_t nearest_y = 0;
  if (length_squared > 0) {
    const std::int64_t along = std::clamp<std::int64_t>(px * dx + py * dy, 0, length_squared);
    nearest_x = dx * along / length_squared;
    nearest_y = dy * along / length_squared;
  }
  return Squared(px - nearest_x) + Squared(py - nearest_y) <= Squared(radius);
}

// Les cases dont le centre est couvert, posées sur le relief et teintes. La
// carte montée commence à la case `origin` de la partie.
void LayCells(const MapStage& stage, glm::ivec2 origin, ro::sim::FieldPoint from,
              ro::sim::FieldPoint to, std::int32_t radius, const std::array<std::uint8_t, 4>& color,
              std::vector<ro::render::GroundQuad>& quads) {
  if (!stage.gat.has_value()) return;
  const std::int32_t reach = std::max(radius, kLeastMarkRadius);
  const std::int32_t first_x = ro::sim::CellOf(std::min(from.x, to.x) - reach);
  const std::int32_t last_x = ro::sim::CellOf(std::max(from.x, to.x) + reach);
  const std::int32_t first_y = ro::sim::CellOf(std::min(from.y, to.y) - reach);
  const std::int32_t last_y = ro::sim::CellOf(std::max(from.y, to.y) + reach);
  for (std::int32_t cell_y = first_y; cell_y <= last_y; ++cell_y) {
    for (std::int32_t cell_x = first_x; cell_x <= last_x; ++cell_x) {
      if (!Covers(from, to, reach, ro::sim::CellCenter(cell_x, cell_y))) continue;
      if (const auto quad = ro::render::ComposeGroundCellQuad(
              stage.scene.gnd, *stage.gat, cell_x - origin.x, cell_y - origin.y, color)) {
        quads.push_back(*quad);
      }
    }
  }
}

[[nodiscard]] ro::sim::FieldPoint Along(ro::sim::FieldPoint from, ro::sim::FieldPoint velocity,
                                        std::uint32_t steps) {
  const auto clamped = [](std::int64_t value) {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
  };
  return {clamped(std::int64_t{from.x} + std::int64_t{velocity.x} * steps),
          clamped(std::int64_t{from.y} + std::int64_t{velocity.y} * steps)};
}

// Les flammes d'une zone : une par case le long d'un mur, une seule pour un
// pilier.
void ZonePoints(const ro::sim::SurvivorsZone& zone, std::vector<ro::sim::FieldPoint>& out) {
  out.clear();
  const std::int64_t dx = std::int64_t{zone.to.x} - zone.from.x;
  const std::int64_t dy = std::int64_t{zone.to.y} - zone.from.y;
  const std::int64_t length = ro::sim::IntegerSqrt(static_cast<std::uint64_t>(dx * dx + dy * dy));
  const std::int64_t gaps = length / kWallFlameSpacing;
  for (std::int64_t index = 0; index <= gaps; ++index) {
    if (gaps == 0) {
      out.push_back(zone.from);
      break;
    }
    out.push_back({static_cast<std::int32_t>(zone.from.x + dx * index / gaps),
                   static_cast<std::int32_t>(zone.from.y + dy * index / gaps)});
  }
}

}  // namespace

ArenaEffects::ArenaEffects(ro::vfs::Vfs& vfs, const ro::render::SpriteCatalog& sprites,
                           const ro::sim::SurvivorsRules& rules)
    : vfs_(&vfs),
      sprites_(&sprites),
      looks_(rules),
      catalog_(vfs),
      nodes_(vfs),
      effects_(catalog_, nodes_, vfs) {
  // LES INTERRUPTEURS DE LA PARTIE NE VALENT PAS ICI : l'arène a les siens.
  effects_.SetEffectSwitches(true, false);
  nodes_.SetEffectSwitches(true, false);
  if (auto renderer = ro::render::EffectRenderer::Create(); renderer) {
    renderer_ = std::move(*renderer);
  } else {
    ro::core::LogWarning("arène : les effets ne se peignent pas — {}",
                         ro::core::Describe(renderer.error()));
  }
  if (auto ground = ro::render::GroundQuadRenderer::Create(); ground) {
    ground_ = std::move(*ground);
  }
  white_ = ro::formats::Image{.width = 1, .height = 1, .pixels = {255, 255, 255, 255}};
  if (auto marks = ro::render::GroundQuadRenderer::Create(); marks) {
    if (marks->UseTexture(white_)) marks_ = std::move(*marks);
  }
  ro::sim::Actor hero;
  hero.id = kArenaHeroActorId;
  hero.kind = ro::sim::ActorKind::kPlayer;
  world_.Spawn(hero);
}

void ArenaEffects::Open(const MapStage& stage, glm::ivec2 origin_cells) {
  stage_ = &stage;
  origin_cells_ = origin_cells;
  planner_.emplace(PlannedMapOf(stage));
  Restart();
}

void ArenaEffects::Shift(glm::vec3 world_delta, glm::ivec2 origin_cells) {
  // CE QUI S'AJOUTE À CHAQUE CASE du monde privé : elle se compte depuis un
  // coin plus loin d'autant.
  const glm::ivec2 cell_delta = origin_cells_ - origin_cells;
  origin_cells_ = origin_cells;
  effects_.Shift(world_delta, cell_delta);
  nodes_.Shift(world_delta);
  // LES ACTEURS DU MONDE PRIVÉ — héros, orbes, cartes — à leur case nouvelle ;
  // `Advance` les repose ensuite à chaque image.
  std::vector<ro::sim::Actor> moved;
  for (const auto& entry : world_.actors()) {
    const ro::sim::Actor& actor = entry.second;
    ro::sim::Actor shifted = actor;
    shifted.position.x =
        static_cast<std::uint16_t>(static_cast<std::int32_t>(actor.position.x) + cell_delta.x);
    shifted.position.y =
        static_cast<std::uint16_t>(static_cast<std::int32_t>(actor.position.y) + cell_delta.y);
    moved.push_back(std::move(shifted));
  }
  for (ro::sim::Actor& actor : moved) world_.Spawn(std::move(actor));
  // La carte a changé sous la même adresse : le chemin et la dispersion se
  // relisent.
  if (stage_ != nullptr) {
    planner_.emplace(PlannedMapOf(*stage_));
    nodes_.NoticeWorld(EffectScatterWorldOf(*stage_));
  }
  for (ro::render::GroundQuad& quad : mark_quads_) {
    for (glm::vec3& corner : quad.corners) corner += world_delta;
  }
}

void ArenaEffects::Restart() {
  effects_.Forget();
  nodes_.Forget();
  if (stage_ != nullptr) nodes_.NoticeWorld(EffectScatterWorldOf(*stage_));
  flames_.clear();
  shots_.clear();
  warnings_.clear();
  mark_quads_.clear();
  lives_.Clear();
  ForgetOrbs();
  ForgetRareGlows();
  ForgetZoneGrounds();
  budget_ = ArenaEffectBudget();
  launched_total_ = 0;
  step_ = 0;
  // Ni la chauffe ni la partie d'avant ne font entendre leurs bruits.
  heard_.clear();
}

void ArenaEffects::PlaceActor(std::uint32_t actor_id, glm::vec2 cell) {
  const CellAndSub x = CellAndSubOf(cell.x - static_cast<float>(origin_cells_.x));
  const CellAndSub y = CellAndSubOf(cell.y - static_cast<float>(origin_cells_.y));
  // UNE RÉANNONCE, comme le serveur en fait à chaque pas : l'acteur garde les
  // effets qu'il porte, et se tient à la sous-case près.
  ro::sim::Actor actor;
  actor.id = actor_id;
  actor.kind = ro::sim::ActorKind::kPlayer;
  actor.position.x = x.cell;
  actor.position.y = y.cell;
  actor.sub_x = x.sub;
  actor.sub_y = y.sub;
  world_.Spawn(std::move(actor));
}

void ArenaEffects::FollowHero(glm::vec2 hero_cell) { PlaceActor(kArenaHeroActorId, hero_cell); }

void ArenaEffects::set_switches(ArenaEffectSwitches switches) {
  switches_ = switches;
  // CE QUI DURE S'ÉTEINT À LA BASCULE MÊME, partie figée comprise : orbes et
  // flammes ne sont plus de ce que l'arène montre.
  if (!switches.shown || switches.reduced) {
    ForgetOrbs();
    ForgetZoneGrounds();
    flames_.clear();
  }
  if (!switches.shown) ForgetRareGlows();
}

void ArenaEffects::ForgetZoneGrounds() {
  for (const ArenaZoneGround& ground : zone_grounds_) {
    effects_.StopById(ground.effect_id, ground.actor_id);
    static_cast<void>(world_.Remove(ground.actor_id));
  }
  zone_grounds_.clear();
}

void ArenaEffects::KeepZoneGrounds(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                                   std::uint32_t now_ms) {
  PlanArenaZoneGrounds(field.zones, looks_, switches_, zone_ground_plan_);
  // UNE ZONE ÉTEINTE EMPORTE SON EFFET AU SOL, et l'acteur qui le portait.
  std::erase_if(zone_grounds_, [this](const ArenaZoneGround& lit) {
    if (std::ranges::any_of(zone_ground_plan_, [&lit](const ArenaZoneGround& planned) {
          return planned.actor_id == lit.actor_id;
        })) {
      return false;
    }
    effects_.StopById(lit.effect_id, lit.actor_id);
    static_cast<void>(world_.Remove(lit.actor_id));
    return true;
  });
  for (const ArenaZoneGround& planned : zone_ground_plan_) {
    const auto lit = std::ranges::find(zone_grounds_, planned.actor_id, &ArenaZoneGround::actor_id);
    const bool first = lit == zone_grounds_.end();
    // UN VISUEL PERMANENT NE SE RELANCE PAS : il ne se resème que quand tout
    // ce qu'il avait semé est mort de sa propre vie, comme sous une unité.
    if (!first && effects_.Carries(planned.effect_id, planned.actor_id)) continue;
    if (first) {
      PlaceActor(planned.actor_id, CellOfField(glm::vec2(static_cast<float>(planned.at.x),
                                                         static_cast<float>(planned.at.y))));
      zone_grounds_.push_back(planned);
    }
    // Ni aura ni vie imposée : le patron du visuel décide ; seul le premier
    // semis s'annonce.
    constexpr bool kNotEndless = false;
    constexpr std::int32_t kPatternLife = 0;
    static_cast<void>(effects_.PlayById(planned.effect_id, ground_of(planned.at), now_ms,
                                        planned.actor_id, kNotEndless, kPatternLife, first));
    ++launched_total_;
  }
}

void ArenaEffects::ForgetRareGlows() {
  for (const std::uint32_t actor_id : glow_actors_) static_cast<void>(world_.Remove(actor_id));
  glow_actors_.clear();
}

void ArenaEffects::KeepRareGlows(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                                 std::uint32_t now_ms) {
  PlanArenaRareGlows(field.pickups, switches_, glow_plan_);
  // UN BUTIN PRIS OU SORTI DU PLAN éteint sa lueur avec son acteur.
  std::erase_if(glow_actors_, [this](std::uint32_t actor_id) {
    if (std::ranges::any_of(glow_plan_, [actor_id](const ArenaRareGlow& glow) {
          return glow.actor_id == actor_id;
        })) {
      return false;
    }
    static_cast<void>(world_.Remove(actor_id));
    return true;
  });
  const ro::render::ItemDropPillar pillar = ro::render::ItemDropPillarOf(kRareGlowPillar);
  const std::string folder(ro::render::kEffectFolder);
  for (const ArenaRareGlow& glow : glow_plan_) {
    PlaceActor(glow.actor_id, CellOfField(glm::vec2(static_cast<float>(glow.at.x),
                                                    static_cast<float>(glow.at.y))));
    if (std::ranges::find(glow_actors_, glow.actor_id) != glow_actors_.end()) continue;
    glow_actors_.push_back(glow.actor_id);
    // La colonne et sa flaque, posées au sol sans relèvement et derrière les
    // corps, comme la colonne d'un butin rare : elle montre la carte ou le
    // coffre sans le noyer. Un fichier absent ne se redemande pas.
    constexpr float kOnTheGround = 0.0F;
    constexpr bool kBehindActors = true;
    for (const std::string_view file : {pillar.top, pillar.bottom}) {
      static_cast<void>(effects_.PlayLooping(folder + std::string(file), ground_of(glow.at), now_ms,
                                             kOnTheGround, glow.actor_id, kBehindActors));
    }
    ++launched_total_;
  }
}

void ArenaEffects::ForgetOrbs() {
  for (std::size_t index = 0; index < orb_effects_.size(); ++index) {
    effects_.StopById(orb_effects_[index], OrbActorId(index));
    static_cast<void>(world_.Remove(OrbActorId(index)));
  }
  orb_effects_.clear();
}

void ArenaEffects::KeepOrbs(std::span<const ro::sim::SurvivorsOrb> orbs, const GroundOf& ground_of,
                            std::uint32_t now_ms) {
  PlanArenaOrbs(orbs, looks_, switches_, orbs_);
  // LES ORBES DE TROP S'ÉTEIGNENT, leur acteur avec elles : une orbite finie,
  // ou « Effets réduits » coché en pleine partie.
  for (std::size_t index = orbs_.size(); index < orb_effects_.size(); ++index) {
    effects_.StopById(orb_effects_[index], OrbActorId(index));
    static_cast<void>(world_.Remove(OrbActorId(index)));
  }
  orb_effects_.resize(std::min(orb_effects_.size(), orbs_.size()));
  for (std::size_t index = 0; index < orbs_.size(); ++index) {
    const ArenaOrbLook& orb = orbs_[index];
    const std::uint32_t actor_id = OrbActorId(index);
    PlaceActor(actor_id,
               CellOfField(glm::vec2(static_cast<float>(orb.at.x), static_cast<float>(orb.at.y))));
    // L'ORBE SE DÉPLACE AVEC SON ACTEUR : sans suivi, ses nœuds déjà semés
    // resteraient là où elle a été allumée, et l'orbite ne tournerait plus.
    nodes_.FollowActor(actor_id, ground_of(orb.at), kArenaBearerFacingDegrees);
    if (index < orb_effects_.size()) {
      if (orb_effects_[index] == orb.effect_id) continue;
      effects_.StopById(orb_effects_[index], actor_id);
      orb_effects_[index] = orb.effect_id;
    } else {
      orb_effects_.push_back(orb.effect_id);
    }
    static_cast<void>(effects_.PlayById(orb.effect_id, ground_of(orb.at), now_ms, actor_id, true));
    ++launched_total_;
  }
}

std::uint32_t ArenaEffects::LifeMsOf(std::uint32_t effect_id) {
  if (const std::string file = ro::render::EffectFile(effect_id); !file.empty()) {
    if (const ro::render::LoadedEffect* loaded =
            catalog_.Load(std::string(ro::render::kEffectFolder) + file)) {
      return static_cast<std::uint32_t>(loaded->prepared.last_frame + 1) *
             static_cast<std::uint32_t>(ro::render::kEffectTickMs);
    }
  }
  if (const std::int32_t ticks = EffectNodesInPlay::ProceduralLifeTicks(effect_id); ticks > 0) {
    return static_cast<std::uint32_t>(ticks) *
           static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  }
  return kUnknownEffectLifeMs;
}

void ArenaEffects::Launch(const ArenaLaunch& launch, const GroundOf& ground_of,
                          std::uint32_t now_ms) {
  const bool on_hero = launch.kind == ArenaLaunchKind::kOnHero;
  const std::optional<glm::vec3> target = launch.target.has_value()
                                              ? std::optional<glm::vec3>(ground_of(*launch.target))
                                              : std::nullopt;
  static_cast<void>(effects_.PlayById(launch.effect_id, ground_of(launch.at), now_ms,
                                      on_hero ? kArenaHeroActorId : 0, false, 0, true, 0, 0.0F,
                                      0.0F, target));
  lives_.Add(now_ms + LifeMsOf(launch.effect_id));
  ++launched_total_;
}

void ArenaEffects::Advance(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                           std::span<const ro::sim::SurvivorsOrb> orbs, const GroundOf& ground_of,
                           std::uint32_t now_ms) {
  step_ = field.step;
  PlaceActor(kArenaHeroActorId, CellOfField(glm::vec2(static_cast<float>(field.hero.position.x),
                                                      static_cast<float>(field.hero.position.y))));
  // Ce que porte le héros le suit, selon la table de suivi de chaque effet.
  nodes_.FollowActor(kArenaHeroActorId, ground_of(field.hero.position), kArenaBearerFacingDegrees);
  lives_.Expire(now_ms);
  KeepOrbs(orbs, ground_of, now_ms);
  KeepRareGlows(field, ground_of, now_ms);
  KeepZoneGrounds(field, ground_of, now_ms);
  budget_.BeginFrame(living());
  PlanArenaLaunches(news, field, looks_, switches_, budget_, plan_);
  for (const ArenaLaunch& launch : plan_) Launch(launch, ground_of, now_ms);
  for (const ro::sim::SurvivorsWarning& warning : news.warnings) {
    warnings_.push_back(WarningMark{.warning = warning, .announced_step = field.step});
  }
  std::erase_if(warnings_, [&field](const WarningMark& mark) {
    return field.step >= mark.warning.strike_step;
  });
  KeepZones(field, ground_of, now_ms);
  ShootFoeShots(field, ground_of);
  LayGroundMarks(field);
}

void ArenaEffects::KeepZones(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                             std::uint32_t now_ms) {
  std::erase_if(flames_, [&field](const ZoneFlame& flame) {
    return std::ranges::none_of(field.zones, [&flame](const ro::sim::SurvivorsZone& zone) {
      return zone.serial == flame.zone_serial;
    });
  });
  if (!switches_.shown || switches_.reduced) {
    flames_.clear();
    return;
  }
  std::vector<ro::sim::FieldPoint> points;
  for (const ro::sim::SurvivorsZone& zone : field.zones) {
    const SkillLook* look = looks_.Find(zone.skill_id);
    if (look == nullptr) continue;
    ZonePoints(zone, points);
    for (const ro::sim::FieldPoint point : points) {
      const bool known = std::ranges::any_of(flames_, [&zone, point](const ZoneFlame& flame) {
        return flame.zone_serial == zone.serial && flame.at == point;
      });
      if (!known) {
        flames_.push_back(ZoneFlame{.zone_serial = zone.serial,
                                    .at = point,
                                    .effect_id = look->effect_id,
                                    .relaunch_ms = now_ms});
      }
    }
  }
  // UNE FLAMME ÉTEINTE SE RALLUME tant que sa zone dure ; sans place au
  // budget, elle réessaie à l'image suivante.
  for (ZoneFlame& flame : flames_) {
    if (now_ms < flame.relaunch_ms || !budget_.Admit()) continue;
    Launch(
        ArenaLaunch{
            .effect_id = flame.effect_id, .kind = ArenaLaunchKind::kOnGround, .at = flame.at},
        ground_of, now_ms);
    flame.relaunch_ms = now_ms + LifeMsOf(flame.effect_id);
  }
}

void ArenaEffects::ShootFoeShots(const ro::sim::SurvivorsField& field, const GroundOf& ground_of) {
  std::erase_if(shots_, [&field](const ShotSeen& seen) {
    return std::ranges::none_of(field.foe_shots, [&seen](const ro::sim::SurvivorsFoeShot& shot) {
      return shot.from_serial == seen.from_serial && shot.expires_step == seen.expires_step;
    });
  });
  for (const ro::sim::SurvivorsFoeShot& shot : field.foe_shots) {
    const bool seen = std::ranges::any_of(shots_, [&shot](const ShotSeen& known) {
      return known.from_serial == shot.from_serial && known.expires_step == shot.expires_step;
    });
    if (seen) continue;
    shots_.push_back(ShotSeen{.from_serial = shot.from_serial, .expires_step = shot.expires_step});
    if (!switches_.shown || switches_.reduced) continue;
    // LA FLÈCHE DU TIREUR, selon son espèce : un tireur déjà mort n'en a plus.
    const auto shooter =
        std::ranges::find(field.foes, shot.from_serial, &ro::sim::SurvivorsFoe::serial);
    if (shooter == field.foes.end()) continue;
    const ro::render::ActorLook look = FoeLook(shooter->foe_id);
    const std::string_view sprite = ro::render::ArrowSpriteOf(*sprites_, look, false);
    if (sprite.empty() || !budget_.Admit()) continue;
    const std::uint32_t steps = shot.expires_step > field.step ? shot.expires_step - field.step : 0;
    const std::uint32_t flight_ms = steps * ro::sim::kSurvivorsStepMs;
    const glm::vec3 lift{0.0F, ro::render::kArrowFlightHeight, 0.0F};
    const auto life_ticks = static_cast<std::int32_t>(std::max<std::uint32_t>(
        flight_ms / static_cast<std::uint32_t>(ro::render::kEffectTickMs), 1));
    static_cast<void>(
        nodes_.ShootSprite(sprite, ground_of(shot.position) + lift,
                           ground_of(Along(shot.position, shot.velocity, steps)) + lift, life_ticks,
                           ro::render::ProjectileFlightOf(*sprites_, look)));
    ++launched_total_;
  }
}

void ArenaEffects::LayGroundMarks(const ro::sim::SurvivorsField& field) {
  mark_quads_.clear();
  if (stage_ == nullptr) return;
  for (const WarningMark& mark : warnings_) {
    const ro::sim::SurvivorsWarning& warning = mark.warning;
    const std::uint32_t span =
        std::max<std::uint32_t>(warning.strike_step - mark.announced_step, 1);
    const std::uint32_t done = std::min(field.step - mark.announced_step, span);
    const auto alpha = static_cast<std::uint8_t>(
        kWarningAlphaFirst + (kWarningAlphaLast - kWarningAlphaFirst) * done / span);
    const std::array<std::uint8_t, 4> color = {kWarningRgb[0], kWarningRgb[1], kWarningRgb[2],
                                               alpha};
    const ro::sim::FieldPoint from =
        warning.kind == ro::sim::WarningKind::kCharge ? warning.from : warning.to;
    LayCells(*stage_, origin_cells_, from, warning.to, warning.radius_units, color, mark_quads_);
  }
  for (const ro::sim::SurvivorsHazard& hazard : field.hazards) {
    if (field.step >= hazard.until_step) continue;
    LayCells(*stage_, origin_cells_, hazard.center, hazard.center, hazard.radius_units,
             hazard.kind == ro::sim::HazardKind::kPuddle ? kPuddleColor : kQuakeColor, mark_quads_);
  }
}

void ArenaEffects::DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                                    const ro::render::SceneFog& fog,
                                    ro::render::SpriteRenderer& sprites) {
  if (marks_.has_value() && !mark_quads_.empty()) {
    marks_->Draw(camera, mark_quads_, aspect_ratio, fog);
  }
  if (!renderer_.has_value()) return;
  if (const std::span<const ro::render::DrawnEffect> behind = effects_.DrawnBehindActors();
      !behind.empty()) {
    renderer_->Draw(camera, behind, aspect_ratio);
  }
  PaintClassSpritesBehindActors(effects_, sprites, camera, aspect_ratio, fog);
}

void ArenaEffects::DrawAfterBodies(ro::render::SpriteRenderer& sprites,
                                   const ro::render::Camera& camera, float aspect_ratio,
                                   float viewport_width, float viewport_height,
                                   const ro::render::SceneFog& fog, std::uint32_t now_ms) {
  if (!renderer_.has_value() || stage_ == nullptr || !planner_.has_value()) return;
  effects_.SeeScene(camera, {});
  const std::span<const ro::render::DrawnEffect> played =
      effects_.Drawn(now_ms, *stage_, world_, *planner_);
  // La troupe bat à chaque image, qu'il y ait ou non de quoi peindre.
  static_cast<void>(nodes_.Drawn(now_ms, camera));
  const EffectPainters painters{.sprites = &sprites,
                                .effects = &*renderer_,
                                .ground_quads = ground_.has_value() ? &*ground_ : nullptr,
                                .stage = stage_,
                                .camera = &camera,
                                .aspect_ratio = aspect_ratio,
                                .viewport_width = viewport_width,
                                .viewport_height = viewport_height,
                                .fog = fog};
  PaintEffectsUnderStr(effects_, nodes_, painters);
  PaintStrEffects(effects_, nodes_, played, painters);
  effects_.SowRequestedEffects(world_, *stage_, *planner_, now_ms);
  PaintEffectsLast(effects_, nodes_, painters);
  // ⚠ LA SECOUSSE ET LES EFFETS LANCÉS SE JETTENT : la caméra de l'arène ne
  // tremble que sur l'ordre du mode, et le bruit d'un lancer part du mode, au
  // lancer (`SkillSoundFilesOf`).
  static_cast<void>(effects_.TakeScreenQuake());
  static_cast<void>(effects_.TakeLaunchedEffects());
  // LES BRUITS QUE LES EFFETS FONT EN VIVANT, eux, sont gardés pour le mode,
  // qui les passe par son budget de voix : chaque boule semée de Soul Strike,
  // et ce que les effets nommés jouent. Ceux de l'image précédente que
  // personne n'a pris se perdent.
  heard_.clear();
  std::uint32_t draw = now_ms;
  for (const SownNamedSound& named : effects_.TakeSownNamedSounds()) {
    ro::render::EffectSoundEntry pattern;
    pattern.sound = named.sound;
    pattern.variants = named.variants;
    heard_.push_back(ro::render::EffectSoundFile(pattern, draw++));
  }
  for (ro::render::EffectNodeSound& sown : nodes_.TakeSounds()) {
    heard_.push_back(std::move(sown.sound));
  }
  SettleActorEffects(world_, effects_.TakeActorEffects());
}

void ArenaEffects::Warm(std::span<const std::uint16_t> skill_ids, glm::vec3 far_point,
                        ro::render::SpriteRenderer& sprites, const ro::render::Camera& camera,
                        float aspect_ratio, float viewport_width, float viewport_height,
                        std::uint32_t now_ms) {
  for (const std::uint16_t skill_id : skill_ids) {
    const SkillLook* look = looks_.Find(skill_id);
    if (look == nullptr) continue;
    static_cast<void>(effects_.PlayById(look->effect_id, far_point, now_ms));
    static_cast<void>(effects_.PlayById(look->hit_effect_id, far_point, now_ms));
  }
  DrawAfterBodies(sprites, camera, aspect_ratio, viewport_width, viewport_height,
                  ro::render::SceneFog(), now_ms);
  // RIEN NE SURVIT À LA CHAUFFE : la partie commence sans effet vivant.
  Restart();
}

}  // namespace moonclient
