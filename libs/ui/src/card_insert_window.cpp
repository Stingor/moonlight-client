#include "ro/ui/card_insert_window.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include <imgui.h>

#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LES MESURES, en pixels d'interface ──────────────────────────────────────
//
// Où la fenêtre naît la première fois, et sa taille d'alors ; ensuite, elle
// reste où le joueur la pose.
constexpr ImVec2 kSpawnPixels = ImVec2(620.0F, 200.0F);
constexpr ImVec2 kFirstSizePixels = ImVec2(300.0F, 320.0F);
// L'icône d'un objet, et la rangée qui la porte.
constexpr float kIconPixels = 24.0F;
constexpr float kRowPixels = 26.0F;
// L'icône est décalée de deux pixels du bord et d'un du haut de sa rangée ; le
// texte commence quatre pixels après elle.
constexpr ImVec2 kIconInsetPixels = ImVec2(2.0F, 1.0F);
constexpr float kIconTextGapPixels = 4.0F;

[[nodiscard]] ImTextureID IconOf(const SkinImage& icon) { return icon.usable() ? icon.id : 0; }

// LA LIGNE D'UNE PIÈCE : une rangée entière cliquable, l'icône et le texte
// peints PAR-DESSUS.
//
// ⚠ PEINTS SUR LA LISTE DE DESSIN, jamais placés au curseur : déplacer le
// curseur hors du flux étend la fenêtre sans y poser d'élément, et ImGui
// l'interdit.
void DrawRow(const CardInsertRowView& row, CardInsertWindowState& state,
             CardInsertWindowAction& action, const CardInsertRowView*& hovered) {
  const float scale = Scale();
  ImGui::PushID(row.equip_slot);
  const bool chosen = state.selected == row.equip_slot;
  const ImVec2 top = ImGui::GetCursorScreenPos();
  if (ImGui::Selectable("##row", chosen, ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(0.0F, kRowPixels * scale))) {
    state.selected = row.equip_slot;
    // LE DOUBLE-CLIC SERTIT UNE FOIS, sans passer par le bouton.
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      action.verb = CardInsertVerb::kInsert;
      action.equip_slot = row.equip_slot;
      action.times = 1;
    }
  }
  if (ImGui::IsItemHovered()) {
    hovered = &row;
  }

  ImDrawList* into = ImGui::GetWindowDrawList();
  float text_x = top.x + kIconInsetPixels.x * scale;
  if (const ImTextureID icon = IconOf(row.icon); icon != 0) {
    const ImVec2 from(text_x, top.y + kIconInsetPixels.y * scale);
    into->AddImage(icon, from, ImVec2(from.x + kIconPixels * scale, from.y + kIconPixels * scale));
    text_x += (kIconPixels + kIconTextGapPixels) * scale;
  }
  // ⚠ LE NOM PORTE DÉJÀ LE RAFFINEMENT ET LE « [N] » : on ne les rajoute pas.
  const std::string_view name = row.name.empty() ? std::string_view("(?)") : row.name;
  const float text_y = top.y + (kRowPixels * scale - ImGui::GetTextLineHeight()) * 0.5F;
  into->AddText(ImVec2(text_x, text_y), ImGui::GetColorU32(ImGuiCol_Text), name.data(),
                name.data() + name.size());
  // LES CASES DÉJÀ PRISES : de quoi choisir, jamais un filtre.
  if (row.used_slots > 0) {
    // ⚠ LES DEUX ESPACES DE TÊTE HORS DE LA CLÉ : le catalogue n'en garde aucun.
    std::array<char, 64> counted = {};
    std::snprintf(counted.data(), counted.size(),
                  row.used_slots > 1 ? core::Tr("(%d serties)") : core::Tr("(%d sertie)"),
                  row.used_slots);
    std::array<char, 72> used = {};
    std::snprintf(used.data(), used.size(), "  %s", counted.data());
    const float after = ImGui::CalcTextSize(name.data(), name.data() + name.size()).x;
    into->AddText(ImVec2(text_x + after, text_y), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  used.data());
  }
  ImGui::PopID();
}

[[nodiscard]] const CardInsertRowView* ChosenRow(const CardInsertView& view,
                                                 std::uint16_t selected) {
  for (const CardInsertRowView& row : view.rows) {
    if (selected != 0 && row.equip_slot == selected) {
      return &row;
    }
  }
  return nullptr;
}

// L'INFOBULLE SIMPLE, quand l'aperçu de description est éteint : le nom, et ce
// qui est déjà serti.
void DrawSimpleTooltip(const CardInsertRowView& row) {
  if (BeginSkinTooltip()) {
    const std::string_view name = row.name.empty() ? std::string_view("(?)") : row.name;
    ImGui::Text(" %.*s ", static_cast<int>(name.size()), name.data());
    if (row.used_slots > 0) {
      std::array<char, 64> used = {};
      std::snprintf(used.data(), used.size(), core::Tr("%d carte(s) sertie(s)"), row.used_slots);
      ImGui::TextDisabled(" %s ", used.data());
    }
    EndSkinTooltip();
  }
}

}  // namespace

CardInsertWindowAction DrawCardInsertWindow(CardInsertWindowState& state,
                                            const CardInsertView& view, const SkinArt& art) {
  CardInsertWindowAction action;
  if (!state.open) {
    // Réarmé ici aussi : une fermeture suivie d'une réouverture entre deux
    // images doit repasser au premier plan.
    state.was_open = false;
    return action;
  }
  // UNE LISTE NOUVELLE EFFACE LE CHOIX : une pièce choisie dans l'ancienne n'a
  // plus de sens, même si elle y figure encore.
  if (view.offer_seq != state.seen_offer_seq) {
    state.seen_offer_seq = view.offer_seq;
    state.selected = 0;
  }
  // Et une pièce qui a quitté la liste n'est plus choisie.
  if (ChosenRow(view, state.selected) == nullptr) {
    state.selected = 0;
  }

  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kSpawnPixels.x * scale, kSpawnPixels.y * scale),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstSizePixels.x * scale, kFirstSizePixels.y * scale),
                           ImGuiCond_FirstUseEver);
  if (!state.was_open) {
    ImGui::SetNextWindowFocus();
    state.was_open = true;
  }

  const CardInsertRowView* hovered = nullptr;
  static WindowName name(kCardInsertWindowTitle, kCardInsertWindowId);
  bool open = true;
  const bool begun =
      BeginSkinnedWindow(name.Get(), &open, art, SkinnedTitle::kBar, ImGuiWindowFlags_NoCollapse);
  if (begun) {
    // ── L'EN-TÊTE : la carte qu'on sertit, et combien il en reste ──────────
    if (view.card_found) {
      if (const ImTextureID icon = IconOf(view.card_icon); icon != 0) {
        ImGui::Image(icon, ImVec2(kIconPixels * scale, kIconPixels * scale));
        ImGui::SameLine();
      }
      ImGui::AlignTextToFramePadding();
      const std::string_view card =
          view.card_name.empty() ? std::string_view(core::Tr("(carte)")) : view.card_name;
      ImGui::TextUnformatted(card.data(), card.data() + card.size());
      ImGui::SameLine();
      ImGui::TextDisabled("(x%d)", view.stock);
    } else {
      ImGui::TextUnformatted(core::Tr("Carte introuvable"));
    }
    ImGui::Separator();

    if (view.rows.empty()) {
      // Deux vides qui ne disent pas la même chose : le serveur a listé des
      // pièces qu'on ne retrouve plus, ou sa liste n'en portait pas.
      ImGui::TextWrapped("%s", view.listed > 0
                                   ? core::Tr("Impossible de retrouver les équipements proposés "
                                              "par le serveur.")
                                   : core::Tr("Aucun équipement compatible avec un emplacement "
                                              "libre."));
    } else {
      ImGui::TextDisabled("%s", core::Tr("Choisissez l'équipement à sertir :"));
      const float footer = SkinButtonHeight(art) + ImGui::GetStyle().ItemSpacing.y * 2.0F;
      if (ImGui::BeginChild("##ci_list", ImVec2(0.0F, -footer), ImGuiChildFlags_Borders)) {
        for (const CardInsertRowView& row : view.rows) {
          DrawRow(row, state, action, hovered);
        }
      }
      ImGui::EndChild();
    }

    // ── LES BOUTONS ─────────────────────────────────────────────────────────
    //
    // Combien de fois d'affilée on peut sertir sur la pièce choisie : autant
    // de cases libres, et pas plus de cartes qu'on n'en a.
    //
    // ⚠ RELUE APRÈS LA LISTE : un clic de cette image-ci vient peut-être de
    // choisir, et les boutons doivent s'allumer sans attendre la suivante.
    const CardInsertRowView* chosen = ChosenRow(view, state.selected);
    const int free_slots =
        chosen != nullptr ? std::max(0, chosen->total_slots - chosen->used_slots) : 0;
    const int most = std::max(0, std::min(free_slots, view.stock));
    const bool can_insert = chosen != nullptr && most >= 1;
    if (!can_insert) {
      ImGui::BeginDisabled();
    }
    if (SkinButton(core::Tr("Sertir"), art)) {
      action.verb = CardInsertVerb::kInsert;
      action.equip_slot = state.selected;
      action.times = 1;
    }
    // « ×2 » à « ×4 », chacun seulement s'il est réalisable.
    for (int times = 2; times <= most && times <= kCardInsertMostTimes; ++times) {
      ImGui::SameLine();
      std::array<char, 8> label = {};
      std::snprintf(label.data(), label.size(), "x%d", times);
      if (SkinButton(label.data(), art)) {
        action.verb = CardInsertVerb::kInsert;
        action.equip_slot = state.selected;
        action.times = times;
      }
    }
    if (!can_insert) {
      ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Fermer"), art)) {
      action.verb = CardInsertVerb::kClose;
    }
  }
  EndSkinnedWindow();

  if (hovered != nullptr) {
    action.hovered_slot = hovered->equip_slot;
    action.hovered_id = hovered->item_id;
    if (!state.hover_preview) {
      DrawSimpleTooltip(*hovered);
    }
  }
  // LA CROIX : même effet que « Fermer ».
  if (!open) {
    action.verb = CardInsertVerb::kClose;
  }
  return action;
}

}  // namespace ro::ui
