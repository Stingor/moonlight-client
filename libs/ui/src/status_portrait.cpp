// Le portrait de statut : quatre cadres, en deux groupes de frères.

#include "ro/ui/status_portrait.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/hud_caption.h"

namespace ro::ui {
namespace {

// L'identité ImGui de chaque cadre, stable et unique.
static_assert(kPortraitPartCount <= static_cast<std::size_t>(kHudMostSiblings),
              "les quatre cadres doivent tenir dans une rangée de frères");
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

// UN GROUPE ET CE SUR QUOI IL S'AIMANTE, rangés comme `HudSiblings` les veut :
// les cadres du groupe en tête — eux seuls partent d'un bloc au CTRL —, puis
// ceux de l'autre groupe et les jauges, qui ne prêtent que leurs bords.
struct GroupFrames {
  std::array<HudRect, kHudMostSiblings> rects{};
  std::array<bool, kHudMostSiblings> shown{};
  // Le cadre que désigne chacun des rangs du groupe, en tête.
  std::array<std::size_t, kPortraitPartCount> parts{};
  std::size_t movable = 0;
  std::size_t count = 0;
};

[[nodiscard]] GroupFrames GatherGroup(PortraitGroup group, const StatusPortraitLook& look,
                                      std::span<const HudRect> neighbours) {
  GroupFrames frames;
  for (std::size_t rank = 0; rank < kPortraitPartCount; ++rank) {
    const auto part = static_cast<PortraitPart>(rank);
    if (PortraitGroupOf(part) == group) {
      frames.parts[frames.movable] = rank;
      frames.rects[frames.count] = look.parts[rank].rect;
      frames.shown[frames.count] = look.PartShown(part);
      ++frames.count;
      frames.movable = frames.count;
    }
  }
  for (std::size_t rank = 0; rank < kPortraitPartCount; ++rank) {
    const auto part = static_cast<PortraitPart>(rank);
    if (PortraitGroupOf(part) != group) {
      frames.rects[frames.count] = look.parts[rank].rect;
      frames.shown[frames.count] = look.PartShown(part);
      ++frames.count;
    }
  }
  for (const HudRect& neighbour : neighbours) {
    if (frames.count >= frames.rects.size()) {
      break;
    }
    frames.rects[frames.count] = neighbour;
    frames.shown[frames.count] = true;
    ++frames.count;
  }
  return frames;
}

void DrawGroup(PortraitGroup group, const StatusPortraitView& view, const AlignGrid& grid,
               StatusPortraitLook& look, HudGrip& grip, StatusPortraitAnswer& answer) {
  const PortraitGroupLook& settings = look.group(group);
  if (!settings.shown) {
    return;
  }
  GroupFrames frames = GatherGroup(group, look, view.neighbours);

  for (std::size_t rank = 0; rank < frames.movable; ++rank) {
    const std::size_t index = frames.parts[rank];
    const PortraitPartLook& mine = look.parts[index];
    if (!mine.shown) {
      continue;
    }
    const auto part = static_cast<PortraitPart>(index);

    HudFrameLook frame;
    frame.locked = view.locked;
    frame.border = settings.border;
    frame.rounding = std::clamp(mine.rounding, 0.0F, kPortraitRoundingMost);
    frame.background = mine.background;
    frame.grid = &grid;
    frame.sticky = settings.sticky;

    HudSiblings siblings;
    siblings.frames = std::span<HudRect>(frames.rects.data(), frames.count);
    siblings.shown = std::span<const bool>(frames.shown.data(), frames.count);
    siblings.mine = static_cast<int>(rank);
    siblings.movable = static_cast<int>(frames.movable);

    bool moved = false;
    // `frames.rects[rank]` lui-même : le cadre y écrit, et écrit aussi chez
    // ses frères en déplacement de bloc.
    if (BeginHudFrame(kPartIds[index], frames.rects[rank], frame, siblings, grip, &moved)) {
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

  // Les frères emportés par un bloc ont bougé aussi : tout le groupe se
  // recopie. Les autres n'ont fait que prêter leurs bords.
  for (std::size_t rank = 0; rank < frames.movable; ++rank) {
    look.parts[frames.parts[rank]].rect = frames.rects[rank];
  }
}

}  // namespace

PortraitGroup PortraitGroupOf(PortraitPart part) noexcept {
  return part == PortraitPart::kHead ? PortraitGroup::kHead : PortraitGroup::kLabels;
}

bool StatusPortraitLook::AnyShown() const noexcept {
  return std::ranges::any_of(groups, &PortraitGroupLook::shown);
}

bool StatusPortraitLook::PartShown(PortraitPart part) const noexcept {
  return group(PortraitGroupOf(part)).shown && parts[static_cast<std::size_t>(part)].shown;
}

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
  if (!view.present) {
    return;
  }
  // Le groupe dessiné en second voit déjà où le premier a posé ses cadres.
  DrawGroup(PortraitGroup::kHead, view, grid, look, grip, answer);
  DrawGroup(PortraitGroup::kLabels, view, grid, look, grip, answer);
}

}  // namespace ro::ui
