#pragma once

#include <cstdint>
#include <span>
#include <string>

#include <imgui.h>

#include "ro/ui/chat_rich_art.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA VISIONNEUSE D'UNE IMAGE DU CHAT ──────────────────────────────────────
//
// L'image en grand, ajustée à l'écran à l'ouverture : la molette zoome, le
// double-clic montre la taille réelle, le glisser déplace. Ses boutons rendent
// une réponse que `app` joue — enregistrer, ouvrir, copier.

enum class ChatViewerAnswer : std::uint8_t { kNone, kSave, kOpenInBrowser, kCopyAddress };

struct ChatPictureViewerState {
  bool open = false;
  bool raise = false;
  // Le facteur de zoom ; zéro veut dire « ajustée à la fenêtre ».
  float zoom = 0.0F;
  ImVec2 pan{0.0F, 0.0F};
  std::string url;
  ChatViewerAnswer answer = ChatViewerAnswer::kNone;
};

// Ce que la visionneuse montre, recomposé à chaque image.
struct ChatPictureViewerView {
  ChatPictureState state = ChatPictureState::kUnknown;
  ImTextureID texture = 0;
  float width_px = 0.0F;
  float height_px = 0.0F;
  std::span<const ChatPictureFrame> frames;
};

inline constexpr const char* kChatPictureViewerId = "chat_picture_viewer";

void DrawChatPictureViewer(ChatPictureViewerState& state, const ChatPictureViewerView& view,
                           const SkinArt& art);

}  // namespace ro::ui
