// LA CONVERSATION, dans la trame de l'interface : ses fenêtres, ses salons et
// ses conversations 1:1. À part de `ui_stage.cpp`, qui n'a plus la place de la
// porter.

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/ui/chat_link.h"
#include "ro/ui/chat_picture_viewer.h"
#include "ro/ui/chat_url_prompt.h"
#include "ro/ui/font_setup.h"

#include "game/ui_stage.h"

namespace moonclient {
namespace {

// LA FAMILLE DU CODE `` `x` `` dans le fil : à chasse fixe, quand la machine
// l'a. Sans elle, le code ne se distingue que par son fond.
constexpr std::string_view kChatCodeFontFamily = "Consolas";

}  // namespace

void UiStage::DrawConversation(Windows& windows, const ro::ui::ChatLinePreview& preview,
                               const ChatTabsView& tabs,
                               std::span<const ro::ui::ChatInputLink> links,
                               std::span<const ro::ui::ChatEmotion> emotions,
                               std::span<const ro::ui::ChatEmojiGroup> emojis, Answers& answers) {
  // LA CONVERSATION, puis le dialogue par-dessus : c'est à lui qu'on DOIT
  // répondre, et il passe donc devant.
  //
  // ⚠ LES LIGNES ARRIVENT DÉJÀ TRIÉES ET DÉJÀ TEINTES. Le genre d'un message
  // décide des deux, et il vit dans `sim` : c'est la boucle qui croise, pas cet
  // étage — voir `ChatTabsView`.
  //
  // ⚠ AUTANT DE FENÊTRES QUE DE GROUPES, et la principale d'abord : les
  // flottantes se posent par-dessus, ce qui est l'ordre qu'on attend d'une
  // fenêtre qu'on vient de sortir.
  //
  // 🔴 CE QU'ELLES DEMANDENT S'ACCUMULE, RIEN NE S'APPLIQUE ICI. Un retrait
  // ou un détachement réordonne la liste des onglets ; la fenêtre suivante
  // lirait alors des rangs périmés.
  answers.chat.clear();
  // LES POLICES DES STYLES DU FIL, reposées à chaque image : la famille peut
  // changer en jeu. ⚠ UNE VARIANTE ABSENTE REND LA POLICE ORDINAIRE, et le fil
  // simule alors le gras ou l'italique.
  windows.chat.rich.bold = ui_->ChosenFont(ro::ui::FontStyle::kBold);
  windows.chat.rich.italic = ui_->ChosenFont(ro::ui::FontStyle::kItalic);
  windows.chat.rich.code = ui_->FontNamed(kChatCodeFontFamily, ro::ui::FontStyle::kRegular);
  // ⚠ LE PLATEAU SE VIDE DE SES MESURES, PAS DE SON GESTE : les rectangles
  // valent pour cette image, la languette qu'on traîne traverse les images.
  windows.chat.board.strips.clear();
  for (const std::uint32_t group : tabs.groups()) {
    ro::ui::ChatWindowState& state = windows.chat.For(group);
    // Une flottante vide ne se dessine pas : elle n'a plus d'onglet, et le tri
    // l'oubliera à la prochaine image.
    if (group != 0 && tabs.tabs(group).empty()) {
      continue;
    }
    answers.chat.push_back(ChatWindowAsk{
        .group = group,
        .asked = ro::ui::DrawChatWindow(
            state, tabs.look(group), preview, tabs.lines(group), tabs.layout(group),
            tabs.tabs(group), tabs.count(), tabs.types(group), tabs.tuned_look(group), emotions,
            emojis, windows.chat.board, skin_,
            // ⚠ À LA PRINCIPALE SEULEMENT : une flottante n'a pas de saisie, et
            // lui donner les pastilles la ferait peindre par-dessus son fil.
            group == 0 ? links : std::span<const ro::ui::ChatInputLink>(),
            std::span<const ro::ui::VoteShopOffer>(),
            windows.damage_meter_view.look.shown ? &windows.damage_meter_view : nullptr)});
  }
  // 🔴 ICI, ET PAS DANS LA BANDE QUI A VU LE LÂCHER : c'est le seul moment
  // où l'on sait laquelle était sous le curseur, puisque toutes viennent d'être
  // dessinées.
  answers.chat_drop = ro::ui::DecideChatDrop(windows.chat.board);
  DrawChatRoomWindows(windows.chat_room, windows.chat.main, preview, tabs, links,
                      windows.chat.vote_shop, answers);
  DrawWhisperWindows(windows.whispers, tabs, emotions, emojis, windows.chat.vote_shop, answers);
  // LES CONFIRMATIONS D'UN LIEN WEB ET LA VISIONNEUSE, après toutes les
  // fenêtres qui portent des liens : elles passent devant celle d'où vient le
  // clic. ⚠ DESSINÉES MÊME SANS QUESTION : une boîte congédiée rend sa réponse
  // ici.
  ro::ui::DrawChatUrlPrompt(windows.chat.url_prompt, skin_);
  ro::ui::DrawChatHostPrompt(windows.chat.host_prompt, skin_);
  ro::ui::DrawChatPictureViewer(windows.chat.viewer, windows.chat.viewer_view, skin_);
  escape_.Register(ro::ui::kChatPictureViewerId, &windows.chat.viewer.open);
  // L'INFOBULLE D'UN LIEN DE RÉGLAGE, que le survol de l'image d'avant a posée :
  // elle ne vaut que pour une image.
  if (windows.chat.setting_tip.has_value()) {
    ro::ui::DrawSettingLinkTip(*windows.chat.setting_tip);
    windows.chat.setting_tip.reset();
  }
  // 🔴 ÉCHAP QUI A REPLIÉ LA BARRE NE FERME RIEN D'AUTRE. La touche a servi :
  // sans ce mot-là, la même frappe refermerait AUSSI la fenêtre la plus au
  // dessus — deux effets pour un geste, et le joueur perd celle qu'il lisait
  // sans comprendre pourquoi.
  for (const ChatWindowAsk& ask : answers.chat) {
    if (ask.asked.folded) {
      escape_.Suppress();
    }
  }
}

}  // namespace moonclient
