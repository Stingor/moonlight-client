#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/image.h"
#include "ro/formats/web_picture.h"

namespace ro::render {

// ── UNE IMAGE DU CHAT, PRÊTE À TÉLÉVERSER ───────────────────────────────────
//
// Décodée puis réduite, sans carte graphique : le téléversement est l'affaire
// de l'appelant, sur le fil qui tient le contexte.

// LES BUDGETS. Une image fixe se réduit à 512 px de côté, une animée à 256 et à
// 60 images.
struct ChatPictureLimits {
  int most_side_still = 512;
  int most_side_animated = 256;
  int most_frames = formats::kWebPictureMostFrames;
  // Ce que toutes les images d'une animation réduite pèsent ensemble.
  std::size_t most_animated_bytes = 12U * 1024U * 1024U;
  // 🔴 LA GARDE AVANT LE DÉCODAGE : largeur × hauteur × 4 × images annoncées au
  // delà de ce budget, et l'image est refusée sans avoir été décodée.
  std::size_t most_decoded_bytes = formats::kWebPictureMostDecodedBytes;
};

struct ChatPicture {
  std::vector<formats::Image> frames;
  std::vector<std::uint32_t> delays_ms;
  [[nodiscard]] bool animated() const noexcept { return frames.size() > 1; }
};

// Mesure, refuse au-delà des budgets, décode, puis réduit.
[[nodiscard]] core::Result<ChatPicture> PrepareChatPicture(std::span<const std::uint8_t> bytes,
                                                           const ChatPictureLimits& limits = {});
// La réduction seule, pour une image décodée ailleurs. Par `FitThumbnail` et
// `ShrinkImage`.
[[nodiscard]] core::Result<ChatPicture> FitChatPicture(formats::DecodedWebPicture decoded,
                                                       const ChatPictureLimits& limits = {});

}  // namespace ro::render
