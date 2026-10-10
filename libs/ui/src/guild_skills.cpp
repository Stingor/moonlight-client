// LE SOUS-ONGLET « COMPÉTENCES » : la grille, ou la liste détaillée.
//
// 🔴 ICI, MONTER UN NIVEAU PART AUSSITÔT, sans réservation comme au grimoire —
// une compétence de guilde se monte immédiatement chez le client d'origine. Le
// clic simple doit donc se départager du double-clic qui LANCE : sans cela, un
// double-clic pour lancer dépenserait un point au passage.

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/staff_skill_id.h"

#include "guild_panes.h"
#include "skill_cell.h"
#include "skill_chain.h"

namespace ro::ui {
namespace {

// ── LA GRILLE ───────────────────────────────────────────────────────────────
constexpr float kCellWidthPixels = 72.0F;
constexpr float kCellHeightPixels = 78.0F;
constexpr float kIconPixels = 40.0F;
constexpr float kCellGapPixels = 10.0F;
constexpr float kIconTopPixels = 7.0F;
constexpr float kLevelGapPixels = 2.0F;
constexpr float kCellRounding = 4.0F;
constexpr float kDragIconPixels = 24.0F;
// La profondeur de l'arbre ne dépasse jamais ce nombre de lignes.
constexpr int kDepthRows = 16;
constexpr ImU32 kHotFill = IM_COL32(255, 255, 255, 28);
constexpr ImU32 kFocusEdge = IM_COL32(255, 205, 105, 220);
constexpr ImU32 kLinkedEdge = IM_COL32(120, 160, 255, 200);
constexpr ImU32 kRaisableEdge = IM_COL32(120, 200, 130, 170);
// LE FILET DES CASES : plein pour la case survolée et ses liées, un peu moins
// pour une compétence qu'on peut monter.
constexpr float kFocusEdgePixels = 2.0F;
constexpr float kRaisableEdgePixels = 1.5F;
constexpr ImU32 kLockedGridTint = IM_COL32(105, 105, 115, 165);
constexpr ImU32 kLevelHalo = IM_COL32(255, 255, 255, 190);
constexpr ImU32 kLearnedLevel = IM_COL32(20, 20, 25, 255);
constexpr ImU32 kUnlearnedLevel = IM_COL32(110, 110, 122, 255);

// ── LA LISTE ────────────────────────────────────────────────────────────────
constexpr float kTreeStepPixels = 20.0F;
// La marge de gauche N'EST PAS DÉCORATIVE : c'est la place des liens.
constexpr float kTreeGutterPixels = 16.0F;
constexpr float kLinkStubPixels = 2.0F;
constexpr float kLinkDotOffsetPixels = 3.0F;
constexpr float kHotLinkThickness = 2.5F;
constexpr float kColdLinkThickness = 0.1F;
constexpr float kHotDotRadius = 3.0F;
constexpr float kColdDotRadius = 2.0F;
constexpr ImU32 kHotLink = IM_COL32(255, 205, 105, 255);
constexpr ImU32 kColdLink = IM_COL32(150, 155, 190, 100);
constexpr ImU32 kFocusRow = IM_COL32(120, 95, 35, 90);
constexpr ImU32 kLinkedRow = IM_COL32(70, 75, 120, 80);
constexpr ImU32 kLockedListTint = IM_COL32(110, 110, 110, 160);
constexpr float kToggleButtonPixels = 58.0F;
constexpr float kLevelColumnPixels = 54.0F;
constexpr float kCostColumnPixels = 56.0F;
constexpr float kUseColumnPixels = 46.0F;
constexpr float kRaiseColumnPixels = 30.0F;
constexpr float kUseButtonPixels = 40.0F;
constexpr float kRaiseButtonPixels = 24.0F;
constexpr std::uint32_t kMillisecondsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;

[[nodiscard]] const GuildSkillView* Find(std::span<const GuildSkillView> skills, std::uint16_t id) {
  const auto found = std::ranges::find_if(
      skills, [id](const GuildSkillView& skill) { return skill.cell.skill_id == id; });
  return found == skills.end() ? nullptr : &*found;
}

[[nodiscard]] bool DependsOn(std::span<const GuildSkillView> skills, std::uint16_t id,
                             std::uint16_t prerequisite) {
  const GuildSkillView* skill = Find(skills, id);
  if (skill == nullptr) {
    return false;
  }
  return std::ranges::any_of(skill->cell.needs, [prerequisite](const SkillNeedView& need) {
    return need.skill_id == prerequisite;
  });
}

[[nodiscard]] bool LinkedToFocus(std::span<const GuildSkillView> skills, std::uint16_t focus,
                                 std::uint16_t id) {
  return focus != 0 && id != focus &&
         (DependsOn(skills, id, focus) || DependsOn(skills, focus, id));
}

[[nodiscard]] bool ActiveSkill(const GuildSkillView& skill) {
  return !skill.locked && skill.active && skill.cell.learned > 0;
}

[[nodiscard]] std::string Label(const GuildSkillView& skill) {
  return skill.cell.name.empty() ? std::string(skill.technical.empty() ? "?" : skill.technical)
                                 : std::string(skill.cell.name);
}

// L'INFOBULLE COMMUNE AUX DEUX VUES : la grille n'affiche que l'icône, tout le
// reste se lit ici.
void SkillTooltip(const GuildSkillView& skill, int points, bool staff_allowed) {
  std::array<char, 192> line = {};
  std::string tip = Label(skill);
  if (!skill.locked && !skill.technical.empty()) {
    tip += "  (" + std::string(skill.technical) + ")";
  }
  const auto add = [&tip](const char* text) {
    tip += '\n';
    tip += text;
  };
  if (skill.cell.max_level > 0) {
    std::snprintf(line.data(), line.size(), core::Tr("Niveau %d / %d"), skill.cell.learned,
                  skill.cell.max_level);
    add(line.data());
  }
  if (!skill.locked && skill.cell.range_cells > 0) {
    std::snprintf(line.data(), line.size(), core::Tr("Portée : %d"), skill.cell.range_cells);
    add(line.data());
  }
  if (skill.cell.cooldown_ms > 0) {
    std::snprintf(line.data(), line.size(),
                  core::Tr("Encore %u s (lancer un skill de guilde les bloque tous les quatre)"),
                  (skill.cell.cooldown_ms + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond);
    add(line.data());
  }
  // LES PRÉREQUIS sont justement ce qui manque à une verrouillée : ils se
  // disent là où le joueur regarde quand il se demande pourquoi elle est grise.
  if (!skill.requirements.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("Requiert : %s"),
                  std::string(skill.requirements).c_str());
    add(line.data());
  }
  if (!skill.locked) {
    add(skill.active ? core::Tr("Active") : core::Tr("Passive (toujours active)"));
  }
  tip += '\n';
  if (skill.locked) {
    add(core::Tr("Verrouillée : prérequis non remplis."));
  } else if (skill.cell.castable) {
    add(core::Tr("Double-clic : lancer — clic droit : description — glisser vers une barre"));
  } else if (ActiveSkill(skill) && skill.wieldable) {
    add(core::Tr("Clic droit : description — glisser vers une barre"));
  } else {
    add(core::Tr("Clic droit : description"));
    // L'ARBRE SE CONSULTE EN LECTURE : dire pourquoi elle ne se lance pas.
    if (ActiveSkill(skill)) {
      add(core::Tr("Réservée au maître de guilde."));
    }
  }
  if (!skill.locked && skill.cell.raisable && points > 0) {
    add(core::Tr("Clic gauche : monter d'un niveau"));
  }
  SkinTooltipWithStaffSkillId(
      tip.c_str(), staff_allowed,
      StaffSkillId{.skill_id = skill.cell.skill_id, .idname = skill.technical});
}

// LE GLISSER VERS UNE BARRE : seulement ce qui se lance, au niveau appris, et
// par qui a le droit de le lancer — une case qui ne partirait jamais n'a rien
// à faire dans une barre.
void DragSource(const GuildSkillView& skill) {
  if (!ActiveSkill(skill) || !skill.wieldable ||
      !ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    return;
  }
  const SkillDrag carried{.skill_id = skill.cell.skill_id,
                          .level = static_cast<std::uint16_t>(skill.cell.learned)};
  ImGui::SetDragDropPayload(kSkillDragPayload, &carried, sizeof(carried));
  if (skill.cell.icon.usable()) {
    ImGui::Image(skill.cell.icon.id, ImVec2(GuildPx(kDragIconPixels), GuildPx(kDragIconPixels)));
    ImGui::SameLine();
  }
  ImGui::TextUnformatted(Label(skill).c_str());
  ImGui::EndDragDropSource();
}

// LE CLIC SIMPLE MONTE, LE DOUBLE LANCE. L'appui retient la case ; le verdict
// se prend au relâché, et la montée attend que le double-clic soit exclu.
void ClickGestures(GuildTabState& state, const GuildSkillView& skill, int points,
                   GuildAction& action) {
  const ImGuiIO& io = ImGui::GetIO();
  const std::uint16_t id = skill.cell.skill_id;
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    state.skill_pressed = id;
    if (io.MouseClickedLastCount[ImGuiMouseButton_Left] <= 1) {
      state.skill_press_clean = false;
    }
  }
  if (state.skill_pressed != id) {
    return;
  }
  // ⚠ LE GLISSEMENT NE VAUT PLUS RIEN PASSÉ L'IMAGE DU RELÂCHÉ : verdict ici.
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    const ImVec2 travel = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
    state.skill_press_clean =
        travel.x == 0.0F && travel.y == 0.0F && ImGui::GetDragDropPayload() == nullptr;
  }
  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && state.skill_press_clean) {
    if (skill.cell.castable) {
      action.verb = GuildVerb::kUseSkill;
      action.skill_id = id;
      action.skill_level = skill.cell.learned;
    }
    state.skill_pressed = 0;
  } else if (ImGui::IsMouseReleasedWithDelay(ImGuiMouseButton_Left, io.MouseDoubleClickTime) &&
             io.MouseClickedLastCount[ImGuiMouseButton_Left] == 1 && state.skill_press_clean &&
             !skill.locked && skill.cell.raisable && points > 0) {
    action.verb = GuildVerb::kSkillUp;
    action.skill_id = id;
    state.skill_pressed = 0;
  }
}

void DescribeOnRightClick(bool hot, std::uint16_t id, GuildAction& action) {
  if (hot && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    action.verb = GuildVerb::kDescribeSkill;
    action.skill_id = id;
  }
}

// ── LA GRILLE : une ligne par palier de prérequis ───────────────────────────
//
// Les compétences de guilde n'ont pas de place dans les tables du client : la
// grille se construit depuis l'arbre, les compétences d'un même palier côte à
// côte.
void DrawGrid(GuildTabState& state, const GuildView& view, bool smooth_icons, bool staff_allowed,
              GuildAction& action) {
  if (!ImGui::BeginChild("grille", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None)) {
    ImGui::EndChild();
    return;
  }
  const float cell_w = GuildPx(kCellWidthPixels);
  const float cell_h = GuildPx(kCellHeightPixels);
  const float icon = GuildPx(kIconPixels);
  const float gap = GuildPx(kCellGapPixels);
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const std::span<const GuildSkillView> skills = view.skills;
  std::vector<ImVec2> centers(skills.size(), ImVec2(-1.0F, -1.0F));
  std::vector<char> shown(skills.size(), 1);
  std::array<int, kDepthRows> columns = {};
  int used_rows = 0;
  int hovered = -1;
  std::uint16_t hovered_now = 0;
  const std::uint16_t focus = state.skill_hover;

  ImposeSkillIconSampler(into, smooth_icons);
  for (std::size_t index = 0; index < skills.size(); ++index) {
    const GuildSkillView& skill = skills[index];
    const int depth = std::clamp(skill.depth, 0, kDepthRows - 1);
    const int column = columns[static_cast<std::size_t>(depth)]++;
    used_rows = std::max(used_rows, depth + 1);
    const ImVec2 low(origin.x + static_cast<float>(column) * cell_w,
                     origin.y + static_cast<float>(depth) * cell_h);
    const ImVec2 high(low.x + cell_w - gap, low.y + cell_h - gap);
    centers[index] = ImVec2((low.x + high.x) * 0.5F, (low.y + high.y) * 0.5F);

    ImGui::PushID(skill.cell.skill_id);
    ImGui::SetCursorScreenPos(low);
    ImGui::InvisibleButton("case", ImVec2(cell_w - gap, cell_h - gap));
    const bool hot = ImGui::IsItemHovered();
    const float rounding = GuildPx(kCellRounding);
    if (hot) {
      hovered = static_cast<int>(index);
      into->AddRectFilled(low, high, kHotFill, rounding);
    }
    if (skill.cell.skill_id == focus) {
      into->AddRect(low, high, kFocusEdge, rounding, 0, GuildPx(kFocusEdgePixels));
    } else if (LinkedToFocus(skills, focus, skill.cell.skill_id)) {
      into->AddRect(low, high, kLinkedEdge, rounding, 0, GuildPx(kFocusEdgePixels));
    } else if (!skill.locked && skill.cell.raisable && view.skill_points > 0) {
      into->AddRect(low, high, kRaisableEdge, rounding, 0, GuildPx(kRaisableEdgePixels));
    }
    const ImVec2 icon_at(low.x + (cell_w - gap - icon) * 0.5F, low.y + GuildPx(kIconTopPixels));
    if (skill.cell.icon.usable()) {
      into->AddImage(skill.cell.icon.id, icon_at, ImVec2(icon_at.x + icon, icon_at.y + icon),
                     ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                     skill.locked ? kLockedGridTint : IM_COL32_WHITE);
    }
    // L'ATTENTE : les compétences de guilde se bloquent toutes les quatre
    // ensemble, plusieurs minutes durant — c'est LE cas où le voile sert.
    if (!skill.locked) {
      PaintSkillCooldown(into, skill.cell, icon_at, icon);
    }
    std::array<char, 24> level = {};
    if (skill.cell.max_level > 0) {
      std::snprintf(level.data(), level.size(), "%d/%d", skill.cell.learned, skill.cell.max_level);
    } else {
      std::snprintf(level.data(), level.size(), "%d", skill.cell.learned);
    }
    const ImVec2 size = ImGui::CalcTextSize(level.data());
    AddTextCrossHalo(
        into,
        ImVec2(low.x + (cell_w - gap - size.x) * 0.5F, icon_at.y + icon + GuildPx(kLevelGapPixels)),
        skill.cell.learned > 0 ? kLearnedLevel : kUnlearnedLevel, level.data(), kLevelHalo);

    DragSource(skill);
    if (hot && ImGui::GetDragDropPayload() == nullptr) {
      hovered_now = skill.cell.skill_id;
      SkillTooltip(skill, view.skill_points, staff_allowed);
    }
    ClickGestures(state, skill, view.skill_points, action);
    DescribeOnRightClick(hot, skill.cell.skill_id, action);
    ImGui::PopID();
  }
  RestoreSkillIconSampler(into);

  // LES FLÈCHES AU SURVOL, en chaîne dans les deux sens, comme au grimoire.
  if (hovered >= 0) {
    std::vector<SkillCellView> cells;
    cells.reserve(skills.size());
    for (const GuildSkillView& skill : skills) {
      cells.push_back(skill.cell);
    }
    const SkillGroupView group{.label = {}, .cells = cells, .splits = {}};
    const float trim = (cell_w - gap) * kChainTrimShare;
    WalkChain(into, group, centers, shown, static_cast<std::size_t>(hovered), true, trim);
    WalkChain(into, group, centers, shown, static_cast<std::size_t>(hovered), false, trim);
  }

  // L'ÉTENDUE RÉSERVÉE : la grille est peinte en absolu, et sans elle
  // l'ascenseur n'aurait rien à faire défiler.
  ImGui::SetCursorScreenPos(origin);
  const int widest = std::max(1, *std::ranges::max_element(columns));
  ImGui::Dummy(ImVec2(static_cast<float>(widest) * cell_w, static_cast<float>(used_rows) * cell_h));
  ImGui::EndChild();
  state.skill_hover = hovered_now;
}

// Le décompte que porte le bouton « lancer » pendant l'attente.
void CooldownLabel(std::uint32_t cooldown_ms, std::array<char, 16>& label) {
  const std::uint32_t seconds = (cooldown_ms + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond;
  if (seconds >= kSecondsPerMinute) {
    std::snprintf(label.data(), label.size(), "%u:%02u", seconds / kSecondsPerMinute,
                  seconds % kSecondsPerMinute);
  } else {
    std::snprintf(label.data(), label.size(), "%us", seconds);
  }
}

// ── LA LISTE : l'arbre indenté, avec ses coudes de liaison ──────────────────
void DrawList(GuildTabState& state, const GuildView& view, bool staff_allowed, const SkinArt& art,
              GuildAction& action) {
  const auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                     ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY;
  // Pas de colonne « Requiert » : les prérequis sont déjà dans l'infobulle et
  // dessinés en liens, et une troisième redite volerait la largeur au nom.
  if (!ImGui::BeginTable("liste", 5, flags)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Skill"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Level"), ImGuiTableColumnFlags_WidthFixed,
                          GuildPx(kLevelColumnPixels));
  // La nature ou le coût : « Passif » à la place du SP, comme le client.
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, GuildPx(kCostColumnPixels));
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, GuildPx(kUseColumnPixels));
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, GuildPx(kRaiseColumnPixels));
  ImGui::TableHeadersRow();

  const std::span<const GuildSkillView> skills = view.skills;
  const float icon = ImGui::GetTextLineHeight();
  const std::uint16_t focus = state.skill_hover;
  std::uint16_t hovered_now = 0;
  // L'ancre d'une ligne : le bord gauche de son icône, à mi-hauteur. Une
  // abscisse négative veut dire « pas encore dessinée ».
  std::vector<ImVec2> anchors(skills.size(), ImVec2(-1.0F, -1.0F));
  const auto index_of = [&skills](std::uint16_t id) -> int {
    for (std::size_t index = 0; index < skills.size(); ++index) {
      if (skills[index].cell.skill_id == id) {
        return static_cast<int>(index);
      }
    }
    return -1;
  };
  for (std::size_t index = 0; index < skills.size(); ++index) {
    const GuildSkillView& skill = skills[index];
    const std::uint16_t id = skill.cell.skill_id;
    ImGui::PushID(id);
    ImGui::TableNextRow();
    if (id == focus) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, kFocusRow);
    } else if (LinkedToFocus(skills, focus, id)) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, kLinkedRow);
    }

    ImGui::TableNextColumn();
    const float indent =
        GuildPx(kTreeGutterPixels) + static_cast<float>(skill.depth) * GuildPx(kTreeStepPixels);
    ImGui::Indent(indent);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    anchors[index] = ImVec2(at.x, at.y + icon * 0.5F);
    ImDrawList* into = ImGui::GetWindowDrawList();
    for (const SkillNeedView& need : skill.cell.needs) {
      const int source = index_of(need.skill_id);
      if (source < 0 || anchors[static_cast<std::size_t>(source)].x < 0.0F) {
        continue;
      }
      const ImVec2 from = anchors[static_cast<std::size_t>(source)];
      const bool hot = focus != 0 && (focus == id || focus == need.skill_id);
      const ImU32 color = hot ? kHotLink : kColdLink;
      const float thickness = hot ? kHotLinkThickness : kColdLinkThickness;
      // ⚠ LA DESCENTE À GAUCHE DE L'ICÔNE SOURCE, d'un demi-pas : sous son
      // centre, elle traverserait les icônes des voisines de même palier.
      const float x = from.x - GuildPx(kTreeStepPixels) * 0.5F;
      into->AddLine(ImVec2(from.x - GuildPx(kLinkStubPixels), from.y), ImVec2(x, from.y), color,
                    thickness);
      into->AddLine(ImVec2(x, from.y), ImVec2(x, anchors[index].y), color, thickness);
      into->AddLine(ImVec2(x, anchors[index].y),
                    ImVec2(anchors[index].x - GuildPx(kLinkStubPixels), anchors[index].y), color,
                    thickness);
      into->AddCircleFilled(
          ImVec2(anchors[index].x - GuildPx(kLinkDotOffsetPixels), anchors[index].y),
          hot ? kHotDotRadius : kColdDotRadius, color);
    }
    if (skill.cell.icon.usable()) {
      into->AddImage(skill.cell.icon.id, at, ImVec2(at.x + icon, at.y + icon), ImVec2(0.0F, 0.0F),
                     ImVec2(1.0F, 1.0F), skill.locked ? kLockedListTint : IM_COL32_WHITE);
    }
    ImGui::Dummy(ImVec2(icon, icon));
    ImGui::SameLine();
    const std::string label = Label(skill);
    if (skill.locked) {
      ImGui::PushStyleColor(ImGuiCol_Text, kGuildLabelColor);
    }
    ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
    if (skill.locked) {
      ImGui::PopStyleColor();
    }
    const bool hot = ImGui::IsItemHovered();
    if (skill.cell.castable && hot && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      action.verb = GuildVerb::kUseSkill;
      action.skill_id = id;
      action.skill_level = skill.cell.learned;
    }
    DragSource(skill);
    // Vaut aussi pour une verrouillée : savoir ce qu'elle fait aide à décider.
    DescribeOnRightClick(hot, id, action);
    if (hot) {
      hovered_now = id;
      SkillTooltip(skill, view.skill_points, staff_allowed);
    }
    ImGui::Unindent(indent);

    ImGui::TableNextColumn();
    std::array<char, 24> text = {};
    if (skill.cell.max_level > 0) {
      if (skill.cell.learned > 0) {
        std::snprintf(text.data(), text.size(), "%d/%d", skill.cell.learned, skill.cell.max_level);
        ImGui::TextUnformatted(text.data());
      } else {
        std::snprintf(text.data(), text.size(), "-/%d", skill.cell.max_level);
        GuildTextColored(kGuildLabelColor, text.data());
      }
    } else if (skill.cell.learned > 0) {
      ImGui::TextUnformatted(std::to_string(skill.cell.learned).c_str());
    } else {
      GuildTextColored(kGuildLabelColor, "-");
    }

    ImGui::TableNextColumn();
    if (!skill.locked && !skill.active) {
      GuildTextColored(kGuildLabelColor, core::Tr("Passif"));
    } else if (!skill.locked && skill.cell.sp_cost > 0) {
      ImGui::TextUnformatted(std::to_string(skill.cell.sp_cost).c_str());
    } else {
      GuildTextColored(kGuildLabelColor, "-");
    }

    // LANCER — et, sous l'attente, le décompte à la place d'un « > » mort :
    // c'est là que le joueur clique, donc là qu'on lui dit pourquoi ça ne part
    // pas.
    ImGui::TableNextColumn();
    std::array<char, 16> use = {'>'};
    if (skill.cell.cooldown_ms > 0) {
      CooldownLabel(skill.cell.cooldown_ms, use);
    }
    ImGui::BeginDisabled(!skill.cell.castable || skill.cell.cooldown_ms > 0);
    if (SkinSmallButton(use.data(), art, ImVec2(GuildPx(kUseButtonPixels), 0.0F))) {
      action.verb = GuildVerb::kUseSkill;
      action.skill_id = id;
      action.skill_level = skill.cell.learned;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      if (skill.cell.cooldown_ms > 0) {
        std::snprintf(
            text.data(), text.size(), "%u",
            (skill.cell.cooldown_ms + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond);
        std::array<char, 192> tip = {};
        std::snprintf(
            tip.data(), tip.size(),
            core::Tr("Encore %s s.\nLancer un skill de guilde les bloque tous les quatre."),
            text.data());
        SkinTooltipFormat("%s", tip.data());
      } else if (skill.cell.castable) {
        SkinTooltipFormat("%s", core::Tr("Lancer (ou double-clic sur le nom)."));
      } else if (ActiveSkill(skill) && !skill.wieldable) {
        SkinTooltipFormat("%s", core::Tr("Réservée au maître de guilde."));
      } else if (!skill.locked && !skill.active) {
        SkinTooltipFormat("%s", core::Tr("Skill passif : rien à lancer."));
      } else if (skill.locked || skill.cell.learned == 0) {
        SkinTooltipFormat("%s", core::Tr("Non apprise."));
      } else {
        SkinTooltipFormat("%s", core::Tr("Se lance sur une cible : à glisser dans une barre."));
      }
    }

    ImGui::TableNextColumn();
    const bool may_raise = !skill.locked && skill.cell.raisable && view.skill_points > 0;
    ImGui::BeginDisabled(!may_raise);
    if (SkinSmallButton("+", art, ImVec2(GuildPx(kRaiseButtonPixels), 0.0F))) {
      action.verb = GuildVerb::kSkillUp;
      action.skill_id = id;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      if (may_raise) {
        SkinTooltipFormat("%s", core::Tr("Monter d'un niveau."));
      } else if (skill.locked) {
        SkinTooltipFormat("%s", core::Tr("Prérequis non remplis."));
      } else if (view.skill_points <= 0) {
        SkinTooltipFormat("%s", core::Tr("Aucun point de skill disponible."));
      } else {
        SkinTooltipFormat("%s", core::Tr("Niveau maximum, ou réservé au maître de guilde."));
      }
    }
    ImGui::PopID();
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
  state.skill_hover = hovered_now;
}

}  // namespace

void DrawGuildSkills(GuildTabState& state, const GuildView& view, bool& smooth_icons,
                     bool staff_allowed, const SkinArt& art, GuildAction& action) {
  if (!view.skills_known) {
    GuildTextColored(kGuildLabelColor,
                     core::Tr("Skills non encore reçus — clic sur « Actualiser »."));
    return;
  }
  std::array<char, 64> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("Points de skill : %d"), view.skill_points);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(line.data());
  ImGui::SameLine();
  if (SkinButton(state.skill_grid ? core::Tr("Grille") : core::Tr("Liste"), art,
                 ImVec2(GuildPx(kToggleButtonPixels), 0.0F))) {
    state.skill_grid = !state.skill_grid;
  }
  GuildTooltip(
      core::Tr("Bascule entre la grille d'icônes et la liste détaillée\n(niveau, SP, "
               "lancer, monter) — comme le Grimoire."));
  ImGui::SameLine();
  ImGui::TextDisabled("%s", core::Tr("Raccourcis"));
  // ⚠ UN SIMPLE MEMBRE NE FAIT QUE CONSULTER : lui promettre des gestes que le
  // serveur refusera serait l'envoyer chercher une panne.
  GuildTooltip(view.master
                   ? core::Tr("Clic gauche      monter d'un niveau (envoyé aussitôt au serveur)\n"
                              "Double-clic      lancer le skill\n"
                              "Clic droit       description du skill\n"
                              "Glisser          pose le skill sur une barre d'action\n"
                              "Survol           flèches de prérequis (ambre) et de suites (bleu)\n"
                              "\n"
                              "Pas de « Appliquer » ici : contrairement au Grimoire, un skill de\n"
                              "guilde se monte immédiatement.")
                   : core::Tr("Lecture seule : seul le maître de guilde monte, lance et pose\n"
                              "ces skills.\n"
                              "\n"
                              "Clic droit       description du skill\n"
                              "Survol           flèches de prérequis (ambre) et de suites (bleu)"));
  // ⚠ LA MÊME PRÉFÉRENCE QUE LE GRIMOIRE, et en grille seulement : en liste,
  // l'icône fait une hauteur de ligne et le filtre ne se voit pas.
  if (state.skill_grid) {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    static_cast<void>(SkinCheckBox(core::Tr("Lisser les icônes"), &smooth_icons, art));
    GuildTooltip(
        core::Tr("Filtrage bilinéaire des icônes de la grille.\nDécoché (défaut) = pixels "
                 "nets, comme le client natif, qui ne filtre pas."));
  }
  // Sans l'arbre, seulement ce que le serveur envoie ; avec, les verrouillées
  // paraissent aussi, et la liste n'est jamais vide.
  if (!view.skill_tree_known && view.skills.empty()) {
    GuildTextColored(kGuildLabelColor, core::Tr("Aucun skill disponible (prérequis non remplis)."));
    return;
  }
  if (state.skill_grid) {
    DrawGrid(state, view, smooth_icons, staff_allowed, action);
  } else {
    DrawList(state, view, staff_allowed, art, action);
  }
}

}  // namespace ro::ui
