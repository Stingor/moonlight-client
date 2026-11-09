#include "ro/render/actor_palettes.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace ro::render {

const formats::Palette& ActorPalettes::Base(const formats::SprSprite& sprite,
                                            const formats::Palette* official) {
  // Sans palette officielle, la fusion rendrait une copie de celle du sprite :
  // la sienne sert telle quelle, et vit aussi longtemps que lui.
  if (official == nullptr) {
    return *sprite.palette();
  }
  const Source source{&sprite, official};
  if (const auto found = merged_.find(source); found != merged_.end()) {
    return found->second;
  }
  return merged_.emplace(source, MergeServerPalette(*sprite.palette(), official)).first->second;
}

const std::array<int, formats::kPaletteEntryCount>& ActorPalettes::Usage(
    const formats::SprSprite& sprite) {
  if (const auto found = usage_.find(&sprite); found != usage_.end()) {
    return found->second;
  }
  // 🔴 L'ÉTAGE QUI COÛTE : tous les pixels de toutes les images. Il ne dépend
  // que du sprite, et se compte donc une fois pour toutes les teintes.
  return usage_.emplace(&sprite, CountIndexUsage(sprite)).first->second;
}

const ActorPalettes::Analysis& ActorPalettes::Analyze(const formats::SprSprite& sprite,
                                                      const formats::Palette* official) {
  const Source source{&sprite, official};
  if (const auto found = analyses_.find(source); found != analyses_.end()) {
    return found->second;
  }
  Analysis analysis;
  // ⚠ LA DÉTECTION SE FAIT SUR LA BASE FUSIONNÉE, jamais sur le sprite nu : les
  // frontières des rampes ne sont pas les mêmes des deux côtés de la fusion.
  // Voir l'ADR 0050.
  analysis.base = &Base(sprite, official);
  const std::array<int, formats::kPaletteEntryCount>& usage = Usage(sprite);
  analysis.ramps = DetectRamps(*analysis.base, usage);
  for (const int used : usage) {
    analysis.pixels_total += used;
  }
  for (const PaletteRamp& ramp : analysis.ramps) {
    const std::size_t end = std::min<std::size_t>(
        static_cast<std::size_t>(ramp.start) + ramp.length, formats::kPaletteEntryCount);
    for (std::size_t index = ramp.start; index < end; ++index) {
      analysis.pixels_covered += usage[index];
    }
  }
  return analyses_.emplace(source, std::move(analysis)).first->second;
}

bool ActorPalettes::Knows(std::uint32_t style) const {
  return recipes_.contains(style) || ready_made_.contains(style);
}

bool ActorPalettes::SetRecipe(std::uint32_t style, const PaletteRecipe& recipe) {
  // ⚠ NI UN IDENTIFIANT TENU PAR UNE PALETTE TOUTE FAITE : elles passent avant
  // les recettes, et l'acteur porterait celle d'un autre.
  if (style == 0 || ready_made_.contains(style)) {
    return false;
  }
  return recipes_.emplace(style, recipe).second;
}

bool ActorPalettes::SetPalette(std::uint32_t style, formats::Palette palette) {
  if (style == 0 || Knows(style)) {
    return false;
  }
  return ready_made_.emplace(style, StyledPalette{std::move(palette), ++last_serial_}).second;
}

void ActorPalettes::Forget(std::uint32_t style) {
  recipes_.erase(style);
  if (const auto ready = ready_made_.find(style); ready != ready_made_.end()) {
    retired_serials_.push_back(ready->second.serial);
    ready_made_.erase(ready);
  }
  for (auto painted = painted_.begin(); painted != painted_.end();) {
    if (painted->first.style != style) {
      ++painted;
      continue;
    }
    retired_serials_.push_back(painted->second.serial);
    painted = painted_.erase(painted);
  }
}

const ActorPalettes::StyledPalette* ActorPalettes::StyledFor(std::uint32_t style,
                                                             const formats::SprSprite& sprite,
                                                             const formats::Palette* official) {
  if (const auto ready = ready_made_.find(style); ready != ready_made_.end()) {
    return &ready->second;
  }
  const Painted key{style, Source{&sprite, official}};
  if (const auto found = painted_.find(key); found != painted_.end()) {
    return &found->second;
  }
  const auto recipe = recipes_.find(style);
  // Un style qu'on ne connaît pas laisse l'apparence NATIVE. Cela arrive pour
  // de bon : le lot du serveur peut décrire un joueur dont le corps n'est pas
  // encore chargé, et la recette arrive alors avant ce qu'elle repeint.
  if (recipe == recipes_.end() || !sprite.palette().has_value()) {
    return nullptr;
  }
  const Analysis& analysis = Analyze(sprite, official);
  return &painted_
              .emplace(key,
                       StyledPalette{ApplyRecipe(*analysis.base, analysis.ramps, recipe->second),
                                     ++last_serial_})
              .first->second;
}

std::vector<std::uint32_t> ActorPalettes::TakeRetiredSerials() {
  return std::exchange(retired_serials_, {});
}

}  // namespace ro::render
