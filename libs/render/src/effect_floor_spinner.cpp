#include "ro/render/effect_floor_spinner.h"

#include <algorithm>
#include <cstdio>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::size_t kLeadingTrack = 0;

// LES DEUX MÉLANGES DE LA VOIE DES QUADS : l'addition et le mélange ordinaire.
constexpr std::int16_t kAddedBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;
// LA VALEUR DE REPOS QUI FIGE L'EAU.
constexpr float kAtRest = 1.0F;

// LES TROIS JEUX D'IMAGES : le chemin avant le numéro, l'extension après lui,
// le compte, et le numéro de la première. Le numéro s'écrit sur deux chiffres.
struct ImageSet {
  const char* stem = nullptr;
  const char* extension = nullptr;
  std::int32_t count = 0;
  std::int32_t first_number = 0;
};

constexpr ImageSet kTorchImages{"data\\texture\\effect\\torch_red", ".bmp", 13, 1};
constexpr ImageSet kWaterImages{"data\\texture\\effect\\W_c_", ".tga", 51, 0};
constexpr ImageSet kDustImages{"data\\texture\\effect\\dust", ".bmp", 9, 1};

// Assez pour le plus long des trois chemins et son numéro.
constexpr std::size_t kPathBufferSize = 64;

[[nodiscard]] const ImageSet& ImagesOf(std::uint8_t byte) {
  if (byte == kFloorSpinnerByteOfTorch) {
    return kTorchImages;
  }
  if (byte == kFloorSpinnerByteOfWater) {
    return kWaterImages;
  }
  return kDustImages;
}

// UN ANGLE RAMENÉ DANS LE TOUR après un pas positif : le client retranche un
// tour une seule fois.
[[nodiscard]] std::int16_t Wrapped(std::int32_t degrees) {
  return static_cast<std::int16_t>(
      degrees >= kEffectTrackFullTurnDegrees ? degrees - kEffectTrackFullTurnDegrees : degrees);
}

}  // namespace

std::span<const EffectFloorSpinnerPattern> EffectFloorSpinnerPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectFloorSpinnerPattern> table = EffectFloorSpinnerPatternTable();
  const auto [first, last] =
      std::ranges::equal_range(table, effect_id, {}, &EffectFloorSpinnerPattern::effect_id);
  return {first, last};
}

std::vector<std::string> EffectFloorSpinnerTextures(std::uint8_t byte) {
  const ImageSet& images = ImagesOf(byte);
  std::vector<std::string> paths;
  paths.reserve(static_cast<std::size_t>(images.count));
  for (std::int32_t rank = 0; rank < images.count; ++rank) {
    std::array<char, kPathBufferSize> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s%02d%s", images.stem, images.first_number + rank,
                  images.extension);
    paths.emplace_back(buffer.data());
  }
  return paths;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedFloorSpinnerTracks(
    const EffectFloorSpinnerPattern& pattern, float instance_first, float instance_second,
    std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> node_tracks;
  EffectTrack& leader = node_tracks[kLeadingTrack];
  leader.active = true;
  leader.ticks = 0;
  leader.alpha = kFloorSpinnerAlpha;
  if (pattern.byte == kFloorSpinnerByteOfWater) {
    leader.radius_at_anchor =
        instance_first < kFloorSpinnerWaterRadiusFloor ? kFloorSpinnerWaterRadius : instance_first;
    const bool within_turn = instance_second >= 0.0F &&
                             instance_second < static_cast<float>(kEffectTrackFullTurnDegrees);
    leader.azimuth_degrees = within_turn ? static_cast<std::int16_t>(instance_second)
                                         : kFloorSpinnerWaterFallbackDegrees;
    leader.value[kFloorSpinnerSlotOfRest] = kAtRest;
  } else {
    leader.radius_at_anchor = kFloorSpinnerRadius;
    leader.azimuth_degrees = pattern.azimuth_degrees;
    leader.value[kFloorSpinnerSlotOfRest] = 0.0F;
  }
  leader.tilt_degrees = static_cast<std::int16_t>(
      NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
  leader.value[kFloorSpinnerSlotOfHeightDown] = kFloorSpinnerHeightDown;
  leader.value[kFloorSpinnerSlotOfImage] = 0.0F;
  return node_tracks;
}

PooledEffectNode SowFloorSpinnerNode(const EffectFloorSpinnerPattern& pattern,
                                     glm::vec3 owner_position, float instance_first,
                                     float instance_second, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kFloorSpinner;
  pooled.ground_position = owner_position;
  pooled.owner_position = owner_position;
  pooled.node.life_ticks = std::max(pattern.life_ticks, 1);
  pooled.node.render_word = kFloorSpinnerRenderWord;
  // ⚠ L'IMAGE EST CELLE DE LA PISTE, et le nœud n'avance pas la sienne : c'est
  // le mouvement qui la recopie.
  pooled.node.frame_hold_ticks = 0;
  pooled.resource_paths = EffectFloorSpinnerTextures(pattern.byte);
  pooled.node.texture_count = pooled.resource_paths.size();
  pooled.tracks = SeedFloorSpinnerTracks(pattern, instance_first, instance_second, generator);
  return pooled;
}

void AdvanceFloorSpinnerTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::size_t image_count) {
  const auto tally = static_cast<float>(image_count);
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.value[kFloorSpinnerSlotOfRest] == 0.0F) {
      effect_track.azimuth_degrees =
          Wrapped(effect_track.azimuth_degrees + kFloorSpinnerTurnPerTick);
      effect_track.tilt_degrees = Wrapped(effect_track.tilt_degrees + kFloorSpinnerBreathPerTick);
    }
    float& image = effect_track.value[kFloorSpinnerSlotOfImage];
    if (effect_track.ticks % kFloorSpinnerTicksPerImage == 0) {
      image += 1.0F;
    }
    if (image >= tally) {
      image -= tally;
    }
  }
}

std::int32_t FloorSpinnerImageOf(const EffectTrack& track) {
  return static_cast<std::int32_t>(track.value[kFloorSpinnerSlotOfImage]);
}

std::optional<EffectWorldQuad> ComposeFloorSpinnerQuad(const EffectTrack& track,
                                                       glm::vec3 node_origin) {
  if (!track.active) {
    return std::nullopt;
  }
  const float radius = track.radius_at_anchor;
  const float breath = SinOfDegrees(track.tilt_degrees);
  const float etendue =
      (breath + 1.0F) * (radius * kFloorSpinnerSwingPart) + radius * kFloorSpinnerSteadyPart;
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : la hauteur change de signe,
  // et la profondeur avec elle. Chaque coin passe entier dans notre repère : n'en
  // retourner qu'une ferait tourner le carré à l'envers.
  const float client_height = track.value[kFloorSpinnerSlotOfHeightDown];

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < request.corners.size(); ++corner) {
    const std::int32_t angle =
        (track.azimuth_degrees + kFloorSpinnerCornerDegrees * static_cast<std::int32_t>(corner)) %
        kEffectTrackFullTurnDegrees;
    request.corners[corner] = EffectPointFromClient(
        {etendue * CosOfDegrees(angle), client_height, etendue * SinOfDegrees(angle)});
  }
  request.absolute = false;
  request.alpha =
      static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, kEffectTrackAlphaMax));
  // LE REPOS CHOISIT LE MÉLANGE : l'eau au mélange ordinaire, le reste en
  // addition.
  request.blend_mode =
      track.value[kFloorSpinnerSlotOfRest] == kAtRest ? kOrdinaryBlend : kAddedBlend;
  request.texture = 0;
  return ComposeEffectParticleQuad(request, node_origin);
}

}  // namespace ro::render
