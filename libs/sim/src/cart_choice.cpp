#include "ro/sim/cart_choice.h"

#include <array>

namespace ro::sim {
namespace {

// Les paliers des types un à neuf : le type N s'offre au-delà du N-ième.
constexpr std::array<std::int64_t, 9> kChangeCartThresholds = {0,   40,  65,  80, 90,
                                                               100, 110, 120, 130};
// Le type qui s'ajoute à part, et le niveau qu'il faut dépasser.
constexpr std::uint8_t kLateCartType = 13;
constexpr std::int64_t kLateCartThreshold = 100;

}  // namespace

std::vector<std::uint8_t> ChangeCartTypesFor(std::int64_t base_level) {
  std::vector<std::uint8_t> types;
  for (std::size_t rank = 0; rank < kChangeCartThresholds.size(); ++rank) {
    if (base_level <= kChangeCartThresholds[rank]) {
      break;
    }
    types.push_back(static_cast<std::uint8_t>(rank + 1));
  }
  if (base_level > kLateCartThreshold) {
    types.push_back(kLateCartType);
  }
  return types;
}

ChangeCartRefusal ChangeCartRefusalFor(std::int64_t sp, std::int32_t cart_type) noexcept {
  if (sp < kChangeCartLeastSp) {
    return ChangeCartRefusal::kNotEnoughSp;
  }
  if (cart_type <= 0) {
    return ChangeCartRefusal::kNoCart;
  }
  return ChangeCartRefusal::kNone;
}

void CartChoice::OfferChange(std::int64_t base_level) {
  if (open_) {
    return;
  }
  offer_ = CartChoiceOffer{0, ChangeCartTypesFor(base_level)};
  open_ = true;
  ++serial_;
}

void CartChoice::OfferFromServer(std::uint32_t identity, std::span<const std::uint8_t> cart_types) {
  if (cart_types.empty()) {
    return;
  }
  offer_ =
      CartChoiceOffer{identity, std::vector<std::uint8_t>(cart_types.begin(), cart_types.end())};
  open_ = true;
  ++serial_;
}

void CartChoice::Close() noexcept {
  open_ = false;
  offer_ = CartChoiceOffer();
}

}  // namespace ro::sim
