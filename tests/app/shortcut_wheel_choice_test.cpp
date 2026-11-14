// La roue d'utilisation, vue de la partie : ce qu'un appui, un glissé et un
// relâcher en font, le geste qui l'ouvre, et la barre retenue d'une partie à
// l'autre.
//
// 🔴 LES ÉVÉNEMENTS SONT FABRIQUÉS, et c'est ce qui rend l'essai possible : la
// roue ne lit que ce que la fenêtre lui donne, sans fenêtre ni partie.

#include <cstddef>
#include <filesystem>
#include <optional>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/shortcut_bar.h"

#include "character/shortcut_wheel.h"
#include "game/settings_file.h"
#include "input/hotkeys.h"
#include "support/temp_directory.h"

using moonclient::ActionInput;
using moonclient::ActionRef;
using moonclient::Binding;
using moonclient::GameAction;
using moonclient::Hotkeys;
using moonclient::KeyOfGesture;
using moonclient::MouseGesture;
using moonclient::ShortcutWheel;
using moonclient::WheelGate;
using ro::ui::ShortcutSlotIndex;

namespace {

constexpr float kCenterX = 500.0F;
constexpr float kCenterY = 400.0F;
// Assez loin du centre pour sortir de la zone morte.
constexpr float kFar = 90.0F;
// La troisième barre, comme le joueur la compte.
constexpr int kThirdBar = 3;
constexpr std::size_t kThirdBarIndex = 2;
// La case à droite du centre : la quatrième, à trois heures.
constexpr std::size_t kRightRank = 3;
constexpr WheelGate kOverWorld{};

[[nodiscard]] SDL_Event Button(SDL_EventType type, std::uint8_t button, float x, float y) {
  SDL_Event event{};
  event.type = type;
  event.button.button = button;
  event.button.x = x;
  event.button.y = y;
  return event;
}

[[nodiscard]] SDL_Event Motion(float x, float y) {
  SDL_Event event{};
  event.type = SDL_EVENT_MOUSE_MOTION;
  event.motion.x = x;
  event.motion.y = y;
  return event;
}

[[nodiscard]] SDL_Event Key(SDL_EventType type, SDL_Keycode key, bool repeat = false) {
  SDL_Event event{};
  event.type = type;
  event.key.key = key;
  event.key.repeat = repeat;
  return event;
}

[[nodiscard]] ro::ui::ShortcutBarsLayout WheelOn(int bar) {
  ro::ui::ShortcutBarsLayout layout;
  layout.wheel_bar = bar;
  return layout;
}

// Ouvre la roue au bouton du milieu, sur le monde, et rend vrai si elle l'a pris.
bool PressMiddle(ShortcutWheel& wheel, const Hotkeys& bindings, WheelGate gate = kOverWorld) {
  return wheel.TakeEvent(Button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_MIDDLE, kCenterX, kCenterY),
                         bindings, gate, SDL_KMOD_NONE, nullptr);
}

}  // namespace

TEST_CASE("Roue : le clic milieu maintenu l'ouvre par défaut", "[app][roue]") {
  const Hotkeys bindings;
  CHECK(bindings.Of(GameAction::kShortcutWheel) ==
        Binding{.key = KeyOfGesture(MouseGesture::kMiddleHold)});
  CHECK(moonclient::InputOfAction(GameAction::kShortcutWheel) == ActionInput::kHold);
  // Le geste se garde par son nom, et se relit.
  CHECK(moonclient::FileNameOfKey(KeyOfGesture(MouseGesture::kMiddleHold)) == "MouseMiddleHold");
  CHECK(moonclient::KeyOfFileName("MouseMiddleHold") == KeyOfGesture(MouseGesture::kMiddleHold));
}

TEST_CASE("Roue : une touche ou le bouton du milieu, rien d'autre", "[app][roue]") {
  const ActionRef wheel = GameAction::kShortcutWheel;
  CHECK(moonclient::BindingFits(wheel, Binding{.key = SDLK_G}));
  CHECK(moonclient::BindingFits(wheel, Binding{.key = KeyOfGesture(MouseGesture::kMiddleHold)}));
  CHECK_FALSE(
      moonclient::BindingFits(wheel, Binding{.key = KeyOfGesture(MouseGesture::kRightDrag)}));
  CHECK_FALSE(
      moonclient::BindingFits(wheel, Binding{.key = KeyOfGesture(MouseGesture::kLeftClick)}));
  // ⚠ ET LE MAINTIEN N'EST À PERSONNE D'AUTRE : une action de frappe le refuse.
  CHECK_FALSE(moonclient::BindingFits(GameAction::kSitOrStand,
                                      Binding{.key = KeyOfGesture(MouseGesture::kMiddleHold)}));
}

TEST_CASE("Roue : sans barre choisie, le bouton du milieu ne fait rien de neuf", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(ro::ui::kNoWheelBar)));
  CHECK_FALSE(PressMiddle(wheel, bindings));
  CHECK_FALSE(wheel.open());
}

TEST_CASE("Roue : relâcher utilise la case visée, comme un clic sur la barre", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  REQUIRE(PressMiddle(wheel, bindings));
  CHECK(wheel.open());
  const ro::ui::ShortcutWheelView view = wheel.Compose(WheelOn(kThirdBar));
  CHECK(view.open);
  CHECK(view.bar == kThirdBarIndex);
  CHECK(view.center.x == kCenterX);

  // LE GLISSÉ EST PRIS, et le curseur du jeu suit quand même.
  glm::vec2 mouse{0.0F, 0.0F};
  CHECK(wheel.TakeEvent(Motion(kCenterX + kFar, kCenterY), bindings, kOverWorld, SDL_KMOD_NONE,
                        &mouse));
  CHECK(mouse.x == kCenterX + kFar);
  CHECK(wheel.TakeEvent(
      Button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_MIDDLE, kCenterX + kFar, kCenterY), bindings,
      kOverWorld, SDL_KMOD_NONE, nullptr));
  CHECK_FALSE(wheel.open());

  const ro::ui::ShortcutBarsAction asked = wheel.WithChosen(ro::ui::ShortcutBarsAction{});
  CHECK(asked.cast == static_cast<int>(ShortcutSlotIndex(kThirdBarIndex, kRightRank)));
  // ⚠ UNE FOIS SEULEMENT.
  CHECK(wheel.WithChosen(ro::ui::ShortcutBarsAction{}).cast < 0);
}

TEST_CASE("Roue : relâcher au centre annule", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  REQUIRE(PressMiddle(wheel, bindings));
  CHECK(wheel.TakeEvent(Button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_MIDDLE, kCenterX, kCenterY),
                        bindings, kOverWorld, SDL_KMOD_NONE, nullptr));
  CHECK(wheel.WithChosen(ro::ui::ShortcutBarsAction{}).cast < 0);
}

TEST_CASE("Roue : sur une fenêtre, le clic milieu reste à l'interface", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  CHECK_FALSE(PressMiddle(wheel, bindings, WheelGate{.mouse_over_interface = true}));
  CHECK_FALSE(PressMiddle(wheel, bindings, WheelGate{.capturing_hotkey = true}));
  CHECK_FALSE(wheel.open());
}

TEST_CASE("Roue : ouverte, elle garde les clics et la molette, pas les relâchers", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  REQUIRE(PressMiddle(wheel, bindings));
  CHECK(wheel.TakeEvent(Button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, kCenterX, kCenterY),
                        bindings, kOverWorld, SDL_KMOD_NONE, nullptr));
  SDL_Event scroll{};
  scroll.type = SDL_EVENT_MOUSE_WHEEL;
  CHECK(wheel.TakeEvent(scroll, bindings, kOverWorld, SDL_KMOD_NONE, nullptr));
  // Le relâcher gauche arrête la marche tenue : il passe.
  CHECK_FALSE(
      wheel.TakeEvent(Button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, kCenterX, kCenterY),
                      bindings, kOverWorld, SDL_KMOD_NONE, nullptr));
  CHECK(wheel.open());
}

TEST_CASE("Roue : Échap et la perte du focus annulent", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  REQUIRE(PressMiddle(wheel, bindings));
  static_cast<void>(wheel.TakeEvent(Motion(kCenterX + kFar, kCenterY), bindings, kOverWorld,
                                    SDL_KMOD_NONE, nullptr));
  CHECK(wheel.TakeEvent(Key(SDL_EVENT_KEY_DOWN, SDLK_ESCAPE), bindings, kOverWorld, SDL_KMOD_NONE,
                        nullptr));
  CHECK_FALSE(wheel.open());
  CHECK(wheel.WithChosen(ro::ui::ShortcutBarsAction{}).cast < 0);

  REQUIRE(PressMiddle(wheel, bindings));
  SDL_Event lost{};
  lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
  CHECK_FALSE(wheel.TakeEvent(lost, bindings, kOverWorld, SDL_KMOD_NONE, nullptr));
  CHECK_FALSE(wheel.open());
}

TEST_CASE("Roue : liée à une touche, elle s'ouvre à l'appui et conclut au relâcher",
          "[app][roue]") {
  ShortcutWheel wheel;
  Hotkeys bindings;
  // ⚠ G SEUL N'EST À PERSONNE : la liaison ne prend la touche à aucune autre action.
  REQUIRE_FALSE(bindings.Bind(GameAction::kShortcutWheel, Binding{.key = SDLK_G}).has_value());
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  // Le bouton du milieu n'est plus le sien.
  CHECK_FALSE(PressMiddle(wheel, bindings));

  const SDL_Event press = Key(SDL_EVENT_KEY_DOWN, SDLK_G);
  CHECK(wheel.TakeKeyDown(press.key, bindings, kOverWorld, glm::vec2{kCenterX, kCenterY}));
  CHECK(wheel.open());
  // La répétition du clavier n'est que le geste qui dure.
  CHECK(wheel.TakeEvent(Key(SDL_EVENT_KEY_DOWN, SDLK_G, /*repeat=*/true), bindings, kOverWorld,
                        SDL_KMOD_NONE, nullptr));
  static_cast<void>(wheel.TakeEvent(Motion(kCenterX, kCenterY - kFar), bindings, kOverWorld,
                                    SDL_KMOD_NONE, nullptr));
  CHECK(
      wheel.TakeEvent(Key(SDL_EVENT_KEY_UP, SDLK_G), bindings, kOverWorld, SDL_KMOD_NONE, nullptr));
  CHECK_FALSE(wheel.open());
  CHECK(wheel.WithChosen(ro::ui::ShortcutBarsAction{}).cast ==
        static_cast<int>(ShortcutSlotIndex(kThirdBarIndex, 0)));
}

TEST_CASE("Roue : un clic sur une barre dans la même image garde la main", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  REQUIRE(PressMiddle(wheel, bindings));
  static_cast<void>(wheel.TakeEvent(
      Button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_MIDDLE, kCenterX + kFar, kCenterY), bindings,
      kOverWorld, SDL_KMOD_NONE, nullptr));
  constexpr int kClickedOnBar = 5;
  CHECK(wheel.WithChosen(ro::ui::ShortcutBarsAction{.cast = kClickedOnBar}).cast == kClickedOnBar);
}

TEST_CASE("Roue : retirer la barre referme la roue sans rien lancer", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  REQUIRE(PressMiddle(wheel, bindings));
  CHECK_FALSE(wheel.Compose(WheelOn(ro::ui::kNoWheelBar)).open);
  CHECK_FALSE(wheel.open());
}

TEST_CASE("Roue : la barre choisie survit au fichier, et un hors-bornes vaut aucune",
          "[app][roue]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  {
    ro::ui::ShortcutBarsLayout layout;
    moonclient::SettingsFile file(path);
    file.Declare("bars_wheel", &layout.wheel_bar);
    file.Load();
    layout.wheel_bar = kThirdBar;
    file.Touch();
    file.Flush();
  }
  {
    ro::ui::ShortcutBarsLayout read;
    moonclient::SettingsFile again(path);
    again.Declare("bars_wheel", &read.wheel_bar);
    again.Load();
    CHECK(ro::ui::WheelBarOf(read) == kThirdBarIndex);
  }
  // UN FICHIER ÉCRIT À LA MAIN : une barre qui n'existe pas n'ouvre rien.
  folder.WriteText("reglages.yaml", "bars_wheel: 99\n");
  ro::ui::ShortcutBarsLayout wrong;
  moonclient::SettingsFile hand(path);
  hand.Declare("bars_wheel", &wrong.wheel_bar);
  hand.Load();
  CHECK_FALSE(ro::ui::WheelBarOf(wrong).has_value());
  ShortcutWheel wheel;
  const Hotkeys bindings;
  CHECK_FALSE(wheel.Compose(wrong).open);
  CHECK_FALSE(PressMiddle(wheel, bindings));
}

TEST_CASE("Roue : la veille referme celle du bouton du milieu sans rien utiliser", "[app][roue]") {
  ShortcutWheel wheel;
  const Hotkeys bindings;
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  REQUIRE(PressMiddle(wheel, bindings));
  static_cast<void>(wheel.TakeEvent(Motion(kCenterX + kFar, kCenterY), bindings, kOverWorld,
                                    SDL_KMOD_NONE, nullptr));
  // 🔴 LA VEILLE AVALE LE RELÂCHEMENT : sans cela, elle resterait ouverte.
  wheel.Cancel();
  CHECK_FALSE(wheel.open());
  // ⚠ LA CASE VISÉE NE PART PAS : le joueur n'a pas fini son geste.
  CHECK(wheel.WithChosen(ro::ui::ShortcutBarsAction{}).cast < 0);
}

TEST_CASE("Roue : la veille laisse celle qu'une touche tient", "[app][roue]") {
  ShortcutWheel wheel;
  Hotkeys bindings;
  REQUIRE_FALSE(bindings.Bind(GameAction::kShortcutWheel, Binding{.key = SDLK_G}).has_value());
  static_cast<void>(wheel.Compose(WheelOn(kThirdBar)));
  const SDL_Event press = Key(SDL_EVENT_KEY_DOWN, SDLK_G);
  REQUIRE(wheel.TakeKeyDown(press.key, bindings, kOverWorld, glm::vec2{kCenterX, kCenterY}));
  wheel.Cancel();
  CHECK(wheel.open());
}
