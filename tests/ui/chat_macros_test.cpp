// LA FENÊTRE DES DIX MACROS DE CHAT.
//
// ⚠ CE QUE CET ESSAI ATTRAPE NE SE VOIT QU'EN JEU, ET MAL : une pile ImGui
// déséquilibrée — un `BeginTable` sans son `EndTable`, un `BeginDisabled` laissé
// ouvert — ne se manifeste pas sur la fenêtre fautive mais sur la SUIVANTE, ou
// à la fin de l'image. Cette fenêtre-ci en empile quatre sortes.

#include "ro/ui/chat_macros.h"

#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Aucune pièce d'habillage : la fenêtre doit tenir sur les seuls replis.
const ro::ui::SkinArt kNoArt;

// Les dix libellés de touche, tels qu'`app` les compose.
[[nodiscard]] std::array<std::string, ro::ui::kChatMacroCount> Keys() {
  std::array<std::string, ro::ui::kChatMacroCount> keys;
  for (std::size_t slot = 0; slot < keys.size(); ++slot) {
    keys[slot] = "Alt+" + std::to_string((slot + 1) % ro::ui::kChatMacroCount);
  }
  return keys;
}

// Combien de fenêtres ImGui connaît, l'image close. Une pile déséquilibrée ne
// rend pas ce compte-là.
[[nodiscard]] int WindowCount() { return ImGui::GetCurrentContext()->Windows.Size; }

}  // namespace

TEST_CASE("Macros : la fenêtre se dessine sans déséquilibrer ImGui", "[ui][macros]") {
  ro::ui::UiContext ui;
  ro::ui::ChatMacroState state;
  state.open = true;
  std::snprintf(state.rows[0].data(), state.rows[0].size(), "%s", "@load prontera");
  const std::array<std::string, ro::ui::kChatMacroCount> keys = Keys();

  ro::ui::ChatMacroAsk asked;
  for (int image = 0; image < 2; ++image) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DrawChatMacros(state, keys, "Alentour", {}, {}, kNoArt, &asked);
    // ⚠ LA TRAME SE FERME ICI, et c'est elle qui vérifie : `Render` fait
    // échouer une assertion d'ImGui si une pile est restée ouverte.
    ui.EndFrame();
  }

  CHECK(state.open);
  // Personne n'a rien touché : la fenêtre ne demande rien d'elle-même.
  CHECK(asked.send < 0);
  CHECK_FALSE(asked.changed);
  CHECK_FALSE(asked.open_hotkeys);
  // La ligne écrite est toujours là : le dessin lit les tampons, il ne les vide
  // pas.
  CHECK(std::string(state.rows[0].data()) == "@load prontera");
}

TEST_CASE("Macros : fermée, elle ne dessine rien du tout", "[ui][macros]") {
  // ⚠ RIEN, ET PAS « UNE FENÊTRE VIDE » : une fenêtre ImGui créée puis laissée
  // fermée garde sa place dans le fichier de réglages et reparaît au démarrage
  // suivant.
  ro::ui::UiContext ui;
  ro::ui::ChatMacroState state;
  const std::array<std::string, ro::ui::kChatMacroCount> keys = Keys();

  ro::ui::ChatMacroAsk asked;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const int before = WindowCount();
  ro::ui::DrawChatMacros(state, keys, "Alentour", {}, {}, kNoArt, &asked);
  const int after = WindowCount();
  ui.EndFrame();

  CHECK(before == after);
}

TEST_CASE("Macros : moins de libellés que de lignes ne retire aucune ligne", "[ui][macros]") {
  // ⚠ UNE MACRO SANS TOUCHE RESTE UNE MACRO. Le joueur peut délier Alt+7 ; sa
  // septième ligne doit rester éditable et son bouton « Essayer » l'envoyer.
  // Une boucle calée sur le compte des LIBELLÉS l'aurait fait disparaître.
  ro::ui::UiContext ui;
  ro::ui::ChatMacroState state;
  state.open = true;
  const std::array<std::string, 2> few = {"Alt+1", "Alt+2"};

  ro::ui::ChatMacroAsk asked;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::DrawChatMacros(state, few, "Alentour", {}, {}, kNoArt, &asked);
  ui.EndFrame();

  CHECK(state.open);
  CHECK(asked.send < 0);
}
