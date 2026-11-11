// LES DEUX PEAUX DANS L'ÉDITEUR : la ligne « Peau du visage » sous l'onglet
// Tête, la ligne « Peau du corps » sous l'onglet Corps, et ce qu'elles envoient.
//
// 🔴 LA PEAU DU CORPS EST RANGÉE SOUS LA CLÉ DU CORPS QU'ON HABILLE, comme un
// vêtement : un autre corps, une autre monture, ont chacun la leur, et un corps
// qui n'en a pas prend celle du visage.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/style_set.h"

#include "character/style_recipe.h"
#include "character/style_view.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

constexpr int kFullValue = 255;

[[nodiscard]] ImU32 ToColor(std::array<std::uint8_t, 3> rgb) {
  return IM_COL32(rgb[0], rgb[1], rgb[2], kFullValue);
}

[[nodiscard]] ro::ui::PaletteTabBlock ToBlock(SkinRefusal refusal) {
  switch (refusal) {
    case SkinRefusal::kNone:
      return ro::ui::PaletteTabBlock::kNone;
    case SkinRefusal::kGmOutfit:
      return ro::ui::PaletteTabBlock::kGmOutfit;
    case SkinRefusal::kOnMount:
      return ro::ui::PaletteTabBlock::kSkinOnMount;
    case SkinRefusal::kNoBodySkinServer:
      return ro::ui::PaletteTabBlock::kBodySkinNotServed;
  }
  return ro::ui::PaletteTabBlock::kNone;
}

// La couleur de la peau du dessin, réglée.
[[nodiscard]] std::array<std::uint8_t, 3> Painted(const ro::formats::Palette& base,
                                                  const ro::render::RampAdjust& adjust) {
  const ro::render::PaletteRamp ramp = SkinRamp();
  const std::array<std::uint8_t, 3> drawn = ro::render::RampColor(base, ramp);
  if (adjust.neutral()) {
    return drawn;
  }
  std::array<std::uint8_t, 3> painted{};
  ro::render::AdjustColor(drawn.data(), adjust, painted.data());
  return painted;
}

}  // namespace

void StyleView::RefreshSkins(const ro::render::ActorLook& look) {
  skins_shown_ = !ro::render::IsDoramJob(look.job);
  skins_gm_outfit_ = look.player && look.gm_outfit;
  ro::render::ActorLook body_look = look;
  KeepBodyOnly(body_look);
  skin_body_paths_ = ro::render::BodySpritePaths(wardrobe_->catalog(), body_look);
}

SkinRefusal StyleView::SkinRefusalOf(SkinLine line) const {
  return SkinRefusalFor(line, skins_gm_outfit_, skin_body_paths_, server_layer_last_);
}

bool StyleView::SkinAdjustable(SkinLine line) const {
  return skins_shown_ && SkinRefusalOf(line) == SkinRefusal::kNone &&
         (line == SkinLine::kFace || session_.body_key() != 0);
}

std::optional<SkinLine> StyleView::OpenSkinLine() const {
  if (!skins_shown_) {
    return std::nullopt;
  }
  if (view_tab_ == ro::ui::PaletteTabKind::kHair) {
    return SkinLine::kFace;
  }
  if (view_tab_ == ro::ui::PaletteTabKind::kBody && session_.body_key() != 0) {
    return SkinLine::kBody;
  }
  return std::nullopt;
}

LayerAt StyleView::SkinAt(SkinLine line) const { return SkinLayerAt(line, session_.body_key()); }

ro::render::RampAdjust StyleView::SkinAdjust(SkinLine line) const {
  return layers_.Current(SkinAt(line), ro::render::kLayerProfileNumber).adjusts.front();
}

bool StyleView::SkinEdited(SkinLine line) const {
  return skins_shown_ && layers_.HasEdits(SkinAt(line));
}

void StyleView::RefreshSkinLine() {
  view_.skin = ro::ui::PaletteSkinLine{};
  if (skin_tones_.empty()) {
    for (const std::array<std::uint8_t, 3>& tone : kSkinTones) {
      skin_tones_.push_back(ToColor(tone));
    }
  }
  view_.skin_tones = skin_tones_;
  const std::optional<SkinLine> line = OpenSkinLine();
  if (!line.has_value() || !doll_.has_value()) {
    return;
  }
  const ro::formats::Palette& base = doll_->body().base;
  const ro::render::RampAdjust face = SkinAdjust(SkinLine::kFace);
  const ro::render::RampAdjust own = *line == SkinLine::kBody ? SkinAdjust(SkinLine::kBody) : face;
  const bool body = *line == SkinLine::kBody;
  // 🔴 UN CORPS SANS PEAU PROPRE MONTRE CELLE DU VISAGE : c'est celle qu'il
  // porte.
  const bool follows_face = body && own.neutral();
  view_.skin.shown = true;
  view_.skin.body = body;
  view_.skin.own = body && !follows_face;
  view_.skin.blocked = ToBlock(SkinRefusalOf(*line));
  view_.skin.adjust = own;
  view_.skin.base_color = ToColor(ro::render::RampColor(base, SkinRamp()));
  view_.skin.color = ToColor(Painted(base, follows_face ? face : own));
}

void StyleView::ApplySkinLine(const ro::ui::PaletteEditorAction& asked) {
  const std::optional<SkinLine> line = OpenSkinLine();
  if (!line.has_value() || !SkinAdjustable(*line) || !doll_.has_value()) {
    return;
  }
  const LayerAt at = SkinAt(*line);
  const ro::formats::Palette& base = doll_->body().base;
  const auto reach = [&base](std::array<std::uint8_t, 3> target) {
    return std::array<LayerEdit, 1>{
        LayerEdit{.rank = 0, .adjust = ro::render::AdjustToReach(base, SkinRamp(), target)}};
  };
  if (asked.skin_tone >= 0 && static_cast<std::size_t>(asked.skin_tone) < kSkinTones.size()) {
    layers_.Set(at, ro::render::kLayerProfileNumber,
                reach(kSkinTones[static_cast<std::size_t>(asked.skin_tone)]));
  }
  if (asked.skin_picked) {
    layers_.Set(at, ro::render::kLayerProfileNumber, reach(asked.skin_color), asked.stroke);
  }
  // 🔴 UN RÉGLAGE NEUTRE RETIRE LA PEAU : le visage retrouve celle du dessin, un
  // corps celle du visage.
  if (asked.skin_reset) {
    const std::array<LayerEdit, 1> neutral{LayerEdit{.rank = 0}};
    layers_.Set(at, ro::render::kLayerProfileNumber, neutral);
  }
}

bool StyleView::TrySkinCode(const ro::sim::StyleLayerCode& code) {
  if (code.layer != ro::sim::StyleLayerKind::kSkin || !SkinAdjustable(SkinLine::kFace)) {
    return false;
  }
  const ro::sim::StyleLayerRecipe told{
      .layer = code.layer, .key = 0, .profile = code.profile, .adjusts = code.adjusts};
  const std::optional<ro::render::LayerDye> dye = FromWire(told);
  if (!dye.has_value() || dye->profile != ro::render::kLayerProfileNumber) {
    return false;
  }
  layers_.Put(*dye);
  return true;
}

}  // namespace moonclient
