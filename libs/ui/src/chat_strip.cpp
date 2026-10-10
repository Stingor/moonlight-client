#include "ro/ui/chat_strip.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_frame.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"

namespace ro::ui {
namespace {

// La grille des emoticones, et sa forme. Dix par rangee : c'est ce qui tient
// dans la largeur d'une chatbox sans que le popup ne deborde de l'ecran.
constexpr const char* kEmotionGrid = "##emotes";
// Combien de rangées la grille montre avant de défiler.
constexpr float kEmotionRowsShown = 6.0F;
// Ce qu'un emoji occupe de sa case : assez pour se voir, assez peu pour ne
// pas mordre sa voisine.
constexpr float kEmojiOfCell = 0.72F;
// De secondes en millisecondes : la cadence d'une action est en ms.
constexpr double kMsPerSecond = 1000.0;
constexpr int kEmotionsPerRow = 10;
// Le cote d'une case, en hauteurs de texte. Assez pour reconnaitre un visage.
constexpr float kEmotionCellRatio = 1.6F;

// Une couleur éclaircie d'autant, son opacité inchangée.
[[nodiscard]] ImU32 Lighten(ImU32 color, int amount) {
  const auto channel = [color, amount](int shift) {
    return std::min(255, static_cast<int>((color >> shift) & 0xFFU) + amount);
  };
  return IM_COL32(channel(IM_COL32_R_SHIFT), channel(IM_COL32_G_SHIFT), channel(IM_COL32_B_SHIFT),
                  (color >> IM_COL32_A_SHIFT) & 0xFFU);
}

// Le nom d'une languette, en chaîne C. ImGui en veut une, et la vue qu'on reçoit
// n'en est pas une.
using TabLabel = std::array<char, 64>;
[[nodiscard]] TabLabel LabelOf(std::string_view name) {
  TabLabel label{};
  const std::size_t kept = std::min(name.size(), label.size() - 1);
  std::copy_n(name.data(), kept, label.data());
  label[kept] = '\0';
  return label;
}

// Une infobulle en deux lignes, au fond et au texte du thème : voir
// `LendSkinArt`.
//
// ⚠ DEUX TEXTES ET NON UN SEUL À DEUX LIGNES : le catalogue de traduction est
// une ligne par texte, pris verbatim et sans échappement — un saut de ligne
// dans la CLÉ ne peut pas s'y écrire. Le saut vit donc ici, entre deux textes
// que le catalogue porte chacun sur sa ligne.
//
// ⚠ LES DEUX TEXTES ARRIVENT DÉJÀ COMPOSÉS : un format reçu en paramètre ne se
// vérifie plus contre ses arguments, alors qu'écrit à l'appel sous `Tr`, il se
// vérifie comme un littéral.
void TwoLineTooltip(const char* first, const char* second) {
  SkinTooltipFormat("%s\n%s", first, second);
}

// La place de la première ligne d'une infobulle de languette, nom compris.
constexpr std::size_t kTooltipHeadRoom = 160;

// Peint une languette : son fond, son filet, son nom.
void PaintTab(ImDrawList* into, ImVec2 top_left, ImVec2 bottom_right, ImU32 body,
              const char* label) {
  into->AddRectFilled(top_left, bottom_right, body);
  into->AddRect(top_left, bottom_right, kChatTabEdgeColor);
  into->AddText(ImVec2(top_left.x + kChatTabTextLeft, top_left.y + kChatTabTextTop),
                kChatTabTextColor, label);
}

// Le rayon du point de non-lu, en part de la hauteur de la languette, et son
// retrait depuis le coin haut droit.
constexpr float kUnreadDotShare = 0.12F;
constexpr float kUnreadDotInset = 2.0F;

// UN POINT DANS LE COIN HAUT DROIT : une ligne est arrivée depuis qu'on a
// regardé cet onglet. Dans la marge de la languette, il ne mord pas son nom.
void PaintUnreadDot(ImDrawList* into, ImVec2 top_left, ImVec2 bottom_right) {
  const float radius = (bottom_right.y - top_left.y) * kUnreadDotShare;
  into->AddCircleFilled(
      ImVec2(bottom_right.x - kUnreadDotInset - radius, top_left.y + kUnreadDotInset + radius),
      radius, kChatUnreadDotColor);
}

}  // namespace

namespace {

// Le bouton de droite de la bande : une loupe, peinte.
//
// ⚠ ON NE DESSINE QUE CE QUI AGIT. Le client en aligne plusieurs, dont certains
// ne font rien chez nous ; un bouton natif décoratif est un piège pour le
// joueur, qui le cliquera jusqu'à croire l'interface cassée.
void DrawSearchButton(ImVec2 at, float side, ImU32 tab_color, bool on, ChatStripAction* asked) {
  ImGui::SetCursorScreenPos(at);
  // ⚠ PAS « ##recherche » : le CHAMP de recherche porte déjà ce nom, dans la
  // même fenêtre, donc le même identifiant — ImGui l'a signalé en jeu par sa
  // fenêtre rouge, et l'un des deux devenait inutilisable.
  ImGui::InvisibleButton("##bouton_recherche", ImVec2(side, side));
  const bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    SkinTooltipFormat("%s", core::Tr("Rechercher / copier"));
  }
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 to(at.x + side, at.y + side);
  const ImU32 body = on ? Lighten(tab_color, kChatTabActiveLift)
                        : (hovered ? Lighten(tab_color, kChatTabHoverLift) : tab_color);
  into->AddRectFilled(at, to, body);
  into->AddRect(at, to, kChatTabEdgeColor);
  // La loupe : un cercle et son manche, calés sur le côté du bouton pour
  // suivre l'échelle sans qu'aucune longueur ne soit écrite en dur.
  const ImVec2 middle(at.x + side * 0.42F, at.y + side * 0.42F);
  const float radius = side * 0.22F;
  into->AddCircle(middle, radius, kChatTabTextColor, 0, 1.5F);
  into->AddLine(ImVec2(middle.x + radius * 0.7F, middle.y + radius * 0.7F),
                ImVec2(at.x + side * 0.78F, at.y + side * 0.78F), kChatTabTextColor, 1.5F);
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    asked->toggled_search = true;
  }
}

// LE BOUTON DES ÉMOTICONES, et la grille qu'il ouvre.
//
// ⚠ DANS LA BANDE, pas dans la barre de saisie. Celle-ci porte déjà trois
// choses — la destination, le destinataire, la phrase — et chacune lui prend
// de la largeur ; la bande, elle, a de la place à droite, et c'est déjà là que
// vit le bouton de recherche.
// L'image à montrer d'une émoticone, à cet instant. Elle tourne en BOUCLE : dans
// une grille, une animation jouée une fois ne se verrait qu'à l'ouverture.
[[nodiscard]] std::span<const ChatEmotionLayer> FrameAt(const ChatEmotion& emotion, double now_s) {
  if (emotion.frames.empty()) {
    return {};
  }
  if (emotion.frame_ms <= 0.0F) {
    return emotion.frames.front().layers;
  }
  const auto elapsed =
      static_cast<std::int64_t>(now_s * kMsPerSecond / static_cast<double>(emotion.frame_ms));
  const auto count = static_cast<std::int64_t>(emotion.frames.size());
  return emotion.frames[static_cast<std::size_t>(elapsed % count)].layers;
}

}  // namespace

// ⚠ `AddImageQuad` ET NON `AddImage` : un quad de sprite peut être TOURNÉ, et un
// rectangle aligné aux axes ne saurait pas le représenter.
void PaintEmotion(ImDrawList* into, const ChatEmotion& emotion, ImVec2 corner, float side,
                  double now_s) {
  const float wide = emotion.box_max.x - emotion.box_min.x;
  const float tall = emotion.box_max.y - emotion.box_min.y;
  if (into == nullptr || wide <= 0.0F || tall <= 0.0F) {
    return;
  }
  const float scale = std::min(side / wide, side / tall);
  const float middle_x = (emotion.box_min.x + emotion.box_max.x) * 0.5F;
  const float middle_y = (emotion.box_min.y + emotion.box_max.y) * 0.5F;
  const float origin_x = corner.x + side * 0.5F - middle_x * scale;
  const float origin_y = corner.y + side * 0.5F - middle_y * scale;
  for (const ChatEmotionLayer& layer : FrameAt(emotion, now_s)) {
    if (layer.id == 0) {
      continue;
    }
    std::array<ImVec2, 4> posed{};
    for (std::size_t point = 0; point < posed.size(); ++point) {
      posed[point] = ImVec2(origin_x + layer.corners[point].x * scale,
                            origin_y + layer.corners[point].y * scale);
    }
    into->AddImageQuad(layer.id, posed[0], posed[1], posed[2], posed[3], ImVec2(0.0F, 0.0F),
                       ImVec2(1.0F, 0.0F), ImVec2(1.0F, 1.0F), ImVec2(0.0F, 1.0F), layer.tint);
  }
}

namespace {

// La taille d'une case, et la largeur qu'une grille prend.
[[nodiscard]] float EmotionCellSide() { return ImGui::GetFontSize() * kEmotionCellRatio; }

[[nodiscard]] float GridWidth(float cell) {
  const ImGuiStyle& style = ImGui::GetStyle();
  return kEmotionsPerRow * (cell + style.ItemSpacing.x) + style.ScrollbarSize;
}

// L'infobulle d'une case : son nom, et le geste qui l'écrit quand elle en a un.
constexpr std::size_t kEmoteTipBytes = 160;
// Le voile d'une case qu'on ne peut pas écrire : l'opacité du fond qui la couvre.
constexpr float kGreyedCellVeil = 0.6F;
// L'opacité du liseré d'une case survolée, tracé à l'encre du texte du menu.
constexpr float kHoveredCellEdge = 0.8F;

// LE LISERÉ D'UNE CASE SURVOLÉE : l'encre du texte du sélecteur, qui suit le
// thème — sombre sur un menu clair, clair sur un sombre.
[[nodiscard]] ImU32 HoveredCellEdge() {
  return ImGui::GetColorU32(ImGuiCol_Text, kHoveredCellEdge);
}

void EmotionTooltip(const ChatEmotion& shown, ChatEmoteClick click) {
  if (shown.chat_token.empty()) {
    // ⚠ SANS NOM DE CHAT, ELLE NE S'ÉCRIT PAS, et l'infobulle le dit : le
    // Maj+clic ou le clic de la 1:1 ne feraient sinon rien sans explication.
    SkinTooltipFormat(":%.*s:\n%s", static_cast<int>(shown.name.size()), shown.name.data(),
                      core::Tr("Pas de nom :nom: : elle ne s'écrit pas dans la saisie."));
  } else if (click == ChatEmoteClick::kWrite) {
    SkinTooltipFormat(":%.*s:", static_cast<int>(shown.chat_token.size()), shown.chat_token.data());
  } else {
    // LE NOM QUI S'ÉCRIRA, et non celui du catalogue : c'est lui que Discord et
    // les autres clients liront.
    const int token_bytes = static_cast<int>(shown.chat_token.size());
    std::array<char, kEmoteTipBytes> gesture{};
    std::snprintf(gesture.data(), gesture.size(),
                  core::Tr("Maj+clic : écrire :%.*s: dans la saisie"), token_bytes,
                  shown.chat_token.data());
    SkinTooltipFormat(":%.*s:\n%s", token_bytes, shown.chat_token.data(), gesture.data());
  }
}

// L'ONGLET DES ÉMOTICONES.
//
// `kSend` : un clic ENVOIE, et referme. ⚠ MAJ + CLIC L'ÉCRIT AU LIEU DE
// L'ENVOYER — son nom `:nom:`, que Discord et les autres clients savent montrer
// — et la palette RESTE OUVERTE, comme pour un emoji : c'est du texte qu'on pose
// dans une phrase. Sans nom, le Maj+clic ne fait rien : envoyer serait le
// contraire de ce qu'il demande.
//
// `kWrite` : tout clic ÉCRIT, et celles qui n'ont pas de nom sont grisées.
void DrawEmotionTab(std::span<const ChatEmotion> emotions, ChatEmoteClick click,
                    ChatStripAction* asked, bool* picked) {
  const float cell = EmotionCellSide();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float grid_width = GridWidth(cell);
  ImGui::BeginChild("##defile_emotes",
                    ImVec2(grid_width, kEmotionRowsShown * (cell + style.ItemSpacing.y)));
  ImDrawList* grid = ImGui::GetWindowDrawList();
  const double now_s = ImGui::GetTime();
  for (int rank = 0; rank < static_cast<int>(emotions.size()); ++rank) {
    const ChatEmotion& shown = emotions[static_cast<std::size_t>(rank)];
    if (rank % kEmotionsPerRow != 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(shown.id);
    const bool writable = !shown.chat_token.empty();
    const bool greyed = click == ChatEmoteClick::kWrite && !writable;
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    ImGui::BeginDisabled(greyed);
    const bool chosen = ImGui::InvisibleButton("##emote", ImVec2(cell, cell));
    ImGui::EndDisabled();
    const ImVec2 far(corner.x + cell, corner.y + cell);
    PaintEmotion(grid, shown, corner, cell, now_s);
    if (greyed) {
      grid->AddRectFilled(corner, far, ImGui::GetColorU32(ImGuiCol_PopupBg, kGreyedCellVeil));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      grid->AddRect(corner, far, HoveredCellEdge());
      EmotionTooltip(shown, click);
    }
    const bool writes = click == ChatEmoteClick::kWrite || ImGui::GetIO().KeyShift;
    if (chosen && writes) {
      if (writable) {
        asked->chose_emote_token = shown.chat_token;
        *picked = true;
      }
    } else if (chosen) {
      // 🔴 LE NUMÉRO, PAS LE RANG. La grille saute ce que le jeu se
      // réserve ; prendre la position de la case ferait envoyer la voisine.
      asked->chose_emotion = shown.id;
      ImGui::CloseCurrentPopup();
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}

// L'ONGLET DES EMOJI : un clic INSÈRE, et la palette RESTE OUVERTE.
//
// 🔴 LA DIFFÉRENCE DE GESTE TIENT À LA NATURE DE CE QU'ON POSE. Une
// émoticone est une IMAGE que le serveur diffuse seule, par son numéro ; un emoji
// est du TEXTE, qui s'affiche au milieu d'une phrase. On en pioche volontiers
// deux ou trois d'affilée.
void DrawEmojiTab(std::span<const ChatEmojiGroup> emojis, ChatStripAction* asked, bool* picked) {
  const float cell = EmotionCellSide();
  const ImGuiStyle& style = ImGui::GetStyle();
  ImGui::BeginChild("##defile_emoji",
                    ImVec2(GridWidth(cell), kEmotionRowsShown * (cell + style.ItemSpacing.y)));
  // ⚠ LA TAILLE NORMALE EST RELEVÉE AVANT D'AGRANDIR : les intertitres la
  // reprennent, et `PushFont(nullptr, 0.0F)` voudrait dire « garde la COURANTE »,
  // donc la grande — les titres seraient géants.
  const float title_size = ImGui::GetFontSize();
  // La police du chat est calibrée pour du texte ; à cette taille un emoji est un
  // timbre-poste. On agrandit pour la grille seulement.
  ImGui::PushFont(nullptr, cell * kEmojiOfCell);
  ImDrawList* grid = ImGui::GetWindowDrawList();
  for (int group = 0; group < static_cast<int>(emojis.size()); ++group) {
    const ChatEmojiGroup& shown = emojis[static_cast<std::size_t>(group)];
    ImGui::PushFont(nullptr, title_size);
    ImGui::SeparatorText(std::string(shown.label).c_str());
    ImGui::PopFont();
    ImGui::PushID(group);
    for (int rank = 0; rank < static_cast<int>(shown.items.size()); ++rank) {
      if (rank % kEmotionsPerRow != 0) {
        ImGui::SameLine();
      }
      ImGui::PushID(rank);
      const std::string_view item = shown.items[static_cast<std::size_t>(rank)];
      const ImVec2 corner = ImGui::GetCursorScreenPos();
      const bool chosen = ImGui::InvisibleButton("##emoji", ImVec2(cell, cell));
      const ImVec2 far(corner.x + cell, corner.y + cell);
      // Centré dans sa case : les glyphes d'emoji n'ont pas tous la même chasse,
      // et alignés à gauche ils donnent une grille qui tremble.
      const ImVec2 size = ImGui::CalcTextSize(item.data(), item.data() + item.size());
      // ⚠ À L'ENCRE DU TEXTE DU MENU : un glyphe EN COULEUR ignore la teinte,
      // mais si la police d'emoji manque au système, le repli est un losange
      // monochrome, qui doit se lire sur le fond du menu, clair ou sombre.
      grid->AddText(ImVec2(corner.x + (cell - size.x) * 0.5F, corner.y + (cell - size.y) * 0.5F),
                    ImGui::GetColorU32(ImGuiCol_Text), item.data(), item.data() + item.size());
      if (ImGui::IsItemHovered()) {
        grid->AddRect(corner, far, HoveredCellEdge());
      }
      if (chosen) {
        asked->chose_emoji = item;
        *picked = true;
      }
      ImGui::PopID();
    }
    ImGui::PopID();
  }
  ImGui::PopFont();
  ImGui::EndChild();
}

}  // namespace

void DrawChatEmotePicker(ChatStripState& state, ImVec2 at, float side, ImU32 tab_color,
                         std::span<const ChatEmotion> emotions,
                         std::span<const ChatEmojiGroup> emojis, ChatStripAction* asked,
                         ChatEmoteClick click) {
  ImGui::SetCursorScreenPos(at);
  ImGui::InvisibleButton("##bouton_emotes", ImVec2(side, side));
  const bool hovered = ImGui::IsItemHovered();
  // ⚠ GRISÉ DANS UN BLOC DÉSACTIVÉ : le bouton est peint à la main, et ses
  // couleurs ne suivent l'opacité d'ImGui — abaissée par `BeginDisabled` — que
  // si on les y passe.
  const auto faded = [](ImU32 color) { return ImGui::GetColorU32(color); };
  if (hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    SkinTooltipFormat("%s", core::Tr("Émoticones"));
  }
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 to(at.x + side, at.y + side);
  const ImU32 ink = faded(kChatTabTextColor);
  into->AddRectFilled(at, to, faded(hovered ? Lighten(tab_color, kChatTabHoverLift) : tab_color));
  into->AddRect(at, to, faded(kChatTabEdgeColor));
  // Un visage : deux yeux et une bouche, calés sur le côté du bouton pour
  // suivre l'échelle sans qu'aucune longueur ne soit écrite en dur.
  const ImVec2 middle(at.x + side * 0.5F, at.y + side * 0.5F);
  into->AddCircle(middle, side * 0.32F, ink, 0, 1.5F);
  into->AddCircleFilled(ImVec2(middle.x - side * 0.12F, middle.y - side * 0.08F), side * 0.045F,
                        ink);
  into->AddCircleFilled(ImVec2(middle.x + side * 0.12F, middle.y - side * 0.08F), side * 0.045F,
                        ink);
  into->AddLine(ImVec2(middle.x - side * 0.12F, middle.y + side * 0.12F),
                ImVec2(middle.x + side * 0.12F, middle.y + side * 0.12F), ink, 1.5F);
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    ImGui::OpenPopup(kEmotionGrid);
  }

  // 🔴 LA PALETTE VIENT DE SE REFERMER APRÈS UNE PIOCHE : c'est SEULEMENT
  // ici qu'on peut rendre le clavier. Le faire au clic refermerait la palette
  // sous les doigts du joueur — ImGui ferme un popup dès que le focus s'en va — et
  // l'on ne pourrait plus en poser qu'un seul à la fois.
  if (!ImGui::IsPopupOpen(kEmotionGrid) && state.picked_emoji) {
    state.picked_emoji = false;
    asked->give_back_keyboard = true;
  }
  // ⚠ LE SÉLECTEUR PREND LE FOND ET LE TEXTE DU THÈME, par l'habillage prêté
  // (voir `LendSkinArt`) ou celui de la fenêtre 1:1 : jamais le texte clair que
  // le cadre de la conversation pose pour son propre fond.
  if (BeginSkinPopup(kEmotionGrid)) {
    const ImVec2 header = ImGui::GetCursorScreenPos();
    const float grid_width = GridWidth(EmotionCellSide());
    // ⚠ DEUX NATURES, DEUX ONGLETS, et deux gestes différents au clic : ce qui
    // arrive au bout du fil n'est pas de même nature.
    if (ImGui::BeginTabBar("##onglets_du_selecteur")) {
      if (ImGui::BeginTabItem(core::Tr("Émoticones"))) {
        DrawEmotionTab(emotions, click, asked, &state.picked_emoji);
        ImGui::EndTabItem();
      }
      // L'onglet ne s'ouvre pas sur rien : sans police d'emoji dans l'atlas, la
      // grille ne rendrait que des losanges, et `app` ne passe alors aucun groupe.
      if (!emojis.empty() && ImGui::BeginTabItem(core::Tr("Emoji"))) {
        DrawEmojiTab(emojis, asked, &state.picked_emoji);
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    // 🔴 UN POPUP N'A PAS DE BARRE DE TITRE, donc pas de bouton système, et
    // se referme d'ordinaire par un clic au dehors ou par le choix d'une
    // émoticone. Le geste existe, mais RIEN NE LE MONTRE — et une palette qu'on
    // ne sait pas refermer est une palette qui reste dans les jambes.
    //
    // ⚠ DESSINÉE EN DERNIER, dans le vide que la barre d'onglets laisse à sa
    // droite : elle doit passer par-dessus.
    const float cross = ImGui::GetFontSize();
    if (ImGui::CloseButton(ImGui::GetID("##fermer_emotes"),
                           ImVec2(header.x + grid_width - cross, header.y))) {
      ImGui::CloseCurrentPopup();
    }
    EndSkinPopup();
  }
}

std::optional<ChatStripSpot> ChatStripUnder(const ChatDragBoard& board, ImVec2 at) {
  for (const ChatDragBoard::Strip& strip : board.strips) {
    if (at.x < strip.min.x || at.x > strip.max.x || at.y < strip.min.y || at.y > strip.max.y) {
      continue;
    }
    // Elle y prend la place que son milieu désigne ; au-delà du dernier
    // milieu, tout au bout.
    ChatStripSpot spot{
        .group = strip.group, .rank = strip.count, .top = strip.min.y, .bottom = strip.max.y};
    for (int rank = 0; rank < strip.count; ++rank) {
      if (at.x < strip.middles[static_cast<std::size_t>(rank)]) {
        spot.rank = rank;
        break;
      }
    }
    // Le repère se pose entre deux languettes : à mi-chemin des milieux qui
    // l'encadrent, ou au bord de la bande.
    const auto middle = [&strip](int rank) {
      return strip.middles[static_cast<std::size_t>(rank)];
    };
    if (strip.count == 0) {
      spot.marker_x = strip.min.x;
    } else if (spot.rank == 0) {
      spot.marker_x =
          std::max(strip.min.x, 2.0F * middle(0) - middle(std::min(1, strip.count - 1)));
    } else if (spot.rank >= strip.count) {
      spot.marker_x = middle(strip.count - 1) +
                      (middle(strip.count - 1) - middle(std::max(0, strip.count - 2))) * 0.5F;
    } else {
      spot.marker_x = (middle(spot.rank - 1) + middle(spot.rank)) * 0.5F;
    }
    return spot;
  }
  return std::nullopt;
}

ChatDrop DecideChatDrop(ChatDragBoard& board) {
  ChatDrop drop;
  if (!board.released) {
    return drop;
  }
  board.released = false;
  drop.from_group = board.from_group;
  drop.from_tab = board.from_tab;
  drop.at = board.at;

  if (const std::optional<ChatStripSpot> spot = ChatStripUnder(board, board.at); spot.has_value()) {
    drop.asked = true;
    drop.to_group = spot->group;
    drop.to_rank = spot->rank;
    return drop;
  }

  // Hors de toute bande. ⚠ DANS SA PROPRE FENÊTRE, LE GESTE EST ANNULÉ : un
  // glissement de six pixels vers le fil sortirait sinon l'onglet dans une
  // fenêtre que personne n'a demandée. Le menu de l'onglet, lui, le fait sans
  // ambiguïté.
  for (const ChatDragBoard::Strip& strip : board.strips) {
    if (strip.group != board.from_group) {
      continue;
    }
    if (board.at.x >= strip.window_min.x && board.at.x <= strip.window_max.x &&
        board.at.y >= strip.window_min.y && board.at.y <= strip.window_max.y) {
      return drop;  // lâchée chez elle : rien
    }
  }
  drop.asked = true;
  drop.to_new_window = true;
  return drop;
}

float DrawChatStrip(ChatStripState& state, std::span<const ChatStripTab> tabs, int current,
                    ImU32 tab_color, bool searching, std::uint32_t group,
                    std::span<const ChatEmotion> emotions, std::span<const ChatEmojiGroup> emojis,
                    ChatDragBoard& board, ChatStripAction* asked) {
  const float height = ImGui::GetFontSize() + kChatTabHeightPadding;
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  // ⚠ MESURÉE AVANT DE DÉPLACER LE CURSEUR : après la première languette,
  // l'espace « restant » ne serait plus celui de la bande mais celui d'après.
  const float strip_width = ImGui::GetContentRegionAvail().x;
  const ImU32 active_color = Lighten(tab_color, kChatTabActiveLift);
  const ImU32 hover_color = Lighten(tab_color, kChatTabHoverLift);

  // Les bords de chaque languette DESSINÉE, relevés au passage : c'est ce qui
  // dit, au moment du lâcher, entre quels deux voisins le curseur se trouve. Un
  // tableau fixe plutôt qu'un vecteur — c'est une mesure d'image, pas un état.
  std::array<float, kMostTabs> slot_left{};
  std::array<float, kMostTabs> slot_right{};
  int slots = 0;

  float x = 0.0F;
  for (int rank = 0; rank < static_cast<int>(tabs.size()); ++rank) {
    const TabLabel label = LabelOf(tabs[static_cast<std::size_t>(rank)].name);
    const float width = ImGui::CalcTextSize(label.data()).x + kChatTabWidthPadding;

    ImGui::SetCursorScreenPos(ImVec2(origin.x + x, origin.y));
    ImGui::PushID(rank);
    ImGui::InvisibleButton("##languette", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      asked->chose = rank;
    }
    // Le clic droit règle CET onglet : le filtre appartient au canal, pas à la
    // fenêtre, et un menu ouvert sous le curseur configure ce qu'il désigne.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      asked->configure = rank;
    }
    if (ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left, kChatTabDragThreshold)) {
      state.dragging = rank;
    }
    if (hovered && state.dragging < 0) {
      // Un menu contextuel sans indice ne se découvre pas.
      std::array<char, kTooltipHeadRoom> head{};
      std::snprintf(head.data(), head.size(), core::Tr("Clic droit : les options de « %s »"),
                    label.data());
      TwoLineTooltip(head.data(), core::Tr("Glisser : réordonner, ou sortir dans une fenêtre"));
    }
    ImGui::PopID();

    const ImVec2 top_left(origin.x + x, origin.y);
    const ImVec2 bottom_right(top_left.x + width, top_left.y + height);
    if (slots < kMostTabs) {
      slot_left[static_cast<std::size_t>(slots)] = top_left.x;
      slot_right[static_cast<std::size_t>(slots)] = bottom_right.x;
      ++slots;
    }
    const ImU32 body = rank == current ? active_color : (hovered ? hover_color : tab_color);
    PaintTab(into, top_left, bottom_right, body, label.data());
    if (tabs[static_cast<std::size_t>(rank)].unread) {
      PaintUnreadDot(into, top_left, bottom_right);
    }
    x += width + kChatTabGap;
  }

  // ── CE QUE CETTE BANDE OCCUPE, pour qui déposera ailleurs ─────────────
  // Publié À CHAQUE IMAGE, qu'un geste soit en cours ou non : au moment où une
  // languette tombe ici, c'est une AUTRE bande qui tient le geste, et elle ne
  // saura où la poser que par ce qu'on laisse ici.
  {
    ChatDragBoard::Strip mine;
    mine.group = group;
    mine.min = origin;
    mine.max = ImVec2(origin.x + strip_width, origin.y + height);
    mine.window_min = ImGui::GetWindowPos();
    mine.window_max = ImVec2(mine.window_min.x + ImGui::GetWindowSize().x,
                             mine.window_min.y + ImGui::GetWindowSize().y);
    mine.count = slots;
    for (int k = 0; k < slots; ++k) {
      const auto at = static_cast<std::size_t>(k);
      mine.middles[at] = (slot_left[at] + slot_right[at]) * 0.5F;
    }
    board.strips.push_back(mine);
  }

  // ── Où la languette tombera-t-elle, si elle tombe ici ? ────────────────────
  // Le rang visé se lit au MILIEU de chaque languette, pas à son bord : c'est le
  // point de bascule naturel — dépasser la moitié d'un voisin, c'est passer
  // devant lui. Comparer aux bords laisserait entre deux positions une zone morte
  // large d'une languette, où le trait ne bougerait pas.
  if (state.dragging >= 0) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool over_strip = mouse.x >= origin.x && mouse.x <= origin.x + strip_width &&
                            mouse.y >= origin.y && mouse.y <= origin.y + height;
    int slot = -1;
    if (over_strip) {
      slot = slots;  // au-delà du dernier milieu : tout au bout
      for (int k = 0; k < slots; ++k) {
        const std::size_t at = static_cast<std::size_t>(k);
        if (mouse.x < (slot_left[at] + slot_right[at]) * 0.5F) {
          slot = k;
          break;
        }
      }
      // Le trait d'insertion, entre les deux voisins. Sans lui le joueur lâche à
      // l'aveugle et découvre l'ordre obtenu après coup.
      const float caret_x =
          slot < slots
              ? slot_left[static_cast<std::size_t>(slot)] - 1.0F
              : (slots > 0 ? slot_right[static_cast<std::size_t>(slots - 1)] + 1.0F : origin.x);
      into->AddLine(ImVec2(caret_x, origin.y), ImVec2(caret_x, origin.y + height),
                    kChatTabTextColor, 2.0F);
    }

    // Le fantôme suit le curseur sur le calque de PREMIER PLAN : ce qu'on survole
    // passerait sinon par-dessus.
    if (state.dragging < static_cast<int>(tabs.size())) {
      const TabLabel label = LabelOf(tabs[static_cast<std::size_t>(state.dragging)].name);
      const float width = ImGui::CalcTextSize(label.data()).x + kChatTabWidthPadding;
      const ImVec2 ghost(mouse.x - kChatTabGhostLeft, mouse.y - kChatTabGhostTop);
      PaintTab(ImGui::GetForegroundDrawList(), ghost, ImVec2(ghost.x + width, ghost.y + height),
               active_color, label.data());
    }

    // Le geste en cours, pour qui tranchera : d'où il part.
    board.from_group = group;
    board.from_tab = state.dragging;
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      if (slot >= 0) {
        // Dans SA bande : un simple réordonnancement, tranché sur place. Personne
        // d'autre n'a besoin de le savoir.
        asked->moved_from = state.dragging;
        asked->moved_to = slot;
      } else {
        // 🔴 AILLEURS : ON NE PEUT PAS TRANCHER ICI. La bande où elle tombe
        // est dessinée ailleurs dans l'image, parfois après nous ; on ne sait
        // laquelle est sous le curseur qu'une fois toutes dessinées.
        board.released = true;
        board.at = mouse;
      }
      state.dragging = -1;
    }
  }
  // Filet de sécurité : un geste dont la fenêtre a cessé d'être dessinée
  // laisserait sinon un glissement fantôme, actif jusqu'au prochain clic. APRÈS
  // le traitement du lâcher, qui se produit précisément sur l'image où le bouton
  // n'est plus enfoncé.
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    state.dragging = -1;
  }

  // Aligné à DROITE de la bande, et posé après les languettes : à survol égal,
  // c'est le dernier élément soumis qui gagne, donc il passe devant elles si
  // jamais elles allaient jusque-là.
  const float side = height * kChatStripButtonShare;
  const float top = origin.y + (height - side) * 0.5F;
  DrawSearchButton(ImVec2(origin.x + strip_width - side, top), side, tab_color, searching, asked);
  // Les émoticones à sa gauche. Vide, rien ne paraît : sans le sprite, le bouton
  // s'ouvrirait sur rien.
  if (!emotions.empty()) {
    DrawChatEmotePicker(state, ImVec2(origin.x + strip_width - side * 2.0F - kChatTabGap, top),
                        side, tab_color, emotions, emojis, asked);
  }

  ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height + kChatTabGap));
  return height;
}

}  // namespace ro::ui
