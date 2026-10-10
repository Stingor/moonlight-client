#include "ro/ui/log_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// La couleur d'une gravité. L'alerte et l'erreur sautent aux yeux ; le détail
// s'efface, parce qu'on le lit en le cherchant et jamais en passant.
[[nodiscard]] ImVec4 ColorOf(core::LogLevel level) {
  switch (level) {
    case core::LogLevel::kDebug:
      return {0.55F, 0.55F, 0.58F, 1.0F};
    case core::LogLevel::kInfo:
      return {0.85F, 0.86F, 0.88F, 1.0F};
    case core::LogLevel::kWarning:
      return {0.95F, 0.75F, 0.30F, 1.0F};
    case core::LogLevel::kError:
      return {0.95F, 0.40F, 0.38F, 1.0F};
  }
  return {1.0F, 1.0F, 1.0F, 1.0F};
}

// « 12.480 » : les secondes depuis le démarrage, au millième.
//
// ⚠ DEPUIS LE DÉMARRAGE, et non l'heure du jour : ce qu'on cherche dans ce
// journal est un ÉCART — combien de temps sépare deux lignes, où une pause a
// eu lieu —, et une heure murale oblige à faire la soustraction de tête.
constexpr std::size_t kStampSize = 16;

[[nodiscard]] std::array<char, kStampSize> StampOf(std::uint32_t since_start_ms) {
  std::array<char, kStampSize> text = {};
  std::snprintf(text.data(), text.size(), "%u.%03u", since_start_ms / 1000U,
                since_start_ms % 1000U);
  return text;
}

// CE QU'UN CLIC FAIT À LA SÉLECTION, selon les touches tenues.
//
// Les trois gestes que tout le monde connaît, et aucun de plus : un clic
// remplace, Ctrl ajoute ou retire, Maj étend depuis la dernière ligne cliquée.
void ClickLine(LogWindowState& state, std::span<const core::LoggedLine> shown,
               std::uint64_t serial) {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.KeyShift && state.anchor != 0) {
    // ⚠ L'ÉTENDUE SE PREND DANS CE QUI EST MONTRÉ, pas dans les numéros : entre
    // deux lignes visibles s'en trouvent d'autres que le filtre a écartées, et
    // les prendre copierait ce qu'on ne voit pas.
    const auto first = std::ranges::find(shown, state.anchor, &core::LoggedLine::serial);
    const auto last = std::ranges::find(shown, serial, &core::LoggedLine::serial);
    if (first != shown.end() && last != shown.end()) {
      const auto from = std::min(first, last);
      const auto to = std::max(first, last);
      for (auto step = from; step <= to; ++step) {
        state.selected.insert(step->serial);
      }
      return;
    }
  }
  if (io.KeyCtrl) {
    if (!state.selected.insert(serial).second) {
      state.selected.erase(serial);
    }
  } else {
    state.selected = {serial};
  }
  state.anchor = serial;
}

// LE NOM D'UNE GRAVITÉ SUR SA CASE À COCHER.
//
// ⚠ UN SWITCH DE LITTÉRAUX, ET NON `Tr(core::ToString(niveau))`. `Tr` n'accepte
// qu'un littéral — une chaîne rendue par une fonction n'est dans aucun
// catalogue, et le relevé qui bâtit les catalogues ne peut pas la voir. Écrire
// les quatre à la main les rend VISIBLES à ce relevé, ce qu'une table indexée
// n'aurait pas fait davantage.
//
// `core::ToString` reste ce qu'elle est : le nom pour le JOURNAL, qui ne se
// traduit pas.
[[nodiscard]] const char* LevelLabel(core::LogLevel level) {
  switch (level) {
    case core::LogLevel::kDebug:
      return core::Tr("détail");
    case core::LogLevel::kInfo:
      return core::Tr("info");
    case core::LogLevel::kWarning:
      return core::Tr("alerte");
    case core::LogLevel::kError:
      return core::Tr("erreur");
  }
  return "";
}

// Les cases à cocher des gravités, dans l'ordre de `core::LogLevel`.
constexpr std::array<core::LogLevel, 4> kLevels = {core::LogLevel::kDebug, core::LogLevel::kInfo,
                                                   core::LogLevel::kWarning,
                                                   core::LogLevel::kError};

// Le filtre tel qu'il est saisi, borné à ce que le tampon porte vraiment.
//
// ⚠ LA FIN SE CHERCHE, ELLE NE SE SUPPOSE PAS : un tampon qu'ImGui n'aurait
// pas terminé — ou qu'un état fabriqué à la main aurait rempli entièrement —
// ferait lire au-delà avec un `strlen`. On borne à la taille du tableau.
[[nodiscard]] std::string_view TypedFilter(const std::array<char, kLogFilterSize>& filter) {
  const auto stop = std::ranges::find(filter, '\0');
  return {filter.data(), static_cast<std::size_t>(stop - filter.begin())};
}

}  // namespace

std::string JoinLogLines(std::span<const core::LoggedLine> lines) {
  std::string joined;
  for (const core::LoggedLine& line : lines) {
    joined += StampOf(line.since_start_ms).data();
    joined += ' ';
    joined += line.text;
    joined += '\n';
  }
  return joined;
}

std::vector<core::LoggedLine> SelectedLogLines(std::span<const core::LoggedLine> shown,
                                               const LogWindowState& state) {
  std::vector<core::LoggedLine> chosen;
  for (const core::LoggedLine& line : shown) {
    if (state.selected.contains(line.serial)) {
      chosen.push_back(line);
    }
  }
  return chosen;
}

bool LinePassesFilter(std::string_view text, std::string_view filter) {
  if (filter.empty()) {
    return true;
  }
  if (filter.size() > text.size()) {
    return false;
  }
  // Recherche naïve, et c'est assez : le texte fait quelques dizaines
  // d'octets, la liste quelques milliers de lignes, et le filtre ne se
  // réévalue qu'à l'image où la fenêtre est ouverte.
  for (std::size_t start = 0; start + filter.size() <= text.size(); ++start) {
    bool same = true;
    for (std::size_t index = 0; index < filter.size(); ++index) {
      if (core::AsciiLower(static_cast<std::uint8_t>(text[start + index])) !=
          core::AsciiLower(static_cast<std::uint8_t>(filter[index]))) {
        same = false;
        break;
      }
    }
    if (same) {
      return true;
    }
  }
  return false;
}

std::vector<core::LoggedLine> SelectLogLines(std::span<const core::LoggedLine> lines,
                                             const LogWindowState& state) {
  std::vector<core::LoggedLine> kept;
  kept.reserve(lines.size());
  const std::string_view wanted = TypedFilter(state.filter);
  for (const core::LoggedLine& line : lines) {
    const auto rank = static_cast<std::size_t>(line.level);
    if (rank >= state.shown.size() || !state.shown[rank]) {
      continue;
    }
    if (!LinePassesFilter(line.text, wanted)) {
      continue;
    }
    kept.push_back(line);
  }
  // AUCUN TRI : l'ordre d'écriture EST l'information. Trier par gravité
  // séparerait une erreur de la ligne qui l'explique, laquelle la précède
  // presque toujours.
  return kept;
}

std::size_t DrawLogWindow(LogWindowState& state, std::span<const core::LoggedLine> lines) {
  if (!state.open) {
    return 0;
  }
  // Recomposé au changement de langue seulement : voir `WindowName`.
  static WindowName name(kLogWindowTitle, kLogWindowId);
  ConstrainNextWindowToScreen(name.Get());
  if (!ImGui::Begin(name.Get(), &state.open)) {
    ImGui::End();
    return 0;
  }

  for (std::size_t rank = 0; rank < kLevels.size(); ++rank) {
    if (rank > 0) {
      ImGui::SameLine();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ColorOf(kLevels[rank]));
    ImGui::Checkbox(LevelLabel(kLevels[rank]), &state.shown[rank]);
    ImGui::PopStyleColor();
  }
  ImGui::SameLine();
  ImGui::Checkbox(core::Tr("suivre"), &state.follow);

  // LE FILTRE SUR SA PROPRE LIGNE, en pleine largeur : ce qu'on y tape est
  // souvent un fragment de message, pas un mot.
  ImGui::SetNextItemWidth(-ImGui::CalcTextSize(core::Tr("effacer")).x -
                          ImGui::GetStyle().ItemSpacing.x * 3);
  ImGui::InputTextWithHint("##filtre", core::Tr("filtrer les lignes..."), state.filter.data(),
                           state.filter.size());
  ImGui::SameLine();
  if (ImGui::Button(core::Tr("effacer"))) {
    state.filter = {};
  }

  const std::vector<core::LoggedLine> shown = SelectLogLines(lines, state);
  const std::vector<core::LoggedLine> chosen = SelectedLogLines(shown, state);

  // ⚠ DEUX BOUTONS, ET NON UN SEUL QUI DEVINE. Un bouton « copier » dont le
  // sens change selon qu'une ligne est choisie oblige à vérifier l'état avant de
  // cliquer ; deux boutons disent chacun ce qu'ils font.
  ImGui::BeginDisabled(chosen.empty());
  if (ImGui::Button(core::Tr("copier la sélection"))) {
    ImGui::SetClipboardText(JoinLogLines(chosen).c_str());
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(core::Tr("tout copier"))) {
    // CE QUI EST MONTRÉ, et non tout le journal : les filtres sont là pour
    // réduire, et copier ce qu'ils viennent d'écarter irait contre le geste.
    ImGui::SetClipboardText(JoinLogLines(shown).c_str());
  }
  ImGui::SameLine();
  ImGui::TextDisabled(core::Tr("%zu ligne(s), %zu choisie(s)"), shown.size(), chosen.size());
  ImGui::Separator();
  if (ImGui::BeginChild("lignes", ImVec2(0, 0), ImGuiChildFlags_None,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    if (shown.empty()) {
      // Sans ce mot, un filtre qui ne trouve rien donne une fenêtre VIDE, ce
      // qui ressemble à une panne plutôt qu'à un résultat.
      ImGui::PushStyleColor(ImGuiCol_Text, ColorOf(core::LogLevel::kDebug));
      ImGui::TextUnformatted(lines.empty() ? core::Tr("Le journal est vide.")
                                           : core::Tr("Aucune ligne ne passe le filtre."));
      ImGui::PopStyleColor();
    }
    for (const core::LoggedLine& line : shown) {
      // LA LIGNE ENTIÈRE EST CLIQUABLE, sur toute la largeur : viser le texte
      // exact d'une ligne de journal, qui n'a pas deux fois la même longueur,
      // serait pénible.
      const bool chosen_here = state.selected.contains(line.serial);
      // ⚠ L'IDENTIFIANT VIENT DU NUMÉRO DE LIGNE, ET NON DE L'ÉTIQUETTE. ImGui
      // nomme chaque élément d'après son texte, et l'étiquette est ici
      // l'horodatage : une rafale de messages arrive dans la même milliseconde,
      // donc douze lignes portaient le même nom et devenaient le MÊME élément.
      // ImGui l'a signalé lui-même en jeu — « 9 visible items with conflicting
      // ID » — et la conséquence était que cliquer sur l'une en sélectionnait une
      // autre.
      //
      // Le numéro d'écriture ne se réemploie jamais : c'est déjà ce qui fait
      // tenir la sélection, et cela fait aussi un identifiant unique.
      ImGui::PushID(static_cast<int>(line.serial));
      ImGui::PushStyleColor(ImGuiCol_Text, ColorOf(core::LogLevel::kDebug));
      if (ImGui::Selectable(StampOf(line.since_start_ms).data(), chosen_here,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        ClickLine(state, shown, line.serial);
      }
      ImGui::PopStyleColor();
      ImGui::PopID();
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Text, ColorOf(line.level));
      ImGui::TextUnformatted(line.text.c_str());
      ImGui::PopStyleColor();
    }
    // ⚠ ON NE REDESCEND QUE SI L'ON ÉTAIT DÉJÀ EN BAS. Sans cette condition, la
    // fenêtre arracherait la lecture à chaque ligne écrite, et le suivi
    // deviendrait une gêne au lieu d'un service.
    if (state.follow && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0F);
    }
  }
  ImGui::EndChild();

  ImGui::End();
  return shown.size();
}

}  // namespace ro::ui
