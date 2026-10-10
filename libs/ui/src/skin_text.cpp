#include "ro/ui/skin_text.h"

#include <array>

namespace ro::ui {
namespace {

// Le rayon d'un cerne, en pixels d'écran. Voir l'en-tête : il ne suit PAS
// l'échelle de l'interface.
constexpr float kHaloRadius = 1.0F;
// Le décalage du faux gras.
constexpr float kBoldOffset = 1.0F;

// Les quatre voisins d'un cerne en croix. Les quatre diagonales manquantes sont
// ce qui le distingue du cerne complet — voir l'en-tête.
constexpr auto kCross =
    std::to_array<ImVec2>({ImVec2(-kHaloRadius, 0.0F), ImVec2(kHaloRadius, 0.0F),
                           ImVec2(0.0F, -kHaloRadius), ImVec2(0.0F, kHaloRadius)});

}  // namespace

void AddTextHalo(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, ImU32 halo,
                 bool bold) {
  AddTextHalo(into, at, color, text, nullptr, halo, bold);
}

void AddTextHalo(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, const char* text_end,
                 ImU32 halo, bool bold) {
  if (into == nullptr || text == nullptr) {
    return;
  }
  for (float down = -kHaloRadius; down <= kHaloRadius; down += kHaloRadius) {
    for (float across = -kHaloRadius; across <= kHaloRadius; across += kHaloRadius) {
      if (across == 0.0F && down == 0.0F) {
        continue;
      }
      into->AddText(ImVec2(at.x + across, at.y + down), halo, text, text_end);
    }
  }
  into->AddText(at, color, text, text_end);
  if (bold) {
    into->AddText(ImVec2(at.x + kBoldOffset, at.y), color, text, text_end);
  }
}

void AddTextHalo(ImDrawList* into, float size, ImVec2 at, ImU32 color, const char* text, ImU32 halo,
                 bool bold) {
  if (into == nullptr || text == nullptr) {
    return;
  }
  if (size <= 0.0F) {
    AddTextHalo(into, at, color, text, halo, bold);
    return;
  }
  ImFont* const font = ImGui::GetFont();
  // ⚠ LE CERNE GARDE SON RAYON EN PIXELS, quelle que soit la taille du texte :
  // c'est ce qui le rend lisible sur une icône, et un rayon proportionnel
  // disparaîtrait justement là où le texte est le plus petit.
  for (float down = -kHaloRadius; down <= kHaloRadius; down += kHaloRadius) {
    for (float across = -kHaloRadius; across <= kHaloRadius; across += kHaloRadius) {
      if (across == 0.0F && down == 0.0F) {
        continue;
      }
      into->AddText(font, size, ImVec2(at.x + across, at.y + down), halo, text);
    }
  }
  into->AddText(font, size, at, color, text);
  if (bold) {
    into->AddText(font, size, ImVec2(at.x + kBoldOffset, at.y), color, text);
  }
}

void AddTextCrossHalo(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, ImU32 halo) {
  if (into == nullptr || text == nullptr) {
    return;
  }
  for (const ImVec2& neighbour : kCross) {
    into->AddText(ImVec2(at.x + neighbour.x, at.y + neighbour.y), halo, text);
  }
  into->AddText(at, color, text);
}

void AddTextRelief(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, ImU32 relief,
                   ImVec2 offset, bool bold) {
  if (into == nullptr || text == nullptr) {
    return;
  }
  // Une ombre à décalage NUL est un cas normal, pas un oubli : un réglage du
  // joueur peut l'annuler, et la peindre alors ne ferait que doubler le texte.
  if (offset.x != 0.0F || offset.y != 0.0F) {
    into->AddText(ImVec2(at.x + offset.x, at.y + offset.y), relief, text);
  }
  into->AddText(at, color, text);
  if (bold) {
    into->AddText(ImVec2(at.x + kBoldOffset, at.y), color, text);
  }
}

}  // namespace ro::ui
