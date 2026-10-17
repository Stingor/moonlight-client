#include "ro/ui/status_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/rich_text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "achievement_paint.h"

namespace ro::ui {
namespace {

using achievement::kBlack;
using achievement::Painter;
using render::SkinPiece;

// ── LES TAILLES DE TEXTE, en pixels d'art ───────────────────────────────────
//
// Un nombre de quatre chiffres passe à la petite taille : à la grande, il
// mordrait sur la case voisine que l'art lui a dessinée.
constexpr float kTextNormal = 13.0F;
constexpr float kTextSmall = 11.0F;
constexpr std::int64_t kLargestNormalNumber = 999;
// Le nom de guilde, un cran sous les nombres, et un pixel plus bas qu'eux : à
// la hauteur des nombres, la petite taille le fait paraître au-dessus du libellé.
constexpr float kTextGuild = 12.0F;
constexpr float kGuildLowerPixels = 1.0F;
// Le haut d'un texte, au-dessus du centre de sa rangée.
constexpr float kTextAboveCenterPixels = 6.0F;
// Le seul bouton que la fenêtre pose dans sa barre de titre : les préréglages.
constexpr int kPresetButtonRank = 0;

// ── LES COLONNES, en pixels d'art depuis le bord gauche du fond ─────────────

// La colonne des caractéristiques : la base calée à gauche, le signe du bonus
// puis sa valeur absolue, et le coût calé à droite.
constexpr float kBaseLeft = 37.0F;
constexpr float kBonusSignLeft = 55.0F;
constexpr float kBonusValueLeft = 61.0F;
constexpr float kCostRight = 121.0F;
// Les deux colonnes des dérivées, calées à droite, et le nom de guilde.
constexpr float kDerivedNearRight = 208.0F;
constexpr float kDerivedFarRight = 294.0F;
constexpr float kGuildLeft = 163.0F;

// LES LIBELLÉS ÉCRITS PAR LE CODE QUAND LE FOND MANQUE : à gauche des
// caractéristiques, puis en tête des deux colonnes de dérivées — là où l'art
// les peint. Plus petits que les nombres, pour tenir dans leur case.
constexpr float kLabelLeft = 6.0F;
constexpr float kNearLabelLeft = 130.0F;
constexpr float kFarLabelLeft = 216.0F;
constexpr float kTextLabel = kTextSmall;

// ── LES BULLES D'AIDE ───────────────────────────────────────────────────────
//
// Un rectangle par libellé, ancré sur lui : survoler le NOM d'une valeur dit ce
// qu'elle mesure. ⚠ L'ESQUIVE PARFAITE N'EN A PAS, comme dans le jeu.
constexpr float kHelpWidth = 80.0F;
constexpr float kHelpHeight = 12.0F;

enum class Help : std::uint8_t {
  kStr,
  kAgi,
  kVit,
  kInt,
  kDex,
  kLuk,
  kStatusPoint,
  kAtk,
  kAspd,
  kDef,
  kMatk,
  kHit,
  kCritical,
  kGuild,
  kFlee,
  kMdef,
  // Nommée par le repli seulement : elle n'a pas de bulle.
  kPerfectDodge,
};

struct HelpSpot {
  Help topic;
  float left;
  float top;
};

constexpr std::array<HelpSpot, 16> kHelpSpots = {{
    {Help::kStr, kLabelLeft, 6.0F},
    {Help::kAgi, kLabelLeft, 21.0F},
    {Help::kVit, kLabelLeft, 37.0F},
    {Help::kInt, kLabelLeft, 53.0F},
    {Help::kDex, kLabelLeft, 70.0F},
    {Help::kLuk, kLabelLeft, 85.0F},
    {Help::kStatusPoint, kLabelLeft, 100.0F},
    {Help::kAtk, kNearLabelLeft, 6.0F},
    {Help::kAspd, kNearLabelLeft, 21.0F},
    {Help::kDef, kNearLabelLeft, 37.0F},
    {Help::kMatk, kNearLabelLeft, 53.0F},
    {Help::kHit, kNearLabelLeft, 70.0F},
    {Help::kCritical, kNearLabelLeft, 85.0F},
    {Help::kGuild, kNearLabelLeft, 100.0F},
    {Help::kFlee, kFarLabelLeft, 21.0F},
    {Help::kMdef, kFarLabelLeft, 37.0F},
}};

// LE TEXTE D'UNE BULLE, balisé comme le jeu l'écrit : `^66cc33` met en vert ce
// que la caractéristique nourrit, `^ffffff` revient au texte.
//
// ⚠ UN AIGUILLAGE DE LITTÉRAUX : `core::Tr` veut voir sa chaîne à l'appel.
[[nodiscard]] const char* HelpText(Help topic) {
  switch (topic) {
    case Help::kStr:
      return core::Tr("Force : ^66cc33ATK^ffffff et ^66cc33capacité de charge^ffffff");
    case Help::kAgi:
      return core::Tr("Agilité : ^66cc33ASPD, FLEE^ffffff et ^66cc33DEF^ffffff");
    case Help::kVit:
      return core::Tr("Vitalité : ^66cc33MaxHP, DEF^ffffff et ^66cc33MDEF^ffffff");
    case Help::kInt:
      return core::Tr(
          "Intelligence : ^66cc33MATK, temps d'incantation^ffffff et ^66cc33MDEF^ffffff");
    case Help::kDex:
      return core::Tr(
          "Dextérité : ^66cc33ATK à distance, HIT, MATK^ffffff et ^66cc33temps "
          "d'incantation^ffffff");
    case Help::kLuk:
      return core::Tr(
          "Chance : ^66cc33critique, HIT, ATK, MATK^ffffff et ^66cc33esquive parfaite^ffffff");
    case Help::kStatusPoint:
      return core::Tr("Points de statut");
    case Help::kAtk:
      return core::Tr("Dégâts physiques");
    case Help::kAspd:
      return core::Tr("Vitesse d'attaque");
    case Help::kDef:
      return core::Tr("Défense physique");
    case Help::kMatk:
      return core::Tr("Dégâts magiques");
    case Help::kHit:
      return core::Tr("Précision");
    case Help::kCritical:
      return core::Tr("Taux de critique");
    case Help::kGuild:
      return core::Tr("Guilde d'appartenance");
    case Help::kFlee:
      return core::Tr("Taux d'esquive");
    case Help::kMdef:
      return core::Tr("Défense magique");
    case Help::kPerfectDodge:
      break;
  }
  return "";
}

// LES LIBELLÉS DU REPLI, aux mots de la fiche : ce sont les mêmes valeurs.
// L'esquive parfaite s'y nomme, même sans bulle d'aide.
struct FallbackLabel {
  Help label;
  float left;
  std::size_t row;
};

constexpr std::array<FallbackLabel, 17> kFallbackLabels = {{
    {Help::kStr, kLabelLeft, 0},
    {Help::kAgi, kLabelLeft, 1},
    {Help::kVit, kLabelLeft, 2},
    {Help::kInt, kLabelLeft, 3},
    {Help::kDex, kLabelLeft, 4},
    {Help::kLuk, kLabelLeft, 5},
    {Help::kStatusPoint, kLabelLeft, 6},
    {Help::kAtk, kNearLabelLeft, 0},
    {Help::kAspd, kNearLabelLeft, 1},
    {Help::kDef, kNearLabelLeft, 2},
    {Help::kMatk, kNearLabelLeft, 3},
    {Help::kHit, kNearLabelLeft, 4},
    {Help::kCritical, kNearLabelLeft, 5},
    {Help::kGuild, kNearLabelLeft, 6},
    {Help::kFlee, kFarLabelLeft, 1},
    {Help::kMdef, kFarLabelLeft, 2},
    {Help::kPerfectDodge, kFarLabelLeft, 5},
}};

// ⚠ UN AIGUILLAGE DE LITTÉRAUX, comme celui des bulles.
[[nodiscard]] const char* LabelText(Help label) {
  switch (label) {
    case Help::kStr:
      return core::Tr("STR");
    case Help::kAgi:
      return core::Tr("AGI");
    case Help::kVit:
      return core::Tr("VIT");
    case Help::kInt:
      return core::Tr("INT");
    case Help::kDex:
      return core::Tr("DEX");
    case Help::kLuk:
      return core::Tr("LUK");
    case Help::kStatusPoint:
      return core::Tr("Points de statut :");
    case Help::kAtk:
      return core::Tr("ATK");
    case Help::kAspd:
      return core::Tr("ASPD");
    case Help::kDef:
      return core::Tr("DEF");
    case Help::kMatk:
      return core::Tr("MATK");
    case Help::kHit:
      return core::Tr("HIT");
    case Help::kCritical:
      return core::Tr("CRI");
    case Help::kGuild:
      return core::Tr("Guilde");
    case Help::kFlee:
      return core::Tr("FLEE");
    case Help::kMdef:
      return core::Tr("MDEF");
    case Help::kPerfectDodge:
      return core::Tr("Esq.P");
  }
  return "";
}

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

[[nodiscard]] float TextTop(std::size_t row) {
  return kStatusRowCenterPixels[row] - kTextAboveCenterPixels;
}

[[nodiscard]] float SizeFor(std::int64_t number) {
  return number > kLargestNormalNumber ? kTextSmall : kTextNormal;
}

// Un nombre, ou une paire de nombres autour d'un séparateur.
[[nodiscard]] std::string Number(std::int64_t value) {
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), "%lld", static_cast<long long>(value));
  return text.data();
}
[[nodiscard]] std::string Pair(std::int64_t left, const char* between, std::int64_t right) {
  std::array<char, 64> text{};
  std::snprintf(text.data(), text.size(), "%lld%s%lld", static_cast<long long>(left), between,
                static_cast<long long>(right));
  return text.data();
}

// LA BULLE D'AIDE, au corps des bulles du jeu, ses couleurs gardées lisibles
// sur son papier.
//
// ⚠ LE JEU L'ÉCRIT EN BLANC SUR SOMBRE, et le balisage le dit (`^ffffff`). Les
// bulles de ce client sont claires : le blanc y devient l'encre, et le vert se
// fonce juste assez pour se lire.
void ShowHelp(Help topic) {
  if (!BeginSkinTooltip()) {
    return;
  }
  const float widest = kSkinnedTooltipWidest * Scale();
  const std::array<std::string, 1> lines = {std::string(HelpText(topic))};
  RichText laid = LayOutRichTextHere(lines, ImGui::GetColorU32(ImGuiCol_Text), widest);
  KeepReadable(laid, PaintingLook().paper);
  // ⚠ LA BULLE SE REFERME SUR SON TEXTE : disposé sur la largeur maximale, un
  // libellé de deux mots ouvrirait une bulle de trois cents pixels.
  float used = 0.0F;
  for (const RichBox& box : laid.boxes) {
    used = std::max(used, box.right);
  }
  laid.width = std::min(laid.width, used);
  static_cast<void>(DrawRichText("##aide", laid, ImGui::GetTextLineHeightWithSpacing()));
  EndSkinTooltip();
}

// LA COLONNE DES CARACTÉRISTIQUES : base, bonus, coût, et la flèche.
void DrawAttributes(const Painter& paint, const StatusWindowView& view, const SkinArt& art,
                    StatusWindowAction& action) {
  const ImU32 ink = paint.Ink(kBlack);
  const SkinImage& arrow = art.Get(SkinPiece::kStatusRaiseArrow);
  for (std::size_t rank = 0; rank < kStatusAttributeCount; ++rank) {
    const CharacterAttributeView& attribute = view.attributes[rank];
    const float top = TextTop(rank);
    paint.Text(kBaseLeft, top, Number(attribute.base), SizeFor(attribute.base), ink);
    // ⚠ LE SIGNE À PART DE LA VALEUR, et la valeur sans le sien : les deux
    // tombent dans des cases distinctes de l'art.
    if (attribute.bonus != 0) {
      const float size = SizeFor(attribute.bonus);
      paint.Text(kBonusSignLeft, top, attribute.bonus > 0 ? "+" : "-", size, ink);
      paint.Text(kBonusValueLeft, top, Number(std::llabs(attribute.bonus)), size, ink);
    }
    paint.TextRight(kCostRight, top, Number(attribute.cost), kTextNormal, ink);

    if (!StatusArrowShown(attribute, view.status_point)) {
      continue;
    }
    const float arrow_top = StatusArrowTopPixels(rank);
    std::array<char, 16> id{};
    std::snprintf(id.data(), id.size(), "##monter%zu", rank);
    // Un bouton du jeu réagit au relâché sur lui.
    if (paint
            .Hit(id.data(), kStatusArrowLeftPixels, arrow_top, kStatusArrowPixels,
                 kStatusArrowPixels, achievement::kOnRelease)
            .pressed) {
      action.verb = StatusVerb::kRaiseAttribute;
      action.attribute = static_cast<std::uint8_t>(rank);
      action.amount = AttributeRaiseAmount(attribute, ImGui::GetIO().KeyShift);
    }
    if (arrow.usable()) {
      paint.Art(arrow, kStatusArrowLeftPixels, arrow_top);
    } else {
      // Sans son image, la flèche reste un « + » qu'on voit et qu'on clique.
      paint.TextCentered(kStatusArrowLeftPixels, kStatusArrowPixels, arrow_top, "+", kTextSmall,
                         ink);
    }
  }
  paint.TextRight(kCostRight, TextTop(kStatusAttributeCount), Number(view.status_point),
                  kTextNormal, ink);
}

// LES DÉRIVÉES, calées à droite sur leurs deux colonnes, puis la guilde.
//
// ⚠ CHAQUE FORME DIT QUELQUE CHOSE, comme dans la fiche : « + » sépare une base
// d'un bonus, « ~ » un minimum d'un maximum.
void DrawDerived(const Painter& paint, const StatusWindowView& view) {
  const ImU32 ink = paint.Ink(kBlack);
  const auto near_column = [&](std::size_t row, const std::string& text) {
    paint.TextRight(kDerivedNearRight, TextTop(row), text, kTextNormal, ink);
  };
  const auto far_column = [&](std::size_t row, const std::string& text) {
    paint.TextRight(kDerivedFarRight, TextTop(row), text, kTextNormal, ink);
  };
  far_column(0, Pair(view.atk1, " + ", view.atk2));
  near_column(1, Number(view.aspd));
  far_column(1, Number(view.flee));
  near_column(2, Pair(view.def1, " + ", view.def2));
  far_column(2, Pair(view.mdef1, " + ", view.mdef2));
  far_column(3, Pair(view.matk_min, " ~ ", view.matk_max));
  near_column(4, Number(view.hit));
  near_column(5, Number(view.critical));
  far_column(5, Number(view.perfect_dodge));
  if (!view.guild_name.empty()) {
    paint.Text(kGuildLeft, TextTop(kStatusAttributeCount) + kGuildLowerPixels, view.guild_name,
               kTextGuild, ink);
  }
}

}  // namespace

bool StatusArrowShown(const CharacterAttributeView& attribute, std::int64_t status_point) {
  return attribute.cost != 0 && status_point >= attribute.cost;
}

float StatusArrowTopPixels(std::size_t rank) {
  return kStatusRowCenterPixels[rank] - kStatusArrowAboveCenterPixels;
}

StatusWindowAction DrawStatusWindow(StatusWindowState& state, const StatusWindowView& view,
                                    const SkinArt& art) {
  StatusWindowAction action;
  // LES PRÉRÉGLAGES SE DESSINENT MÊME STATUT FERMÉ : une application lancée se
  // suit et s'arrête depuis leur fenêtre.
  action.presets = DrawStatPresetWindow(state.presets, view.presets, art);
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  // 🔴 LE CORPS EST CELUI DU FOND, ET IMPOSÉ À CHAQUE IMAGE : les nombres
  // tombent aux pixels où l'art leur laisse la place, et rien ne se
  // redimensionne. Sans le fond, la fenêtre garde la géométrie du jeu.
  //
  // ⚠ LA LARGEUR SE POSE, LA HAUTEUR S'AJUSTE AU CONTENU — un `y` nul le
  // demande à ImGui. La barre de titre n'a pas une hauteur fixe : elle suit son
  // art ET la police, la plus grande des deux. Une hauteur totale imposée
  // rognerait le bas du fond dès que la police dépasse la barre.
  const SkinImage& background = art.Get(SkinPiece::kStatusBackground);
  const bool painted = background.usable();
  const float width = painted ? background.width : kStatusArtWidthPixels;
  const float body = painted ? background.height : kStatusBodyPixels;
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(width * scale, 0.0F), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  // ⚠ REPLIABLE : la barre porte son bouton de minimisation, comme dans le jeu.
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  static WindowName name(kStatusWindowTitle, kStatusWindowId);
  PoseStatPresetButton();
  if (BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar, kFlags)) {
    if (TitleActionClicked(kPresetButtonRank)) {
      state.presets.open = !state.presets.open;
    }
    ImGuiWindow* const window = ImGui::GetCurrentWindow();
    // L'ORIGINE EST LE HAUT DU FOND, là où la barre réelle s'arrête.
    const Painter paint(window->DrawList,
                        ImVec2(window->Pos.x, window->Pos.y + window->TitleBarHeight), scale,
                        PaintingLook().paper);
    paint.PushClip(0.0F, 0.0F, width, body);
    // LE CORPS EST DU PAPIER : l'art et l'encre du jeu, choisis pour du blanc,
    // s'y posent — et les libellés du repli s'y lisent.
    paint.Fill(0.0F, 0.0F, width, body, paint.paper());
    if (painted) {
      paint.Art(background, 0.0F, 0.0F);
    } else {
      const ImU32 ink = paint.Ink(kBlack);
      for (const FallbackLabel& label : kFallbackLabels) {
        paint.Text(label.left, TextTop(label.row), LabelText(label.label), kTextLabel, ink);
      }
    }
    DrawAttributes(paint, view, art, action);
    DrawDerived(paint, view);
    if (ImGui::IsWindowHovered()) {
      for (const HelpSpot& spot : kHelpSpots) {
        if (ImGui::IsMouseHoveringRect(paint.At(spot.left, spot.top),
                                       paint.At(spot.left + kHelpWidth, spot.top + kHelpHeight))) {
          ShowHelp(spot.topic);
          break;
        }
      }
    }
    paint.PopClip();
    // LE CONTENU VAUT LE FOND, sous la barre réelle : c'est lui que la hauteur
    // ajustée mesure. ⚠ ARRONDI AU PIXEL SUPÉRIEUR : ImGui tronque la taille
    // ajustée au pixel entier, et un fond de 172,5 pixels y perdrait sa
    // dernière ligne.
    ImGui::SetCursorScreenPos(ImVec2(window->Pos.x, window->Pos.y + window->TitleBarHeight));
    ImGui::Dummy(ImVec2(width * scale, std::ceil(body * scale)));
  }
  EndSkinnedWindow();
  ImGui::PopStyleVar();
  return action;
}

}  // namespace ro::ui
