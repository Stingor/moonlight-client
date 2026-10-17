// LE CURSEUR DE RÉGLAGE DU JEU — QUI EST SON ASCENSEUR HORIZONTAL.
//
// ⚠ RAGNAROK N'A PAS DE « SLIDER ». Son équivalent visuel est la barre de
// défilement horizontale, flèches comprises, et l'on reprend EXACTEMENT ses
// pièces plutôt que d'inventer une forme que le jeu n'a pas. Aucune rotation :
// elles sont déjà horizontales.
//
// ⚠ LE COMPORTEMENT RESTE CELUI D'IMGUI — `SliderBehavior`, et Ctrl+clic pour
// saisir la valeur au clavier. Seules les FLÈCHES et la peinture sont à nous.

#include <algorithm>
#include <array>
#include <cmath>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Les six pièces : la piste et ses deux flèches, la poignée en trois morceaux.
constexpr std::array<render::SkinPiece, 6> kSliderPieces = {
    render::SkinPiece::kSliderTrackLeft,  render::SkinPiece::kSliderTrackMiddle,
    render::SkinPiece::kSliderTrackRight, render::SkinPiece::kSliderGrabLeft,
    render::SkinPiece::kSliderGrabMiddle, render::SkinPiece::kSliderGrabRight};

// La hauteur d'une barre quand l'art manque : celle de l'ascenseur du jeu.
constexpr float kBarHeightPixels = 13.0F;
// La largeur d'un bout de poignée, même défaut.
constexpr float kGrabCapPixels = 4.0F;
// La plus petite poignée : en deçà, on ne l'attrape plus.
constexpr float kSmallestGrabPixels = 16.0F;
// ⚠ LA PISTE DÉBORDE SOUS LES FLÈCHES de deux pixels : sans ce recouvrement, la
// jointure laisse un trait clair que l'on voit à toutes les échelles.
constexpr float kTrackOverlapPixels = 2.0F;
// En dessous de cette place, les flèches ne tiennent plus et la piste prend tout.
constexpr float kRoomForArrowsPixels = 8.0F;
// Le pas d'une flèche, quand l'appelant n'en impose pas.
// Combien de décimales on suppose à un format qui n'en annonce pas — deux,
// comme ImGui. Un format sans précision est rare : il faut l'avoir écrit à la
// main.
constexpr int kDefaultFloatDecimals = 2;
constexpr int kDefaultIntStep = 1;
// Maintenir Majuscule multiplie le pas : c'est le geste du jeu.
constexpr float kShiftMultiplier = 10.0F;

[[nodiscard]] bool Paintable(const SkinArt& art) {
  return std::ranges::all_of(kSliderPieces,
                             [&art](render::SkinPiece piece) { return art.Get(piece).usable(); });
}

void Stretch(ImDrawList* into, const SkinImage& image, ImVec2 top_left, ImVec2 bottom_right,
             ImU32 tint) {
  if (image.usable() && bottom_right.x > top_left.x) {
    into->AddImage(image.id, top_left, bottom_right, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), tint);
  }
}

// La largeur que le NOMBRE occupe, réservée d'avance.
//
// ⚠ MESURÉE SUR LES BORNES, PAS SUR LA VALEUR COURANTE. Prise sur la valeur,
// elle changerait avec le nombre de chiffres et la barre se raccourcirait à
// chaque cran — une barre qui « respire ». La valeur entre quand même dans le
// calcul : un appelant peut passer une valeur hors bornes, et elle ne doit pas
// déborder.
[[nodiscard]] float ValueWidth(ImGuiDataType type, const void* value, const void* lowest,
                               const void* highest, const char* format) {
  std::array<char, 64> text{};
  const auto measure = [&text, type, format](const void* which) {
    const int length = ImGui::DataTypeFormatString(text.data(), static_cast<int>(text.size()), type,
                                                   which, format);
    return ImGui::CalcTextSize(text.data(), text.data() + length).x;
  };
  return std::max({measure(value), measure(lowest), measure(highest)});
}

// LE PAS QU'UNE BARRE AVANCE QUAND SON APPELANT N'EN DIT RIEN.
//
// 🔴 IL SE LIT DANS LE FORMAT, et c'est un défaut payé en jeu : un pas fixe
// d'un centième sur une barre qui AFFICHE des entiers donnait une molette
// asymétrique. Le réglage arrondit vers zéro en rangeant la valeur, donc
// 100 + 0,01 revenait à 100 — rien — tandis que 100 - 0,01 tombait à 99. Un
// cran vers le bas descendait, un cran vers le haut ne faisait rien.
//
// ⚠ ET UN CENTIÈME DE PIXEL N'A AUCUN SENS de toute façon. Le format est la
// seule chose qui dise l'unité du réglage : « %.0f px » compte en pixels,
// « %.2f » en centièmes. ImGui le lit déjà pour ses propres widgets.
[[nodiscard]] float StepFromFormat(ImGuiDataType type, const char* format) {
  if (type != ImGuiDataType_Float) {
    return static_cast<float>(kDefaultIntStep);
  }
  float step = 1.0F;
  for (int rank = ImParseFormatPrecision(format, kDefaultFloatDecimals); rank > 0; --rank) {
    step *= 0.1F;
  }
  return step;
}

// Un cran de flèche. Rend vrai si la valeur a bougé.
[[nodiscard]] bool StepByArrow(ImGuiDataType type, void* value, const void* lowest,
                               const void* highest, float step, float direction) {
  const float shift = ImGui::GetIO().KeyShift ? kShiftMultiplier : 1.0F;
  if (type == ImGuiDataType_Float) {
    auto* current = static_cast<float*>(value);
    const float by = step * shift;
    const float wanted = ImClamp(*current + direction * by, *static_cast<const float*>(lowest),
                                 *static_cast<const float*>(highest));
    if (wanted == *current) {
      return false;
    }
    *current = wanted;
    return true;
  }
  auto* current = static_cast<int*>(value);
  // ⚠ LE PAS D'UN ENTIER NE DESCEND PAS SOUS UN : un pas nul ferait une flèche
  // qui répond au clic sans rien changer, ce qui se lit « la flèche est cassée ».
  const int by = std::max(1, static_cast<int>(step * shift));
  const int wanted = ImClamp(*current + static_cast<int>(direction) * by,
                             *static_cast<const int*>(lowest), *static_cast<const int*>(highest));
  if (wanted == *current) {
    return false;
  }
  *current = wanted;
  return true;
}

[[nodiscard]] bool SkinSliderScalar(const char* label, const SkinArt& art, ImGuiDataType type,
                                    void* value, const void* lowest, const void* highest,
                                    const char* format, float arrow_step, SkinSliderWheel wheel) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window == nullptr || window->SkipItems) {
    return false;
  }
  ImGuiContext& context = *ImGui::GetCurrentContext();
  const ImGuiStyle& style = context.Style;
  const ImGuiID id = window->GetID(label);
  if (format == nullptr) {
    format = ImGui::DataTypeGetInfo(type)->PrintFmt;
  }

  // ⚠ RÉSOLU UNE FOIS, APRÈS LE FORMAT : les flèches et la molette doivent
  // avancer du MÊME pas, sinon la barre répond différemment selon le geste.
  const float step_size = arrow_step > 0.0F ? arrow_step : StepFromFormat(type, format);

  const float scale = Scale();
  const bool painted = Paintable(art);
  const SkinImage& arrow_art = art.Get(render::SkinPiece::kSliderTrackLeft);
  const float bar_height = (painted ? arrow_art.height : kBarHeightPixels) * scale;

  const char* label_end = ImGui::FindRenderedTextEnd(label);
  const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);
  const float value_width = ValueWidth(type, value, lowest, highest, format);
  const float width = ImGui::CalcItemWidth();
  const float frame_height = std::max(bar_height, label_size.y + style.FramePadding.y * 2.0F);

  const ImVec2 at = window->DC.CursorPos;
  const ImRect frame(at, ImVec2(at.x + width, at.y + frame_height));
  const float label_room = label_size.x > 0.0F ? style.ItemInnerSpacing.x + label_size.x : 0.0F;
  const ImRect whole(frame.Min, ImVec2(frame.Max.x + label_room, frame.Max.y));

  ImGui::ItemSize(whole, style.FramePadding.y);
  if (!ImGui::ItemAdd(whole, id, &frame, ImGuiItemFlags_Inputable)) {
    return false;
  }

  // La barre occupe tout sauf la place du nombre ; la piste, tout sauf les
  // flèches. ⚠ LA POIGNÉE NE PASSE JAMAIS SOUS UNE FLÈCHE, comme sur un
  // ascenseur.
  const float bar_top = std::floor(frame.Min.y + (frame_height - bar_height) * 0.5F);
  const ImRect bar(frame.Min.x, bar_top, frame.Max.x - value_width - style.ItemInnerSpacing.x,
                   bar_top + bar_height);
  const float arrow = (painted ? arrow_art.width * scale : bar_height);
  const bool with_arrows = bar.GetWidth() > arrow * 2.0F + kRoomForArrowsPixels * scale;
  const ImRect track(with_arrows ? bar.Min.x + arrow : bar.Min.x, bar.Min.y,
                     with_arrows ? bar.Max.x - arrow : bar.Max.x, bar.Max.y);

  const bool hovered = ImGui::ItemHoverable(frame, id, context.LastItemData.ItemFlags);
  const bool on_left = with_arrows && hovered &&
                       ImGui::IsMouseHoveringRect(bar.Min, ImVec2(track.Min.x, bar.Max.y), false);
  const bool on_right = with_arrows && hovered &&
                        ImGui::IsMouseHoveringRect(ImVec2(track.Max.x, bar.Min.y), bar.Max, false);

  // Activation, calquée sur `SliderScalar`. ⚠ UN CLIC SUR UNE FLÈCHE NE SAISIT
  // PAS LA POIGNÉE : il ne doit faire qu'un pas, sinon la valeur saute d'un bout
  // à l'autre au premier clic sur une flèche.
  bool typing = ImGui::TempInputIsActive(id);
  if (!typing) {
    const bool clicked =
        hovered && !on_left && !on_right && ImGui::IsMouseClicked(0, ImGuiInputFlags_None, id);
    const bool waking = clicked || context.NavActivateId == id;
    if (waking && clicked) {
      ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
    }
    if (waking && ((clicked && context.IO.KeyCtrl) ||
                   (context.NavActivateId == id &&
                    (context.NavActivateFlags & ImGuiActivateFlags_PreferInput) != 0))) {
      typing = true;
    }
    if (waking && !typing) {
      ImGui::SetActiveID(id, window);
      ImGui::SetFocusID(id, window);
      ImGui::FocusWindow(window);
      context.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }
  }
  if (typing) {
    // Ctrl+clic : on tape la valeur. C'est le geste d'ImGui, et il rend
    // atteignables des valeurs qu'aucun pixel de piste ne désigne.
    //
    // 🔴 ET LES BORNES PARTENT AVEC, SANS QUOI LA SAISIE N'EN A AUCUNE. Deux
    // `nullptr` à leur place laisseraient écrire n'importe quel nombre : le
    // réglage sortirait alors de la plage que la piste peut montrer, et le
    // curseur se retrouverait collé à un bout sans que rien ne dise pourquoi.
    // C'est l'effet de `ImGuiSliderFlags_AlwaysClamp` d'ImGui ; ici il n'y a
    // pas de drapeau à poser, on donne directement les deux bornes.
    //
    // ⚠ CE N'EST PAS UN CONFORT : `static_cast<int>` d'un flottant hors des
    // bornes d'un entier est un COMPORTEMENT INDÉFINI, et l'appelant range
    // presque toujours la valeur dans un entier.
    return ImGui::TempInputScalar(frame, id, label, type, value, format, lowest, highest);
  }

  bool changed = false;
  if ((on_left || on_right) && ImGui::IsMouseClicked(ImGuiMouseButton_Left, /*repeat=*/true)) {
    changed = StepByArrow(type, value, lowest, highest, step_size, on_left ? -1.0F : 1.0F);
  }

  // ── LA MOLETTE, SUR LA BARRE SURVOLÉE ────────────────────────────────────
  // Un cran vaut un cran de flèche, et Majuscule avance par dizaines : c'est
  // le seul geste qui permette d'ajuster sans viser la poignée.
  //
  // ⚠ ON S'APPROPRIE LA MOLETTE, ET À CHAQUE IMAGE. ImGui décide du défilement
  // au DÉBUT de l'image, avant que ce widget ne s'exécute : une possession
  // posée au moment où la molette tourne arriverait trop tard, et la barre de
  // défilement emporterait la page pendant qu'on règle. Demandée en permanence
  // dès le survol, elle vaut pour l'image suivante — celle où la molette
  // tournera.
  //
  // TODO : LE VERROU QUI MANQUE — PARCOURIR UNE PAGE DE RÉGLAGES NE DOIT
  // TOUCHER À AUCUNE BARRE TRAVERSÉE. S'approprier la molette dès le survol
  // règle le problème du défilement qui emporte la page PENDANT qu'on ajuste,
  // mais pas l'inverse : une barre effleurée en descendant la page prend le
  // cran. Résoudre les deux demande deux choses de plus que ce qu'on a ici :
  //
  //   un DÉLAI DE POSE   la barre ne prend la molette qu'après un court moment
  //                      de survol immobile — et le survol est NEUF aussi quand
  //                      l'image précédente ne survolait pas, sinon revenir sur
  //                      une barre quittée hérite de son ancien chrono ;
  //   un VERROU DE PAGE  dès qu'un défilement a VRAIMENT eu lieu (ImGui le note
  //                      lui-même dans `WheelingWindowScrolledFrame`), plus
  //                      aucune barre ne prend la molette pendant la salve.
  //                      Compter les crans ne suffirait pas : un cran sur une
  //                      fenêtre qui n'a rien à défiler ne doit rien verrouiller.
  //
  // Cela demande un état qui survit d'une image à l'autre, que ce widget n'a
  // pas : il vivra dans `UiContext`, qui est déjà ce qui tient l'image.
  //
  // ⚠ `SkinSliderWheel::kLeaves` : ni possession ni cran, la fenêtre défile.
  const bool takes_wheel = wheel == SkinSliderWheel::kTakes;
  const bool owns_wheel = takes_wheel && ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
  if (const float turned = context.IO.MouseWheel; owns_wheel && turned != 0.0F) {
    // ⚠ MAJUSCULE MULTIPLIE DANS `StepByArrow`, ET NULLE PART AILLEURS. Le
    // faire aussi ici multipliait DEUX FOIS — cent crans d'un coup au lieu de
    // dix.
    changed = StepByArrow(type, value, lowest, highest, step_size, turned > 0.0F ? 1.0F : -1.0F) ||
              changed;
  }

  ImRect grab;
  ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, kSmallestGrabPixels * scale);
  if (ImGui::SliderBehavior(track, id, type, value, lowest, highest, format, ImGuiSliderFlags_None,
                            &grab)) {
    changed = true;
  }
  ImGui::PopStyleVar();
  // 🔴 MARQUER L'ITEM ÉDITÉ N'EST PERMIS QU'À L'ITEM ACTIF — ou quand aucun
  // ne l'est. ImGui le vérifie par une assertion, et une assertion franchie TUE
  // le programme : c'est un plantage en jeu, pas un avertissement.
  //
  // ⚠ ET LA MOLETTE ÉDITE SANS SAISIR. Les flèches et la molette nous
  // atteignent dès le SURVOL, sans jamais nous rendre actifs — c'est voulu, un
  // cran ne doit pas empoigner la valeur. Il suffit alors qu'un autre widget
  // tienne l'item actif pour que le marquage devienne illégal. La condition
  // ci-dessous est celle de l'assertion elle-même.
  //
  // Ce qu'on perd en s'abstenant : `IsItemEdited` ne le dira pas pour cette
  // image-là. Nous rendons `changed` nous-mêmes, et c'est ce que nos appelants
  // lisent.
  if (changed && (context.ActiveId == id || context.ActiveId == 0)) {
    ImGui::MarkItemEdited(id);
  }

  // ── La peinture ──────────────────────────────────────────────────────────
  ImDrawList* into = window->DrawList;
  if (painted) {
    // GRISÉ COMME UN BOUTON : l'art s'efface sous `BeginDisabled`.
    const ImU32 tint = PaintedTint(art.look().slider_tint,
                                   (context.LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0);
    const float overlap = kTrackOverlapPixels * scale;
    Stretch(into, art.Get(render::SkinPiece::kSliderTrackMiddle),
            ImVec2(track.Min.x - overlap, bar.Min.y), ImVec2(track.Max.x + overlap, bar.Max.y),
            tint);
    if (with_arrows) {
      Stretch(into, art.Get(render::SkinPiece::kSliderTrackLeft), bar.Min,
              ImVec2(track.Min.x, bar.Max.y), tint);
      Stretch(into, art.Get(render::SkinPiece::kSliderTrackRight), ImVec2(track.Max.x, bar.Min.y),
              bar.Max, tint);
    }
    const SkinImage& cap_art = art.Get(render::SkinPiece::kSliderGrabLeft);
    const float cap = (cap_art.usable() ? cap_art.width : kGrabCapPixels) * scale;
    const float from = std::floor(grab.Min.x);
    const float to = std::floor(grab.Max.x);
    if (to > from) {
      Stretch(into, cap_art, ImVec2(from, bar.Min.y), ImVec2(from + cap, bar.Max.y), tint);
      Stretch(into, art.Get(render::SkinPiece::kSliderGrabRight), ImVec2(to - cap, bar.Min.y),
              ImVec2(to, bar.Max.y), tint);
      if (to - from > cap * 2.0F) {
        Stretch(into, art.Get(render::SkinPiece::kSliderGrabMiddle), ImVec2(from + cap, bar.Min.y),
                ImVec2(to - cap, bar.Max.y), tint);
      }
    }
  } else {
    into->AddRectFilled(bar.Min, bar.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
    into->AddRect(bar.Min, bar.Max, ImGui::GetColorU32(ImGuiCol_Border));
    if (grab.Max.x > grab.Min.x) {
      into->AddRectFilled(ImVec2(grab.Min.x, bar.Min.y), ImVec2(grab.Max.x, bar.Max.y),
                          ImGui::GetColorU32(ImGuiCol_SliderGrab));
    }
  }

  // Le nombre à DROITE de la barre, puis le libellé — comme un curseur d'ImGui.
  // ⚠ PAS AU CENTRE : une barre du jeu fait treize pixels de haut, le texte n'y
  // tient pas.
  std::array<char, 64> shown{};
  const int length = ImGui::DataTypeFormatString(shown.data(), static_cast<int>(shown.size()), type,
                                                 value, format);
  const ImVec2 size = ImGui::CalcTextSize(shown.data(), shown.data() + length);
  into->AddText(ImVec2(frame.Max.x - size.x, frame.Min.y + (frame_height - size.y) * 0.5F),
                ImGui::GetColorU32(ImGuiCol_Text), shown.data(), shown.data() + length);
  if (label_size.x > 0.0F) {
    ImGui::RenderText(
        ImVec2(frame.Max.x + style.ItemInnerSpacing.x, frame.Min.y + style.FramePadding.y), label,
        label_end, false);
  }

  // ⚠ TROIS GESTES SUR CETTE BARRE NE SE DEVINENT PAS. La poignée se voit ; la
  // molette, son pas de Majuscule et le Ctrl+clic qui ouvre la saisie ne se
  // voient nulle part, et personne ne les essaie sans qu'on les lui dise.
  SkinTooltip(takes_wheel ? core::Tr("- Flèches et molette règlent la valeur "
                                     "(Majuscule = pas plus grand)\n"
                                     "- Ctrl+clic pour taper la valeur.")
                          : core::Tr("- Flèches règlent la valeur "
                                     "(Majuscule = pas plus grand)\n"
                                     "- Ctrl+clic pour taper la valeur."));
  return changed;
}

}  // namespace

bool SkinSliderFloat(const char* label, float* value, float lowest, float highest,
                     const SkinArt& art, const char* format, float arrow_step) {
  return SkinSliderScalar(label, art, ImGuiDataType_Float, value, &lowest, &highest, format,
                          arrow_step, SkinSliderWheel::kTakes);
}

bool SkinSliderInt(const char* label, int* value, int lowest, int highest, const SkinArt& art,
                   const char* format, int arrow_step, SkinSliderWheel wheel) {
  return SkinSliderScalar(label, art, ImGuiDataType_S32, value, &lowest, &highest, format,
                          static_cast<float>(arrow_step), wheel);
}

}  // namespace ro::ui
