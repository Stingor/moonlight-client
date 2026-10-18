#include "world/monster_sprite.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/formats/act.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/vfs/encoding.h"

namespace moonclient {
namespace {

// ── LE RYTHME DE LA CHATOUILLE ──────────────────────────────────────────────
//
// Le temps mort entre deux clics fixe aussi la cadence de l'escalade, d'où des
// seuils courts : six clics font douze secondes, et personne ne clique une
// minute pour voir ce qui se passe.
constexpr std::uint32_t kPokeCooldownMs = 2000;
// Une chatouille sur trois, il riposte…
constexpr int kPokesPerRiposte = 3;
// …et à la sixième, il fait le mort.
constexpr int kPokesBeforeDeath = 6;
// Le temps passé à terre avant de se relever.
constexpr std::uint32_t kDeathHoldMs = 1600;
// Après s'être relevé, un souffle avant le clic suivant.
constexpr std::uint32_t kGetUpBreathMs = 300;
// Une réaction d'une seule image resterait invisible : elle dure au moins ça.
constexpr std::uint32_t kLeastReactionMs = 200;

// ── LE SURSAUT ──────────────────────────────────────────────────────────────
// Une oscillation horizontale qui s'éteint avec l'animation.
constexpr float kShakeRadiansPerSecond = 47.0F;
constexpr float kShakeAmplitudePx = 3.5F;
constexpr float kMillisecondsPerSecond = 1000.0F;

// Le son de COUP du jeu, celui qu'on entend en tapant un monstre : le repli
// quand le .act ne déclare aucun wav pour sa réaction — le cas de la plupart.
constexpr const char* kPokeFallbackWav = "effect\\EF_hit2.wav";
constexpr std::string_view kWavSuffix = ".wav";

// Le nombre d'orientations d'un .act, et l'angle qui les sépare.
constexpr int kDirections = 8;
constexpr float kRadiansPerDirection = 0.7853982F;

// Ce que l'action a à faire entendre, et QUAND.
enum class PokeAudio : std::uint8_t {
  // Un vrai wav dans le .act : le dessin le déclenchera, image par image.
  kFrameWav,
  // Pas de wav, mais un marqueur de coup : le repli partira sur SON image.
  kOnMarker,
  // Ni l'un ni l'autre : le repli part tout de suite, sinon le clic est muet.
  kNow,
};

// LE .ACT DONNE SES OCTETS BRUTS, en CP949. ⚠ PAS LA VARIANTE TOLÉRANTE : son
// point d'interrogation ferait une clé qui ne désigne plus rien.
[[nodiscard]] std::string FromClientBytes(std::string_view cp949) {
  auto utf8 = ro::vfs::Cp949ToUtf8(cp949);
  return utf8 ? std::move(*utf8) : std::string(cp949);
}

// Le son, ou le marqueur, que porte une image ; vide pour une image muette.
//
// ⚠ LA TABLE DES SONS PORTE AUSSI DES MARQUEURS SANS EXTENSION — l'image où le
// coup porte. Ce n'est PAS un son.
[[nodiscard]] std::string_view FrameCue(const ro::formats::ActAnimation& animation,
                                        const ro::formats::ActFrame& frame) {
  if (frame.sound_index < 0) {
    return {};
  }
  const auto index = static_cast<std::size_t>(frame.sound_index);
  const std::vector<std::string>& files = animation.sound_files();
  return index < files.size() ? std::string_view(files[index]) : std::string_view();
}

[[nodiscard]] PokeAudio ClassifyAudio(const ro::formats::ActAnimation& animation,
                                      const ro::formats::ActAction& action) {
  bool marker = false;
  for (const ro::formats::ActFrame& frame : action.frames) {
    const std::string_view cue = FrameCue(animation, frame);
    if (ro::core::AsciiEndsWithFolded(cue, kWavSuffix)) {
      return PokeAudio::kFrameWav;
    }
    marker = marker || !cue.empty();
  }
  return marker ? PokeAudio::kOnMarker : PokeAudio::kNow;
}

// La dernière image de l'action qui dessine quelque chose.
[[nodiscard]] std::size_t LastVisibleFrame(const ro::formats::ActAction& action) {
  for (std::size_t frame = action.frames.size(); frame > 1; --frame) {
    const std::vector<ro::formats::ActLayer>& layers = action.frames[frame - 1].layers;
    const bool drawn = std::ranges::any_of(
        layers, [](const ro::formats::ActLayer& layer) { return layer.frame_index >= 0; });
    if (drawn) {
      return frame - 1;
    }
  }
  return 0;
}

}  // namespace

void MonsterSprite::Show(std::uint32_t sprite_class, std::uint16_t pet_accessory) {
  if (sprite_class == sprite_class_ && pet_accessory == pet_accessory_) {
    return;
  }
  sprite_class_ = sprite_class;
  pet_accessory_ = pet_accessory;
  idle_scale_ = 0.0F;
  poke_.reset();
  poke_frozen_ = false;
  // ⚠ LE TEMPS MORT REPART AUSSI : il est absolu, et le garder ferait taire le
  // nouveau monstre.
  poke_ready_at_ms_ = 0;
  poke_count_ = 0;
  hit_pending_ = false;
}

const ro::formats::ActAnimation* MonsterSprite::BodyAnimation() {
  ro::render::ActorLook look;
  look.job = sprite_class_;
  look.pet_accessory = pet_accessory_;
  if (sprite_class_ == 0 || ro::render::DrawnAsModel(wardrobe_->catalog(), look)) {
    return nullptr;
  }
  const ro::render::ActorOutfit* const outfit = wardrobe_->OutfitFor(look);
  if (outfit == nullptr || outfit->pieces().empty()) {
    return nullptr;
  }
  return outfit->pieces().front().animation;
}

MonsterSprite::Pose MonsterSprite::PoseFor(std::size_t animation, int direction) {
  const auto facing =
      static_cast<std::size_t>(((direction % kDirections) + kDirections) % kDirections);
  const ro::formats::ActAnimation* const body = BodyAnimation();
  if (body == nullptr) {
    return {};
  }
  const auto carries = [&](std::size_t rank) {
    const std::size_t action = ro::render::ActionIndex(rank, facing);
    return action < body->actions().size() && !body->actions()[action].frames.empty();
  };
  if (carries(animation)) {
    return Pose{.animation = animation, .direction = facing};
  }
  if (carries(ro::render::kAnimationIdle)) {
    return Pose{.animation = ro::render::kAnimationIdle, .direction = facing};
  }
  return {};
}

const ro::formats::ActAction* MonsterSprite::ActionOf(const Pose& pose) {
  const ro::formats::ActAnimation* const body = BodyAnimation();
  if (body == nullptr) {
    return nullptr;
  }
  const std::size_t action = ro::render::ActionIndex(pose.animation, pose.direction);
  return action < body->actions().size() ? &body->actions()[action] : nullptr;
}

void MonsterSprite::Poke(int direction, std::uint32_t now_ms) {
  if (now_ms < poke_ready_at_ms_) {
    return;
  }
  // UN ACTEUR 3D N'A PAS D'ACTION À JOUER : il sursaute, et le coup du jeu
  // sonne tout de suite.
  if (is_model_) {
    poke_ = Pose{};
    poke_start_ms_ = now_ms;
    poke_anim_end_ms_ = now_ms + kLeastReactionMs;
    poke_end_ms_ = poke_anim_end_ms_;
    poke_frozen_ = false;
    poke_ready_at_ms_ = std::max(now_ms + kPokeCooldownMs, poke_end_ms_ + kGetUpBreathMs);
    sounds_.emplace_back(kPokeFallbackWav);
    return;
  }
  const ro::formats::ActAnimation* const body = BodyAnimation();
  if (body == nullptr) {
    return;
  }
  ++poke_count_;

  // 🔴 IL NE MEURT QUE SI SON .ACT A VRAIMENT UNE MORT : la pose retombe en
  // silence sur l'attente quand la motion manque, et on aurait un monstre
  // « mort » debout, figé une seconde et demie.
  const Pose dying = PoseFor(ro::render::kCreatureAnimationDie, direction);
  const bool dies =
      dying.animation == ro::render::kCreatureAnimationDie && poke_count_ >= kPokesBeforeDeath;
  Pose pose;
  if (dies) {
    pose = dying;
    // Il repart intact en se relevant.
    poke_count_ = 0;
  } else {
    pose = PoseFor(poke_count_ % kPokesPerRiposte == 0 ? ro::render::kCreatureAnimationAttack
                                                       : ro::render::kCreatureAnimationHurt,
                   direction);
  }
  const ro::formats::ActAction* const action = ActionOf(pose);
  if (action == nullptr) {
    return;
  }

  // LA DURÉE EST CELLE DU .ACT, pas une constante : l'animation de dégât va de
  // deux à une dizaine d'images, à des cadences sans rapport.
  const std::size_t frames = std::max<std::size_t>(1, action->frames.size());
  const auto length_ms = std::max(
      kLeastReactionMs,
      static_cast<std::uint32_t>(static_cast<float>(frames) * action->frame_interval_ms()));

  poke_ = pose;
  poke_start_ms_ = now_ms;
  poke_anim_end_ms_ = now_ms + length_ms;
  poke_frozen_ = dies;
  poke_end_ms_ = poke_anim_end_ms_ + (dies ? kDeathHoldMs : 0);
  poke_freeze_frame_ = LastVisibleFrame(*action);
  // On ne tape pas un monstre à terre : la reprise se fait après qu'il s'est
  // relevé, pas pendant.
  poke_ready_at_ms_ = std::max(now_ms + kPokeCooldownMs, poke_end_ms_ + kGetUpBreathMs);
  // ⚠ AUCUNE IMAGE ENCORE JOUÉE : la toute première doit compter comme un
  // CHANGEMENT, sinon le son qu'elle porte serait sauté.
  poke_frame_.reset();

  hit_pending_ = false;
  switch (ClassifyAudio(*body, *action)) {
    case PokeAudio::kFrameWav:
      break;
    case PokeAudio::kOnMarker:
      hit_pending_ = true;
      break;
    case PokeAudio::kNow:
      sounds_.emplace_back(kPokeFallbackWav);
      break;
  }
}

void MonsterSprite::HearFrame(const ro::formats::ActAction& action, std::size_t frame) {
  if (poke_frame_ == frame || frame >= action.frames.size()) {
    return;
  }
  poke_frame_ = frame;
  const ro::formats::ActAnimation* const body = BodyAnimation();
  if (body == nullptr) {
    return;
  }
  const std::string_view cue = FrameCue(*body, action.frames[frame]);
  if (ro::core::AsciiEndsWithFolded(cue, kWavSuffix)) {
    sounds_.push_back(FromClientBytes(cue));
  } else if (hit_pending_ && !cue.empty()) {
    // L'action muette mais marquée : le son de coup part sur l'image du
    // contact, une seule fois par chatouille.
    hit_pending_ = false;
    sounds_.emplace_back(kPokeFallbackWav);
  }
}

float MonsterSprite::ShakeAt(std::uint32_t now_ms) const {
  // Le sursaut s'arrête AVEC l'animation : un corps à terre ne tremble plus.
  if (!poke_.has_value() || now_ms >= poke_anim_end_ms_) {
    return 0.0F;
  }
  const auto elapsed_ms = static_cast<float>(now_ms - poke_start_ms_);
  const auto length = static_cast<float>(poke_anim_end_ms_ - poke_start_ms_);
  const float left = static_cast<float>(poke_anim_end_ms_ - now_ms) / length;
  return std::sin(elapsed_ms / kMillisecondsPerSecond * kShakeRadiansPerSecond) *
         kShakeAmplitudePx * left;
}

MonsterSprite::Shown MonsterSprite::ComposeModel(const Framing& framing, std::uint32_t now_ms) {
  Shown shown;
  shown.model = true;
  if (!model_.has_value() || framing.box_width <= 0 || framing.box_height <= 0) {
    shown.missing = true;
    return shown;
  }
  // L'horloge de l'attente, figée à zéro si le réglage le veut ; celle de la
  // réaction, qui repart du clic.
  std::uint32_t clock_ms = framing.animate ? now_ms : 0;
  if (poke_.has_value()) {
    clock_ms = now_ms - poke_start_ms_;
    shown.shake_px = ShakeAt(now_ms);
  }
  // ⚠ L'ORIENTATION EST CONTINUE, un huitième de tour par cran : c'est la même
  // mémoire que celle d'un sprite, pour qu'un aller-retour entre un monstre 2D
  // et un 3D garde le même angle.
  const std::string resource =
      ro::render::NonPlayerResourceName(wardrobe_->catalog(), sprite_class_);
  const ro::render::IconTexture* const frame =
      model_->FrameAt(MonsterModel::Framing{.resource_name = resource,
                                            .box_width = framing.box_width,
                                            .box_height = framing.box_height,
                                            .yaw_radians = static_cast<float>(framing.direction) *
                                                           kRadiansPerDirection},
                      clock_ms);
  if (frame == nullptr) {
    shown.missing = true;
    return shown;
  }
  shown.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(frame->texture.name()),
                                  .width = static_cast<float>(framing.box_width),
                                  .height = static_cast<float>(framing.box_height)};
  return shown;
}

MonsterSprite::Shown MonsterSprite::Compose(const Framing& framing, std::uint32_t now_ms) {
  Show(framing.sprite_class, framing.pet_accessory);
  Shown shown;
  ro::render::ActorLook look;
  look.job = sprite_class_;
  look.pet_accessory = pet_accessory_;
  if (sprite_class_ == 0) {
    shown.missing = true;
    return shown;
  }
  is_model_ = ro::render::DrawnAsModel(wardrobe_->catalog(), look);
  if (poke_.has_value() && now_ms >= poke_end_ms_) {
    poke_.reset();
  }
  if (is_model_) {
    return ComposeModel(framing, now_ms);
  }
  if (BodyAnimation() == nullptr) {
    shown.missing = true;
    return shown;
  }
  if (framing.box_width <= 0 || framing.box_height <= 0) {
    return shown;
  }

  ro::render::ActorVignette::Pose pose;
  pose.look = look;
  const Pose idle = PoseFor(ro::render::kAnimationIdle, framing.direction);
  pose.animation = idle.animation;
  pose.direction = idle.direction;
  pose.animated = framing.animate;
  pose.elapsed_ms = static_cast<float>(now_ms);
  // ⚠ AJUSTÉE AU CADRE, petit monstre compris : il remplit sa case au lieu de
  // s'y perdre — sauf au cadrage centré, qui ne fait que réduire. L'enveloppe
  // mesurée couvre toutes les images de l'action, si bien que la taille ne bat
  // pas au rythme de l'attente.
  pose.box.width = framing.box_width;
  pose.box.height = framing.box_height;
  pose.box.fit = framing.fit;

  // ── LA RÉACTION PRIME SUR L'ATTENTE ───────────────────────────────────────
  //
  // Elle s'anime même quand l'attente est figée : c'est une réaction, pas une
  // décoration. Son horloge repart du clic, et elle ne se joue qu'une fois.
  if (poke_.has_value()) {
    if (const ro::formats::ActAction* const action = ActionOf(*poke_); action != nullptr) {
      const auto elapsed_ms = static_cast<float>(now_ms - poke_start_ms_);
      // ⚠ LA MORT SE TIENT : passé l'animation, l'image du corps à terre reste,
      // sans quoi le cadavre se relèverait tout seul, en boucle.
      const std::size_t frame = poke_frozen_ && now_ms >= poke_anim_end_ms_
                                    ? poke_freeze_frame_
                                    : ro::render::FrameAtTimeOnce(*action, elapsed_ms);
      pose.animation = poke_->animation;
      pose.direction = poke_->direction;
      pose.held_frame = frame;
      pose.box.scale = idle_scale_;
      HearFrame(*action, frame);
      shown.shake_px = ShakeAt(now_ms);
    }
  }

  if (!vignette_.has_value()) {
    vignette_.emplace(*wardrobe_);
  }
  const ro::render::IconTexture& drawn = vignette_->Get(pose);
  if (!poke_.has_value() && vignette_->framing().usable()) {
    idle_scale_ = vignette_->framing().scale;
  }
  if (drawn.usable()) {
    shown.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                    .width = static_cast<float>(framing.box_width),
                                    .height = static_cast<float>(framing.box_height)};
  }
  return shown;
}

std::vector<std::string> MonsterSprite::TakeSounds() { return std::exchange(sounds_, {}); }

void MonsterSprite::Forget() {
  vignette_.reset();
  if (model_.has_value()) {
    model_->Forget();
  }
  is_model_ = false;
  sprite_class_ = 0;
  pet_accessory_ = 0;
  idle_scale_ = 0.0F;
  poke_.reset();
  poke_frozen_ = false;
  poke_ready_at_ms_ = 0;
  poke_count_ = 0;
  hit_pending_ = false;
  sounds_.clear();
}

}  // namespace moonclient
