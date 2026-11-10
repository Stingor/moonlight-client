// LES CALQUES DE L'ÉDITEUR DE STYLE : les onglets des calques portés, leurs
// pièces, la pipette qui choisit l'onglet, et les teintures qui partent au
// serveur.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/net/player_style_layers.h"
#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/style_layer_code.h"
#include "ro/sim/world.h"

#include "character/style_recipe.h"
#include "character/style_view.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

using ro::render::ActorPart;
using ro::render::StyleLayer;
using ro::ui::PaletteTabBlock;
using ro::ui::PaletteTabKind;

constexpr int kPercent = 100;
constexpr int kFullValue = 255;
constexpr int kHalfTurn = 180;
constexpr int kTurn = 360;
// Un mélange pour la graine de l'inspiration : deux tirages rapprochés dans le
// temps ne doivent pas se ressembler.
constexpr std::uint32_t kSeedMix = 0x9E3779B9U;

// LA COULEUR QUI REPRÉSENTE UNE CLASSE DE TEINTE : la teinte du secteur, une
// saturation franche, une clarté moyenne — de quoi lire « les rouges » sur une
// pastille. Le gris pour les gris.
constexpr std::array<std::uint8_t, 3> kClassSeed = {204, 71, 71};
constexpr std::int8_t kClassSaturation = 65;
constexpr int kHueClassDegrees = 30;
constexpr std::array<std::uint8_t, 3> kGreySeed = {178, 178, 178};

// LES TEINTS PROPOSÉS, du plus clair au plus foncé.
constexpr std::array<std::array<std::uint8_t, 3>, 7> kSkinTones = {{
    {255, 228, 210},
    {244, 204, 176},
    {228, 180, 146},
    {204, 148, 110},
    {168, 116, 80},
    {128, 84, 56},
    {88, 56, 38},
}};

// Le mot bas d'une clé de tête : la couleur de cheveux.
constexpr std::uint32_t kHairPaletteMask = 0xFFFFU;
constexpr unsigned kHairStyleShift = 16;

// LES PLACES D'UN OBJET TEINTABLE : sa partie, où le lire sur l'apparence, où le
// chercher dans l'équipement, et comment le désigner au serveur.
struct ItemPlace {
  PaletteTabKind kind;
  ActorPart part;
  StyleLayer layer;
  // Le rang dans `ActorLook::headgears`, de la basse à la haute ; la cape n'en
  // a pas.
  std::optional<std::size_t> headgear;
  std::uint32_t ordinary;
  std::uint32_t costume;
  ro::net::StyleSlot ordinary_slot;
  ro::net::StyleSlot costume_slot;
};
constexpr std::size_t kTopRank = 2;
constexpr std::size_t kMiddleRank = 1;
constexpr std::size_t kLowRank = 0;
const std::array<ItemPlace, 4> kItemPlaces = {{
    {PaletteTabKind::kHeadgearTop, ActorPart::kHeadgearTop, StyleLayer::kHeadgear, kTopRank,
     ro::sim::kEquipHeadTop, ro::sim::kEquipCostumeHeadTop, ro::net::StyleSlot::kHeadTop,
     ro::net::StyleSlot::kCostumeHeadTop},
    {PaletteTabKind::kHeadgearMiddle, ActorPart::kHeadgearMiddle, StyleLayer::kHeadgear,
     kMiddleRank, ro::sim::kEquipHeadMid, ro::sim::kEquipCostumeHeadMid,
     ro::net::StyleSlot::kHeadMid, ro::net::StyleSlot::kCostumeHeadMid},
    {PaletteTabKind::kHeadgearLow, ActorPart::kHeadgearLow, StyleLayer::kHeadgear, kLowRank,
     ro::sim::kEquipHeadLow, ro::sim::kEquipCostumeHeadLow, ro::net::StyleSlot::kHeadLow,
     ro::net::StyleSlot::kCostumeHeadLow},
    {PaletteTabKind::kGarment, ActorPart::kGarment, StyleLayer::kGarment, std::nullopt,
     ro::sim::kEquipGarment, ro::sim::kEquipCostumeGarment, ro::net::StyleSlot::kGarment,
     ro::net::StyleSlot::kCostumeGarment},
}};

[[nodiscard]] std::uint16_t ViewAt(const ro::render::ActorLook& look, const ItemPlace& place) {
  return place.headgear.has_value() ? look.headgears[*place.headgear] : look.robe;
}

[[nodiscard]] ImU32 ToColor(std::array<std::uint8_t, 3> rgb) {
  return IM_COL32(rgb[0], rgb[1], rgb[2], kFullValue);
}

[[nodiscard]] std::array<std::uint8_t, 3> Entry(const ro::formats::Palette& palette,
                                                std::size_t index) {
  return {palette.entries[index][0], palette.entries[index][1], palette.entries[index][2]};
}

// L'OBJET QUE L'ÉQUIPEMENT MONTRE À CETTE PLACE : celui dont l'apparence est
// celle affichée, le costume d'abord ; à défaut, ce qui y est porté.
struct ShownItem {
  const ro::sim::InventoryItem* item = nullptr;
  bool costume = false;
};
[[nodiscard]] ShownItem FindShown(const ro::sim::Inventory& inventory, const ItemPlace& place,
                                  std::uint16_t view) {
  const ro::sim::InventoryItem* costume = inventory.WornAt(place.costume);
  const ro::sim::InventoryItem* ordinary = inventory.WornAt(place.ordinary);
  if (costume != nullptr && costume->view_id == view) {
    return {costume, true};
  }
  if (ordinary != nullptr && ordinary->view_id == view) {
    return {ordinary, false};
  }
  if (costume != nullptr) {
    return {costume, true};
  }
  return {ordinary, false};
}

// LES CLASSES DE TEINTE EN PIÈCES : une couleur représentative par classe, et
// une « rampe » d'un index qui la désigne. C'est ce qui laisse la liste des
// pièces, le nuancier et `AdjustToReach` servir tels quels.
void HueClassPieces(ro::formats::Palette& base, std::vector<ro::render::PaletteRamp>& ramps) {
  base = ro::formats::Palette{};
  ramps.clear();
  for (std::size_t rank = 0; rank < ro::render::kHueClassCount; ++rank) {
    std::array<std::uint8_t, 3> color = kGreySeed;
    if (rank != ro::render::kGreyHueClass) {
      const ro::render::RampAdjust toward{
          .hue = static_cast<std::int16_t>(static_cast<int>(rank) * kHueClassDegrees),
          .saturation = kClassSaturation,
          .absolute = true};
      ro::render::AdjustColor(kClassSeed.data(), toward, color.data());
    }
    const auto index = static_cast<std::uint8_t>(rank + 1);
    base.entries[index] = {color[0], color[1], color[2], kFullValue};
    ramps.push_back(ro::render::PaletteRamp{.start = index, .length = 1, .ref = index});
  }
}

// La palette d'une teinture posée sur la base de son onglet.
[[nodiscard]] ro::formats::Palette Painted(const ro::render::LayerDye& dye,
                                           const ro::formats::Palette& base,
                                           std::span<const ro::render::PaletteRamp> ramps) {
  if (dye.layer == StyleLayer::kSkin) {
    ro::formats::Palette painted = base;
    ro::render::ApplySkin(painted, base, ro::render::SkinRace::kHuman, dye.adjusts[0]);
    return painted;
  }
  if (dye.profile == ro::render::kHueClassProfileNumber) {
    return ro::render::ApplyHueClasses(base, ro::render::LayerHueClasses(dye));
  }
  return ro::render::ApplyRecipe(base, ramps, ro::render::LayerRecipe(dye));
}

// L'écart entre deux teintes, en degrés, sur le cercle.
[[nodiscard]] int HueGap(int left, int right) {
  const int gap = std::abs(left - right) % kTurn;
  return gap > kHalfTurn ? kTurn - gap : gap;
}

[[nodiscard]] const char* RefusalText(std::optional<ro::sim::StyleVerdictResult> refusal) {
  if (!refusal.has_value()) {
    return nullptr;
  }
  switch (*refusal) {
    case ro::sim::StyleVerdictResult::kAccepted:
    case ro::sim::StyleVerdictResult::kSame:
      return nullptr;
    case ro::sim::StyleVerdictResult::kTooSoon:
      return ro::core::Tr("Trop rapproché de la demande précédente : attends une seconde.");
    case ro::sim::StyleVerdictResult::kMalformed:
      return ro::core::Tr("Le serveur n'a pas compris la demande.");
    case ro::sim::StyleVerdictResult::kCap:
      return ro::core::Tr("Trop de teintures pour le serveur.");
    case ro::sim::StyleVerdictResult::kKeyRefused:
      return ro::core::Tr("Le serveur refuse cette coupe, cette couleur ou cet item.");
    case ro::sim::StyleVerdictResult::kNoAtelier:
      return ro::core::Tr("Une teinture d'item se valide auprès du styliste.");
    case ro::sim::StyleVerdictResult::kNotWorn:
      return ro::core::Tr("L'item n'est plus porté à sa place.");
    case ro::sim::StyleVerdictResult::kBusy:
      return ro::core::Tr(
          "Impossible pendant un échange, une boutique, un storage ou un "
          "dialogue, ni une fois à terre.");
    case ro::sim::StyleVerdictResult::kUnavailable:
      return ro::core::Tr("Le serveur ne peut pas ranger les teintures pour l'instant.");
  }
  return nullptr;
}

[[nodiscard]] bool SameLook(const ro::render::ActorLook& left, const ro::render::ActorLook& right) {
  const ro::render::ActorLookOrder before;
  return !before(left, right) && !before(right, left);
}

}  // namespace

// ── LES ONGLETS ─────────────────────────────────────────────────────────────

void StyleView::RefreshTabs(const ro::render::ActorLook& look, const ro::sim::World& world) {
  // 🔴 LA MESURE SE REFAIT AU CHANGEMENT D'APPARENCE, les objets à chaque
  // image : un chapeau échangé pour un autre de même apparence ne change pas
  // l'apparence, mais change l'exemplaire qu'on teint.
  const bool measured = tabs_look_.has_value() && SameLook(*tabs_look_, look) &&
                        tabs_generation_ == doll_->generation();
  if (!measured) {
    tabs_look_ = look;
    tabs_generation_ = doll_->generation();
    tabs_.clear();
    const bool doram = ro::render::IsDoramJob(look.job);
    const bool gm_outfit = look.player && look.gm_outfit;

    // ── LA PEAU : un Doram n'en a pas d'onglet ─────────────────────────────
    if (!doram) {
      LayerTab skin;
      skin.shown.kind = PaletteTabKind::kSkin;
      skin.at = LayerAt{.layer = StyleLayer::kSkin, .key = 0};
      ro::render::ActorLook body_look = look;
      KeepBodyOnly(body_look);
      const std::vector<std::string> paths =
          ro::render::BodySpritePaths(wardrobe_->catalog(), body_look);
      const bool on_mount = std::ranges::any_of(
          paths, [](const std::string& path) { return !ro::render::SkinAdjustable(path); });
      skin.shown.blocked = gm_outfit  ? PaletteTabBlock::kGmOutfit
                           : on_mount ? PaletteTabBlock::kSkinOnMount
                                      : PaletteTabBlock::kNone;
      skin.base = doll_->body().base;
      skin.sprite_base = skin.base;
      const std::span<const std::uint8_t> indices =
          ro::render::SkinIndices(ro::render::SkinRace::kHuman);
      if (!indices.empty()) {
        skin.ramps.push_back(
            ro::render::PaletteRamp{.start = indices.front(),
                                    .length = static_cast<std::uint8_t>(indices.size()),
                                    .ref = indices[indices.size() / 2]});
      }
      tabs_.push_back(std::move(skin));
    }

    // ── LES CHEVEUX ────────────────────────────────────────────────────────
    {
      LayerTab hair;
      hair.shown.kind = PaletteTabKind::kHair;
      hair.part = ActorPart::kHead;
      hair.at = LayerAt{.layer = StyleLayer::kHead,
                        .key = ro::render::HeadDyeKey(look.hair, look.hair_palette)};
      const ro::render::ActorWardrobe::BodyRamps measure =
          wardrobe_->LayerRampsFor(look, ActorPart::kHead);
      hair.base = measure.base;
      hair.sprite_base = measure.base;
      hair.ramps = measure.ramps;
      if (measure.pixels_total > 0) {
        hair.shown.coverage_percent = static_cast<int>(
            (static_cast<long long>(measure.pixels_covered) * kPercent + measure.pixels_total / 2) /
            measure.pixels_total);
      }
      hair.shown.blocked = gm_outfit            ? PaletteTabBlock::kGmOutfit
                           : look.orc_head      ? PaletteTabBlock::kOrcHead
                           : hair.ramps.empty() ? PaletteTabBlock::kNoPiece
                                                : PaletteTabBlock::kNone;
      tabs_.push_back(std::move(hair));
    }

    // ── LES OBJETS : un onglet par apparence affichée ──────────────────────
    std::vector<std::uint16_t> seen;
    for (const ItemPlace& place : kItemPlaces) {
      const std::uint16_t view = ViewAt(look, place);
      // ⚠ UN CHAPEAU QUI COUVRE DEUX PLACES N'A QU'UN ONGLET, celui de sa place
      // la plus haute.
      if (view == 0 || (place.headgear.has_value() && std::ranges::contains(seen, view))) {
        continue;
      }
      if (place.headgear.has_value()) {
        seen.push_back(view);
      }
      LayerTab item;
      item.shown.kind = place.kind;
      item.shown.item = true;
      item.part = place.part;
      item.at = LayerAt{.layer = place.layer, .key = view};
      const ro::render::ActorWardrobe::BodyRamps measure =
          wardrobe_->LayerRampsFor(look, place.part);
      item.sprite_base = measure.base;
      // 🔴 SANS PIÈCE ASSEZ GRANDE, OU EN COULEURS VRAIES, LES CLASSES DE TEINTE :
      // un objet porté se teint toujours.
      if (!measure.ramps.empty()) {
        item.base = measure.base;
        item.ramps = measure.ramps;
        item.profile = ro::render::kLayerProfileNumber;
        if (measure.pixels_total > 0) {
          item.shown.coverage_percent =
              static_cast<int>((static_cast<long long>(measure.pixels_covered) * kPercent +
                                measure.pixels_total / 2) /
                               measure.pixels_total);
        }
      } else {
        HueClassPieces(item.base, item.ramps);
        item.profile = ro::render::kHueClassProfileNumber;
        item.shown.coverage_percent = kPercent;
      }
      tabs_.push_back(std::move(item));
    }
  }

  // ── CE QUI CHANGE SANS L'APPARENCE : l'exemplaire, les teintures ──────────
  const ro::sim::Inventory& inventory = world.inventory();
  for (LayerTab& tab : tabs_) {
    if (tab.shown.item) {
      for (const ItemPlace& place : kItemPlaces) {
        if (place.kind != tab.shown.kind) {
          continue;
        }
        const ShownItem shown = FindShown(inventory, place, static_cast<std::uint16_t>(tab.at.key));
        tab.item_id = shown.item != nullptr ? shown.item->item_id : 0;
        tab.slot = shown.item != nullptr
                       ? std::optional(shown.costume ? place.costume_slot : place.ordinary_slot)
                       : std::nullopt;
        tab.shown.blocked =
            shown.item == nullptr ? PaletteTabBlock::kNotFound : PaletteTabBlock::kNone;
        const ro::vfs::ItemEntry* entry =
            shown.item != nullptr ? wardrobe_->catalog().items.Find(tab.item_id) : nullptr;
        tab.shown.name = entry != nullptr && !entry->name.empty()
                             ? entry->name
                             : (tab.item_id != 0 ? "#" + std::to_string(tab.item_id) : "");
      }
    }
    const ro::render::LayerDye* worn = layers_.Worn(tab.at);
    tab.shown.dyed = worn != nullptr && !NeutralDye(*worn);
    tab.shown.edited = layers_.HasEdits(tab.at);
  }
  tab_views_.clear();
  tab_views_.push_back(ro::ui::PaletteTab{
      .kind = PaletteTabKind::kBody, .edited = session_.has_edits(), .dyed = session_.own_style()});
  for (const LayerTab& tab : tabs_) {
    tab_views_.push_back(tab.shown);
  }
}

const StyleView::LayerTab* StyleView::TabOf(ro::ui::PaletteTabKind kind) const {
  for (const LayerTab& tab : tabs_) {
    if (tab.shown.kind == kind) {
      return &tab;
    }
  }
  return nullptr;
}

const StyleView::LayerTab* StyleView::OpenLayer() const {
  return view_tab_ == PaletteTabKind::kBody ? nullptr : TabOf(view_tab_);
}

// ── CE QUE LA VUE MONTRE D'UN CALQUE ────────────────────────────────────────

void StyleView::RefreshLayerPieces(const LayerTab& tab) {
  const ro::render::LayerDye dye = layers_.Current(tab.at, tab.profile);
  const ro::formats::Palette painted = Painted(dye, tab.base, tab.ramps);
  layer_adjusts_.clear();
  layer_colors_.clear();
  layer_base_colors_.clear();
  layer_ceilings_.clear();
  layer_painted_.clear();
  for (std::size_t rank = 0; rank < tab.ramps.size() && rank < dye.adjusts.size(); ++rank) {
    const ro::render::PaletteRamp& ramp = tab.ramps[rank];
    layer_adjusts_.push_back(dye.adjusts[rank]);
    layer_painted_.push_back(ro::render::RampColor(painted, ramp));
    layer_colors_.push_back(ToColor(layer_painted_.back()));
    layer_base_colors_.push_back(ToColor(ro::render::RampColor(tab.base, ramp)));
    layer_ceilings_.push_back(ro::render::RampValueCeiling(tab.base, ramp));
  }
  ro::sim::StyleLayerCode code;
  const ro::sim::StyleLayerRecipe told = ToWire(dye);
  code.layer = told.layer;
  code.profile = told.profile;
  code.adjusts = told.adjusts;
  layer_code_ = ro::sim::EncodeStyleLayerCode(code);

  view_.ramps = tab.ramps;
  view_.adjusts = layer_adjusts_;
  view_.colors = layer_colors_;
  view_.base_colors = layer_base_colors_;
  view_.ceilings = layer_ceilings_;
  view_.coverage_percent = tab.shown.coverage_percent;
  view_.hue_classes = tab.profile == ro::render::kHueClassProfileNumber;
  const PieceMarks& marks = layer_marks_[tab.at];
  view_.links = marks.links();
  view_.locked = marks.locked();
  view_.has_edits = layers_.HasEdits(tab.at);
  view_.can_undo = layers_.can_undo();
  view_.can_redo = layers_.can_redo();
  view_.share_code = layer_code_.c_str();
  view_.code_shares_in_chat = false;
  view_.can_adapt_cut = tab.shown.kind == PaletteTabKind::kHair && !tab.ramps.empty() &&
                        layers_.PreviousHead(tab.at.key).has_value() && !layers_.HasEdits(tab.at);
  if (tab.shown.kind == PaletteTabKind::kSkin) {
    skin_tones_.clear();
    for (const std::array<std::uint8_t, 3>& tone : kSkinTones) {
      skin_tones_.push_back(ToColor(tone));
    }
    view_.skin_tones = skin_tones_;
  }
}

void StyleView::RefreshLayerState(const ro::sim::World& world, std::uint32_t now_ms) {
  std::vector<ro::render::LayerDye> worn;
  if (const std::vector<ro::sim::StyleLayerRecipe>* told =
          world.styles().FindLayers(world.player_id())) {
    for (const ro::sim::StyleLayerRecipe& recipe : *told) {
      if (const std::optional<ro::render::LayerDye> dye = FromWire(recipe)) {
        worn.push_back(*dye);
      }
    }
  }
  layers_.Observe(worn, world.styles().RevisionOf(world.player_id()));
  atelier_open_ = world.styles().atelier().OpenAt(now_ms);
}

ro::render::ActorLook StyleView::DollLook(const ro::render::ActorLook& body_look,
                                          const ro::render::ActorLook& full_look) const {
  ro::render::ActorLook look = body_look;
  const LayerTab* tab = OpenLayer();
  if (tab == nullptr) {
    return look;
  }
  // ⚠ LE CALQUE RÉGLÉ SEUL, sur le corps qu'on règle : une autre coiffe
  // masquerait celle qu'on regarde.
  for (const ItemPlace& place : kItemPlaces) {
    if (place.kind != tab->shown.kind) {
      continue;
    }
    if (place.headgear.has_value()) {
      look.headgears[*place.headgear] = full_look.headgears[*place.headgear];
    } else {
      look.robe = full_look.robe;
    }
  }
  return look;
}

void StyleView::ReadPipette(const ro::ui::PaletteEditorState& state) {
  if (!state.doll.hovered) {
    return;
  }
  const std::optional<ro::render::PalettePick> pick =
      doll_->PickUnder(glm::vec2(state.doll.hover_x, state.doll.hover_y));
  if (!pick.has_value()) {
    return;
  }
  ro::ui::PalettePipette found;
  const LayerTab* skin = TabOf(PaletteTabKind::kSkin);
  const bool skin_index =
      skin != nullptr && skin->shown.blocked == PaletteTabBlock::kNone &&
      std::ranges::contains(ro::render::SkinIndices(ro::render::SkinRace::kHuman), pick->index);
  if ((pick->part == ActorPart::kBody || pick->part == ActorPart::kHead) && skin_index) {
    found = {.found = true, .tab = PaletteTabKind::kSkin, .piece = 0};
  } else if (pick->part == ActorPart::kBody) {
    const std::optional<std::size_t> rank = ro::render::RampAt(doll_->body().ramps, pick->index);
    found = {.found = true,
             .tab = PaletteTabKind::kBody,
             .piece = rank.has_value() ? static_cast<int>(*rank) : -1};
  } else {
    for (const LayerTab& tab : tabs_) {
      if (tab.part != pick->part || tab.shown.blocked != PaletteTabBlock::kNone) {
        continue;
      }
      int piece = -1;
      if (tab.profile == ro::render::kHueClassProfileNumber) {
        const std::array<std::uint8_t, 3> color = Entry(tab.sprite_base, pick->index);
        piece = static_cast<int>(ro::render::HueClassOf(color[0], color[1], color[2]));
      } else if (const std::optional<std::size_t> rank =
                     ro::render::RampAt(tab.ramps, pick->index)) {
        piece = static_cast<int>(*rank);
      }
      found = {.found = true, .tab = tab.shown.kind, .piece = piece};
      break;
    }
  }
  view_.pipette = found;
  if (found.found && found.tab == view_tab_) {
    view_.pipette_ramp = found.piece;
  }
}

// ── LES GESTES ──────────────────────────────────────────────────────────────

std::vector<LayerEdit> StyleView::Reach(const LayerTab& tab, std::span<const std::size_t> ranks,
                                        std::array<std::uint8_t, 3> target) const {
  std::vector<LayerEdit> edits;
  for (const std::size_t rank : ranks) {
    if (rank < tab.ramps.size()) {
      edits.push_back(LayerEdit{
          .rank = rank, .adjust = ro::render::AdjustToReach(tab.base, tab.ramps[rank], target)});
    }
  }
  return edits;
}

void StyleView::ApplyLayerPieces(const ro::ui::PaletteEditorAction& asked, const LayerTab& tab,
                                 Harmony harmony, std::uint32_t now_ms) {
  PieceMarks& marks = layer_marks_[tab.at];
  const std::size_t count = tab.ramps.size();
  const auto in_range = [count](int rank) {
    return rank >= 0 && static_cast<std::size_t>(rank) < count;
  };
  if (in_range(asked.picked_ramp)) {
    const std::vector<std::size_t> ranks =
        marks.Reached(static_cast<std::size_t>(asked.picked_ramp));
    layers_.Set(tab.at, tab.profile, Reach(tab, ranks, asked.picked_color), asked.stroke);
  }
  if (in_range(asked.adjusted_ramp)) {
    std::vector<LayerEdit> edits;
    for (const std::size_t rank : marks.Reached(static_cast<std::size_t>(asked.adjusted_ramp))) {
      edits.push_back(LayerEdit{.rank = rank, .adjust = asked.adjust});
    }
    layers_.Set(tab.at, tab.profile, edits, asked.stroke);
  }
  if (in_range(asked.reset_ramp)) {
    const std::array<LayerEdit, 1> neutral{
        LayerEdit{.rank = static_cast<std::size_t>(asked.reset_ramp)}};
    layers_.Set(tab.at, tab.profile, neutral);
  }
  if (in_range(asked.link_from) && in_range(asked.link_to)) {
    marks.Link(static_cast<std::size_t>(asked.link_from), static_cast<std::size_t>(asked.link_to));
  }
  if (in_range(asked.unlink)) {
    marks.Unlink(static_cast<std::size_t>(asked.unlink));
  }
  if (in_range(asked.toggle_lock)) {
    marks.ToggleLock(static_cast<std::size_t>(asked.toggle_lock));
  }
  if (asked.verb == ro::ui::PaletteEditorVerb::kInspire && count > 0) {
    std::vector<std::array<std::uint8_t, 3>> origin;
    origin.reserve(count);
    for (const ro::render::PaletteRamp& ramp : tab.ramps) {
      origin.push_back(ro::render::RampColor(tab.base, ramp));
    }
    ++inspirations_;
    const auto inspired = Inspire(origin, marks, harmony, now_ms ^ (inspirations_ * kSeedMix));
    std::vector<LayerEdit> edits;
    for (std::size_t rank = 0; rank < inspired.size(); ++rank) {
      if (inspired[rank].has_value()) {
        const std::array<std::size_t, 1> one{rank};
        const std::vector<LayerEdit> reached = Reach(tab, one, *inspired[rank]);
        edits.insert(edits.end(), reached.begin(), reached.end());
      }
    }
    layers_.Set(tab.at, tab.profile, edits);
  }
  // 🔴 UNE COULEUR POUR TOUS LES CHEVEUX : les pièces se lient, et prennent la
  // couleur de la première. Chacune la reçoit sur son propre dégradé, qui garde
  // son modelé.
  if (asked.one_hair_color && count > 1 && !layer_painted_.empty()) {
    std::vector<std::size_t> all(count);
    for (std::size_t rank = 0; rank < count; ++rank) {
      all[rank] = rank;
      if (rank > 0) {
        marks.Link(0, rank);
      }
    }
    layers_.Set(tab.at, tab.profile, Reach(tab, all, layer_painted_.front()));
  }
  if (asked.adapt_cut) {
    AdaptCut(tab);
  }
  if (asked.skin_tone >= 0 && static_cast<std::size_t>(asked.skin_tone) < kSkinTones.size()) {
    const std::array<std::size_t, 1> skin{0};
    layers_.Set(tab.at, tab.profile,
                Reach(tab, skin, kSkinTones[static_cast<std::size_t>(asked.skin_tone)]));
  }
}

void StyleView::AdaptCut(const LayerTab& tab) {
  const std::optional<ro::render::LayerDye> previous = layers_.PreviousHead(tab.at.key);
  if (!previous.has_value() || !tabs_look_.has_value() || tab.ramps.empty()) {
    return;
  }
  // L'ANCIENNE TÊTE, mesurée et peinte comme elle l'était.
  ro::render::ActorLook old_look = *tabs_look_;
  old_look.hair = static_cast<std::uint16_t>(previous->key >> kHairStyleShift);
  old_look.hair_palette = static_cast<std::int16_t>(previous->key & kHairPaletteMask);
  const ro::render::ActorWardrobe::BodyRamps old =
      wardrobe_->LayerRampsFor(old_look, ActorPart::kHead);
  if (old.ramps.empty()) {
    return;
  }
  const ro::formats::Palette painted =
      ro::render::ApplyRecipe(old.base, old.ramps, ro::render::LayerRecipe(*previous));
  // 🔴 LES PIÈCES S'APPARIENT PAR TEINTE, et non par rang : les rangs d'une
  // coupe ne désignent pas les mêmes mèches sur une autre. Une pièce grise
  // reprend une pièce grise ; à défaut, la pièce de même rang.
  std::vector<LayerEdit> edits;
  for (std::size_t rank = 0; rank < tab.ramps.size(); ++rank) {
    const std::optional<int> hue = tab.ramps[rank].hue;
    std::size_t best = std::min(rank, old.ramps.size() - 1);
    int best_gap = kTurn;
    for (std::size_t other = 0; other < old.ramps.size(); ++other) {
      const std::optional<int> other_hue = old.ramps[other].hue;
      if (hue.has_value() != other_hue.has_value()) {
        continue;
      }
      const int gap = hue.has_value() ? HueGap(*hue, *other_hue) : 0;
      if (gap < best_gap) {
        best_gap = gap;
        best = other;
      }
    }
    const std::array<std::uint8_t, 3> target = ro::render::RampColor(painted, old.ramps[best]);
    edits.push_back(LayerEdit{
        .rank = rank, .adjust = ro::render::AdjustToReach(tab.base, tab.ramps[rank], target)});
  }
  layers_.Set(tab.at, tab.profile, edits);
}

// ── CE QUI PART ─────────────────────────────────────────────────────────────

std::vector<ro::net::StyleLayerOrder> StyleView::LayerOrders(bool atelier_open) const {
  std::vector<ro::net::StyleLayerOrder> orders;
  for (const ro::render::LayerDye& dye : layers_.Pending()) {
    // ⚠ SEULEMENT CE QU'ON PORTE : la retouche d'une coupe qu'on n'a plus, ou
    // d'un chapeau ôté, attend qu'on le remette. Le serveur ne la rendrait
    // pas, et elle resterait « à valider » pour toujours.
    const LayerTab* tab = nullptr;
    for (const LayerTab& candidate : tabs_) {
      if (candidate.at == LayerAt{.layer = dye.layer, .key = dye.key}) {
        tab = &candidate;
      }
    }
    if (tab == nullptr || tab->shown.blocked != PaletteTabBlock::kNone) {
      continue;
    }
    ro::net::StyleLayerOrder order{.recipe = ToWire(dye), .clear = NeutralDye(dye)};
    if (tab->shown.item) {
      // 🔴 UN OBJET SE DÉSIGNE PAR SA PLACE ET L'OBJET ATTENDU, jamais par son
      // apparence : c'est le serveur qui trouve l'exemplaire. Et seulement à
      // l'atelier du styliste.
      if (!atelier_open || !tab->slot.has_value() || tab->item_id == 0) {
        continue;
      }
      order.slot = tab->slot;
      order.recipe.key = tab->item_id;
    }
    orders.push_back(order);
  }
  return orders;
}

void StyleView::ValidateLayers(ro::net::Connection& link, std::uint32_t now_ms) {
  const std::vector<ro::net::StyleLayerOrder> orders = LayerOrders(atelier_open_);
  if (!orders.empty()) {
    static_cast<void>(SendLayers(link, orders, /*clear_character=*/false, now_ms));
  }
}

void StyleView::ClearItemDye(const LayerTab& tab, ro::net::Connection& link, std::uint32_t now_ms) {
  const ro::render::LayerDye* worn = layers_.Worn(tab.at);
  if (!atelier_open_ || worn == nullptr || !tab.slot.has_value() || tab.item_id == 0) {
    return;
  }
  ro::net::StyleLayerOrder order{.recipe = ToWire(*worn), .slot = tab.slot, .clear = true};
  order.recipe.key = tab.item_id;
  const std::array<ro::net::StyleLayerOrder, 1> one{order};
  if (SendLayers(link, one, /*clear_character=*/false, now_ms)) {
    layers_.BackToWorn(tab.at);
  }
}

bool StyleView::TryLayerCode(std::string_view code) {
  const std::optional<ro::sim::StyleLayerCode> read = ro::sim::DecodeStyleLayerCode(code);
  const LayerTab* tab = OpenLayer();
  if (!read.has_value() || tab == nullptr || tab->shown.blocked != PaletteTabBlock::kNone) {
    return false;
  }
  // ⚠ LE MÊME CALQUE ET LE MÊME PROFIL : les rangs d'un autre profil
  // désignent d'autres pièces.
  const ro::sim::StyleLayerRecipe told{
      .layer = read->layer, .key = tab->at.key, .profile = read->profile, .adjusts = read->adjusts};
  const std::optional<ro::render::LayerDye> dye = FromWire(told);
  if (!dye.has_value() || dye->layer != tab->at.layer || dye->profile != tab->profile) {
    return false;
  }
  layers_.Put(*dye);
  return true;
}

const char* StyleView::LayersRefusal() const { return RefusalText(session_.layers_refusal()); }

}  // namespace moonclient
