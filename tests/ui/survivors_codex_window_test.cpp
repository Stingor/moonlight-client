// LA FENÊTRE DU CODEX DES SURVIVANTS : six onglets ; une entrée non vue ne
// montre que sa silhouette et « ??? », ni son nom, ni sa phrase, ni un de ses
// chiffres, ni ses sources, ni sa carte ; une entrée vue montre tout, sauf le
// nom et la couleur de ses liens vers des entrées non vues ; flèches, clic et Tab changent la
// sélection, gardée par onglet ; l'onglet Cartes dessine l'Album ; une liste vide tient ; huit
// niveaux tiennent dans la fiche ; la lecture seule ne rend aucune intention.
// Chaque ligne de la liste a un texte et une hauteur d'icône, et la fiche
// commence par le nom de l'entrée.

#include "ro/ui/survivors_codex_window.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/survivors_album_window.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::SkinArt;
using ro::ui::SurvivorsCodexEntry;
using ro::ui::SurvivorsCodexFigure;
using ro::ui::SurvivorsCodexFigureLabel;
using ro::ui::SurvivorsCodexSheet;
using ro::ui::SurvivorsCodexState;
using ro::ui::SurvivorsCodexTab;
using ro::ui::SurvivorsCodexUnit;
using ro::ui::SurvivorsCodexView;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kBetweenClicksSeconds = 0.5F;
constexpr int kSettleFrames = 3;
// Un clic toutes les quelques lignes : la liste, la fiche et les onglets.
constexpr float kSweepStep = 48.0F;
constexpr ImTextureID kCardBack = 12;
constexpr float kListIconSide = 24.0F;
constexpr float kIntoRowPixels = 60.0F;
const SkinArt kNoArt;

constexpr std::size_t kMostLevels = ro::ui::kSurvivorsCodexMostLevels;

[[nodiscard]] std::size_t RankOf(SurvivorsCodexTab tab) { return static_cast<std::size_t>(tab); }

// Une ligne de chiffres d'un sort, de `first` en `first + step` sur `count`
// niveaux.
[[nodiscard]] SurvivorsCodexFigure Levels(SurvivorsCodexFigureLabel label, SurvivorsCodexUnit unit,
                                          std::int64_t first, std::int64_t step,
                                          std::uint8_t count) {
  SurvivorsCodexFigure figure{.label = label, .unit = unit, .count = count};
  for (std::uint8_t level = 0; level < count; ++level) figure.values[level] = first + step * level;
  return figure;
}

const std::array<SurvivorsCodexFigure, 2> kFireBoltFigures = {
    Levels(SurvivorsCodexFigureLabel::kDamage, SurvivorsCodexUnit::kPlain, 4321, 1, kMostLevels),
    Levels(SurvivorsCodexFigureLabel::kCooldown, SurvivorsCodexUnit::kMs, 1500, 0, kMostLevels),
};

[[nodiscard]] SurvivorsCodexEntry FireBolt(bool seen) {
  return SurvivorsCodexEntry{.sheet = SurvivorsCodexSheet::kSkill,
                             .seen = seen,
                             .key = "fire_bolt",
                             .ro_id = 19,
                             .name = "Fire Bolt",
                             .description = "Un trait de feu vers le plus proche.",
                             .element = 3,
                             .figures = kFireBoltFigures};
}

// Une vue où seul l'onglet des sorts a des entrées.
struct SkillsView {
  std::array<SurvivorsCodexEntry, 2> skills;
  SurvivorsCodexView view;

  explicit SkillsView(bool seen) : skills{FireBolt(seen), FireBolt(seen)} {
    skills[1].name = "Cold Bolt";
    view.entries[RankOf(SurvivorsCodexTab::kSkills)] = skills;
  }
};

struct Drawn {
  SurvivorsCodexState state{.open = true};
  ro::ui::SurvivorsAlbumState album;
  ro::ui::SurvivorsAlbumView album_view;
  SurvivorsCodexView view;

  bool operator()() {
    return DrawSurvivorsCodexWindow(state, view, album, album_view, kNoArt).closed;
  }
};

[[nodiscard]] bool Frame(ro::ui::UiContext& ui, Drawn& drawn, float seconds = kFrameSeconds) {
  ui.BeginFrame(kViewport, seconds);
  const bool asked = drawn();
  ui.EndFrame();
  return asked;
}

void Settle(ro::ui::UiContext& ui, Drawn& drawn) {
  for (int frame = 0; frame < kSettleFrames; ++frame) (void)Frame(ui, drawn);
}

// Le texte que la fenêtre pose à une image, relevé par le journal d'ImGui.
[[nodiscard]] std::string TextOf(ro::ui::UiContext& ui, Drawn& drawn) {
  Settle(ui, drawn);
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::LogToBuffer();
  (void)drawn();
  std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
  ImGui::LogFinish();
  ui.EndFrame();
  return text;
}

void PressKey(ro::ui::UiContext& ui, Drawn& drawn, ImGuiKey key) {
  ImGui::GetIO().AddKeyEvent(key, true);
  (void)Frame(ui, drawn);
  ImGui::GetIO().AddKeyEvent(key, false);
  (void)Frame(ui, drawn);
  Settle(ui, drawn);
}

[[nodiscard]] bool ClickAt(ro::ui::UiContext& ui, Drawn& drawn, ImVec2 at) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  bool asked = Frame(ui, drawn);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  asked = Frame(ui, drawn) || asked;
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  asked = Frame(ui, drawn) || asked;
  // Une image longue : deux clics proches feraient un double-clic.
  asked = Frame(ui, drawn, kBetweenClicksSeconds) || asked;
  return asked;
}

// La fenêtre enfant dont le nom contient `part`.
[[nodiscard]] const ImGuiWindow* ChildNamed(std::string_view part) {
  for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (std::string_view(window->Name).find(part) != std::string_view::npos) return window;
  }
  return nullptr;
}

[[nodiscard]] bool PaintsTexture(ImTextureID texture) {
  const ImDrawData* data = ImGui::GetDrawData();
  if (data == nullptr) return false;
  for (const ImDrawList* list : data->CmdLists) {
    for (const ImDrawCmd& command : list->CmdBuffer) {
      if (command.TexRef._TexData == nullptr && command.TexRef._TexID == texture) return true;
    }
  }
  return false;
}

// LA COULEUR DES SOMMETS de la première image peinte avec `texture` : la teinte
// que la fenêtre lui a donnée. Rien si elle n'est pas peinte.
[[nodiscard]] std::optional<ImU32> TintOf(ImTextureID texture) {
  const ImDrawData* data = ImGui::GetDrawData();
  if (data == nullptr) return std::nullopt;
  for (const ImDrawList* list : data->CmdLists) {
    for (const ImDrawCmd& command : list->CmdBuffer) {
      if (command.TexRef._TexData != nullptr || command.TexRef._TexID != texture) continue;
      if (command.ElemCount == 0) continue;
      const ImDrawIdx index = list->IdxBuffer[static_cast<int>(command.IdxOffset)];
      return list->VtxBuffer[static_cast<int>(command.VtxOffset + index)].col;
    }
  }
  return std::nullopt;
}

constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kOpaqueBlack = IM_COL32(0, 0, 0, 255);
constexpr ImTextureID kEntryIcon = 21;
constexpr ImTextureID kLinkIcon = 22;
constexpr float kIconSide = 24.0F;

[[nodiscard]] ro::ui::SkinImage IconOf(ImTextureID texture) {
  return ro::ui::SkinImage{.id = texture, .width = kIconSide, .height = kIconSide};
}

// Un monstre avec ses chiffres, sa source d'arrivée et sa carte : de quoi
// vérifier qu'une fiche n'en montre rien quand elle ne le doit pas.
const std::array<SurvivorsCodexFigure, 1> kPoringFigures = {
    Levels(SurvivorsCodexFigureLabel::kHp, SurvivorsCodexUnit::kPlain, 4321, 0, 1)};

struct PoringView {
  std::array<ro::ui::SurvivorsCodexSource, 1> sources;
  std::array<SurvivorsCodexEntry, 1> foes;

  PoringView(bool seen, bool links_seen) {
    sources[0] =
        ro::ui::SurvivorsCodexSource{.kind = ro::ui::SurvivorsCodexSourceKind::kEscort,
                                     .at_ms = 300000,
                                     .bringer = ro::ui::SurvivorsCodexLink{
                                         .ro_id = 1093, .seen = links_seen, .name = "Eclipse"}};
    foes[0] = SurvivorsCodexEntry{.sheet = SurvivorsCodexSheet::kFoe,
                                  .seen = seen,
                                  .key = "1002",
                                  .ro_id = 1002,
                                  .name = "Poring",
                                  .figures = kPoringFigures,
                                  .arrives = true,
                                  .sources = sources,
                                  .drops_card = true,
                                  .card = ro::ui::SurvivorsCodexCardDrop{
                                      .card = ro::ui::SurvivorsCodexLink{.ro_id = 4001,
                                                                         .seen = links_seen,
                                                                         .icon = IconOf(kLinkIcon),
                                                                         .name = "Poring Card"},
                                      .odds = 300}};
  }
};

// LA CHRONOLOGIE : les élites, une nuée, un boss dont le sprite est cadré
// (l'entrée choisie de `app` en porte un, haut), la nuit. Les événements
// n'ont ni nom ni image : `ui` les nomme.
constexpr ImTextureID kBossSprite = 23;
constexpr float kSpriteWidth = 120.0F;
constexpr float kSpriteHeight = 140.0F;
// ImGui tronque la taille du contenu au pixel.
constexpr float kRowSlack = 1.0F;

struct TimelineView {
  std::array<SurvivorsCodexEntry, 4> rows;

  TimelineView() {
    rows[0] = SurvivorsCodexEntry{.sheet = SurvivorsCodexSheet::kEvent,
                                  .seen = true,
                                  .key = "elites",
                                  .at_ms = 120000,
                                  .event_kind = ro::ui::SurvivorsCodexEventKind::kElites};
    rows[1] = SurvivorsCodexEntry{
        .sheet = SurvivorsCodexSheet::kEvent,
        .seen = true,
        .key = "swarm_1011",
        .at_ms = 150000,
        .event_kind = ro::ui::SurvivorsCodexEventKind::kSwarm,
        .event_foe = ro::ui::SurvivorsCodexLink{.ro_id = 1011, .seen = true, .name = "Chonchon"},
        .event_count = 24};
    rows[2] = SurvivorsCodexEntry{
        .sheet = SurvivorsCodexSheet::kBoss,
        .seen = true,
        .key = "1090",
        .ro_id = 1090,
        .name = "Mastering",
        .icon =
            ro::ui::SkinImage{.id = kBossSprite, .width = kSpriteWidth, .height = kSpriteHeight},
        .at_ms = 180000};
    rows[3] = SurvivorsCodexEntry{.sheet = SurvivorsCodexSheet::kEvent,
                                  .seen = true,
                                  .key = "night",
                                  .at_ms = 420000,
                                  .event_kind = ro::ui::SurvivorsCodexEventKind::kNight};
  }
};

// La hauteur que prennent `count` lignes d'une hauteur d'icône.
[[nodiscard]] float RowsHeight(std::size_t count) {
  const float rows = static_cast<float>(count);
  return rows * kListIconSide + (rows - 1.0F) * ImGui::GetStyle().ItemSpacing.y;
}

}  // namespace

TEST_CASE("Codex : chaque ligne de la chronologie porte son heure et son nom",
          "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  TimelineView timeline;
  Drawn drawn;
  drawn.view.entries[RankOf(SurvivorsCodexTab::kBosses)] = timeline.rows;
  drawn.state.tab = SurvivorsCodexTab::kBosses;
  const std::string text = TextOf(ui, drawn);
  CHECK(text.find("02:00  Élites") != std::string::npos);
  CHECK(text.find("02:30  Nuée : Chonchon") != std::string::npos);
  CHECK(text.find("03:00  Mastering") != std::string::npos);
  CHECK(text.find("07:00  Nuit") != std::string::npos);

  // Non vue, une ligne ne dit que « ??? », sans son heure.
  timeline.rows[1].seen = false;
  const std::string hidden = TextOf(ui, drawn);
  CHECK(hidden.find("Chonchon") == std::string::npos);
  CHECK(hidden.find("02:30") == std::string::npos);
  CHECK(hidden.find("???") != std::string::npos);
}

TEST_CASE("Codex : les lignes de la liste ont toutes une hauteur d'icône",
          "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  TimelineView timeline;
  Drawn drawn;
  drawn.view.entries[RankOf(SurvivorsCodexTab::kBosses)] = timeline.rows;
  drawn.state.tab = SurvivorsCodexTab::kBosses;
  // Le boss choisi porte son sprite : la liste ne le peint pas.
  drawn.state.selected[RankOf(SurvivorsCodexTab::kBosses)] = 2;
  Settle(ui, drawn);
  const ImGuiWindow* list = ChildNamed("##codex_list");
  REQUIRE(list != nullptr);
  CHECK(std::abs(list->ContentSize.y - RowsHeight(timeline.rows.size())) <= kRowSlack);

  // Des lignes à icône, à la même hauteur.
  SkillsView skills(true);
  skills.skills[0].icon = IconOf(kEntryIcon);
  skills.skills[1].icon = IconOf(kEntryIcon);
  drawn.view = skills.view;
  drawn.state.tab = SurvivorsCodexTab::kSkills;
  drawn.state.tab_wanted = drawn.state.tab;
  Settle(ui, drawn);
  list = ChildNamed("##codex_list");
  REQUIRE(list != nullptr);
  CHECK(std::abs(list->ContentSize.y - RowsHeight(skills.skills.size())) <= kRowSlack);
}

TEST_CASE("Codex : le nom d'une classe annonce ses skills, une seule fois",
          "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView skills(true);
  skills.skills[0].class_name = "Magicien";
  skills.skills[1].class_name = "Magicien";
  Drawn drawn;
  drawn.view = skills.view;
  const std::string one_class = TextOf(ui, drawn);
  const std::size_t first = one_class.find("Magicien");
  REQUIRE(first != std::string::npos);
  CHECK(one_class.find("Magicien", first + 1) == std::string::npos);

  skills.skills[1].class_name = "Chevalier";
  const std::string two_classes = TextOf(ui, drawn);
  CHECK(two_classes.find("Magicien") < two_classes.find("Cold Bolt"));
  CHECK(two_classes.find("Chevalier") < two_classes.find("Cold Bolt"));
  CHECK(two_classes.find("Magicien") < two_classes.find("Chevalier"));
}

TEST_CASE("Codex : sous plusieurs classes, un onglet par classe ne montre que les siens",
          "[ui][survivors][codex]") {
  constexpr std::uint16_t kMageClassId = 1;
  constexpr std::uint16_t kKnightClassId = 2;
  ro::ui::UiContext ui;
  SkillsView skills(true);
  skills.skills[0].class_id = kMageClassId;
  skills.skills[0].class_name = "Magicien";
  skills.skills[1].class_id = kKnightClassId;
  skills.skills[1].class_name = "Chevalier";
  CHECK(ro::ui::SurvivorsCodexClassTabs(skills.skills) ==
        std::vector<std::uint16_t>{kMageClassId, kKnightClassId});
  CHECK(ro::ui::SurvivorsCodexClassRows(skills.skills, kKnightClassId) ==
        std::vector<std::size_t>{1});
  CHECK(ro::ui::SurvivorsCodexClassRows(skills.skills, 0).size() == 2U);

  // L'onglet de la classe de l'accueil s'ouvre d'abord : le Chevalier.
  Drawn drawn;
  drawn.view = skills.view;
  drawn.view.home_class_id = kKnightClassId;
  std::string text = TextOf(ui, drawn);
  CHECK(drawn.state.skill_class_id == kKnightClassId);
  CHECK(text.find("Magicien") != std::string::npos);
  CHECK(text.find("Cold Bolt") != std::string::npos);
  CHECK(text.find("Fire Bolt") == std::string::npos);
  CHECK(drawn.state.selected[RankOf(SurvivorsCodexTab::kSkills)] == 1);

  // Gauche passe au Magicien, et la sélection suit dans sa liste.
  PressKey(ui, drawn, ImGuiKey_LeftArrow);
  CHECK(drawn.state.skill_class_id == kMageClassId);
  CHECK(drawn.state.selected[RankOf(SurvivorsCodexTab::kSkills)] == 0);
  text = TextOf(ui, drawn);
  CHECK(text.find("Fire Bolt") != std::string::npos);
  CHECK(text.find("Cold Bolt") == std::string::npos);
  PressKey(ui, drawn, ImGuiKey_RightArrow);
  CHECK(drawn.state.skill_class_id == kKnightClassId);
}

TEST_CASE("Codex : la fiche commence par le nom de l'entrée", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  TimelineView timeline;
  Drawn drawn;
  drawn.view.entries[RankOf(SurvivorsCodexTab::kBosses)] = timeline.rows;
  drawn.state.tab = SurvivorsCodexTab::kBosses;
  const std::string text = TextOf(ui, drawn);
  // La liste nomme les élites une fois ; la fiche ensuite, avant sa phrase.
  const std::size_t in_list = text.find("Élites");
  REQUIRE(in_list != std::string::npos);
  const std::size_t in_sheet = text.find("Élites", in_list + 1);
  const std::size_t phrase = text.find(ro::ui::SurvivorsCodexPhrase("elites"));
  REQUIRE(in_sheet != std::string::npos);
  REQUIRE(phrase != std::string::npos);
  CHECK(in_sheet < phrase);
  // Le sprite du boss appartient à sa fiche : la liste ne le peint pas.
  CHECK_FALSE(PaintsTexture(kBossSprite));
}

TEST_CASE("Codex : une entrée non vue peint son icône en silhouette", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView hidden(false);
  hidden.skills[0].icon = IconOf(kEntryIcon);
  Drawn drawn;
  drawn.view = hidden.view;
  Settle(ui, drawn);
  CHECK(TintOf(kEntryIcon) == std::optional<ImU32>(kOpaqueBlack));

  SkillsView shown(true);
  shown.skills[0].icon = IconOf(kEntryIcon);
  drawn.view = shown.view;
  Settle(ui, drawn);
  CHECK(TintOf(kEntryIcon) == std::optional<ImU32>(kWhite));
}

TEST_CASE("Codex : un monstre non vu ne montre ni chiffre, ni source, ni carte",
          "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  PoringView poring(/*seen=*/false, /*links_seen=*/true);
  Drawn drawn;
  drawn.view.entries[RankOf(SurvivorsCodexTab::kFoes)] = poring.foes;
  drawn.state.tab = SurvivorsCodexTab::kFoes;
  const std::string text = TextOf(ui, drawn);
  CHECK(text.find("???") != std::string::npos);
  CHECK(text.find("Poring") == std::string::npos);
  CHECK(text.find("4321") == std::string::npos);
  CHECK(text.find("Eclipse") == std::string::npos);
  CHECK(text.find("300") == std::string::npos);
  CHECK_FALSE(TintOf(kLinkIcon).has_value());

  // Même chose dans l'onglet des boss.
  poring.foes[0].sheet = SurvivorsCodexSheet::kBoss;
  drawn.view.entries[RankOf(SurvivorsCodexTab::kFoes)] = {};
  drawn.view.entries[RankOf(SurvivorsCodexTab::kBosses)] = poring.foes;
  drawn.state.tab = SurvivorsCodexTab::kBosses;
  drawn.state.tab_wanted = drawn.state.tab;
  const std::string boss_text = TextOf(ui, drawn);
  CHECK(boss_text.find("Poring") == std::string::npos);
  CHECK(boss_text.find("4321") == std::string::npos);
}

TEST_CASE("Codex : la fiche d'une entrée vue tait ses liens non vus", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  PoringView poring(/*seen=*/true, /*links_seen=*/false);
  Drawn drawn;
  drawn.view.entries[RankOf(SurvivorsCodexTab::kFoes)] = poring.foes;
  drawn.state.tab = SurvivorsCodexTab::kFoes;
  const std::string text = TextOf(ui, drawn);
  CHECK(text.find("Poring") != std::string::npos);
  CHECK(text.find("4321") != std::string::npos);
  CHECK(text.find("Eclipse") == std::string::npos);
  CHECK(text.find("Poring Card") == std::string::npos);
  CHECK(text.find("???") != std::string::npos);
  CHECK(TintOf(kLinkIcon) == std::optional<ImU32>(kOpaqueBlack));

  // Vus, les liens se nomment, en couleur.
  PoringView known(/*seen=*/true, /*links_seen=*/true);
  drawn.view.entries[RankOf(SurvivorsCodexTab::kFoes)] = known.foes;
  const std::string known_text = TextOf(ui, drawn);
  CHECK(known_text.find("Eclipse") != std::string::npos);
  CHECK(known_text.find("Poring Card") != std::string::npos);
  CHECK(TintOf(kLinkIcon) == std::optional<ImU32>(kWhite));
}

TEST_CASE("Codex : six onglets, et Tab les parcourt tous", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  Drawn drawn;
  Settle(ui, drawn);
  CHECK(drawn.state.tab == SurvivorsCodexTab::kSkills);
  std::array<bool, ro::ui::kSurvivorsCodexTabCount> visited = {};
  for (std::size_t press = 0; press < ro::ui::kSurvivorsCodexTabCount; ++press) {
    PressKey(ui, drawn, ImGuiKey_Tab);
    visited[RankOf(drawn.state.tab)] = true;
  }
  for (const bool tab_visited : visited) CHECK(tab_visited);
  CHECK(drawn.state.tab == SurvivorsCodexTab::kSkills);
  CHECK_FALSE(drawn.state.tab_wanted.has_value());
}

TEST_CASE("Codex : une entrée non vue ne montre que « ??? »", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView skills(false);
  skills.skills[0].unlock_feat = "sight";
  Drawn drawn;
  drawn.view = skills.view;
  const std::string text = TextOf(ui, drawn);
  CHECK(text.find("???") != std::string::npos);
  CHECK(text.find("Fire Bolt") == std::string::npos);
  CHECK(text.find("Cold Bolt") == std::string::npos);
  CHECK(text.find("trait de feu") == std::string::npos);
  CHECK(text.find("4321") == std::string::npos);
  CHECK(text.find("1,5") == std::string::npos);
  // Un sort verrouillé dit le but de son exploit, et rien d'autre.
  CHECK(text.find("Survivre en une seule partie") != std::string::npos);
}

TEST_CASE("Codex : un sort verrouillé dit où en est son exploit, sur quel seuil",
          "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView skills(false);
  skills.skills[0].unlock_feat = "earth_spike";
  const std::array<ro::ui::SurvivorsFeatEntry, 1> feats{
      {{.key = "earth_spike", .progress = 124, .threshold = 300}}};
  Drawn drawn;
  drawn.view = skills.view;
  drawn.album_view.feats = feats;
  const std::string text = TextOf(ui, drawn);
  CHECK(text.find("Monstres Wind vaincus, au cumul") != std::string::npos);
  CHECK(text.find("124 / 300") != std::string::npos);
}

TEST_CASE("Codex : une entrée vue montre nom, phrase et chiffres", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView skills(true);
  Drawn drawn;
  drawn.view = skills.view;
  const std::string text = TextOf(ui, drawn);
  CHECK(text.find("Fire Bolt") != std::string::npos);
  CHECK(text.find("Un trait de feu vers le plus proche.") != std::string::npos);
  CHECK(text.find("4321") != std::string::npos);
  CHECK(text.find("1,5 s") != std::string::npos);
  CHECK(text.find("Feu") != std::string::npos);
}

TEST_CASE("Codex : une entrée hors des sorts lit sa phrase par sa clé", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  const std::array<SurvivorsCodexEntry, 1> foes = {SurvivorsCodexEntry{
      .sheet = SurvivorsCodexSheet::kFoe, .seen = true, .key = "1002", .name = "Poring"}};
  Drawn drawn;
  drawn.view.entries[RankOf(SurvivorsCodexTab::kFoes)] = foes;
  drawn.state.tab = SurvivorsCodexTab::kFoes;
  const std::string text = TextOf(ui, drawn);
  CHECK(text.find(ro::ui::SurvivorsCodexPhrase("1002")) != std::string::npos);
  CHECK(std::string_view(ro::ui::SurvivorsCodexPhrase("no_such_key")).empty());
}

TEST_CASE("Codex : flèches et clic changent la sélection, gardée par onglet",
          "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView skills(true);
  Drawn drawn;
  drawn.view = skills.view;
  Settle(ui, drawn);
  PressKey(ui, drawn, ImGuiKey_DownArrow);
  CHECK(drawn.state.selected[RankOf(SurvivorsCodexTab::kSkills)] == 1);
  PressKey(ui, drawn, ImGuiKey_UpArrow);
  CHECK(drawn.state.selected[RankOf(SurvivorsCodexTab::kSkills)] == 0);

  const ImGuiWindow* list = ChildNamed("##codex_list");
  REQUIRE(list != nullptr);
  const float row = kListIconSide + ImGui::GetStyle().ItemSpacing.y;
  const ImVec2 second_row(list->DC.CursorStartPos.x + kIntoRowPixels,
                          list->DC.CursorStartPos.y + row + kListIconSide * 0.5F);
  (void)ClickAt(ui, drawn, second_row);
  CHECK(drawn.state.selected[RankOf(SurvivorsCodexTab::kSkills)] == 1);

  // Un autre onglet, puis le retour : la sélection des sorts est restée.
  PressKey(ui, drawn, ImGuiKey_Tab);
  CHECK(drawn.state.tab == SurvivorsCodexTab::kCards);
  for (std::size_t press = 1; press < ro::ui::kSurvivorsCodexTabCount; ++press) {
    PressKey(ui, drawn, ImGuiKey_Tab);
  }
  CHECK(drawn.state.tab == SurvivorsCodexTab::kSkills);
  CHECK(drawn.state.selected[RankOf(SurvivorsCodexTab::kSkills)] == 1);
}

TEST_CASE("Codex : l'onglet Cartes dessine l'Album", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  const std::array<ro::ui::SurvivorsAlbumEntry, 1> cards = {
      ro::ui::SurvivorsAlbumEntry{.name = "Golem Card", .dropped_by = "Golem"}};
  Drawn drawn;
  drawn.state.tab = SurvivorsCodexTab::kCards;
  drawn.album_view = ro::ui::SurvivorsAlbumView{.cards = cards, .card_back = kCardBack};
  Settle(ui, drawn);
  CHECK(PaintsTexture(kCardBack));
  CHECK(drawn.state.tab == SurvivorsCodexTab::kCards);
}

TEST_CASE("Codex : une liste vide tient, à chaque onglet", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  Drawn drawn;
  for (std::size_t tab = 0; tab < ro::ui::kSurvivorsCodexTabCount; ++tab) {
    drawn.state.tab = static_cast<SurvivorsCodexTab>(tab);
    drawn.state.tab_wanted = drawn.state.tab;
    Settle(ui, drawn);
    CHECK(drawn.state.tab == static_cast<SurvivorsCodexTab>(tab));
  }
}

TEST_CASE("Codex : huit niveaux tiennent dans la largeur de la fiche", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView skills(true);
  Drawn drawn;
  drawn.view = skills.view;
  Settle(ui, drawn);
  const ImGuiWindow* sheet = ChildNamed("##codex_sheet");
  REQUIRE(sheet != nullptr);
  CHECK_FALSE(sheet->ScrollbarX);
  CHECK(sheet->ContentSize.x <= sheet->InnerRect.GetWidth() + 1.0F);
}

TEST_CASE("Codex : la lecture seule ne rend aucune intention", "[ui][survivors][codex]") {
  ro::ui::UiContext ui;
  SkillsView skills(true);
  Drawn drawn;
  drawn.view = skills.view;
  Settle(ui, drawn);
  const ImGuiWindow* window = ImGui::FindWindowByName("###survivors_codex");
  REQUIRE(window != nullptr);
  const ImRect bounds = window->InnerRect;
  bool asked = false;
  for (float y = bounds.Min.y; y < bounds.Max.y && !asked; y += kSweepStep) {
    for (float x = bounds.Min.x; x < bounds.Max.x && !asked; x += kSweepStep) {
      asked = ClickAt(ui, drawn, ImVec2(x, y));
    }
  }
  CHECK_FALSE(asked);
  CHECK(drawn.state.open);
}
