#include "character/style_layers.h"

#include <algorithm>

#include "character/style_recipe.h"

namespace moonclient {
namespace {

// Les têtes teintes qu'on retient pour « adapter à ma nouvelle coupe » : de quoi
// revenir sur quelques passages chez le styliste, sans garder toute la partie.
constexpr std::size_t kHeadsKept = 8;

[[nodiscard]] LayerAt AtOf(const ro::render::LayerDye& dye) {
  return LayerAt{.layer = dye.layer, .key = dye.key};
}

[[nodiscard]] ro::render::LayerDye NeutralAt(LayerAt at, std::uint8_t profile) {
  ro::render::LayerDye dye;
  dye.layer = at.layer;
  dye.key = at.key;
  dye.profile = profile;
  return dye;
}

}  // namespace

bool NeutralDye(const ro::render::LayerDye& dye) {
  const auto neutral = [](const ro::render::RampAdjust& adjust) { return adjust.neutral(); };
  return std::ranges::all_of(dye.adjusts, neutral) && std::ranges::all_of(dye.rest, neutral) &&
         dye.retouches.empty();
}

void StyleLayers::Observe(std::span<const ro::render::LayerDye> worn, std::uint32_t revision) {
  if (revision == worn_revision_) {
    return;
  }
  worn_revision_ = revision;
  worn_.assign(worn.begin(), worn.end());
  for (const ro::render::LayerDye& dye : worn_) {
    if (dye.layer == ro::render::StyleLayer::kHead && !NeutralDye(dye)) {
      RememberHead(dye);
    }
  }
  Settle();
  Changed();
}

const ro::render::LayerDye* StyleLayers::Worn(LayerAt at) const {
  for (const ro::render::LayerDye& dye : worn_) {
    if (AtOf(dye) == at) {
      return &dye;
    }
  }
  return nullptr;
}

ro::render::LayerDye StyleLayers::Current(LayerAt at, std::uint8_t profile) const {
  const ro::render::LayerDye* found = nullptr;
  if (const auto edit = edits_.find(at); edit != edits_.end()) {
    found = &edit->second;
  } else {
    found = Worn(at);
  }
  if (found == nullptr || found->profile != profile) {
    return NeutralAt(at, profile);
  }
  return *found;
}

void StyleLayers::Set(LayerAt at, std::uint8_t profile, std::span<const LayerEdit> edits,
                      std::uint32_t stroke) {
  ro::render::LayerDye dye = Current(at, profile);
  for (const LayerEdit& edit : edits) {
    if (edit.rank < dye.adjusts.size()) {
      dye.adjusts[edit.rank] = edit.adjust;
    } else if (edit.rank >= kRestRankFirst && edit.rank - kRestRankFirst < dye.rest.size()) {
      dye.rest[edit.rank - kRestRankFirst] = edit.adjust;
    }
  }
  Replace(at, std::move(dye), stroke);
}

bool StyleLayers::Retouch(LayerAt at, std::uint8_t profile,
                          const ro::render::PaletteRetouch& retouch, std::uint32_t stroke) {
  if (!ro::render::RetouchAllowed(retouch)) {
    return false;
  }
  ro::render::LayerDye dye = Current(at, profile);
  // 🔴 TRIÉES PAR INDEX, UNE PAR INDEX : c'est la forme que le fil exige.
  const auto place = std::ranges::lower_bound(dye.retouches, retouch.index, {},
                                              &ro::render::PaletteRetouch::index);
  if (place != dye.retouches.end() && place->index == retouch.index) {
    *place = retouch;
  } else if (dye.retouches.size() >= kLayerRetouchesMost) {
    return false;
  } else {
    dye.retouches.insert(place, retouch);
  }
  Replace(at, std::move(dye), stroke);
  return true;
}

void StyleLayers::Unretouch(LayerAt at, std::uint8_t profile, std::uint8_t index) {
  ro::render::LayerDye dye = Current(at, profile);
  if (std::erase_if(dye.retouches, [index](const ro::render::PaletteRetouch& retouch) {
        return retouch.index == index;
      }) == 0) {
    return;
  }
  Replace(at, std::move(dye), 0);
}

void StyleLayers::Replace(LayerAt at, ro::render::LayerDye dye, std::uint32_t stroke) {
  const Edits before = edits_;
  const bool head = at.layer == ro::render::StyleLayer::kHead && !NeutralDye(dye);
  if (head) {
    RememberHead(dye);
  }
  edits_[at] = std::move(dye);
  Settle();
  Step(before, stroke);
}

void StyleLayers::Put(const ro::render::LayerDye& dye) {
  const Edits before = edits_;
  edits_[AtOf(dye)] = dye;
  if (dye.layer == ro::render::StyleLayer::kHead && !NeutralDye(dye)) {
    RememberHead(dye);
  }
  Settle();
  Step(before, 0);
}

void StyleLayers::BackToWorn(LayerAt at) {
  const Edits before = edits_;
  edits_.erase(at);
  Step(before, 0);
}

void StyleLayers::BackToWornAll() {
  const Edits before = edits_;
  edits_.clear();
  Step(before, 0);
}

bool StyleLayers::Undo() {
  if (undo_.empty()) {
    return false;
  }
  redo_.push_back(std::move(edits_));
  edits_ = std::move(undo_.back());
  undo_.pop_back();
  stroke_ = 0;
  Settle();
  Changed();
  return true;
}

bool StyleLayers::Redo() {
  if (redo_.empty()) {
    return false;
  }
  undo_.push_back(std::move(edits_));
  edits_ = std::move(redo_.back());
  redo_.pop_back();
  stroke_ = 0;
  Settle();
  Changed();
  return true;
}

bool StyleLayers::HasEdits(LayerAt at) const { return edits_.contains(at); }

std::vector<ro::render::LayerDye> StyleLayers::Pending() const {
  std::vector<ro::render::LayerDye> pending;
  pending.reserve(edits_.size());
  for (const auto& [at, dye] : edits_) {
    pending.push_back(dye);
  }
  return pending;
}

std::vector<ro::render::LayerDye> StyleLayers::Composed() const {
  std::vector<ro::render::LayerDye> composed;
  for (const ro::render::LayerDye& dye : worn_) {
    if (!edits_.contains(AtOf(dye)) && !NeutralDye(dye)) {
      composed.push_back(dye);
    }
  }
  for (const auto& [at, dye] : edits_) {
    if (!NeutralDye(dye)) {
      composed.push_back(dye);
    }
  }
  std::ranges::sort(composed,
                    [](const ro::render::LayerDye& left, const ro::render::LayerDye& right) {
                      return AtOf(left) < AtOf(right);
                    });
  return composed;
}

std::optional<ro::render::LayerDye> StyleLayers::PreviousHead(std::uint32_t current) const {
  for (auto seen = heads_.rbegin(); seen != heads_.rend(); ++seen) {
    if (seen->key != current) {
      return *seen;
    }
  }
  return std::nullopt;
}

void StyleLayers::Step(const Edits& before, std::uint32_t stroke) {
  const bool same = before.size() == edits_.size() &&
                    std::ranges::equal(before, edits_, [](const auto& left, const auto& right) {
                      return left.first == right.first && SameDye(left.second, right.second);
                    });
  if (same) {
    return;
  }
  // ⚠ UN TRAIT QUI SE PROLONGE NE FAIT PAS DE PAS NEUF : l'état d'avant est
  // celui d'avant son premier réglage, déjà rangé.
  if (stroke == 0 || stroke != stroke_) {
    undo_.push_back(before);
    if (undo_.size() > kLayerHistoryDepth) {
      undo_.erase(undo_.begin());
    }
  }
  redo_.clear();
  stroke_ = stroke;
  Changed();
}

void StyleLayers::Changed() { ++revision_; }

void StyleLayers::Settle() {
  std::erase_if(edits_, [this](const auto& entry) {
    const ro::render::LayerDye* worn = Worn(entry.first);
    return worn != nullptr ? SameDye(*worn, entry.second) : NeutralDye(entry.second);
  });
}

void StyleLayers::RememberHead(const ro::render::LayerDye& dye) {
  std::erase_if(heads_, [&dye](const ro::render::LayerDye& seen) { return seen.key == dye.key; });
  heads_.push_back(dye);
  if (heads_.size() > kHeadsKept) {
    heads_.erase(heads_.begin());
  }
}

}  // namespace moonclient
