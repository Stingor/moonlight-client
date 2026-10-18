#pragma once

// LES PETITS TRACÉS DE TEXTE DE L'INTERFACE, en un seul endroit : poser un texte
// tel quel, le colorer, le griser, le mesurer, le filtrer, en souligner un lien.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <string_view>

#include <imgui.h>

namespace ro::ui {

// Pose une vue de texte telle quelle. Ce n'est pas un format : un « % »
// s'affiche. La borne est la fin de la vue, et non un octet nul : la vue n'a
// pas à être terminée par un nul, et un nul qu'elle contiendrait ne l'arrête
// pas. Elle ne pose aucun retour à la ligne d'elle-même, mais suit le repli
// qu'un PushTextWrapPos extérieur a posé.
inline void Unformatted(std::string_view text) {
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

// Pose un texte tel quel dans une couleur : la couleur est poussée et retirée
// ENSEMBLE, autour de ce seul texte. Ce n'est pas un format, et le texte
// s'arrête à son premier octet nul.
inline void ColoredText(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

// La même sur une vue : la borne est la fin de la vue, comme pour
// `Unformatted`, et non un octet nul. ⚠ Un `const char*` nul n'y est pas admis.
inline void ColoredText(ImU32 color, std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  Unformatted(text);
  ImGui::PopStyleColor();
}

// La même pour une couleur en flottants, sur une vue. ⚠ Un `const char*` nul
// n'y est pas admis ; un `const char*` valide y est borné par son premier nul.
//
// LES SURCHARGES SE DÉPARTAGENT D'ABORD PAR LA COULEUR, car rien ne convertit
// implicitement un ImU32 en ImVec4 ni l'inverse ; puis, pour un ImU32, par le
// texte : un `const char*` prend la première, une vue ou une `std::string` la
// deuxième. Un ImColor, qui sait devenir l'un et l'autre, se convertit d'abord.
inline void ColoredText(const ImVec4& color, std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  Unformatted(text);
  ImGui::PopStyleColor();
}

// Un texte replié, dans une couleur poussée et retirée ENSEMBLE. Il passe par
// `"%s"` et non en format : un « % » du texte s'affiche. Il s'arrête à son
// premier octet nul. Un repli déjà posé par l'appelant (`PushTextWrapPos`) est
// gardé ; sans lui, le texte se replie au bord de la fenêtre.
inline void ColoredWrapped(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

// LE TEXTE SECONDAIRE DE L'HABILLAGE : ce qui informe sans rien demander.
//
// ⚠ DANS LA COULEUR QUE LA PALETTE DE LA FENÊTRE POSE EN `TextDisabled` — et
// non le gris éteint d'ImGui, illisible sur un corps clair. Même rendu que
// `ImGui::TextDisabled("%s", text)` pour un texte non nul ; un pointeur nul
// dessine ici une ligne vide. Ce n'est pas un format, et le texte s'arrête à
// son premier octet nul.
inline void FaintText(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

// La même sur une vue : la borne est la fin de la vue, et non un octet nul.
// ⚠ Un `const char*` nul n'y est pas admis.
inline void FaintText(std::string_view text) {
  ColoredText(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), text);
}

// La largeur d'une vue de texte dans la police et à la taille COURANTES
// d'ImGui, en pixels d'écran, arrondie au pixel supérieur comme ImGui le fait.
// La borne est la fin de la vue, et non un octet nul, comme pour
// `Unformatted`. Une vue vide mesure zéro. ⚠ Une police ou une taille imposée
// passe par `ImFont::CalcTextSizeA`, qui n'arrondit pas : ce n'est pas cette
// mesure.
[[nodiscard]] inline float TextWidth(std::string_view text) {
  return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
}

// Le filtre de recherche retient-il ce texte ? ⚠ PAR SES BORNES, jamais par
// `c_str()` ni par une copie : une vue n'a pas d'octet nul garanti, et une
// liste se refiltre à chaque image. Un filtre vide retient tout.
[[nodiscard]] inline bool PassesTextFilter(const ImGuiTextFilter& filter, std::string_view text) {
  return filter.PassFilter(text.data(), text.data() + text.size());
}

// UN LIEN SURVOLÉ : la main, et un trait dans la couleur des liens de
// l'habillage sous l'item qu'on vient de poser. Rien hors survol.
//
// ⚠ LE TRAIT SE POSE SUR LE BAS DU RECTANGLE DE L'ITEM, et non à une distance
// choisie sous la ligne de base : la hauteur d'une ligne suit l'échelle de
// l'interface, un écart en dur ne la suivrait pas. Sa couleur porte l'opacité
// du style (`style.Alpha`), comme le texte du lien qu'il souligne.
inline void DecorateLink(bool hovered) {
  if (!hovered) {
    return;
  }
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const ImVec2 least = ImGui::GetItemRectMin();
  const ImVec2 most = ImGui::GetItemRectMax();
  ImGui::GetWindowDrawList()->AddLine(ImVec2(least.x, most.y), ImVec2(most.x, most.y),
                                      ImGui::GetColorU32(ImGuiCol_TextLink));
}

}  // namespace ro::ui
