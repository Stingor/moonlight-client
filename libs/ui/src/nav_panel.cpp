#include "ro/ui/nav_panel.h"

#include <algorithm>
#include <cstddef>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link_offer.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// La liste ne descend pas sous cette largeur — en caractères, pour qu'elle suive
// la police et l'échelle plutôt qu'un nombre de pixels.
constexpr float kSmallestNavChars = 5.0F;
// Et elle ne prend jamais plus de cette part de la place : le contenu à droite
// en a besoin, et c'est lui qu'on est venu lire.
constexpr float kLargestNavShare = 0.4F;

// Les deux volets, nommés une fois : l'identité d'un enfant décide du
// défilement qu'il retient, et deux noms écrits à deux endroits divergent.
constexpr const char* kListChild = "liste";
constexpr const char* kContentChild = "contenu";

// L'entrée dont l'appui a posé un lien, retenue par la liste jusqu'au
// relâchement ; « aucune » sinon.
constexpr const char* kLinkArmedKey = "##lien_arme";
constexpr int kNoLinkArmed = -1;

// ⚠ JAMAIS TRADUIT : c'est une clé ImGui, pas un texte.
constexpr const char* kFilterId = "##filtre_nav";

// Ce que la liste VOUDRAIT, et ce qu'on lui DONNE. Les deux, parce que leur
// écart est exactement ce qui dit si les libellés sont coupés.
struct NavWidth {
  float wanted = 0.0F;
  float given = 0.0F;
};

[[nodiscard]] NavWidth MeasureNav(std::span<const NavEntry> entries) {
  const ImGuiStyle& style = ImGui::GetStyle();
  float widest = 0.0F;
  for (const NavEntry& entry : entries) {
    widest = std::max(widest, ImGui::CalcTextSize(entry.label).x);
    // ⚠ LE TITRE DE FAMILLE COMPTE AUSSI, et il est souvent plus court — mais
    // pas toujours. L'oublier le ferait couper alors qu'il est ce qui range
    // tout le reste.
    if (entry.group != nullptr) {
      widest = std::max(widest, ImGui::CalcTextSize(entry.group).x);
    }
  }
  NavWidth measured;
  measured.wanted = widest + (style.WindowPadding.x + style.FramePadding.x) * 2.0F;
  const float smallest = ImGui::GetFontSize() * kSmallestNavChars;
  const float largest = std::max(smallest, ImGui::GetContentRegionAvail().x * kLargestNavShare);
  measured.given = std::clamp(measured.wanted, smallest, largest);
  return measured;
}

// LE CHAMP DE FILTRE, à la largeur de la liste, puis — quand on cherche — le
// bouton qui le vide et le compte des résultats.
//
// ⚠ LE COMPTE NE DÉBORDE PAS SUR LE CONTENU : la colonne n'est pas un cadre qui
// le couperait. Trop large à côté du bouton, il passe à la ligne.
void DrawFilter(NavFilter& filter, float width, const SkinArt& art) {
  if (filter.focus) {
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::SetNextItemWidth(width);
  ImGui::InputTextWithHint(kFilterId, filter.hint, filter.typed.data(), filter.typed.size(),
                           ImGuiInputTextFlags_EscapeClearsAll);
  if (filter.typed.empty() || filter.typed.front() == '\0') {
    return;
  }
  const float left = ImGui::GetCursorPosX();
  if (SkinSmallButton(core::Tr("Effacer"), art)) {
    filter.typed.front() = '\0';
  }
  if (filter.found == nullptr) {
    return;
  }
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float used = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x - left;
  if (used + spacing + ImGui::CalcTextSize(filter.found).x <= width) {
    ImGui::SameLine();
  }
  ImGui::TextDisabled("%s", filter.found);
}

}  // namespace

float NavPanelWidth(std::span<const NavEntry> entries) { return MeasureNav(entries).given; }

void BeginNavPanel(const char* id, std::span<const NavEntry> entries, int* current,
                   const SkinArt& art, int* linked, NavFilter* filter) {
  ImGui::PushID(id);
  const NavWidth measured = MeasureNav(entries);
  // La borne a mordu : les libellés ne tiennent pas, et ImGui les coupe NET,
  // sans même une ellipse pour le dire. On ne triche pas sur la largeur — le
  // contenu en a besoin —, on rend le libellé lisible autrement, en infobulle.
  const bool clipped = measured.given < measured.wanted;

  // LE FILTRE ET LA LISTE FORMENT UNE COLONNE : le champ ne défile pas avec les
  // entrées, il reste là où on le tape.
  ImGui::BeginGroup();
  if (filter != nullptr) {
    DrawFilter(*filter, measured.given, art);
  }
  // Hauteur nulle : les deux volets prennent toute la place que la fenêtre leur
  // laisse. C'est elle qui décide de la taille, et c'est elle que le joueur
  // redimensionne.
  ImGui::BeginChild(kListChild, ImVec2(measured.given, 0.0F), ImGuiChildFlags_Borders);
  // L'ENTRÉE DONT L'APPUI A POSÉ UN LIEN, jusqu'au relâchement. 🔴 LA SÉLECTION
  // SE JOUE AU RELÂCHEMENT : Maj lâché entre les deux, le même clic changerait de
  // section après avoir posé le lien. Son relâchement est donc ignoré.
  ImGuiStorage* const kept = ImGui::GetStateStorage();
  const ImGuiID armed_key = ImGui::GetID(kLinkArmedKey);
  int armed = kept->GetInt(armed_key, kNoLinkArmed);
  for (int rank = 0; rank < static_cast<int>(entries.size()); ++rank) {
    const NavEntry& entry = entries[static_cast<std::size_t>(rank)];
    // ⚠ LE TITRE DE FAMILLE N'EST PAS CLIQUABLE : il ne mène nulle part, et le
    // rendre sélectionnable donnerait une case vide à qui le vise.
    if (entry.group != nullptr) {
      if (rank > 0) {
        ImGui::Spacing();
      }
      ImGui::TextDisabled("%s", entry.group);
      ImGui::Separator();
    }
    const int target = entry.target >= 0 ? entry.target : rank;
    // ⚠ SOUS L'IDENTITÉ DE LA SECTION, pas du rang : la liste filtrée décale les
    // rangs, et un appui tenu suivrait sinon l'entrée qui prend la place.
    ImGui::PushID(target);
    const bool chosen = ImGui::Selectable(entry.label, current != nullptr && *current == target);
    const bool offers = linked != nullptr;
    if (offers && ShiftClickedLastItem()) {
      *linked = target;
      armed = target;
    } else if (chosen && armed == target) {
      armed = kNoLinkArmed;
    } else if (chosen && current != nullptr && !(offers && ImGui::GetIO().KeyShift)) {
      *current = target;
    }
    // ⚠ UNE SEULE BULLE PORTE LES DEUX AIDES — le libellé entier d'une liste
    // trop étroite, et le geste du lien : deux bulles sur le même élément, c'est
    // la seconde qui écrase la première.
    if (offers && HoveredForLinkTooltip()) {
      if (clipped) {
        SkinTooltipFormat("%s\n%s", entry.label, ChatLinkGestureHint());
      } else {
        SkinTooltipFormat("%s", ChatLinkGestureHint());
      }
    } else if (!offers && clipped && ImGui::IsItemHovered()) {
      SkinTooltipFormat("%s", entry.label);
    }
    ImGui::PopID();
  }
  // ⚠ LE BOUTON RELÂCHÉ AILLEURS DÉSARME AUSSI : l'appui suivant repart de zéro.
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    armed = kNoLinkArmed;
  }
  kept->SetInt(armed_key, armed);
  // L'ascenseur de la liste, tant qu'elle est la fenêtre courante : sans cela
  // elle défilerait sans rien montrer.
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  ImGui::EndGroup();

  ImGui::SameLine();
  ImGui::BeginChild(kContentChild, ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders);
  // Le texte se replie à la largeur du volet : une aide d'une phrase y tient
  // rarement sur une ligne, et une fenêtre étroite est le cas normal.
  ImGui::PushTextWrapPos(0.0F);
}

void EndNavPanel(const SkinArt& art) {
  ImGui::PopTextWrapPos();
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  ImGui::PopID();
}

}  // namespace ro::ui
