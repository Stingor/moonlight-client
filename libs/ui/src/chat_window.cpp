#include "ro/ui/chat_window.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include <imgui_internal.h>

#include "ro/core/ascii.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/window_name.h"

#include "chat_input_chips.h"
#include "chat_tab_menu.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Ce que la fenêtre mesure la première fois. Large et basse : c'est la forme
// d'une chatbox, et le joueur la règle ensuite.
constexpr float kFirstWidthPixels = 620.0F;
constexpr float kFirstHeightPixels = 220.0F;

// Le pourcentage est un entier ; la taille en découle.
constexpr float kPercent = 100.0F;
// La part de la rangée que prend le champ de recherche : le reste va au bouton
// et à la case.
constexpr float kSearchFieldShare = 0.45F;
// Le gris du mot qui remplace la barre prêtée : il se lit sans se prendre pour
// une ligne du fil.
constexpr ImU32 kLentInputColor = IM_COL32(160, 160, 160, 255);

// LA RANGÉE DE RECHERCHE, sous la bande. Elle ne paraît que dépliée.
//
// ⚠ LE CHAMP ET LE BOUTON SONT CLAIRS alors que le cadre pousse un texte clair
// pour son fond sombre : clair sur clair, on ne lit rien. Le texte repasse en
// sombre le temps de ces deux-là, et SEULEMENT eux — le libellé de la case,
// lui, est sur le fond sombre.
void DrawSearchRow(ChatWindowState& state, const SkinArt& art) {
  ImGui::PushStyleColor(ImGuiCol_Text, kChatDarkTextColor);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * kSearchFieldShare);
  ImGui::InputTextWithHint("##recherche", core::Tr("Rechercher..."), state.search.data(),
                           state.search.size());
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Effacer"), art)) {
    state.search[0] = '\0';
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();
  // ⚠ LE SURVOL SE TESTE À PART DU CLIC : liés par `&&`, l'infobulle ne
  // paraissait qu'à l'image où la case changeait d'état, donc jamais.
  static_cast<void>(SkinCheckBox(core::Tr("Sélection"), &state.selecting, art));
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", core::Tr("Du texte nu, qu'on glisse pour sélectionner."));
  }
}

// LE FIL EN TEXTE NU, le temps d'en copier un morceau.
//
// ⚠ LES COULEURS ET LE REPLI DISPARAISSENT, et c'est le marché : ImGui sait
// sélectionner du texte dans un champ, pas dans ce qu'on peint soi-même. Le
// joueur échange l'allure contre Ctrl+C, le temps d'un geste.
void DrawSelectableLog(ChatWindowState& state, std::span<const ChatLineView> lines, float height) {
  state.selected.clear();
  for (const ChatLineView& line : lines) {
    state.selected.append(line.text);
    state.selected.push_back('\n');
  }
  ImGui::PushStyleColor(ImGuiCol_Text, kChatDarkTextColor);
  // `&buf[0]` et non `.data()` : valable même sur une chaîne vide. En lecture
  // seule, ImGui ne touche jamais au tampon — la taille plus un ne lui sert
  // qu'à en connaître la fin.
  ImGui::InputTextMultiline("##fil_nu", &state.selected[0], state.selected.size() + 1,
                            ImVec2(-FLT_MIN, height), ImGuiInputTextFlags_ReadOnly);
  ImGui::PopStyleColor();
}

// LE MOT QUI TIENT LA PLACE DE LA BARRE PRÊTÉE À UN SALON. Il dit où l'on
// écrit, et son clic ramène la salle devant.
void DrawLentInputWord(ChatWindowAction* asked) {
  ImGui::AlignTextToFramePadding();
  ColoredText(kLentInputColor, core::Tr("Tu écris dans le salon de chat."));
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s\n%s", core::Tr("Dans un salon, tout ce que tu dis y part."),
                      core::Tr("Clic : ramener la fenêtre du salon devant."));
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      asked->lent_input_asked = true;
    }
  }
}

}  // namespace

std::string ChatWindowId(std::uint32_t group) {
  if (group == 0) {
    return kChatWindowId;
  }
  return std::string(kChatWindowId) + "_" + std::to_string(group);
}

ChatWindowState& ChatWindows::For(std::uint32_t group) {
  if (group == 0) {
    return main;
  }
  const auto found = floating.find(group);
  if (found != floating.end()) {
    return found->second;
  }
  ChatWindowState& fresh = floating[group];
  fresh.group = group;
  fresh.name = WindowName(kChatWindowTitle, ChatWindowId(group));
  return fresh;
}

void ChatWindows::Keep(std::span<const std::uint32_t> living) {
  for (auto it = floating.begin(); it != floating.end();) {
    it = std::find(living.begin(), living.end(), it->first) == living.end() ? floating.erase(it)
                                                                            : std::next(it);
  }
}

ChatTabPowers ChatTabMenuPowers(std::uint32_t group, int tabs_here, int tabs_everywhere) {
  return ChatTabPowers{
      .can_add = tabs_everywhere < kMostTabs,
      // La principale porte la saisie : sans onglet il ne resterait rien où
      // lire ni rien où cliquer pour en refaire un. Une flottante, elle,
      // s'efface avec son dernier onglet et la conversation reste entière.
      .can_close = group != 0 || tabs_here > 1,
      // Sortir le seul onglet d'une fenêtre ne le déplace nulle part : il
      // serait seul dans une fenêtre à lui, ce qu'il est déjà.
      .can_detach = tabs_here > 1,
  };
}

ChatWindowAction DrawChatWindow(ChatWindowState& state, const ChatAppearance& look,
                                const ChatLinePreview& preview, std::span<const ChatLineView> lines,
                                ChatLogLayout* log_layout, std::span<const ChatStripTab> tabs,
                                int tabs_everywhere, std::span<const ChatTypeToggle> types,
                                const ChatTabLook& tuned, std::span<const ChatEmotion> emotions,
                                std::span<const ChatEmojiGroup> emojis, ChatDragBoard& board,
                                const SkinArt& art, std::span<const ChatInputLink> input_links,
                                ItemLinkFacts link_facts, const DamageMeterView* meter) {
  ChatWindowAction asked;
  if (!state.open) {
    return asked;
  }

  // ── LES TOUCHES REÇUES HORS DU DESSIN, D'ABORD ─────────────────────────────
  // 🔴 AVANT TOUT LE RESTE, et pas dans la rangée de saisie : repliée, celle-ci
  // n'est pas dessinée du tout, donc rien n'y consommerait la touche — la barre
  // ne se serait jamais ouverte. Et la décision change la HAUTEUR que la rangée
  // prendra, ce qu'il est trop tard de découvrir une fois le cadre ouvert.
  //
  // ⚠ SEULE LA PRINCIPALE A UNE SAISIE : une flottante n'a aucune touche à
  // pomper, et son `ChatInputState` ne sert à rien.
  // UN SALON OUVERT VAUT « HORS BATTLE MODE » : la barre y est permanente, et
  // son dépliement ne décrit plus rien.
  const bool battle_mode = look.battle_mode && !state.input_lent;
  std::optional<std::string> said_outside;
  if (state.group == 0) {
    // ⚠ CHANGER DE MODE REPLIE. En sortir doit rendre la barre tout de suite ; y
    // entrer doit la reprendre, sinon elle resterait ouverte jusqu'au prochain
    // Échap — donc visible alors que le joueur vient de demander qu'elle ne le
    // soit plus.
    if (state.input.battle_mode != battle_mode) {
      state.input.battle_mode = battle_mode;
      state.input.unfolded = false;
    }
    ChatBarAnswer from_keys = PumpChatBarKeys(state.input);
    said_outside = std::move(from_keys.said);
    asked.folded = from_keys.folded;
  }

  ChatLogStyle log_style;
  // ⚠ RELEVÉE HORS DE TOUTE FENÊTRE : c'est la seule taille de police qui ne
  // porte l'échelle d'aucune d'entre elles. Tout le fil s'en déduit.
  log_style.font_size = ImGui::GetFontSize() * static_cast<float>(look.font_percent) / kPercent;
  log_style.line_gap = static_cast<float>(look.line_gap_pixels);
  log_style.timestamps = look.timestamps;
  log_style.link_facts = link_facts;
  log_style.rich = look.rich;
  log_style.staff_warp = look.staff_warp;
  log_style.font_percent = look.font_percent;

  ChatStyle frame;
  state.input.grow = look.grow_input;
  // ⚠ POSÉE À CHAQUE IMAGE, comme le reste de l'apparence : le joueur peut la
  // changer pendant qu'il écrit, et l'historique doit se resserrer tout de
  // suite plutôt qu'au prochain démarrage.
  state.input.history.kept = look.kept_typed;
  frame.body = look.body;
  frame.border = look.border;
  frame.padding = static_cast<float>(look.padding_pixels);
  frame.line_gap = static_cast<float>(look.line_gap_pixels);
  frame.row_pixels = state.row_pixels;
  frame.chrome_pixels = state.chrome_pixels;
  frame.movable = !look.locked;
  frame.resizable = !look.locked;

  // ⚠ LE NOM VIENT DE L'ÉTAT, pas d'un statique : il y a autant de fenêtres de
  // conversation que de groupes, et chacune a besoin de la sienne pour garder
  // sa place à l'écran.
  WindowName& name = state.name;
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  // ── LA FENÊTRE GAGNE, OU REND, CE QUE LA PHRASE DEMANDE ────────────────
  // Vers le HAUT : le bas ne bouge pas, parce que c'est là qu'est la saisie et
  // que le joueur y a les yeux.
  //
  // ⚠ LE PAS EST BORNÉ PAR CE QUE LES CONTRAINTES LAISSERONT PASSER. Sans cette
  // borne, une fenêtre déjà à son maximum ne grandirait pas mais sa POSITION,
  // elle, monterait quand même — et glisserait d'un cran par image.
  if (state.grow_step != 0.0F) {
    if (const ImGuiWindow* window = ImGui::FindWindowByName(name.Get())) {
      const ImVec2 display = ImGui::GetIO().DisplaySize;
      const float tallest = std::max(kChatSmallestHeight, display.y * kChatLargestScreenShare);
      const float step = std::clamp(state.grow_step, kChatSmallestHeight - window->Size.y,
                                    tallest - window->Size.y);
      if (step != 0.0F) {
        ImGui::SetNextWindowSize(ImVec2(window->Size.x, window->Size.y + step), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(window->Pos.x, window->Pos.y - step), ImGuiCond_Always);
        state.grown_pixels += step;
      }
    }
    state.grow_step = 0.0F;
  }
  // LES MENUS ET LES BULLES SUIVENT LE THÈME, le cadre garde son corps : voir
  // `LendSkinArt`. ⚠ PRÊTÉ JUSQU'À LA SORTIE, par où qu'on sorte.
  const LentSkinArt popups_dressed(art);
  if (!BeginChatFrame(name.Get(), frame)) {
    EndChatFrame();
    return asked;
  }
  // Ce que cette fenêtre-ci est. Relevé UNE fois : le reste du dessin s'y
  // réfère, et le relire à chaque endroit inviterait à en oublier un.
  const bool main_window = state.group == 0;
  // ET LA BARRE EST-ELLE LÀ ? Une flottante n'en a jamais ; la principale la
  // perd quand le battle mode l'a repliée. ⚠ TOUT CE QUI MESURE LA RANGÉE DOIT
  // LIRE CE MOT-CI : mesurer une rangée qu'on ne dessine pas laisse un trou
  // sous le fil, et la fenêtre gagnerait une hauteur pour personne.
  // ⚠ PRÊTÉE À UN SALON, elle se dessine dans sa salle : ici ne reste qu'un mot.
  const bool lent = main_window && state.input_lent;
  const bool draws_input =
      main_window && !lent && ChatBarShown(look.battle_mode, state.input.unfolded);

  // ⚠ LE RANG SE REBORNE À CHAQUE IMAGE. Un onglet retiré — par ce joueur ou par
  // une relecture du fichier — laisserait sinon un rang qui désigne le vide.
  const int last = static_cast<int>(tabs.size()) - 1;
  state.tab = std::clamp(state.tab, 0, std::max(0, last));

  ChatStripAction strip;
  DrawChatStrip(state.strip, tabs, state.tab, look.tabs, state.searching, state.group, emotions,
                emojis, board, &strip);
  // CE QUE LA PALETTE ÉCRIT : un emoji pioché, ou le `:nom:` d'une émoticone
  // prise au Maj+clic.
  std::string picked(strip.chose_emoji);
  if (!strip.chose_emote_token.empty()) {
    picked = EmoteTokenText(strip.chose_emote_token);
  }
  if (!picked.empty() && !main_window) {
    // 🔴 UNE FLOTTANTE N'A PAS DE SAISIE : ce qu'on y pioche remonte, et
    // l'appelant le pose dans celle de la principale. L'écrire dans l'état de la
    // flottante le perdait sans un mot.
    asked.insert_into_main = std::move(picked);
  } else if (!picked.empty()) {
    // ⚠ AJOUTÉ AU DÉPÔT, jamais à sa place : une ligne retenue pour sa longueur
    // y attend peut-être. Ce qui ne tient plus se perd seul.
    static_cast<void>(AppendPickedToChatInput(state.input, picked));
    // 🔴 ET LA BARRE S'OUVRE SI LE BATTLE MODE L'AVAIT REPLIÉE. La bande, elle,
    // reste dessinée : le joueur peut donc piocher un emoji sans barre à
    // l'écran, et il tomberait dans un champ que rien ne montre. Tout ce qui
    // écrit dans la saisie doit la déplier.
    state.input.unfolded = true;
  }
  // Le geste est une SOURIS, mais ce qui suit reste du clavier. ⚠ SEULEMENT UNE
  // FOIS LA PALETTE REFERMEE : lui prendre le focus pendant qu'elle est ouverte
  // la referme, et l'on ne pourrait poser qu'un emoji a la fois.
  if (strip.give_back_keyboard) {
    state.input.wants_focus = true;
  }
  if (strip.chose >= 0) {
    state.tab = strip.chose;
  }
  if (strip.moved_from >= 0) {
    asked.moved_from = strip.moved_from;
    asked.moved_to = strip.moved_to;
  }
  if (strip.configure >= 0) {
    state.configuring = strip.configure;
    CopyBounded(state.tab_name, tabs[static_cast<std::size_t>(strip.configure)].name);
    ImGui::OpenPopup(kChatTabMenuId);
  }
  // L'émoticone choisie remonte telle quelle : `ui` ne sait pas l'envoyer.
  asked.chose_emotion = strip.chose_emotion;
  if (!strip.sent_emote_token.empty()) {
    asked.sent_emote = EmoteTokenText(strip.sent_emote_token);
  }
  if (strip.toggled_search) {
    state.searching = !state.searching;
    // Replier la recherche rend le fil à son allure : laisser le texte nu
    // derrière un réglage qu'on ne voit plus serait un piège.
    if (!state.searching) {
      state.selecting = false;
    }
  }
  if (state.searching) {
    DrawSearchRow(state, art);
  }

  // Ce que l'onglet MONTRE vraiment : la recherche retire des lignes, elle n'en
  // ajoute jamais. Vide ou repliée, on garde la liste reçue telle quelle — pas
  // de recopie pour rien.
  std::span<const ChatLineView> shown = lines;
  if (state.searching && state.search[0] != '\0') {
    const std::string_view needle(state.search.data());
    state.shown.clear();
    for (const ChatLineView& line : lines) {
      if (core::AsciiContainsFolded(line.text, needle)) {
        state.shown.push_back(line);
      }
    }
    shown = state.shown;
  }

  // Le fil prend tout ce qui reste, moins la rangée de saisie — et celle-ci
  // DIT SA HAUTEUR, qui n'est plus fixe depuis qu'elle grandit avec la phrase.
  //
  // ⚠ UNE FLOTTANTE N'A PAS DE SAISIE, et le fil y prend donc toute la place.
  // C'est ce que fait le client d'origine : sa fenêtre détachée renvoie le
  // clavier au chat principal.
  const float spacing = ImGui::GetStyle().ItemSpacing.y;
  const float room = ImGui::GetContentRegionAvail().y;
  // Le plafond cédé à la saisie laisse au fil au moins une ligne.
  float row_height =
      draws_input ? ChatInputHeight(state.input, preview, room - log_style.font_size - spacing, art)
                  : 0.0F;
  if (lent) {
    row_height = ImGui::GetFrameHeight();
  }
  // Ce qui MANQUE au champ (ou ce qu'il a en trop) une fois le fil réduit à sa
  // dernière ligne : la fenêtre le gagnera — ou le rendra — à l'image suivante.
  // Compté par rapport au cumul DÉJÀ accordé : à l'état grandi le plafond a
  // monté d'autant, donc l'écart retombe à zéro et la fenêtre ne bouge plus —
  // pas d'oscillation.
  //
  // ⚠ ET UNE BARRE QUI DISPARAÎT REND TOUT CE QU'ELLE AVAIT PRIS. Sans cela, le
  // battle mode qui replie une barre de trois rangées laisserait la fenêtre
  // haute de deux rangées de trop, pour un champ que plus rien ne dessine.
  const int missing = draws_input ? state.input.rows_wanted - state.input.rows_allowed : 0;
  const float wanted =
      draws_input
          ? std::max(0.0F, state.grown_pixels + static_cast<float>(missing) * ImGui::GetFontSize())
          : 0.0F;
  state.grow_step = wanted - state.grown_pixels;
  float log_height = draws_input || lent ? room - row_height - spacing : room;
  // Jamais moins d'UNE ligne de fil : une chatbox sans rien où lire ne serait
  // plus qu'une barre de saisie.
  log_height = std::max(log_height, log_style.font_size);

  // ── Les mesures de la PROCHAINE image ──────────────────────────────────────
  // Le « chrome » n'est pas seulement ce qui entoure le fil : dedans, la marge de
  // l'enfant et le débord du dernier glyphe ne sont pas non plus du texte. Les
  // oublier, c'est contraindre la fenêtre sur une grille décalée — donc continuer
  // à couper une ligne alors même que le pas est juste.
  state.row_pixels = ChatLineStep(log_style);
  state.chrome_pixels = ImGui::GetWindowSize().y - log_height +
                        2.0F * ImGui::GetStyle().WindowPadding.y + ChatLineOverhang(log_style);

  // ⚠ L'ONGLET DU COMPTEUR PEINT LE COMPTEUR, à la place exacte du fil : la
  // saisie, la bande et les mesures de la fenêtre n'en savent rien.
  const bool meter_shown =
      meter != nullptr && !tabs.empty() && tabs[static_cast<std::size_t>(state.tab)].meter;
  if (meter_shown) {
    DrawDamageMeterBody(*meter, ImVec2(ImGui::GetContentRegionAvail().x, log_height), &asked.meter);
  } else if (state.selecting) {
    DrawSelectableLog(state, shown, log_height);
  } else {
    // ⚠ EN MODE SÉLECTION LE FIL EST UN CHAMP DE TEXTE, et un champ n'a pas de
    // liens : c'est le prix du copier-coller, et c'est ce qu'on vient y chercher.
    const ChatLogAction drawn = DrawChatLog(shown, log_style, log_height, art, log_layout);
    asked.link = drawn.link;
    // ⚠ LE ZOOM REMONTE, IL NE S'APPLIQUE PAS ICI : la taille du texte est un
    // réglage — de l'onglet ou de la conversation —, et c'est `app` qui le tient.
    asked.zoom_steps = drawn.zoom_steps;
  }

  if (lent) {
    DrawLentInputWord(&asked);
  }
  // UNE BARRE QUI N'EST PAS DESSINÉE N'A PAS LE CLAVIER : sans ce mot, repliée
  // pendant qu'on y écrivait, elle resterait « la dernière saisie active ».
  if (main_window && !draws_input) {
    state.input.was_active = false;
  }
  if (draws_input) {
    ChatInputAsk from_row;
    from_row.chose_mode = asked.chose_mode;
    asked.said = DrawChatInput(state.input, preview, &from_row, art, input_links, link_facts);
    asked.chose_mode = from_row.chose_mode;
    asked.said_keys = from_row.keys;
    // ⚠ LA SAISIE L'EMPORTE SUR LE FIL, et l'ordre ne décide de rien : le joueur
    // n'a qu'une souris, et les deux surfaces ne se recouvrent pas.
    if (from_row.link.gesture != ChatLinkGesture::kNothing) {
      asked.link.gesture = from_row.link.gesture;
      // ⚠ LE GENRE AVEC LE GESTE : sans lui, le geste d'une pastille de MVP ou
      // de recherche se lisait comme celui d'un objet.
      asked.link.kind = from_row.link.kind;
      asked.link.item_id = from_row.link.item_id;
      asked.link.token = from_row.link.token;
      asked.link.from_input = true;
    }
    if (from_row.link.hovering) {
      asked.link.hovered_id = from_row.link.hovered_id;
      asked.link.hovered_kind = from_row.link.hovered_kind;
      asked.link.hovered_token = from_row.link.hovered_token;
      asked.link.hovered_from_input = true;
      asked.link.hovering = true;
    }
    asked.forget_whispers = from_row.forget_whispers;
    // Le repli demandé PAR LA RANGÉE : Entrée sur une phrase vide, ou Échap qui
    // vient de quitter l'une des deux boîtes.
    asked.folded = asked.folded || from_row.folded;
  }
  // ⚠ CE QU'ENTRÉE A VALIDÉ DEPUIS DEHORS NE S'ÉCRASE PAS. La rangée n'a rien
  // pu rendre dans ce cas-là — elle n'avait pas le clavier, et elle n'est
  // parfois même pas dessinée.
  if (!asked.said.has_value()) {
    asked.said = std::move(said_outside);
  }

  // ⚠ LE MENU SE DESSINE DANS LA MÊME FENÊTRE QUE SON OUVERTURE : son identifiant
  // se hache avec la pile d'identifiants de la fenêtre courante, et l'ouvrir
  // ailleurs qu'ici le chercherait sous un autre nom.
  DrawChatTabMenu(state, look, types, tabs, tabs_everywhere, tuned, art, &asked);

  EndChatFrame();
  return asked;
}

}  // namespace ro::ui
