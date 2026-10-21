#include "ro/ui/basic_info_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/stat_bars.h"

#include "achievement_paint.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

using achievement::kBlack;
using achievement::kOnRelease;
using achievement::kWhite;
using achievement::Painter;
using render::SkinPiece;

// ── LES COULEURS DU JEU ─────────────────────────────────────────────────────
//
// Celles de sa table de teintes d'interface : le gris du cadre de la barre
// repliée, celui du cadre des jauges d'expérience et leur bleu.
constexpr ImU32 kFrameGrey = IM_COL32(0xC8, 0xC8, 0xC8, 0xFF);
constexpr ImU32 kExpFrame = IM_COL32(0xAF, 0xAF, 0xAF, 0xFF);
constexpr ImU32 kExpFill = IM_COL32(0x56, 0x7F, 0xCF, 0xFF);
// Le rouge d'alerte des textes : vie et magie basses, surcharge.
constexpr ImU32 kAlertRed = IM_COL32(0xFF, 0x00, 0x00, 0xFF);

// ── LES TAILLES DE TEXTE ────────────────────────────────────────────────────
//
// Le jeu écrit en deux polices : les nombres et libellés à 14, le titre, le
// pseudo et la classe à 12 ; le texte des jauges à 13.
constexpr float kTextLarge = 14.0F;
constexpr float kTextSmall = 12.0F;
constexpr float kTextGauge = 13.0F;

// ── LA BARRE DE TITRE ───────────────────────────────────────────────────────
//
// Un bout gauche, un bout droit, et la tranche du milieu répétée DEPUIS le bout
// gauche jusqu'à couvrir la largeur. ⚠ LA DERNIÈRE TRANCHE DÉBORDE SUR LE BOUT
// DROIT, posé avant elle : c'est ainsi que le jeu la montre.
constexpr float kTitleTilePixels = 12.0F;
// Le titre, écrit deux fois : en blanc un pixel plus bas et à droite, puis en
// noir par-dessus.
constexpr float kTitleLeft = 17.0F;
constexpr float kTitleTop = 2.0F;
// Les deux boutons de la barre : la puce, qui ne fait rien, et le repli.
constexpr float kButtonPixels = 11.0F;
constexpr float kButtonTop = 3.0F;
constexpr float kBulletLeft = 3.0F;
constexpr float kCollapseLeft = 206.0F;

// ── LA BARRE REPLIÉE ────────────────────────────────────────────────────────
//
// Le pseudo se pose sur la barre de titre ; dessous, deux lignes.
constexpr float kFoldedNameLeft = 17.0F;
constexpr float kFoldedNameTop = 3.0F;
constexpr float kFoldedLeft = 9.0F;
constexpr float kFoldedLevelsTop = 20.0F;
constexpr float kFoldedGaugesTop = 36.0F;
// Le cadre gris : il commence sous la barre, et sa première bande est pleine.
constexpr float kFoldedGreyBand = 5.0F;
// Le repli de la ligne des niveaux quand elle déborde : centrée, ou collée au
// bord gauche si même la forme courte est trop large.
constexpr float kFoldedNarrowLeft = 3.0F;

// ── LE PANNEAU DÉPLIÉ ───────────────────────────────────────────────────────
constexpr float kNameLeft = 9.0F;
constexpr float kNameTop = 22.0F;
constexpr float kJobTop = 36.0F;
constexpr float kLabelLeft = 17.0F;
constexpr float kHpTop = 51.0F;
constexpr float kSpTop = 65.0F;
constexpr float kPercentLeft = 173.0F;
constexpr float kBaseLevelTop = 84.0F;
constexpr float kJobLevelTop = 96.0F;
constexpr float kPurseTop = 116.0F;
// Le zeny se cale à droite à cinq pixels du bord ; le poids, à dix pixels du
// bord moins la largeur du zeny — soit cinq pixels entre les deux.
constexpr float kPurseRightMargin = 5.0F;
constexpr float kWeightRightMargin = 10.0F;
// Le pseudo tient dans une case de quinze pixels de haut, plus étroite sur la
// barre repliée, où le bouton de repli la borne.
constexpr float kNameHeight = 15.0F;
constexpr float kFoldedNameRoom = 30.0F;
constexpr float kNameRoom = 20.0F;

// Les deux jauges de vie et de magie : leur gouttière fait 135 × 9, et le
// remplissage tient entre deux bouts de quatre pixels.
constexpr float kGaugeLeft = 35.0F;
constexpr float kHpGaugeTop = 53.0F;
constexpr float kSpGaugeTop = 68.0F;
constexpr float kGaugeWidth = 135.0F;
constexpr float kGaugeEndPixels = 4.0F;
// Le texte d'une jauge, centré, monte de deux pixels au-dessus d'elle.
constexpr float kGaugeTextLift = 2.0F;

// Les deux jauges d'expérience : 112 × 6, un pixel de cadre.
constexpr float kExpLeft = 85.0F;
constexpr float kBaseExpTop = 89.0F;
constexpr float kJobExpTop = 101.0F;
constexpr float kExpWidth = 112.0F;
constexpr float kExpHeight = 6.0F;
constexpr float kExpFramePixels = 1.0F;

// La bulle du poids : le bas-gauche du panneau, sur cent pixels et vingt de
// haut.
constexpr float kWeightTipWidth = 100.0F;
constexpr float kWeightTipHeight = 20.0F;

constexpr double kPercent = 100.0;
constexpr double kPerMille = 1000.0;
constexpr std::int64_t kTenthsPerUnit = 10;
constexpr std::size_t kDigitsPerGroup = 3;

using Line = std::array<char, 128>;

// ── LA BARRE DE TITRE ───────────────────────────────────────────────────────

void PaintTitleBar(const Painter& paint, const SkinArt& art) {
  paint.Art(art.Get(SkinPiece::kTitleBarLeft), 0.0F, 0.0F);
  paint.Art(art.Get(SkinPiece::kTitleBarRight), kBasicInfoWidthPixels - kTitleTilePixels, 0.0F);
  const SkinImage& middle = art.Get(SkinPiece::kTitleBarMiddle);
  const float span = kBasicInfoWidthPixels - 2.0F * kTitleTilePixels;
  const int tiles = static_cast<int>(std::ceil(span / kTitleTilePixels));
  for (int tile = 0; tile < tiles; ++tile) {
    paint.Art(middle, kTitleTilePixels + static_cast<float>(tile) * kTitleTilePixels, 0.0F);
  }
}

// Un bouton de la barre : éteint au repos, allumé survolé ou enfoncé. Rend vrai
// au relâché sur lui.
[[nodiscard]] bool TitleButton(const Painter& paint, const SkinArt& art, const char* id, float left,
                               SkinPiece off, SkinPiece on) {
  const achievement::HitResult hit =
      paint.Hit(id, left, kButtonTop, kButtonPixels, kButtonPixels, kOnRelease);
  paint.Art(art.Get(hit.hovered || hit.held ? on : off), left, kButtonTop);
  return hit.pressed;
}

// ── LES JAUGES ──────────────────────────────────────────────────────────────

// UNE JAUGE DE VIE OU DE MAGIE : la gouttière, le remplissage bleu ou rouge en
// trois morceaux, puis les deux nombres centrés.
//
// ⚠ UN ACQUIS AU-DESSUS DU MAXIMUM NE DESSINE RIEN, pas même la gouttière ; un
// maximum nul la laisse vide et muette.
void PaintGauge(const Painter& paint, const SkinArt& art, float top, std::int64_t current,
                std::int64_t most) {
  if (most < current) {
    return;
  }
  paint.Art(art.Get(SkinPiece::kBasicInfoGaugeTrough), kGaugeLeft, top);
  if (most == 0) {
    return;
  }
  const bool low = BasicInfoLow(current, most);
  const float fill = BasicInfoFill(current, most, kGaugeWidth - 2.0F * kGaugeEndPixels);
  const SkinImage& left =
      art.Get(low ? SkinPiece::kBasicInfoGaugeRedLeft : SkinPiece::kBasicInfoGaugeBlueLeft);
  const SkinImage& middle =
      art.Get(low ? SkinPiece::kBasicInfoGaugeRedMiddle : SkinPiece::kBasicInfoGaugeBlueMiddle);
  const SkinImage& right =
      art.Get(low ? SkinPiece::kBasicInfoGaugeRedRight : SkinPiece::kBasicInfoGaugeBlueRight);
  paint.Art(left, kGaugeLeft, top);
  // LA TRANCHE S'ÉTIRE au lieu de se répéter : un pixel de large, elle donne la
  // même image en un seul blit.
  if (fill > 0.0F && middle.usable()) {
    PaintOnPaper(paint.into(), middle, paint.At(kGaugeLeft + kGaugeEndPixels, top),
                 paint.At(kGaugeLeft + kGaugeEndPixels + fill, top + middle.height), paint.paper());
  }
  paint.Art(right, kGaugeLeft + kGaugeEndPixels + fill, top);

  std::array<char, 64> text{};
  std::snprintf(text.data(), text.size(), "%3lld  /  %3lld", static_cast<long long>(current),
                static_cast<long long>(most));
  // ⚠ CENTRÉ À LA DIVISION ENTIÈRE, comme le jeu.
  const float width = std::floor(paint.TextWidth(text.data(), kTextGauge));
  const float left_edge = std::trunc((kGaugeWidth - width) / 2.0F);
  paint.Text(kGaugeLeft + left_edge, top - kGaugeTextLift, text.data(), kTextGauge,
             paint.Ink(kBlack));
}

// UNE JAUGE D'EXPÉRIENCE : un cadre gris, un fond blanc, le bleu de l'acquis.
// Pas de texte, et jamais de rouge.
void PaintExpBar(const Painter& paint, float top, std::int64_t exp, std::int64_t next) {
  if (!BasicInfoExpBarShown(exp, next)) {
    return;
  }
  paint.Fill(kExpLeft, top, kExpWidth, kExpHeight, paint.Laid(kExpFrame));
  const float inner_width = kExpWidth - 2.0F * kExpFramePixels;
  const float inner_height = kExpHeight - 2.0F * kExpFramePixels;
  paint.Fill(kExpLeft + kExpFramePixels, top + kExpFramePixels, inner_width, inner_height,
             paint.Laid(kWhite));
  paint.Fill(kExpLeft + kExpFramePixels, top + kExpFramePixels,
             BasicInfoFill(exp, next, inner_width), inner_height, paint.Laid(kExpFill));
}

// ── LE PSEUDO ───────────────────────────────────────────────────────────────

// LE PSEUDO, coupé au bord de sa case : un nom long ne passe pas sous le
// bouton de repli. `room` est ce que la case laisse à droite de la fenêtre.
void PaintName(const Painter& paint, float left, float top, float room, std::string_view name) {
  paint.PushClip(left, top, kBasicInfoWidthPixels - room, kNameHeight);
  paint.Text(left, top, name, kTextSmall, paint.Ink(kBlack));
  paint.PopClip();
}

// ── LA BARRE REPLIÉE ────────────────────────────────────────────────────────

// LE FOND REPLIÉ, sans image : un cadre gris aux coins rognés, une première
// bande grise, le corps blanc.
void PaintFoldedBody(const Painter& paint, float height) {
  const ImU32 grey = paint.Laid(kFrameGrey);
  const ImU32 white = paint.Laid(kWhite);
  const float top = kBasicInfoTitleBarPixels;
  const float bottom = height - 1.0F;
  const float right = kBasicInfoWidthPixels - 1.0F;
  // Le contour, chaque coin laissé vide.
  paint.Fill(1.0F, top, kBasicInfoWidthPixels - 2.0F, 1.0F, grey);
  paint.Fill(1.0F, bottom, kBasicInfoWidthPixels - 2.0F, 1.0F, grey);
  paint.Fill(0.0F, top + 1.0F, 1.0F, bottom - top - 1.0F, grey);
  paint.Fill(right, top + 1.0F, 1.0F, bottom - top - 1.0F, grey);
  paint.Fill(0.0F, top, kBasicInfoWidthPixels, kFoldedGreyBand, grey);
  paint.Fill(1.0F, top, kBasicInfoWidthPixels - 2.0F, height - top - 2.0F, white);
  paint.Fill(2.0F, height - 2.0F, kBasicInfoWidthPixels - 4.0F, 1.0F, white);
}

void PaintFolded(const Painter& paint, const BasicInfoView& view) {
  const ImU32 ink = paint.Ink(kBlack);
  const ImU32 alert = paint.Ink(kAlertRed);
  PaintName(paint, kFoldedNameLeft, kFoldedNameTop, kFoldedNameRoom, view.name);

  // LA LIGNE DES NIVEAUX, raccourcie et centrée quand elle déborde.
  const double percent = BasicInfoExpPercent(view.base_exp, view.next_base_exp);
  Line levels{};
  std::snprintf(levels.data(), levels.size(), core::Tr("Lv.%2d / %s / Lv.%2d / Exp. %.1f %%"),
                static_cast<int>(view.base_level), view.job.c_str(),
                static_cast<int>(view.job_level), percent);
  float left = kFoldedLeft;
  const bool shortened =
      paint.TextWidth(levels.data(), kTextLarge) + kFoldedLeft > kBasicInfoWidthPixels;
  if (shortened) {
    std::snprintf(levels.data(), levels.size(), core::Tr("Lv%2d / %s / Lv%2d / Exp%.1f%%"),
                  static_cast<int>(view.base_level), view.job.c_str(),
                  static_cast<int>(view.job_level), percent);
    const float room = kBasicInfoWidthPixels - paint.TextWidth(levels.data(), kTextLarge);
    left = room >= 0.0F ? std::trunc(room / 2.0F) : kFoldedNarrowLeft;
  }
  paint.Text(left, kFoldedLevelsTop, levels.data(), kTextLarge, ink);

  // LA LIGNE DES JAUGES : la vie, un séparateur, la magie. ⚠ LE POINT APRÈS
  // « HP » ET « SP » TOMBE avec la forme courte de la ligne du dessus.
  const bool narrow = shortened;
  Line text{};
  float x = left;
  if (view.max_hp != 0) {
    if (narrow) {
      std::snprintf(text.data(), text.size(), core::Tr("HP %d / %d"), static_cast<int>(view.hp),
                    static_cast<int>(view.max_hp));
    } else {
      std::snprintf(text.data(), text.size(), core::Tr("HP. %d / %d"), static_cast<int>(view.hp),
                    static_cast<int>(view.max_hp));
    }
    paint.Text(x, kFoldedGaugesTop, text.data(), kTextLarge,
               BasicInfoLow(view.hp, view.max_hp) ? alert : ink);
    x += paint.TextWidth(text.data(), kTextLarge);
    constexpr const char* kSeparator = " | ";
    paint.Text(x, kFoldedGaugesTop, kSeparator, kTextLarge, ink);
    x += paint.TextWidth(kSeparator, kTextLarge);
  }
  if (view.max_sp >= 0) {
    if (narrow) {
      std::snprintf(text.data(), text.size(), core::Tr("SP %d / %d"), static_cast<int>(view.sp),
                    static_cast<int>(view.max_sp));
    } else {
      std::snprintf(text.data(), text.size(), core::Tr("SP. %d / %d"), static_cast<int>(view.sp),
                    static_cast<int>(view.max_sp));
    }
    paint.Text(x, kFoldedGaugesTop, text.data(), kTextLarge,
               BasicInfoLow(view.sp, view.max_sp) ? alert : ink);
  }
}

// ── LE PANNEAU DÉPLIÉ ───────────────────────────────────────────────────────

void PaintExpanded(const Painter& paint, const BasicInfoView& view, const SkinArt& art) {
  const ImU32 ink = paint.Ink(kBlack);
  PaintName(paint, kNameLeft, kNameTop, kNameRoom, view.name);
  paint.Text(kNameLeft, kJobTop, view.job, kTextSmall, ink);

  paint.Text(kLabelLeft, kHpTop, core::Tr("HP"), kTextLarge, ink);
  paint.Text(kLabelLeft, kSpTop, core::Tr("SP"), kTextLarge, ink);
  PaintGauge(paint, art, kHpGaugeTop, view.hp, view.max_hp);
  PaintGauge(paint, art, kSpGaugeTop, view.sp, view.max_sp);
  Line text{};
  // ⚠ UN POUR CENT ENTIER, tronqué : 99 % jusqu'au dernier point.
  if (view.max_hp != 0) {
    std::snprintf(text.data(), text.size(), "%lld%%",
                  static_cast<long long>(view.hp * 100 / view.max_hp));
    paint.Text(kPercentLeft, kHpTop, text.data(), kTextLarge, ink);
  }
  if (view.max_sp != 0) {
    std::snprintf(text.data(), text.size(), "%lld%%",
                  static_cast<long long>(view.sp * 100 / view.max_sp));
    paint.Text(kPercentLeft, kSpTop, text.data(), kTextLarge, ink);
  }

  std::snprintf(text.data(), text.size(), core::Tr("Base Lv. %d"),
                static_cast<int>(view.base_level));
  paint.Text(kLabelLeft, kBaseLevelTop, text.data(), kTextLarge, ink);
  std::snprintf(text.data(), text.size(), core::Tr("Job Lv. %d"), static_cast<int>(view.job_level));
  paint.Text(kLabelLeft, kJobLevelTop, text.data(), kTextLarge, ink);
  PaintExpBar(paint, kBaseExpTop, view.base_exp, view.next_base_exp);
  PaintExpBar(paint, kJobExpTop, view.job_exp, view.next_job_exp);

  // LE ZENY CALÉ À DROITE, PUIS LE POIDS CALÉ CONTRE LUI.
  const std::string grouped = BasicInfoGrouped(view.zeny);
  // ⚠ SANS TRADUCTION : le jeu écrit « Zeny : » partout, et sa largeur place le
  // poids.
  std::snprintf(text.data(), text.size(), "Zeny : %s", grouped.c_str());
  const float purse_right = kBasicInfoWidthPixels - kPurseRightMargin;
  const float zeny_width = paint.TextWidth(text.data(), kTextLarge);
  paint.TextRight(purse_right, kPurseTop, text.data(), kTextLarge, ink);
  const std::int64_t weight = view.weight_tenths / kTenthsPerUnit;
  const std::int64_t max_weight = view.max_weight_tenths / kTenthsPerUnit;
  std::snprintf(text.data(), text.size(), core::Tr("Poids : %3d / %3d"), static_cast<int>(weight),
                static_cast<int>(max_weight));
  paint.TextRight(
      kBasicInfoWidthPixels - zeny_width - kWeightRightMargin, kPurseTop, text.data(), kTextLarge,
      BasicInfoOverweight(weight, max_weight, view.overweight_percent) ? paint.Ink(kAlertRed)
                                                                       : ink);
}

// LA BULLE DU POIDS, au survol du bas-gauche du panneau.
void ShowWeightTip(const Painter& paint, const BasicInfoView& view) {
  const float top = kBasicInfoExpandedPixels - kWeightTipHeight;
  if (!ImGui::IsWindowHovered() ||
      !ImGui::IsMouseHoveringRect(paint.At(0.0F, top),
                                  paint.At(kWeightTipWidth, kBasicInfoExpandedPixels))) {
    return;
  }
  const std::int64_t weight = view.weight_tenths / kTenthsPerUnit;
  const std::int64_t max_weight = view.max_weight_tenths / kTenthsPerUnit;
  const std::int64_t percent = max_weight != 0 ? weight * 100 / max_weight : 0;
  SkinTooltipFormat(core::Tr("Poids %d%%"), static_cast<int>(percent));
}

}  // namespace

double BasicInfoExpPercent(std::int64_t exp, std::int64_t next) {
  if (next == kBaseExpCeiling || next == kJobExpCeiling) {
    return kPercent;
  }
  const std::int64_t acquired = std::max<std::int64_t>(exp, 0);
  const std::int64_t step = next != 0 ? next : acquired + 1;
  // TRONQUÉ AU DIXIÈME, jamais arrondi : 99,99 % s'écrit 99,9.
  const double per_mille =
      std::trunc(static_cast<double>(acquired) / static_cast<double>(step) * kPerMille);
  return per_mille / (kPerMille / kPercent);
}

bool BasicInfoExpBarShown(std::int64_t exp, std::int64_t next) {
  return next >= exp && next != kBaseExpCeiling && next != kJobExpCeiling;
}

float BasicInfoFill(std::int64_t current, std::int64_t most, float span) {
  if (most <= 0 || current <= 0) {
    return 0.0F;
  }
  const double share = std::min(1.0, static_cast<double>(current) / static_cast<double>(most));
  return std::trunc(static_cast<float>(share * static_cast<double>(span)));
}

bool BasicInfoLow(std::int64_t current, std::int64_t most) {
  return most > 0 && static_cast<double>(current) / static_cast<double>(most) < kBasicInfoLowShare;
}

bool BasicInfoOverweight(std::int64_t weight, std::int64_t max_weight,
                         std::int64_t overweight_percent) {
  if (overweight_percent <= 0) {
    return false;
  }
  return weight * 100 / std::max<std::int64_t>(max_weight, 1) >= overweight_percent;
}

std::string BasicInfoGrouped(std::int64_t value) {
  // ⚠ LA VALEUR ABSOLUE EN NON SIGNÉ : celle du plus petit entier ne tient pas
  // dans son type.
  const std::uint64_t magnitude =
      value < 0 ? 0U - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
  const std::string digits = std::to_string(magnitude);
  std::string grouped = value < 0 ? "-" : "";
  for (std::size_t rank = 0; rank < digits.size(); ++rank) {
    if (rank != 0 && (digits.size() - rank) % kDigitsPerGroup == 0) {
      grouped += ',';
    }
    grouped += digits[rank];
  }
  return grouped;
}

BasicInfoAnswer DrawBasicInfoWindow(BasicInfoWindowState& state, const BasicInfoView& view,
                                    const SkinArt& art) {
  BasicInfoAnswer answer;
  if (!state.shown) {
    return answer;
  }
  const float scale = Scale();
  const float height = state.expanded ? kBasicInfoExpandedPixels : kBasicInfoCollapsedPixels;
  // AU COIN HAUT-GAUCHE DE L'ÉCRAN, la première fois : la place du jeu.
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos, ImGuiCond_FirstUseEver);
  // ⚠ APRÈS UN REPLI, LE HAUT RESTE OÙ IL ÉTAIT : posée dans le bas de l'écran,
  // la fenêtre s'ancrerait sinon par son bord bas, et sa barre sauterait.
  if (state.keep_top_left.has_value()) {
    ImGui::SetNextWindowPos(*state.keep_top_left, ImGuiCond_Always);
    state.keep_top_left.reset();
  }
  ImGui::SetNextWindowSize(ImVec2(kBasicInfoWidthPixels * scale, std::ceil(height * scale)),
                           ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  // ⚠ SANS DÉPLACEMENT D'IMGUI : seule la barre de titre emporte la fenêtre,
  // et c'est elle qui le fait.
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse |
                                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
  std::array<char, 32> name{};
  std::snprintf(name.data(), name.size(), "###%s", kBasicInfoWindowId);
  if (ImGui::Begin(name.data(), nullptr, kFlags)) {
    ImGuiWindow* const window = ImGui::GetCurrentWindow();
    const Painter paint(window->DrawList, window->Pos, scale, PaintingLook().paper);
    paint.PushClip(0.0F, 0.0F, kBasicInfoWidthPixels, height);

    const SkinImage& background = art.Get(SkinPiece::kBasicInfoBackground);
    if (state.expanded) {
      if (background.usable()) {
        paint.Art(background, 0.0F, 0.0F);
      } else {
        PaintFoldedBody(paint, height);
      }
    } else {
      PaintFoldedBody(paint, height);
    }
    PaintTitleBar(paint, art);

    // ⚠ LES DEUX BOUTONS AVANT LA PRISE : ImGui donne le survol au premier
    // posé, et la prise couvre toute la barre.
    //
    // La puce ne fait rien, mais elle s'allume au survol comme partout.
    static_cast<void>(TitleButton(paint, art, "##puce", kBulletLeft, SkinPiece::kBulletOff,
                                  SkinPiece::kBulletOn));
    bool toggle = TitleButton(paint, art, "##replier", kCollapseLeft, SkinPiece::kCollapseOff,
                              SkinPiece::kCollapseOn);
    if (ImGui::IsItemHovered() && !view.toggle_keys.empty()) {
      SkinTooltipFormat("%s", view.toggle_keys.c_str());
    }
    const achievement::HitResult grip =
        paint.Hit("##prise", 0.0F, 0.0F, kBasicInfoWidthPixels, kBasicInfoTitleBarPixels);
    if (grip.held && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0F)) {
      const ImVec2 delta = ImGui::GetIO().MouseDelta;
      ImGui::SetWindowPos(ImVec2(window->Pos.x + delta.x, window->Pos.y + delta.y));
    }
    if (grip.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      toggle = true;
    }

    if (state.expanded) {
      // LE TITRE, que le jeu écrit en blanc puis en noir par-dessus.
      const char* const title = core::Tr("Basic Info");
      paint.Text(kTitleLeft + 1.0F, kTitleTop + 1.0F, title, kTextSmall, kWhite);
      paint.Text(kTitleLeft, kTitleTop, title, kTextSmall, kBlack);
      PaintExpanded(paint, view, art);
      ShowWeightTip(paint, view);
    } else {
      PaintFolded(paint, view);
    }
    paint.PopClip();
    // LE CORPS ARRÊTE LES CLICS : sous la barre, la fenêtre ne s'emporte pas et
    // ne laisse rien passer au monde.
    static_cast<void>(paint.Hit("##corps", 0.0F, kBasicInfoTitleBarPixels, kBasicInfoWidthPixels,
                                height - kBasicInfoTitleBarPixels));
    if (toggle) {
      state.expanded = !state.expanded;
      state.keep_top_left = window->Pos;
    }
    // LE COIN DE LA TAILLE QUI VIENT, repli compris : la grille ne traîne pas
    // une image derrière.
    const float next = state.expanded ? kBasicInfoExpandedPixels : kBasicInfoCollapsedPixels;
    answer.dock = ImVec2(window->Pos.x, window->Pos.y + std::ceil(next * scale));
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  return answer;
}

}  // namespace ro::ui
