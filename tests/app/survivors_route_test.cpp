// LA ROUTE DES ÉVÉNEMENTS D'UNE PARTIE DE SURVIVORS, sans fenêtre : ce qui est
// pris, ce qui passe, et l'intention qui en sort. La touche Z d'un clavier
// AZERTY est le scancode W : elle donne le cap nord. Échap met en pause ; la
// perte de focus aussi, sans avaler l'événement.

#include "survivors/survivors_route.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "survivors/survivors_screen_look.h"
#include "survivors/survivors_stage.h"

using moonclient::kLeftMouseButton;
using moonclient::kRightMouseButton;
using moonclient::kScancodeA;
using moonclient::kScancodeDigit1;
using moonclient::kScancodeDigit4;
using moonclient::kScancodeDown;
using moonclient::kScancodeEscape;
using moonclient::kScancodeKeypadEnter;
using moonclient::kScancodeReturn;
using moonclient::kScancodeSpace;
using moonclient::kScancodeW;
using moonclient::SurvivorsEvent;
using moonclient::SurvivorsEventKind;
using moonclient::SurvivorsGestures;
using moonclient::SurvivorsKeyFacts;
using moonclient::SurvivorsKeyTurn;
using moonclient::SurvivorsKeyTurnOf;
using moonclient::SurvivorsReadsKey;
using moonclient::SurvivorsRoute;
using moonclient::SurvivorsScreen;

namespace {

// Le cap du nord sur 64 valeurs.
constexpr std::uint8_t kNorthHeading = 0;
// Un scancode qui n'est ni une touche de marche ni une touche du mode : la
// lettre Q d'un clavier QWERTY, la barre de raccourcis d'un AZERTY sur A.
constexpr std::uint32_t kScancodeQ = 20;

SurvivorsEvent Key(SurvivorsEventKind kind, std::uint32_t scancode) {
  return SurvivorsEvent{.kind = kind, .scancode = scancode};
}

// L'intention que l'image tire des gestes, comme le mode la compose.
moonclient::SurvivorsIntent IntentOf(const SurvivorsRoute& route,
                                     const SurvivorsGestures& gestures) {
  moonclient::SurvivorsIntent intent;
  intent.heading = route.keys().Heading();
  intent.escape = gestures.escape;
  intent.focus_lost = gestures.focus_lost;
  return intent;
}

// Un étage dont la partie en or vient d'arriver : la descente est finie.
void BeginGolden(moonclient::SurvivorsStage& stage) {
  using ro::test_support::GoldenRules;
  stage.Open();
  const auto begun =
      stage.Begin(GoldenRules(), ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells),
                  ro::test_support::GoldenStart(ro::test_support::kGoldenSeed));
  REQUIRE(begun.has_value());
  stage.Loaded();
  stage.Advance(moonclient::kArrivalMs, moonclient::SurvivorsIntent{});
  REQUIRE(stage.screen() == SurvivorsScreen::kPlaying);
}

}  // namespace

TEST_CASE("survivors route : Z sur AZERTY, le scancode W, donne le cap nord", "[app][survivors]") {
  SurvivorsRoute route;
  CHECK(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeW)));
  CHECK(route.keys().Heading() == kNorthHeading);
  CHECK(route.Take(Key(SurvivorsEventKind::kKeyUp, kScancodeW)));
  CHECK(route.keys().Heading() == ro::sim::kNoHeading);
}

TEST_CASE("survivors route : le clavier, la souris et la molette sont tous pris",
          "[app][survivors]") {
  SurvivorsRoute route;
  // Une touche sans rôle dans le mode est prise quand même : elle ne doit pas
  // déclencher une barre de raccourcis du jeu.
  CHECK(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeQ)));
  CHECK(route.Take(Key(SurvivorsEventKind::kKeyUp, kScancodeQ)));
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kText}));
  CHECK(route.Take(SurvivorsEvent{
      .kind = SurvivorsEventKind::kMouseDown, .button = kLeftMouseButton, .x = 10.0F, .y = 20.0F}));
  CHECK(route.mouse_held());
  CHECK(route.mouse_x() == 10.0F);
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseMove, .x = 30.0F, .y = 40.0F}));
  CHECK(route.mouse_y() == 40.0F);
  CHECK(
      route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseUp, .button = kLeftMouseButton}));
  CHECK_FALSE(route.mouse_held());
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kWheel, .wheel_notches = 2.0F}));
  CHECK(route.TakeGestures().wheel_notches == 2.0F);
  // Fenêtre, sortie, redimensionnement : rien de cela n'est pris.
  CHECK_FALSE(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kOther}));
}

TEST_CASE("survivors route : la molette sur une fenêtre ne touche pas la caméra",
          "[app][survivors]") {
  SurvivorsRoute route;
  // Prise quand même : elle ne doit pas parvenir au jeu resté derrière.
  CHECK(route.Take(SurvivorsEvent{
      .kind = SurvivorsEventKind::kWheel, .wheel_notches = 2.0F, .over_interface = true}));
  CHECK(route.TakeGestures().wheel_notches == 0.0F);
  // Hors des fenêtres, elle revient à la caméra de l'arène.
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kWheel, .wheel_notches = -1.0F}));
  CHECK(route.TakeGestures().wheel_notches == -1.0F);
}

TEST_CASE("survivors route : Échap met en pause, une fois par appui", "[app][survivors]") {
  SurvivorsRoute route;
  CHECK(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeEscape)));
  SurvivorsGestures gestures = route.TakeGestures();
  CHECK(gestures.escape);
  CHECK_FALSE(gestures.focus_lost);
  // Consommé : l'image suivante ne le revoit pas, ni sa répétition.
  CHECK_FALSE(route.TakeGestures().escape);
  SurvivorsEvent repeated = Key(SurvivorsEventKind::kKeyDown, kScancodeEscape);
  repeated.repeat = true;
  CHECK(route.Take(repeated));
  CHECK_FALSE(route.TakeGestures().escape);

  // L'étage, en jeu, entre en pause sur cette intention.
  moonclient::SurvivorsStage stage;
  BeginGolden(stage);
  REQUIRE(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeEscape)));
  stage.Advance(ro::sim::kSurvivorsStepMs, IntentOf(route, route.TakeGestures()));
  CHECK(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == moonclient::PauseCause::kPlayer);
}

TEST_CASE("survivors route : la perte de focus pause sans avaler l'événement", "[app][survivors]") {
  SurvivorsRoute route;
  REQUIRE(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeW)));
  REQUIRE(route.Take(
      SurvivorsEvent{.kind = SurvivorsEventKind::kMouseDown, .button = kLeftMouseButton}));
  CHECK_FALSE(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kFocusLost}));
  CHECK(route.TakeGestures().focus_lost);
  // Leurs relâchements n'arriveront pas : rien ne reste tenu.
  CHECK(route.keys().Heading() == ro::sim::kNoHeading);
  CHECK_FALSE(route.mouse_held());

  // L'étage, en jeu, entre en pause automatique (U6).
  moonclient::SurvivorsStage stage;
  BeginGolden(stage);
  REQUIRE_FALSE(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kFocusLost}));
  stage.Advance(ro::sim::kSurvivorsStepMs, IntentOf(route, route.TakeGestures()));
  CHECK(stage.screen() == SurvivorsScreen::kPaused);
  CHECK(stage.pause_cause() == moonclient::PauseCause::kFocusLost);
}

TEST_CASE("survivors route : la rangée des chiffres dit la carte par sa place",
          "[app][survivors]") {
  SurvivorsRoute route;
  REQUIRE(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeDigit1)));
  CHECK(route.TakeGestures().pressed_card_key == 1);
  REQUIRE(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeDigit4)));
  CHECK(route.TakeGestures().pressed_card_key == 4);
  CHECK(route.TakeGestures().pressed_card_key == 0);
}

TEST_CASE("survivors route : Entrée et Espace valident, une fois par appui", "[app][survivors]") {
  SurvivorsRoute route;
  for (const std::uint32_t scancode : {moonclient::kScancodeReturn, moonclient::kScancodeSpace,
                                       moonclient::kScancodeKeypadEnter}) {
    REQUIRE(route.Take(Key(SurvivorsEventKind::kKeyDown, scancode)));
    CHECK(route.TakeGestures().confirm);
    CHECK_FALSE(route.TakeGestures().confirm);
    SurvivorsEvent repeated = Key(SurvivorsEventKind::kKeyDown, scancode);
    repeated.repeat = true;
    REQUIRE(route.Take(repeated));
    CHECK_FALSE(route.TakeGestures().confirm);
  }
  REQUIRE(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeQ)));
  CHECK_FALSE(route.TakeGestures().confirm);
}

TEST_CASE("survivors route : Ctrl+Tab bascule la minimap, Tab seul non", "[app][survivors]") {
  SurvivorsRoute route;
  REQUIRE(route.Take(Key(SurvivorsEventKind::kKeyDown, moonclient::kScancodeTab)));
  CHECK_FALSE(route.TakeGestures().toggle_minimap);
  SurvivorsEvent with_ctrl = Key(SurvivorsEventKind::kKeyDown, moonclient::kScancodeTab);
  with_ctrl.ctrl = true;
  REQUIRE(route.Take(with_ctrl));
  CHECK(route.TakeGestures().toggle_minimap);
  CHECK_FALSE(route.TakeGestures().toggle_minimap);
  // La partie garde Tab : la minimap du jeu ne bascule pas en dessous.
  CHECK(moonclient::SurvivorsReadsKey(moonclient::kScancodeTab));
}

TEST_CASE("survivors route : grisé du choix et de la pause, noir et blanc de la mort",
          "[app][survivors]") {
  using moonclient::kSurvivorsDeathDesaturation;
  using moonclient::kSurvivorsGreyedDesaturation;
  using moonclient::SurvivorsDesaturation;
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kPlaying, false) == 0.0F);
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kArriving, false) == 0.0F);
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kPaused, false) == kSurvivorsGreyedDesaturation);
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kChoosing, false) == kSurvivorsGreyedDesaturation);
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kChest, false) == kSurvivorsGreyedDesaturation);
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kDying, true) == kSurvivorsDeathDesaturation);
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kSummary, true) == kSurvivorsDeathDesaturation);
  CHECK(SurvivorsDesaturation(SurvivorsScreen::kWon, false) == 0.0F);
  CHECK(moonclient::SurvivorsMusicPaused(SurvivorsScreen::kPaused));
  CHECK_FALSE(moonclient::SurvivorsMusicPaused(SurvivorsScreen::kChoosing));
  CHECK_FALSE(moonclient::SurvivorsArenaShown(SurvivorsScreen::kLoading));
  CHECK(moonclient::SurvivorsArenaShown(SurvivorsScreen::kSummary));
}

TEST_CASE("survivors route : la partie garde ses touches, l'enregistreur les autres",
          "[app][survivors]") {
  CHECK(SurvivorsReadsKey(kScancodeW));
  CHECK(SurvivorsReadsKey(kScancodeA));
  CHECK(SurvivorsReadsKey(kScancodeDown));
  CHECK(SurvivorsReadsKey(kScancodeEscape));
  CHECK(SurvivorsReadsKey(kScancodeDigit1));
  CHECK(SurvivorsReadsKey(kScancodeDigit4));
  CHECK_FALSE(SurvivorsReadsKey(kScancodeDigit4 + 1));
  CHECK(SurvivorsReadsKey(moonclient::kScancodeReturn));
  CHECK(SurvivorsReadsKey(moonclient::kScancodeSpace));
  CHECK_FALSE(SurvivorsReadsKey(kScancodeQ));
}

TEST_CASE("survivors route : un champ de texte prend toute frappe, et le héros lâche ses touches",
          "[app][survivors]") {
  SurvivorsRoute route;
  CHECK(route.Take(Key(SurvivorsEventKind::kKeyDown, kScancodeW)));
  REQUIRE(route.keys().Heading() == kNorthHeading);
  const SurvivorsKeyFacts typing{.typing = true};
  // Même les touches de la partie, Entrée et Échap : elles écrivent, valident
  // ou quittent le champ.
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyUp, kScancodeW), typing) ==
        SurvivorsKeyTurn::kTyping);
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeEscape), typing) ==
        SurvivorsKeyTurn::kTyping);
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeReturn), typing) ==
        SurvivorsKeyTurn::kTyping);
  CHECK(SurvivorsKeyTurnOf(SurvivorsEvent{.kind = SurvivorsEventKind::kText}, typing) ==
        SurvivorsKeyTurn::kTyping);
  // Le relâchement de W part au champ : la route lâche d'elle-même.
  route.ReleaseHeld();
  CHECK(route.keys().Heading() == ro::sim::kNoHeading);
  CHECK_FALSE(route.mouse_held());
  // La molette reste à l'arène.
  CHECK(SurvivorsKeyTurnOf(SurvivorsEvent{.kind = SurvivorsEventKind::kWheel}, typing) ==
        SurvivorsKeyTurn::kArena);
}

TEST_CASE("survivors route : Entrée ouvre la conversation, sauf sur le coffre",
          "[app][survivors]") {
  const SurvivorsEvent enter = Key(SurvivorsEventKind::kKeyDown, kScancodeReturn);
  CHECK(SurvivorsKeyTurnOf(enter, {}) == SurvivorsKeyTurn::kOpenChat);
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeKeypadEnter), {}) ==
        SurvivorsKeyTurn::kOpenChat);
  // Le coffre attend « Continuer » : Entrée y reste à l'arène.
  CHECK(SurvivorsKeyTurnOf(enter, {.chest_shown = true}) == SurvivorsKeyTurn::kArena);
  // Une Entrée tenue n'ouvre pas deux fois.
  SurvivorsEvent repeated = enter;
  repeated.repeat = true;
  CHECK(SurvivorsKeyTurnOf(repeated, {}) == SurvivorsKeyTurn::kArena);
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeSpace), {}) ==
        SurvivorsKeyTurn::kArena);
}

TEST_CASE("survivors route : Échap replie la barre dépliée avant de mettre en pause",
          "[app][survivors]") {
  const SurvivorsEvent escape = Key(SurvivorsEventKind::kKeyDown, kScancodeEscape);
  CHECK(SurvivorsKeyTurnOf(escape, {}) == SurvivorsKeyTurn::kArena);
  CHECK(SurvivorsKeyTurnOf(escape, {.chat_claims = true, .chat_folds_on_escape = true}) ==
        SurvivorsKeyTurn::kFoldChat);
}

TEST_CASE("survivors route : la barre dépliée prend les lettres, pas Entrée", "[app][survivors]") {
  const SurvivorsKeyFacts claims{.chat_claims = true};
  // Z sur AZERTY écrit dans la barre au lieu de faire marcher le héros.
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeW), claims) ==
        SurvivorsKeyTurn::kTyping);
  CHECK(SurvivorsKeyTurnOf(SurvivorsEvent{.kind = SurvivorsEventKind::kText}, claims) ==
        SurvivorsKeyTurn::kTyping);
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeReturn), claims) ==
        SurvivorsKeyTurn::kOpenChat);
}

TEST_CASE("survivors route : la touche Parler va à la voix, pas à l'arène", "[app][survivors]") {
  const SurvivorsKeyFacts talk{.talk_key = true};
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeQ), talk) ==
        SurvivorsKeyTurn::kVoice);
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyUp, kScancodeQ), talk) ==
        SurvivorsKeyTurn::kVoice);
  CHECK(SurvivorsKeyTurnOf(Key(SurvivorsEventKind::kKeyDown, kScancodeQ), {}) ==
        SurvivorsKeyTurn::kArena);
}

TEST_CASE("survivors route : un clic sur une fenêtre ne vise pas", "[app][survivors]") {
  SurvivorsRoute route;
  CHECK(route.Take(SurvivorsEvent{
      .kind = SurvivorsEventKind::kMouseDown, .button = kLeftMouseButton, .over_interface = true}));
  CHECK_FALSE(route.mouse_held());
  CHECK(route.Take(
      SurvivorsEvent{.kind = SurvivorsEventKind::kMouseDown, .button = kLeftMouseButton}));
  CHECK(route.mouse_held());
  // Le relâchement lâche, même au-dessus d'une fenêtre.
  CHECK(route.Take(SurvivorsEvent{
      .kind = SurvivorsEventKind::kMouseUp, .button = kLeftMouseButton, .over_interface = true}));
  CHECK_FALSE(route.mouse_held());
}

TEST_CASE("survivors route : le clic droit glissé tourne, le double clic droit remet au nord",
          "[app][survivors]") {
  constexpr float kStepPx = 12.0F;
  SurvivorsRoute route;
  // Sans bouton tenu, la souris ne tourne rien.
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseMove, .dx_px = kStepPx}));
  CHECK(route.TakeGestures().orbit_px == 0.0F);
  CHECK(route.Take(
      SurvivorsEvent{.kind = SurvivorsEventKind::kMouseDown, .button = kRightMouseButton}));
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseMove, .dx_px = kStepPx}));
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseMove, .dx_px = -kStepPx / 2}));
  const SurvivorsGestures dragged = route.TakeGestures();
  CHECK(dragged.orbit_px == kStepPx / 2);
  CHECK_FALSE(dragged.reset_yaw);
  CHECK(route.Take(
      SurvivorsEvent{.kind = SurvivorsEventKind::kMouseUp, .button = kRightMouseButton}));
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseMove, .dx_px = kStepPx}));
  CHECK(route.TakeGestures().orbit_px == 0.0F);
  // L'APPUI QUI COMPLÈTE UN DOUBLE CLIC remet au nord ; le gauche, non.
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseDown,
                                  .button = kRightMouseButton,
                                  .second_of_double = true}));
  CHECK(route.TakeGestures().reset_yaw);
  CHECK(route.Take(
      SurvivorsEvent{.kind = SurvivorsEventKind::kMouseUp, .button = kRightMouseButton}));
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseDown,
                                  .button = kLeftMouseButton,
                                  .second_of_double = true}));
  CHECK_FALSE(route.TakeGestures().reset_yaw);
  CHECK(
      route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseUp, .button = kLeftMouseButton}));
  // Sur une fenêtre, ni remise ni rotation.
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseDown,
                                  .button = kRightMouseButton,
                                  .second_of_double = true,
                                  .over_interface = true}));
  CHECK(route.Take(SurvivorsEvent{.kind = SurvivorsEventKind::kMouseMove, .dx_px = kStepPx}));
  const SurvivorsGestures over = route.TakeGestures();
  CHECK_FALSE(over.reset_yaw);
  CHECK(over.orbit_px == 0.0F);
}
