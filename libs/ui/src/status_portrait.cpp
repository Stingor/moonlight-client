// Le portrait de statut : quatre cadres frères.

#include "ro/ui/status_portrait.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/hud_caption.h"

namespace ro::ui {
namespace {

// L'identité ImGui de chaque cadre, stable et unique.
static_assert(kPortraitPartCount <= static_cast<std::size_t>(kHudMostSiblings),
              "les quatre cadres doivent tenir dans un groupe de frères");
constexpr std::array<const char*, kPortraitPartCount> kPartIds = {
    "##status_portrait_head",
    "##status_portrait_name",
    "##status_portrait_class",
    "##status_portrait_level",
};

// La géométrie d'origine : le portrait en haut à gauche, les trois étiquettes
// empilées dessous.
constexpr std::array<HudRect, kPortraitPartCount> kDefaultRects = {{
    {.x = 60.0F, .y = 60.0F, .width = 100.0F, .height = 100.0F},
    {.x = 60.0F, .y = 164.0F, .width = 110.0F, .height = 20.0F},
    {.x = 60.0F, .y = 186.0F, .width = 110.0F, .height = 20.0F},
    {.x = 60.0F, .y = 208.0F, .width = 110.0F, .height = 20.0F},
}};
constexpr ImU32 kFrameBackground = IM_COL32(13, 13, 18, 199);
constexpr ImU32 kNameText = IM_COL32(255, 209, 77, 255);
constexpr ImU32 kLevelText = IM_COL32(92, 199, 255, 255);
constexpr ImU32 kModelText = IM_COL32_WHITE;

// Ce que le cadre écrit faute de pseudo.
constexpr const char* kUnknownName = "?";
// Les deux parts du niveau. ⚠ EN ANGLAIS DANS TOUTES LES LANGUES : c'est le
// vocabulaire du jeu.
constexpr const char* kBaseWord = "Base ";
constexpr const char* kJobWord = "Job ";
constexpr const char* kLabelledJoint = " · ";
constexpr const char* kSlashedJoint = "/";

void PaintHead(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right,
               const StatusPortraitView& view, float pixels) {
  if (view.portrait.usable()) {
    // LES EFFETS DE CHAPEAU ENCADRENT L'IMAGE, dans le rectangle même où elle
    // tombe : ce que la partie peint avant les corps passe dessous, le reste
    // dessus. Voir `DollOverlay`.
    DollOverlayCall call{
        .painter = view.overlay.painter,
        .min = ImVec2(top_left.x + kPortraitImageInset, top_left.y + kPortraitImageInset),
        .max = ImVec2(bottom_right.x - kPortraitImageInset, bottom_right.y - kPortraitImageInset),
        .pass = DollOverlayPass::kBehind};
    PaintDollOverlay(view.overlay, call);
    if (view.overlay.paint == nullptr || !view.body_overlaid) {
      into.AddImage(view.portrait.id, call.min, call.max);
    }
    call.pass = DollOverlayPass::kFront;
    PaintDollOverlay(view.overlay, call);
    return;
  }
  if (view.model) {
    // Ces classes n'ont aucun sprite : le dire vaut mieux qu'un cadre vide.
    PaintHudCaption(into, top_left, bottom_right, core::Tr("(modèle 3D)"), kModelText, pixels);
  }
}

[[nodiscard]] std::string CaptionOf(PortraitPart part, const StatusPortraitView& view,
                                    const StatusPortraitLook& look) {
  switch (part) {
    case PortraitPart::kName:
      return view.name.empty() ? std::string(kUnknownName) : std::string(view.name);
    case PortraitPart::kClass:
      return std::string(view.job);
    case PortraitPart::kLevel:
      return PortraitLevelText(view.base_level, view.job_level, LevelFormatOf(look.level_format));
    case PortraitPart::kHead:
      break;
  }
  return {};
}

}  // namespace

LevelFormat LevelFormatOf(int stored) noexcept {
  if (stored < 0 || stored > static_cast<int>(LevelFormat::kBaseOnly)) {
    return LevelFormat::kSlashed;
  }
  return static_cast<LevelFormat>(stored);
}

std::string PortraitLevelText(std::optional<std::int32_t> base_level,
                              std::optional<std::int32_t> job_level, LevelFormat format) {
  const std::string base = base_level.has_value() ? std::to_string(*base_level) : std::string();
  const std::string job = job_level.has_value() ? std::to_string(*job_level) : std::string();
  switch (format) {
    case LevelFormat::kBaseOnly:
      return base;
    case LevelFormat::kLabelled: {
      std::string text = base.empty() ? std::string() : kBaseWord + base;
      if (!job.empty()) {
        text += (text.empty() ? std::string() : std::string(kLabelledJoint)) + kJobWord + job;
      }
      return text;
    }
    case LevelFormat::kSlashed:
      break;
  }
  // La barre reste devant le niveau de job seul : c'est elle qui dit lequel
  // des deux on lit.
  return job.empty() ? base : base + kSlashedJoint + job;
}

HudRect DefaultPortraitPartRect(PortraitPart part) {
  return kDefaultRects[static_cast<std::size_t>(part)];
}

std::array<PortraitPartLook, kPortraitPartCount> DefaultPortraitParts() {
  std::array<PortraitPartLook, kPortraitPartCount> parts{};
  for (std::size_t rank = 0; rank < kPortraitPartCount; ++rank) {
    parts[rank].rect = kDefaultRects[rank];
    parts[rank].background = kFrameBackground;
  }
  parts[static_cast<std::size_t>(PortraitPart::kName)].text = kNameText;
  parts[static_cast<std::size_t>(PortraitPart::kLevel)].text = kLevelText;
  return parts;
}

void DrawStatusPortrait(const StatusPortraitView& view, const AlignGrid& grid,
                        StatusPortraitLook& look, HudGrip& grip, StatusPortraitAnswer& answer) {
  if (!look.shown || !view.present) {
    return;
  }

  // LES QUATRE CADRES EN TÊTE, puis les voisins d'un autre HUD : ceux-là ne
  // prêtent que leurs bords. Voir `HudSiblings::movable`.
  std::array<HudRect, kHudMostSiblings> rects{};
  std::array<bool, kHudMostSiblings> shown{};
  for (std::size_t rank = 0; rank < kPortraitPartCount; ++rank) {
    rects[rank] = look.parts[rank].rect;
    shown[rank] = look.parts[rank].shown;
  }
  const std::size_t count = std::min(rects.size(), kPortraitPartCount + view.neighbours.size());
  for (std::size_t rank = kPortraitPartCount; rank < count; ++rank) {
    rects[rank] = view.neighbours[rank - kPortraitPartCount];
    shown[rank] = true;
  }

  for (std::size_t rank = 0; rank < kPortraitPartCount; ++rank) {
    const PortraitPartLook& mine = look.parts[rank];
    if (!mine.shown) {
      continue;
    }
    const auto part = static_cast<PortraitPart>(rank);

    HudFrameLook frame;
    frame.locked = look.locked;
    frame.border = look.border;
    frame.rounding = std::clamp(mine.rounding, 0.0F, kPortraitRoundingMost);
    frame.background = mine.background;
    frame.grid = &grid;
    frame.sticky = look.sticky;

    HudSiblings siblings;
    siblings.frames = std::span<HudRect>(rects.data(), count);
    siblings.shown = std::span<const bool>(shown.data(), count);
    siblings.mine = static_cast<int>(rank);
    siblings.movable = static_cast<int>(kPortraitPartCount);

    bool moved = false;
    // `rects[rank]` lui-même : le cadre y écrit, et écrit aussi chez ses frères
    // en déplacement de bloc.
    if (BeginHudFrame(kPartIds[rank], rects[rank], frame, siblings, grip, &moved)) {
      ImDrawList* into = ImGui::GetWindowDrawList();
      const ImVec2 top_left = ImGui::GetWindowPos();
      const ImVec2 size = ImGui::GetWindowSize();
      const ImVec2 bottom_right(top_left.x + size.x, top_left.y + size.y);
      const float pixels =
          ImGui::GetFontSize() *
          std::clamp(mine.text_scale, kPortraitTextScaleLeast, kPortraitTextScaleMost);
      if (into != nullptr) {
        if (part == PortraitPart::kHead) {
          PaintHead(*into, top_left, bottom_right, view, pixels);
        } else {
          const std::string caption = CaptionOf(part, view, look);
          PaintHudCaption(*into, top_left, bottom_right, caption.c_str(), mine.text, pixels);
        }
      }
    }
    EndHudFrame(grip);
    answer.moved = answer.moved || moved;
  }

  // Les frères emportés par un bloc ont bougé aussi : tous se recopient.
  for (std::size_t rank = 0; rank < kPortraitPartCount; ++rank) {
    look.parts[rank].rect = rects[rank];
  }
}

}  // namespace ro::ui
