#include "ro/render/accessory_look.h"

#include "ro/render/companion_cart.h"
#include "ro/render/companion_falcon.h"

namespace ro::render {

std::optional<std::string_view> AccessorySpritePath(AccessoryKind kind,
                                                    std::uint32_t dye_key) noexcept {
  switch (kind) {
    case AccessoryKind::kFalcon:
      if (const std::optional<FalconArt> art = FalconArtOfDyeKey(dye_key)) {
        return art->spr_path;
      }
      break;
    case AccessoryKind::kCart:
      if (const std::optional<CartArt> art = CartArtOfDyeKey(dye_key)) {
        return art->spr_path;
      }
      break;
    case AccessoryKind::kNone:
      break;
  }
  return std::nullopt;
}

}  // namespace ro::render
