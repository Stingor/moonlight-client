#include "ro/render/font_atlas.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

// Ce qui est gravé : l'ASCII imprimable, puis le supplément latin-1 dont
// viennent les accents. L'espace insécable et le caractère de contrôle 173 en
// font partie sans rien dessiner, ce qui ne coûte qu'une case vide.
constexpr char32_t kFirstAscii = 32;
constexpr char32_t kLastAscii = 126;
constexpr char32_t kFirstLatin = 160;
constexpr char32_t kLastLatin = 255;

// ⚠ LA LIGATURE FRANÇAISE N'EST PAS DANS LATIN-1, et c'est le seul caractère
// de notre langue qui en sorte. Sans elle, « nœud » se grave « nud » — le
// glyphe manquant ne se dessine pas, et rien ne le signale. Le dépôt écrit ses
// messages en français ; cœur, sœur, œuvre et nœud les traverseront tous.
constexpr std::array<char32_t, 2> kFrenchLigatures = {0x0152, 0x0153};

// La largeur de l'image gravée. Assez pour tenir toute la table à une taille de
// nom, et une puissance de deux pour ne pas gêner les cartes anciennes.
constexpr int kAtlasWidth = 512;

// L'image gravée est en RVBA : quatre octets par pixel.
constexpr std::size_t kBytesPerPixel = 4;

// Une mesure de FreeType devient un INDICE de pixel.
//
// FreeType compte en `int`, le tampon d'image s'indexe en `std::size_t`, et un
// produit qui mêle les deux convertit en silence. La conversion est nommée une
// fois ici plutôt que répétée à chaque terme.
//
// Ce qu'elle suppose est écrit dessous : ces mesures sont des tailles et des
// positions dans l'image, jamais des écarts, donc jamais négatives. Une valeur
// négative deviendrait un indice énorme et sortirait du tampon — l'assertion la
// prend en construction de mise au point, et le sanitizer en intégration.
[[nodiscard]] std::size_t PixelIndex(int measure) {
  assert(measure >= 0);
  return static_cast<std::size_t>(measure);
}

// Un pixel de marge autour de chaque glyphe : sans elle, le filtrage ramène sur
// un bord la colonne du glyphe voisin.
constexpr int kGlyphPadding = 1;

// Les tailles de police que le format accepte. En dehors, la gravure produirait
// une image vide ou démesurée.
constexpr int kSmallestPixelHeight = 6;
constexpr int kLargestPixelHeight = 96;

// FreeType compte en 1/64 de pixel.
constexpr float kSubpixelsPerPixel = 64.0F;

struct Library {
  FT_Library handle = nullptr;
  ~Library() {
    if (handle != nullptr) {
      FT_Done_FreeType(handle);
    }
  }
};

struct Face {
  FT_Face handle = nullptr;
  ~Face() {
    if (handle != nullptr) {
      FT_Done_Face(handle);
    }
  }
};

}  // namespace

core::Result<FontAtlas> FontAtlas::Load(const std::filesystem::path& font_path, int pixel_height) {
  if (pixel_height < kSmallestPixelHeight || pixel_height > kLargestPixelHeight) {
    return Fail(ErrorCode::kInvalidArgument, "hauteur de police hors des bornes");
  }
  Library library;
  if (FT_Init_FreeType(&library.handle) != 0) {
    return Fail(ErrorCode::kIoFailure, "FreeType ne s'initialise pas");
  }
  Face face;
  if (FT_New_Face(library.handle, font_path.string().c_str(), 0, &face.handle) != 0) {
    return Fail(ErrorCode::kNotFound, "police illisible : " + font_path.string());
  }
  if (FT_Set_Pixel_Sizes(face.handle, 0, static_cast<FT_UInt>(pixel_height)) != 0) {
    return Fail(ErrorCode::kInvalidArgument, "taille de police refusée");
  }

  // Premier passage : mesurer, pour savoir quelle hauteur d'image réserver.
  // Graver dans une image qu'on agrandit ensuite obligerait à tout recopier.
  std::vector<char32_t> codes;
  for (char32_t code = kFirstAscii; code <= kLastAscii; ++code) {
    codes.push_back(code);
  }
  for (char32_t code = kFirstLatin; code <= kLastLatin; ++code) {
    codes.push_back(code);
  }
  for (const char32_t code : kFrenchLigatures) {
    codes.push_back(code);
  }

  FontAtlas atlas;
  atlas.line_height_ = static_cast<float>(face.handle->size->metrics.height) / kSubpixelsPerPixel;
  atlas.ascent_ = static_cast<float>(face.handle->size->metrics.ascender) / kSubpixelsPerPixel;

  int pen_x = kGlyphPadding;
  int pen_y = kGlyphPadding;
  int row_height = 0;
  struct Engraved {
    char32_t code;
    int x = 0;
    int y = 0;
    std::vector<std::uint8_t> coverage;
    int width = 0;
    int height = 0;
  };
  std::vector<Engraved> engraved;
  for (const char32_t code : codes) {
    if (FT_Load_Char(face.handle, code, FT_LOAD_RENDER) != 0) {
      continue;
    }
    const FT_GlyphSlot slot = face.handle->glyph;
    const int width = static_cast<int>(slot->bitmap.width);
    const int height = static_cast<int>(slot->bitmap.rows);
    if (pen_x + width + kGlyphPadding > kAtlasWidth) {
      pen_x = kGlyphPadding;
      pen_y += row_height + kGlyphPadding;
      row_height = 0;
    }
    Engraved one;
    one.code = code;
    one.x = pen_x;
    one.y = pen_y;
    one.width = width;
    one.height = height;
    one.coverage.assign(slot->bitmap.buffer, slot->bitmap.buffer + (width * height));
    engraved.push_back(std::move(one));

    Glyph glyph;
    glyph.min = glm::vec2{static_cast<float>(pen_x), static_cast<float>(pen_y)};
    glyph.max = glm::vec2{static_cast<float>(pen_x + width), static_cast<float>(pen_y + height)};
    glyph.bearing =
        glm::vec2{static_cast<float>(slot->bitmap_left), -static_cast<float>(slot->bitmap_top)};
    glyph.advance = static_cast<float>(slot->advance.x) / kSubpixelsPerPixel;
    atlas.glyphs_.emplace(code, glyph);

    pen_x += width + kGlyphPadding;
    row_height = std::max(row_height, height);
  }
  const int atlas_height = pen_y + row_height + kGlyphPadding;
  if (engraved.empty() || atlas_height <= 0) {
    return Fail(ErrorCode::kCorruptData, "police sans aucun glyphe");
  }

  // Second passage : le report dans l'image. Du blanc dont seule l'opacité
  // varie — la couleur viendra de la teinte du quad.
  atlas.image_.width = kAtlasWidth;
  atlas.image_.height = atlas_height;
  atlas.image_.pixels.assign(PixelIndex(kAtlasWidth) * PixelIndex(atlas_height) * kBytesPerPixel,
                             0);
  for (const Engraved& one : engraved) {
    for (int row = 0; row < one.height; ++row) {
      for (int column = 0; column < one.width; ++column) {
        const std::size_t source = PixelIndex(row) * PixelIndex(one.width) + PixelIndex(column);
        const std::size_t target =
            (PixelIndex(one.y + row) * PixelIndex(kAtlasWidth) + PixelIndex(one.x + column)) *
            kBytesPerPixel;
        atlas.image_.pixels[target + 0] = 255;
        atlas.image_.pixels[target + 1] = 255;
        atlas.image_.pixels[target + 2] = 255;
        atlas.image_.pixels[target + 3] = one.coverage[source];
      }
    }
  }
  return atlas;
}

const FontAtlas::Glyph* FontAtlas::GlyphFor(char32_t code) const {
  const auto found = glyphs_.find(code);
  return found == glyphs_.end() ? nullptr : &found->second;
}

}  // namespace ro::render
