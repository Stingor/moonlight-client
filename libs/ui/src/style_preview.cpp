#include "ro/ui/style_preview.h"

#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "palette_doll.h"

namespace ro::ui {
namespace {

// Le côté du pantin, en largeurs de police. ⚠ DÉRIVÉ DE LA POLICE et non d'un
// nombre de pixels : la taille du texte est un réglage du joueur, et un carré
// en pixels serait faux dès qu'il la change.
constexpr float kDollInFonts = 9.0F;
// Le repli du texte, en largeurs de police, et celui de l'infobulle.
constexpr float kWrapInFonts = 16.0F;
constexpr float kTipWrapInFonts = 20.0F;

}  // namespace

StylePreviewVerb DrawStylePreview(StylePreviewState& state, const StylePreviewView& view,
                                  const SkinArt& art) {
  StylePreviewVerb verb = StylePreviewVerb::kNothing;
  if (!state.open) {
    return verb;
  }

  static WindowName name(kStylePreviewTitle, kStylePreviewId);
  bool open = true;
  // ⚠ SANS REDIMENSIONNEMENT : la fenêtre suit son contenu, comme l'éditeur.
  const bool begun =
      BeginSkinnedWindow(name.Get(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (!open) {
    state.open = false;
  }
  if (!begun) {
    EndSkinnedWindow();
    return verb;
  }

  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kWrapInFonts);
  // ⛔ « SUR TOI », ET C'EST TOUT LE PROPOS : ce qu'on montre n'est pas le
  // personnage de l'autre, c'est SON STYLE SUR NOTRE CORPS. Une recette
  // s'applique par rang de pièce, et le résultat dépend du sprite qui la
  // reçoit — montrer autre chose serait montrer une apparence que personne
  // n'aura jamais.
  if (state.owner.empty()) {
    // ⚠ UNE SECONDE PHRASE PLUTÔT QU'UN POINT D'INTERROGATION : la balise
    // autorise une étiquette vide, et « Le style de ?, sur toi. » se lit comme
    // un défaut d'affichage.
    ImGui::TextUnformatted(core::Tr("Le style reçu, sur toi."));
  } else {
    char line[192] = {};
    std::snprintf(line, sizeof(line), core::Tr("Le style de %s, sur toi."), state.owner.c_str());
    ImGui::TextUnformatted(line);
  }
  ImGui::PopTextWrapPos();

  DrawPaletteDoll(state.doll, ImGui::GetFontSize() * kDollInFonts, art.look());
  if (view.bodyless) {
    // ⚠ ON LE DIT : un carré vide se lit comme un défaut d'affichage, alors
    // qu'il n'y a simplement pas de corps à peindre.
    ImGui::TextDisabled("%s", core::Tr("Rien à montrer : aucune pièce de ton corps ne se teint."));
  }

  if (SkinButton(core::Tr("Essayer dans l'éditeur"), art)) {
    verb = StylePreviewVerb::kTry;
    state.open = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * kTipWrapInFonts);
    SkinTooltip(
        core::Tr("Charge ce style dans l'éditeur. Ton personnage ne change qu'à la "
                 "validation."));
    ImGui::PopTextWrapPos();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Copier le code"), art)) {
    ImGui::SetClipboardText(state.code.c_str());
    verb = StylePreviewVerb::kCopy;
  }

  EndSkinnedWindow();
  return verb;
}

}  // namespace ro::ui
