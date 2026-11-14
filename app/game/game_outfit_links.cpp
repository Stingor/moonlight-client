// LES LIENS DE TENUE : poser celui d'un rangement dans la saisie, et suivre
// celui qu'on reçoit.

#include <cstdint>
#include <string>

#include <imgui.h>

#include "ro/net/style_outfits.h"
#include "ro/ui/chat_input.h"

#include "chat/chat_item_link.h"
#include "chat/chat_outfit_link.h"
#include "game/game.h"

namespace moonclient {

void Game::PostOutfitLink(const std::string& code, const std::string& label) {
  // 🔴 L'ÉTIQUETTE DIT À QUI EST LA TENUE ET COMMENT ELLE S'APPELLE :
  // « Stingor (Soirée) ». Le nom du personnage appartient au monde, d'où ce
  // qu'il se résout ici et non dans l'éditeur.
  std::string owner(world_.player_name());
  if (!label.empty()) {
    owner += " (";
    owner += label;
    owner += ")";
  }
  const std::string wire = BuildOutfitLink(owner, code);
  if (wire.empty()) {
    return;
  }
  // ⚠ LE LIBELLÉ SE RELIT SUR LA BALISE : ce que le joueur voit avant l'envoi
  // est ce que tous liront après, étiquette nettoyée comprise.
  OutfitLink posed;
  if (!ParseOutfitLink(wire, &posed, nullptr)) {
    return;
  }
  static_cast<void>(PlaceInChatInput(PendingChatLink{.shown = OutfitLinkLabel(posed.label),
                                                     .wire = wire,
                                                     .kind = ro::ui::ChatLinkKind::kOutfit,
                                                     .code = posed.code,
                                                     .owner = posed.label}));
}

void Game::FollowOutfitLink(const ChatLinkTarget& aimed, ro::ui::ChatLinkGesture gesture,
                            std::uint32_t now_ms) {
  switch (gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
    case ro::ui::ChatLinkGesture::kTryStyle:
      // ⚠ LA FENÊTRE S'OUVRE AVEC : la tenue y arrive en proposition, et rien
      // ne change avant « Valider ».
      windows_.style.open = true;
      style_view_->FetchOutfit(aimed.code, link_, world_, now_ms);
      break;
    case ro::ui::ChatLinkGesture::kCopyCode:
      ImGui::SetClipboardText(ro::net::ShowOutfitCode(aimed.code).c_str());
      break;
    case ro::ui::ChatLinkGesture::kRepost: {
      // ⚠ L'ÉTIQUETTE TELLE QU'ELLE A VOYAGÉ : la tenue est celle d'un autre.
      const std::string wire = BuildOutfitLink(aimed.owner, aimed.code);
      if (!wire.empty()) {
        static_cast<void>(PlaceInChatInput(PendingChatLink{.shown = OutfitLinkLabel(aimed.owner),
                                                           .wire = wire,
                                                           .kind = ro::ui::ChatLinkKind::kOutfit,
                                                           .code = aimed.code,
                                                           .owner = aimed.owner}));
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace moonclient
