// LES ENSEIGNES DES SALONS ET DES ÉCHOPPES : leurs surfaces, leur titre, leur
// place.
//
// 🔴 CE BANC GARDE L'ORDRE DES PIÈCES, qui est ce qu'il y a de plus fragile :
// une pièce posée efface ce qu'elle recouvre là où elle est transparente, et
// c'est ainsi que la queue ouvre le bas du cadre. Composer avec un mélange
// alpha donnerait une surface plausible et fausse, sans qu'aucune taille ne
// change.

#include "ro/render/signboard.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/font_atlas.h"
#include "ro/render/screen_text.h"
#include "ro/vfs/encoding.h"

#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using ro::formats::Image;
using ro::render::ChatRoomFrame;
using ro::render::ComposeChatRoomSurface;
using ro::render::ComposeShopSurface;
using ro::test_support::BmpPixel;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;

namespace {

using Pixel = std::array<std::uint8_t, 4>;

constexpr Pixel kTransparent = {255, 0, 255, 0};

// Une pièce d'une seule couleur opaque.
[[nodiscard]] Image Solid(int width, int height, Pixel color) {
  Image image;
  image.width = width;
  image.height = height;
  for (int index = 0; index < width * height; ++index) {
    image.pixels.insert(image.pixels.end(), color.begin(), color.end());
  }
  return image;
}

void Put(Image& image, int x, int y, Pixel color) {
  const auto at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                   static_cast<std::size_t>(x)) *
                  4;
  for (std::size_t channel = 0; channel < 4; ++channel) {
    image.pixels[at + channel] = color[channel];
  }
}

[[nodiscard]] Pixel At(const Image& image, int x, int y) {
  const auto at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                   static_cast<std::size_t>(x)) *
                  4;
  return {image.pixels[at], image.pixels[at + 1], image.pixels[at + 2], image.pixels[at + 3]};
}

[[nodiscard]] Pixel Opaque(std::array<std::uint8_t, 3> color) {
  return {color[0], color[1], color[2], 255};
}

// Les pièces de l'installation font quatorze pixels de côté ; chacune a ici sa
// couleur, pour qu'on sache laquelle a gagné un pixel.
constexpr int kPiecePixels = 14;
constexpr Pixel kTopLeftColor = {10, 0, 0, 255};
constexpr Pixel kTopRightColor = {20, 0, 0, 255};
constexpr Pixel kTopColor = {30, 0, 0, 255};
constexpr Pixel kBottomLeftColor = {40, 0, 0, 255};
constexpr Pixel kBottomRightColor = {50, 0, 0, 255};
constexpr Pixel kBottomColor = {60, 0, 0, 255};
constexpr Pixel kTailColor = {70, 0, 0, 255};
constexpr Pixel kIconColor = {80, 0, 0, 255};

[[nodiscard]] ChatRoomFrame ColoredFrame() {
  return ChatRoomFrame{
      .top_left = Solid(kPiecePixels, kPiecePixels, kTopLeftColor),
      .top_right = Solid(kPiecePixels, kPiecePixels, kTopRightColor),
      .top = Solid(kPiecePixels, kPiecePixels, kTopColor),
      .bottom_left = Solid(kPiecePixels, kPiecePixels, kBottomLeftColor),
      .bottom_right = Solid(kPiecePixels, kPiecePixels, kBottomRightColor),
      .bottom = Solid(kPiecePixels, kPiecePixels, kBottomColor),
      .tail = Solid(kPiecePixels, kPiecePixels, kTailColor),
  };
}

constexpr int kIconPixels = 24;

// L'écart toléré sur une coordonnée de texture recalculée.
constexpr float kUvTolerance = 1e-5F;

std::optional<ro::render::FontAtlas> SystemFont() {
  constexpr int kPixelHeight = 12;
  const std::array<const char*, 3> candidates = {
      "C:\\Windows\\Fonts\\arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"};
  for (const char* path : candidates) {
    if (!std::filesystem::exists(path)) {
      continue;
    }
    if (auto atlas = ro::render::FontAtlas::Load(path, kPixelHeight); atlas) {
      return std::move(*atlas);
    }
  }
  return std::nullopt;
}

// Le chemin tel que L'ARCHIVE le range, en CP949.
[[nodiscard]] std::string ArchivePathOf(std::string_view utf8) {
  auto cp949 = ro::vfs::Utf8ToCp949(utf8);
  REQUIRE(cp949.has_value());
  return *cp949;
}

}  // namespace

TEST_CASE("Enseigne de salon : les coins, puis le bord qui mord sur le coin droit",
          "[render][signboard]") {
  const Image surface = ComposeChatRoomSurface(ColoredFrame(), Image{});
  REQUIRE(surface.width == ro::render::kSignboardWidthPixels);
  REQUIRE(surface.height == ro::render::kSignboardHeightPixels);

  CHECK(At(surface, 0, 0) == kTopLeftColor);
  // Le bord haut se répète de 12 à 120 par pas de douze ; sa dernière copie
  // couvre 120 à 133, et donc le début du coin droit posé en 126.
  CHECK(At(surface, 12, 0) == kTopColor);
  CHECK(At(surface, 130, 0) == kTopColor);
  CHECK(At(surface, 135, 0) == kTopRightColor);
  // La rangée basse s'arrête six pixels au-dessus du bord : elle commence en
  // 34 − 14 − 6.
  CHECK(At(surface, 0, 14) == kBottomLeftColor);
  CHECK(At(surface, 30, 14) == kBottomColor);
  CHECK(At(surface, 139, 14) == kBottomRightColor);
  // La queue, centrée en bas : de 63 à 76, de 20 au bord.
  CHECK(At(surface, 63, 33) == kTailColor);
  CHECK(At(surface, 70, 20) == kTailColor);
  CHECK(At(surface, 62, 33) == kTransparent);
  CHECK(At(surface, 30, 28) == kTransparent);
}

TEST_CASE("Enseigne de salon : le transparent d'une pièce EFFACE ce qu'elle couvre",
          "[render][signboard]") {
  ChatRoomFrame frame = ColoredFrame();
  // L'échancrure de la queue : son coin haut-gauche est transparent, et la
  // rangée basse y était peinte.
  Put(frame.tail, 0, 0, kTransparent);
  // Une icône à demi transparente ne gomme RIEN, elle.
  Image icon = Solid(kIconPixels, kIconPixels, kIconColor);
  Put(icon, 0, 0, kTransparent);

  const Image surface = ComposeChatRoomSurface(frame, icon);
  CHECK(At(surface, 63, 20) == kTransparent);
  CHECK(At(surface, 64, 20) == kTailColor);
  CHECK(At(surface, 3, 3) == kTopLeftColor);
  CHECK(At(surface, 4, 3) == kIconColor);
}

TEST_CASE("Enseigne de salon : sans pièces, une surface vide à la bonne taille",
          "[render][signboard]") {
  const Image surface = ComposeChatRoomSurface(ChatRoomFrame{}, Image{});
  REQUIRE(surface.width == ro::render::kSignboardWidthPixels);
  REQUIRE(surface.height == ro::render::kSignboardHeightPixels);
  CHECK(At(surface, 70, 10) == kTransparent);
}

TEST_CASE("Enseigne d'échoppe : aplat arrondi, filet gris, six pixels vides en bas",
          "[render][signboard]") {
  const Pixel fill = Opaque(ro::render::kShopSignFill);
  const Pixel edge = Opaque(ro::render::kShopSignEdge);
  const Image surface = ComposeShopSurface(Image{}, ro::render::kShopSignFill);

  // Le coin coupé en trois marches : rien en (0,0), (1,0), (2,0), (0,1), (0,2).
  CHECK(At(surface, 0, 0) == kTransparent);
  CHECK(At(surface, 2, 0) == kTransparent);
  CHECK(At(surface, 0, 2) == kTransparent);
  CHECK(At(surface, 3, 0) == fill);
  CHECK(At(surface, 1, 1) == fill);
  CHECK(At(surface, 0, 3) == fill);
  // Le filet, à deux pixels du bord, arrondi lui aussi.
  CHECK(At(surface, 5, 2) == edge);
  CHECK(At(surface, 2, 5) == edge);
  CHECK(At(surface, 2, 2) == fill);
  CHECK(At(surface, 70, 25) == edge);
  CHECK(At(surface, 70, 15) == fill);
  // L'aplat s'arrête à vingt-huit.
  CHECK(At(surface, 70, 27) == fill);
  CHECK(At(surface, 70, 28) == kTransparent);
}

TEST_CASE("Enseigne d'échoppe : celle que la recherche désigne passe au jaune",
          "[render][signboard]") {
  const Image surface = ComposeShopSurface(Image{}, ro::render::kSpotlitShopSignFill);
  CHECK(At(surface, 70, 15) == Opaque(ro::render::kSpotlitShopSignFill));
  CHECK(At(surface, 5, 2) == Opaque(ro::render::kShopSignEdge));
}

TEST_CASE("Enseigne : un titre trop long se coupe en « ... », jamais au milieu d'un caractère",
          "[render][signboard]") {
  const std::optional<ro::render::FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  const auto widest = static_cast<float>(ro::render::kSignboardTitleWidthPixels);

  CHECK(ro::render::FitSignboardTitle(*font, "Potions") == "Potions");

  const std::string long_title = "Vends potions blanches et jaunes pas cher du tout";
  const std::string fitted = ro::render::FitSignboardTitle(*font, long_title);
  REQUIRE(fitted.ends_with(ro::render::kSignboardEllipsis));
  CHECK(ro::render::MeasureText(*font, fitted) <= widest);
  const std::string kept = fitted.substr(0, fitted.size() - ro::render::kSignboardEllipsis.size());
  CHECK(long_title.starts_with(kept));
  // LE PLUS LONG QUI TIENNE : un caractère de plus déborderait.
  const std::string one_more =
      long_title.substr(0, kept.size() + 1) + std::string(ro::render::kSignboardEllipsis);
  CHECK(ro::render::MeasureText(*font, one_more) > widest);

  // Des « é » de deux octets : la coupe tombe entre deux caractères.
  std::string accents;
  for (int count = 0; count < 40; ++count) {
    accents += "é";
  }
  const std::string cut = ro::render::FitSignboardTitle(*font, accents);
  const std::string cut_kept = cut.substr(0, cut.size() - ro::render::kSignboardEllipsis.size());
  CHECK(cut_kept.size() % 2 == 0);
  CHECK(accents.starts_with(cut_kept));
}

TEST_CASE("Enseigne : la fenêtre se centre sur la tête, sur la grille des pixels",
          "[render][signboard]") {
  const glm::vec2 room =
      ro::render::SignboardTopLeft(glm::vec2{100.7F, 200.4F}, ro::render::kChatRoomSignLiftPixels);
  CHECK(room.x == 30.0F);
  CHECK(room.y == 183.0F);
  const glm::vec2 shop =
      ro::render::SignboardTopLeft(glm::vec2{100.7F, 200.4F}, ro::render::kShopSignLiftPixels);
  CHECK(shop.y == 166.0F);

  const ro::render::ScreenRect whole = ro::render::SignboardRect(room);
  CHECK(whole.max.x - whole.min.x == 140.0F);
  CHECK(whole.max.y - whole.min.y == 34.0F);
  const ro::render::ScreenRect title = ro::render::SignboardTitleRect(room);
  CHECK(title.min.x == 61.0F);
  CHECK(title.min.y == 191.0F);
  CHECK(title.max.x - title.min.x == 106.0F);
}

TEST_CASE("Enseigne : la surface d'abord, à sa taille, puis le titre en noir",
          "[render][signboard]") {
  const std::optional<ro::render::FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  const Image surface = ComposeShopSurface(Image{}, ro::render::kShopSignFill);
  const glm::vec2 top_left{30.0F, 40.0F};
  const std::vector<ro::render::ScreenQuad> quads =
      ro::render::ComposeSignboard(*font, surface, "Potions", top_left);
  REQUIRE(quads.size() > 1);
  CHECK(quads.front().atlas == &surface);
  CHECK(quads.front().min == top_left);
  CHECK(quads.front().max == top_left + glm::vec2{140.0F, 34.0F});
  for (std::size_t rank = 1; rank < quads.size(); ++rank) {
    CHECK(quads[rank].tint == std::array<std::uint8_t, 4>{0, 0, 0, 255});
    // Un glyphe peut déborder d'un pixel à gauche de son origine, pas davantage.
    CHECK(quads[rank].min.x >= top_left.x + 30.0F);
  }

  // Sans surface, le titre seul.
  const std::vector<ro::render::ScreenQuad> bare =
      ro::render::ComposeSignboard(*font, Image{}, "Potions", top_left);
  CHECK(bare.size() == quads.size() - 1);
}

TEST_CASE("Enseigne : la zone du titre rogne ce qui en sort", "[render][signboard]") {
  const std::optional<ro::render::FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  // Un texte qu'on n'a PAS coupé : la zone doit le rogner seule.
  const std::string overflowing = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
  const glm::vec2 top_left{30.0F, 40.0F};
  const ro::render::ScreenRect zone = ro::render::SignboardTitleRect(top_left, font->line_height());
  const std::vector<ro::render::ScreenQuad> quads =
      ro::render::ComposeSignboard(*font, Image{}, overflowing, top_left);
  REQUIRE_FALSE(quads.empty());
  bool clipped = false;
  for (const ro::render::ScreenQuad& glyph : quads) {
    CHECK(glyph.min.x >= zone.min.x);
    CHECK(glyph.max.x <= zone.max.x);
    CHECK(glyph.max.y <= zone.max.y);
    if (glyph.max.x == zone.max.x) {
      clipped = true;
    }
  }
  CHECK(clipped);

  // Le glyphe tranché perd la même part de sa texture que de sa largeur : sa
  // portion, rapportée à sa largeur, garde la densité du glyphe entier.
  const std::vector<ro::render::ScreenQuad> unclipped = ro::render::ComposeText(
      *font, overflowing, glm::vec2{zone.min.x, zone.min.y + font->ascent()},
      std::array<std::uint8_t, 4>{0, 0, 0, 255});
  for (const ro::render::ScreenQuad& glyph : quads) {
    for (const ro::render::ScreenQuad& entire : unclipped) {
      if (entire.min != glyph.min || entire.max.x == glyph.max.x) {
        continue;
      }
      const float density_entire =
          (entire.uv_max.x - entire.uv_min.x) / (entire.max.x - entire.min.x);
      const float density_kept = (glyph.uv_max.x - glyph.uv_min.x) / (glyph.max.x - glyph.min.x);
      CHECK(std::abs(density_kept - density_entire) < kUvTolerance);
    }
  }
}

TEST_CASE("Enseigne : les pièces se lisent dans l'installation", "[render][signboard]") {
  constexpr BmpPixel kWhite{.red = 255, .green = 255, .blue = 255};
  constexpr BmpPixel kMagenta{.red = 255, .green = 0, .blue = 255};
  const std::vector<std::uint8_t> piece = ro::test_support::BuildBmp(2, 1, {kWhite, kMagenta});

  std::vector<GrfFileSpec> files;
  for (const std::string_view path :
       {ro::render::kChatRoomFrameTopLeftPath, ro::render::kChatRoomFrameTopRightPath,
        ro::render::kChatRoomFrameTopPath, ro::render::kChatRoomFrameBottomLeftPath,
        ro::render::kChatRoomFrameBottomRightPath, ro::render::kChatRoomFrameBottomPath,
        ro::render::kChatRoomFrameTailPath, ro::render::kOpenRoomIconPath,
        ro::render::kLockedRoomIconPath, ro::render::kVendingIconPath,
        ro::render::kBuyingIconPath}) {
    files.push_back(GrfFileSpec{ArchivePathOf(path), piece});
  }
  const TempDirectory client;
  client.WriteFile("data.grf", ro::test_support::BuildGrf(files));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"data.grf"}});
  REQUIRE(vfs.has_value());

  const ro::render::SignboardSurfaces surfaces = ro::render::LoadSignboardSurfaces(*vfs);
  // Le coin haut-gauche du cadre a été posé : son pixel blanc est en (0, 0),
  // son magenta — devenu transparent — en (1, 0).
  CHECK(At(surfaces.open_room, 0, 0) == Pixel{255, 255, 255, 255});
  CHECK(At(surfaces.open_room, 1, 0)[3] == 0);
  // L'icône, posée en (3, 3) par-dessus l'échoppe. Son magenta, en (4, 3),
  // laisse voir la marche du coin du filet gris qui était dessous.
  CHECK(At(surfaces.vending, 3, 3) == Pixel{255, 255, 255, 255});
  CHECK(At(surfaces.vending, 4, 3) == Opaque(ro::render::kShopSignEdge));
  CHECK(At(surfaces.spotlit_buying, 70, 15) == Opaque(ro::render::kSpotlitShopSignFill));
}

TEST_CASE("Enseigne : sans les pièces, les échoppes gardent leur aplat", "[render][signboard]") {
  const TempDirectory client;
  client.WriteFile("data.grf", ro::test_support::BuildGrf({}));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"data.grf"}});
  REQUIRE(vfs.has_value());

  const ro::render::SignboardSurfaces surfaces = ro::render::LoadSignboardSurfaces(*vfs);
  CHECK(surfaces.open_room.width == ro::render::kSignboardWidthPixels);
  CHECK(At(surfaces.open_room, 0, 0) == kTransparent);
  CHECK(At(surfaces.vending, 70, 15) == Opaque(ro::render::kShopSignFill));
}

TEST_CASE("Enseigne : les couleurs d'une échoppe se repeignent dans ses quatre surfaces",
          "[render][signboard]") {
  ro::render::SignboardSurfaces surfaces;
  ro::render::RecolorShopSurfaces(surfaces, ro::render::ShopSignColors{});
  CHECK(At(surfaces.vending, 70, 15) == Opaque(ro::render::kShopSignFill));
  CHECK(surfaces.shop_colors == ro::render::ShopSignColors{});

  const ro::render::ShopSignColors colors{
      .fill = {10, 20, 30}, .spotlit_fill = {40, 50, 60}, .edge = {70, 80, 90}};
  ro::render::RecolorShopSurfaces(surfaces, colors);
  CHECK(At(surfaces.vending, 70, 15) == Opaque(colors.fill));
  CHECK(At(surfaces.buying, 70, 15) == Opaque(colors.fill));
  CHECK(At(surfaces.spotlit_vending, 70, 15) == Opaque(colors.spotlit_fill));
  CHECK(At(surfaces.spotlit_buying, 70, 15) == Opaque(colors.spotlit_fill));
  CHECK(At(surfaces.vending, 5, 2) == Opaque(colors.edge));
  CHECK(surfaces.shop_colors == colors);
}

TEST_CASE("Enseigne : la zone du titre grandit avec la police, jusqu'au bas du corps",
          "[render][signboard]") {
  const glm::vec2 top_left{30.0F, 40.0F};
  const auto height = [&top_left](float line_height) {
    const ro::render::ScreenRect zone = ro::render::SignboardTitleRect(top_left, line_height);
    return zone.max.y - zone.min.y;
  };
  // La police du client tient dans les quinze pixels d'origine.
  CHECK(height(0.0F) == 15.0F);
  CHECK(height(13.8F) == 15.0F);
  CHECK(height(17.2F) == 18.0F);
  // Pas plus bas que le corps : 34 de haut, 6 pour la queue, le titre à 8.
  CHECK(height(40.0F) == 20.0F);
}

TEST_CASE("Enseigne : la teinte et l'opacité réglées passent aux quads", "[render][signboard]") {
  const std::optional<ro::render::FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  const Image surface = ComposeShopSurface(Image{}, ro::render::kShopSignFill);
  const ro::render::SignboardPaint paint{.surface_tint = {200, 100, 50, 128},
                                         .title_color = {255, 0, 0, 64}};
  const std::vector<ro::render::ScreenQuad> quads =
      ro::render::ComposeSignboard(*font, surface, "Potions", glm::vec2{30.0F, 40.0F}, paint);
  REQUIRE(quads.size() > 1);
  CHECK(quads.front().tint == paint.surface_tint);
  for (std::size_t rank = 1; rank < quads.size(); ++rank) {
    CHECK(quads[rank].tint == paint.title_color);
  }
}
