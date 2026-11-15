#include "character/style_skin.h"

#include <algorithm>

#include "ro/render/style_set.h"

namespace moonclient {
namespace {

// Le dernier calque que le serveur doit accepter pour ranger la peau du corps.
constexpr int kBodySkinLayer = static_cast<int>(ro::render::StyleLayer::kBodySkin);

}  // namespace

LayerAt SkinLayerAt(SkinLine line, std::uint32_t body_key) noexcept {
  if (line == SkinLine::kBody) {
    return LayerAt{.layer = ro::render::StyleLayer::kBodySkin, .key = body_key};
  }
  return LayerAt{.layer = ro::render::StyleLayer::kSkin, .key = 0};
}

std::optional<SkinLine> SkinLineUnder(ro::render::ActorPart part, std::uint8_t index,
                                      const ro::render::SkinSet& body_skin) noexcept {
  switch (part) {
    case ro::render::ActorPart::kHead:
      if (std::ranges::contains(ro::render::SkinIndices(ro::render::SkinRace::kHuman), index)) {
        return SkinLine::kFace;
      }
      return std::nullopt;
    case ro::render::ActorPart::kBody:
      if (std::ranges::contains(body_skin.indices(), index)) {
        return SkinLine::kBody;
      }
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

SkinRefusal SkinRefusalFor(SkinLine line, bool gm_outfit, std::span<const std::string> body_paths,
                           int server_layer_last) {
  if (gm_outfit) {
    return SkinRefusal::kGmOutfit;
  }
  if (std::ranges::any_of(
          body_paths, [](const std::string& path) { return !ro::render::SkinAdjustable(path); })) {
    return SkinRefusal::kOnMount;
  }
  if (line == SkinLine::kBody && server_layer_last < kBodySkinLayer) {
    return SkinRefusal::kNoBodySkinServer;
  }
  return SkinRefusal::kNone;
}

ro::render::PaletteRamp SkinRamp(const ro::render::SkinSet& body_skin) {
  const std::span<const std::uint8_t> indices =
      body_skin.empty() ? ro::render::SkinIndices(ro::render::SkinRace::kHuman)
                        : body_skin.indices();
  if (indices.empty()) {
    return {};
  }
  return ro::render::PaletteRamp{.start = indices.front(),
                                 .length = static_cast<std::uint8_t>(indices.size()),
                                 .ref = indices[indices.size() / 2]};
}

}  // namespace moonclient
