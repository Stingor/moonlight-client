// LE PROFIL MIXTE DANS L'ÉDITEUR : le reste de chaque onglet, réglé par classe
// de teinte sous ses pièces ; la suite et le reste du corps sous ses huit pièces
// v7 ; et les retouches exactes, couleur par couleur.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/palette_rest.h"

#include "character/style_view.h"

namespace moonclient {
namespace {

using ro::render::StyleLayer;

constexpr int kFullValue = 255;
// L'index où une palette d'une seule couleur pose sa couleur : le zéro est le
// transparent, qu'aucun calcul ne lit.
constexpr std::uint8_t kLoneIndex = 1;
constexpr ro::render::PaletteRamp kLoneRamp{.start = kLoneIndex, .length = 1, .ref = kLoneIndex};

[[nodiscard]] ImU32 ToColor(std::array<std::uint8_t, 3> rgb) {
  return IM_COL32(rgb[0], rgb[1], rgb[2], kFullValue);
}

// UNE PALETTE D'UNE SEULE COULEUR : la couleur moyenne d'une classe du reste,
// pour que le nuancier et `AdjustToReach` servent comme pour une pièce.
[[nodiscard]] ro::formats::Palette Lone(std::array<std::uint8_t, 3> rgb) {
  ro::formats::Palette palette;
  palette.entries[kLoneIndex] = {rgb[0], rgb[1], rgb[2], kFullValue};
  return palette;
}

// Les pièces d'un onglet, peintes sur sa base : les classes au profil 2, les
// rampes sinon.
[[nodiscard]] ro::formats::Palette PaintedPieces(const ro::render::LayerDye& dye,
                                                 const ro::formats::Palette& base,
                                                 std::span<const ro::render::PaletteRamp> ramps) {
  if (dye.profile == ro::render::kHueClassProfileNumber) {
    return ro::render::ApplyHueClasses(base, ro::render::LayerHueClasses(dye));
  }
  return ro::render::ApplyRecipe(base, ramps, ro::render::LayerRecipe(dye));
}

}  // namespace

void StyleView::FillRest(LayerTab& tab, const ro::render::RestMeasure& rest) const {
  tab.rest_classes.clear();
  tab.rest_means.clear();
  const bool takes_rest =
      mixed_allowed_ && (tab.profile == ro::render::kLayerProfileNumber ||
                         tab.profile == ro::render::kBodyContinuationProfileNumber);
  if (!takes_rest) {
    return;
  }
  for (std::size_t hue_class = 0; hue_class < ro::render::kHueClassCount; ++hue_class) {
    if (rest.pixels[hue_class] > 0) {
      tab.rest_classes.push_back(hue_class);
      tab.rest_means.push_back(rest.mean[hue_class]);
    }
  }
  // 🔴 « RÉGLABLE » DIT LA NOUVELLE COUVERTURE : les pièces, puis le reste, sauf
  // les noirs de contour qu'aucun réglage ne fait sortir du noir.
  tab.shown.coverage_percent = rest.ReachablePercent();
}

std::size_t StyleView::RowCount(const LayerTab& tab) {
  return tab.ramps.size() + tab.rest_classes.size();
}

std::size_t StyleView::EditRank(const LayerTab& tab, std::size_t row) {
  if (row < tab.ramps.size()) {
    return row;
  }
  return kRestRankFirst + tab.rest_classes[row - tab.ramps.size()];
}

std::array<std::uint8_t, 3> StyleView::RowBaseColor(const LayerTab& tab, std::size_t row) {
  if (row < tab.ramps.size()) {
    return ro::render::RampColor(tab.base, tab.ramps[row]);
  }
  return tab.rest_means[row - tab.ramps.size()];
}

int StyleView::RowAt(const LayerTab& tab, std::uint8_t index, std::size_t first) {
  if (const std::optional<std::size_t> rank = ro::render::RampAt(tab.ramps, index)) {
    return static_cast<int>(first + *rank);
  }
  if (tab.rest_classes.empty()) {
    return -1;
  }
  const auto& entry = tab.sprite_base.entries[index];
  const std::size_t hue_class = ro::render::HueClassOf(entry[0], entry[1], entry[2]);
  const auto found = std::ranges::find(tab.rest_classes, hue_class);
  if (found == tab.rest_classes.end()) {
    return -1;
  }
  return static_cast<int>(first + tab.ramps.size() +
                          static_cast<std::size_t>(found - tab.rest_classes.begin()));
}

void StyleView::AppendRows(const LayerTab& tab, const ro::render::LayerDye& dye, const Rows& rows) {
  const ro::formats::Palette painted = PaintedPieces(dye, tab.base, tab.ramps);
  for (std::size_t rank = 0; rank < tab.ramps.size() && rank < dye.adjusts.size(); ++rank) {
    const ro::render::PaletteRamp& ramp = tab.ramps[rank];
    rows.ramps->push_back(ramp);
    rows.adjusts->push_back(dye.adjusts[rank]);
    rows.painted->push_back(ro::render::RampColor(painted, ramp));
    rows.colors->push_back(ToColor(rows.painted->back()));
    rows.base_colors->push_back(ToColor(ro::render::RampColor(tab.base, ramp)));
    rows.ceilings->push_back(ro::render::RampValueCeiling(tab.base, ramp));
  }
  for (std::size_t at = 0; at < tab.rest_classes.size(); ++at) {
    const ro::render::RampAdjust& adjust = dye.rest[tab.rest_classes[at]];
    const ro::formats::Palette lone = Lone(tab.rest_means[at]);
    std::array<std::uint8_t, 3> shown = tab.rest_means[at];
    if (!adjust.neutral()) {
      ro::render::AdjustColor(tab.rest_means[at].data(), adjust, shown.data());
    }
    // ⚠ UNE RANGÉE DU RESTE N'A PAS DE RAMPE : celle d'une couleur seule tient
    // sa place dans la liste.
    rows.ramps->push_back(kLoneRamp);
    rows.adjusts->push_back(adjust);
    rows.painted->push_back(shown);
    rows.colors->push_back(ToColor(shown));
    rows.base_colors->push_back(ToColor(tab.rest_means[at]));
    rows.ceilings->push_back(ro::render::RampValueCeiling(lone, kLoneRamp));
  }
}

ro::formats::Palette StyleView::PaintedPalette(const LayerTab& tab,
                                               const ro::render::LayerDye& dye) {
  ro::formats::Palette painted;
  if (dye.profile == ro::render::kHueClassProfileNumber) {
    painted = ro::render::ApplyHueClasses(tab.sprite_base, ro::render::LayerHueClasses(dye));
  } else {
    painted = PaintedPieces(dye, tab.base, tab.ramps);
    ro::render::ApplyLayerRest(painted, tab.base, tab.ramps, tab.race, dye);
  }
  ro::render::ApplyRetouches(painted, dye.retouches);
  return painted;
}

void StyleView::RefreshRetouchView(const LayerTab& tab, const ro::render::LayerDye& dye) {
  palette_colors_.clear();
  view_retouches_.clear();
  view_.palette_colors = {};
  view_.retouches = {};
  view_.retouches_allowed = tab.retouchable;
  if (!tab.retouchable) {
    return;
  }
  const ro::formats::Palette painted =
      tab.at.layer == StyleLayer::kBodyMore ? tab.base : PaintedPalette(tab, dye);
  palette_colors_.reserve(ro::formats::kPaletteEntryCount);
  for (const auto& entry : painted.entries) {
    palette_colors_.push_back(IM_COL32(entry[0], entry[1], entry[2], kFullValue));
  }
  view_retouches_ = dye.retouches;
  view_.retouches_most = retouches_most_;
  view_.retouches = view_retouches_;
  view_.palette_colors = palette_colors_;
}

void StyleView::ApplyRetouch(const ro::ui::PaletteEditorAction& asked, const LayerTab& tab) {
  if (!tab.retouchable) {
    return;
  }
  if (asked.retouch_index > 0 && asked.retouch_index < static_cast<int>(kFullValue + 1)) {
    static_cast<void>(layers_.Retouch(
        tab.at, tab.profile,
        ro::render::PaletteRetouch{.index = static_cast<std::uint8_t>(asked.retouch_index),
                                   .rgb = asked.retouch_color},
        asked.stroke));
  }
  if (asked.remove_retouch > 0 && asked.remove_retouch < static_cast<int>(kFullValue + 1)) {
    layers_.Unretouch(tab.at, tab.profile, static_cast<std::uint8_t>(asked.remove_retouch));
  }
}

void StyleView::RefreshBodyMore(const ro::render::ActorWardrobe::BodyRamps& body) {
  body_more_ = LayerTab{};
  body_more_.shown.kind = ro::ui::PaletteTabKind::kBody;
  body_more_.at = LayerAt{.layer = StyleLayer::kBodyMore, .key = session_.body_key()};
  body_more_.part = ro::render::ActorPart::kBody;
  body_more_.profile = ro::render::kBodyContinuationProfileNumber;
  body_more_.base = body.base;
  body_more_.sprite_base = body.base;
  body_more_.ramps = body.continuation;
  body_more_.race = ro::render::IsDoramJob(tabs_look_.has_value() ? tabs_look_->job : 0)
                        ? ro::render::SkinRace::kDoram
                        : ro::render::SkinRace::kHuman;
  FillRest(body_more_, body.rest);
  body_more_.retouchable = mixed_allowed_ && !body.ramps.empty();

  // ── LES RANGÉES DE L'ONGLET CORPS : les huit v7, puis la suite et le reste ─
  body_rows_.assign(body.ramps.begin(), body.ramps.end());
  body_adjusts_.assign(
      session_.recipe().ramps.begin(),
      session_.recipe().ramps.begin() +
          static_cast<std::ptrdiff_t>(std::min(body.ramps.size(), ro::render::kMaxRamps)));
  body_colors_.assign(colors_.begin(), colors_.end());
  body_base_colors_.assign(base_colors_.begin(), base_colors_.end());
  body_ceilings_.assign(ceilings_.begin(), ceilings_.end());
  body_painted_.assign(body.ramps.size(), std::array<std::uint8_t, 3>{});
  const ro::render::LayerDye more = layers_.Current(body_more_.at, body_more_.profile);
  AppendRows(body_more_, more,
             Rows{&body_rows_, &body_adjusts_, &body_colors_, &body_base_colors_, &body_ceilings_,
                  &body_painted_});
  view_.ramps = body_rows_;
  view_.adjusts = body_adjusts_;
  view_.colors = body_colors_;
  view_.base_colors = body_base_colors_;
  view_.ceilings = body_ceilings_;
  view_.rest_first = body_more_.rest_classes.empty()
                         ? -1
                         : static_cast<int>(body.ramps.size() + body_more_.ramps.size());
  if (!body_more_.rest_classes.empty()) {
    view_.coverage_percent = body_more_.shown.coverage_percent;
  }

  // ── LA PALETTE DU CORPS, telle qu'il la portera : celle des retouches ────
  palette_colors_.clear();
  view_retouches_.clear();
  view_.palette_colors = {};
  view_.retouches = {};
  view_.retouches_allowed = body_more_.retouchable;
  if (!body_more_.retouchable) {
    return;
  }
  ro::formats::Palette painted = ro::render::ApplyRecipe(body.base, body.ramps, session_.recipe());
  ro::render::ApplyBodyMore(painted, body.base, body.ramps, body.continuation, body_more_.race,
                            more);
  ro::render::ApplyRetouches(painted, more.retouches);
  palette_colors_.reserve(ro::formats::kPaletteEntryCount);
  for (const auto& entry : painted.entries) {
    palette_colors_.push_back(IM_COL32(entry[0], entry[1], entry[2], kFullValue));
  }
  view_retouches_ = more.retouches;
  view_.retouches_most = retouches_most_;
  view_.retouches = view_retouches_;
  view_.palette_colors = palette_colors_;
}

void StyleView::ApplyBodyMore(const ro::ui::PaletteEditorAction& asked, Harmony harmony,
                              std::uint32_t now_ms) {
  if (!doll_.has_value()) {
    return;
  }
  const std::size_t v7 = doll_->body().ramps.size();
  // 🔴 LES RANGÉES AU-DELÀ DES HUIT PIÈCES V7 SONT CELLES DU CALQUE « CORPS,
  // SUITE » : le geste se décale, et passe par le chemin des calques. Un lien
  // entre une pièce v7 et une rangée de la suite ne se fait pas : ce sont deux
  // teintures.
  const auto shift = [v7](int row) {
    return row >= 0 && static_cast<std::size_t>(row) >= v7 ? row - static_cast<int>(v7) : -1;
  };
  ro::ui::PaletteEditorAction shifted;
  shifted.stroke = asked.stroke;
  shifted.picked_ramp = shift(asked.picked_ramp);
  shifted.picked_color = asked.picked_color;
  shifted.adjusted_ramp = shift(asked.adjusted_ramp);
  shifted.adjust = asked.adjust;
  shifted.reset_ramp = shift(asked.reset_ramp);
  shifted.unlink = shift(asked.unlink);
  shifted.toggle_lock = shift(asked.toggle_lock);
  if (shift(asked.link_from) >= 0 && shift(asked.link_to) >= 0) {
    shifted.link_from = shift(asked.link_from);
    shifted.link_to = shift(asked.link_to);
  }
  shifted.verb = asked.verb;
  shifted.harmony = asked.harmony;
  shifted.retouch_index = asked.retouch_index;
  shifted.retouch_color = asked.retouch_color;
  shifted.remove_retouch = asked.remove_retouch;
  ApplyLayerPieces(shifted, body_more_, harmony, now_ms);
}

}  // namespace moonclient
