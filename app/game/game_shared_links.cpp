// LES LIENS QUE L'ÉDITEUR DE STYLE PARTAGE : une tenue de la garde-robe, les
// couleurs d'un calque. Les poser dans la saisie, et suivre ceux qu'on reçoit.

#include <cstdint>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/net/style_outfits.h"
#include "ro/ui/chat_input.h"

#include "chat/chat_item_link.h"
#include "chat/chat_layer_link.h"
#include "chat/chat_outfit_link.h"
#include "game/game.h"

namespace moonclient {
namespace {

// LA BALISE ET SON LIBELLÉ, relus sur la balise : ce que le joueur voit avant
// l'envoi est ce que tous liront après, étiquette nettoyée comprise. Rien si
// le code ne part pas.
[[nodiscard]] PendingChatLink Pending(ro::ui::ChatLinkKind kind, const std::string& label,
                                      const std::string& code) {
  if (kind == ro::ui::ChatLinkKind::kOutfit) {
    const std::string wire = BuildOutfitLink(label, code);
    OutfitLink posed;
    if (wire.empty() || !ParseOutfitLink(wire, &posed, nullptr)) {
      return {};
    }
    return PendingChatLink{.shown = OutfitLinkLabel(posed.label),
                           .wire = wire,
                           .kind = kind,
                           .code = posed.code,
                           .owner = posed.label};
  }
  const std::string wire = BuildLayerLink(label, code);
  LayerLink posed;
  if (wire.empty() || !ParseLayerLink(wire, &posed, nullptr)) {
    return {};
  }
  return PendingChatLink{.shown = LayerLinkLabel(posed.label),
                         .wire = wire,
                         .kind = kind,
                         .code = posed.code,
                         .owner = posed.label};
}

}  // namespace

void Game::PostSharedLink(ro::ui::ChatLinkKind kind, const std::string& code,
                          const std::string& label) {
  // 🔴 UNE TENUE DIT À QUI ELLE EST ET COMMENT ELLE S'APPELLE : « Stingor
  // (Soirée) ». Le nom du personnage appartient au monde, d'où ce qu'il se
  // résout ici. Un calque, lui, se nomme par l'objet qu'il habille.
  std::string shown = label;
  if (kind == ro::ui::ChatLinkKind::kOutfit) {
    shown = std::string(world_.player_name());
    if (!label.empty()) {
      shown += " (" + label + ")";
    }
  }
  PendingChatLink pending = Pending(kind, shown, code);
  if (!pending.wire.empty()) {
    static_cast<void>(PlaceInChatInput(std::move(pending)));
  }
}

void Game::FollowSharedLink(const ChatLinkTarget& aimed, ro::ui::ChatLinkGesture gesture,
                            std::uint32_t now_ms) {
  const bool outfit = aimed.kind == ro::ui::ChatLinkKind::kOutfit;
  switch (gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
    case ro::ui::ChatLinkGesture::kTryStyle:
      // ⚠ LA FENÊTRE S'OUVRE AVEC : ce qu'on reçoit y arrive en proposition, et
      // rien ne change sur le personnage avant « Valider ».
      windows_.style.open = true;
      if (outfit) {
        style_view_->FetchOutfit(aimed.code, link_, world_, now_ms);
      } else {
        style_view_->OpenLayerLink(aimed.code);
      }
      break;
    case ro::ui::ChatLinkGesture::kCopyCode:
      ImGui::SetClipboardText(outfit ? ro::net::ShowOutfitCode(aimed.code).c_str()
                                     : aimed.code.c_str());
      break;
    case ro::ui::ChatLinkGesture::kRepost: {
      // ⚠ L'ÉTIQUETTE TELLE QU'ELLE A VOYAGÉ : c'est le partage d'un autre.
      PendingChatLink pending = Pending(aimed.kind, aimed.owner, aimed.code);
      if (!pending.wire.empty()) {
        static_cast<void>(PlaceInChatInput(std::move(pending)));
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace moonclient
