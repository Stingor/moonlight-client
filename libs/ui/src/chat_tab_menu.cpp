#include "chat_tab_menu.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// La largeur des barres du sous-menu d'apparence. Assez pour lire le nombre sans
// que le menu ne prenne la moitie de l'ecran.
constexpr float kLookSliderWidth = 150.0F;
// La marge d'un MENU, resserrée. Celle du cadre du chat est réglable jusqu'à
// douze pixels : cela convient à une fenêtre, cela étale un menu — et un menu
// qu'on ne lit plus d'un coup d'œil est un menu qu'on n'utilise plus.
constexpr float kMenuPadding = 4.0F;

// LES FILTRES DU FIL, dans leur sous-menu : les deux blocs, puis une case par
// type de message, dans l'ordre des numéros.
//
// ⚠ UNE CASE EN ATTENTE SE COCHE COMME LES AUTRES : seul son libellé se grise.
// Le joueur qui prépare un onglet « Quêtes » doit pouvoir le faire dès
// aujourd'hui, et le trouver rempli le jour où les messages arriveront.
void DrawFilterMenu(std::span<const ChatTypeToggle> types, bool diagnostic, const SkinArt& art,
                    ChatWindowAction* asked) {
  // ⚠ LE COMPTE EST DANS LE LIBELLÉ : c'est l'information qu'on voulait
  // obtenir en ouvrant, dans neuf cas sur dix, sans avoir à dérouler.
  int accepted = 0;
  for (const ChatTypeToggle& type : types) {
    accepted += type.on ? 1 : 0;
  }
  std::array<char, 96> menu_label{};
  std::snprintf(menu_label.data(), menu_label.size(), core::Tr("Filtres du fil  (%d/%d)###filtres"),
                accepted, static_cast<int>(types.size()));
  if (!ImGui::BeginMenu(menu_label.data())) {
    return;
  }
  // ⚠ LES DEUX BLOCS D'ABORD : à vingt-huit cases, aller les cocher une à une
  // pour n'en garder qu'une est le geste qu'on fait le plus souvent.
  if (SkinSmallButton(core::Tr("Tout cocher"), art)) {
    asked->set_every_type = true;
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Tout décocher"), art)) {
    asked->set_every_type = false;
  }
  ImGui::Separator();
  for (int rank = 0; rank < static_cast<int>(types.size()); ++rank) {
    const ChatTypeToggle& type = types[static_cast<std::size_t>(rank)];
    ImGui::PushID(rank);
    bool on = type.on;
    if (SkinCheckBox("##type", &on, art)) {
      asked->toggled_type = rank;
    }
    ImGui::SameLine();
    // Le libellé d'une case en attente, au texte grisé du thème.
    if (type.waiting) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    // ⚠ LE NUMÉRO SEULEMENT AU DIAGNOSTIC : c'est là qu'il sert, à relier une
    // ligne préfixée « [t07] » à sa case. Hors diagnostic, il n'ajouterait que
    // du bruit à une liste qu'on lit pour son sens.
    if (diagnostic && !type.tag.empty()) {
      ImGui::Text("%.*s  %.*s", static_cast<int>(type.tag.size()), type.tag.data(),
                  static_cast<int>(type.label.size()), type.label.data());
    } else {
      Unformatted(type.label);
    }
    if (type.waiting) {
      ImGui::PopStyleColor();
      if (ImGui::IsItemHovered() && !type.waiting_tip.empty()) {
        SkinTooltipFormat("%.*s", static_cast<int>(type.waiting_tip.size()),
                          type.waiting_tip.data());
      }
    }
    ImGui::PopID();
  }
  ImGui::EndMenu();
}

// SON APPARENCE À LUI, dans son sous-menu.
//
// ⚠ LE LIBELLÉ ANNONCE L'ÉTAT, et c'est là tout son intérêt : on sait AVANT
// d'ouvrir si cet onglet a été personnalisé ou s'il suit les réglages
// généraux — la question qu'on se pose en neuf cas sur dix.
void DrawLookMenu(const ChatTabLook& tuned, const SkinArt& art, ChatWindowAction* asked) {
  std::array<char, 96> look_label{};
  std::snprintf(look_label.data(), look_label.size(), core::Tr("Apparence  (%s)###apparence"),
                tuned.own ? core::Tr("propre à cet onglet") : core::Tr("générale"));
  if (!ImGui::BeginMenu(look_label.data())) {
    return;
  }
  ChatTabLook edited = tuned;
  bool touched = false;
  // ⚠ LE DRAPEAU D'ABORD, ET IL COMMANDE LE RESTE : décoché, l'onglet SUIT
  // les réglages généraux — y compris leurs changements à venir. Montrer des
  // barres actives sous une case décochée promettrait un effet qu'elles
  // n'ont pas.
  if (SkinCheckBox("##sienne", &edited.own, art)) {
    touched = true;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(core::Tr("Une apparence à lui"));

  ImGui::BeginDisabled(!edited.own);
  ImGui::SetNextItemWidth(Px(kLookSliderWidth));
  if (SkinSliderInt(core::Tr("Taille du texte###taille_onglet"), &edited.font_percent,
                    kSmallestChatFontPercent, kLargestChatFontPercent, art, "%d %%")) {
    touched = true;
  }
  ImGui::SetNextItemWidth(Px(kLookSliderWidth));
  if (SkinSliderInt(core::Tr("Marges###marges_onglet"), &edited.padding_pixels, 0,
                    kLargestChatPaddingPixels, art, "%d px")) {
    touched = true;
  }
  ImGui::SetNextItemWidth(Px(kLookSliderWidth));
  if (SkinSliderInt(core::Tr("Interligne###interligne_onglet"), &edited.line_gap_pixels, 0,
                    kLargestChatLineGapPixels, art, "%d px")) {
    touched = true;
  }
  // Le fond, TRANSPARENCE COMPRISE : c'est elle qu'on vient régler le plus
  // souvent — un onglet qu'on veut lire par-dessus le jeu, ou l'inverse.
  //
  // ⚠ EN DERNIER DANS CE SOUS-MENU, et ce n'est pas indifférent : son
  // sélecteur s'ouvre vers le bas et recouvrirait ce qui le suit.
  if (SkinColorSwatch(core::Tr("Fond###fond_onglet"), &edited.body, art).changed) {
    touched = true;
  }
  ImGui::EndDisabled();

  if (touched) {
    asked->tuned_look = true;
    asked->look = edited;
  }
  ImGui::EndMenu();
}

// OÙ CET ONGLET EST MONTRÉ : le sortir dans une fenêtre, ou l'y rendre.
void DrawPlaceItems(const ChatWindowState& state, bool meter_tab, bool can_detach,
                    ChatWindowAction* asked) {
  if (meter_tab) {
    // Toujours permis : le compteur ne prive personne de quoi lire.
    if (ImGui::Selectable(core::Tr("Détacher le compteur"))) {
      asked->detach_tab = state.configuring;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltipFormat("%s", core::Tr("Il redevient une fenêtre à lui. On l'y range de nouveau "
                                       "en glissant sa barre de titre sur une bande d'onglets."));
    }
    return;
  }
  if (state.group != 0 && ImGui::Selectable(core::Tr("Rendre à la conversation"))) {
    asked->attach_tab = state.configuring;
  }
  // Depuis une flottante, c'est le geste qui sépare deux onglets qui
  // partageaient la même ; seul dans la sienne, il n'a nulle part où aller.
  ImGui::BeginDisabled(!can_detach);
  if (ImGui::Selectable(state.group == 0 ? core::Tr("Sortir dans une fenêtre")
                                         : core::Tr("Sortir dans une fenêtre à lui"))) {
    asked->detach_tab = state.configuring;
  }
  ImGui::EndDisabled();
  if (!can_detach) {
    SkinTooltip(state.group == 0 ? core::Tr("Le dernier onglet de la conversation ne "
                                            "peut pas en sortir.")
                                 : core::Tr("Il est déjà seul dans sa fenêtre."));
  }
}

// LE MENU D'UN ONGLET, ouvert au clic droit sur sa languette, comme au client.
//
// ⚠ L'ORDRE EST CELUI QUE LES JOUEURS ONT DANS LA MAIN : le nom ;
// où l'onglet est montré et le verrou de sa fenêtre ; créer, fermer ; son
// apparence ; ses filtres ; le vider. Les réglages généraux viennent en
// dernier, parce qu'ils ne touchent pas qu'à lui.
//
// ⚠ IL SUIT LE THÈME, fond et texte, par l'habillage que la conversation prête
// à ses surgissants (`LendSkinArt`). Seul le champ du nom reste celui du chat :
// clair, et donc écrit en presque-noir.
}  // namespace

void DrawChatTabMenu(ChatWindowState& state, const ChatAppearance& look,
                     std::span<const ChatTypeToggle> types, std::span<const ChatStripTab> tabs,
                     int tabs_everywhere, const ChatTabLook& tuned, const SkinArt& art,
                     ChatWindowAction* asked) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kMenuPadding, kMenuPadding));
  if (!BeginSkinPopup(kChatTabMenuId)) {
    ImGui::PopStyleVar();
    // ⚠ REFERMÉ AILLEURS QUE PAR NOUS — Échap, un clic à côté : le réglage doit
    // cesser, sinon la prochaine ouverture porterait sur un onglet qu'on ne
    // regarde plus.
    state.configuring = -1;
    return;
  }
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::PushStyleColor(ImGuiCol_Text, kChatDarkTextColor);
  const bool renamed = ImGui::InputText("##nom", state.tab_name.data(), state.tab_name.size(),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::PopStyleColor();
  if (renamed) {
    // Un nom vide rendrait l'onglet inattrapable : on refuse en silence plutôt
    // que d'ouvrir une boîte pour si peu.
    if (state.tab_name[0] != '\0') {
      asked->renamed_tab = state.configuring;
      asked->new_name = std::string(state.tab_name.data());
    }
    ImGui::CloseCurrentPopup();
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", core::Tr("Renommer l'onglet - Entrée pour valider."));
  }

  // L'ONGLET DU COMPTEUR n'a ni filtres, ni vidage, ni apparence de fil : il ne
  // montre aucune ligne. Le fermer éteint le compteur, le détacher en refait
  // une fenêtre à lui — toujours permis, il ne prive personne de quoi lire.
  const bool meter_tab = state.configuring >= 0 &&
                         state.configuring < static_cast<int>(tabs.size()) &&
                         tabs[static_cast<std::size_t>(state.configuring)].meter;
  // ⚠ LES POUVOIRS D'UN ONGLET DE FIL SE COMPTENT SANS LE COMPTEUR : fermer
  // ou sortir le dernier fil d'une fenêtre ne doit pas se croire permis parce
  // que le compteur y est rangé.
  const auto logs_here = static_cast<int>(
      std::count_if(tabs.begin(), tabs.end(), [](const ChatStripTab& tab) { return !tab.meter; }));
  ChatTabPowers powers = ChatTabMenuPowers(state.group, logs_here, tabs_everywhere);
  if (meter_tab) {
    powers.can_close = true;
    powers.can_detach = true;
  }

  // ── OÙ IL EST MONTRÉ, ET LE VERROU DE CETTE FENÊTRE-CI ─────────────
  ImGui::Separator();
  DrawPlaceItems(state, meter_tab, powers.can_detach, asked);
  // ⚠ LE LIBELLÉ DU VERROU NOMME LA FENÊTRE, parce que « verrouiller » tout
  // court ne veut rien dire quand trois sont ouvertes.
  bool locked = look.locked;
  if (SkinCheckBox(state.group == 0 ? core::Tr("Verrouiller la conversation")
                                    : core::Tr("Verrouiller cette fenêtre"),
                   &locked, art)) {
    asked->set_locked = locked;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s\n%s",
                      core::Tr("Fige la position et la taille de cette fenêtre-là. "
                               "Les onglets, les menus et l'arrachage continuent "
                               "de fonctionner."),
                      core::Tr("Chaque fenêtre a le sien : une flottante qu'on "
                               "détache naît libre."));
  }

  // ── CRÉER, FERMER ─────────────────────────────────────
  ImGui::Separator();
  ImGui::BeginDisabled(!powers.can_add);
  if (ImGui::Selectable(core::Tr("Nouvel onglet"))) {
    asked->add_tab = true;
  }
  ImGui::EndDisabled();
  if (!powers.can_add && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat(core::Tr("Maximum atteint : %d onglets, fenêtres détachées"
                               " comprises. C'est la limite du client."),
                      kMostTabs);
  }
  ImGui::BeginDisabled(!powers.can_close);
  if (ImGui::Selectable(meter_tab ? core::Tr("Fermer le compteur") : core::Tr("Fermer l'onglet"))) {
    asked->remove_tab = state.configuring;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  if (!powers.can_close) {
    SkinTooltip(core::Tr("Il doit rester au moins un onglet dans la conversation."));
  }
  ImGui::Separator();

  if (meter_tab) {
    if (ImGui::Selectable(core::Tr("Réglages du compteur…"))) {
      asked->meter.open_settings = true;
      ImGui::CloseCurrentPopup();
    }
  } else {
    DrawLookMenu(tuned, art, asked);
    ImGui::Separator();
    DrawFilterMenu(types, look.diagnostic, art, asked);

    // ── VIDER CET ONGLET ──────────────────────────────────────────
    // Sa place est ICI, avec les filtres : les deux décident de ce que l'onglet
    // MONTRE. Et surtout pas à côté de « Fermer l'onglet » — deux gestes qui
    // vident l'écran, voisins d'un pixel dans un menu, finissent par se
    // confondre.
    ImGui::Separator();
    const bool cleared = state.configuring >= 0 &&
                         state.configuring < static_cast<int>(tabs.size()) &&
                         tabs[static_cast<std::size_t>(state.configuring)].cleared;
    if (ImGui::Selectable(core::Tr("Vider cet onglet"))) {
      asked->clear_tab = state.configuring;
    }
    if (ImGui::IsItemHovered()) {
      // ⚠ DEUX TEXTES POUR DEUX LIGNES : le catalogue est une ligne par texte,
      // et le saut vit donc ici (voir ADR 0034).
      SkinTooltipFormat(
          "%s\n%s",
          core::Tr("Masque ce que cet onglet affiche aujourd'hui ; la suite s'affichera "
                   "normalement."),
          core::Tr("Rien n'est détruit : les autres onglets gardent ces lignes."));
    }
    // Proposé seulement s'il y a quelque chose à défaire : un menu qui propose
    // toujours d'annuler ne dit plus rien de l'état de l'onglet.
    if (cleared && ImGui::Selectable(core::Tr("Réafficher tout"))) {
      asked->unclear_tab = state.configuring;
    }

    // ── ET LE RESTE, QUI N'EST PAS ICI ─────────────────────
    // 🔴 CE MENU NE RÈGLE QUE CET ONGLET-LÀ ET SA FENÊTRE. La police, les
    // couleurs générales et les bulles valent pour TOUTES les conversations :
    // l'entrée y mène, elle ne les pose pas ici.
    //
    // ⚠ C'EST LA PUCE DES AUTRES FENÊTRES, sous un autre nom. Le cadre de la
    // chatbox n'est pas habillé — pas de barre de titre, donc pas de puce — et
    // sans cette entrée, le panneau ne s'atteindrait que par le menu du jeu.
    ImGui::Separator();
    if (ImGui::Selectable(core::Tr("Réglages de la conversation"))) {
      asked->settings_asked = true;
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltipFormat("%s\n%s",
                        core::Tr("Ouvre les Options sur la section Conversation : la police, "
                                 "les couleurs et les bulles."),
                        core::Tr("Ces réglages-là valent pour toutes les fenêtres de "
                                 "conversation."));
    }
  }

  EndSkinPopup();
  ImGui::PopStyleVar();
}

}  // namespace ro::ui
