#include "ro/ui/survivors_codex_window.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/survivors_hud.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "survivors_codex_text.h"
#include "survivors_meta_text.h"
#include "survivors_text.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

inline constexpr const char* kCodexTitle = core::TrText("Codex des Survivants");

constexpr float kFirstWidth = 780.0F;
constexpr float kFirstHeight = 560.0F;
constexpr float kListWidth = 230.0F;
constexpr float kListIconSide = 24.0F;
constexpr float kSheetIconSide = 48.0F;
constexpr float kLinkIconSide = 20.0F;
// La boîte où `app` cadre le sprite d'un monstre, d'un boss ou d'une plante.
constexpr float kSpriteBoxWidth = 120.0F;
constexpr float kSpriteBoxHeight = 140.0F;
constexpr ImU32 kShownTint = IM_COL32(255, 255, 255, 255);
// Une silhouette : l'image peinte en noir, sa forme seule.
constexpr ImU32 kSilhouetteTint = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kSpriteBoxBack = IM_COL32(0, 0, 0, 40);
constexpr ImU32 kHeadingColor = IM_COL32(255, 215, 90, 255);
constexpr std::size_t kLineSize = 224;
using Line = std::array<char, kLineSize>;
constexpr ImGuiTableFlags kLevelTableFlags =
    ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH;

constexpr std::array<SurvivorsCodexTab, kSurvivorsCodexTabCount> kTabs = {
    SurvivorsCodexTab::kSkills, SurvivorsCodexTab::kCards, SurvivorsCodexTab::kItems,
    SurvivorsCodexTab::kPlants, SurvivorsCodexTab::kFoes,  SurvivorsCodexTab::kBosses,
};

[[nodiscard]] std::size_t RankOf(SurvivorsCodexTab tab) { return static_cast<std::size_t>(tab); }

[[nodiscard]] SurvivorsCodexTab NextTab(SurvivorsCodexTab tab) {
  return kTabs[(RankOf(tab) + 1) % kTabs.size()];
}

// UNE IMAGE RAMENÉE DANS SA BOÎTE, son rapport gardé, en couleur ou en
// silhouette ; la boîte vide quand l'image manque.
void PaintImage(const SkinImage& image, ImVec2 box, bool seen) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  ImGui::Dummy(box);
  if (!image.usable()) return;
  const float scale = std::min(box.x / image.width, box.y / image.height);
  const ImVec2 size(image.width * scale, image.height * scale);
  const ImVec2 from(corner.x + (box.x - size.x) * 0.5F, corner.y + (box.y - size.y) * 0.5F);
  ImGui::GetWindowDrawList()->AddImage(image.id, from, ImVec2(from.x + size.x, from.y + size.y),
                                       ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                                       seen ? kShownTint : kSilhouetteTint);
}

// Une autre entrée nommée par la fiche : son icône et son nom, ou sa
// silhouette et « ??? » tant qu'elle n'a pas été vue.
void DrawLink(const SurvivorsCodexLink& link) {
  PaintImage(link.icon, ImVec2(kLinkIconSide, kLinkIconSide), link.seen);
  ImGui::SameLine();
  ImGui::TextUnformatted(CodexLinkName(link));
}

[[nodiscard]] bool ShowsSprite(SurvivorsCodexSheet sheet) {
  return sheet == SurvivorsCodexSheet::kFoe || sheet == SurvivorsCodexSheet::kBoss ||
         sheet == SurvivorsCodexSheet::kPlant;
}

// L'image de la fiche : le sprite dans sa boîte relevée, l'icône ailleurs.
void DrawPortrait(SurvivorsCodexState& state, const SurvivorsCodexEntry& entry) {
  if (!ShowsSprite(entry.sheet)) {
    PaintImage(entry.icon, ImVec2(kSheetIconSide, kSheetIconSide), entry.seen);
    return;
  }
  const ImVec2 box(kSpriteBoxWidth, kSpriteBoxHeight);
  state.sprite_box_width = box.x;
  state.sprite_box_height = box.y;
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddRectFilled(corner, ImVec2(corner.x + box.x, corner.y + box.y),
                                            kSpriteBoxBack);
  PaintImage(entry.icon, box, entry.seen);
}

// La phrase : celle du catalogue pour un sort, celle de la clé ailleurs.
[[nodiscard]] const char* PhraseOf(const SurvivorsCodexEntry& entry) {
  if (entry.description != nullptr && entry.description[0] != '\0') {
    return SurvivorsText(entry.description);
  }
  return SurvivorsCodexPhrase(entry.key);
}

// ── LA LISTE ────────────────────────────────────────────────────────────────

void DrawList(SurvivorsCodexState& state, std::span<const SurvivorsCodexEntry> entries,
              bool scroll_to_selected) {
  std::size_t& selected = state.selected[RankOf(state.tab)];
  ImGui::BeginChild("##codex_list", ImVec2(kListWidth, 0.0F), ImGuiChildFlags_Borders);
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const SurvivorsCodexEntry& entry = entries[index];
    ImGui::PushID(static_cast<int>(index));
    PaintImage(entry.icon, ImVec2(kListIconSide, kListIconSide), entry.seen);
    ImGui::SameLine();
    if (ImGui::Selectable(entry.seen ? entry.name : kCodexUnknown, index == selected)) {
      selected = index;
    }
    if (scroll_to_selected && index == selected) ImGui::SetScrollHereY();
    ImGui::PopID();
  }
  ImGui::EndChild();
}

// Flèches haut et bas, fenêtre au premier plan ; vrai si le choix a bougé.
[[nodiscard]] bool ReadListKeys(SurvivorsCodexState& state, std::size_t count) {
  if (count == 0 || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return false;
  std::size_t& selected = state.selected[RankOf(state.tab)];
  const std::size_t before = selected;
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selected > 0) --selected;
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected + 1 < count) ++selected;
  return selected != before;
}

// ── LES CHIFFRES ────────────────────────────────────────────────────────────

// LE TABLEAU D'UN SORT : une ligne par chiffre, une colonne par niveau. Les
// colonnes s'étirent à parts égales : huit niveaux tiennent dans la fiche.
void DrawLevelTable(std::span<const SurvivorsCodexFigure> figures) {
  std::uint8_t levels = 1;
  for (const SurvivorsCodexFigure& figure : figures) levels = std::max(levels, figure.count);
  levels = std::min<std::uint8_t>(levels, kSurvivorsCodexMostLevels);
  if (!ImGui::BeginTable("##codex_levels", levels + 1, kLevelTableFlags)) return;
  ImGui::TableSetupColumn("##label");
  Line line = {};
  for (int level = 1; level <= levels; ++level) {
    std::snprintf(line.data(), line.size(), core::Tr("Lv %d"), level);
    ImGui::TableSetupColumn(line.data());
  }
  ImGui::TableHeadersRow();
  for (const SurvivorsCodexFigure& figure : figures) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(CodexFigureLabelText(figure).data());
    for (std::uint8_t level = 0; level < levels; ++level) {
      ImGui::TableNextColumn();
      if (level < figure.count) {
        ImGui::TextUnformatted(CodexValueText(figure.unit, figure.values[level]).data());
      }
    }
  }
  ImGui::EndTable();
}

// Une ligne par chiffre, « libellé : valeur ».
void DrawFigureLines(std::span<const SurvivorsCodexFigure> figures) {
  Line line = {};
  for (const SurvivorsCodexFigure& figure : figures) {
    std::snprintf(line.data(), line.size(), core::Tr("%s : %s"),
                  CodexFigureLabelText(figure).data(),
                  CodexValueText(figure.unit, figure.values[0]).data());
    ImGui::TextUnformatted(line.data());
  }
}

// ── LES FICHES ──────────────────────────────────────────────────────────────

void DrawEvolution(const SurvivorsCodexEntry& entry) {
  Line line = {};
  if (entry.has_evolution) {
    const SurvivorsCodexEvolution& evolution = entry.evolution;
    ImGui::TextUnformatted(core::Tr("Évolution :"));
    ImGui::SameLine();
    DrawLink(evolution.card);
    ImGui::SameLine();
    std::snprintf(line.data(), line.size(), core::Tr("+ niveau %d donne"),
                  static_cast<int>(evolution.max_level));
    ImGui::TextUnformatted(line.data());
    ImGui::SameLine();
    DrawLink(evolution.result);
  }
  if (entry.evolved) {
    const SurvivorsCodexEvolution& origin = entry.evolved_from;
    ImGui::TextUnformatted(core::Tr("Vient de :"));
    ImGui::SameLine();
    DrawLink(origin.result);
    ImGui::SameLine();
    std::snprintf(line.data(), line.size(), core::Tr("niveau %d +"),
                  static_cast<int>(origin.max_level));
    ImGui::TextUnformatted(line.data());
    ImGui::SameLine();
    DrawLink(origin.card);
  }
}

void DrawSkillSheet(const SurvivorsCodexEntry& entry) {
  DrawLevelTable(entry.figures);
  const bool hurts = std::ranges::any_of(entry.figures, [](const SurvivorsCodexFigure& figure) {
    return figure.label == SurvivorsCodexFigureLabel::kDamage;
  });
  if (hurts) FaintText(core::Tr("Dégâts de base, avant cartes et niveau du héros."));
  DrawEvolution(entry);
}

// D'où vient un monstre, et quand au plus tôt.
void DrawArrival(const SurvivorsCodexEntry& entry) {
  Line line = {};
  if (!entry.arrives) {
    FaintText(core::Tr("Aucune source ne l'amène."));
    return;
  }
  std::snprintf(line.data(), line.size(), core::Tr("Arrive dès %s"),
                SurvivorsClockText(entry.arrives_ms).data());
  ImGui::TextUnformatted(line.data());
  for (const SurvivorsCodexSource& source : entry.sources) {
    ImGui::BulletText("%s", CodexSourceText(source).data());
  }
}

// Faiblesses, résistances, comportement, chiffres et carte d'un monstre ou
// d'un boss.
void DrawFoeBody(const SurvivorsCodexEntry& entry) {
  Line line = {};
  FaintText(CodexBehaviourText(entry.behaviour).data());
  if (!entry.weaknesses.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("Faible à : %s"),
                  CodexAffinitiesText(entry.weaknesses).data());
    ImGui::TextWrapped("%s", line.data());
  }
  if (!entry.resistances.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("Résiste à : %s"),
                  CodexAffinitiesText(entry.resistances).data());
    ImGui::TextWrapped("%s", line.data());
  }
  DrawFigureLines(entry.figures);
  if (!entry.drops_card) return;
  ImGui::TextUnformatted(core::Tr("Carte :"));
  ImGui::SameLine();
  DrawLink(entry.card.card);
  ImGui::SameLine();
  if (entry.card.guaranteed) {
    ImGui::TextUnformatted(core::Tr("garantie au premier tué"));
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("1 chance sur %u"),
                  static_cast<unsigned>(entry.card.odds));
    ImGui::TextUnformatted(line.data());
  }
}

void DrawBossSheet(const SurvivorsCodexEntry& entry) {
  Line line = {};
  std::snprintf(line.data(), line.size(), core::Tr("Arrive à %s"),
                SurvivorsClockText(entry.at_ms).data());
  ImGui::TextUnformatted(line.data());
  if (entry.final) ColoredText(kHeadingColor, core::Tr("Sa chute est la victoire."));
  if (entry.escort_count > 0) {
    std::snprintf(line.data(), line.size(), core::Tr("Escorte : %d %s"),
                  static_cast<int>(entry.escort_count), CodexLinkName(entry.escort));
    ImGui::TextUnformatted(line.data());
  }
  DrawFoeBody(entry);
  if (entry.moves.empty()) return;
  ImGui::SeparatorText(core::Tr("Attaques"));
  for (const SurvivorsCodexMove& move : entry.moves) {
    ImGui::Bullet();
    ImGui::TextWrapped("%s", CodexMoveText(move).data());
  }
}

void DrawEventSheet(const SurvivorsCodexEntry& entry) {
  ColoredText(kHeadingColor, CodexEventKindName(entry.event_kind));
  ImGui::TextWrapped("%s", CodexEventText(entry).data());
  DrawFigureLines(entry.figures);
}

void DrawPlantSheet(const SurvivorsCodexEntry& entry) {
  ImGui::TextUnformatted(core::Tr("Lâche :"));
  ImGui::SameLine();
  DrawLink(entry.drop);
  DrawFigureLines(entry.figures);
}

// Une entrée non vue : sa silhouette, « ??? », et pour un sort verrouillé le
// but de l'exploit qui l'ouvre. Rien d'autre.
void DrawUnseenSheet(SurvivorsCodexState& state, const SurvivorsCodexEntry& entry) {
  DrawPortrait(state, entry);
  ColoredText(kHeadingColor, kCodexUnknown);
  FaintText(kCodexUnknown);
  const char* goal = SurvivorsFeatGoal(entry.unlock_feat);
  if (goal[0] == '\0') return;
  Line line = {};
  std::snprintf(line.data(), line.size(), core::Tr("Se débloque : %s"), goal);
  ImGui::TextWrapped("%s", line.data());
}

void DrawSheet(SurvivorsCodexState& state, const SurvivorsCodexEntry& entry) {
  ImGui::BeginChild("##codex_sheet", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders);
  ImGui::PushTextWrapPos(0.0F);
  if (!entry.seen) {
    DrawUnseenSheet(state, entry);
  } else {
    DrawPortrait(state, entry);
    ColoredText(kHeadingColor, entry.name);
    Line line = {};
    if (const char* element = CodexElementName(entry.element); element[0] != '\0') {
      std::snprintf(line.data(), line.size(), core::Tr("Élément : %s"), element);
      ImGui::TextUnformatted(line.data());
    }
    if (const char* size = CodexSizeName(entry.size); size[0] != '\0') {
      std::snprintf(line.data(), line.size(), core::Tr("Taille : %s"), size);
      ImGui::TextUnformatted(line.data());
    }
    ImGui::TextWrapped("%s", PhraseOf(entry));
    ImGui::Separator();
    switch (entry.sheet) {
      case SurvivorsCodexSheet::kSkill:
        DrawSkillSheet(entry);
        break;
      case SurvivorsCodexSheet::kChest:
        DrawFigureLines(entry.figures);
        FaintText(core::Tr("La chance du héros majore ces chances."));
        break;
      case SurvivorsCodexSheet::kItem:
      case SurvivorsCodexSheet::kGem:
      case SurvivorsCodexSheet::kGroundCard:
        DrawFigureLines(entry.figures);
        break;
      case SurvivorsCodexSheet::kPlant:
        DrawPlantSheet(entry);
        break;
      case SurvivorsCodexSheet::kFoe:
        DrawArrival(entry);
        DrawFoeBody(entry);
        break;
      case SurvivorsCodexSheet::kBoss:
        DrawBossSheet(entry);
        break;
      case SurvivorsCodexSheet::kEvent:
        DrawEventSheet(entry);
        break;
    }
  }
  ImGui::PopTextWrapPos();
  ImGui::EndChild();
}

// ── LES ONGLETS ─────────────────────────────────────────────────────────────

// La barre : l'onglet voulu s'impose jusqu'à ce qu'elle le montre ; sinon
// l'onglet ouvert est celui qu'ImGui tient, relevé.
void DrawTabs(SurvivorsCodexState& state, const SkinArt& art) {
  if (ImGui::IsWindowAppearing()) state.tab_wanted = state.tab;
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    state.tab = NextTab(state.tab);
    state.tab_wanted = state.tab;
  }
  if (!BeginSkinTabBar("##codex_tabs", art)) return;
  Line label = {};
  for (const SurvivorsCodexTab tab : kTabs) {
    // Une identité fixe : le nom traduit change avec la langue.
    std::snprintf(label.data(), label.size(), "%s###codex_tab_%zu", CodexTabLabel(tab),
                  RankOf(tab));
    const ImGuiTabItemFlags flags =
        state.tab_wanted == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem(label.data(), nullptr, flags)) {
      if (!state.tab_wanted.has_value()) {
        state.tab = tab;
      } else if (*state.tab_wanted == tab) {
        state.tab_wanted.reset();
      }
      ImGui::EndTabItem();
    }
  }
  EndSkinTabBar();
}

}  // namespace

SurvivorsCodexAsk DrawSurvivorsCodexWindow(SurvivorsCodexState& state,
                                           const SurvivorsCodexView& view,
                                           SurvivorsAlbumState& album_state,
                                           const SurvivorsAlbumView& album_view,
                                           const SkinArt& art) {
  SurvivorsCodexAsk ask;
  if (!state.open) return ask;
  static WindowName name(kCodexTitle, kSurvivorsCodexId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  const bool opened = BeginSkinnedWindow(name.Get(), &state.open, art);
  ask.closed = !state.open;
  if (!opened) {
    EndSkinnedWindow();
    return ask;
  }
  DrawTabs(state, art);
  if (state.tab == SurvivorsCodexTab::kCards) {
    DrawSurvivorsAlbumBody(album_state, album_view);
    EndSkinnedWindow();
    return ask;
  }
  const std::span<const SurvivorsCodexEntry> entries = view.entries[RankOf(state.tab)];
  std::size_t& selected = state.selected[RankOf(state.tab)];
  if (!entries.empty()) selected = std::min(selected, entries.size() - 1);
  const bool moved = ReadListKeys(state, entries.size());
  DrawList(state, entries, moved);
  if (!entries.empty()) {
    ImGui::SameLine();
    DrawSheet(state, entries[selected]);
  }
  EndSkinnedWindow();
  return ask;
}

}  // namespace ro::ui
