// LES CONVERSATIONS 1:1, dans la trame de l'interface : leur dessin. À part de
// `ui_stage.cpp`, qui n'a plus la place de les porter.

#include <string>
#include <utility>

#include "game/ui_stage.h"

namespace moonclient {
namespace {

constexpr float kPercent = 100.0F;

}  // namespace

void UiStage::DrawWhisperWindows(WhisperWindows& whispers, const ChatTabsView& tabs,
                                 std::span<const ro::ui::ChatEmotion> emotions,
                                 std::span<const ro::ui::ChatEmojiGroup> emojis,
                                 ro::ui::ItemLinkFacts link_facts, Answers& answers) {
  // ⚠ L'APPARENCE DU FIL EST CELLE DE LA CONVERSATION PRINCIPALE : une
  // conversation à part n'a pas de réglage à elle, et le joueur a déjà réglé
  // celle-là une fois.
  const ro::ui::ChatAppearance& look = tabs.look(0);
  for (const ro::ui::WhisperWindowView& composed : whispers.views) {
    const auto state = whispers.states.find(composed.peer);
    if (state == whispers.states.end()) {
      continue;
    }
    ro::ui::WhisperWindowView view = composed;
    view.lines = tabs.whisper_lines(view.peer);
    view.layout = tabs.whisper_layout(view.peer);
    view.log_style.font_size =
        ImGui::GetFontSize() * static_cast<float>(look.font_percent) / kPercent;
    view.log_style.line_gap = static_cast<float>(look.line_gap_pixels);
    view.log_style.timestamps = look.timestamps;
    view.log_style.link_facts = link_facts;
    // Le texte riche et les vignettes, comme dans le fil.
    view.log_style.rich = look.rich;
    view.log_style.staff_warp = look.staff_warp;
    view.log_style.font_percent = look.font_percent;
    // SA SAISIE A LA PALETTE DE LA BARRE : les émoticones s'y écrivent en
    // `:nom:`, les emoji s'y insèrent. Ses pastilles viennent de l'étape, qui
    // tient ses liens posés.
    view.emotions = emotions;
    view.emojis = emojis;
    view.link_facts = link_facts;
    ro::ui::WhisperWindowAction asked = ro::ui::DrawWhisperWindow(state->second, view, skin_);
    // UN GESTE SUR UN LIEN DU FIL se suit comme dans la conversation : ses
    // jetons sont ceux des onglets.
    if (asked.link.gesture != ro::ui::ChatLinkGesture::kNothing || asked.link.hovering) {
      ro::ui::ChatWindowAction from_whisper;
      from_whisper.link = asked.link;
      answers.chat.push_back(ChatWindowAsk{.group = 0, .asked = std::move(from_whisper)});
    }
    // UN GESTE SUR UNE PASTILLE DE SA SAISIE, de même : le routage lit la table
    // des liens posés de CE correspondant, que la demande nomme.
    if (asked.input_link.gesture != ro::ui::ChatLinkGesture::kNothing ||
        asked.input_link.hovering) {
      ro::ui::ChatWindowAction from_input;
      from_input.link = asked.input_link;
      // ⚠ LE SURVOL AUSSI DIT SA SURFACE : sans ce mot, le jeton d'une pastille
      // survolée se lirait dans la table du fil, et l'aperçu montrerait un autre
      // objet.
      from_input.link.hovered_from_input = asked.input_link.hovering;
      answers.chat.push_back(ChatWindowAsk{.group = 0, .asked = std::move(from_input)});
    }
    whispers.answers.emplace_back(std::string(view.peer), std::move(asked));
  }
}

void UiStage::RegisterWhisperEscapes(WhisperWindows& whispers) {
  // ⚠ LES IDENTITÉS VIVENT DANS LA TABLE, et non sur la pile : la pile retrouve
  // la fenêtre par son nom, et une chaîne temporaire pendrait.
  whispers.escape_ids.clear();
  whispers.escape_ids.reserve(whispers.states.size());
  for (auto& [peer, state] : whispers.states) {
    whispers.escape_ids.push_back(ro::ui::WhisperWindowId(peer));
    escape_.Register(whispers.escape_ids.back().c_str(), &state.open);
  }
}

}  // namespace moonclient
