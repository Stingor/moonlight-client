// LES FENÊTRES DES SALONS, dans la trame de l'interface : leur dessin, et leur
// place dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la place
// de les porter.

#include <utility>

#include "game/ui_stage.h"

namespace moonclient {
namespace {

constexpr float kPercent = 100.0F;

}  // namespace

void UiStage::DrawChatRoomWindows(ChatRoomWindows& chat_room, ro::ui::ChatWindowState& chat,
                                  const ro::ui::ChatLinePreview& preview, const ChatTabsView& tabs,
                                  std::span<const ro::ui::ChatInputLink> links,
                                  std::span<const ro::ui::VoteShopOffer> vote_shop,
                                  Answers& answers) {
  if (chat_room.make_view != nullptr) {
    chat_room.make_answer =
        ro::ui::DrawChatRoomMakeWindow(chat_room.make, *chat_room.make_view, skin_);
  }
  if (chat_room.room_view != nullptr) {
    // LA SALLE REÇOIT CE QUE L'ÉTAPE NE PEUT PAS LUI DONNER : son fil, trié avec
    // la conversation, et la barre que celle-ci lui prête.
    ro::ui::ChatRoomView view = *chat_room.room_view;
    const ro::ui::ChatAppearance& look = tabs.look(0);
    view.lines = tabs.room_lines();
    view.layout = tabs.room_layout();
    view.log_style.font_size =
        ImGui::GetFontSize() * static_cast<float>(look.font_percent) / kPercent;
    view.log_style.line_gap = static_cast<float>(look.line_gap_pixels);
    view.log_style.timestamps = look.timestamps;
    view.log_style.vote_shop = vote_shop;
    // Le texte riche et les vignettes, comme dans le fil.
    view.log_style.rich = look.rich;
    view.log_style.staff_warp = look.staff_warp;
    view.log_style.font_percent = look.font_percent;
    view.vote_shop = vote_shop;
    view.input = &chat.input;
    // À L'ENTRÉE DANS UN SALON, LA BARRE GARDE LE CLAVIER : elle vient de
    // changer de fenêtre, et le joueur va écrire.
    if (chat_room.room.raise) {
      chat.input.wants_focus = true;
    }
    view.preview = &preview;
    view.input_links = links;
    ro::ui::ChatRoomWindowAction asked = ro::ui::DrawChatRoomWindow(chat_room.room, view, skin_);
    // CE QUE LA BARRE A RENDU DANS LA SALLE se route comme si la conversation
    // l'avait rendu : c'est la même barre, et la même ligne qui part.
    ro::ui::ChatWindowAction from_room;
    from_room.said = std::move(asked.said);
    from_room.link = asked.link;
    // ⚠ LES PASTILLES DE LA SAISIE L'EMPORTENT SUR LE FIL, comme dans la
    // conversation : le joueur n'a qu'une souris.
    if (asked.input.link.gesture != ro::ui::ChatLinkGesture::kNothing) {
      from_room.link.gesture = asked.input.link.gesture;
      from_room.link.kind = asked.input.link.kind;
      from_room.link.item_id = asked.input.link.item_id;
      from_room.link.token = asked.input.link.token;
      from_room.link.from_input = true;
    }
    if (asked.input.link.hovering) {
      from_room.link.hovered_id = asked.input.link.hovered_id;
      from_room.link.hovered_kind = asked.input.link.hovered_kind;
      from_room.link.hovered_token = asked.input.link.hovered_token;
      from_room.link.hovered_from_input = true;
      from_room.link.hovering = true;
    }
    from_room.chose_mode = asked.input.chose_mode;
    from_room.forget_whispers = asked.input.forget_whispers;
    from_room.folded = asked.input.folded;
    answers.chat.push_back(ChatWindowAsk{.group = 0, .asked = std::move(from_room)});
    chat_room.room_answer = std::move(asked);
  }
  if (chat_room.password_view != nullptr) {
    chat_room.password_answer =
        ro::ui::DrawChatRoomPasswordWindow(chat_room.password, *chat_room.password_view, skin_);
  }
}

void UiStage::RegisterChatRoomEscapes(ChatRoomWindows& chat_room) {
  // Le formulaire et l'invite se rangent dans la pile ; LA SALLE NON : la
  // fermer, c'est quitter le salon, et un Échap destiné au menu ne doit pas
  // faire sortir le joueur sans un mot.
  if (chat_room.make_view != nullptr) {
    escape_.Register(ro::ui::kChatRoomMakeWindowId, &chat_room.make.open);
  }
  if (chat_room.password_view != nullptr) {
    escape_.Register(ro::ui::kChatRoomPasswordWindowId, &chat_room.password.open);
  }
}

}  // namespace moonclient
