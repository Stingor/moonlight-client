#pragma once

#include <array>
#include <cstddef>

#include "ro/render/gl_texture.h"
#include "ro/render/skin_pieces.h"

namespace ro::render {

// LES PIÈCES DU SKIN, SUR LA CARTE GRAPHIQUE.
//
// Une pièce absente ou refusée laisse sa case vide : c'est la même tolérance
// qu'à la lecture, et pour la même raison — un cadre manquant ne doit pas
// empêcher de jouer.
//
// ⚠ CETTE CLASSE VIT DANS LA COUCHE GL, et c'est pourquoi `ui` ne la voit pas.
// L'interface reçoit des identifiants et des tailles, pas des textures : voir
// `ro/ui/skin_art.h`. C'est la même frontière que la `FrameScene` du monde.
class SkinTextures {
 public:
  // Ce qu'une pièce téléversée occupe. Les dimensions sont celles du bitmap :
  // c'est à elles que se mesurent les coins d'un découpage.
  struct Piece {
    GlTexture texture;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool valid() const noexcept { return texture.valid(); }
  };

  [[nodiscard]] static SkinTextures Upload(const SkinPieces& pieces);

  [[nodiscard]] const Piece& Get(SkinPiece piece) const {
    return pieces_[static_cast<std::size_t>(piece)];
  }

 private:
  std::array<Piece, kSkinPieceCount> pieces_;
};

}  // namespace ro::render
