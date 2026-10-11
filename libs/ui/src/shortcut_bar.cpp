#include "ro/ui/shortcut_bar.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include <imgui_internal.h>

#include "ro/core/decimal_text.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

namespace ro::ui {

bool ItemDroppedOnNothing(ItemDrag* carried) {
  const ImGuiPayload* const payload = ImGui::GetDragDropPayload();
  if (carried == nullptr || payload == nullptr || !payload->IsDataType(kItemDragPayload)) {
    return false;
  }
  // ⚠ LES TROIS CONDITIONS SE LISENT ENSEMBLE : le bouton vient d'être relâché,
  // AUCUNE cible n'a pris la charge, et le curseur n'est sur aucune fenêtre.
  // Sans la dernière, lâcher sur une fenêtre qui n'accepte pas les objets — le
  // journal, les options — jetterait l'objet au sol.
  if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left) || payload->IsDelivery() ||
      ImGui::GetCurrentContext()->HoveredWindow != nullptr) {
    return false;
  }
  if (payload->DataSize != static_cast<int>(sizeof(ItemDrag))) {
    return false;
  }
  *carried = *static_cast<const ItemDrag*>(payload->Data);
  return true;
}
namespace {

constexpr float kSlotRounding = 3.0F;

// De combien une barre se pose plus haut que la précédente à la PREMIÈRE partie.
//
// ⚠ ASSEZ POUR QU'ELLES NE SE RECOUVRENT PAS quelle que soit la taille des
// cases : une barre montrée pour la première fois exactement sous une autre
// passerait pour un défaut d'affichage, et le joueur la chercherait ailleurs.
// Ensuite, c'est ImGui qui retient la place de chacune.
constexpr float kFirstStackStep = 40.0F;

constexpr ImU32 kHoverVeil = IM_COL32(255, 255, 255, 36);
// La case qu'un glisser survole : elle s'annonce, sinon on lâche à l'aveugle.
constexpr ImU32 kDropOutline = IM_COL32(255, 205, 105, 230);
constexpr float kDropThickness = 2.0F;
// La case éclairée par le mode clavier du grimoire : un voile et un cadre
// bleus, pour ne pas se confondre avec la cible d'un glisser.
constexpr ImU32 kHighlightVeil = IM_COL32(110, 180, 255, 70);
constexpr ImU32 kHighlightOutline = IM_COL32(120, 190, 255, 240);
// L'aperçu d'une case qu'on s'apprête à VIDER, hors des barres.
constexpr ImU32 kRemoveVeil = IM_COL32(190, 30, 30, 90);
constexpr ImU32 kRemoveCross = IM_COL32(240, 70, 70, 235);
constexpr float kRemoveThickness = 2.0F;
// L'icône d'une compétence que le serveur ne donne plus.
constexpr ImU32 kUnknownTint = IM_COL32(105, 105, 115, 150);
// Le voile du temps d'attente : il MONTE du bas, comme au client.
constexpr ImU32 kCooldownVeil = IM_COL32(10, 10, 16, 165);
constexpr float kLabelMargin = 2.0F;
// En deçà, un libellé rétréci n'est plus qu'une tache : mieux vaut qu'il déborde
// un peu que de le réduire jusqu'à l'illisible.
constexpr float kSmallestLabelSize = 7.0F;
// La part de la case que prend le décompte.
constexpr float kCountdownShare = 0.42F;
constexpr std::uint32_t kMillisecondsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// UN TEXTE DE CASE, avec l'ombre que la barre a choisie.
//
// ⚠ UN SEUL POINT DE PASSAGE pour les quatre textes d'une case — touche, rang,
// quantité, décompte. Le choix d'ombre répété sur chacun serait quatre endroits
// où l'un d'eux resterait en arrière au prochain réglage ajouté.
void PaintSlotText(ImDrawList* into, const ShortcutBarLayout& look, float size, ImVec2 at,
                   ImU32 color, const char* text) {
  const float scale = Scale();
  switch (static_cast<SlotTextShadow>(look.shadow_kind)) {
    case SlotTextShadow::kNone:
      if (size > 0.0F) {
        into->AddText(ImGui::GetFont(), size, at, color, text);
      } else {
        into->AddText(at, color, text);
      }
      break;
    case SlotTextShadow::kHalo:
      AddTextHalo(into, size, at, color, text, look.shadow_color);
      break;
    case SlotTextShadow::kDrop: {
      // ⚠ LE DÉCALAGE SUIT L'ÉCHELLE DE L'INTERFACE, contrairement au cerne : une
      // ombre est une DISTANCE que le joueur a choisie, là où un cerne n'est
      // qu'un contour qui doit rester d'un pixel pour ne pas fermer les lettres.
      const ImVec2 away(at.x + look.shadow_offset * scale, at.y + look.shadow_offset * scale);
      if (size > 0.0F) {
        into->AddText(ImGui::GetFont(), size, away, look.shadow_color, text);
        into->AddText(ImGui::GetFont(), size, at, color, text);
      } else {
        into->AddText(away, look.shadow_color, text);
        into->AddText(at, color, text);
      }
      break;
    }
  }
}

// Ce que la barre garde sur le style. Elle est TRANSLUCIDE et sans marge : neuf
// cases doivent occuper exactement la place de neuf cases.
constexpr auto kBarColors = std::to_array<ImGuiCol>({ImGuiCol_WindowBg, ImGuiCol_Border});
constexpr int kBarStyleVars = 3;

void PaintSlot(ImDrawList* into, const ShortcutBarLayout& look, const ShortcutSlotView& slot,
               ImVec2 top_left, float side, bool hot, bool dropping, bool show_key) {
  const float scale = Scale();
  const ImVec2 bottom_right(top_left.x + side, top_left.y + side);
  // LE FOND DIT CE QUE LA CASE PORTE. ⚠ TROIS COULEURS ET NON UNE : huit barres
  // servent à séparer les familles, et c'est la teinte qui rend la séparation
  // lisible du coin de l'œil. Les deux fonds de contenu valent celui du vide par
  // défaut — une icône les recouvre, ils n'existent que pour qui veut teinter.
  const ImU32 background = slot.item_id != 0    ? look.item_color
                           : slot.skill_id != 0 ? look.skill_color
                                                : look.empty_color;
  into->AddRectFilled(top_left, bottom_right, background, kSlotRounding * scale);

  if (slot.icon.usable()) {
    into->AddImage(slot.icon.id, top_left, bottom_right, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                   slot.known ? IM_COL32_WHITE : kUnknownTint);
  }
  // ⚠ LE VOILE MONTE DU BAS. Une case qui s'assombrit d'un coup ne dit pas
  // combien de temps il reste ; celle-ci se vide comme un sablier, et c'est ce
  // qu'on lit du coin de l'œil en combattant.
  if (slot.cooldown > 0.0F) {
    const float top = bottom_right.y - side * slot.cooldown;
    into->AddRectFilled(ImVec2(top_left.x, top), bottom_right, kCooldownVeil);
  }
  if (hot) {
    into->AddRectFilled(top_left, bottom_right, kHoverVeil, kSlotRounding * scale);
  }
  if (slot.highlighted) {
    into->AddRectFilled(top_left, bottom_right, kHighlightVeil, kSlotRounding * scale);
  }
  // ⚠ ÉPAISSEUR ZÉRO = PAS DE CADRE, et c'est une valeur que des joueurs
  // choisiront : des cases jointives sans bordure est une disposition qui se
  // défend. La cible d'un glisser garde la sienne — elle doit s'annoncer même
  // sur une barre sans cadre, sinon on lâche à l'aveugle. La case éclairée
  // aussi, pour la même raison.
  const bool marked = dropping || slot.highlighted;
  const float border = marked ? kDropThickness * scale : look.border_thickness * scale;
  if (border > 0.0F) {
    const ImU32 edge = dropping           ? kDropOutline
                       : slot.highlighted ? kHighlightOutline
                                          : look.border_color;
    into->AddRect(top_left, bottom_right, edge, kSlotRounding * scale, 0, border);
  }

  // COMBIEN IL EN RESTE, en bas à droite — la place où la grille du sac écrit
  // déjà ses quantités. Rien pour une compétence, ni pour un exemplaire unique.
  const float margin = kLabelMargin * scale;
  ImFont* const font = ImGui::GetFont();
  const float count_size =
      std::max(ImGui::GetFontSize() * look.count_scale, kSmallestLabelSize * scale);
  if (slot.item_id != 0 && slot.amount > 1) {
    std::array<char, 16> count{};
    std::snprintf(count.data(), count.size(), "%d", slot.amount);
    const ImVec2 size = font->CalcTextSizeA(count_size, FLT_MAX, 0.0F, count.data());
    PaintSlotText(into, look, count_size,
                  ImVec2(bottom_right.x - margin - size.x, bottom_right.y - margin - size.y),
                  look.count_color, count.data());
  }
  if (show_key && !slot.key.empty()) {
    const std::array<char, 32> text = [&slot] {
      std::array<char, 32> copy{};
      std::snprintf(copy.data(), copy.size(), "%.*s", static_cast<int>(slot.key.size()),
                    slot.key.data());
      return copy;
    }();
    // 🔴 L'ÉTIQUETTE RÉTRÉCIT PLUTÔT QUE DE DÉBORDER, et il a fallu les huit
    // barres pour que cela compte : sept d'entre elles n'ont de touches qu'avec
    // un modificateur, donc « Ctrl+F1 » et « Ctrl+Maj+F1 » sont la règle, pas
    // l'exception. Tronquer donnerait « Ctrl+ », qui désigne tout et rien.
    // ⚠ LE RÉTRÉCISSEMENT S'APPLIQUE APRÈS le réglage du joueur : c'est un
    // FILET, pas une taille. Qui choisit deux fois la taille d'étiquette sur une
    // petite case obtient ce qui tient, et non « Ctrl+ ».
    float size = std::max(ImGui::GetFontSize() * look.key_scale, kSmallestLabelSize * scale);
    const float room = side - margin * 2.0F;
    const float wide = font->CalcTextSizeA(size, FLT_MAX, 0.0F, text.data()).x;
    if (wide > room && wide > 0.0F) {
      size = std::max(size * room / wide, kSmallestLabelSize * scale);
    }
    PaintSlotText(into, look, size, ImVec2(top_left.x + margin, top_left.y + margin),
                  look.key_color, text.data());
  }
  if (slot.level > 0) {
    std::array<char, ro::core::kDecimalTextBytes<int>> rank{};
    std::snprintf(rank.data(), rank.size(), "%d", slot.level);
    const ImVec2 size = font->CalcTextSizeA(count_size, FLT_MAX, 0.0F, rank.data());
    PaintSlotText(into, look, count_size,
                  ImVec2(bottom_right.x - size.x - margin, bottom_right.y - size.y - margin),
                  look.count_color, rank.data());
  }
  // LE DÉCOMPTE, PAR-DESSUS LE VOILE. En deçà de la minute, la seconde suffit ;
  // au-delà, on écrit m:ss — « 143 » ne se lit pas comme « 2:23 ».
  if (slot.cooldown_ms > 0) {
    std::array<char, 16> left{};
    if (slot.cooldown_ms >= kSecondsPerMinute * kMillisecondsPerSecond) {
      std::snprintf(left.data(), left.size(), "%u:%02u",
                    slot.cooldown_ms / (kSecondsPerMinute * kMillisecondsPerSecond),
                    (slot.cooldown_ms / kMillisecondsPerSecond) % kSecondsPerMinute);
    } else {
      // ⚠ ARRONDI AU-DESSUS : une attente qui affiche « 0 » pendant une seconde
      // entière se lit « c'est prêt », et le clic part pour rien.
      std::snprintf(left.data(), left.size(), "%u",
                    (slot.cooldown_ms + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond);
    }
    // La taille suit la CASE et non la police de l'interface : un décompte
    // gravé à douze points disparaît sur une case de soixante-quatre. Le réglage
    // du joueur la multiplie, comme pour les autres textes.
    const float size =
        std::max(side * kCountdownShare * look.count_scale, kSmallestLabelSize * scale);
    const ImVec2 room = font->CalcTextSizeA(size, FLT_MAX, 0.0F, left.data());
    PaintSlotText(into, look, size,
                  ImVec2(top_left.x + (side - room.x) * 0.5F, top_left.y + (side - room.y) * 0.5F),
                  look.countdown_color, left.data());
  }
}

}  // namespace

ShortcutBarGrid GridOfBar(const ShortcutBarLayout& look) {
  ShortcutBarGrid grid;
  const auto most = static_cast<int>(kShortcutSlotsPerBar);
  grid.shown = static_cast<std::size_t>(std::clamp(look.shown_slots, 1, most));
  grid.columns = static_cast<std::size_t>(std::clamp(look.columns, 1, most));
  // ⚠ LA DERNIÈRE LIGNE PEUT ÊTRE INCOMPLÈTE : sept cases sur quatre colonnes
  // font DEUX lignes, pas une et trois quarts.
  grid.rows = (grid.shown + grid.columns - 1) / grid.columns;
  return grid;
}

std::optional<std::size_t> ShortcutSlotUnder(const ShortcutBarGrid& grid, float local_x,
                                             float local_y, float side, float gap) {
  const float step = side + gap;
  if (step <= 0.0F || local_x < 0.0F || local_y < 0.0F) {
    return std::nullopt;
  }
  const auto column = static_cast<std::size_t>(local_x / step);
  const auto row = static_cast<std::size_t>(local_y / step);
  // ⚠ L'ÉCART ENTRE DEUX CASES N'EST PAS UNE CASE : sans ce reste, une barre
  // espacée répondrait sur toute sa surface, gouttières comprises — et le clic
  // se perdrait dans le vide entre deux icônes.
  if (local_x - static_cast<float>(column) * step >= side ||
      local_y - static_cast<float>(row) * step >= side) {
    return std::nullopt;
  }
  // ⚠ LA COLONNE SE VÉRIFIE À PART DU RANG : sans elle, le point posé juste à
  // droite de la dernière colonne retomberait sur la première case de la ligne
  // SUIVANTE, qui est une vraie case — et l'erreur passerait inaperçue.
  if (column >= grid.columns) {
    return std::nullopt;
  }
  const std::size_t rank = row * grid.columns + column;
  if (rank >= grid.shown) {
    return std::nullopt;
  }
  return rank;
}

namespace {

// LE CURSEUR ÉTAIT-IL SUR UNE BARRE À L'IMAGE PRÉCÉDENTE ?
//
// 🔴 C'EST CE QUI DÉCIDE DE LA CROIX ROUGE, et il faut bien un souvenir : le
// verdict exact ne se connaît qu'une fois les huit barres dessinées, alors que
// l'aperçu se peint pendant la barre SOURCE, donc avant. Un avertissement en
// retard d'une image est invisible ; le même verdict pris trop tôt serait faux.
//
// ⚠ VRAI AU REPOS, et c'est l'état neutre : entre deux glissers, rien ne doit se
// barrer de rouge sur la première image.
bool g_cursor_was_over_a_bar = true;

// La fenêtre de la dernière barre dessinée. ⚠ RENDUE PAR UN CÔTÉ PLUTÔT QUE PAR
// UN RETOUR : `DrawOneBar` sort par deux chemins, dont le `Begin` qui refuse.
const ImGuiWindow* g_last_bar_window = nullptr;

// LES FENÊTRES DES HUIT BARRES, une identité chacune — sinon les huit partagent
// une place à l'écran. Les objets sont gardés d'une image à l'autre : leur
// identité ne change jamais, et le titre se refait seul au changement de langue.
//
// 🔴 LA PREMIÈRE GARDE L'IDENTITÉ NUE, SANS SON NUMÉRO, et l'incohérence est
// volontaire : c'est celle de la barre unique d'avant. Une identité ne change
// JAMAIS — c'est la règle de `window_name.h` —, et la renommer « shortcut_bar1 »
// ferait perdre à chaque joueur la place qu'il avait donnée à sa barre.
[[nodiscard]] std::array<WindowName, kShortcutBarCount>& BarNames() {
  static std::array<WindowName, kShortcutBarCount> names = [] {
    return std::array<WindowName, kShortcutBarCount>{
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId)),
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId) + "2"),
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId) + "3"),
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId) + "4"),
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId) + "5"),
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId) + "6"),
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId) + "7"),
        WindowName(kShortcutBarTitle, std::string(kShortcutBarId) + "8")};
  }();
  return names;
}

// LE CURSEUR EST-IL SUR UNE CASE PLEINE DE CETTE BARRE ?
//
// 🔴 CALCULÉ À LA MAIN, ET C'EST CE QUI REND LE CLIC-TRAVERSANT POSSIBLE.
// `NoMouseInputs` se pose sur le `Begin`, donc AVANT qu'ImGui ait rien à dire de
// ce que la fenêtre survole : lui demander reviendrait à lire la réponse de
// l'image précédente, laquelle dépend elle-même du drapeau qu'on cherche à
// poser. Le calcul, lui, ne dépend de rien.
//
// ⚠ LA PLACE VIENT DE L'IMAGE PRÉCÉDENTE — c'est ImGui qui retient la position
// d'une fenêtre, et la barre n'est pas encore ouverte. Une barre qu'on vient de
// déplacer répondrait donc faux pendant une image ; elle ne se déplace que
// DÉVERROUILLÉE, état où l'appelant ne pose pas la question.
[[nodiscard]] bool MouseOverFilledSlot(const char* window_id, const ShortcutBarGrid& grid,
                                       std::span<const ShortcutSlotView> slots, float side,
                                       float gap) {
  const ImGuiWindow* placed = ImGui::FindWindowByName(window_id);
  if (placed == nullptr) {
    return false;
  }
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const std::optional<std::size_t> rank =
      ShortcutSlotUnder(grid, mouse.x - placed->Pos.x, mouse.y - placed->Pos.y, side, gap);
  if (!rank.has_value() || *rank >= slots.size()) {
    return false;
  }
  return slots[*rank].skill_id != 0 || slots[*rank].item_id != 0;
}

// Dessine UNE barre. `slots` porte ses douze cases ; `first_index` est l'index
// global de la première, donc ce qui change un rang de barre en une case que le
// reste du client sait désigner.
//
// ⚠ CE QU'ELLE DEMANDE S'AJOUTE À `asked` SANS L'ÉCRASER : une barre qui n'a
// rien reçu ne doit pas effacer ce que la précédente a rendu. Le joueur n'a
// qu'une souris, donc au plus une barre remplit quoi que ce soit.
void DrawOneBar(std::size_t bar, const ShortcutBarLayout& look, const ShortcutBarsLayout& common,
                std::span<const ShortcutSlotView> slots, std::size_t first_index,
                ShortcutBarsAction& asked) {
  const bool show_keys = common.show_keys;
  const float scale = Scale();
  const float side =
      std::clamp(look.slot_side_px, kSmallestSlotSidePixels, kLargestSlotSidePixels) * scale;
  const float gap = std::clamp(look.gap_px, 0.0F, kLargestSlotGapPixels) * scale;
  const ShortcutBarGrid grid = GridOfBar(look);
  const auto across = static_cast<float>(grid.across());
  const auto down = static_cast<float>(grid.rows);

  // ⚠ SANS BARRE DE TITRE ET SANS MARGE : la barre EST ses cases. Un cadre
  // autour prendrait une place que le jeu ne prend pas, et la poserait plus haut
  // que le joueur ne l'attend.
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
  ImGui::SetNextWindowSize(
      ImVec2(across * side + (across - 1.0F) * gap, down * side + (down - 1.0F) * gap),
      ImGuiCond_Always);
  // La PREMIÈRE en bas de l'écran, comme celle du jeu ; les autres empilées
  // au-dessus. Ensuite, là où le joueur les a mises — c'est ImGui qui retient.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const float stacked = static_cast<float>(bar) * (kFirstStackStep * scale);
  ImGui::SetNextWindowPos(
      ImVec2(screen->GetCenter().x, screen->WorkPos.y + screen->WorkSize.y - stacked),
      ImGuiCond_FirstUseEver, ImVec2(0.5F, 1.0F));

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  // ⚠ VERROUILLÉE, LA BARRE NE SE DÉPLACE PAS — c'est tout ce que le verrou fait.
  // Ses cases restent cliquables, glissables et déposables ; sans cela le verrou
  // serait un interrupteur qui éteint la barre, et personne ne l'emploierait.
  if (common.locked) {
    flags |= ImGuiWindowFlags_NoMove;
  }
  std::array<WindowName, kShortcutBarCount>& names = BarNames();
  // ── CE QUE LE CLIC TRAVERSE ────────────────────────────────────────────────
  //
  // 🔴 UNE BARRE SE POSE SUR LE MONDE, et tout ce qu'elle prend à la souris,
  // elle le prend au jeu. Une fenêtre entière qui capture pour douze icônes rend
  // sourds ses propres trous : le joueur clique entre deux cases pour marcher,
  // et rien ne se passe.
  //
  // Deux raisons de se taire, et elles ne portent pas sur la même chose :
  //  — le réglage, qui rend la barre ENTIÈRE traversante, décor compris ;
  //  — le verrou, qui ne libère que ce qui n'est pas une case PLEINE. Une case
  //    vide n'a rien à lancer ; la laisser prendre le clic ne servirait qu'à
  //    l'avaler.
  //
  // ⚠ ET UN GLISSER RAMÈNE TOUT LE MONDE : c'est justement sur une case VIDE
  // qu'on dépose ce qu'on range. Une barre sourde pendant le dépôt serait une
  // barre qu'on ne peut plus remplir.
  const bool dragging = ImGui::GetDragDropPayload() != nullptr;
  bool deaf = false;
  if (look.click_through && !ImGui::GetIO().KeyShift) {
    deaf = true;
  } else if (common.locked && !MouseOverFilledSlot(names[bar].Get(), grid, slots, side, gap)) {
    deaf = true;
  }
  if (dragging) {
    deaf = false;
  }
  if (deaf) {
    flags |= ImGuiWindowFlags_NoMouseInputs;
  }
  const bool open = ImGui::Begin(names[bar].Get(), nullptr, flags);
  // La fenêtre existe même repliée : c'est elle qu'on compare au survol.
  g_last_bar_window = ImGui::GetCurrentWindow();
  if (!open) {
    ImGui::End();
    ImGui::PopStyleVar(kBarStyleVars);
    ImGui::PopStyleColor(static_cast<int>(kBarColors.size()));
    return;
  }

  ImDrawList* into = ImGui::GetWindowDrawList();
  for (std::size_t rank = 0; rank < grid.shown && rank < slots.size(); ++rank) {
    const ShortcutSlotView& slot = slots[rank];
    const auto index = static_cast<int>(first_index + rank);
    if (rank % grid.columns != 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(static_cast<int>(rank));
    const ImVec2 at = ImGui::GetCursorScreenPos();
    // ⚠ LA CASE EST LE WIDGET : c'est elle qui prend le clic, la molette et le
    // dépôt. L'art n'est que de la peinture.
    //
    // ⚠ DÉVERROUILLÉE, ELLE DEVIENT INERTE : la barre se saisit alors de
    // n'importe où, et une case qui prendrait encore le clic ne laisserait rien
    // à attraper.
    bool pressed = false;
    if (common.locked) {
      // 🔴 C'EST LE BOUTON QUI DIT « LANCÉ », ET NON L'APPUI. Un `IsMouseClicked`
      // part à la PRESSION : glisser une case pour la réarranger lançait donc ce
      // qu'elle porte avant même d'avoir bougé, et la tirer dehors pour la vider
      // la lançait une dernière fois au passage. Le bouton, lui, ne répond qu'au
      // RELÂCHEMENT sur lui-même — un glisser qui part ailleurs ne le déclenche
      // jamais.
      pressed = ImGui::InvisibleButton("case", ImVec2(side, side));
    } else {
      ImGui::Dummy(ImVec2(side, side));
    }
    const bool hot = common.locked && ImGui::IsItemHovered();

    // LA CASE PART EN GLISSANT, et ce qu'elle porte est son INDEX : la cible ne
    // regarde ni la compétence ni l'objet, elle échange deux places.
    const bool carries_something = slot.skill_id != 0 || slot.item_id != 0;
    if (common.locked && carries_something && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      const ShortcutDrag carried{.index = static_cast<std::uint16_t>(index)};
      ImGui::SetDragDropPayload(kShortcutDragPayload, &carried, sizeof(carried));
      if (slot.icon.usable()) {
        ImGui::Image(slot.icon.id, ImVec2(side, side));
      } else if (!slot.name.empty()) {
        ImGui::TextUnformatted(slot.name.data(), slot.name.data() + slot.name.size());
      }
      // 🔴 HORS DES BARRES, L'APERÇU SE BARRE DE ROUGE. Relâcher là VIDE la case,
      // sans confirmation — comme dans le jeu. Le prévenir pendant qu'il le fait
      // est le seul moment où cela sert encore.
      if (!g_cursor_was_over_a_bar) {
        ImDrawList* over = ImGui::GetWindowDrawList();
        const ImVec2 from = ImGui::GetItemRectMin();
        const ImVec2 to = ImGui::GetItemRectMax();
        over->AddRectFilled(from, to, kRemoveVeil);
        over->AddLine(from, to, kRemoveCross, kRemoveThickness * scale);
        over->AddLine(ImVec2(to.x, from.y), ImVec2(from.x, to.y), kRemoveCross,
                      kRemoveThickness * scale);
      }
      ImGui::EndDragDropSource();
    }

    bool dropping = false;
    if (ImGui::BeginDragDropTarget()) {
      dropping = true;
      if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kSkillDragPayload);
          dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(SkillDrag))) {
        asked.dropped_on = index;
        asked.dropped = *static_cast<const SkillDrag*>(dropped->Data);
      }
      // ⚠ UN SECOND NOM DE CHARGE, et c'est lui qui fait le tri. ImGui refuse
      // celle dont le nom ne correspond pas : ni le sac ni le grimoire n'ont à
      // vérifier ce que l'autre envoie.
      if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
          dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(ItemDrag))) {
        asked.item_dropped_on = index;
        asked.item_dropped = *static_cast<const ItemDrag*>(dropped->Data);
      }
      // UNE CASE POSÉE SUR UNE AUTRE : les deux s'échangent.
      if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kShortcutDragPayload);
          dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(ShortcutDrag))) {
        asked.moved_from = static_cast<const ShortcutDrag*>(dropped->Data)->index;
        asked.moved_to = index;
      }
      ImGui::EndDragDropTarget();
    }

    // ⚠ ET PAS AU BOUT D'UN GLISSER, même revenu à son point de départ. Le
    // bouton reste l'élément actif pendant qu'on traîne sa case ; la relâcher
    // là où on l'a prise est un geste ANNULÉ, et lancer alors serait la
    // surprise de trop.
    if (pressed && ImGui::GetDragDropPayload() == nullptr) {
      // MAJ ET CLIC GAUCHE POSE LE LIEN, au lieu de lancer : c'est le geste du
      // sac, et le joueur qui l'y a appris ne le cherche pas deux fois.
      //
      // ⚠ LA COMPÉTENCE PART QUAND MÊME : il n'existe pas de balise de
      // compétence dans le fil, donc Maj n'a rien à lui prendre.
      if (ImGui::GetIO().KeyShift && slot.item_id != 0) {
        asked.link = index;
      } else {
        asked.cast = index;
      }
    }

    if (hot) {
      // Le clic MOLETTE vide la case : c'est le geste du jeu, et il ne demande
      // ni menu ni confirmation.
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        asked.cleared = index;
      }
      // LE CLIC DROIT DÉCRIT — et seulement un OBJET. Il n'existe pas de fenêtre
      // de description pour une compétence : la demander ouvrirait le vide, et
      // un clic qui n'ouvre rien passe pour une panne.
      if (slot.item_id != 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        asked.describe = index;
      }
      // ── MAJ + MOLETTE : LE RANG AUQUEL CETTE CASE PART ────────────────────
      //
      // 🔴 IL FAUT REPRENDRE LA MOLETTE À IMGUI, et pas seulement lire ses
      // crans : il lie la possession de la molette à la fenêtre survolée, et
      // sous Maj il la convertit en défilement HORIZONTAL. Sans cette
      // revendication, la barre glisserait de côté au lieu de régler.
      //
      // ⚠ REVENDIQUÉE À CHAQUE IMAGE, pas au moment où un cran arrive : le
      // défilement s'applique au DÉBUT de l'image, donc ce qu'on revendique ne
      // vaut que pour la SUIVANTE.
      if (slot.skill_id != 0 && slot.known && slot.most_level > 1 && ImGui::GetIO().KeyShift) {
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGui::GetItemID());
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0F) {
          // ⚠ UN CRAN VAUT UN RANG, quel que soit le pas : les souris à
          // défilement libre envoient des fractions, et on ne veut pas zéro
          // rang de plus.
          const int shown = slot.level > 0 ? slot.level : slot.most_level;
          const int wanted = std::clamp(shown + (wheel > 0.0F ? 1 : -1), 1, slot.most_level);
          if (wanted != shown) {
            asked.level_for = index;
            asked.level = wanted;
          }
          // Consommée : plus personne ne la relit dans cette image.
          ImGui::GetIO().MouseWheel = 0.0F;
        }
      }
      if (carries_something) {
        if (BeginSkinTooltip()) {
          // ⚠ UN REPLI NOMMÉ PLUTÔT QU'UNE BULLE MUETTE : un identifiant que la
          // base ne connaît pas est précisément le cas où l'on vient lire le
          // numéro, et c'est alors que le nom manque.
          if (slot.name.empty()) {
            ImGui::TextUnformatted(slot.item_id != 0 ? core::Tr("Item") : core::Tr("Skill"));
          } else {
            ImGui::TextUnformatted(slot.name.data(), slot.name.data() + slot.name.size());
          }
          // 🔴 LE NUMÉRO EST CE QU'ON VIENT Y CHERCHER. Le nom, l'icône le dit
          // déjà ; l'identifiant ne se lit nulle part ailleurs, et c'est lui qu'on
          // recopie dans une commande, un script ou un message. Le rang d'une
          // compétence l'accompagne parce qu'il décide de ce que le sort fait, et
          // que la case ne l'écrit qu'en tout petit, dans un coin.
          if (slot.item_id != 0) {
            ImGui::TextDisabled(core::Tr("ID : %u"), slot.item_id);
          } else {
            ImGui::TextDisabled(core::Tr("Niveau %d — ID : %u"), slot.level,
                                static_cast<unsigned>(slot.skill_id));
          }
          if (!slot.known) {
            // ⚠ DEUX RAISONS DE SE GRISER, DEUX PHRASES. « Plus connue » sur une
            // potion ne voudrait rien dire, et « il n'en reste plus » sur une
            // compétence encore moins.
            ImGui::TextDisabled("%s", slot.item_id != 0 ? core::Tr("Il n'en reste plus.")
                                                        : core::Tr("Ce skill n'est plus connu."));
          }
          EndSkinTooltip();
        }
      }
    }
    PaintSlot(into, look, slot, at, side, hot, dropping, show_keys);
    ImGui::PopID();
  }

  ImGui::End();
  ImGui::PopStyleVar(kBarStyleVars);
  ImGui::PopStyleColor(static_cast<int>(kBarColors.size()));
}

}  // namespace

ShortcutBarsAction DrawShortcutBars(const ShortcutBarsLayout& layout,
                                    std::span<const ShortcutSlotView> slots, const SkinArt& art) {
  static_cast<void>(art);
  ShortcutBarsAction asked;
  // Les fenêtres qu'on vient d'ouvrir, pour savoir ensuite si le curseur est sur
  // l'une d'elles. ⚠ LES OBJETS D'IMGUI, ET NON LEURS NOMS : un nom se traduit,
  // un pointeur de fenêtre est la fenêtre.
  std::array<const ImGuiWindow*, kShortcutBarCount> drawn{};
  for (std::size_t bar = 0; bar < kShortcutBarCount; ++bar) {
    const std::size_t first = ShortcutSlotIndex(bar, 0);
    if (!layout.bars[bar].shown || first >= slots.size()) {
      continue;
    }
    // ⚠ CE QUE LA BARRE A VRAIMENT : les cases attendues, ou ce qu'il en reste.
    // Une vue plus courte que prévu n'est pas une erreur — c'est ce qui arrive
    // pendant qu'une partie s'ouvre.
    const std::size_t have = std::min(kShortcutSlotsPerBar, slots.size() - first);
    DrawOneBar(bar, layout.bars[bar], layout, slots.subspan(first, have), first, asked);
    drawn[bar] = g_last_bar_window;
  }

  // LE CURSEUR EST-IL SUR UNE BARRE ? Relevé APRÈS les huit, quand la réponse
  // est enfin exacte, et gardé pour la croix rouge de l'image suivante.
  const ImGuiWindow* hovered = ImGui::GetCurrentContext()->HoveredWindow;
  g_cursor_was_over_a_bar = false;
  for (const ImGuiWindow* window : drawn) {
    if (window != nullptr && window == hovered) {
      g_cursor_was_over_a_bar = true;
    }
  }

  // UNE CASE LÂCHÉE HORS DES BARRES SE VIDE.
  //
  // 🔴 SANS CE GESTE, VIDER UNE CASE DEMANDAIT DE DEVINER LE CLIC MOLETTE, que
  // rien n'annonce. Celui-ci s'apprend seul — on tire la case dehors, comme on
  // range — et l'aperçu prévient en chemin.
  //
  // ⚠ LES TROIS CONDITIONS SE LISENT ENSEMBLE, comme pour un objet du sac : le
  // bouton vient d'être relâché, AUCUNE case n'a pris la charge, et le curseur
  // n'est sur aucune barre.
  const ImGuiPayload* const carried = ImGui::GetDragDropPayload();
  if (asked.moved_to < 0 && carried != nullptr && carried->IsDataType(kShortcutDragPayload) &&
      !carried->IsDelivery() && carried->DataSize == static_cast<int>(sizeof(ShortcutDrag)) &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !g_cursor_was_over_a_bar) {
    asked.dropped_away = static_cast<const ShortcutDrag*>(carried->Data)->index;
  }
  return asked;
}

float CenteredBarLeft(float screen_left, float screen_width, float bar_width) {
  // ⚠ ARRONDI AU PIXEL : une barre posée sur un demi-pixel floute ses icônes.
  return std::floor(screen_left + (screen_width - bar_width) * 0.5F);
}

ImVec2 SnappedAmongBars(ImVec2 at, ImVec2 size, const HudSiblings& bars) {
  return ImVec2(SnappedToSiblings(at.x, size.x, bars, false),
                SnappedToSiblings(at.y, size.y, bars, true));
}

void SnapMovingShortcutBar(const ShortcutBarsLayout& layout) {
  if (!layout.sticky || layout.locked) {
    return;
  }
  const ImGuiContext* context = ImGui::GetCurrentContext();
  if (context == nullptr || context->MovingWindow == nullptr) {
    return;
  }
  const ImGuiWindow* moving = context->MovingWindow->RootWindow;
  std::array<HudRect, kShortcutBarCount> frames{};
  std::array<bool, kShortcutBarCount> shown{};
  ImGuiWindow* mine = nullptr;
  int mine_rank = -1;
  std::array<WindowName, kShortcutBarCount>& names = BarNames();
  for (std::size_t bar = 0; bar < kShortcutBarCount; ++bar) {
    ImGuiWindow* window = ImGui::FindWindowByName(names[bar].Get());
    // ⚠ UNE BARRE ÉTEINTE N'AIMANTE RIEN : sa fenêtre survit dans ImGui, à la
    // place où on l'a laissée, et l'on s'alignerait sur ce qu'on ne voit pas.
    if (window == nullptr || !layout.bars[bar].shown || !window->WasActive) {
      continue;
    }
    frames[bar] = HudRect{
        .x = window->Pos.x, .y = window->Pos.y, .width = window->Size.x, .height = window->Size.y};
    shown[bar] = true;
    if (window == moving) {
      mine = window;
      mine_rank = static_cast<int>(bar);
    }
  }
  if (mine == nullptr) {
    return;
  }
  const HudSiblings bars{.frames = frames, .shown = shown, .mine = mine_rank};
  const ImVec2 posed = SnappedAmongBars(mine->Pos, mine->Size, bars);
  if (posed.x != mine->Pos.x || posed.y != mine->Pos.y) {
    ImGui::SetWindowPos(mine, posed, ImGuiCond_Always);
  }
}

void CenterShortcutBar(std::size_t bar) {
  if (bar >= kShortcutBarCount || ImGui::GetCurrentContext() == nullptr) {
    return;
  }
  ImGuiWindow* window = ImGui::FindWindowByName(BarNames()[bar].Get());
  if (window == nullptr) {
    return;
  }
  // 🔴 LA POSITION D'IMGUI, ÉCRITE COMME LA SOURIS L'ÉCRIRAIT. Le placeur des
  // fenêtres voit la barre ailleurs que là où il l'avait posée, et retient la
  // nouvelle place — ancrée au centre — exactement comme après un glisser.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const float left = CenteredBarLeft(screen->WorkPos.x, screen->WorkSize.x, window->Size.x);
  ImGui::SetWindowPos(window, ImVec2(left, window->Pos.y), ImGuiCond_Always);
}

}  // namespace ro::ui
