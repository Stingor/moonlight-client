#include "effects/effect_str_look.h"

#include <cmath>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_update_quake.h"
#include "ro/render/primitive_str_loop.h"
#include "ro/render/str_effect_setup.h"

namespace moonclient {
namespace {

constexpr float kFullTurnDegrees = 360.0F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kPi = 3.14159265358979F;
// L'écart d'écran, en coordonnées normalisées, sous lequel deux points se
// confondent : leur angle ne veut plus rien dire.
constexpr float kSameScreenPoint = 1e-6F;

std::uint32_t TickMs(std::int32_t ticks) {
  return static_cast<std::uint32_t>(ticks) * static_cast<std::uint32_t>(ro::render::kEffectTickMs);
}

// Un tour ramené dans [0, 360).
float WithinTurn(float degrees) {
  const float turned = std::fmod(degrees, kFullTurnDegrees);
  return turned < 0.0F ? turned + kFullTurnDegrees : turned;
}

}  // namespace

std::vector<std::uint32_t> ShapeStrPlaying(EffectMapMemory::Playing& playing,
                                           std::uint32_t effect_id, std::uint32_t owner_id,
                                           std::optional<float> toward_target_degrees) {
  // UN PORTEUR QUI S'EN VA EMPORTE CE FICHIER-CI, ou le laisse finir sur place.
  playing.dies_with_actor = playing.actor_id != 0 && ro::render::StrDiesWithBearer(effect_id);
  // UN PRIMITIF DONT LE FICHIER REPART À SA FIN rejoue jusqu'à la fin de sa vie
  // de table, et s'en va plus tôt avec son porteur.
  if (ro::render::PrimitiveStrLoopsForLife(effect_id)) {
    playing.life_ticks = ro::render::EffectDurationTicks(effect_id);
    if (owner_id != 0) {
      playing.loops_with_actor = owner_id;
    } else {
      playing.loops_endlessly = true;
    }
  }
  const ro::render::StrEffectSetup* setup = ro::render::StrEffectSetupFor(effect_id);
  if (setup == nullptr) {
    return {};
  }
  playing.behind_actors = playing.behind_actors || ro::render::StrPaintsBehindActors(*setup);
  playing.over_everything = ro::render::StrPaintsOverEverything(*setup);
  // UN FICHIER QUI BOUCLE s'éteint avec son porteur quand il vit avec lui, et
  // jamais sinon. Un porteur absent — un effet posé au sol — ne s'en va pas :
  // la vie liée à lui tient alors pour toujours.
  if (setup->loops) {
    if (setup->lives_with_owner && owner_id != 0) {
      playing.loops_with_actor = owner_id;
    } else {
      playing.loops_endlessly = true;
    }
  }
  playing.scale *= setup->scale;
  playing.replays = setup->replays;
  if (setup->life_from_duration_table) {
    playing.life_ticks = ro::render::EffectDurationTicks(effect_id);
  }
  if (setup->turns_toward_target && toward_target_degrees.has_value()) {
    playing.turn_degrees = *toward_target_degrees;
  }
  std::vector<std::uint32_t> quakes_ms;
  quakes_ms.reserve(setup->quake_ticks.size());
  for (const std::uint16_t tick : setup->quake_ticks) {
    quakes_ms.push_back(playing.started_ms + TickMs(tick));
  }
  return quakes_ms;
}

void QueueQuakes(std::vector<EffectMapMemory::PendingQuake>& pending,
                 const std::vector<std::uint32_t>& due_ms) {
  for (const std::uint32_t due : due_ms) {
    pending.push_back({due, ro::render::kEffectQueuedQuakeMs});
  }
}

void QueueUpdateQuakes(std::vector<EffectMapMemory::PendingQuake>& pending, std::uint32_t effect_id,
                       std::uint32_t born_ms) {
  const ro::render::EffectUpdateQuake* quakes = ro::render::EffectUpdateQuakeFor(effect_id);
  if (quakes == nullptr) {
    return;
  }
  for (const std::uint16_t tick : quakes->ticks) {
    pending.push_back({born_ms + TickMs(tick), quakes->quake_ms});
  }
}

void LookStrPlaying(EffectMapMemory::Playing& playing, std::uint32_t effect_id, const StrLook& look,
                    float view_yaw_degrees) {
  // UN APPELANT QUI NE TOURNE PAS L'IMAGE laisse celle que le lecteur lui a
  // donnée : une rotation nulle n'en est pas une.
  if (look.spin_degrees != 0.0F || look.spin_follows_view) {
    playing.turn_degrees = look.spin_degrees;
    playing.turn_follows_view = look.spin_follows_view;
    playing.birth_view_yaw_degrees = view_yaw_degrees;
  }
  const ro::render::StrEffectSetup* setup = ro::render::StrEffectSetupFor(effect_id);
  playing.scale =
      look.scale * (setup != nullptr ? ro::render::StrEffectLevelScale(*setup, look.level) : 1.0F);
}

std::optional<float> ScreenBearingDegrees(const ro::render::Camera& camera, const glm::vec3& from,
                                          const glm::vec3& to) {
  // UN RAPPORT D'ASPECT UNITÉ : l'angle entre deux points de l'écran n'en
  // dépend pas, la projection étire les deux axes de pixels à l'identique.
  const glm::mat4 projection = camera.ProjectionMatrix(1.0F) * camera.ViewMatrix();
  const glm::vec4 a = projection * glm::vec4(from, 1.0F);
  const glm::vec4 b = projection * glm::vec4(to, 1.0F);
  if (!(a.w > 0.0F) || !(b.w > 0.0F)) {
    return std::nullopt;
  }
  // L'ordonnée de l'écran descend : celle de la projection monte.
  const float dx = b.x / b.w - a.x / a.w;
  const float dy = -(b.y / b.w - a.y / a.w);
  const float length = std::sqrt(dx * dx + dy * dy);
  if (!(length > kSameScreenPoint)) {
    return std::nullopt;
  }
  const float degrees = std::acos(dx / length) * kHalfTurnDegrees / kPi;
  return dy < 0.0F ? kFullTurnDegrees - degrees : degrees;
}

std::optional<float> StrTowardTargetDegrees(std::uint32_t effect_id,
                                            const ro::render::Camera* camera, const glm::vec3& from,
                                            const std::optional<glm::vec3>& to) {
  const ro::render::StrEffectSetup* setup = ro::render::StrEffectSetupFor(effect_id);
  if (setup == nullptr || !setup->turns_toward_target || camera == nullptr || !to.has_value()) {
    return std::nullopt;
  }
  // L'IMAGE REGARDE D'OÙ ELLE VIENT : l'angle vers le point visé, retourné.
  const std::optional<float> bearing = ScreenBearingDegrees(*camera, from, *to);
  if (!bearing.has_value()) {
    return std::nullopt;
  }
  return WithinTurn(*bearing + kHalfTurnDegrees);
}

bool StrAlreadyOnOwner(std::span<const EffectMapMemory::Playing> playing, std::uint32_t effect_id,
                       std::uint32_t owner_id) {
  const ro::render::StrEffectSetup* setup = ro::render::StrEffectSetupFor(effect_id);
  if (setup == nullptr || !setup->unique_on_owner || owner_id == 0) {
    return false;
  }
  for (const EffectMapMemory::Playing& sown : playing) {
    if (sown.from_effect_id == effect_id && sown.actor_id == owner_id) {
      return true;
    }
  }
  return false;
}

std::optional<std::int32_t> StrFrameAt(const EffectMapMemory::Playing& playing, float elapsed_ms) {
  const ro::render::PreparedEffect& prepared = playing.effect->prepared;
  if (playing.life_ticks > 0 && elapsed_ms >= static_cast<float>(TickMs(playing.life_ticks))) {
    return std::nullopt;
  }
  if (playing.loops_with_actor != 0 || playing.loops_endlessly) {
    return ro::render::EffectLoopFrameAt(prepared, elapsed_ms);
  }
  // REJOUÉ À SA FIN, autant de fois que le lecteur le dit, puis éteint.
  const float play_ms = static_cast<float>(TickMs(prepared.last_frame + 1));
  if (playing.replays > 0 && play_ms > 0.0F &&
      elapsed_ms < play_ms * static_cast<float>(playing.replays + 1)) {
    return ro::render::EffectLoopFrameAt(prepared, elapsed_ms);
  }
  std::optional<std::int32_t> frame = ro::render::EffectFrameAt(prepared, elapsed_ms);
  // UNE IMAGE QUI S'ARRÊTE SUR LA DERNIÈRE ne s'efface pas d'elle-même.
  if (!frame.has_value() && playing.holds_last_frame) {
    frame = prepared.last_frame;
  }
  return frame;
}

float StrTurnDegrees(const EffectMapMemory::Playing& playing, float view_yaw_degrees) {
  if (!playing.turn_follows_view) {
    return playing.turn_degrees;
  }
  // CE DONT LA VUE A TOURNÉ S'AJOUTE : l'image garde son orientation dans le
  // monde. Ramenée dans un tour — zéro la laisse droite, comme ailleurs.
  return WithinTurn(playing.turn_degrees + (view_yaw_degrees - playing.birth_view_yaw_degrees));
}

}  // namespace moonclient
