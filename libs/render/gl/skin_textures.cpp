#include "ro/render/skin_textures.h"

namespace ro::render {

SkinTextures SkinTextures::Upload(const SkinPieces& pieces) {
  SkinTextures uploaded;
  for (std::size_t rank = 0; rank < kSkinPieceCount; ++rank) {
    const formats::Image* image = pieces.Get(static_cast<SkinPiece>(rank));
    if (image == nullptr) {
      continue;
    }
    // ⚠ AU PLUS PROCHE : voir GlTexture::Filtering::kPixelArt. Interpolée, une
    // barre de titre étirée rend une bouillie là où le client montre des lignes.
    auto texture = GlTexture::Upload(*image, GlTexture::Filtering::kPixelArt);
    if (!texture) {
      continue;
    }
    uploaded.pieces_[rank].texture = std::move(*texture);
    uploaded.pieces_[rank].width = image->width;
    uploaded.pieces_[rank].height = image->height;
  }
  return uploaded;
}

}  // namespace ro::render
