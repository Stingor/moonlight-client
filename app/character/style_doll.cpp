#include "character/style_doll.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_frame.h"

namespace moonclient {
namespace {

// ── LA PULSATION DE LA PIÈCE SURVOLÉE ───────────────────────────────────────
//
// 🔴 ON PULSE LA LUMINOSITÉ, PAS LA TRANSPARENCE. Un sprite RO n'a qu'un index
// transparent — le zéro —, et toucher l'alpha des autres les ferait
// disparaître. Un éclaircissement se lit de toute façon mieux sur un fond
// chargé.
//
// 🔴🔴 ET LE NIVEAU EST QUANTIFIÉ, ce qui commande tout le reste : chaque palier
// a sa palette, son style et sa vignette. Quatre suffisent à l'œil.
constexpr int kPulseFloor = 60;
constexpr int kPulseStride = 55;
// Deux allers-retours par seconde.
constexpr float kPulsePerSecond = 2.0F;
constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kPi = 3.14159265F;
constexpr int kFullValue = 255;
constexpr std::size_t kColorChannels = 3;

// L'image que le pantin TIENT : la première, comme le cadrage la dessine. ⚠ LA
// MÊME QUE CELLE DU DESSIN, sinon la pipette interrogerait une pose que
// personne ne regarde.
constexpr std::size_t kHeldFrame = 0;

// Le palier de la pulsation à cet instant.
[[nodiscard]] std::size_t PulseStep(std::uint32_t now_ms, std::size_t steps) {
  const float seconds = static_cast<float>(now_ms) / kMillisecondsPerSecond * kPulsePerSecond;
  // Une onde douce de zéro à un, puis des paliers.
  const float wave = 0.5F - 0.5F * std::cos(seconds * kPi);
  return static_cast<std::size_t>(wave * static_cast<float>(steps - 1) + 0.5F);
}

// Éclaircit d'un bloc les index d'une pièce : la pulsation doit se voir sur
// toute la pièce, ce qu'aucun réglage de rampe — qui garde les écarts du
// dégradé — n'exprime.
void Lighten(ro::formats::Palette& palette, const ro::render::PaletteRamp& ramp, std::size_t step) {
  const int force = kPulseFloor + static_cast<int>(step) * kPulseStride;
  const std::size_t end = std::min<std::size_t>(static_cast<std::size_t>(ramp.start) + ramp.length,
                                                ro::formats::kPaletteEntryCount);
  for (std::size_t index = ramp.start; index < end; ++index) {
    for (std::size_t channel = 0; channel < kColorChannels; ++channel) {
      const int lit = palette.entries[index][channel] + force;
      palette.entries[index][channel] = static_cast<std::uint8_t>(std::min(lit, kFullValue));
    }
  }
}

}  // namespace

StyleDoll::StyleDoll(ro::render::ActorWardrobe& wardrobe) : wardrobe_(&wardrobe) {}

const ro::render::ActorWardrobe::BodyRamps& StyleDoll::Measure(const ro::render::ActorLook& look,
                                                               std::uint32_t body_key) {
  if (!measured_ || body_key != body_key_ || look.body_palette != measured_palette_) {
    body_ = wardrobe_->BodyRampsFor(look);
    body_key_ = body_key;
    measured_palette_ = look.body_palette;
    measured_ = true;
    ++generation_;
  }
  return body_;
}

void StyleDoll::Forget(Painted& painted) {
  if (painted.style != 0) {
    wardrobe_->ForgetStyle(painted.style);
  }
  painted.style = 0;
}

void StyleDoll::ForgetPulse() {
  for (Painted& step : pulse_) {
    Forget(step);
  }
  pulse_ramp_ = -1;
}

void StyleDoll::ForgetPainted() {
  Forget(plain_);
  ForgetPulse();
}

const ro::render::IconTexture* StyleDoll::Draw(const Shot& shot) {
  shown_ = nullptr;
  if (shot.recipe == nullptr || body_.ramps.empty() || shot.box.width <= 0 ||
      shot.box.height <= 0) {
    return nullptr;
  }
  // ⚠ LES PALETTES PEINTES VALENT POUR UNE MESURE ET UNE RÉVISION : peintes sur
  // l'ancienne base, elles porteraient des couleurs calculées sur un découpage
  // qui n'existe plus.
  if (painted_generation_ != generation_ || painted_revision_ != shot.revision) {
    ForgetPainted();
    painted_generation_ = generation_;
    painted_revision_ = shot.revision;
  }
  const bool pulses = shot.highlight >= 0 && shot.highlight < static_cast<int>(body_.ramps.size());
  if (pulses && shot.highlight != pulse_ramp_) {
    ForgetPulse();
    pulse_ramp_ = shot.highlight;
  }
  const std::size_t step = pulses ? PulseStep(shot.now_ms, kPulseSteps) : 0;
  Painted& painted = pulses ? pulse_[step] : plain_;

  if (painted.style == 0) {
    ro::formats::Palette palette = ro::render::ApplyRecipe(body_.base, body_.ramps, *shot.recipe);
    if (pulses) {
      Lighten(palette, body_.ramps[static_cast<std::size_t>(shot.highlight)], step);
    }
    // 🔴 UN IDENTIFIANT NEUF : la garde-robe refuse de réécrire une palette en
    // place, parce qu'une tenue déjà composée en retient l'ADRESSE — la
    // repeindre ne se verrait qu'à la prochaine éviction.
    painted.style = wardrobe_->NewStyleId();
    if (!wardrobe_->SetStylePalette(painted.style, std::move(palette))) {
      painted.style = 0;
      return nullptr;
    }
  }
  if (!painted.vignette.has_value()) {
    painted.vignette.emplace(*wardrobe_);
  }

  ro::render::ActorVignette::Pose pose;
  pose.look = shot.look;
  pose.look.style = painted.style;
  // Debout, figé : un aperçu de couleur n'a pas à s'agiter.
  pose.animation = ro::render::kAnimationIdle;
  pose.direction = shot.direction;
  pose.animated = false;
  pose.box = shot.box;
  const ro::render::IconTexture& drawn = painted.vignette->Get(pose);
  if (!drawn.usable()) {
    return nullptr;
  }
  shown_ = &painted;
  shown_look_ = pose.look;
  shown_direction_ = shot.direction;
  return &drawn;
}

std::optional<std::size_t> StyleDoll::RampUnder(glm::vec2 in_box) {
  if (shown_ == nullptr || !shown_->vignette.has_value() || body_.ramps.empty()) {
    return std::nullopt;
  }
  // ⚠ LE CADRAGE VIENT DE LA VIGNETTE ELLE-MÊME : le refaire ici le ferait
  // diverger du dessin un jour, et la pipette désignerait alors la pièce d'à
  // côté sans que rien ne le signale.
  const ro::render::VignetteFraming& framing = shown_->vignette->framing();
  if (!framing.usable()) {
    return std::nullopt;
  }
  // ⚠ LA MÊME APPARENCE QUE CELLE DESSINÉE, style compris : c'est la
  // composition affichée qu'on interroge, pas une autre.
  ro::render::ActorOutfit* outfit = wardrobe_->OutfitFor(shown_look_);
  if (outfit == nullptr) {
    return std::nullopt;
  }
  const ro::render::ComposedActor& composed = outfit->Composed(
      ro::render::kAnimationIdle, shown_direction_, 0.0F, /*loop=*/true, kHeldFrame);
  // L'INVERSE DU CADRAGE : une soustraction et une division, l'origine et
  // l'échelle étant en pixels de la boîte.
  const glm::vec2 at((in_box.x - framing.origin.x) / framing.scale,
                     (in_box.y - framing.origin.y) / framing.scale);
  const std::optional<ro::render::PalettePick> pick = ro::render::PaletteIndexAt(composed, at);
  if (!pick.has_value() || pick->part != ro::render::ActorPart::kBody) {
    return std::nullopt;
  }
  // ⚠ UN INDEX HORS DE TOUTE RAMPE EST UNE VRAIE RÉPONSE : c'est la part du
  // corps qu'aucun réglage n'atteint, celle qu'annonce « Réglable : N % ».
  return ro::render::RampAt(body_.ramps, pick->index);
}

void StyleDoll::Close() {
  ForgetPainted();
  // ⚠ LES TEXTURES AUSSI : un pantin refermé n'en vaut aucune.
  plain_.vignette.reset();
  for (Painted& step : pulse_) {
    step.vignette.reset();
  }
  shown_ = nullptr;
}

}  // namespace moonclient
