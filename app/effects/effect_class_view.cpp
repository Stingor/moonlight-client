#include "effects/effect_class_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/render/actor_pick.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_sprite.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_step_blend.h"
#include "ro/render/effect_table.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/status_icons.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_requests.h"
#include "effects/effect_view.h"
#include "world/actor_view.h"
#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

constexpr std::uint32_t kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
// UN RATTRAPAGE BORNÉ : après une longue pause, le gestionnaire ne rejoue pas
// des milliers de tics d'un coup.
constexpr std::uint32_t kMostTicksPerFrame = 8;
constexpr float kFullTurnDegrees = 360.0F;
constexpr std::string_view kSpriteFolder = "data\\sprite\\";
// Le préfixe de la variante allégée d'un `.str`, sous `/mineffect`.
constexpr std::string_view kMinimalPrefix = "min_";
// La carte que le client nomme en clair pour ses étincelles de soin.
constexpr std::string_view kWideScatterMapName = "ecl_fild01";

// Un angle ramené dans le tour, comme le client garde le lacet de sa caméra.
float WithinTurn(float degrees) {
  const float remainder = std::fmod(degrees, kFullTurnDegrees);
  return remainder < 0.0F ? remainder + kFullTurnDegrees : remainder;
}

// La variante « min_ » d'un chemin : le nom du fichier préfixé, le dossier gardé.
std::string MinimalVariant(const std::string& path) {
  const std::size_t slash = path.find_last_of('\\');
  if (slash == std::string::npos) {
    return std::string(kMinimalPrefix) + path;
  }
  return path.substr(0, slash + 1) + std::string(kMinimalPrefix) + path.substr(slash + 1);
}

}  // namespace

void EffectClassesInPlay::SeeScene(const ro::render::Camera& camera, float viewport_width,
                                   float viewport_height, bool minimal_effects) {
  camera_ = camera;
  viewport_width_ = viewport_width;
  viewport_height_ = viewport_height;
  minimal_effects_ = minimal_effects;
}

ro::render::EffectClassView EffectClassesInPlay::View() const {
  ro::render::EffectClassView view;
  if (!camera_.has_value()) {
    return view;
  }
  // LE CLIENT COMPTE SES DEUX ANGLES À L'OPPOSÉ DES NÔTRES : son lacet est
  // l'opposé du nôtre, gardé dans le tour ; son tangage est l'élévation niée.
  view.yaw_degrees = WithinTurn(-camera_->yaw_degrees);
  view.pitch_degrees = -camera_->pitch_degrees;
  const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(*camera_);
  view.right = basis.right;
  view.up = basis.up;
  view.forward = basis.forward;
  const ro::render::Camera camera = *camera_;
  const float width = viewport_width_;
  const float height = viewport_height_;
  view.project = [camera, width, height](glm::vec3 point) -> std::optional<glm::vec2> {
    if (width <= 0.0F || height <= 0.0F) {
      return std::nullopt;
    }
    return ro::render::ProjectToScreen(camera, width / height, point, width, height);
  };
  view.viewport_width = width;
  view.viewport_height = height;
  return view;
}

namespace {

// La place au sol d'un acteur ou d'une unité, dans notre repère ; vide pour un
// identifiant que le monde ne tient pas.
std::optional<glm::vec3> GroundOf(std::uint32_t id, const ro::sim::World& world,
                                  const MapStage& stage, ro::sim::WalkPlanner& planner,
                                  std::uint32_t at_ms) {
  if (id == 0) {
    return std::nullopt;
  }
  if (const ro::sim::Actor* actor = world.Find(id); actor != nullptr) {
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(*actor, planner.PathFor(*actor), at_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    return glm::vec3(ground[0], ground[1], ground[2]);
  }
  if (const auto unit = world.skill_units().find(id); unit != world.skill_units().end()) {
    const std::array<float, 3> ground = GroundPositionAt(
        stage, static_cast<float>(unit->second.cell.x), static_cast<float>(unit->second.cell.y));
    return glm::vec3(ground[0], ground[1], ground[2]);
  }
  return std::nullopt;
}

}  // namespace

std::optional<glm::vec3> EffectClassesInPlay::PlaceOf(std::uint32_t id, const ro::sim::World& world,
                                                      const MapStage& stage,
                                                      ro::sim::WalkPlanner& planner,
                                                      std::uint32_t at_ms) const {
  return GroundOf(id, world, stage, planner, at_ms);
}

bool EffectClassesInPlay::Spawn(const Birth& birth, std::uint32_t now_ms) {
  if (!ro::render::IsPortedEffectClass(birth.effect_id)) {
    return false;
  }
  // UNE CLASSE QUI SE REPREND ne naît pas deux fois sur le même porteur.
  for (Living& living : living_) {
    if (living.instance->effect_id() == birth.effect_id && living.owner_id == birth.owner_id &&
        birth.owner_id != 0 && living.instance->reused_on_same_owner()) {
      // L'instance retrouvée reçoit le paramètre de cette naissance-ci, et la
      // marque d'un chapeau qui la fait naître.
      living.instance->Retell(birth.level);
      living.from_hat = living.from_hat || birth.from_hat;
      return true;
    }
  }
  ro::render::EffectClassBirth birth_point;
  birth_point.effect_id = birth.effect_id;
  birth_point.position = ro::render::EffectClassToClient(birth.ground_position);
  if (birth.bound_ground_position.has_value()) {
    birth_point.bound_position = ro::render::EffectClassToClient(*birth.bound_ground_position);
  }
  birth_point.level = birth.level;
  birth_point.delay_ms = birth.delay_ms;
  birth_point.owner_charm_element = birth.owner_charm_element;
  birth_point.owner_charm_count = birth.owner_charm_count;
  birth_point.minimal_effects = minimal_effects_;
  birth_point.now_ms = now_ms;
  birth_point.seed = ++seed_;
  std::unique_ptr<ro::render::EffectClassInstance> instance =
      ro::render::SpawnEffectClass(birth_point, View());
  if (instance != nullptr) {
    living_.push_back(Living{std::move(instance), birth.owner_id, birth.bound_id, birth.from_hat,
                             ++last_serial_});
  }
  return true;
}

void EffectClassesInPlay::Stop(std::uint32_t effect_id, std::uint32_t owner_id) {
  std::erase_if(living_, [effect_id, owner_id](const Living& living) {
    return living.instance->effect_id() == effect_id && living.owner_id == owner_id;
  });
}

bool EffectClassesInPlay::Carries(std::uint32_t effect_id, std::uint32_t owner_id) const {
  return std::ranges::any_of(living_, [effect_id, owner_id](const Living& living) {
    return living.instance->effect_id() == effect_id && living.owner_id == owner_id;
  });
}

void EffectClassesInPlay::Advance(const ro::sim::World& world, const MapStage& stage,
                                  ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  drawn_.clear();
  behind_.clear();
  quads_.clear();
  quad_lots_.clear();
  composed_.clear();
  actors_.clear();
  sprite_batches_.clear();
  screen_quad_lots_.clear();
  screen_quads_.clear();
  if (!clock_started_) {
    last_tick_ms_ = now_ms;
    clock_started_ = true;
  }
  const ro::render::EffectClassView view = View();
  std::optional<glm::vec3> player;
  if (const ro::sim::Actor* self = world.Find(world.player_id()); self != nullptr) {
    player = PlaceOf(self->id, world, stage, planner, now_ms);
  }
  ro::render::EffectClassTick tick;
  tick.view = &view;
  tick.wide_scatter_map = stage.name == kWideScatterMapName;
  tick.str_last_frame = [this](std::string_view path) -> std::optional<std::int32_t> {
    const ro::render::LoadedEffect* loaded = StrFor(std::string(path));
    if (loaded == nullptr) {
      return std::nullopt;
    }
    return loaded->prepared.last_frame;
  };
  tick.sprite_frame_count = [this](std::string_view sprite, std::int32_t action) {
    const ro::render::LoadedActorSprite* loaded = SpriteFor(std::string(sprite));
    if (loaded == nullptr || action < 0 ||
        static_cast<std::size_t>(action) >= loaded->animation.actions().size()) {
      return 0;
    }
    return static_cast<std::int32_t>(
        loaded->animation.actions()[static_cast<std::size_t>(action)].frames.size());
  };
  ro::render::EffectClassOutput events;
  std::uint32_t ticks = 0;
  while (now_ms - last_tick_ms_ >= kTickMs && ticks < kMostTicksPerFrame) {
    last_tick_ms_ += kTickMs;
    ++ticks;
    tick.now_ms = last_tick_ms_;
    std::erase_if(living_, [&](Living& living) {
      tick.owner_position.reset();
      tick.owner_present = living.owner_id == 0;
      if (const std::optional<glm::vec3> owner =
              PlaceOf(living.owner_id, world, stage, planner, last_tick_ms_)) {
        tick.owner_position = ro::render::EffectClassToClient(*owner);
        tick.owner_present = true;
      }
      // CE QUE LE PORTEUR MONTRE, pour les classes qui s'y règlent : la
      // direction de son image, la caméra comptée, et l'angle de son corps.
      tick.owner_sprite_direction.reset();
      tick.owner_body_degrees.reset();
      tick.owner_job.reset();
      tick.owner_shrink_scale = 1.0F;
      tick.owner_hover_pixels = 0.0F;
      if (hover_pixels_ != nullptr) {
        if (const auto hover = hover_pixels_->find(living.owner_id);
            hover != hover_pixels_->end()) {
          tick.owner_hover_pixels = hover->second;
        }
      }
      if (const ro::sim::Actor* owner = world.Find(living.owner_id); owner != nullptr) {
        const std::uint8_t facing =
            ro::sim::FacingAt(*owner, planner.PathFor(*owner), last_tick_ms_);
        tick.owner_sprite_direction = static_cast<std::uint8_t>(
            ro::render::SpriteDirection(facing, camera_.has_value() ? camera_->yaw_degrees : 0.0F));
        tick.owner_body_degrees = ClientBodyDegrees(*owner, facing);
        // LE MÉTIER MONTRÉ ET LA TAILLE DU CORPS, que les sprites posés sur lui
        // suivent : un corps monté les lève.
        tick.owner_job = ShownJobOf(*owner);
        owner_effects_.clear();
        for (const ro::sim::PlayedEffect& effect : owner->effects) {
          owner_effects_.push_back(
              ro::render::ActorEffectPlay{effect.id, (last_tick_ms_ - effect.since_ms) / kTickMs});
        }
        tick.owner_shrink_scale = ro::render::BodyShrinkAfterEffects(owner_effects_);
      }
      tick.bound_position.reset();
      if (const std::optional<glm::vec3> bound =
              PlaceOf(living.bound_id, world, stage, planner, last_tick_ms_)) {
        tick.bound_position = ro::render::EffectClassToClient(*bound);
      }
      if (!living.instance->Alive(tick)) {
        return true;
      }
      const glm::vec3 here = ro::render::EffectClassFromClient(
          tick.owner_position.value_or(living.instance->position()));
      tick.culled =
          player.has_value() && glm::length(here - *player) >= ro::render::kEffectClassCullDistance;
      const std::size_t asked_before = events.spawns_on_owner.size();
      living.instance->Advance(tick, events);
      // Ce que ce battement demande sur le porteur reste attaché à lui.
      for (std::size_t rang = asked_before; rang < events.spawns_on_owner.size(); ++rang) {
        owner_spawns_.push_back(OwnerSpawn{events.spawns_on_owner[rang], living.owner_id, here});
      }
      return false;
    });
  }
  if (ticks == kMostTicksPerFrame) {
    last_tick_ms_ = now_ms;
  }
  for (const ro::render::EffectClassSound& sound : events.sounds) {
    const std::array<float, 2> cellule =
        ro::render::FractionalCellAt(stage.scene.gnd, sound.position.x, sound.position.z);
    SownNamedSound named_sound;
    named_sound.sound = sound.file;
    named_sound.cell =
        ro::sim::CellPosition{static_cast<std::uint16_t>(std::max(cellule[0], 0.0F)),
                              static_cast<std::uint16_t>(std::max(cellule[1], 0.0F)), 0};
    sounds_.push_back(named_sound);
  }
  if (events.quake_ms != 0) {
    quake_until_ms_ = std::max(quake_until_ms_.value_or(0U), now_ms + events.quake_ms);
  }
  // LA PORTE DE DESSIN SAUTE LE DESSIN SEUL : l'instance a battu, elle a fait
  // ses sons et ses demandes. Voir `EffectDrawGate`.
  // LE LISSAGE retient ce que chaque instance dessine : voir `EffectClassSteps`.
  steps_.BeginFrame(ticks, ro::render::EffectStepAlpha(now_ms - last_tick_ms_));
  ro::render::EffectClassOutput dessin;
  for (const Living& living : living_) {
    if (!living.instance->culled() &&
        draw_gate_.DrawsEffect(living.instance->effect_id(), living.owner_id, living.from_hat)) {
      steps_.OpenInstance(living.serial, dessin);
      living.instance->Draw(view, dessin);
      steps_.CloseInstance(dessin);
    }
  }
  steps_.Blend(dessin);
  Compose(dessin);
}

const ro::render::LoadedEffect* EffectClassesInPlay::StrFor(const std::string& path) {
  if (const auto found = strs_.find(path); found != strs_.end()) {
    return found->second;
  }
  const ro::render::LoadedEffect* loaded =
      catalog_->Load(std::string(ro::render::kEffectFolder) + path);
  strs_.emplace(path, loaded);
  return loaded;
}

const ro::formats::Image& EffectClassesInPlay::TextureFor(const std::string& path) {
  if (vfs_->overlay_revision() != textures_revision_) {
    textures_.clear();
    textures_revision_ = vfs_->overlay_revision();
  }
  if (const auto found = textures_.find(path); found != textures_.end()) {
    return found->second;
  }
  ro::formats::Image image;
  // ⚠ `Read` ET NON `ReadRaw` : ces chemins viennent d'une source UTF-8.
  if (auto brut = vfs_->Read(std::string(ro::render::kTextureFolder) + path); brut) {
    if (auto decodee = ro::formats::DecodeImage(path, *brut); decodee) {
      image = std::move(*decodee);
      // LE MAGENTA EST LA TRANSPARENCE DU JEU, pas celle du format, à la
      // précision de sa texture de seize bits — qui en fait aussi les paliers.
      ro::formats::ApplyEffectTextureColorKey(image);
      ro::formats::QuantizeToEffectTextureDepth(image);
    }
  }
  return textures_.emplace(path, std::move(image)).first->second;
}

const ro::render::LoadedActorSprite* EffectClassesInPlay::SpriteFor(const std::string& path) {
  if (const auto found = sprites_.find(path); found != sprites_.end()) {
    return &found->second;
  }
  const std::array<ro::render::ActorPieceRequest, 1> request = {ro::render::ActorPieceRequest{
      ro::render::ActorPart::kBody, std::string(kSpriteFolder) + path}};
  auto loaded = ro::render::LoadActorSprites(*vfs_, request);
  if (!loaded || loaded->empty()) {
    return nullptr;
  }
  return &sprites_.emplace(path, std::move(loaded->front())).first->second;
}

void EffectClassesInPlay::Compose(const ro::render::EffectClassOutput& output) {
  for (const ro::render::EffectClassStrDraw& str : output.strs) {
    const ro::render::LoadedEffect* loaded = nullptr;
    if (minimal_effects_ && str.allows_min_variant) {
      loaded = StrFor(MinimalVariant(str.path));
    }
    if (loaded == nullptr) {
      loaded = StrFor(str.path);
    }
    if (loaded == nullptr) {
      continue;
    }
    ro::render::DrawnEffect peint{loaded, str.ground_position, str.frame, str.lift_pixels,
                                  str.behind_actors};
    peint.spin_degrees = str.spin_degrees;
    peint.scale = str.scale;
    peint.alpha = str.alpha;
    peint.flip_x = str.flip_x;
    peint.frame_fraction = str.frame_fraction;
    (str.behind_actors ? behind_ : drawn_).push_back(peint);
  }
  // LES QUADS, UN LOT PAR TEXTURE ET PAR MOT DE RENDU, dans l'ordre d'arrivée.
  std::vector<std::size_t> debuts;
  for (const ro::render::EffectClassQuadDraw& quad : output.quads) {
    const std::string texture_path(quad.texture);
    const ro::formats::Image& image = TextureFor(texture_path);
    if (image.pixels.empty()) {
      continue;
    }
    const auto key = textures_.find(texture_path);
    if (quad_lots_.empty() || quad_lots_.back().texture_path != &key->first ||
        quad_lots_.back().render_word != quad.render_word) {
      debuts.push_back(quads_.size());
      ro::render::DrawnWorldQuads lot;
      lot.texture_path = &key->first;
      lot.texture = &key->second;
      lot.ground_position = quad.quad.corners[0];
      lot.render_word = quad.render_word;
      quad_lots_.push_back(lot);
    }
    quads_.push_back(quad.quad);
  }
  for (std::size_t rang = 0; rang < quad_lots_.size(); ++rang) {
    const std::size_t fin = rang + 1 < quad_lots_.size() ? debuts[rang + 1] : quads_.size();
    quad_lots_[rang].quads = std::span(quads_).subspan(debuts[rang], fin - debuts[rang]);
  }
  // LES SPRITES, composés comme ceux d'un nœud : échelle, action, image, et le
  // roulis dans le plan de l'écran.
  std::vector<SpriteBatch> buckets;
  for (const ro::render::EffectClassSpriteDraw& sprite : output.sprites) {
    const ro::render::LoadedActorSprite* loaded = SpriteFor(std::string(sprite.sprite));
    if (loaded == nullptr) {
      continue;
    }
    ro::render::EffectNode node;
    node.sprite_scale = sprite.scale;
    node.action = sprite.action;
    node.frame = sprite.frame;
    node.spin_degrees = sprite.spin_degrees;
    node.draws_rotated = sprite.spin_degrees != 0.0F;
    node.flags = sprite.keeps_layer_color ? ro::render::kEffectNodeKeepsLayerColor : 0U;
    ro::render::ComposedActor actor;
    actor.quads = ro::render::ComposeEffectNodeSprite(node, loaded->sprite, loaded->animation);
    if (actor.quads.empty()) {
      continue;
    }
    for (const ro::render::SpriteQuad& quad : actor.quads) {
      actor.sources.push_back(
          ro::render::SpriteImageKey{&loaded->sprite, nullptr, quad.section, quad.image_index});
    }
    composed_.push_back(std::move(actor));
    ro::render::DrawnActor drawn;
    drawn.ground_position = sprite.ground_position;
    drawn.tint = sprite.tint;
    drawn.depth_bias = sprite.depth_bias;
    drawn.screen_offset_pixels = glm::vec2(0.0F, sprite.screen_lift_pixels);
    SpriteBatch bucket;
    bucket.additive = sprite.additive;
    if (sprite.render_word.has_value()) {
      const std::uint32_t word = *sprite.render_word;
      bucket.additive = ro::render::EffectSpriteIsAdditive(word);
      bucket.depth_tested = ro::render::EffectLotIsDepthTested(word);
      bucket.behind_actors = ro::render::EffectRenderWordPaintsBeforeActors(word);
      bucket.before_str = ro::render::EffectRenderWordPaintsBeforeStr(word);
      drawn.fogged = ro::render::EffectRenderWordIsFogged(word);
    }
    actors_.push_back(drawn);
    buckets.push_back(bucket);
  }
  // Les pointeurs se prennent une fois les tableaux finis de grandir.
  for (std::size_t rang = 0; rang < actors_.size(); ++rang) {
    actors_[rang].actor = &composed_[rang];
  }
  // Un lot par seau, les sprites voisins du même seau ensemble : l'ordre suit
  // celui des instances, et le recouvrement passe avant l'addition.
  const auto same = [](const SpriteBatch& left, const SpriteBatch& right) {
    return left.additive == right.additive && left.depth_tested == right.depth_tested &&
           left.before_str == right.before_str && left.behind_actors == right.behind_actors;
  };
  for (const bool additif : {false, true}) {
    std::size_t rang = 0;
    while (rang < actors_.size()) {
      if (buckets[rang].additive != additif) {
        ++rang;
        continue;
      }
      const std::size_t start = rang;
      while (rang < actors_.size() && same(buckets[rang], buckets[start])) {
        ++rang;
      }
      SpriteBatch batch = buckets[start];
      batch.actors = std::span<const ro::render::DrawnActor>(actors_).subspan(start, rang - start);
      sprite_batches_.push_back(batch);
    }
  }
  // LES QUADS D'ÉCRAN, UN LOT PAR TEXTURE, dans l'ordre d'arrivée. C'est ici,
  // l'image en main, que chaque coin reçoit le demi-texel du client.
  for (const ro::render::EffectClassScreenDraw& screen : output.screen_quads) {
    const std::string texture_path(screen.texture);
    const ro::formats::Image& image = TextureFor(texture_path);
    if (image.pixels.empty()) {
      continue;
    }
    const auto key = textures_.find(texture_path);
    if (screen_quads_.empty() || screen_quads_.back().texture_path != &key->first) {
      screen_quad_lots_.emplace_back();
      screen_quads_.push_back(ro::render::DrawnScreenQuads{{}, &key->first, &key->second});
    }
    ro::render::EffectScreenQuad quad = screen.quad;
    for (glm::vec2& uv : quad.texture_uv) {
      uv.x = (uv.x * static_cast<float>(image.width) + ro::render::kEffectScreenTexelOffset) /
             static_cast<float>(image.width);
      uv.y = (uv.y * static_cast<float>(image.height) + ro::render::kEffectScreenTexelOffset) /
             static_cast<float>(image.height);
    }
    screen_quad_lots_.back().push_back(quad);
  }
  for (std::size_t rang = 0; rang < screen_quads_.size(); ++rang) {
    screen_quads_[rang].quads = screen_quad_lots_[rang];
  }
}

void EffectClassesInPlay::Forget() {
  living_.clear();
  steps_.Forget();
  drawn_.clear();
  behind_.clear();
  quads_.clear();
  quad_lots_.clear();
  composed_.clear();
  actors_.clear();
  sprite_batches_.clear();
  screen_quad_lots_.clear();
  screen_quads_.clear();
  sounds_.clear();
  quake_until_ms_.reset();
  clock_started_ = false;
}

std::optional<glm::vec3> SpawnSkillClass(EffectClassesInPlay& classes, const ro::sim::World& world,
                                         const MapStage& stage, ro::sim::WalkPlanner& planner,
                                         const ro::sim::Actor& target, std::uint32_t caster_id,
                                         std::uint32_t started_ms,
                                         const ro::render::SkillSuiteStep& step,
                                         std::uint32_t delay_ms, std::int32_t skill_level) {
  using Carrier = ro::render::SkillSuiteCarrier;
  if (!ro::render::IsPortedEffectClass(step.effect_id)) {
    return std::nullopt;
  }
  const std::optional<glm::vec3> aimed = GroundOf(target.id, world, stage, planner, started_ms);
  const std::optional<glm::vec3> caster = GroundOf(caster_id, world, stage, planner, started_ms);
  EffectClassesInPlay::Birth birth;
  birth.effect_id = step.effect_id;
  birth.bound_id = target.id;
  birth.bound_ground_position = aimed;
  birth.delay_ms = static_cast<std::int32_t>(delay_ms);
  // LE NIVEAU NE VA QU'AUX CLASSES QUI LE REÇOIVENT chez le client ; les autres
  // naissent sans paramètre.
  if (ro::render::EffectClassReceivesSkillLevel(step.effect_id)) {
    birth.level = skill_level;
  } else if (const std::optional<std::int32_t> thrown =
                 ro::render::EffectClassThrownParam(step.effect_id, skill_level)) {
    birth.level = *thrown;
  }
  // LE PORTEUR QUE L'ÉTAPE NOMME, et la place où la classe naît.
  const glm::vec3 offset{step.offset_x, 0.0F, -step.offset_z};
  switch (step.carrier) {
    case Carrier::kCaster:
    case Carrier::kCasterTowardTarget:
    // Le joueur de la caméra arrive ici déjà mis à la place du lanceur.
    case Carrier::kLocalPlayer:
      birth.owner_id = caster_id;
      birth.ground_position = caster.value_or(aimed.value_or(glm::vec3{0.0F}));
      break;
    case Carrier::kTarget:
      birth.owner_id = target.id;
      birth.ground_position = aimed.value_or(glm::vec3{0.0F});
      break;
    case Carrier::kWorldAtCaster:
      birth.ground_position = caster.value_or(aimed.value_or(glm::vec3{0.0F})) + offset;
      break;
    case Carrier::kWorldNearTarget:
      birth.ground_position = aimed.value_or(glm::vec3{0.0F}) + offset;
      break;
  }
  if (ro::render::EffectClassBornAtCaster(step.effect_id) && caster.has_value()) {
    birth.ground_position = *caster;
  }
  if (!classes.Spawn(birth, started_ms)) {
    return std::nullopt;
  }
  return birth.ground_position;
}

}  // namespace moonclient
