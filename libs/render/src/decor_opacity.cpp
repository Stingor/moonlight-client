#include "ro/render/decor_opacity.h"

#include <algorithm>
#include <cmath>

#include "ro/core/sorted_table.h"

namespace ro::render {
namespace {

constexpr float kOpacityByteScale = 255.0F;
constexpr unsigned kHalfByteForRounding = 127U;

// Combien de poses le modèle porte, figées et animées.
[[nodiscard]] std::size_t PoseCount(const ModelInstances& model) {
  std::size_t count = model.transforms.size();
  for (const ModelPlaybackGroup& group : model.animated) {
    count += group.transforms.size();
  }
  return count;
}

// L'opacité du rang `slot` d'une rangée ; pleine hors de la rangée.
[[nodiscard]] std::uint8_t OpacityAt(const ModelOpacities* opacities, std::size_t model_index,
                                     std::size_t slot) {
  if (opacities == nullptr || model_index >= opacities->size()) {
    return kVeilOpaque;
  }
  const std::vector<std::uint8_t>& row = (*opacities)[model_index];
  return slot < row.size() ? row[slot] : kVeilOpaque;
}

// L'opacité que `by_rank` donne au rang `rank` du RSW, pleine s'il n'y est pas.
[[nodiscard]] std::uint8_t OpacityOfRank(std::span<const mapedit::ModelOpacity> by_rank,
                                         std::size_t rank) {
  const mapedit::ModelOpacity* found =
      core::RowWithKey(by_rank, rank, &mapedit::ModelOpacity::model_index);
  if (found == nullptr) {
    return kVeilOpaque;
  }
  return OpacityByte(found->opacity);
}

}  // namespace

std::uint8_t OpacityByte(float opacity) noexcept {
  const float bounded = std::clamp(opacity, mapedit::kNoOpacity, mapedit::kFullOpacity);
  return static_cast<std::uint8_t>(std::lround(bounded * kOpacityByteScale));
}

std::uint8_t CombinedOpacity(std::uint8_t veil, std::uint8_t document) noexcept {
  const unsigned product = static_cast<unsigned>(veil) * static_cast<unsigned>(document);
  return static_cast<std::uint8_t>((product + kHalfByteForRounding) / kVeilOpaque);
}

std::size_t OpacitySlot(const ModelInstances& model, std::optional<std::size_t> group,
                        std::size_t index) {
  if (!group.has_value()) {
    return index;
  }
  std::size_t slot = model.transforms.size();
  for (std::size_t before = 0; before < *group && before < model.animated.size(); ++before) {
    slot += model.animated[before].transforms.size();
  }
  return slot + index;
}

void DocumentOpacities(std::span<const ModelInstances> models,
                       std::span<const mapedit::ModelOpacity> by_rank, ModelOpacities& into) {
  into.clear();
  if (by_rank.empty()) {
    return;
  }
  into.resize(models.size());
  bool translucent = false;
  const auto fill = [&](std::vector<std::uint8_t>& row, std::span<const std::size_t> ranks) {
    for (const std::size_t rank : ranks) {
      const std::uint8_t opacity = OpacityOfRank(by_rank, rank);
      translucent = translucent || opacity < kVeilOpaque;
      row.push_back(opacity);
    }
  };
  for (std::size_t model_index = 0; model_index < models.size(); ++model_index) {
    const ModelInstances& model = models[model_index];
    std::vector<std::uint8_t>& row = into[model_index];
    row.reserve(PoseCount(model));
    fill(row, model.rsw_ranks);
    for (const ModelPlaybackGroup& group : model.animated) {
      fill(row, group.rsw_ranks);
    }
  }
  // AUCUNE POSE DESSINÉE N'EST TRANSLUCIDE — les translucides sont hors de la
  // carte, ou illisibles : rien à composer.
  if (!translucent) {
    into.clear();
  }
}

void ComposeDecorOpacity(std::span<const ModelInstances> models, const ModelOpacities& document,
                         const ModelOpacities* veil, std::span<const VeiledPose> veiled,
                         DecorOpacity& into) {
  into.opacities.resize(models.size());
  into.translucent.clear();
  for (std::size_t model_index = 0; model_index < models.size(); ++model_index) {
    const ModelInstances& model = models[model_index];
    std::vector<std::uint8_t>& row = into.opacities[model_index];
    row.resize(PoseCount(model));
    for (std::size_t slot = 0; slot < row.size(); ++slot) {
      row[slot] = CombinedOpacity(OpacityAt(veil, model_index, slot),
                                  OpacityAt(&document, model_index, slot));
    }
    // LES POSES QUE LE DOCUMENT SEUL REND TRANSLUCIDES : celles que le voile
    // efface viennent de sa liste, qui sait si la vue les montre.
    const auto add = [&](std::optional<std::size_t> group, std::size_t index) {
      const std::size_t slot = OpacitySlot(model, group, index);
      if (OpacityAt(veil, model_index, slot) < kVeilOpaque || row[slot] == kVeilOpaque ||
          row[slot] == 0) {
        return;
      }
      into.translucent.push_back(VeiledPose{.model_index = model_index,
                                            .group = group,
                                            .index = index,
                                            .opacity = row[slot],
                                            .across = true});
    };
    for (std::size_t index = 0; index < model.transforms.size(); ++index) {
      add(std::nullopt, index);
    }
    for (std::size_t group = 0; group < model.animated.size(); ++group) {
      for (std::size_t index = 0; index < model.animated[group].transforms.size(); ++index) {
        add(group, index);
      }
    }
  }
  for (VeiledPose pose : veiled) {
    if (pose.model_index >= models.size()) {
      continue;
    }
    pose.opacity = CombinedOpacity(
        pose.opacity, OpacityAt(&document, pose.model_index,
                                OpacitySlot(models[pose.model_index], pose.group, pose.index)));
    if (pose.opacity > 0) {
      into.translucent.push_back(pose);
    }
  }
}

void ShadowCastingOpacities(const ModelOpacities& document, ModelOpacities& into) {
  into.resize(document.size());
  bool shadowless = false;
  for (std::size_t model_index = 0; model_index < document.size(); ++model_index) {
    const std::vector<std::uint8_t>& from = document[model_index];
    std::vector<std::uint8_t>& row = into[model_index];
    row.resize(from.size());
    for (std::size_t slot = 0; slot < from.size(); ++slot) {
      const bool casts = from[slot] >= kShadowlessBelowOpacity;
      shadowless = shadowless || !casts;
      row[slot] = casts ? kVeilOpaque : 0;
    }
  }
  if (!shadowless) {
    into.clear();
  }
}

}  // namespace ro::render
