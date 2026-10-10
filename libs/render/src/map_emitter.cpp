#include "ro/render/map_emitter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace ro::render {
namespace {

// Où naît une particule par rapport à son écart tiré : DIX UNITÉS PLUS HAUT,
// l'ordonnée du client descendant.
constexpr float kBirthLift = 10.0F;

// Le plafond d'alpha, et le nombre de pas par seconde qui sert à comparer une
// durée de vie en secondes à un âge en pas.
constexpr std::int32_t kOpaqueAlpha = 255;
constexpr double kStepsPerSecond = 60.0;
// 🔴 LE FONDU DURE UNE SECONDE : il monte tant qu'il reste au moins ce nombre
// de pas à vivre, et descend ensuite.
constexpr float kFadeSteps = 60.0F;
constexpr double kAlphaScale = 255.0;

// Une composante qui vaut ceci n'est PAS recopiée : le client la saute, et la
// particule garde la sienne.
constexpr std::int16_t kUnchangedChannel = -1;

// Les mélanges d'une particule neuve, ceux de son constructeur, qu'un émetteur
// réglé à −1 lui laisse.
constexpr std::int32_t kParticleBlendSource = 5;
constexpr std::int32_t kParticleBlendDestination = 2;
constexpr std::int32_t kUnsetBlend = -1;

// LE QUAD D'UNE PARTICULE, posé par son constructeur : un carré de ±1 à UNE
// unité de profondeur, dans l'ordre de la bande de triangles du client.
constexpr std::array<glm::vec3, 4> kUnitQuad = {
    glm::vec3{-1.0F, 1.0F, 1.0F}, glm::vec3{1.0F, 1.0F, 1.0F}, glm::vec3{-1.0F, -1.0F, 1.0F},
    glm::vec3{1.0F, -1.0F, 1.0F}};
// 🔴 LE HAUT DE L'IMAGE VA AU COIN DU BAS : le premier coin est à l'ordonnée
// +1, qui DESCEND, et c'est lui qui reçoit la première ligne de la texture. Le
// client peint donc ses particules tête en bas — ce qui ne se voit que sur une
// image qui a un sens.
constexpr std::array<glm::vec2, 4> kUnitQuadUv = {glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 0.0F},
                                                  glm::vec2{0.0F, 1.0F}, glm::vec2{1.0F, 1.0F}};
// Notre renderer prend ses coins EN TOUR ; la bande du client passe par 0, 1,
// 3 puis 2.
constexpr std::array<std::size_t, 4> kStripToCycle = {0, 1, 3, 2};

// Le masque d'un octet de couleur.
constexpr std::uint32_t kByteMask = 0xFFU;
constexpr int kByteBits = 8;

// LA COULEUR D'UN SOMMET, composée comme chez le client : les trois composantes
// sont des mots SIGNÉS, étendus avant d'être décalés. Une composante hors de
// zéro à 255 déborde donc sur ses voisines, comme chez le client.
[[nodiscard]] std::array<std::uint8_t, 4> PackedTint(const std::array<std::int16_t, 3>& rgb,
                                                     std::int32_t alpha) {
  const auto red = static_cast<std::uint32_t>(static_cast<std::int32_t>(rgb[0]));
  const auto green = static_cast<std::uint32_t>(static_cast<std::int32_t>(rgb[1]));
  const auto blue = static_cast<std::uint32_t>(static_cast<std::int32_t>(rgb[2]));
  const std::uint32_t packed =
      blue | ((green | ((red | (static_cast<std::uint32_t>(alpha) << kByteBits)) << kByteBits))
              << kByteBits);
  return {static_cast<std::uint8_t>((packed >> (2 * kByteBits)) & kByteMask),
          static_cast<std::uint8_t>((packed >> kByteBits) & kByteMask),
          static_cast<std::uint8_t>(packed & kByteMask),
          static_cast<std::uint8_t>((packed >> (3 * kByteBits)) & kByteMask)};
}

}  // namespace

MapEmitter::MapEmitter(glm::vec3 native_position, MapEmitterSetting setting, std::uint32_t& draws)
    : position_(native_position), setting_(std::move(setting)) {
  // Toute la réserve d'un coup, chaque particule née puis rendue LIBRE.
  particles_.resize(static_cast<std::size_t>(std::max(0, setting_.max_count)));
  for (Particle& particle : particles_) {
    particle.position = position_;
    InitParticle(particle, draws);
  }

  // Le réglage de l'émetteur, que la table déclenche en dernier. Un objet
  // qu'aucune entrée ne règle ne la reçoit pas, et n'en diffère en rien : ses
  // défauts ne sont pas éternels, et la naissance est la même. 🔴 UNE ÉTERNELLE
  // SANS CADENCE NAÎT TOUT ENTIÈRE : la réserve est déclarée pleine d'un coup,
  // et rien d'autre ne la remplira.
  if (setting_.eternal && setting_.rate[0] == 0.0F && setting_.rate[1] == 0.0F) {
    for (Particle& particle : particles_) {
      particle.used = true;
    }
    used_ = particles_.size();
  }
  for (Particle& particle : particles_) {
    InitParticle(particle, draws);
    if (particle.used &&
        (particle.life_seconds == 0.0F ||
         static_cast<float>(static_cast<double>(particle.life_seconds) * kStepsPerSecond) >
             static_cast<float>(particle.age_steps) ||
         setting_.eternal)) {
      particle.alpha = setting_.color[3];
    }
  }
}

void MapEmitter::InitParticle(Particle& particle, std::uint32_t& draws) const {
  particle.position = position_;
  // ⚠ L'ORDRE DES TIRAGES EST CELUI DU CLIENT : la vie, la taille, la vitesse
  // axe par axe, puis l'écart axe par axe.
  particle.life_seconds = EffectToolRandomBetween(draws, setting_.life[0], setting_.life[1]);
  particle.size = EffectToolRandomBetween(draws, setting_.size[0], setting_.size[1]);
  particle.velocity = {
      EffectToolRandomBetween(draws, setting_.first_direction.x, setting_.second_direction.x),
      EffectToolRandomBetween(draws, setting_.first_direction.y, setting_.second_direction.y),
      EffectToolRandomBetween(draws, setting_.first_direction.z, setting_.second_direction.z)};
  const float x = EffectToolRandomBetween(draws, -setting_.radius.x, setting_.radius.x);
  const float y =
      EffectToolRandomBetween(draws, -setting_.radius.y, setting_.radius.y) - kBirthLift;
  const float z = EffectToolRandomBetween(draws, -setting_.radius.z, setting_.radius.z);
  particle.offset = {x, y, z};
  particle.alpha = 0;
  particle.age_steps = 0;
}

void MapEmitter::Tick(glm::vec3 native_player, std::uint32_t& draws) {
  // ⚠ LOIN, IL NE SE CACHE QU'UNE FOIS SA RÉSERVE VIDE : ses particules
  // vivantes finissent leur vie.
  ticked_ = true;
  near_ = kMapEffectReachUnits > glm::distance(native_player, position_);
  if (near_) {
    hidden_ = false;
  } else if (used_ == 0) {
    hidden_ = true;
  }
  if (hidden_) {
    return;
  }

  // L'émission, seulement à portée.
  if (near_) {
    const float rate = EffectToolRandomBetween(draws, setting_.rate[0], setting_.rate[1]);
    // ⚠ LA CADENCE EST TRONQUÉE À L'ENTIER AVANT D'ÊTRE MULTIPLIÉE par le pas.
    const float owed = static_cast<float>(static_cast<std::int32_t>(rate)) * kMapEffectStepSeconds;
    std::int32_t count = static_cast<std::int32_t>(owed);
    const float carried = (owed - static_cast<float>(static_cast<std::int32_t>(owed))) + carried_;
    carried_ = carried;
    // ⚠ STRICTEMENT plus que un : un report d'exactement un attend un pas.
    if (carried > 1.0F) {
      count += static_cast<std::int32_t>(carried);
      carried_ = carried - static_cast<float>(static_cast<std::int32_t>(carried));
    }
    for (; count > 0; --count) {
      // La première particule libre, ou rien quand tout sert.
      const auto free_slot =
          std::ranges::find_if(particles_, [](const Particle& one) { return !one.used; });
      if (free_slot == particles_.end()) {
        continue;
      }
      free_slot->used = true;
      ++used_;
      InitParticle(*free_slot, draws);
      if (setting_.eternal) {
        free_slot->alpha = setting_.color[3];
      }
    }
  }
  for (Particle& particle : particles_) {
    UpdateParticle(particle);
  }
}

void MapEmitter::UpdateParticle(Particle& particle) {
  // ⚠ LA PLACE EST REMISE AU POINT D'ÉMISSION À CHAQUE PAS, vivante ou non :
  // une particule morte qui se tourne vers l'œil se peint donc au centre de son
  // émetteur.
  particle.position = position_;
  if (!particle.used) {
    ++particle.age_steps;
    return;
  }
  const float life_steps =
      static_cast<float>(static_cast<double>(particle.life_seconds) * kStepsPerSecond);
  const auto age = static_cast<float>(particle.age_steps);
  if (particle.life_seconds == 0.0F || life_steps > age || (setting_.eternal && near_)) {
    for (std::size_t channel = 0; channel < particle.rgb.size(); ++channel) {
      const auto value = static_cast<std::int16_t>(setting_.color[channel]);
      if (value != kUnchangedChannel) {
        particle.rgb[channel] = value;
      }
    }
    if (!setting_.eternal) {
      // 🔴 LE PAS D'ALPHA VAUT 4,25 ET LA TRONCATURE LE CHANGE : il monte de
      // QUATRE par pas et descend de CINQ.
      const auto step =
          static_cast<float>(static_cast<double>(kMapEffectStepSeconds) * kAlphaScale);
      const auto current = static_cast<float>(particle.alpha);
      std::int32_t alpha = 0;
      if (life_steps - age >= kFadeSteps) {
        alpha = std::min(static_cast<std::int32_t>(step + current), kOpaqueAlpha);
      } else if (const auto faded = static_cast<std::int32_t>(current - step); faded >= 0) {
        alpha = faded;
      }
      particle.alpha = alpha;
    }
    const float step = kMapEffectStepSeconds;
    const float speed = setting_.speed;
    particle.offset.y = particle.velocity.y * speed * step + particle.offset.y;
    particle.offset.z = particle.velocity.z * speed * step + particle.offset.z;
    particle.offset.x = particle.velocity.x * speed * step + particle.offset.x;
    particle.position = particle.offset + position_;
    particle.velocity += step * setting_.gravity;

    const float width = particle.size * setting_.scale[0];
    const float height = particle.size * setting_.scale[1];
    if (setting_.flat) {
      // Couchée : l'échelle, les trois angles ENTIERS de la table, la place.
      NativeMatrix matrix = NativeScale(width, height, 1.0F);
      MultiplyRotationX(matrix,
                        NativeTableRadians(static_cast<std::int32_t>(setting_.rotation_degrees.x)));
      MultiplyRotationY(matrix,
                        NativeTableRadians(static_cast<std::int32_t>(setting_.rotation_degrees.y)));
      MultiplyRotationZ(matrix,
                        NativeTableRadians(static_cast<std::int32_t>(setting_.rotation_degrees.z)));
      MultiplyTranslation(matrix, particle.position);
      particle.matrix = matrix;
      particle.pose = Pose::kPlaced;
    } else {
      particle.facing_scale = {width, height};
      particle.pose = Pose::kFacing;
    }
  } else {
    // Elle retourne à la réserve.
    particle.used = false;
    --used_;
  }
  ++particle.age_steps;
}

void MapEmitter::AppendQuads(const MapEffectFrame& frame, const BillboardBasis& basis,
                             std::vector<EffectWorldQuad>& quads) const {
  if (!drawn()) {
    return;
  }
  const std::int32_t source =
      setting_.blend_source != kUnsetBlend ? setting_.blend_source : kParticleBlendSource;
  const std::int32_t destination = setting_.blend_destination != kUnsetBlend
                                       ? setting_.blend_destination
                                       : kParticleBlendDestination;
  // Une particule jamais mise à jour vivante n'a que le lacet de la caméra :
  // son quad reste DEBOUT, face à l'azimut de l'œil.
  glm::vec3 forward_level{basis.forward.x, 0.0F, basis.forward.z};
  if (const float forward_length = glm::length(forward_level); forward_length > 0.0F) {
    forward_level /= forward_length;
  }
  constexpr glm::vec3 kWorldUp{0.0F, 1.0F, 0.0F};

  for (const Particle& particle : particles_) {
    std::array<glm::vec3, 4> corners{};
    const glm::vec3 center = frame.ToWorld(particle.position);
    for (std::size_t corner = 0; corner < kUnitQuad.size(); ++corner) {
      const glm::vec3 local = kUnitQuad[corner];
      if (setting_.flat) {
        corners[corner] = frame.ToWorld(Transform(local, particle.matrix));
      } else if (particle.pose == Pose::kFacing) {
        // 🔴 L'ORDONNÉE DU CLIENT DESCEND À L'ÉCRAN, comme celle d'un sprite :
        // le panneau la porte sur le « bas » de la caméra. Sa profondeur
        // s'éloigne de l'œil.
        corners[corner] = center + basis.right * (local.x * particle.facing_scale.x) -
                          basis.up * (local.y * particle.facing_scale.y) + basis.forward * local.z;
      } else {
        corners[corner] =
            center + basis.right * local.x - kWorldUp * local.y + forward_level * local.z;
      }
    }
    EffectWorldQuad quad;
    for (std::size_t corner = 0; corner < kStripToCycle.size(); ++corner) {
      quad.corners[corner] = corners[kStripToCycle[corner]];
      quad.texture_uv[corner] = kUnitQuadUv[kStripToCycle[corner]];
    }
    quad.tint = PackedTint(particle.rgb, particle.alpha);
    quad.blend_source = source;
    quad.blend_destination = destination;
    quads.push_back(quad);
  }
}

}  // namespace ro::render
