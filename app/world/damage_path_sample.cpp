#include "world/damage_path_sample.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "ro/render/actor_pick.h"
#include "ro/render/sprite_billboard.h"

namespace moonclient {
namespace {

// LE RÉGLAGE QUE LE CADRE APPLIQUE : celui du joueur, tout rendu visible. On
// vient y régler une trajectoire, et un type que « Ce qui se montre » tait ne
// doit pas laisser le cadre vide.
[[nodiscard]] HitPopupLook Showing(const HitPopupLook& look) {
  HitPopupLook shown = look;
  shown.shown = true;
  shown.own_dealt = true;
  shown.own_taken = true;
  shown.others = true;
  shown.miss = true;
  shown.lucky = true;
  shown.heals = true;
  shown.effect_numbers = true;
  return shown;
}

// Le texte d'un nombre, en pixels de sprite depuis les pieds de sa cible, y
// descendant — la forme que le cadre attend. C'est `DamageTextOnScreen` à
// l'échelle un : le cadre met lui-même les pixels de sprite à l'écran.
[[nodiscard]] ro::ui::DamageTextView TextInSprite(const ro::render::NumberText& text,
                                                  const ro::render::GlyphBox& digit,
                                                  float outline_pixels, glm::vec2 drift_pixels) {
  const glm::vec2 middle =
      text.landing_pixels + glm::vec2{0.0F, digit.middle_y_pixels * text.scale} + drift_pixels;
  ro::ui::DamageTextView view;
  view.middle = ImVec2(middle.x, middle.y);
  view.digit_height = digit.height_pixels * text.scale;
  view.pitch = text.pitch_pixels * text.scale;
  view.outline = std::max(outline_pixels, 0.0F) * text.scale;
  view.color = IM_COL32(text.tint[0], text.tint[1], text.tint[2], text.tint[3]);
  const std::size_t written = std::min(text.glyphs.size(), view.glyphs.size() - 1);
  std::copy_n(text.glyphs.begin(), written, view.glyphs.begin());
  return view;
}

// LE PAS DU RELEVÉ DE L'ALLURE, et la plus longue vie qu'il suit : un nombre
// vit moins de deux secondes chez le client, quatre à la durée la plus longue.
constexpr std::uint32_t kTraceStepMs = 16;
constexpr std::uint32_t kLongestTracedLifeMs = 10'000;

// Le milieu des quads d'un cadre, en pixels de sprite, y descendant.
[[nodiscard]] std::optional<glm::vec2> MiddleOf(const ro::render::ComposedActor& frame) {
  if (frame.quads.empty()) {
    return std::nullopt;
  }
  glm::vec2 low{std::numeric_limits<float>::max()};
  glm::vec2 high{std::numeric_limits<float>::lowest()};
  for (const ro::render::SpriteQuad& quad : frame.quads) {
    for (const glm::vec2& corner : quad.corners) {
      low = glm::min(low, corner);
      high = glm::max(high, corner);
    }
  }
  return (low + high) * 0.5F;
}

}  // namespace

ro::ui::EntitySampleShown PathTargetShown(DamagePath path) noexcept {
  switch (path) {
    case DamagePath::kOnPlayer:
    case DamagePath::kHeal:
    case DamagePath::kWords:
      return ro::ui::EntitySampleShown{.bars = ro::ui::EntityBarKind::kOwn};
    case DamagePath::kOnMonster:
    case DamagePath::kSkillOnMonster:
    case DamagePath::kCritical:
    case DamagePath::kTotal:
    case DamagePath::kCount:
      break;
  }
  return ro::ui::EntitySampleShown{.bars = ro::ui::EntityBarKind::kMonster};
}

HitPopups::SampleGround SampleGroundOf(const ro::render::Camera& camera, float aspect_ratio,
                                       float viewport_width, float viewport_height,
                                       const glm::vec3& ground, float sprite_scale,
                                       float body_degrees) {
  HitPopups::SampleGround sampled;
  sampled.body_degrees = body_degrees;
  // Sans projection, un sol vu de face : l'est à droite, le nord invisible.
  sampled.per_world_x = glm::vec2{ro::render::kSpritePixelsPerWorldUnit, 0.0F};
  sampled.per_world_z = glm::vec2{0.0F, 0.0F};
  const auto project = [&](const glm::vec3& point) {
    return ro::render::ProjectToScreen(camera, aspect_ratio, point, viewport_width,
                                       viewport_height);
  };
  const std::optional<glm::vec2> origin = project(ground);
  const std::optional<glm::vec2> east = project(ground + glm::vec3{1.0F, 0.0F, 0.0F});
  const std::optional<glm::vec2> north = project(ground + glm::vec3{0.0F, 0.0F, 1.0F});
  if (sprite_scale > 0.0F && origin.has_value() && east.has_value() && north.has_value()) {
    sampled.per_world_x = (*east - *origin) / sprite_scale;
    sampled.per_world_z = (*north - *origin) / sprite_scale;
  }
  return sampled;
}

ro::ui::PathSample DamagePathSampler::Compose(const std::optional<HitPopups>& game_numbers,
                                              const HitPopupLook& look, DamagePathStage stage,
                                              std::uint32_t now_ms) {
  ro::ui::PathSample sample;
  sample.target = std::move(stage.target);
  sample.sprite_scale = stage.sprite_scale;
  sample.font = stage.font;
  if (!game_numbers.has_value()) {
    return sample;
  }
  const DamagePath path = DamagePathOf(look.path_shown);
  const HitPopupLook shown = Showing(look);
  // UN AUTRE TYPE REPART À VIDE, et tout de suite : les nombres du précédent
  // suivraient une trajectoire qu'on ne regarde plus.
  if (!numbers_.has_value() || path != shown_) {
    numbers_.emplace(game_numbers->Twin());
    shown_ = path;
    next_spawn_ms_ = now_ms;
  }
  if (now_ms >= next_spawn_ms_) {
    numbers_->SpawnSample(path, now_ms, shown);
    next_spawn_ms_ = now_ms + RehearsalStepMs(shown);
  }
  TraceAllure(path, shown, stage.ground, sample);

  const std::optional<ro::render::GlyphBox>& digit = numbers_->digit_box();
  for (const HitPopups::SampledNumber& number : numbers_->Sampled(now_ms, shown, stage.ground)) {
    if (number.frame != nullptr) {
      const ro::render::ComposedActor& frame = *number.frame;
      const std::size_t count = std::min(frame.quads.size(), frame.sources.size());
      for (std::size_t rank = 0; rank < count; ++rank) {
        const ImTextureID texture = TextureOf(frame.sources[rank]);
        if (texture == 0) {
          continue;
        }
        const ro::render::SpriteQuad& quad = frame.quads[rank];
        ro::ui::PathSampleQuad painted;
        painted.texture = texture;
        for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
          const glm::vec2 at = quad.corners[corner] + number.drift_pixels;
          painted.corners[corner] = ImVec2(at.x, at.y);
        }
        painted.mirrored = quad.mirrored;
        painted.tint = IM_COL32(quad.tint[0], quad.tint[1], quad.tint[2], quad.tint[3]);
        sample.quads.push_back(painted);
      }
    }
    if (number.text.has_value() && digit.has_value()) {
      sample.texts.push_back(
          TextInSprite(*number.text, *digit, look.font_outline_pixels, number.drift_pixels));
    }
  }
  return sample;
}

ImTextureID DamagePathSampler::TextureOf(const ro::render::SpriteImageKey& key) {
  auto found = textures_.find(key);
  if (found == textures_.end()) {
    // ⚠ UN ÉCHEC SE RETIENT AUSSI, en texture vide : une image qui ne se
    // téléverse pas ne se retente pas à chaque image.
    ro::render::GlTexture uploaded;
    const ro::formats::Image image = ro::render::ResolveImage(key);
    if (!image.empty()) {
      if (auto texture =
              ro::render::GlTexture::Upload(image, ro::render::GlTexture::Filtering::kSprite);
          texture) {
        uploaded = std::move(*texture);
      }
    }
    found = textures_.emplace(key, std::move(uploaded)).first;
  }
  return static_cast<ImTextureID>(found->second.name());
}

void DamagePathSampler::TraceAllure(DamagePath path, const HitPopupLook& look,
                                    const HitPopups::SampleGround& ground,
                                    ro::ui::PathSample& sample) const {
  sample.birth = ImVec2(0.0F, 0.0F);
  sample.allure.clear();
  if (!numbers_.has_value()) {
    return;
  }
  // CE QUI DÉPLACE LE NOMBRE HORS DE SA TRAJECTOIRE : toutes les trajectoires à
  // zéro, mais la dérive de côté et la vie gardées — la montée propre d'un mot
  // aussi. ⚠ ET LES CHIFFRES EN POLICE : leur milieu se lit sur le texte, alors
  // que la boîte d'un cadre de sprite bougerait avec l'étoile d'un critique.
  HitPopupLook plain = look;
  for (NumberPath& traced : plain.paths) {
    traced = NumberPath{.facing_drift = traced.facing_drift};
  }
  plain.font_text = true;
  HitPopups probe = numbers_->Twin();
  probe.SpawnSample(path, 0, plain);
  const std::optional<ro::render::GlyphBox>& digit = probe.digit_box();
  // Le milieu du PREMIER nombre de ce type à chaque pas, y montant, jusqu'à ce
  // qu'il s'éteigne.
  std::vector<glm::vec2> middles;
  for (std::uint32_t now_ms = 0; now_ms <= kLongestTracedLifeMs; now_ms += kTraceStepMs) {
    std::optional<glm::vec2> middle;
    for (const HitPopups::SampledNumber& number : probe.Sampled(now_ms, plain, ground)) {
      if (number.path != path || number.born_ms != 0) {
        continue;
      }
      if (number.text.has_value() && digit.has_value()) {
        middle = number.text->landing_pixels +
                 glm::vec2{0.0F, digit->middle_y_pixels * number.text->scale};
      } else if (number.frame != nullptr) {
        middle = MiddleOf(*number.frame);
      }
      if (middle.has_value()) {
        *middle += number.drift_pixels;
      }
      break;
    }
    if (!middle.has_value()) {
      break;
    }
    middles.emplace_back(middle->x, -middle->y);
  }
  if (middles.empty()) {
    return;
  }
  sample.birth = ImVec2(middles.front().x, middles.front().y);
  // RÉÉCHANTILLONNÉ À PARTS ÉGALES DE LA VIE : c'est la progression que suit le
  // chemin vers le point d'arrivée, et le cadre les additionne rang à rang.
  const auto last = static_cast<float>(middles.size() - 1);
  for (std::size_t rank = 0; rank < ro::ui::kAllureSamples; ++rank) {
    const float at =
        last * static_cast<float>(rank) / static_cast<float>(ro::ui::kAllureSamples - 1);
    const auto below = static_cast<std::size_t>(at);
    const std::size_t above = std::min(below + 1, middles.size() - 1);
    const float share = at - static_cast<float>(below);
    const glm::vec2 point = middles[below] + (middles[above] - middles[below]) * share;
    sample.allure.emplace_back(point.x - sample.birth.x, point.y - sample.birth.y);
  }
}

}  // namespace moonclient
