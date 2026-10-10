#include "ro/render/card_thumbnails.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ro/formats/bmp.h"
#include "ro/formats/image.h"
#include "ro/render/card_thumbnail_pixels.h"
#include "ro/render/item_icons.h"

namespace ro::render {
namespace {

// Vignettes décodées et téléversées par image, au plus. Une vignette coûte une
// lecture, un décodage de cent vingt mille pixels et une réduction : autour de
// la milliseconde. Six par image remplissent une double page en trois images
// sans qu'on le voie ; toutes d'un coup feraient un à-coup à chaque page.
constexpr int kLoadsPerFrame = 6;

// Entrées gardées, au plus. Une vignette de pochette (120 sur 160 à l'échelle
// 100 %) pèse 77 Ko : cent soixante font 12 Mo, une dizaine de doubles pages —
// assez pour feuilleter dans les deux sens sans recharger.
constexpr std::size_t kMostEntries = 160;

// La clé : trente-deux bits de carte, un de silhouette, deux fois douze de
// boîte — une boîte est bornée à 4095 pixels, bien au-delà d'une pochette.
constexpr int kBoxBits = 12;
constexpr int kBoxMost = (1 << kBoxBits) - 1;
constexpr int kSilhouetteShift = 2 * kBoxBits;
constexpr int kCardShift = kSilhouetteShift + 1;

[[nodiscard]] std::uint64_t KeyOf(std::uint32_t card_id, bool silhouette, int width, int height) {
  const auto w = static_cast<std::uint64_t>(std::clamp(width, 1, kBoxMost));
  const auto h = static_cast<std::uint64_t>(std::clamp(height, 1, kBoxMost));
  return (static_cast<std::uint64_t>(card_id) << kCardShift) |
         (static_cast<std::uint64_t>(silhouette ? 1 : 0) << kSilhouetteShift) | (w << kBoxBits) | h;
}

}  // namespace

CardThumbnails::CardThumbnails(vfs::Vfs& vfs, const vfs::CardIllustrations* cards)
    : vfs_(&vfs), cards_(cards) {}

void CardThumbnails::BeginFrame() {
  ++frame_;
  budget_ = kLoadsPerFrame;
}

CardThumbnails::Thumb CardThumbnails::Get(std::uint32_t card_id, bool silhouette, int box_width,
                                          int box_height) {
  Entry& entry = entries_[KeyOf(card_id, silhouette, box_width, box_height)];
  entry.last_frame = frame_;
  entry.last_use = ++uses_;
  if (!entry.loaded && !entry.missing) {
    if (budget_ <= 0) {
      return Thumb{.texture = nullptr, .pending = true};
    }
    --budget_;
    Load(entry, card_id, silhouette, box_width, box_height);
    EvictBeyondBound();
  }
  // ⚠ L'ÉVICTION NE TOUCHE PAS À CETTE ENTRÉE — elle est servie à l'image en
  // cours — et effacer une autre clé ne déplace pas celle-ci : la référence
  // tient donc encore.
  if (entry.missing || !entry.loaded) {
    return Thumb{};
  }
  return Thumb{.texture = &entry.texture, .pending = false};
}

void CardThumbnails::Load(Entry& entry, std::uint32_t card_id, bool silhouette, int box_width,
                          int box_height) {
  entry.missing = true;
  if (vfs_ == nullptr || cards_ == nullptr) {
    return;
  }
  const std::string path = CardIllustrationPath(*cards_, card_id);
  if (path.empty()) {
    return;
  }
  auto bytes = vfs_->ReadRaw(path);
  if (!bytes) {
    return;
  }
  auto image = formats::DecodeBmp(*bytes);
  if (!image) {
    return;
  }
  // Le magenta EST la transparence des bitmaps du jeu : les coins arrondis de
  // la carte en sont peints.
  formats::ApplyMagentaColorKey(*image);
  const ThumbnailSize size = FitThumbnail(image->width, image->height, box_width, box_height);
  formats::Image thumb = ShrinkImage(*image, size);
  if (thumb.empty()) {
    return;
  }
  if (silhouette) {
    MakeSilhouette(thumb);
  }
  // ⚠ AU PLUS PROCHE : la vignette a la taille de sa pochette, chaque texel
  // tombe sur un pixel. Interpolée, elle se ramollirait.
  auto texture = GlTexture::Upload(thumb, GlTexture::Filtering::kPixelArt);
  if (!texture) {
    return;
  }
  entry.texture.texture = std::move(*texture);
  entry.texture.width_px = thumb.width;
  entry.texture.height_px = thumb.height;
  entry.loaded = true;
  entry.missing = false;
}

void CardThumbnails::EvictBeyondBound() {
  while (entries_.size() > kMostEntries) {
    auto victim = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      // 🔴 JAMAIS UNE VIGNETTE SERVIE À CETTE IMAGE : la liste de dessin ne fait
      // que NOTER sa texture, le rendu la lira plus tard. Les images d'avant,
      // elles, sont déjà rendues — leur texture peut partir.
      if (it->second.last_frame == frame_) {
        continue;
      }
      if (victim == entries_.end() || it->second.last_use < victim->second.last_use) {
        victim = it;
      }
    }
    if (victim == entries_.end()) {
      return;  // tout sert à cette image : on déborde plutôt que de casser
    }
    entries_.erase(victim);
  }
}

}  // namespace ro::render
