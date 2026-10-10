#include "chat/chat_line_color.h"

#include "ro/sim/battlefield.h"

namespace moonclient {
namespace {

using ro::sim::ChatKind;

// Une couleur du serveur tient sur trois octets, dans l'ordre rouge, vert,
// bleu. Nommés plutôt qu'écrits : un 16 au milieu d'un décalage ne dit pas ce
// qu'il décale.
constexpr int kRedShift = 16;
constexpr int kGreenShift = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr int kOpaque = 255;

}  // namespace

ImU32 ColorOfChatKind(ChatKind kind) {
  switch (kind) {
    case ChatKind::kPublic:
      return 0;  // la couleur ordinaire
    case ChatKind::kSelf:
      return IM_COL32(255, 255, 255, 255);
    case ChatKind::kServer:
      return IM_COL32(180, 220, 255, 255);
    case ChatKind::kAnnouncement:
      return IM_COL32(255, 220, 90, 255);
    case ChatKind::kWhisper:
      return IM_COL32(230, 160, 230, 255);
    case ChatKind::kParty:
      return IM_COL32(255, 200, 200, 255);
    case ChatKind::kGuild:
      return IM_COL32(180, 255, 180, 255);
    case ChatKind::kChannel:
      return IM_COL32(150, 210, 240, 255);
    case ChatKind::kClan:
      // Ni le rose du groupe ni le vert de la guilde : ce sont trois
      // appartenances qu'un même personnage peut tenir en même temps.
      return IM_COL32(215, 200, 140, 255);
    case ChatKind::kItem:
      // ⚠ ELLE NE SE VOIT PRESQUE JAMAIS : les lignes du sac demandent leur
      // propre couleur, et `ColorOfChatLine` ne retombe ici que si aucune n'est
      // demandée. Un gris neutre, donc, qui n'emprunte la teinte d'aucun
      // autre genre et ne promet rien de ce que la ligne dira.
      return IM_COL32(190, 190, 190, 255);
    case ChatKind::kBattle:
    case ChatKind::kPartyBattle:
    case ChatKind::kExp:
    case ChatKind::kCall:
      // ⚠ NE SE VOIT PAS NON PLUS : chacune de ces lignes demande sa couleur.
      return IM_COL32(190, 190, 190, 255);
    case ChatKind::kBattlefield:
      // LA TEINTE DE LA PAROLE DU CHAMP, la même pour les lignes reçues et
      // pour l'entrée du sélecteur qui y parle.
      return ColorOfShownRgb(ro::sim::kBattlefieldChatRgb);
  }
  return 0;
}

ImU32 ColorOfChatLine(ChatKind kind, std::uint32_t asked_rgb) {
  if (asked_rgb == 0) {
    return ColorOfChatKind(kind);
  }
  return ColorOfShownRgb(asked_rgb);
}

std::uint32_t ShownRgbOfChatLine(ChatKind kind, std::uint32_t asked_rgb) {
  if (asked_rgb != 0) {
    return asked_rgb;
  }
  const ImU32 painted = ColorOfChatKind(kind);
  if (painted == 0) {
    return 0;  // la couleur ordinaire
  }
  return (((painted >> IM_COL32_R_SHIFT) & kByteMask) << kRedShift) |
         (((painted >> IM_COL32_G_SHIFT) & kByteMask) << kGreenShift) |
         ((painted >> IM_COL32_B_SHIFT) & kByteMask);
}

ImU32 ColorOfShownRgb(std::uint32_t shown_rgb) {
  if (shown_rgb == 0) {
    return 0;
  }
  return IM_COL32((shown_rgb >> kRedShift) & kByteMask, (shown_rgb >> kGreenShift) & kByteMask,
                  shown_rgb & kByteMask, kOpaque);
}

}  // namespace moonclient
