// LA RECHERCHE D'UNE LONGUE LISTE D'UNE RÈGLE : la liste des monstres du serveur
// en compte des milliers. Ce qu'elle garde, et la liste ouverte qui vide son
// champ, y met la saisie et le remplit sans conflit d'identifiants.

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/companion_ai_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::CompanionAiWindowState;
using ro::ui::RuleChoice;
using ro::ui::RuleChoiceMatches;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 30.0F;
constexpr float kClickStepPixels = 14.0F;
// Plus de mille choix, comme la table du serveur.
constexpr int kMonsterCount = 1200;
constexpr int kFirstMonster = 1001;
constexpr int kMonsterPredicate = 13;
constexpr int kTargetSubject = 2;

// LA CONDITION « MONSTRE DE LA CIBLE » ET SA LONGUE LISTE, qui vit ici : la
// sorte la désigne.
struct MonsterBench {
  std::vector<RuleChoice> monsters_;
  std::vector<RuleChoice> is_not{{.value = 2, .label = "est"}, {.value = 3, .label = "n'est pas"}};
  std::vector<ro::ui::RuleConditionKind> conditions;

  MonsterBench() {
    for (int index = 0; index < kMonsterCount; ++index) {
      monsters_.push_back(RuleChoice{.value = kFirstMonster + index,
                                     .label = "Monstre " + std::to_string(kFirstMonster + index),
                                     .detail = "niv. 1"});
    }
    ro::ui::RuleConditionKind monster;
    monster.subject = kTargetSubject;
    monster.predicate = kMonsterPredicate;
    monster.label = "Monstre de la cible";
    monster.comparisons = is_not;
    monster.list = ro::ui::RuleListField::kId;
    monster.choices = monsters_;
    conditions.push_back(monster);
  }
};

}  // namespace

TEST_CASE("Recherche d'une règle : le libellé à la casse près, ou l'identifiant",
          "[ui][companion_ai_rule_search]") {
  const RuleChoice baphomet{.value = 1039, .label = "Baphomet", .detail = "niv. 81 · MVP"};
  const RuleChoice elder{.value = 1377, .label = "Élder", .detail = ""};
  CHECK(RuleChoiceMatches(baphomet, ""));
  CHECK(RuleChoiceMatches(baphomet, "baph"));
  CHECK(RuleChoiceMatches(baphomet, "HOMET"));
  CHECK(RuleChoiceMatches(baphomet, "103"));
  CHECK_FALSE(RuleChoiceMatches(baphomet, "poring"));
  // Ce qui aide à choisir n'est pas cherché : « MVP » trouverait tous les MVP
  // par leur nom, pas par leur détail.
  CHECK_FALSE(RuleChoiceMatches(baphomet, "MVP"));
  // La casse se replie hors de l'ASCII.
  CHECK(RuleChoiceMatches(elder, "élder"));
}

TEST_CASE("Recherche d'une règle : la longue liste s'ouvre sur sa recherche",
          "[ui][companion_ai_rule_search]") {
  ro::ui::UiContext ui;
  ImGuiIO& io = ImGui::GetIO();
  int errors = 0;
  io.ConfigErrorRecoveryEnableAssert = false;
  io.ConfigErrorRecoveryEnableDebugLog = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  io.ConfigDebugHighlightIdConflicts = true;
  GImGui->ErrorCallback = [](ImGuiContext*, void* counted, const char*) {
    ++*static_cast<int*>(counted);
  };
  GImGui->ErrorCallbackUserData = &errors;

  const MonsterBench bench;
  const std::vector<ro::ui::RuleActionKind> actions{{.action = 0, .label = "Attaquer"}};
  ro::ui::RuleFields attack{.action = 0};
  attack.conditions = {{.subject = kTargetSubject,
                        .predicate = kMonsterPredicate,
                        .comparison = 2,
                        .id = kFirstMonster}};
  const std::vector<ro::ui::RuleRowView> rules{
      {.sentence = "Si la cible est Monstre 1001, alors attaquer", .fields = attack}};

  ro::ui::CompanionAiView view;
  view.homunculus_present = true;
  view.companion = ro::ui::kCompanionAiHomunculus;
  view.companion_name = "Vanilmirth";
  view.profile = "Mon profil";
  view.rules = rules;
  // L'en-tête des profils, que chaque section montre.
  const std::vector<ro::ui::ProfileChoiceView> profiles{
      {.key = "Basic AI", .label = "IA basique"}, {.key = "My profile", .label = "Mon profil"}};
  view.profiles.profiles = profiles;
  view.profiles.edited = 1;
  view.profiles.chosen = {1, -1, -1};
  view.profiles.fallback = {"Par défaut (Mon profil)", "Comme en PvE (Mon profil)",
                            "Comme en PvE (Mon profil)"};
  view.vocabulary = {.conditions = bench.conditions, .actions = actions, .step_most = 1};

  const ro::ui::SkinArt art;
  CompanionAiWindowState state;
  int conflicts = 0;
  const auto frame = [&]() {
    state.open = true;
    state.companion = view.companion;
    state.expanded_rule = 0;
    state.section_asked = static_cast<int>(ro::ui::CompanionAiSection::kRules);
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawCompanionAiWindow(state, view, art));
    ui.EndFrame();
    conflicts += GImGui->DebugDrawIdConflictsId != 0 ? 1 : 0;
  };
  frame();
  const ImGuiWindow* window = ImGui::FindWindowByName(ro::ui::kCompanionAiWindowId);
  REQUIRE(window != nullptr);
  const ImRect area = window->Rect();

  // UN TEXTE LAISSÉ PAR UNE RECHERCHE PRÉCÉDENTE : la liste qui l'efface est
  // celle qui s'ouvre sur sa recherche.
  constexpr std::string_view kLeftOver = "reste";
  bool searched = false;
  for (float y = area.Min.y; y < area.Max.y && !searched; y += kClickStepPixels) {
    for (float x = area.Min.x; x < area.Max.x && !searched; x += kClickStepPixels) {
      std::ranges::copy(kLeftOver, state.list_filter.begin());
      state.list_filter[kLeftOver.size()] = '\0';
      io.AddMousePosEvent(x, y);
      frame();
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
      frame();
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
      frame();
      frame();
      if (!GImGui->OpenPopupStack.empty() && std::string_view(state.list_filter.data()).empty()) {
        searched = true;
        break;
      }
      if (!GImGui->OpenPopupStack.empty()) {
        io.AddKeyEvent(ImGuiKey_Escape, true);
        frame();
        io.AddKeyEvent(ImGuiKey_Escape, false);
        frame();
      }
    }
  }
  REQUIRE(searched);
  // LA SAISIE EST DANS LE CHAMP dès l'ouverture : on tape sans cliquer.
  io.AddInputCharactersUTF8("1150");
  frame();
  frame();
  CHECK(std::string_view(state.list_filter.data()) == "1150");
  CHECK_FALSE(GImGui->OpenPopupStack.empty());
  CHECK(errors == 0);
  CHECK(conflicts == 0);
}
