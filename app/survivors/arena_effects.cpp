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

// LES ZONES DE TOUTE L'ÉQUIPE, chacune sous le numéro unique que la partie lui donne.
static std::span<const ro::sim::SurvivorsZone> TeamZonesOf(
    const ro::sim::SurvivorsField& field, std::vector<ro::sim::SurvivorsZone>& out) {
  out.clear();
  for (std::size_t index = 0; index < ro::sim::HeroCount(field); ++index) {
    const std::vector<ro::sim::SurvivorsZone>& zones = ro::sim::ZonesAt(field, index);
    out.insert(out.end(), zones.begin(), zones.end());
  }
  return out;
}
// Les tirs de toute l'équipe, dans l'ordre de ses héros.
static void TeamShotsOf(const ro::sim::SurvivorsField& field,
                        std::vector<ro::sim::SurvivorsShot>& out) {
  out.clear();
  for (std::size_t index = 0; index < ro::sim::HeroCount(field); ++index) {
    const std::vector<ro::sim::SurvivorsShot>& shots = ro::sim::ShotsAt(field, index);
    out.insert(out.end(), shots.begin(), shots.end());
  }
}
namespace {

// La marque d'un piège qui attend : franche, pour trancher sur le sable.
constexpr std::array<std::uint8_t, 4> kTrapColor = {230, 170, 40, 160};
// Un piège posé se montre au moins ce temps, sur ce rayon, même sauté aussitôt.
constexpr std::uint32_t kTrapMarkLeastMs = 600;
constexpr std::int32_t kTrapMarkCells = 1;

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
// Un séisme qui gèle s'annonce et frappe en bleu de glace.
constexpr std::array<std::uint8_t, 3> kFrostWarningRgb = {60, 150, 255};
constexpr std::array<std::uint8_t, 4> kFrostColor = {120, 190, 255, 120};
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
      effects_(catalog_, nodes_, vfs),
      trap_models_(vfs) {
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
  trap_models_.Forget(world_);
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
  trap_marks_.clear();
  mark_quads_.clear();
  trap_models_.Forget(world_);
  lives_.Clear();
  ForgetOrbs();
  ForgetRareGlows();
  zone_grounds_.Forget(PutOutHands());
  // Les compteurs repartent de zéro, les plafonds restent.
  budget_ = ArenaEffectBudget(budget_.caps());
  launched_total_ = 0;
  step_ = 0;
  // Ni la chauffe ni la partie d'avant ne font entendre leurs bruits.
  heard_.clear();
  // La troupe oubliée, la météo se relève à la prochaine image.
  weather_raised_ = false;
}

void ArenaEffects::set_weather(std::vector<std::uint32_t> effect_ids) {
  if (weather_raised_) {
    for (const std::uint32_t effect_id : weather_) nodes_.StopEffect(effect_id, kArenaHeroActorId);
  }
  weather_ = std::move(effect_ids);
  weather_raised_ = false;
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
    flames_.clear();
  }
  // Un effet au sol que le jeu ne coupe jamais reste allumé.
  zone_grounds_.PutOutHidden(switches, PutOutHands());
  if (!switches.shown) ForgetRareGlows();
}

ArenaZoneGroundHands ArenaEffects::PutOutHands() {
  // Ni pose ni sol ni heure : ces mains ne sèment jamais.
  constexpr std::uint32_t kNeverSows = 0;
  return ArenaZoneGroundHandsOn(effects_, world_, {}, {}, kNeverSows);
}

void ArenaEffects::KeepZoneGrounds(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                                   std::uint32_t now_ms) {
  PlanArenaZoneGrounds(TeamZonesOf(field, team_zones_), looks_, switches_, zone_ground_plan_);
  const std::size_t sown_before = zone_grounds_.sown_total();
  zone_grounds_.Keep(zone_ground_plan_, now_ms,
                     ArenaZoneGroundHandsOn(
                         effects_, world_,
                         [this](const ArenaZoneGround& ground) {
                           PlaceActor(ground.actor_id,
                                      CellOfField(glm::vec2(static_cast<float>(ground.at.x),
                                                            static_cast<float>(ground.at.y))));
                         },
                         ground_of, now_ms));
  // Seuls les semis qui ont pris se comptent lancés.
  launched_total_ += zone_grounds_.sown_total() - sown_before;
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
  orb_retry_ms_.clear();
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
  orb_retry_ms_.resize(orb_effects_.size());
  for (std::size_t index = 0; index < orbs_.size(); ++index) {
    const ArenaOrbLook& orb = orbs_[index];
    const std::uint32_t actor_id = OrbActorId(index);
    PlaceActor(actor_id,
               CellOfField(glm::vec2(static_cast<float>(orb.at.x), static_cast<float>(orb.at.y))));
    // L'ORBE SE DÉPLACE AVEC SON ACTEUR : sans suivi, ses nœuds déjà semés
    // resteraient là où elle a été allumée, et l'orbite ne tournerait plus.
    nodes_.FollowActor(actor_id, ground_of(orb.at), kArenaBearerFacingDegrees);
    if (index < orb_effects_.size()) {
      // UNE ORBE DONT L'EFFET EST MORT SE RESÈME : un semis refusé par la troupe
      // pleine, ou un effet éteint au bout de sa vie, la laisserait invisible
      // alors qu'elle tourne et frappe encore. Le semis refusé ne se retente
      // qu'après `kZoneGroundRetryMs`.
      if (orb_effects_[index] == orb.effect_id &&
          (effects_.Carries(orb.effect_id, actor_id) || now_ms < orb_retry_ms_[index])) {
        continue;
      }
      if (orb_effects_[index] != orb.effect_id) {
        effects_.StopById(orb_effects_[index], actor_id);
        orb_effects_[index] = orb.effect_id;
      }
    } else {
      orb_effects_.push_back(orb.effect_id);
      orb_retry_ms_.push_back(0);
    }
    if (effects_.PlayById(orb.effect_id, ground_of(orb.at), now_ms, actor_id, true)) {
      ++launched_total_;
    } else {
      orb_retry_ms_[index] = now_ms + kZoneGroundRetryMs;
    }
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
  lives_.Add(now_ms + LifeMsOf(launch.effect_id),
             on_hero ? std::nullopt : std::optional<ro::sim::FieldPoint>(launch.at));
  ++launched_total_;
}

std::size_t ArenaEffects::LivingSeen(const ArenaSight& sight) const {
  const auto seen = [&sight](ro::sim::FieldPoint at) { return sight.Sees(at); };
  std::size_t count = lives_.CountSeen(sight);
  count += static_cast<std::size_t>(std::ranges::count_if(orbs_, seen, &ArenaOrbLook::at));
  count += static_cast<std::size_t>(std::ranges::count_if(glow_plan_, seen, &ArenaRareGlow::at));
  count += static_cast<std::size_t>(
      std::ranges::count_if(zone_ground_plan_, seen, &ArenaZoneGround::at));
  return count;
}

void ArenaEffects::set_caps(ArenaEffectCaps caps) { budget_ = ArenaEffectBudget(caps); }

void ArenaEffects::Advance(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                           std::span<const ro::sim::SurvivorsOrb> orbs, const GroundOf& ground_of,
                           std::uint32_t now_ms, const ArenaSight& sight) {
  step_ = field.step;
  PlaceActor(kArenaHeroActorId, CellOfField(glm::vec2(static_cast<float>(field.hero.position.x),
                                                      static_cast<float>(field.hero.position.y))));
  // Ce que porte le héros le suit, selon la table de suivi de chaque effet.
  nodes_.FollowActor(kArenaHeroActorId, ground_of(field.hero.position), kArenaBearerFacingDegrees);
  // LA MÉTÉO suit l'interrupteur des effets : levée quand ils se montrent,
  // éteinte quand ils se coupent.
  if (const bool wanted = switches_.shown && !weather_.empty(); wanted != weather_raised_) {
    for (const std::uint32_t effect_id : weather_) {
      if (wanted) {
        static_cast<void>(nodes_.PlayProcedural(effect_id, ground_of(field.hero.position),
                                                std::nullopt, 0.0F, kArenaHeroActorId));
      } else {
        nodes_.StopEffect(effect_id, kArenaHeroActorId);
      }
    }
    weather_raised_ = wanted;
  }
  lives_.Expire(now_ms);
  KeepOrbs(orbs, ground_of, now_ms);
  KeepRareGlows(field, ground_of, now_ms);
  KeepZoneGrounds(field, ground_of, now_ms);
  budget_.set_reserves_held(switches_.shown && !switches_.reduced);
  budget_.BeginFrame(LivingSeen(sight), living());
  // LES SORTS ET LES FLAMMES DES MURS AU RANG DU HÉROS, devant les impacts : le
  // budget tranche sur tous les candidats de l'image à la fois.
  CollectArenaLaunches(news, field, looks_, switches_, plan_);
  KeepZones(field, now_ms);
  if (stage_ != nullptr) trap_models_.Follow(world_, team_zones_, *stage_, origin_cells_, now_ms);
  AdmitArenaLaunches(sight, budget_, plan_);
  for (const ArenaLaunch& launch : plan_) {
    Launch(launch, ground_of, now_ms);
    if (launch.caller_index.has_value()) {
      ZoneFlame& flame = flames_[*launch.caller_index];
      flame.relaunch_ms = now_ms + LifeMsOf(flame.effect_id);
    }
  }
  for (const ro::sim::SurvivorsWarning& warning : news.warnings) {
    warnings_.push_back(WarningMark{.warning = warning, .announced_step = field.step});
  }
  std::erase_if(warnings_, [&field](const WarningMark& mark) {
    return field.step >= mark.warning.strike_step;
  });
  for (const ro::sim::SurvivorsCast& cast : news.casts) {
    if (cast.kind != ro::sim::CastKind::kRingOn) continue;
    const SkillLook* look = looks_.Find(cast.skill_id);
    if (look == nullptr || !look->trap) continue;
    trap_marks_.push_back(TrapMark{.center = cast.from,
                                   .until_step = field.step + ro::sim::StepsOf(kTrapMarkLeastMs)});
  }
  std::erase_if(trap_marks_,
                [&field](const TrapMark& mark) { return field.step >= mark.until_step; });
  ShootFoeShots(field, ground_of, sight, now_ms);
  ShootHeroArrows(field, ground_of, sight);
  LayGroundMarks(field);
}

void ArenaEffects::KeepZones(const ro::sim::SurvivorsField& field, std::uint32_t now_ms) {
  (void)TeamZonesOf(field, team_zones_);
  std::erase_if(flames_, [this](const ZoneFlame& flame) {
    return std::ranges::none_of(team_zones_, [&flame](const ro::sim::SurvivorsZone& zone) {
      return zone.serial == flame.zone_serial;
    });
  });
  if (!switches_.shown || switches_.reduced) {
    flames_.clear();
    return;
  }
  std::vector<ro::sim::FieldPoint> points;
  for (const ro::sim::SurvivorsZone& zone : team_zones_) {
    // Un bloc de barrière n'a pas de flamme : son unité au sol le montre. Un
    // piège non plus : sa marque au sol le montre. Ni une zone dont l'effet
    // EST le visuel entretenu de son unité (Venom Dust) : relancé sans porteur,
    // il sèmerait sans fin, bien après la zone.
    if (zone.archetype == ro::sim::SkillArchetype::kBarrier) continue;
    const SkillLook* look = looks_.Find(zone.skill_id);
    if (look == nullptr || look->trap) continue;
    if (look->ground_effect_id != 0 && look->effect_id == look->ground_effect_id) continue;
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
  // UNE FLAMME ÉTEINTE SE RALLUME tant que sa zone dure, candidate au rang du
  // héros ; sans place au budget, ou hors champ, elle réessaie à l'image
  // suivante.
  for (std::size_t index = 0; index < flames_.size(); ++index) {
    const ZoneFlame& flame = flames_[index];
    if (now_ms < flame.relaunch_ms) continue;
    plan_.push_back(ArenaLaunch{.effect_id = flame.effect_id,
                                .kind = ArenaLaunchKind::kOnGround,
                                .at = flame.at,
                                .rank = ArenaEffectRank::kHero,
                                .caller_index = index});
  }
}

void ArenaEffects::ShootFoeShots(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                                 const ArenaSight& sight, std::uint32_t now_ms) {
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
    const std::optional<std::uint32_t> bolt = FoeShotEffectOf(shooter->foe_id);
    if (sprite.empty() && !bolt.has_value()) continue;
    const std::uint32_t steps = shot.expires_step > field.step ? shot.expires_step - field.step : 0;
    const ro::sim::FieldPoint lands = Along(shot.position, shot.velocity, steps);
    // UNE FLÈCHE EST UN EFFET SECONDAIRE : la dernière servie, et hors champ
    // tout son vol, elle ne part pas.
    if (!sight.Sees(shot.position) && !sight.Sees(lands)) {
      budget_.NoteUnseen();
      continue;
    }
    if (!budget_.Admit(ArenaEffectRank::kLoose)) continue;
    // UN TIREUR SANS MUNITION au client tire l'effet que le mode lui donne, de
    // sa place vers le point où le tir tombe.
    if (sprite.empty()) {
      Launch(ArenaLaunch{.effect_id = *bolt,
                         .kind = ArenaLaunchKind::kOnGround,
                         .at = shot.position,
                         .target = lands},
             ground_of, now_ms);
      continue;
    }
    const std::uint32_t flight_ms = steps * ro::sim::kSurvivorsStepMs;
    const glm::vec3 lift{0.0F, ro::render::kArrowFlightHeight, 0.0F};
    const auto life_ticks = static_cast<std::int32_t>(std::max<std::uint32_t>(
        flight_ms / static_cast<std::uint32_t>(ro::render::kEffectTickMs), 1));
    static_cast<void>(nodes_.ShootSprite(sprite, ground_of(shot.position) + lift,
                                         ground_of(lands) + lift, life_ticks,
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
    const std::array<std::uint8_t, 3>& rgb = warning.freezes ? kFrostWarningRgb : kWarningRgb;
    const std::array<std::uint8_t, 4> color = {rgb[0], rgb[1], rgb[2], alpha};
    const ro::sim::FieldPoint from =
        warning.kind == ro::sim::WarningKind::kCharge ? warning.from : warning.to;
    LayCells(*stage_, origin_cells_, from, warning.to, warning.radius_units, color, mark_quads_);
  }
  for (const ro::sim::SurvivorsHazard& hazard : field.hazards) {
    if (field.step >= hazard.until_step) continue;
    const std::array<std::uint8_t, 4>& color = hazard.kind == ro::sim::HazardKind::kPuddle
                                                   ? kPuddleColor
                                               : hazard.slow_pct > 0 ? kFrostColor
                                                                     : kQuakeColor;
    LayCells(*stage_, origin_cells_, hazard.center, hazard.center, hazard.radius_units, color,
             mark_quads_);
  }
  // LES PIÈGES QUI ATTENDENT, de toute l'équipe : leur aire, en marque calme.
  if (!switches_.shown) return;
  for (const ro::sim::SurvivorsZone& zone : team_zones_) {
    const SkillLook* look = looks_.Find(zone.skill_id);
    if (look == nullptr || !look->trap) continue;
    LayCells(*stage_, origin_cells_, zone.from, zone.from, zone.radius_units, kTrapColor,
             mark_quads_);
  }
  // Et ceux qui viennent d'être posés, même déjà sautés.
  for (const TrapMark& mark : trap_marks_) {
    LayCells(*stage_, origin_cells_, mark.center, mark.center, ro::sim::Cells(kTrapMarkCells),
             kTrapColor, mark_quads_);
  }
}

void ArenaEffects::ShootHeroArrows(const ro::sim::SurvivorsField& field, const GroundOf& ground_of,
                                   const ArenaSight& sight) {
  TeamShotsOf(field, team_shots_);
  std::erase_if(arrows_, [this](std::uint32_t serial) {
    return std::ranges::none_of(team_shots_, [serial](const ro::sim::SurvivorsShot& shot) {
      return shot.serial == serial;
    });
  });
  const ro::render::ActorLook bow{
      .player = true, .job = kArrowShooterJob, .weapon = kArrowShooterWeapon};
  const ro::render::ActorLook wand{
      .player = true, .job = kWandShooterJob, .weapon = kWandShooterWeapon};
  const std::string_view arrow_sprite = ro::render::ArrowSpriteOf(*sprites_, bow, false);
  const std::string_view wand_sprite = ro::render::ArrowSpriteOf(*sprites_, wand, false);
  for (const ro::sim::SurvivorsShot& shot : team_shots_) {
    if (std::ranges::find(arrows_, shot.serial) != arrows_.end()) continue;
    const SkillLook* look = looks_.Find(shot.skill_id);
    if (look == nullptr || !look->shoots_arrow) continue;
    arrows_.push_back(shot.serial);
    const ro::render::ActorLook& shooter = look->shoots_wand_bolt ? wand : bow;
    const std::string_view sprite = look->shoots_wand_bolt ? wand_sprite : arrow_sprite;
    if (!switches_.shown || switches_.reduced || sprite.empty()) continue;
    const std::uint32_t steps = shot.expires_step > field.step ? shot.expires_step - field.step : 0;
    const ro::sim::FieldPoint lands = Along(shot.position, shot.velocity, steps);
    if (!sight.Sees(shot.position) && !sight.Sees(lands)) {
      budget_.NoteUnseen();
      continue;
    }
    if (!budget_.Admit(ArenaEffectRank::kLoose)) continue;
    const std::uint32_t flight_ms = steps * ro::sim::kSurvivorsStepMs;
    const glm::vec3 lift{0.0F, ro::render::kArrowFlightHeight, 0.0F};
    const auto life_ticks = static_cast<std::int32_t>(std::max<std::uint32_t>(
        flight_ms / static_cast<std::uint32_t>(ro::render::kEffectTickMs), 1));
    static_cast<void>(nodes_.ShootSprite(sprite, ground_of(shot.position) + lift,
                                         ground_of(lands) + lift, life_ticks,
                                         ro::render::ProjectileFlightOf(*sprites_, shooter)));
    ++launched_total_;
  }
}

void ArenaEffects::DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                                    const ro::render::SceneFog& fog,
                                    ro::render::SpriteRenderer& sprites) {
  if (marks_.has_value() && !mark_quads_.empty()) {
    marks_->Draw(camera, mark_quads_, aspect_ratio, fog);
  }
  trap_models_.Draw(world_, camera, aspect_ratio, fog);
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
