#include "ro/render/signboard.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/screen_text.h"

namespace ro::render {
namespace {

constexpr std::size_t kChannels = 4;
constexpr std::size_t kAlpha = 3;
constexpr std::uint8_t kOpaque = 255;

// Le pixel d'une surface où rien n'est peint : le magenta du client, son alpha
// tombé — la forme que `ApplyMagentaColorKey` donne aux pixels qu'il efface.
constexpr std::array<std::uint8_t, kChannels> kBlankPixel = {255, 0, 255, 0};

// Le titre s'écrit en noir, sans ombre.
constexpr std::array<std::uint8_t, 4> kSignboardTitleColor = {0, 0, 0, 255};

// La marge des bords haut et bas d'un cadre de salon, de chaque côté : c'est
// elle que les coins occupent, et le compte des répétitions la retranche.
constexpr int kChatRoomFrameMarginPixels = kChatRoomFrameStepPixels;

[[nodiscard]] formats::Image BlankSurface() {
  formats::Image surface;
  surface.width = kSignboardWidthPixels;
  surface.height = kSignboardHeightPixels;
  surface.pixels.resize(static_cast<std::size_t>(surface.width) *
                        static_cast<std::size_t>(surface.height) * kChannels);
  for (std::size_t offset = 0; offset < surface.pixels.size(); offset += kChannels) {
    for (std::size_t channel = 0; channel < kChannels; ++channel) {
      surface.pixels[offset + channel] = kBlankPixel[channel];
    }
  }
  return surface;
}

// Le décalage du pixel (x, y) dans une image RGBA.
[[nodiscard]] std::size_t OffsetOf(const formats::Image& image, int x, int y) {
  return (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
          static_cast<std::size_t>(x)) *
         kChannels;
}

// Pose `piece` à (x, y). `keyed` laisse en place ce que ses pixels
// transparents recouvrent ; sinon ils l'EFFACENT. Ce qui sort de la surface
// est perdu.
void Blit(formats::Image& surface, const formats::Image& piece, int x, int y, bool keyed) {
  for (int row = 0; row < piece.height; ++row) {
    for (int column = 0; column < piece.width; ++column) {
      const int at_x = x + column;
      const int at_y = y + row;
      if (at_x < 0 || at_y < 0 || at_x >= surface.width || at_y >= surface.height) {
        continue;
      }
      const std::size_t from = OffsetOf(piece, column, row);
      if (keyed && piece.pixels[from + kAlpha] == 0) {
        continue;
      }
      const std::size_t to = OffsetOf(surface, at_x, at_y);
      for (std::size_t channel = 0; channel < kChannels; ++channel) {
        surface.pixels[to + channel] = piece.pixels[from + channel];
      }
    }
  }
}

// Un aplat opaque de (x0, y0) inclus à (x1, y1) exclu, borné à la surface.
void Fill(formats::Image& surface, int x0, int y0, int x1, int y1,
          const std::array<std::uint8_t, 3>& color) {
  for (int y = std::max(0, y0); y < std::min(surface.height, y1); ++y) {
    for (int x = std::max(0, x0); x < std::min(surface.width, x1); ++x) {
      const std::size_t to = OffsetOf(surface, x, y);
      surface.pixels[to] = color[0];
      surface.pixels[to + 1] = color[1];
      surface.pixels[to + 2] = color[2];
      surface.pixels[to + kAlpha] = kOpaque;
    }
  }
}

// LE FILET D'UN RECTANGLE AUX COINS ARRONDIS, d'un pixel. Chaque coin se
// coupe en trois marches : le bord s'arrête à trois pixels du coin, deux
// pixels le rejoignent en diagonale, puis un.
void RoundedOutline(formats::Image& surface, int x, int y, int width, int height,
                    const std::array<std::uint8_t, 3>& color) {
  const int right = x + width;
  const int bottom = y + height;
  Fill(surface, x + 3, y, right - 3, y + 1, color);
  Fill(surface, x + 3, bottom - 1, right - 3, bottom, color);
  Fill(surface, x, y + 3, x + 1, bottom - 3, color);
  Fill(surface, right - 1, y + 3, right, bottom - 3, color);
  Fill(surface, x + 1, y + 1, x + 3, y + 2, color);
  Fill(surface, x + 1, bottom - 2, x + 3, bottom - 1, color);
  Fill(surface, right - 3, y + 1, right - 1, y + 2, color);
  Fill(surface, right - 3, bottom - 2, right - 1, bottom - 1, color);
  Fill(surface, x + 1, y + 2, x + 2, y + 3, color);
  Fill(surface, x + 1, bottom - 3, x + 2, bottom - 2, color);
  Fill(surface, right - 2, y + 2, right - 1, y + 3, color);
  Fill(surface, right - 2, bottom - 3, right - 1, bottom - 2, color);
}

// Le même rectangle, PLEIN : l'intérieur à un pixel du bord, puis le filet.
void RoundedFill(formats::Image& surface, int x, int y, int width, int height,
                 const std::array<std::uint8_t, 3>& color) {
  Fill(surface, x + 1, y + 1, x + width - 1, y + height - 1, color);
  RoundedOutline(surface, x, y, width, height, color);
}

// Une pièce de l'installation, son magenta transparent.
//
// `Read` et non `ReadRaw` : ce chemin est une constante À NOUS, donc de
// l'UTF-8, et c'est le VFS qui le transcode vers le CP949 de l'archive.
[[nodiscard]] formats::Image LoadPiece(vfs::Vfs& vfs, std::string_view path) {
  auto bytes = vfs.Read(path);
  if (!bytes) {
    core::LogWarning("enseigne : {} absent — l'enseigne se peindra sans cette pièce", path);
    return {};
  }
  auto image = formats::DecodeImage(path, *bytes);
  if (!image) {
    core::LogWarning("enseigne : {} illisible ({}) — l'enseigne se peindra sans cette pièce", path,
                     core::Describe(image.error()));
    return {};
  }
  formats::ApplyMagentaColorKey(*image);
  return std::move(*image);
}

// Combien d'octets fait le dernier caractère UTF-8 de `text`, non vide.
[[nodiscard]] std::size_t LastCharacterBytes(std::string_view text) {
  constexpr unsigned char kContinuationMask = 0xC0;
  constexpr unsigned char kContinuationTag = 0x80;
  std::size_t bytes = 1;
  while (bytes < text.size() && (static_cast<unsigned char>(text[text.size() - bytes]) &
                                 kContinuationMask) == kContinuationTag) {
    ++bytes;
  }
  return bytes;
}

// Un glyphe rogné à `zone`, sa portion de texture rognée d'autant, ou rien
// s'il en sort entièrement.
[[nodiscard]] std::optional<ScreenQuad> ClipToZone(const ScreenQuad& glyph,
                                                   const ScreenRect& zone) {
  const glm::vec2 min = glm::max(glyph.min, zone.min);
  const glm::vec2 max = glm::min(glyph.max, zone.max);
  if (min.x >= max.x || min.y >= max.y) {
    return std::nullopt;
  }
  ScreenQuad kept = glyph;
  const glm::vec2 size = glyph.max - glyph.min;
  const glm::vec2 uv_span = glyph.uv_max - glyph.uv_min;
  kept.min = min;
  kept.max = max;
  kept.uv_min = glyph.uv_min + uv_span * (min - glyph.min) / size;
  kept.uv_max = glyph.uv_min + uv_span * (max - glyph.min) / size;
  return kept;
}

}  // namespace

formats::Image ComposeChatRoomSurface(const ChatRoomFrame& frame, const formats::Image& icon) {
  formats::Image surface = BlankSurface();
  const int width = kSignboardWidthPixels;
  const int height = kSignboardHeightPixels;
  const int span = width - 2 * kChatRoomFrameMarginPixels;
  const int repeats = (span + kChatRoomFrameStepPixels - 1) / kChatRoomFrameStepPixels;

  // ⚠ L'ORDRE EST CELUI DU CLIENT, et il compte : chaque pièce efface ce
  // qu'elle recouvre. Les deux coins d'une rangée, PUIS son bord — qui mord
  // donc sur le coin de droite.
  Blit(surface, frame.top_left, 0, 0, false);
  Blit(surface, frame.top_right, width - frame.top_right.width, 0, false);
  for (int rank = 0; rank < repeats; ++rank) {
    Blit(surface, frame.top, kChatRoomFrameMarginPixels + rank * kChatRoomFrameStepPixels, 0,
         false);
  }
  const auto bottom_row_y = [&](const formats::Image& piece) {
    return height - piece.height - kChatRoomTailRoomPixels;
  };
  Blit(surface, frame.bottom_left, 0, bottom_row_y(frame.bottom_left), false);
  Blit(surface, frame.bottom_right, width - frame.bottom_right.width,
       bottom_row_y(frame.bottom_right), false);
  for (int rank = 0; rank < repeats; ++rank) {
    Blit(surface, frame.bottom, kChatRoomFrameMarginPixels + rank * kChatRoomFrameStepPixels,
         bottom_row_y(frame.bottom), false);
  }
  Blit(surface, frame.tail, width / 2 - frame.tail.width / 2, height - frame.tail.height, false);
  Blit(surface, icon, kSignboardIconAt.x, kSignboardIconAt.y, true);
  return surface;
}

formats::Image ComposeShopSurface(const formats::Image& icon,
                                  const std::array<std::uint8_t, 3>& fill) {
  formats::Image surface = BlankSurface();
  const int width = kSignboardWidthPixels;
  const int panel_height = kSignboardHeightPixels - kShopSignBottomRoomPixels;
  RoundedFill(surface, 0, 0, width, panel_height, fill);
  RoundedOutline(surface, kShopSignEdgeInsetPixels, kShopSignEdgeInsetPixels,
                 width - 2 * kShopSignEdgeInsetPixels, panel_height - 2 * kShopSignEdgeInsetPixels,
                 kShopSignEdge);
  Blit(surface, icon, kSignboardIconAt.x, kSignboardIconAt.y, true);
  return surface;
}

SignboardSurfaces LoadSignboardSurfaces(vfs::Vfs& vfs) {
  const ChatRoomFrame frame{
      .top_left = LoadPiece(vfs, kChatRoomFrameTopLeftPath),
      .top_right = LoadPiece(vfs, kChatRoomFrameTopRightPath),
      .top = LoadPiece(vfs, kChatRoomFrameTopPath),
      .bottom_left = LoadPiece(vfs, kChatRoomFrameBottomLeftPath),
      .bottom_right = LoadPiece(vfs, kChatRoomFrameBottomRightPath),
      .bottom = LoadPiece(vfs, kChatRoomFrameBottomPath),
      .tail = LoadPiece(vfs, kChatRoomFrameTailPath),
  };
  const formats::Image vending_icon = LoadPiece(vfs, kVendingIconPath);
  const formats::Image buying_icon = LoadPiece(vfs, kBuyingIconPath);
  SignboardSurfaces surfaces;
  surfaces.open_room = ComposeChatRoomSurface(frame, LoadPiece(vfs, kOpenRoomIconPath));
  surfaces.locked_room = ComposeChatRoomSurface(frame, LoadPiece(vfs, kLockedRoomIconPath));
  surfaces.vending = ComposeShopSurface(vending_icon, kShopSignFill);
  surfaces.buying = ComposeShopSurface(buying_icon, kShopSignFill);
  surfaces.spotlit_vending = ComposeShopSurface(vending_icon, kSpotlitShopSignFill);
  surfaces.spotlit_buying = ComposeShopSurface(buying_icon, kSpotlitShopSignFill);
  return surfaces;
}

std::string FitSignboardTitle(const FontAtlas& font, std::string_view title) {
  const auto widest = static_cast<float>(kSignboardTitleWidthPixels);
  if (MeasureText(font, title) <= widest) {
    return std::string(title);
  }
  std::string kept(title);
  std::string shown;
  // ⚠ LE DERNIER CARACTÈRE RESTE TOUJOURS : un titre dont même la première
  // lettre et « ... » débordent s'écrit ainsi, et déborde.
  while (kept.size() > 1) {
    kept.resize(kept.size() - LastCharacterBytes(kept));
    shown = kept + std::string(kSignboardEllipsis);
    if (MeasureText(font, shown) <= widest) {
      return shown;
    }
  }
  return kept + std::string(kSignboardEllipsis);
}

glm::vec2 SignboardTopLeft(glm::vec2 head_px, int lift_pixels) {
  return {std::trunc(head_px.x) - static_cast<float>(kSignboardWidthPixels / 2),
          std::trunc(head_px.y - static_cast<float>(lift_pixels))};
}

ScreenRect SignboardRect(glm::vec2 top_left) {
  return ScreenRect{.min = top_left,
                    .max = top_left + glm::vec2{static_cast<float>(kSignboardWidthPixels),
                                                static_cast<float>(kSignboardHeightPixels)}};
}

ScreenRect SignboardTitleRect(glm::vec2 top_left) {
  const glm::vec2 min = top_left + glm::vec2{kSignboardTitleAt};
  return ScreenRect{.min = min,
                    .max = min + glm::vec2{static_cast<float>(kSignboardTitleWidthPixels),
                                           static_cast<float>(kSignboardTitleHeightPixels)}};
}

std::vector<ScreenQuad> ComposeSignboard(const FontAtlas& font, const formats::Image& surface,
                                         std::string_view shown_title, glm::vec2 top_left) {
  std::vector<ScreenQuad> quads;
  if (!surface.empty()) {
    // ⚠ À SA TAILLE ET SUR LA GRILLE DES PIXELS : chaque pixel de l'écran tombe
    // au centre d'un texel, et le filtrage n'en mélange aucun.
    ScreenQuad panel;
    panel.min = top_left;
    panel.max =
        top_left + glm::vec2{static_cast<float>(surface.width), static_cast<float>(surface.height)};
    panel.atlas = &surface;
    quads.push_back(panel);
  }
  const ScreenRect zone = SignboardTitleRect(top_left);
  for (const ScreenQuad& glyph :
       ComposeText(font, shown_title, glm::vec2{zone.min.x, zone.min.y + font.ascent()},
                   kSignboardTitleColor)) {
    if (const std::optional<ScreenQuad> kept = ClipToZone(glyph, zone); kept.has_value()) {
      quads.push_back(*kept);
    }
  }
  return quads;
}

}  // namespace ro::render
