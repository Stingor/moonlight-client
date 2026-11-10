#include "ro/render/actor_palettes.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace ro::render {
namespace {

// Une teinture qu'on sait appliquer : un profil connu, et pour les rampes un
// calque que le profil 1 découpe.
[[nodiscard]] bool Applicable(const LayerDye& dye) {
  if (dye.layer == StyleLayer::kBodyMore) {
    return dye.profile == kBodyContinuationProfileNumber;
  }
  if (dye.profile == kHueClassProfileNumber) {
    return true;
  }
  return dye.profile == kLayerProfileNumber && RampLayerOf(dye.layer).has_value();
}

}  // namespace

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

ActorPalettes::Analysis ActorPalettes::Measured(
    const formats::Palette& base, std::vector<PaletteRamp> ramps,
    const std::array<int, formats::kPaletteEntryCount>& usage) const {
  Analysis analysis;
  analysis.base = &base;
  analysis.usage = &usage;
  analysis.ramps = std::move(ramps);
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
  return analysis;
}

const ActorPalettes::Analysis& ActorPalettes::Analyze(const formats::SprSprite& sprite,
                                                      const formats::Palette* official) {
  const Source source{&sprite, official};
  if (const auto found = analyses_.find(source); found != analyses_.end()) {
    return found->second;
  }
  // ⚠ LA DÉTECTION SE FAIT SUR LA BASE FUSIONNÉE, jamais sur le sprite nu : les
  // frontières des rampes ne sont pas les mêmes des deux côtés de la fusion.
  // Voir l'ADR 0050.
  const formats::Palette& base = Base(sprite, official);
  const std::array<int, formats::kPaletteEntryCount>& usage = Usage(sprite);
  Analysis analysis = Measured(base, DetectRamps(base, usage), usage);
  analysis.continuation = DetectBodyContinuation(base, usage);
  for (const PaletteRamp& ramp : analysis.continuation) {
    analysis.continuation_pixels += ramp.pixels;
  }
  return analyses_.emplace(source, std::move(analysis)).first->second;
}

const ActorPalettes::Analysis& ActorPalettes::AnalyzeLayer(const formats::SprSprite& sprite,
                                                           const formats::Palette* official,
                                                           RampLayer layer, SkinRace race) {
  const LayerSource source{Source{&sprite, official}, layer, race};
  if (const auto found = layer_analyses_.find(source); found != layer_analyses_.end()) {
    return found->second;
  }
  const formats::Palette& base = Base(sprite, official);
  const std::array<int, formats::kPaletteEntryCount>& usage = Usage(sprite);
  // Le profil 1 existe pour chaque calque qu'il découpe : la teinture qui le
  // demande l'a déjà vérifié.
  const std::optional<LayerRampProfile> profile = LayerProfile(kLayerProfileNumber, layer, race);
  std::vector<PaletteRamp> ramps =
      profile.has_value() ? DetectLayerRamps(base, usage, *profile) : std::vector<PaletteRamp>();
  return layer_analyses_.emplace(source, Measured(base, std::move(ramps), usage)).first->second;
}

bool ActorPalettes::Knows(std::uint32_t style) const { return sets_.contains(style); }

bool ActorPalettes::SetStyle(std::uint32_t style, StyleSet set) {
  if (style == 0 || Knows(style)) {
    return false;
  }
  if (set.body_palette.has_value()) {
    ready_made_.emplace(style, StyledPalette{std::move(*set.body_palette), {}, ++last_serial_});
    set.body_palette.reset();
  }
  sets_.emplace(style, std::move(set));
  return true;
}

bool ActorPalettes::SetRecipe(std::uint32_t style, const PaletteRecipe& recipe) {
  return SetStyle(style, StyleSet{.body = recipe});
}

bool ActorPalettes::SetPalette(std::uint32_t style, formats::Palette palette) {
  return SetStyle(style, StyleSet{.body_palette = std::move(palette)});
}

void ActorPalettes::Forget(std::uint32_t style) {
  sets_.erase(style);
  if (const auto ready = ready_made_.find(style); ready != ready_made_.end()) {
    retired_serials_.push_back(ready->second.serial);
    ready_made_.erase(ready);
  }
  for (auto painted = painted_.begin(); painted != painted_.end();) {
    if (painted->first.style != style) {
      ++painted;
      continue;
    }
    if (painted->second.has_value()) {
      retired_serials_.push_back(painted->second->serial);
    }
    painted = painted_.erase(painted);
  }
}

std::optional<ActorPalettes::StyledPalette> ActorPalettes::Paint(const StyleSet& set,
                                                                 const formats::SprSprite& sprite,
                                                                 const formats::Palette* official,
                                                                 const formats::Palette* ready,
                                                                 const Piece& piece) {
  const PaletteRecipe* body =
      piece.layer == StyleLayer::kBody && set.body.has_value() ? &*set.body : nullptr;
  const LayerDye* dye =
      RampLayerOf(piece.layer).has_value() ? set.Find(piece.layer, piece.key) : nullptr;
  if (dye != nullptr && !Applicable(*dye)) {
    dye = nullptr;
  }
  // La suite du corps, sous la clé du corps que porte la pièce.
  const LayerDye* more =
      piece.layer == StyleLayer::kBody ? set.Find(StyleLayer::kBodyMore, piece.key) : nullptr;
  if (more != nullptr && !Applicable(*more)) {
    more = nullptr;
  }
  const LayerDye* skin = piece.skin ? set.Find(StyleLayer::kSkin, 0) : nullptr;
  if (skin != nullptr && skin->profile != kLayerProfileNumber) {
    skin = nullptr;
  }
  const bool by_classes = dye != nullptr && dye->profile == kHueClassProfileNumber;

  StyledPalette styled;
  if (by_classes) {
    styled.tint = LayerHueClasses(*dye);
  }
  // Un sprite sans palette n'a que des images en couleurs vraies : seules les
  // classes de teinte les atteignent.
  if (!sprite.palette().has_value()) {
    return by_classes ? std::optional<StyledPalette>(std::move(styled)) : std::nullopt;
  }
  if (ready == nullptr && body == nullptr && more == nullptr && dye == nullptr && skin == nullptr) {
    return std::nullopt;
  }
  const formats::Palette& base = Base(sprite, official);
  if (ready != nullptr) {
    styled.palette = *ready;
  } else if (body != nullptr) {
    styled.palette = ApplyRecipe(base, Analyze(sprite, official).ramps, *body);
  } else if (by_classes) {
    styled.palette = ApplyHueClasses(base, *styled.tint);
  } else if (dye != nullptr) {
    const Analysis& layer = AnalyzeLayer(sprite, official, *RampLayerOf(dye->layer), piece.race);
    styled.palette = ApplyRecipe(base, layer.ramps, LayerRecipe(*dye));
    ApplyLayerRest(styled.palette, base, layer.ramps, piece.race, *dye);
  } else {
    styled.palette = base;
  }
  // ⚠ LA SUITE ET LE RESTE se posent aussi sur la palette toute faite de
  // l'aperçu, qui ne peint que les huit premières : ni l'une ni l'autre n'y
  // touchent une pièce v7.
  if (more != nullptr) {
    const Analysis& analysis = Analyze(sprite, official);
    ApplyBodyMore(styled.palette, base, analysis.ramps, analysis.continuation, piece.race, *more);
  }
  // 🔴 LA PEAU APRÈS LES PIÈCES ET LE RESTE : elle l'emporte sur eux.
  if (skin != nullptr) {
    ApplySkin(styled.palette, base, piece.race, skin->adjusts.front());
  }
  // 🔴 LES RETOUCHES EN TOUT DERNIER : une couleur exacte, que rien ne repeint.
  const LayerDye* touched = dye != nullptr ? dye : more;
  if (touched != nullptr && LayerTakesRetouches(*touched)) {
    ApplyRetouches(styled.palette, touched->retouches);
  }
  return styled;
}

const ActorPalettes::StyledPalette* ActorPalettes::StyledFor(std::uint32_t style,
                                                             const formats::SprSprite& sprite,
                                                             const formats::Palette* official,
                                                             const Piece& piece) {
  // Un style qu'on ne connaît pas laisse l'apparence NATIVE. Cela arrive pour
  // de bon : le lot du serveur peut décrire un joueur dont le corps n'est pas
  // encore chargé, et la recette arrive alors avant ce qu'elle repeint.
  const auto set = sets_.find(style);
  if (set == sets_.end()) {
    return nullptr;
  }
  const auto ready = piece.layer == StyleLayer::kBody ? ready_made_.find(style) : ready_made_.end();
  const bool skinned = piece.skin && set->second.Find(StyleLayer::kSkin, 0) != nullptr;
  const bool continued = piece.layer == StyleLayer::kBody &&
                         set->second.Find(StyleLayer::kBodyMore, piece.key) != nullptr;
  // La palette toute faite vaut pour tous les sprites, tant qu'aucune peau ni
  // aucune suite du corps ne s'y pose.
  if (ready != ready_made_.end() && !skinned && !continued) {
    return &ready->second;
  }
  const Painted key{style,     Source{&sprite, official}, piece.layer, piece.key, piece.race,
                    piece.skin};
  auto found = painted_.find(key);
  if (found == painted_.end()) {
    std::optional<StyledPalette> painted =
        Paint(set->second, sprite, official,
              ready != ready_made_.end() ? &ready->second.palette : nullptr, piece);
    if (painted.has_value()) {
      painted->serial = ++last_serial_;
    }
    found = painted_.emplace(key, std::move(painted)).first;
  }
  return found->second.has_value() ? &*found->second : nullptr;
}

std::vector<std::uint32_t> ActorPalettes::TakeRetiredSerials() {
  return std::exchange(retired_serials_, {});
}

}  // namespace ro::render
