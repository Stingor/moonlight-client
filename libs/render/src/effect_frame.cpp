#include "ro/render/effect_frame.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ro::render {
namespace {

using formats::StrAnimationType;
using formats::StrKeyFrame;

// Une clé qui ne POSE rien : elle dit seulement que la précédente se déforme.
constexpr std::int32_t kMorphKeyType = 1;

float Lerp(float from, float to, float progress) { return (to - from) * progress + from; }

// L'index de texture d'un mode d'animation, à cette image.
//
// Les modes ne regardent JAMAIS la texture de la clé suivante : ils avancent
// depuis celle de la clé courante, au rythme de son délai. C'est ainsi que le
// jeu anime une couche de plusieurs textures sans clé par image.
std::size_t TextureIndexAt(const StrKeyFrame& key, const StrKeyFrame& next, std::int32_t frame,
                           std::size_t texture_count) {
  if (texture_count == 0) {
    return 0;
  }
  const auto count = static_cast<std::int64_t>(texture_count);
  const auto elapsed = static_cast<float>(frame - key.frame_index);
  const auto advanced = static_cast<std::int64_t>(key.delay * elapsed + key.texture_index);
  const auto wrapped = [count](std::int64_t value) {
    return static_cast<std::size_t>(((value % count) + count) % count);
  };

  switch (key.animation) {
    case StrAnimationType::kInterpolation: {
      const float span = static_cast<float>(next.frame_index - key.frame_index);
      const float progress = span > 0.0F ? elapsed / span : 0.0F;
      return wrapped(
          static_cast<std::int64_t>(Lerp(key.texture_index, next.texture_index, progress)));
    }
    case StrAnimationType::kOnce:
      // De la texture de la clé jusqu'à la DERNIÈRE de la couche, puis on y
      // reste.
      return static_cast<std::size_t>(std::clamp<std::int64_t>(advanced, 0, count - 1));
    case StrAnimationType::kLoop:
      return wrapped(advanced);
    case StrAnimationType::kReverseLoop:
      // À rebours. La première image change aussitôt, et c'est voulu : le jeu
      // fait ainsi.
      return wrapped(static_cast<std::int64_t>(key.texture_index - key.delay * elapsed));
    case StrAnimationType::kBiLoop: {
      // Aller PUIS retour, sans répéter les deux extrémités.
      if (count <= 1) {
        return 0;
      }
      const std::int64_t cycle = (count - 1) * 2;
      std::int64_t position = ((advanced % cycle) + cycle) % cycle;
      if (position >= count) {
        position = cycle - position;
      }
      return static_cast<std::size_t>(position);
    }
    case StrAnimationType::kStop:
      break;
  }
  return wrapped(static_cast<std::int64_t>(key.texture_index));
}

EffectLayerFrame Posed(const StrKeyFrame& key, std::size_t texture_count) {
  EffectLayerFrame shown;
  shown.corners = key.corners;
  shown.uv = {key.uv[0], key.uv[1], key.uv[2], key.uv[3]};
  shown.color = key.color;
  shown.offset_x = key.offset_x;
  shown.offset_y = key.offset_y;
  shown.angle_degrees = key.angle_degrees;
  const auto index = static_cast<std::int64_t>(key.texture_index);
  shown.texture_index = texture_count == 0
                            ? 0
                            : static_cast<std::size_t>(std::clamp<std::int64_t>(
                                  index, 0, static_cast<std::int64_t>(texture_count) - 1));
  shown.blend_source = key.blend_source;
  shown.blend_destination = key.blend_destination;
  return shown;
}

}  // namespace

PreparedEffect PrepareEffect(const formats::StrEffect& effect) {
  PreparedEffect prepared;
  prepared.last_frame = effect.last_frame();
  // La cadence que le fichier déclare ne se reporte PAS : le jeu ne la lit pas
  // (voir `kEffectTickMs`), et la porter ici inviterait à s'en resservir.
  prepared.layers.reserve(effect.layers().size());

  for (const formats::StrLayer& source : effect.layers()) {
    PreparedEffectLayer layer;
    layer.texture_count = source.texture_names.size();
    layer.first_texture = prepared.texture_names.size();
    for (const std::string& name : source.texture_names) {
      prepared.texture_names.push_back(name);
    }

    // Chaque clé de déformation marque celle qui la PRÉCÈDE — sauf lorsqu'une
    // clé posée la suit à la même image, auquel cas la couche coupe net.
    std::vector<bool> morphs(source.key_frames.size(), false);
    for (std::size_t index = 1; index < source.key_frames.size(); ++index) {
      if (source.key_frames[index].type != kMorphKeyType) {
        continue;
      }
      const std::size_t after = index + 1;
      const bool cut = after < source.key_frames.size() &&
                       source.key_frames[after].type != kMorphKeyType &&
                       source.key_frames[after].frame_index == source.key_frames[index].frame_index;
      morphs[index - 1] = !cut;
    }

    layer.keys.reserve(source.key_frames.size());
    for (std::size_t index = 0; index < source.key_frames.size(); ++index) {
      if (source.key_frames[index].type == kMorphKeyType) {
        continue;
      }
      layer.keys.push_back(PreparedEffectKey{source.key_frames[index], morphs[index]});
    }

    // Deux clés à la même image : seule la DERNIÈRE compte. Garder les deux
    // donnerait une durée nulle entre elles, donc une division par zéro à
    // l'interpolation.
    std::size_t kept = 0;
    for (std::size_t index = 0; index < layer.keys.size(); ++index) {
      const bool shadowed =
          index + 1 < layer.keys.size() &&
          layer.keys[index + 1].key.frame_index == layer.keys[index].key.frame_index;
      if (!shadowed) {
        layer.keys[kept++] = layer.keys[index];
      }
    }
    layer.keys.resize(kept);
    prepared.layers.push_back(std::move(layer));
  }
  return prepared;
}

std::optional<EffectLayerFrame> EffectLayerAt(const PreparedEffectLayer& layer, std::int32_t frame,
                                              std::int32_t last_frame) {
  if (layer.keys.empty() || frame < layer.keys.front().key.frame_index) {
    return std::nullopt;
  }
  // La dernière clé posée à cette image ou avant.
  std::size_t at = 0;
  for (std::size_t index = 1; index < layer.keys.size(); ++index) {
    if (layer.keys[index].key.frame_index > frame) {
      break;
    }
    at = index;
  }

  const PreparedEffectKey& current = layer.keys[at];
  // ⚠ Passé sa DERNIÈRE clé, une couche ne tient sa pose que si cette clé se
  // DÉFORME — elle se déforme alors vers elle-même, jusqu'à la fin de l'effet.
  // Sinon elle ne paraît qu'à son image, et s'éteint aussitôt après.
  //
  // Sans cette extinction, une couche survit jusqu'au bout de l'effet en tenant
  // sa dernière pose. Cela ne se voit pas d'une couche qui finit transparente —
  // une tempête qui se dissipe — mais cela FIGE À L'ÉCRAN celle qui finit
  // opaque : dans StormGust, la neige s'éteint d'elle-même et les éclairs
  // restaient allumés vingt images de plus que la tempête.
  const bool last = at + 1 == layer.keys.size();
  if (last && !current.morphs && frame > current.key.frame_index) {
    return std::nullopt;
  }
  if (!current.morphs || current.key.frame_index == frame) {
    return Posed(current.key, layer.texture_count);
  }
  // Une clé qui se déforme sans clé suivante se déforme vers ELLE-MÊME, posée à
  // la dernière image : la couche tient sa pose et son animation de texture
  // continue jusqu'au bout.
  StrKeyFrame tail = current.key;
  tail.frame_index = last_frame;
  const StrKeyFrame& next = at + 1 < layer.keys.size() ? layer.keys[at + 1].key : tail;

  const float span = static_cast<float>(next.frame_index - current.key.frame_index);
  const float progress =
      span > 0.0F ? static_cast<float>(frame - current.key.frame_index) / span : 0.0F;

  EffectLayerFrame shown;
  for (std::size_t index = 0; index < shown.corners.size(); ++index) {
    shown.corners[index] = Lerp(current.key.corners[index], next.corners[index], progress);
  }
  for (std::size_t index = 0; index < shown.uv.size(); ++index) {
    shown.uv[index] = Lerp(current.key.uv[index], next.uv[index], progress);
  }
  for (std::size_t index = 0; index < shown.color.size(); ++index) {
    shown.color[index] = Lerp(current.key.color[index], next.color[index], progress);
  }
  shown.offset_x = Lerp(current.key.offset_x, next.offset_x, progress);
  shown.offset_y = Lerp(current.key.offset_y, next.offset_y, progress);
  shown.angle_degrees = Lerp(current.key.angle_degrees, next.angle_degrees, progress);
  shown.texture_index = TextureIndexAt(current.key, next, frame, layer.texture_count);
  // Le MÉLANGE ne se mélange pas : c'est celui de la clé de départ, jusqu'à la
  // suivante.
  shown.blend_source = current.key.blend_source;
  shown.blend_destination = current.key.blend_destination;
  return shown;
}

std::int32_t EffectLoopFrameAt(const PreparedEffect& effect, float elapsed_ms) {
  const std::int64_t span = static_cast<std::int64_t>(effect.last_frame) + 1;
  if (span <= 1 || !(elapsed_ms > 0.0F)) {
    return 0;
  }
  const auto tick = static_cast<std::int64_t>(elapsed_ms / static_cast<float>(kEffectTickMs));
  return static_cast<std::int32_t>(tick % span);
}

std::optional<std::int32_t> EffectFrameAt(const PreparedEffect& effect, float elapsed_ms) {
  if (!(elapsed_ms > 0.0F)) {
    // Une durée négative — ou pas un nombre — vaut le début, plutôt qu'une
    // image négative que la couche lirait comme « pas encore commencée ».
    return 0;
  }
  const auto frame = static_cast<std::int64_t>(elapsed_ms / static_cast<float>(kEffectTickMs));
  if (frame > effect.last_frame) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(frame);
}

}  // namespace ro::render
