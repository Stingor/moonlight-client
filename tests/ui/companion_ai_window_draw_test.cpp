// LA FENÊTRE « IA DU COMPAGNON » DESSINÉE : chaque section, une règle et une
// ligne de tactique dépliées, la souris promenée sur toute la fenêtre. La pile
// d'identifiants d'ImGui reste équilibrée, et aucun identifiant survolé n'est
// porté par deux éléments. Chaque liste déroulante s'ouvre, et les onglets se
// choisissent même sous l'IA basique.

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/companion_ai_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

using ro::ui::CompanionAiSection;
using ro::ui::CompanionAiView;
using ro::ui::CompanionAiWindowState;
using ro::ui::RuleChoice;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 30.0F;
// Le balayage de la souris : un pas assez fin pour passer sur chaque ligne.
constexpr float kSweepStepPixels = 14.0F;
constexpr float kSweepLeftPixels = 0.0F;
constexpr float kSweepTopPixels = 0.0F;
constexpr float kSweepRightPixels = 1280.0F;
constexpr float kSweepBottomPixels = 900.0F;
// UNE COLONNE QUI TRAVERSE LES CURSEURS de la fenêtre, à sa place d'ouverture.
constexpr float kSliderColumnPixels = 480.0F;
constexpr std::uint16_t kVanilmirthClass = 6009;
constexpr std::uint16_t kArcherClass = 6017;

// UN BANC DE VUE : ses listes vivent ici, la vue les désigne.
struct Bench {
  std::vector<RuleChoice> below_above{{.value = 0, .label = "sous"},
                                      {.value = 1, .label = "au-dessus de"}};
  std::vector<RuleChoice> is_not{{.value = 2, .label = "est"}, {.value = 3, .label = "n'est pas"}};
  std::vector<RuleChoice> races_{{.value = 0, .label = "Formless"},
                                 {.value = 1, .label = "Undead"}};
  std::vector<RuleChoice> enemy_aims{{.value = 0, .label = "la cible"},
                                     {.value = 3, .label = "ce que le maître attaque"}};
  std::vector<RuleChoice> only_is{{.value = 2, .label = "au moins"}};
  std::vector<RuleChoice> around_{{.value = 0, .label = "du compagnon"},
                                  {.value = 1, .label = "du maître"}};
  std::vector<RuleChoice> statuses_{{.value = 12, .label = "Blessing"},
                                    {.value = 13, .label = "Increase AGI"}};
  std::vector<ro::ui::RuleConditionKind> conditions;
  std::vector<ro::ui::RuleActionKind> actions;
  std::vector<ro::ui::RuleSkillChoice> skills;
  std::vector<ro::ui::RuleRowView> rules;
  std::vector<std::string> rejected{"Règle 3 du fichier : illisible"};
  std::vector<ro::ui::OffensiveSkillRow> offensive{
      {.skill_id = 8001, .label = "Caprice", .learned_level = 5}};
  std::vector<ro::ui::TacticRowView> tactics;
  std::vector<std::string> friends{"Alice", "Bob"};
  std::vector<ro::ui::ProfileChoiceView> profiles{{.key = "Basic AI", .label = "IA basique"},
                                                  {.key = "My profile", .label = "Mon profil"},
                                                  {.key = "Farm", .label = "Farm"}};
  std::vector<std::string> imports{"Archer, rang 3 — Tir", "Autre · Filir — Chasse"};
  std::vector<ro::ui::ConductRowView> conducts{{.label = "Rival", .rank = 0, .trigger = 1},
                                               {.label = "Priest", .rank = 6, .trigger = 0}};
  std::vector<RuleChoice> player_jobs{{.value = 8, .label = "Priest"},
                                      {.value = 7, .label = "Knight"}};

  Bench() {
    ro::ui::RuleConditionKind hp;
    hp.subject = 0;
    hp.predicate = 0;
    hp.label = "PV du maître";
    hp.comparisons = below_above;
    hp.numbers[0] = {
        .field = ro::ui::RuleNumberField::kNumber, .least = 0, .most = 100, .label = "pour cent"};
    conditions.push_back(hp);
    ro::ui::RuleConditionKind race;
    race.subject = 2;
    race.predicate = 11;
    race.label = "Race de la cible";
    race.comparisons = is_not;
    race.list = ro::ui::RuleListField::kChoice;
    race.choices = races_;
    conditions.push_back(race);
    // UN NOMBRE TAPÉ (au-delà de mille), DEUX NOMBRES ET UNE LISTE, UNE LISTE
    // D'IDENTIFIANTS AVEC SON INFOBULLE.
    ro::ui::RuleConditionKind points;
    points.subject = 1;
    points.predicate = 2;
    points.label = "SP du compagnon (points)";
    points.comparisons = below_above;
    points.numbers[0] = {
        .field = ro::ui::RuleNumberField::kNumber, .least = 0, .most = 99999, .label = "SP"};
    conditions.push_back(points);
    ro::ui::RuleConditionKind crowd;
    crowd.subject = 3;
    crowd.predicate = 18;
    crowd.label = "Ennemis autour";
    crowd.comparisons = only_is;
    crowd.numbers[0] = {.field = ro::ui::RuleNumberField::kNumber,
                        .least = 1,
                        .most = 50,
                        .label = "ennemis (poids)"};
    crowd.numbers[1] = {.field = ro::ui::RuleNumberField::kCells,
                        .least = 1,
                        .most = 20,
                        .label = "à moins de (cases)"};
    crowd.list = ro::ui::RuleListField::kChoice;
    crowd.choices = around_;
    conditions.push_back(crowd);
    ro::ui::RuleConditionKind status;
    status.subject = 1;
    status.predicate = 5;
    status.label = "Statut du compagnon";
    status.comparisons = is_not;
    status.list = ro::ui::RuleListField::kId;
    status.choices = statuses_;
    status.tip = "sans liste du serveur, faux";
    conditions.push_back(status);
    actions.push_back({.action = 0, .label = "Attaquer", .aims = enemy_aims});
    actions.push_back({.action = 1, .label = "Lancer un skill", .needs_skill = true});
    actions.push_back({.action = 3,
                       .label = "S'écarter",
                       .needs_step = true,
                       .aims = enemy_aims,
                       .tip = "il s'éloigne"});
    skills.push_back(
        {.skill_id = 8001, .label = "Caprice", .learned_level = 5, .aims = enemy_aims});
    ro::ui::RuleFields cast{.action = 1, .aim = 0, .skill_id = 8001, .skill_level = 3};
    cast.conditions = {{.subject = 0, .predicate = 0, .comparison = 0, .number = 50},
                       {.subject = 2, .predicate = 11, .comparison = 2, .choice = 1}};
    rules.push_back({.sentence = "Si PV du maître < 50 %, alors lancer Caprice niv. 3 sur la cible",
                     .fields = cast,
                     .deciding = true});
    ro::ui::RuleFields attack{.action = 0, .aim = 0};
    attack.conditions = {{.subject = 0, .predicate = 0, .comparison = 1, .number = 80}};
    rules.push_back(
        {.sentence = "Si PV du maître > 80 %, alors attaquer la cible", .fields = attack});
    ro::ui::RuleFields away{.action = 3, .aim = 0, .step_cells = 4};
    away.conditions = {{.subject = 1, .predicate = 2, .comparison = 0, .number = 1500},
                       {.subject = 3, .predicate = 18, .comparison = 2, .number = 3, .cells = 5},
                       {.subject = 1, .predicate = 5, .comparison = 3, .id = 12}};
    rules.push_back({.sentence = "Si …, alors s'écarter", .fields = away});
    rules.push_back({.sentence = "Si PV du maître < 50 %, alors lancer Morsure",
                     .fields = cast,
                     .unusable = "Ce compagnon ne possède pas ce skill."});
    tactics.push_back({.label = "Poring", .rank = 1, .trigger = 0});
    tactics.push_back({.label = "Tout autre monstre", .rank = 2, .trigger = 1});
  }

  [[nodiscard]] CompanionAiView View() const {
    CompanionAiView view;
    view.homunculus_present = true;
    view.mercenary_present = true;
    view.companion = ro::ui::kCompanionAiHomunculus;
    view.companion_class = kVanilmirthClass;
    view.companion_name = "Vanilmirth";
    view.refusal = "refusée";
    view.refused_rule = 2;
    view.profile = "Mon profil";
    view.base.permissions = {{.skill_id = 8001, .allowed = true, .level = 0}};
    view.offensive_skills = offensive;
    view.tactics = tactics;
    view.rules = rules;
    view.rejected_rules = rejected;
    view.vocabulary = {
        .conditions = conditions, .actions = actions, .skills = skills, .step_most = 20};
    view.friends = friends;
    view.profiles.profiles = profiles;
    view.profiles.edited = 1;
    view.profiles.chosen = {2, -1, 0};
    view.profiles.fallback = {"Par défaut (Mon profil)", "Comme en PvE (Farm)",
                              "Comme en PvE (Farm)"};
    view.profiles.current_context = 1;
    view.profiles.imports = imports;
    view.profiles.proposal = "Recopier le profil de Lif ?";
    view.conducts = conducts;
    view.conduct_generic_present = {true, false};
    view.player_jobs = player_jobs;
    return view;
  }
};

}  // namespace

TEST_CASE("Fenêtre IA du compagnon : chaque section se dessine sans conflit d'identifiants",
          "[ui][companion_ai]") {
  ro::ui::UiContext ui;
  ImGuiIO& io = ImGui::GetIO();
  // ⚠ LES ERREURS SE COMPTENT, ELLES N'INTERROMPENT PAS : une boîte d'assertion
  // attendrait un clic sur l'écran du joueur.
  int errors = 0;
  io.ConfigErrorRecoveryEnableAssert = false;
  io.ConfigErrorRecoveryEnableDebugLog = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  io.ConfigDebugHighlightIdConflicts = true;
  GImGui->ErrorCallback = [](ImGuiContext*, void* counted, const char*) {
    ++*static_cast<int*>(counted);
  };
  GImGui->ErrorCallbackUserData = &errors;
  const ro::ui::SkinArt art;
  const Bench bench;
  const CompanionAiView view = bench.View();
  CompanionAiWindowState state;
  state.open = true;
  state.expanded_rule = 0;
  state.expanded_tactic = 0;

  int conflicts = 0;
  int frames_drawn = 0;
  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawCompanionAiWindow(state, view, art));
    ui.EndFrame();
    ++frames_drawn;
    if (GImGui->DebugDrawIdConflictsId != 0) {
      ++conflicts;
    }
  };
  const auto sweep = [&]() {
    for (float y = kSweepTopPixels; y < kSweepBottomPixels; y += kSweepStepPixels) {
      for (float x = kSweepLeftPixels; x < kSweepRightPixels; x += kSweepStepPixels * 4.0F) {
        io.AddMousePosEvent(x, y);
        frame();
      }
    }
  };
  // La confirmation de remise à zéro, pour CE compagnon : elle se dessine.
  state.reset_asked = true;
  state.reset_companion = view.companion;
  state.reset_class = view.companion_class;
  state.expanded_conduct = 0;
  for (const CompanionAiSection section :
       {CompanionAiSection::kSettings, CompanionAiSection::kTactics, CompanionAiSection::kPlayers,
        CompanionAiSection::kFriends}) {
    state.section_asked = static_cast<int>(section);
    frame();
    sweep();
  }
  CHECK(state.reset_asked);
  // CHAQUE SAISIE DE L'EN-TÊTE DES PROFILS tour à tour, l'import compris.
  for (const ro::ui::ProfileDialog dialog :
       {ro::ui::ProfileDialog::kNew, ro::ui::ProfileDialog::kRename,
        ro::ui::ProfileDialog::kDuplicate, ro::ui::ProfileDialog::kImport,
        ro::ui::ProfileDialog::kDelete}) {
    state.profile_dialog = dialog;
    state.section_asked = static_cast<int>(CompanionAiSection::kPlayers);
    frame();
    sweep();
    state.profile_dialog = dialog;
  }
  // CHAQUE RÈGLE DÉPLIÉE TOUR À TOUR, la dernière avec son refus.
  for (int rule = 0; rule < static_cast<int>(bench.rules.size()); ++rule) {
    state.section_asked = static_cast<int>(CompanionAiSection::kRules);
    state.expanded_rule = rule;
    frame();
    sweep();
  }
  CHECK(frames_drawn > 0);
  CHECK(state.open);
  // La pile d'Échap la trouve par son identité.
  CHECK(ImGui::FindWindowByName(ro::ui::kCompanionAiWindowId) != nullptr);
  CHECK(errors == 0);
  CHECK(conflicts == 0);
}

TEST_CASE("Fenêtre IA du compagnon : le balayage voit un conflit d'identifiants",
          "[ui][companion_ai]") {
  // LE TÉMOIN : deux boutons de même libellé. Sans conflit vu ici, le « zéro
  // conflit » de l'essai précédent ne prouverait rien.
  ro::ui::UiContext ui;
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigErrorRecoveryEnableAssert = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  io.ConfigDebugHighlightIdConflicts = true;
  int conflicts = 0;
  for (float y = kSweepTopPixels; y < kSweepBottomPixels / 4.0F; y += kSweepStepPixels / 2.0F) {
    io.AddMousePosEvent(kSweepStepPixels * 2.0F, y);
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::Begin("témoin");
    static_cast<void>(ImGui::Button("pareil"));
    static_cast<void>(ImGui::Button("pareil"));
    ImGui::End();
    ui.EndFrame();
    conflicts += GImGui->DebugDrawIdConflictsId != 0 ? 1 : 0;
  }
  CHECK(conflicts > 0);
}

TEST_CASE("Fenêtre IA du compagnon : chaque curseur de distance dit sa zone au survol",
          "[ui][companion_ai]") {
  // ⚠ SOUS L'IA BASIQUE AUSSI, où les curseurs sont grisés : la zone se montre
  // même quand elle ne se règle pas.
  for (const bool basic : {false, true}) {
    ro::ui::UiContext ui;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableTooltip = false;
    const ro::ui::SkinArt art;
    const Bench bench;
    CompanionAiView view = bench.View();
    view.basic = basic;
    CompanionAiWindowState state;
    state.open = true;
    state.section_asked = static_cast<int>(CompanionAiSection::kSettings);
    std::vector<ro::ui::CompanionAiZoneSlider> seen;
    for (float y = kSweepTopPixels; y < kSweepBottomPixels; y += kSweepStepPixels / 2.0F) {
      io.AddMousePosEvent(kSliderColumnPixels, y);
      ui.BeginFrame(kViewport, kFrameSeconds);
      const ro::ui::CompanionAiWindowAction action =
          ro::ui::DrawCompanionAiWindow(state, view, art);
      ui.EndFrame();
      seen.push_back(action.hovered_zone);
      // Survoler n'est pas cliquer : la case ne bascule pas d'elle-même.
      CHECK_FALSE(action.ground_zones_toggled);
    }
    for (const ro::ui::CompanionAiZoneSlider zone :
         {ro::ui::CompanionAiZoneSlider::kFollow, ro::ui::CompanionAiZoneSlider::kLeashStill,
          ro::ui::CompanionAiZoneSlider::kLeashWalking, ro::ui::CompanionAiZoneSlider::kEngageStill,
          ro::ui::CompanionAiZoneSlider::kEngageWalking}) {
      CHECK(std::ranges::find(seen, zone) != seen.end());
    }
    CHECK(std::ranges::find(seen, ro::ui::CompanionAiZoneSlider::kNone) != seen.end());
  }
}

TEST_CASE("Fenêtre IA du compagnon : sans compagnon, rien ne se dessine", "[ui][companion_ai]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  CompanionAiWindowState state;
  state.open = true;
  const CompanionAiView empty;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ro::ui::CompanionAiWindowAction action = ro::ui::DrawCompanionAiWindow(state, empty, art);
  ui.EndFrame();
  CHECK(action.verb == ro::ui::CompanionAiVerb::kNothing);
  CHECK(ImGui::FindWindowByName(ro::ui::kCompanionAiWindowId) == nullptr);
}

TEST_CASE("Ligne IA d'un onglet : absente sans profil", "[ui][companion_ai]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("onglet");
  CHECK(ro::ui::DrawCompanionAiLine("", "", art) == ro::ui::CompanionAiLineVerb::kNothing);
  CHECK(ro::ui::DrawCompanionAiLine("Mon profil", "Recopier le profil de Lif ?", art) ==
        ro::ui::CompanionAiLineVerb::kNothing);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Fenêtre IA du compagnon : une confirmation ne passe jamais à l'autre compagnon",
          "[ui][companion_ai]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  const Bench bench;
  // DEMANDÉE SUR L'HOMONCULE…
  CompanionAiWindowState state;
  state.open = true;
  state.reset_asked = true;
  state.reset_companion = ro::ui::kCompanionAiHomunculus;
  state.reset_class = kVanilmirthClass;
  state.expanded_rule = 1;
  // … LA VUE DÉCRIT LE MERCENAIRE, sans que l'onglet ait été changé par son
  // chemin : la confirmation tombe au dessin, rien ne part.
  CompanionAiView mercenary = bench.View();
  mercenary.companion = ro::ui::kCompanionAiMercenary;
  mercenary.companion_class = kArcherClass;
  state.companion = ro::ui::kCompanionAiMercenary;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ro::ui::CompanionAiWindowAction action =
      ro::ui::DrawCompanionAiWindow(state, mercenary, art);
  ui.EndFrame();
  CHECK(action.verb != ro::ui::CompanionAiVerb::kResetToBasic);
  CHECK_FALSE(state.reset_asked);

  // CHANGER D'ONGLET remet à zéro tout ce qui est propre au compagnon.
  state.reset_asked = true;
  state.expanded_rule = 1;
  state.expanded_tactic = 0;
  state.friend_name[0] = 'A';
  ro::ui::ChangeAiWindowCompanion(state, ro::ui::kCompanionAiHomunculus);
  CHECK(state.companion == ro::ui::kCompanionAiHomunculus);
  CHECK_FALSE(state.reset_asked);
  CHECK(state.expanded_rule == -1);
  CHECK(state.expanded_tactic == -1);
  CHECK(state.friend_name[0] == '\0');
  // … LE PROFIL ÉDITÉ ET SA SAISIE AUSSI : ils sont ceux de l'autre compagnon.
  state.edited_profile = "Farm";
  state.profile_dialog = ro::ui::ProfileDialog::kRename;
  state.expanded_conduct = 1;
  ro::ui::ChangeAiWindowCompanion(state, ro::ui::kCompanionAiMercenary);
  CHECK(state.edited_profile.empty());
  CHECK(state.profile_dialog == ro::ui::ProfileDialog::kNone);
  CHECK(state.expanded_conduct == -1);
}

TEST_CASE("Fenêtre IA du compagnon : l'IA basique se montre sans se régler", "[ui][companion_ai]") {
  ro::ui::UiContext ui;
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigErrorRecoveryEnableAssert = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  io.ConfigDebugHighlightIdConflicts = true;
  const ro::ui::SkinArt art;
  const Bench bench;
  CompanionAiView view = bench.View();
  view.basic = true;
  view.profiles.edited = 0;
  view.profiles.proposal.clear();
  CompanionAiWindowState state;
  state.open = true;
  int conflicts = 0;
  for (float y = kSweepTopPixels; y < kSweepBottomPixels; y += kSweepStepPixels) {
    io.AddMousePosEvent(kSweepRightPixels / 2.0F, y);
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::CompanionAiWindowAction action = ro::ui::DrawCompanionAiWindow(state, view, art);
    ui.EndFrame();
    conflicts += GImGui->DebugDrawIdConflictsId != 0 ? 1 : 0;
    // Un geste, s'il en part un, viserait le profil édité.
    CHECK((action.verb == ro::ui::CompanionAiVerb::kNothing || action.profile == "Basic AI"));
  }
  CHECK(conflicts == 0);
}

TEST_CASE("Fenêtre IA du compagnon : sous l'IA basique, les onglets se choisissent",
          "[ui][companion_ai]") {
  ro::ui::UiContext ui;
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigErrorRecoveryEnableAssert = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  const ro::ui::SkinArt art;
  const Bench bench;
  CompanionAiView view = bench.View();
  view.basic = true;
  view.profiles.edited = 0;
  CompanionAiWindowState state;
  state.open = true;
  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawCompanionAiWindow(state, view, art));
    ui.EndFrame();
  };
  // LES DEUX PREMIÈRES IMAGES MESURENT, fenêtre cachée : elle ne prend le clic
  // qu'à la troisième.
  frame();
  frame();
  frame();
  // LA BARRE DES SECTIONS : celle qui porte un onglet par section.
  constexpr int kSectionTabs = 5;
  ImGuiTabBar* sections = nullptr;
  for (int index = 0; index < GImGui->TabBars.GetMapSize(); ++index) {
    if (ImGuiTabBar* bar = GImGui->TabBars.TryGetMapData(index);
        bar != nullptr && bar->Tabs.Size == kSectionTabs) {
      sections = bar;
    }
  }
  REQUIRE(sections != nullptr);
  for (int tab = 0; tab < kSectionTabs; ++tab) {
    const ImGuiTabItem& item = sections->Tabs[tab];
    if (item.ID == sections->SelectedTabId) {
      continue;
    }
    const ImGuiID wanted = item.ID;
    io.AddMousePosEvent(sections->BarRect.Min.x + item.Offset + item.Width / 2.0F,
                        (sections->BarRect.Min.y + sections->BarRect.Max.y) / 2.0F);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame();
    frame();
    CHECK(sections->SelectedTabId == wanted);
  }
}

TEST_CASE("Fenêtre IA du compagnon : chaque liste déroulante s'ouvre sans conflit",
          "[ui][companion_ai]") {
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
  const ro::ui::SkinArt art;
  const Bench bench;
  CompanionAiView view = bench.View();
  view.profiles.unused = true;
  CompanionAiWindowState state;
  int conflicts = 0;
  const auto frame = [&]() {
    // LES CLICS PEUVENT FERMER LA FENÊTRE, ÔTER LA SAISIE OU CHANGER DE
    // COMPAGNON : chaque image repart du même décor.
    state.open = true;
    state.companion = view.companion;
    state.profile_dialog = ro::ui::ProfileDialog::kImport;
    state.section_asked = static_cast<int>(CompanionAiSection::kPlayers);
    state.expanded_conduct = 0;
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawCompanionAiWindow(state, view, art));
    ui.EndFrame();
    conflicts += GImGui->DebugDrawIdConflictsId != 0 ? 1 : 0;
  };
  frame();
  const ImGuiWindow* window = ImGui::FindWindowByName(ro::ui::kCompanionAiWindowId);
  REQUIRE(window != nullptr);
  const ImRect area = window->Rect();
  // UN CLIC À CHAQUE PAS ; une liste ouverte se parcourt, puis Échap la ferme.
  constexpr float kClickStepPixels = 2.0F * kSweepStepPixels;
  std::vector<ImGuiID> opened;
  for (float y = area.Min.y; y < area.Max.y; y += kClickStepPixels) {
    for (float x = area.Min.x; x < area.Max.x; x += kClickStepPixels) {
      io.AddMousePosEvent(x, y);
      frame();
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
      frame();
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
      frame();
      if (GImGui->OpenPopupStack.empty()) {
        continue;
      }
      const ImGuiID popup = GImGui->OpenPopupStack.back().PopupId;
      if (std::ranges::find(opened, popup) == opened.end()) {
        opened.push_back(popup);
      }
      io.AddMousePosEvent(x, y + kClickStepPixels);
      frame();
      io.AddKeyEvent(ImGuiKey_Escape, true);
      frame();
      io.AddKeyEvent(ImGuiKey_Escape, false);
      frame();
    }
  }
  // LE PROFIL ÉDITÉ, LES TROIS CONTEXTES, L'IMPORT ET LES CLASSES, au moins.
  constexpr std::size_t kCombosLeast = 6;
  CHECK(opened.size() >= kCombosLeast);
  CHECK(errors == 0);
  CHECK(conflicts == 0);
}

TEST_CASE("Fenêtre IA du compagnon : les dépliés suivent un geste accepté", "[ui][companion_ai]") {
  using ro::ui::CompanionAiVerb;
  CompanionAiWindowState state;
  const auto after = [&state](CompanionAiVerb verb, std::size_t index, std::size_t to,
                              std::size_t count) {
    ro::ui::CompanionAiWindowAction action;
    action.verb = verb;
    action.index = index;
    action.to = to;
    ro::ui::FollowAcceptedAiGesture(state, action, count);
  };
  state.expanded_rule = 2;
  after(CompanionAiVerb::kMoveRule, 2, 0, 4);
  CHECK(state.expanded_rule == 0);
  after(CompanionAiVerb::kMoveRule, 3, 0, 4);
  CHECK(state.expanded_rule == 1);
  after(CompanionAiVerb::kRemoveRule, 0, 0, 3);
  CHECK(state.expanded_rule == 0);
  after(CompanionAiVerb::kDuplicateRule, 0, 0, 4);
  CHECK(state.expanded_rule == 0);
  after(CompanionAiVerb::kRemoveRule, 0, 0, 3);
  CHECK(state.expanded_rule == -1);
  after(CompanionAiVerb::kAddRule, 0, 0, 4);
  CHECK(state.expanded_rule == 3);
  state.expanded_tactic = 3;
  after(CompanionAiVerb::kRemoveTactic, 1, 0, 4);
  CHECK(state.expanded_tactic == 2);
  after(CompanionAiVerb::kRemoveTactic, 2, 0, 4);
  CHECK(state.expanded_tactic == -1);
  state.expanded_conduct = 2;
  after(CompanionAiVerb::kRemoveConduct, 0, 0, 3);
  CHECK(state.expanded_conduct == 1);
  after(CompanionAiVerb::kRemoveConduct, 1, 0, 2);
  CHECK(state.expanded_conduct == -1);
}

TEST_CASE("Fenêtre IA du compagnon : une condition et une action se ramènent dans leur sorte",
          "[ui][companion_ai]") {
  const Bench bench;
  const CompanionAiView view = bench.View();
  // UNE COMPARAISON ABSENTE DE LA SORTE prend la première, les nombres leurs
  // bornes, le choix le premier de la liste.
  ro::ui::RuleConditionFields condition{.comparison = 0, .number = 500, .cells = 99, .choice = 7};
  ro::ui::FitRuleCondition(condition, bench.conditions[3]);
  CHECK(condition.subject == 3);
  CHECK(condition.predicate == 18);
  CHECK(condition.comparison == 2);
  CHECK(condition.number == 50);
  CHECK(condition.cells == 20);
  CHECK(condition.choice == 0);
  // Un identifiant de la liste reste.
  ro::ui::RuleConditionFields status{.comparison = 3, .id = 13};
  ro::ui::FitRuleCondition(status, bench.conditions[4]);
  CHECK(status.comparison == 3);
  CHECK(status.id == 13);

  // LANCER UN SKILL : un skill possédé, son niveau borné à l'appris.
  ro::ui::RuleFields rule{.action = 1, .aim = 5, .skill_id = 0, .skill_level = 9};
  ro::ui::FitRuleAction(rule, bench.actions[1], view.vocabulary);
  CHECK(rule.skill_id == 8001);
  CHECK(rule.skill_level == 5);
  CHECK(rule.aim == 0);
  // S'ÉCARTER : plus de skill, un pas dans ses bornes.
  ro::ui::FitRuleAction(rule, bench.actions[2], view.vocabulary);
  CHECK(rule.skill_id == 0);
  CHECK(rule.step_cells == 1);
  // UNE ACTION SANS CIBLE ne garde pas celle d'avant.
  rule.aim = 3;
  const ro::ui::RuleActionKind back{.action = 2, .label = "Revenir au maître"};
  ro::ui::FitRuleAction(rule, back, view.vocabulary);
  CHECK(rule.aim == 0);
  CHECK(rule.step_cells == 0);
}

TEST_CASE("Fenêtre IA du compagnon : un nom d'ami perd ses espaces", "[ui][companion_ai]") {
  CHECK(ro::ui::TrimmedCharacterName("  Bob ") == "Bob");
  CHECK(ro::ui::TrimmedCharacterName("Le Roi") == "Le Roi");
  CHECK(ro::ui::TrimmedCharacterName("   ").empty());
}
