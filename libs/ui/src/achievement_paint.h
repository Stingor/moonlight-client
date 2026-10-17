#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/achievement_window.h"
#include "ro/ui/gauge_fill.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_widgets.h"

// LES BRIQUES DE PEINTURE DE LA FENÊTRE DES SUCCÈS.
//
// En-tête PRIVÉ. Tout se pose en PIXELS D'ART depuis une origine, et l'échelle
// de l'interface s'applique à chaque coordonnée : la fenêtre est un dessin à
// position fixe, pas une disposition qui coule.
namespace ro::ui::achievement {

// ── LES COULEURS ────────────────────────────────────────────────────────────

inline constexpr ImU32 kWhite = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
inline constexpr ImU32 kBlack = IM_COL32(0x00, 0x00, 0x00, 0xFF);
// L'arbre : racine choisie, racine au repos, sous-ligne.
inline constexpr ImU32 kTabChosenText = IM_COL32(0x10, 0x18, 0x9C, 0xFF);
inline constexpr ImU32 kTabIdleText = IM_COL32(0x5A, 0x6B, 0x9C, 0xFF);
inline constexpr ImU32 kSubtabText = IM_COL32(0x59, 0x6A, 0x9B, 0xFF);
// Le bandeau : titre et compteurs de l'onglet, puis le rang.
inline constexpr ImU32 kUpperText = IM_COL32(0x08, 0x27, 0x58, 0xFF);
inline constexpr ImU32 kRankText = IM_COL32(0x58, 0x4C, 0x04, 0xFF);
// Les noms, les scores, les résumés.
inline constexpr ImU32 kNameText = IM_COL32(0x39, 0x52, 0xA5, 0xFF);
// Les dates, les compteurs du résumé, le survol d'un nom.
inline constexpr ImU32 kDateText = IM_COL32(0xA5, 0x5A, 0x63, 0xFF);
// La description, un objectif atteint, le résumé d'un succès récent.
inline constexpr ImU32 kPaleText = IM_COL32(0x7D, 0x97, 0xC9, 0xFF);
inline constexpr ImU32 kObjectiveUntouchedText = IM_COL32(0x36, 0x57, 0x99, 0xFF);
inline constexpr ImU32 kObjectiveStartedText = IM_COL32(0xE6, 0x89, 0x41, 0xFF);
// Le nom d'un succès récent, bouton-texte.
inline constexpr ImU32 kRecentNameText = IM_COL32(0x36, 0x78, 0x99, 0xFF);
// L'en-tête des succès récents.
inline constexpr ImU32 kRecentHeaderText = kTabIdleText;
// Une ligne atteinte du panneau de suivi.
inline constexpr ImU32 kTrackerReachedText = IM_COL32(0x6F, 0xFF, 0x0D, 0xFF);

// LES COULEURS D'UNE JAUGE : remplissage, fond, cadre extérieur, intérieur.
struct GaugeColors {
  ImU32 fill = IM_COL32(0x52, 0x7B, 0xCE, 0xFF);
  ImU32 background = kWhite;
  ImU32 outer = IM_COL32(0xE7, 0xE7, 0xEF, 0xFF);
  ImU32 inner = IM_COL32(0xAD, 0xAD, 0xAD, 0xFF);
};
// Celles du bandeau sont les couleurs par défaut.
inline constexpr GaugeColors kUpperGauge{};
// Celles des objectifs et du résumé.
inline constexpr GaugeColors kObjectiveGauge{
    .fill = IM_COL32(0x71, 0x8A, 0xC7, 0xFF),
    .background = kWhite,
    .outer = kWhite,
    .inner = IM_COL32(0x96, 0xA8, 0xDA, 0xFF),
};

// ── LES TAILLES DE TEXTE, en pixels d'art ───────────────────────────────────

inline constexpr float kTextLarge = 12.0F;
inline constexpr float kTextMedium = 11.0F;
inline constexpr float kTextSmall = 10.0F;
inline constexpr float kTextBanner = 20.0F;

// ── LES MESURES COMMUNES ────────────────────────────────────────────────────

// Le cadre d'une jauge : un pixel, deux fois.
inline constexpr float kGaugeFramePixels = 1.0F;
inline constexpr float kGaugeInsetPixels = 2.0F;
// L'ascenseur : sa largeur, celle de ses flèches, et la hauteur d'un morceau de
// pouce.
inline constexpr float kScrollWidth = 11.0F;
inline constexpr float kScrollArrow = 11.0F;
inline constexpr float kScrollThumbPiece = 4.0F;
// Le décalage d'un contour ou d'une ombre.
inline constexpr float kOutlinePixels = 1.0F;
inline constexpr float kShadowPixels = 1.0F;

// Un contrôle du client réagit à l'appui ; un bouton dessiné, au relâché sur
// lui.
inline constexpr ImGuiButtonFlags kOnRelease = ImGuiButtonFlags_MouseButtonLeft;
inline constexpr ImGuiButtonFlags kOnPress =
    kOnRelease | static_cast<ImGuiButtonFlags>(ImGuiButtonFlags_PressedOnClick);

// Un rectangle cliquable, tel qu'ImGui l'a vu à cette image.
struct HitResult {
  bool pressed = false;
  bool hovered = false;
  bool held = false;
  // La souris, en pixels d'art depuis le coin du rectangle.
  ImVec2 mouse;
};

class Painter {
 public:
  // `paper` : celui sur lequel l'art et la palette se posent. Blanc par défaut,
  // où rien ne change.
  Painter(ImDrawList* into, ImVec2 origin, float scale, ImU32 paper = IM_COL32_WHITE)
      : into_(into), origin_(origin), scale_(scale), paper_(paper) {}

  [[nodiscard]] ImDrawList* into() const { return into_; }
  [[nodiscard]] float scale() const { return scale_; }

  // Le point d'écran d'une coordonnée d'art, arrondi au pixel : l'art est
  // échantillonné au plus proche, et un demi-pixel en fait baver les bords.
  [[nodiscard]] ImVec2 At(float x, float y) const {
    return {std::floor(origin_.x + x * scale_), std::floor(origin_.y + y * scale_)};
  }

  // Le même peintre, l'origine déplacée : un contrôle posé dans son parent.
  [[nodiscard]] Painter Child(float x, float y) const {
    return {into_, ImVec2(origin_.x + x * scale_, origin_.y + y * scale_), scale_, paper_};
  }

  void PushClip(float x, float y, float width, float height) const {
    into_->PushClipRect(At(x, y), At(x + width, y + height), true);
  }
  void PopClip() const { into_->PopClipRect(); }

  // LE PAPIER : l'art et la palette des succès, choisis pour du blanc, s'y
  // traduisent. Voir `PaintOnPaper` et `OnPaper`.
  [[nodiscard]] ImU32 paper() const { return paper_; }
  // Une couleur de la palette, posée sur le papier.
  [[nodiscard]] ImU32 Laid(ImU32 color) const { return OnPaper(color, paper_); }
  // Un texte de la palette, posé sur le papier et gardé lisible.
  [[nodiscard]] ImU32 Ink(ImU32 color) const { return ReadableOn(OnPaper(color, paper_), paper_); }

  // Une pièce à sa taille d'art, telle : un objet peint, une icône. Absente,
  // rien.
  void Image(const SkinImage& image, float x, float y) const {
    if (!image.usable()) {
      return;
    }
    into_->AddImage(image.id, At(x, y), At(x + image.width, y + image.height));
  }

  // Une pièce d'interface à sa taille d'art, peinte sur du blanc : posée sur le
  // papier.
  void Art(const SkinImage& image, float x, float y) const {
    if (!image.usable()) {
      return;
    }
    PaintOnPaper(into_, image, At(x, y), At(x + image.width, y + image.height), paper_);
  }

  // La même, ramenée à `height` en retirant des lignes EN SON MILIEU : le haut
  // et le bas gardent leurs bords, sans l'étirement qui les brouillerait. Pas
  // plus basse qu'elle, elle se pose à sa taille.
  void ArtCut(const SkinImage& image, float x, float y, float height) const {
    if (!image.usable()) {
      return;
    }
    if (height >= image.height) {
      Art(image, x, y);
      return;
    }
    const float upper = std::floor(height * 0.5F);
    const float lower = height - upper;
    PaintOnPaper(into_, image, At(x, y), At(x + image.width, y + upper), paper_, ImVec2(0.0F, 0.0F),
                 ImVec2(1.0F, upper / image.height));
    PaintOnPaper(into_, image, At(x, y + upper), At(x + image.width, y + height), paper_,
                 ImVec2(0.0F, 1.0F - lower / image.height), ImVec2(1.0F, 1.0F));
  }

  void Fill(float x, float y, float width, float height, ImU32 color) const {
    if (width <= 0.0F || height <= 0.0F) {
      return;
    }
    into_->AddRectFilled(At(x, y), At(x + width, y + height), color);
  }

  // Un cadre d'un pixel d'art, en dedans du rectangle.
  void Frame(float x, float y, float width, float height, ImU32 color) const {
    Fill(x, y, width, kGaugeFramePixels, color);
    Fill(x, y + height - kGaugeFramePixels, width, kGaugeFramePixels, color);
    Fill(x, y, kGaugeFramePixels, height, color);
    Fill(x + width - kGaugeFramePixels, y, kGaugeFramePixels, height, color);
  }

  // La largeur d'un texte, en pixels d'art. Le faux gras ajoute son décalage.
  [[nodiscard]] float TextWidth(std::string_view text, float size, bool bold = false) const {
    const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size * scale_, FLT_MAX, 0.0F, text.data(),
                                                          text.data() + text.size());
    return extent.x / scale_ + (bold ? kFakeBoldPixels : 0.0F);
  }
  [[nodiscard]] float TextHeight(float size) const {
    return ImGui::GetFont()->CalcTextSizeA(size * scale_, FLT_MAX, 0.0F, "A").y / scale_;
  }

  // Le texte, son coin haut-gauche en (x, y). Le gras est le faux gras du
  // client : le même texte, repassé un pixel plus loin.
  void Text(float x, float y, std::string_view text, float size, ImU32 color,
            bool bold = false) const {
    if (text.empty()) {
      return;
    }
    ImFont* const font = ImGui::GetFont();
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    into_->AddText(font, size * scale_, At(x, y), color, begin, end);
    if (bold) {
      into_->AddText(font, size * scale_, At(x + kFakeBoldPixels, y), color, begin, end);
    }
  }

  void TextRight(float right, float y, std::string_view text, float size, ImU32 color,
                 bool bold = false) const {
    Text(right - TextWidth(text, size, bold), y, text, size, color, bold);
  }

  // Centré dans [x, x + width], l'écart tronqué au pixel.
  void TextCentered(float x, float width, float y, std::string_view text, float size, ImU32 color,
                    bool bold = false) const {
    Text(x + std::trunc((width - TextWidth(text, size, bold)) * 0.5F), y, text, size, color, bold);
  }

  // Le contour : quatre passes en croix, puis le texte.
  void TextOutlined(float x, float y, std::string_view text, float size, ImU32 color, ImU32 outline,
                    bool bold = false) const {
    Text(x - kOutlinePixels, y, text, size, outline, bold);
    Text(x + kOutlinePixels, y, text, size, outline, bold);
    Text(x, y - kOutlinePixels, text, size, outline, bold);
    Text(x, y + kOutlinePixels, text, size, outline, bold);
    Text(x, y, text, size, color, bold);
  }

  // L'ombre portée, un pixel en bas à droite, puis le texte.
  void TextShadowed(float x, float y, std::string_view text, float size, ImU32 color, ImU32 shadow,
                    bool bold = false) const {
    Text(x + kShadowPixels, y + kShadowPixels, text, size, shadow, bold);
    Text(x, y, text, size, color, bold);
  }

  // LA JAUGE : deux cadres, le fond, puis le remplissage, qui tient dans
  // l'intérieur — les deux cadres déduits — à toute part. Rien du tout quand le
  // compte dépasse le maximum.
  void Gauge(float x, float y, float width, float height, std::int64_t current,
             std::int64_t maximum, const GaugeColors& colors) const {
    const RoundedBox interior =
        AchievementGaugeInterior(ImVec2(x, y), ImVec2(x + width, y + height));
    const std::optional<AxisBox> filled = AchievementGaugeFillBox(interior, current, maximum);
    if (!filled.has_value()) {
      return;
    }
    Frame(x, y, width, height, Laid(colors.outer));
    Frame(x + kGaugeFramePixels, y + kGaugeFramePixels, width - 2.0F * kGaugeFramePixels,
          height - 2.0F * kGaugeFramePixels, Laid(colors.inner));
    Fill(interior.min.x, interior.min.y, interior.max.x - interior.min.x,
         interior.max.y - interior.min.y, Laid(colors.background));
    const RoundedBox shown{.min = At(interior.min.x, interior.min.y),
                           .max = At(interior.max.x, interior.max.y),
                           .rounding = interior.rounding};
    FillRoundedBoxWithin(
        *into_, shown,
        AxisBox{.min = At(filled->min.x, filled->min.y), .max = At(filled->max.x, filled->max.y)},
        Laid(colors.fill));
  }

  // UN RECTANGLE CLIQUABLE, sans dessin. Par défaut il réagit à l'APPUI, comme
  // les contrôles du client ; un bouton dessiné passe `kOnRelease`.
  [[nodiscard]] HitResult Hit(const char* id, float x, float y, float width, float height,
                              ImGuiButtonFlags flags = kOnPress) const {
    HitResult result;
    const ImVec2 min = At(x, y);
    const ImVec2 max = At(x + width, y + height);
    if (max.x <= min.x || max.y <= min.y) {
      return result;
    }
    ImGui::SetCursorScreenPos(min);
    result.pressed = ImGui::InvisibleButton(id, ImVec2(max.x - min.x, max.y - min.y), flags);
    result.hovered = ImGui::IsItemHovered();
    result.held = ImGui::IsItemActive();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    result.mouse = ImVec2((mouse.x - min.x) / scale_, (mouse.y - min.y) / scale_);
    return result;
  }

  // Les crans de molette sur ce rectangle, vers le bas positifs. Zéro hors de
  // lui, ou quand la fenêtre n'est pas sous la souris.
  [[nodiscard]] int WheelSteps(float x, float y, float width, float height) const {
    if (!ImGui::IsWindowHovered() ||
        !ImGui::IsMouseHoveringRect(At(x, y), At(x + width, y + height))) {
      return 0;
    }
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0.0F) {
      return -1;
    }
    return wheel < 0.0F ? 1 : 0;
  }

 private:
  ImDrawList* into_;
  ImVec2 origin_;
  float scale_;
  ImU32 paper_;
};

// ── L'ASCENSEUR VERTICAL ────────────────────────────────────────────────────

struct ScrollbarModel {
  float x = 0.0F;
  float y = 0.0F;
  float height = 0.0F;
  int position = 0;
  int range = 0;
  int page = 0;
  int total = 0;
  // Ce qu'un clic dans la piste, hors du pouce, fait avancer.
  int track_step = 1;
};

// LE GESTE : les flèches avancent d'un, la piste d'un pas, le pouce se tire.
// Rend la position qui en résulte. ⚠ À SOUMETTRE AVANT ce qu'il recouvre :
// ImGui donne la souris au premier rectangle soumis.
[[nodiscard]] inline int ScrollbarInput(const Painter& paint, const char* id,
                                        const ScrollbarModel& bar) {
  if (bar.range <= 0) {
    return bar.position;
  }
  const HitResult hit = paint.Hit(id, bar.x, bar.y, kScrollWidth, bar.height);
  ImGuiStorage* const storage = ImGui::GetStateStorage();
  const ImGuiID grab_id = ImGui::GetID(id);
  // Un pouce lâché se note par une prise négative.
  constexpr float kNoGrab = -1.0F;
  const float grab = storage->GetFloat(grab_id, kNoGrab);
  const AchievementThumb thumb =
      AchievementScrollThumb(bar.height, bar.position, bar.range, bar.page, bar.total);
  int position = bar.position;
  const float local_y = hit.mouse.y;
  if (hit.pressed) {
    if (local_y < kScrollArrow) {
      position -= 1;
    } else if (local_y >= bar.height - kScrollArrow) {
      position += 1;
    } else if (local_y < thumb.top) {
      position -= bar.track_step;
    } else if (local_y >= thumb.top + thumb.length) {
      position += bar.track_step;
    } else {
      storage->SetFloat(grab_id, local_y - thumb.top);
    }
  } else if (hit.held && grab >= 0.0F) {
    const float travel = bar.height - 2.0F * kScrollArrow - thumb.length;
    if (travel > 0.0F) {
      position = static_cast<int>(
          std::lround((local_y - grab - kScrollArrow) * static_cast<float>(bar.range) / travel));
    }
  }
  if (!hit.held && !hit.pressed && grab >= 0.0F) {
    storage->SetFloat(grab_id, kNoGrab);
  }
  return ClampAchievementScroll(position, bar.range);
}

// LE DESSIN : la piste, les flèches, le pouce en trois morceaux. Rien quand
// tout tient.
inline void PaintScrollbar(const Painter& paint, const AchievementArt& art,
                           const ScrollbarModel& bar) {
  if (bar.range <= 0) {
    return;
  }
  const int tiles = static_cast<int>(bar.height / kScrollArrow);
  for (int tile = 0; tile < tiles; ++tile) {
    paint.Art(art.scroll_bg_mid, bar.x, bar.y + kScrollArrow * static_cast<float>(tile));
  }
  const float bottom = bar.y + bar.height - kScrollArrow;
  paint.Art(art.scroll_bg_down, bar.x, bottom);
  paint.Art(art.scroll_arrow_up, bar.x, bar.y);
  paint.Art(art.scroll_arrow_down, bar.x, bottom);
  const AchievementThumb thumb =
      AchievementScrollThumb(bar.height, bar.position, bar.range, bar.page, bar.total);
  const float top = bar.y + thumb.top;
  paint.Art(art.scroll_thumb_up, bar.x, top);
  for (float piece = top + kScrollThumbPiece; piece < top + thumb.length - kScrollThumbPiece;
       piece += kScrollThumbPiece) {
    paint.Art(art.scroll_thumb_mid, bar.x, piece);
  }
  paint.Art(art.scroll_thumb_down, bar.x, top + thumb.length - kScrollThumbPiece);
}

// ── LE RETOUR À LA LIGNE ────────────────────────────────────────────────────

// La longueur en octets du caractère UTF-8 qui commence par `lead`. Un octet de
// suite isolé compte pour un : on avance toujours.
[[nodiscard]] inline std::size_t Utf8Length(char lead) {
  const auto byte = static_cast<unsigned char>(lead);
  constexpr unsigned char kTwoBytes = 0xC0;
  constexpr unsigned char kThreeBytes = 0xE0;
  constexpr unsigned char kFourBytes = 0xF0;
  if (byte >= kFourBytes) {
    return 4;
  }
  if (byte >= kThreeBytes) {
    return 3;
  }
  return byte >= kTwoBytes ? 2 : 1;
}

// Les lignes d'un texte replié AU MOT dans `width` pixels d'art, les sauts de
// ligne respectés, les blancs de tête des lignes repliées retirés.
[[nodiscard]] inline std::vector<std::string_view> WrapLines(const Painter& paint,
                                                             std::string_view text, float size,
                                                             float width) {
  std::vector<std::string_view> lines;
  ImFont* const font = ImGui::GetFont();
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t newline = text.find('\n', start);
    const std::size_t stop = newline == std::string_view::npos ? text.size() : newline;
    const char* cursor = text.data() + start;
    const char* const end = text.data() + stop;
    if (cursor == end) {
      lines.emplace_back();
    }
    while (cursor < end) {
      const char* cut =
          font->CalcWordWrapPosition(size * paint.scale(), cursor, end, width * paint.scale());
      if (cut <= cursor) {
        // Pas même un caractère ne tient : il passe seul, entier.
        cut = cursor + Utf8Length(*cursor);
      }
      lines.emplace_back(cursor, static_cast<std::size_t>(cut - cursor));
      cursor = cut;
      while (cursor < end && *cursor == ' ') {
        ++cursor;
      }
    }
    if (newline == std::string_view::npos) {
      break;
    }
    start = newline + 1;
  }
  return lines;
}

// ── LES VOLETS, écrits dans `achievement_panes.cpp` ──────────────────────────
//
// Chacun reçoit un peintre dont l'origine est le coin de son volet.

void DrawAchievementList(const Painter& paint, AchievementWindowState& state,
                         const AchievementView& view, AchievementAction& action);
void DrawAchievementDetail(const Painter& paint, const AchievementView& view,
                           AchievementAction& action);
void DrawAchievementSummary(const Painter& paint, const AchievementView& view,
                            AchievementAction& action);

// L'art de la vue, ou un art vide : une pièce absente ne se peint pas.
[[nodiscard]] const AchievementArt& ArtOf(const AchievementView& view);

// La marge d'un menu de réglages autour de ses entrées, et son liseré : posées
// avant son ouverture, retirées après sa fermeture.
void PushSettingsMenuFrame();
void PopSettingsMenuFrame();

}  // namespace ro::ui::achievement
