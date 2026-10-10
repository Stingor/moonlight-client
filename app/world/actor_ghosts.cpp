#include "world/actor_ghosts.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ro/render/effect_frame.h"
#include "ro/sim/movement.h"

#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

// Une rémanence sans image n'en est pas une : la copier coûterait autant et ne
// montrerait rien.
[[nodiscard]] bool Visible(const ro::render::ComposedActor& body) { return !body.quads.empty(); }

constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
constexpr std::size_t kAlphaChannel = 3;
// La table des effets d'acteur range alpha, rouge, vert, bleu ; une teinte de
// dessin range rouge, vert, bleu, alpha.
constexpr std::array<std::size_t, 4> kTableToDrawn = {3, 0, 1, 2};
constexpr float kChannelMax = 255.0F;
constexpr float kDegreesPerRadian = 57.29578F;

// LA TEINTE D'UNE COPIE : celle de son porteur, multipliée composante par
// composante par ce que son effet lui donne — une composante laissée
// « inchangée » garde celle du porteur.
[[nodiscard]] std::array<std::uint8_t, 4> CloneTint(const std::array<std::uint8_t, 4>& bearer,
                                                    const ro::render::ActorEffectChange& change) {
  std::array<std::uint8_t, 4> tint = bearer;
  for (std::size_t channel = 0; channel < change.clone_tint.size(); ++channel) {
    const std::int16_t wanted = change.clone_tint[channel];
    if (wanted == ro::render::kTintUnchanged || wanted == ro::render::kTintRandom) {
      continue;
    }
    std::uint8_t& drawn = tint[kTableToDrawn[channel]];
    drawn = static_cast<std::uint8_t>(static_cast<float>(drawn) * static_cast<float>(wanted) /
                                      kChannelMax);
  }
  return tint;
}

// LA PLACE D'UNE COPIE RECULÉE : derrière son porteur sur l'axe de son corps,
// l'angle en degrés du client — tronqué en entier, comme le client lit sa
// table de sinus —, la profondeur retournée dans notre repère.
[[nodiscard]] glm::vec3 BehindBearer(const glm::vec3& ground, float body_degrees, float units) {
  const float heading = std::trunc(body_degrees) / kDegreesPerRadian;
  return ground + glm::vec3(-std::sin(heading) * units, 0.0F, -std::cos(heading) * units);
}

}  // namespace

void ActorGhosts::Notice(std::uint32_t actor_id,
                         std::span<const ro::render::ActorEffectPlay> played,
                         const ro::render::ComposedActor& body, const glm::vec3& ground,
                         const std::array<std::uint8_t, 4>& tint, float life_ms,
                         std::uint32_t now_ms) {
  if (!Visible(body) || !(life_ms > 0.0F)) {
    return;
  }
  for (const ro::render::ActorEffectPlay& play : played) {
    // La traînée a ses propres règles — couleur, immobilité, fondu : voir
    // `NoticeBlur`.
    if (play.effect_id == ro::render::kAfterimageBlurEffect) {
      continue;
    }
    const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(play.effect_id);
    if (change == nullptr || change->clone_period_ticks == 0) {
      continue;
    }
    if (play.age_ticks % change->clone_period_ticks != 0) {
      continue;
    }
    const auto key = std::pair{actor_id, play.effect_id};
    const auto sown = sown_.find(key);
    if (sown != sown_.end() && sown->second == play.age_ticks) {
      continue;
    }
    sown_.insert_or_assign(key, play.age_ticks);
    glm::vec3 place = ground;
    if (change->clone_behind_units != 0.0F) {
      const auto heading = body_degrees_.find(actor_id);
      place = BehindBearer(ground, heading == body_degrees_.end() ? 0.0F : heading->second,
                           change->clone_behind_units);
    }
    Sow(body, place, CloneTint(tint, *change), life_ms, false, now_ms);
  }
}

std::optional<ro::render::TintEveryTickWrite> BlurTintWriteOf(
    const ro::sim::Actor& actor, const ro::render::AfterimageBlur& blur, std::uint32_t draw) {
  const auto worn = std::ranges::find(actor.effects, ro::render::kAfterimageBlurEffect,
                                      &ro::sim::PlayedEffect::id);
  if (worn == actor.effects.end()) {
    return std::nullopt;
  }
  return ro::render::TintEveryTickWrite{
      .rank = static_cast<std::size_t>(worn - actor.effects.begin()),
      .tint = ro::render::AfterimageBlurWrittenTint(blur, draw)};
}

std::optional<ro::render::AfterimageBlur> ActorGhosts::BlurOf(std::uint32_t actor_id,
                                                              const ro::sim::Actor& actor,
                                                              const glm::vec3& native_place,
                                                              std::uint32_t now_ms) {
  body_degrees_.insert_or_assign(actor_id,
                                 ClientBodyDegrees(actor, ro::sim::FacingAt(actor, now_ms)));
  const auto worn = std::ranges::find(actor.effects, ro::render::kAfterimageBlurEffect,
                                      &ro::sim::PlayedEffect::id);
  if (worn == actor.effects.end()) {
    blur_places_.erase(actor_id);
    return std::nullopt;
  }
  ro::render::AfterimageBlur blur;
  blur.age_ticks = (now_ms - worn->since_ms) / kTickMs;
  // LA COULEUR DONNÉE, par le client lui-même ou par l'état qui pose la traînée.
  const std::optional<std::array<float, 3>> from_state =
      worn->told_color.has_value() ? worn->told_color
      : worn->posed_by_state.has_value()
          ? ro::render::AfterimageBlurStateColor(*worn->posed_by_state)
          : std::nullopt;
  if (from_state.has_value()) {
    blur.color = *from_state;
    return blur;
  }
  const auto known = blur_places_.find(actor_id);
  if (known == blur_places_.end() || known->second.since_ms != worn->since_ms) {
    blur_places_.insert_or_assign(
        actor_id, BlurPlace{worn->since_ms, {native_place.x, native_place.y, native_place.z}});
  }
  blur.color = blur_places_.at(actor_id).color;
  return blur;
}

void ActorGhosts::NoticeBlur(std::uint32_t actor_id, const ro::render::AfterimageBlur& blur,
                             ro::render::Motion motion, const ro::render::ComposedActor& body,
                             const glm::vec3& ground, const std::array<std::uint8_t, 4>& tint,
                             std::uint32_t now_ms) {
  // L'ÂGE DE LA DERNIÈRE IMAGE REGARDÉE, semée ou non : le semis se juge sur
  // les tics enjambés depuis — voir `AfterimageBlurSowsSince`.
  const auto seen = blur_seen_ages_.find(actor_id);
  const std::optional<std::uint32_t> previous_age =
      seen != blur_seen_ages_.end() ? std::optional{seen->second} : std::nullopt;
  blur_seen_ages_.insert_or_assign(actor_id, blur.age_ticks);
  if (!Visible(body) || !ro::render::AfterimageBlurSowsSince(blur, previous_age, motion)) {
    return;
  }
  const auto key = std::pair{actor_id, ro::render::kAfterimageBlurEffect};
  const auto sown = sown_.find(key);
  if (sown != sown_.end() && sown->second == blur.age_ticks) {
    return;
  }
  sown_.insert_or_assign(key, blur.age_ticks);
  Sow(body, ground, tint, static_cast<float>(ro::render::kAfterimageGhostLifeTicks * kTickMs), true,
      now_ms);
}

void ActorGhosts::Sow(const ro::render::ComposedActor& body, const glm::vec3& ground,
                      const std::array<std::uint8_t, 4>& tint, float life_ms, bool fades,
                      std::uint32_t now_ms) {
  if (ghosts_.size() >= kMaxActorGhosts) {
    ghosts_.erase(ghosts_.begin());
  }
  Ghost ghost;
  ghost.body = body;
  // ⚠ LE SON NE SUIT PAS LA COPIE. `ComposedActor::sound` pointe dans le .act
  // de la tenue ; une rémanence peut survivre au changement d'apparence de
  // son porteur, et cette vue pendrait. Elle ne sert qu'à entendre, et une
  // rémanence ne fait aucun bruit.
  ghost.body.sound = {};
  ghost.ground = ground;
  ghost.tint = tint;
  ghost.born_ms = now_ms;
  ghost.life_ms = life_ms;
  ghost.fades = fades;
  ghosts_.push_back(std::move(ghost));
}

void ActorGhosts::Drawn(std::uint32_t now_ms, std::vector<ro::render::DrawnActor>& behind) {
  std::erase_if(ghosts_, [now_ms](const Ghost& ghost) {
    return static_cast<float>(now_ms - ghost.born_ms) >= ghost.life_ms;
  });
  for (const Ghost& ghost : ghosts_) {
    std::array<std::uint8_t, 4> tint = ghost.tint;
    if (ghost.fades) {
      const std::optional<std::uint8_t> alpha =
          ro::render::AfterimageGhostAlphaAt((now_ms - ghost.born_ms) / kTickMs);
      if (!alpha.has_value()) {
        continue;
      }
      tint[kAlphaChannel] = *alpha;
    }
    // Ni ombre, ni recul de désignation : une rémanence n'est pas un acteur, on
    // ne la clique pas et elle ne pose rien au sol.
    behind.push_back(ro::render::DrawnActor{&ghost.body, nullptr, ghost.ground, tint});
  }
}

}  // namespace moonclient
