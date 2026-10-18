#include "ro/ui/status_cell.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// La minute, en secondes et en millisecondes : un compte à rebours change de
// forme au-delà.
constexpr std::uint32_t kMsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;
constexpr std::uint32_t kSecondsPerHour = 3600;

// De quoi écrire « 999 min » et le mot le plus long d'une traduction.
constexpr std::size_t kLongRemainingRoom = 32;

// Ce dont le fond du compte à rebours déborde du texte, et son arrondi.
constexpr float kCountdownPadding = 2.0F;
constexpr float kCountdownRounding = 2.0F;

// Ce dont l'image se retire des bords de la case.
constexpr float kIconInset = 0.0F;

// Le cerné du texte, en pixels : un compte à rebours se lit sur n'importe quelle
// icône, claire ou sombre.
constexpr float kTextShadow = 1.0F;

// La part de la case qu'une abréviation occupe en hauteur.
constexpr float kAbbreviationHeightShare = 0.55F;

// Le balayage horaire, en segments. Assez pour que le bord ne se voie pas
// facetté à seize pixels, et pour que son avance ne saute pas : c'est par
// segment ENTIER qu'il progresse.
constexpr int kRadialSegments = 48;

// Le rayon du balayage, en parts du côté de la case.
//
// 🔴 IL DÉBORDE VOLONTAIREMENT, et il le doit : la demi-diagonale vaut 0,707
// côté, donc un rayon plus court laisserait les quatre coins clairs alors que le
// secteur les a dépassés. C'est le CLIP qui rend au balayage sa forme carrée.
constexpr float kSweepRadiusShare = 0.78F;
constexpr float kQuarterTurn = 1.5707963F;
constexpr float kFullTurn = 6.2831853F;

[[nodiscard]] ImU32 Faded(ImU32 color, float alpha) {
  if (alpha >= 1.0F) {
    return color;
  }
  const float kept = std::clamp(alpha, 0.0F, 1.0F);
  const auto opacity =
      static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) * kept);
  return (color & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);
}

// « 42 » sous la minute, « 2:05 » au-delà. Le format du client, à la lettre.
void WriteRemaining(std::uint32_t remaining_ms, char* into, std::size_t room) {
  const std::uint32_t seconds = remaining_ms / kMsPerSecond;
  if (seconds < kSecondsPerMinute) {
    std::snprintf(into, room, "%u", seconds);
    return;
  }
  std::snprintf(into, room, "%u:%02u", seconds / kSecondsPerMinute, seconds % kSecondsPerMinute);
}

// Le texte cerné : une ombre d'un pixel, puis le texte.
void PaintOutlined(ImDrawList& into, ImFont* font, float height, ImVec2 at, const char* text,
                   float alpha) {
  into.AddText(font, height, ImVec2(at.x + kTextShadow, at.y + kTextShadow),
               Faded(IM_COL32(0, 0, 0, 190), alpha), text);
  into.AddText(font, height, at, Faded(IM_COL32_WHITE, alpha), text);
}

// « 2 minute(s) 53 second(s) », le format LONG de l'infobulle.
//
// ⚠ CES MOTS SONT CEUX DU CLIENT, parenthèses comprises : c'est ce que le joueur
// lit déjà partout ailleurs, et « 2 min 53 s » se serait lu comme un second jeu.
//
// 🔴 ET NON CELUI DE LA CASE. Sous une icône il faut deux ou trois caractères, et
// « 2:53 » suffit ; dans une infobulle on LIT, et une durée de deux heures
// écrite « 120:04 » demande un calcul. Ce sont deux besoins, donc deux formats —
// c'est aussi ce que fait le client.
//
// ⚠ LES SECONDES S'ÉCRIVENT MÊME QUAND TOUT EST À ZÉRO : « 0 s » se lit, une
// ligne vide non.
[[nodiscard]] std::string LongRemaining(std::uint32_t remaining_ms) {
  const std::uint32_t seconds = remaining_ms / kMsPerSecond;
  const std::uint32_t hours = seconds / kSecondsPerHour;
  const std::uint32_t minutes = (seconds % kSecondsPerHour) / kSecondsPerMinute;
  const std::uint32_t rest = seconds % kSecondsPerMinute;

  std::array<char, kLongRemainingRoom> piece{};
  std::string written;
  // ⚠ LE FORMAT S'ÉCRIT À L'APPEL, sous `Tr` : passé en paramètre, il ne se
  // vérifierait plus contre sa valeur.
  const auto add = [&piece, &written]() {
    if (!written.empty()) {
      written += ' ';
    }
    written += piece.data();
  };
  if (hours > 0) {
    std::snprintf(piece.data(), piece.size(), core::Tr("%u heure(s)"), hours);
    add();
  }
  if (minutes > 0) {
    std::snprintf(piece.data(), piece.size(), core::Tr("%u minute(s)"), minutes);
    add();
  }
  if (rest > 0 || written.empty()) {
    std::snprintf(piece.data(), piece.size(), core::Tr("%u seconde(s)"), rest);
    add();
  }
  return written;
}

// ── LE SKIN DE L'INFOBULLE, QU'ELLE IMPOSE ──────────────────────────────────
//
// 🔴 ELLE N'HÉRITE PAS DU BON. Une infobulle ImGui prend le style de la fenêtre
// D'OÙ ON SURVOLE : dans une fenêtre du jeu elle est claire, mais la barre
// d'états est posée SUR LE MONDE et ne pousse aucun style — elle héritait donc du
// gris sombre d'ImGui, avec du texte blanc, quand toutes nos autres infobulles
// sont claires. La MÊME infobulle avait deux aspects selon l'endroit d'où on la
// regardait.
//
// ⚠ ET LA GÉOMÉTRIE AVEC, pour la même raison : la barre pousse une marge NULLE
// et aucune bordure — ce que veut un cadre collé à ses icônes, mais pas une
// infobulle, dont le texte se serait collé au bord sans filet ni coins.
constexpr float kTooltipPadding = 6.0F;
constexpr float kTooltipRounding = 3.0F;
// Quatre couleurs et trois variables, à dépiler sur CHAQUE sortie.
constexpr int kTooltipColors = 4;
constexpr int kTooltipStyleVars = 3;

void PushTooltipSkin() {
  const float scale = Scale();
  ImGui::PushStyleColor(ImGuiCol_PopupBg, kSkinPopupColor);
  ImGui::PushStyleColor(ImGuiCol_Text, kSkinTextColor);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, kSkinFaintTextColor);
  ImGui::PushStyleColor(ImGuiCol_Border, kSkinBorderColor);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kTooltipPadding * scale, kTooltipPadding * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kTooltipRounding * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0F);
}

void PopTooltipSkin() {
  ImGui::PopStyleVar(kTooltipStyleVars);
  ImGui::PopStyleColor(kTooltipColors);
}

}  // namespace

void PaintStatusSweep(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, float spent,
                      ImU32 color) {
  const int used =
      static_cast<int>(static_cast<float>(kRadialSegments) * std::clamp(spent, 0.0F, 1.0F) + 0.5F);
  if (used <= 0) {
    return;
  }
  const ImVec2 middle((top_left.x + bottom_right.x) * 0.5F, (top_left.y + bottom_right.y) * 0.5F);
  const float radius = (bottom_right.x - top_left.x) * kSweepRadiusShare;

  const ImDrawListFlags kept = into.Flags;
  into.Flags &= ~ImDrawListFlags_AntiAliasedFill;
  into.PushClipRect(top_left, bottom_right, true);
  for (int step = 0; step < used; ++step) {
    const float from =
        -kQuarterTurn + kFullTurn * static_cast<float>(step) / static_cast<float>(kRadialSegments);
    const float to = -kQuarterTurn +
                     kFullTurn * static_cast<float>(step + 1) / static_cast<float>(kRadialSegments);
    into.AddTriangleFilled(
        middle, ImVec2(middle.x + std::cos(from) * radius, middle.y + std::sin(from) * radius),
        ImVec2(middle.x + std::cos(to) * radius, middle.y + std::sin(to) * radius), color);
  }
  into.PopClipRect();
  into.Flags = kept;
}

void DrawStatusTooltip(const StatusCellView& cell) {
  PushTooltipSkin();
  ImGui::BeginTooltip();

  // Le nom. Sans lui, on montre au moins de QUOI on parle : un carré vide ne se
  // cherche pas.
  Unformatted(cell.name.empty() ? std::string_view(core::Tr("État sans nom")) : cell.name);

  // ⚠ ZÉRO N'EST PAS UNE DURÉE ÉCOULÉE : un état sans échéance ne finit pas, et
  // écrire « 0 s » le ferait croire terminé.
  const std::string left = cell.remaining_ms == 0 ? std::string(core::Tr("Sans durée"))
                                                  : LongRemaining(cell.remaining_ms);

  // 🔴 LA DURÉE VA À LA PLACE QUE LA TABLE LUI RÉSERVE, au milieu de la
  // description et non en tête : c'est la mise en page du client, et deux
  // infobulles du même jeu qui se liraient différemment seraient un défaut à
  // elles seules. Sans description, elle passe juste sous le nom — il faut bien
  // qu'elle soit quelque part.
  if (cell.description.empty()) {
    ImGui::TextDisabled("%s", left.c_str());
    ImGui::EndTooltip();
    PopTooltipSkin();
    return;
  }

  const auto slot = static_cast<std::size_t>(
      std::clamp(cell.time_line, 0, static_cast<int>(cell.description.size())));
  ImGui::Separator();
  for (std::size_t rank = 0; rank < cell.description.size(); ++rank) {
    if (rank == slot) {
      ImGui::TextDisabled("%s", left.c_str());
    }
    Unformatted(cell.description[rank]);
  }
  // La place était en DERNIÈRE position : la boucle ne l'a pas atteinte.
  if (slot >= cell.description.size()) {
    ImGui::TextDisabled("%s", left.c_str());
  }
  ImGui::EndTooltip();
  PopTooltipSkin();
}

bool DrawStatusCell(const StatusCellView& cell, ImVec2 top_left, ImVec2 bottom_right,
                    const StatusCellStyle& style, bool tooltip) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  if (into == nullptr) {
    return false;
  }
  const float width = bottom_right.x - top_left.x;
  const float height = bottom_right.y - top_left.y;
  if (width <= 0.0F || height <= 0.0F) {
    return false;
  }

  if (cell.icon.usable()) {
    if ((style.icon_background & IM_COL32_A_MASK) != 0) {
      into->AddRectFilled(ImVec2(top_left.x + kIconInset, top_left.y + kIconInset),
                          ImVec2(bottom_right.x - kIconInset, bottom_right.y - kIconInset),
                          Faded(style.icon_background, style.alpha));
    }
    into->AddImage(cell.icon.id, ImVec2(top_left.x + kIconInset, top_left.y + kIconInset),
                   ImVec2(bottom_right.x - kIconInset, bottom_right.y - kIconInset), ImVec2(0, 0),
                   ImVec2(1, 1), Faded(IM_COL32_WHITE, style.alpha));
  } else if (!cell.abbreviation.empty()) {
    // ⚠ CE REPLI N'EST PAS DÉCORATIF : deux altérations que le joueur veut voir
    // — l'aveuglement et le saignement — n'ont d'image dans AUCUNE table du
    // client. Sans lui, elles seraient invisibles.
    ImFont* font = ImGui::GetFont();
    const float text_height = height * kAbbreviationHeightShare;
    const std::string label(cell.abbreviation);
    const ImVec2 size = font->CalcTextSizeA(text_height, FLT_MAX, 0.0F, label.c_str());
    PaintOutlined(
        *into, font, text_height,
        ImVec2(top_left.x + (width - size.x) * 0.5F, top_left.y + (height - size.y) * 0.5F),
        label.c_str(), style.alpha);
  } else {
    // Rien à peindre : ni image, ni mot. La place ne se prend pas.
    return false;
  }

  // ⚠ UN ÉTAT SANS ÉCHÉANCE N'A PAS DE PART ÉCOULÉE : on ne le voile pas. Le
  // griser à moitié le ferait croire à mi-course, alors qu'il ne finira pas.
  if (style.sweep != StatusSweep::kNone && cell.remaining_ms != 0 && cell.total_ms > 0) {
    const float left = static_cast<float>(cell.remaining_ms);
    const float spent = std::clamp(1.0F - left / static_cast<float>(cell.total_ms), 0.0F, 1.0F);
    const ImU32 veil = Faded(style.sweep_color, style.alpha);
    if (style.sweep == StatusSweep::kRadial) {
      PaintStatusSweep(*into, top_left, bottom_right, spent, veil);
    } else {
      // Le voile DESCEND : la part sombre coiffe la case, et le clair qui reste
      // en bas est ce qu'il reste de temps.
      into->AddRectFilled(top_left, ImVec2(bottom_right.x, top_left.y + height * spent), veil);
    }
  }

  if (style.countdown_pixels > 0.0F && cell.remaining_ms != 0) {
    char written[16] = {};
    WriteRemaining(cell.remaining_ms, written, sizeof(written));
    ImFont* font = ImGui::GetFont();
    const ImVec2 size = font->CalcTextSizeA(style.countdown_pixels, FLT_MAX, 0.0F, written);
    // 🔴 SOUS L'ICÔNE, ET NON DEDANS. Le réglage s'appelle « temps restant SOUS
    // l'icône » : posé à l'intérieur, il mange les pixels du bas — souvent ceux
    // qui distinguent deux icônes voisines — et le libellé ment.
    //
    // ⚠ LA PLACE EST RÉSERVÉE PAR LA RANGÉE, pas prise ici : la fenêtre découpe
    // à son bord, et un texte posé hors du cadre serait simplement invisible.
    const ImVec2 at(top_left.x + (width - size.x) * 0.5F, bottom_right.y);
    if ((style.countdown_background & IM_COL32_A_MASK) != 0) {
      const float pad = kCountdownPadding;
      into->AddRectFilled(ImVec2(at.x - pad, at.y), ImVec2(at.x + size.x + pad, at.y + size.y),
                          Faded(style.countdown_background, style.alpha), kCountdownRounding);
    }
    PaintOutlined(*into, font, style.countdown_pixels, at, written, style.alpha);
  }

  if (tooltip && ImGui::IsMouseHoveringRect(top_left, bottom_right)) {
    DrawStatusTooltip(cell);
  }
  return true;
}

std::vector<ImVec2> PlaceStatusRow(std::size_t count, ImVec2 origin,
                                   const StatusRowLayout& layout) {
  std::vector<ImVec2> places;
  const int rows = std::max(1, layout.rows);
  const auto most = static_cast<std::size_t>(std::max(0, layout.most));
  const std::size_t kept = std::min(count, most);
  if (kept == 0) {
    return places;
  }

  // Le plafond réparti sur les lignes, arrondi vers le haut : huit cases sur
  // trois lignes font trois, trois, deux.
  const auto per_row = static_cast<std::size_t>((static_cast<int>(most) + rows - 1) / rows);
  places.reserve(kept);
  const float step = layout.side + layout.gap;
  // ⚠ LE PAS VERTICAL N'EST PAS L'HORIZONTAL : il porte en plus la place
  // réservée sous chaque case, faute de quoi le compte à rebours d'une ligne
  // tomberait sur les icônes de la suivante.
  const float down = layout.side + layout.gap + layout.below;
  // La ligne se compte à part : quand c'est le bord qui force le retour, une
  // ligne se termine avant son quota.
  std::size_t row = 0;
  std::size_t column = 0;
  for (std::size_t rank = 0; rank < kept; ++rank) {
    bool past_limit = false;
    if (layout.limit != 0.0F && column > 0) {
      const float pen = layout.right_to_left ? origin.x - static_cast<float>(column) * step
                                             : origin.x + static_cast<float>(column) * step;
      past_limit = layout.right_to_left ? pen - layout.side < layout.limit
                                        : pen + layout.side > layout.limit;
    }
    if (column >= per_row || past_limit) {
      if (row + 1 >= static_cast<std::size_t>(rows)) {
        break;
      }
      ++row;
      column = 0;
    }
    const float x = layout.right_to_left
                        ? origin.x - static_cast<float>(column + 1) * step + layout.gap
                        : origin.x + static_cast<float>(column) * step;
    const float y = layout.upward ? origin.y - static_cast<float>(row + 1) * down + layout.gap
                                  : origin.y + static_cast<float>(row) * down;
    places.emplace_back(x, y);
    ++column;
  }
  return places;
}

std::vector<std::size_t> OrderStatusCells(std::span<const StatusCellView> cells, StatusSort sort,
                                          int most) {
  std::vector<std::size_t> order(cells.size());
  std::iota(order.begin(), order.end(), std::size_t{0});

  if (sort != StatusSort::kArrival) {
    // ⚠ UN ÉTAT SANS ÉCHÉANCE NE FINIT JAMAIS : il passe donc APRÈS tout ce qui
    // se compte, sous « bientôt fini », et AVANT tout sous « plus long ». Le
    // ranger à zéro l'aurait mis en tête des urgences, qu'il n'est pas.
    const auto ending_soon = [cells](std::size_t left, std::size_t right) {
      const std::uint32_t first = cells[left].remaining_ms;
      const std::uint32_t second = cells[right].remaining_ms;
      if ((first == 0) != (second == 0)) {
        return second == 0;
      }
      return first < second;
    };
    const auto longest = [cells](std::size_t left, std::size_t right) {
      const std::uint32_t first = cells[left].remaining_ms;
      const std::uint32_t second = cells[right].remaining_ms;
      if ((first == 0) != (second == 0)) {
        return first == 0;
      }
      return first > second;
    };
    // ⚠ STABLE : deux états de même durée gardent leur ordre d'arrivée, qui est
    // la seule chose qui les distingue.
    if (sort == StatusSort::kEndingSoon) {
      std::stable_sort(order.begin(), order.end(), ending_soon);
    } else {
      std::stable_sort(order.begin(), order.end(), longest);
    }
  }

  const auto kept = static_cast<std::size_t>(std::max(0, most));
  if (order.size() > kept) {
    order.resize(kept);
  }
  return order;
}

void DrawStatusRow(std::span<const StatusCellView> cells, ImVec2 origin,
                   const StatusRowLayout& layout, const StatusCellStyle& style, bool tooltip) {
  const std::vector<std::size_t> order = OrderStatusCells(cells, layout.sort, layout.most);
  const std::vector<ImVec2> places = PlaceStatusRow(order.size(), origin, layout);
  const std::size_t drawn = std::min(order.size(), places.size());
  for (std::size_t rank = 0; rank < drawn; ++rank) {
    const ImVec2 at = places[rank];
    DrawStatusCell(cells[order[rank]], at, ImVec2(at.x + layout.side, at.y + layout.side), style,
                   tooltip);
  }
}

}  // namespace ro::ui
