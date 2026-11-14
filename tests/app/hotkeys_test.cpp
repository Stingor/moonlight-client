// La table des raccourcis, et son fichier : cent dix-neuf lignes, dont
// quatre-vingt-seize cases de barre et dix macros de chat.
//
// ⚠ CE QUE CES ESSAIS ATTRAPENT EST MUET EN JEU. Une migration manquée rend les
// touches d'origine, une case qu'on ne peut pas délier revient au démarrage
// suivant, une touche volée sans le dire disparaît de la ligne d'à côté : dans
// les trois cas, le joueur croit s'être trompé.

#include "input/hotkeys.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <SDL3/SDL_keycode.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/shortcut_bar.h"

#include "input/hotkey_file.h"
#include "support/temp_directory.h"

namespace {

using moonclient::ActionRef;
using moonclient::AllBindableActions;
using moonclient::Binding;
using moonclient::GameAction;
using moonclient::HotkeyFilePath;
using moonclient::Hotkeys;
using moonclient::kBindableCount;
using moonclient::kNamedActionCount;
using moonclient::LabelOfBinding;
using moonclient::LoadHotkeys;
using moonclient::NameOfAction;
using moonclient::SavedKeyOfAction;
using moonclient::SaveHotkeys;
using ro::sim::kShortcutSlotCount;
using ro::sim::ShortcutIndex;

[[nodiscard]] Binding Key(std::uint32_t key, bool ctrl = false, bool shift = false,
                          bool alt = false) {
  return Binding{.key = key, .ctrl = ctrl, .shift = shift, .alt = alt};
}

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

}  // namespace

TEST_CASE("Raccourcis : la table porte les actions nommées puis les cases", "[app][raccourcis]") {
  const auto actions = AllBindableActions();
  REQUIRE(actions.size() == kBindableCount);
  CHECK(actions.size() == kNamedActionCount + kShortcutSlotCount + moonclient::kChatMacroSlots +
                              moonclient::kFlagEmoteSlots);

  // Une action nommée se convertit sans le dire, et se relit telle quelle.
  const ActionRef named = GameAction::kOpenInventory;
  CHECK_FALSE(named.is_shortcut());
  CHECK(named.named() == GameAction::kOpenInventory);

  // Une case connaît son index, et le rang dans la table le retrouve.
  const ActionRef slot = ActionRef::OfShortcut(ShortcutIndex(3, 7));
  CHECK(slot.is_shortcut());
  CHECK_FALSE(slot.named().has_value());
  CHECK(slot.shortcut_index() == ShortcutIndex(3, 7));
  CHECK(actions[slot.rank()] == slot);
}

TEST_CASE("Raccourcis : une macro n'est NI une action nommée NI une case", "[app][raccourcis]") {
  // 🔴 LE PIÈGE QUE CETTE TROISIÈME FAMILLE POSE. `is_shortcut()` valait
  // « tout ce qui n'est pas nommé » : une macro y serait tombée, et le clavier
  // l'aurait envoyée lancer une case de barre — avec un index hors compte, donc
  // une touche muette que rien n'expliquerait. Les trois familles doivent se
  // distinguer, et chacune doit REFUSER les deux autres.
  const ActionRef macro = ActionRef::OfMacro(3);
  CHECK(macro.is_macro());
  CHECK_FALSE(macro.is_shortcut());
  CHECK_FALSE(macro.named().has_value());
  CHECK(macro.macro_slot() == 3);

  // ⚠ ET LA DERNIÈRE CASE DE BARRE N'EST PAS UNE MACRO : c'est la frontière qui
  // se déplacerait si l'on intercalait un rang.
  const ActionRef last_slot = ActionRef::OfShortcut(kShortcutSlotCount - 1);
  CHECK(last_slot.is_shortcut());
  CHECK_FALSE(last_slot.is_macro());

  const ActionRef named = GameAction::kOpenChatMacros;
  CHECK_FALSE(named.is_macro());
  CHECK_FALSE(named.is_shortcut());

  // La table les porte toutes, et le rang les retrouve.
  const auto actions = AllBindableActions();
  for (std::size_t slot = 0; slot < moonclient::kChatMacroSlots; ++slot) {
    const ActionRef one = ActionRef::OfMacro(slot);
    REQUIRE(one.rank() < actions.size());
    CHECK(actions[one.rank()] == one);
  }
}

TEST_CASE("Raccourcis : Ctrl+1 à Ctrl+9 sur les neuf drapeaux", "[app][raccourcis]") {
  // Le client du joueur envoie un drapeau sur Ctrl et un chiffre. Une famille
  // à part, après les macros : ni macro ni case de barre.
  const Hotkeys bindings;
  const ActionRef first = ActionRef::OfFlag(0);
  CHECK(first.is_flag());
  CHECK_FALSE(first.is_macro());
  CHECK_FALSE(first.is_shortcut());
  CHECK_FALSE(ActionRef::OfMacro(moonclient::kChatMacroSlots - 1).is_flag());
  CHECK(bindings.Of(first).key == SDLK_1);
  CHECK(bindings.Of(first).ctrl);
  CHECK_FALSE(bindings.Of(first).alt);
  CHECK(bindings.Of(ActionRef::OfFlag(moonclient::kFlagEmoteSlots - 1)).key == SDLK_9);
  CHECK(moonclient::SavedKeyOfAction(first) == "flag_1");
  const auto actions = AllBindableActions();
  REQUIRE(ActionRef::OfFlag(moonclient::kFlagEmoteSlots - 1).rank() == actions.size() - 1);
}

TEST_CASE("Raccourcis : Alt+1 à Alt+0 sur les dix macros", "[app][raccourcis]") {
  const Hotkeys bindings;
  // ⚠ LE ZÉRO EN DIXIÈME POSITION : c'est la rangée du clavier qu'on suit, pas
  // la valeur du chiffre, et c'est l'ordre du client.
  CHECK(bindings.Of(ActionRef::OfMacro(0)).key == SDLK_1);
  CHECK(bindings.Of(ActionRef::OfMacro(0)).alt);
  CHECK(bindings.Of(ActionRef::OfMacro(8)).key == SDLK_9);
  CHECK(bindings.Of(ActionRef::OfMacro(9)).key == SDLK_0);
  CHECK(bindings.Of(ActionRef::OfMacro(9)).alt);
  CHECK(bindings.Of(GameAction::kOpenChatMacros).key == SDLK_M);
  CHECK(bindings.Of(GameAction::kOpenChatMacros).alt);

  // 🔴 ET AUCUN DÉFAUT N'EN CACHE UN AUTRE. Onze touches s'ajoutent d'un coup :
  // une seule qui doublerait un raccourci existant rendrait l'autre muet au
  // premier lancement, sans que rien ne le dise.
  std::map<std::string, std::string> taken;
  for (const ActionRef action : AllBindableActions()) {
    const Binding bound = bindings.Of(action);
    if (!bound.bound()) {
      continue;
    }
    const std::string label = LabelOfBinding(bound);
    const auto [where, fresh] = taken.emplace(label, NameOfAction(action));
    INFO("« " << label << " » est à la fois « " << where->second << " » et « "
              << NameOfAction(action) << " »");
    CHECK(fresh);
  }
}

TEST_CASE("Raccourcis : chaque action a une clé de fichier À ELLE", "[app][raccourcis]") {
  // 🔴 DEUX ACTIONS SOUS LA MÊME CLÉ, ET L'UNE MANGE L'AUTRE EN SILENCE : le
  // fichier est une table, la seconde écriture écrase la première, et la
  // relecture donne la même touche aux deux. Les macros arrivent avec dix clés
  // neuves — c'est le moment où cela pouvait arriver.
  std::set<std::string> keys;
  for (const ActionRef action : AllBindableActions()) {
    const std::string key = SavedKeyOfAction(action);
    INFO("clé « " << key << " » pour « " << NameOfAction(action) << " »");
    CHECK(keys.insert(key).second);
  }
}

TEST_CASE("Raccourcis : trois barres ont leur rangée du clavier, les autres rien",
          "[app][raccourcis]") {
  const Hotkeys bindings;
  const auto key_of = [&bindings](std::size_t bar, std::size_t rank) {
    return bindings.Of(ActionRef::OfShortcut(ShortcutIndex(bar, rank)));
  };
  // F1 à F12.
  CHECK(key_of(0, 0).key == SDLK_F1);
  CHECK(key_of(0, 11).key == SDLK_F12);
  // La rangée des chiffres d'un AZERTY, jusqu'au « = ».
  CHECK(key_of(1, 0).key == SDLK_1);
  CHECK(key_of(1, 9).key == SDLK_0);
  CHECK(key_of(1, 10).key == SDLK_RIGHTPAREN);
  CHECK(key_of(1, 11).key == SDLK_EQUALS);
  // La rangée haute d'un AZERTY, de A à P : dix cases.
  CHECK(key_of(2, 0).key == SDLK_A);
  CHECK(key_of(2, 9).key == SDLK_P);
  CHECK_FALSE(key_of(2, 10).bound());
  CHECK_FALSE(key_of(2, 0).alt);

  // 🔴 ET LES CINQ AUTRES BARRES N'EN ONT AUCUN : il ne reste plus de rangée du
  // clavier qui ne serve déjà.
  for (std::size_t index = ShortcutIndex(3, 0); index < kShortcutSlotCount; ++index) {
    CHECK_FALSE(bindings.Of(ActionRef::OfShortcut(index)).bound());
  }
}

TEST_CASE("Raccourcis : une touche trouve sa case", "[app][raccourcis]") {
  Hotkeys bindings;
  const ActionRef far = ActionRef::OfShortcut(ShortcutIndex(5, 2));
  static_cast<void>(bindings.Bind(far, Key(SDLK_A, /*ctrl=*/true)));

  CHECK(bindings.ActionFor(SDLK_A, true, false, false) == far);
  // ⚠ LES MODIFICATEURS COMPTENT EXACTEMENT : sans Ctrl, ce n'est pas le même
  // raccourci — A seul reste à la troisième barre.
  CHECK(bindings.ActionFor(SDLK_A, false, false, false) ==
        ActionRef::OfShortcut(ShortcutIndex(2, 0)));
  CHECK(bindings.ActionFor(SDLK_F1, false, false, false) ==
        ActionRef::OfShortcut(ShortcutIndex(0, 0)));
}

TEST_CASE("Raccourcis : Entrée du pavé vaut Entrée, sauf liaison à elle", "[app][raccourcis]") {
  Hotkeys bindings;
  CHECK(bindings.ActionFor(SDLK_KP_ENTER, false, false, false) == ActionRef(GameAction::kOpenChat));

  const ActionRef own = ActionRef::OfShortcut(ShortcutIndex(5, 2));
  static_cast<void>(bindings.Bind(own, Key(SDLK_KP_ENTER)));
  CHECK(bindings.ActionFor(SDLK_KP_ENTER, false, false, false) == own);
  CHECK(bindings.ActionFor(SDLK_RETURN, false, false, false) == ActionRef(GameAction::kOpenChat));
}

TEST_CASE("Raccourcis : poser une touche prise la VOLE, et nomme la victime", "[app][raccourcis]") {
  // 🔴 NI REFUSER, NI VOLER EN SILENCE. Refuser laisserait la touche à la
  // première action alors que le joueur vient de la donner à une autre ; et
  // voler sans le dire, sur cent huit lignes, fait disparaître un raccourci de
  // la ligne d'à côté sans que rien ne l'apprenne.
  Hotkeys bindings;
  const ActionRef stolen_from = ActionRef::OfShortcut(ShortcutIndex(0, 0));  // F1
  const ActionRef thief = ActionRef::OfShortcut(ShortcutIndex(2, 4));

  const std::optional<ActionRef> robbed = bindings.Bind(thief, Key(SDLK_F1));
  REQUIRE(robbed.has_value());
  CHECK(*robbed == stolen_from);
  CHECK_FALSE(bindings.Of(stolen_from).bound());
  CHECK(bindings.Of(thief).key == SDLK_F1);
}

TEST_CASE("Raccourcis : le saut sur Espace laisse la montée du vol", "[app][raccourcis]") {
  // 🔴 SANS QUOI ESPACE NE MONTE PLUS EN VOL : le saut la prenait, et la
  // montée, vidée, s'enregistrait vide dans le profil.
  Hotkeys bindings;
  REQUIRE(bindings.Of(GameAction::kFlyUp).key == SDLK_SPACE);
  CHECK_FALSE(bindings.Bind(GameAction::kJump, Key(SDLK_SPACE)).has_value());
  CHECK(bindings.Of(GameAction::kFlyUp).key == SDLK_SPACE);
  CHECK(bindings.Of(GameAction::kJump).key == SDLK_SPACE);
  // La route ordinaire voit le saut : la montée n'existe qu'en vol.
  const std::optional<ActionRef> routed = bindings.ActionFor(SDLK_SPACE, false, false, false);
  REQUIRE(routed.has_value());
  CHECK(*routed == ActionRef(GameAction::kJump));
}

TEST_CASE("Raccourcis : la marche dispute encore sa touche à la montée", "[app][raccourcis]") {
  // ⚠ EN VOL, LA VUE LIT LES DEUX : une même touche n'y ferait qu'une chose.
  Hotkeys bindings;
  const std::optional<ActionRef> robbed = bindings.Bind(GameAction::kFlyUp, Key(SDLK_UP));
  REQUIRE(robbed.has_value());
  CHECK(*robbed == ActionRef(GameAction::kStepForward));
}

TEST_CASE("Raccourcis : se reposer sa propre touche ne dépossède personne", "[app][raccourcis]") {
  // Sans ce test, la fenêtre annoncerait au joueur qu'il vient de se voler à
  // lui-même — et la touche serait effacée avant d'être reposée.
  Hotkeys bindings;
  const ActionRef first = ActionRef::OfShortcut(ShortcutIndex(0, 0));
  CHECK_FALSE(bindings.Bind(first, Key(SDLK_F1)).has_value());
  CHECK(bindings.Of(first).key == SDLK_F1);
}

TEST_CASE("Raccourcis : retirer une touche ne vole rien", "[app][raccourcis]") {
  Hotkeys bindings;
  const ActionRef first = ActionRef::OfShortcut(ShortcutIndex(0, 0));
  CHECK_FALSE(bindings.Bind(first, Binding{}).has_value());
  CHECK_FALSE(bindings.Of(first).bound());
}

TEST_CASE("Fichier de raccourcis : ce qui s'écrit se relit", "[app][raccourcis]") {
  const ro::test_support::TempDirectory folder;
  Hotkeys written;
  const ActionRef far = ActionRef::OfShortcut(ShortcutIndex(7, 11));
  static_cast<void>(written.Bind(far, Key(SDLK_B, /*ctrl=*/false, /*shift=*/true, /*alt=*/true)));
  REQUIRE(SaveHotkeys(folder.path(), written));

  Hotkeys read;
  LoadHotkeys(folder.path(), read);
  CHECK(read.Of(far).key == SDLK_B);
  CHECK(read.Of(far).shift);
  CHECK(read.Of(far).alt);
  CHECK_FALSE(read.Of(far).ctrl);
  CHECK(read.Of(GameAction::kOpenInventory).key == SDLK_E);
}

TEST_CASE("Fichier de raccourcis : un caractère se relit tel qu'il s'est écrit",
          "[app][raccourcis]") {
  // 🔴 LA RANGÉE DES CHIFFRES D'UN AZERTY. Relu par la disposition du clavier,
  // « 4 » devenait l'apostrophe de sa touche, et « ) » le 0 d'un QWERTY : la
  // case perdait sa touche au démarrage suivant, sans un mot.
  using moonclient::FileNameOfKey;
  using moonclient::KeyOfFileName;
  const std::vector<std::uint32_t> keys = {
      SDLK_1,          SDLK_4, SDLK_7,       SDLK_0,       SDLK_RIGHTPAREN, SDLK_EQUALS,
      SDLK_APOSTROPHE, SDLK_A, 0xE9 /* é */, 0xE0 /* à */, 0xE8 /* è */,    0xB2 /* ² */};
  const ro::test_support::TempDirectory folder;
  Hotkeys written;
  for (std::size_t slot = 0; slot < keys.size(); ++slot) {
    static_cast<void>(written.Bind(ActionRef::OfShortcut(ShortcutIndex(1, slot)), Key(keys[slot])));
  }
  REQUIRE(SaveHotkeys(folder.path(), written));

  Hotkeys read;
  LoadHotkeys(folder.path(), read);
  for (std::size_t slot = 0; slot < keys.size(); ++slot) {
    CAPTURE(slot, FileNameOfKey(keys[slot]));
    CHECK(read.Of(ActionRef::OfShortcut(ShortcutIndex(1, slot))).key == keys[slot]);
  }
  CHECK(KeyOfFileName(")") == SDLK_RIGHTPAREN);
  CHECK(KeyOfFileName("A") == SDLK_A);
  CHECK(FileNameOfKey(SDLK_A) == "A");
  CHECK(KeyOfFileName("Space") == SDLK_SPACE);
}

TEST_CASE("Fichier de raccourcis : une case sans touche ni défaut ne s'écrit pas",
          "[app][raccourcis]") {
  // ⚠ QUATRE-VINGT-CINQ LIGNES VIDES noieraient un fichier fait pour se relire.
  const ro::test_support::TempDirectory folder;
  const Hotkeys bindings;
  REQUIRE(SaveHotkeys(folder.path(), bindings));

  const std::string text = Contents(HotkeyFilePath(folder.path()));
  CHECK(text.find("bar1_slot1:") != std::string::npos);
  CHECK(text.find("bar3_slot10:") != std::string::npos);
  CHECK(text.find("bar3_slot11:") == std::string::npos);
  CHECK(text.find("bar4_slot1:") == std::string::npos);
}

TEST_CASE("Fichier de raccourcis : une case à défaut peut se DÉLIER", "[app][raccourcis]") {
  // 🔴 LE PIÈGE SYMÉTRIQUE DU PRÉCÉDENT. Si une case vide ne s'écrivait jamais,
  // retirer F1 de la première ne survivrait pas au redémarrage : l'entrée
  // absente vaut « garde ton défaut », et la touche reviendrait sans un mot.
  const ro::test_support::TempDirectory folder;
  Hotkeys written;
  const ActionRef first = ActionRef::OfShortcut(ShortcutIndex(0, 0));
  static_cast<void>(written.Bind(first, Binding{}));
  REQUIRE(SaveHotkeys(folder.path(), written));

  Hotkeys read;
  LoadHotkeys(folder.path(), read);
  CHECK_FALSE(read.Of(first).bound());
}

TEST_CASE("Raccourcis : le nom d'une case porte sa barre et son rang", "[app][raccourcis]") {
  // ⚠ LA BARRE EST DANS LE NOM, pas seulement dans le groupe : c'est le nom que
  // la RECHERCHE de la fenêtre fouille, et taper « barre 6 » doit rendre ses
  // douze cases.
  const ActionRef slot = ActionRef::OfShortcut(ShortcutIndex(5, 6));
  const std::string name = moonclient::NameOfAction(slot);
  CHECK(name.find('6') != std::string::npos);
  CHECK(name.find('7') != std::string::npos);
  CHECK(moonclient::GroupOfAction(slot).find('6') != std::string::npos);
  // Une action nommée n'a pas de groupe : elle ne se range sous aucun intertitre.
  CHECK(moonclient::GroupOfAction(GameAction::kOpenInventory).empty());
  CHECK(moonclient::SavedKeyOfAction(slot) == "bar6_slot7");
}

TEST_CASE("Raccourcis : les couleurs ont Alt+P, et cela se reconfigure", "[app][raccourcis]") {
  // ⚠ ALT+P N'EST QU'UN DÉFAUT : l'action
  // est une ligne de la table comme les autres, donc elle paraît dans la
  // fenêtre de configuration et se réécrit dans le fichier. Une touche gravée
  // dans la boucle n'y paraîtrait pas, et le joueur la chercherait.
  Hotkeys bindings;
  const Binding factory = bindings.Of(GameAction::kOpenStyleEditor);
  CHECK(factory.key == SDLK_P);
  CHECK(factory.alt);
  CHECK_FALSE(factory.ctrl);
  CHECK_FALSE(factory.shift);
  // Elle se retrouve par sa touche, comme n'importe quelle autre.
  const std::optional<ActionRef> found = bindings.ActionFor(SDLK_P, false, false, true);
  REQUIRE(found.has_value());
  CHECK(found->named() == GameAction::kOpenStyleEditor);

  // 🔴 ET ELLE EST DANS LA LISTE QUE LA FENÊTRE PARCOURT : c'est cela, « être
  // configurable ». Une action absente de `AllBindableActions` garderait sa
  // touche d'usine pour toujours, sans une ligne pour le dire.
  const std::span<const ActionRef> all = AllBindableActions();
  CHECK(std::ranges::any_of(
      all, [](const ActionRef action) { return action.named() == GameAction::kOpenStyleEditor; }));

  // Reliée ailleurs, elle suit — et rend sa place. ⚠ SUR UNE TOUCHE LIBRE :
  // F8 est la huitième case de la première barre, et la poser là serait
  // éprouver le VOL de touche, qui a son propre essai.
  CHECK_FALSE(bindings.Bind(GameAction::kOpenStyleEditor, Key(SDLK_K)).has_value());
  CHECK(bindings.Of(GameAction::kOpenStyleEditor).key == SDLK_K);
  CHECK_FALSE(bindings.ActionFor(SDLK_P, false, false, true).has_value());
}

TEST_CASE("Raccourcis : l'onglet de guilde a Alt+G, et cela se reconfigure", "[app][raccourcis]") {
  // ⚠ ALT+G EST LA TOUCHE DU JEU pour sa fenêtre de guilde : un joueur l'a dans
  // les doigts avant de connaître ce client.
  Hotkeys bindings;
  const Binding factory = bindings.Of(GameAction::kOpenCharacterGuild);
  CHECK(factory == Key(SDLK_G, /*ctrl=*/false, /*shift=*/false, /*alt=*/true));
  const std::optional<ActionRef> found = bindings.ActionFor(SDLK_G, false, false, true);
  REQUIRE(found.has_value());
  CHECK(found->named() == GameAction::kOpenCharacterGuild);
  CHECK(SavedKeyOfAction(GameAction::kOpenCharacterGuild) == "sheet_guild");

  // Reliée ailleurs, elle se relit du fichier à sa nouvelle place.
  CHECK_FALSE(
      bindings.Bind(GameAction::kOpenCharacterGuild, Key(SDLK_K, false, false, true)).has_value());
  const ro::test_support::TempDirectory folder;
  REQUIRE(SaveHotkeys(folder.path(), bindings));
  Hotkeys reread;
  LoadHotkeys(folder.path(), reread);
  CHECK(reread.Of(GameAction::kOpenCharacterGuild) == Key(SDLK_K, false, false, true));
  CHECK_FALSE(reread.ActionFor(SDLK_G, false, false, true).has_value());
}

TEST_CASE("Raccourcis : les gestes de la caméra sont ceux du client", "[app][raccourcis]") {
  // Ce sont ceux du glisser de caméra et de la molette du client. Un joueur qui
  // n'a rien changé doit retrouver ses gestes.
  using moonclient::KeyOfGesture;
  using moonclient::MouseGesture;
  const Hotkeys bindings;
  const auto gesture_of = [&bindings](ActionRef action) { return bindings.Of(action); };
  CHECK(gesture_of(GameAction::kCameraOrbit) == Key(KeyOfGesture(MouseGesture::kRightDrag)));
  CHECK(gesture_of(GameAction::kCameraTilt) ==
        Key(KeyOfGesture(MouseGesture::kRightDrag), false, /*shift=*/true));
  CHECK(gesture_of(GameAction::kCameraOrbitZoom) ==
        Key(KeyOfGesture(MouseGesture::kRightDrag), /*ctrl=*/true));
  CHECK(gesture_of(GameAction::kCameraZoomWheel) == Key(KeyOfGesture(MouseGesture::kWheel)));
  CHECK(gesture_of(GameAction::kCameraResetZoom) ==
        Key(KeyOfGesture(MouseGesture::kRightDoubleClick), /*ctrl=*/true));
  // R est à la troisième barre : le recentrage n'a pas de touche d'usine.
  CHECK_FALSE(gesture_of(GameAction::kCameraReset).bound());
  CHECK(gesture_of(GameAction::kFrameWholeMap) == Key(SDLK_R, false, /*shift=*/true));
}

TEST_CASE("Raccourcis : Ctrl s'ajoute à un geste qui ne le nomme pas", "[app][raccourcis]") {
  // Ctrl + molette zoome comme la molette seule ; Ctrl + Maj + molette incline.
  using moonclient::MouseGesture;
  const Hotkeys bindings;
  CHECK(bindings.ActionForGesture(MouseGesture::kWheel, true, false, false) ==
        ActionRef(GameAction::kCameraZoomWheel));
  CHECK(bindings.ActionForGesture(MouseGesture::kWheel, true, true, false) ==
        ActionRef(GameAction::kCameraTiltWheel));
  // Une liaison qui NOMME Ctrl passe devant.
  CHECK(bindings.ActionForGesture(MouseGesture::kRightDrag, true, false, false) ==
        ActionRef(GameAction::kCameraOrbitZoom));
  // Alt ne s'ajoute pas : Alt + clic droit glissé ne fait rien, comme au client.
  CHECK_FALSE(bindings.ActionForGesture(MouseGesture::kRightDrag, false, false, true).has_value());
}

TEST_CASE("Raccourcis : un glisser ne se pose pas sur une touche", "[app][raccourcis]") {
  using moonclient::KeyOfGesture;
  using moonclient::MouseGesture;
  Hotkeys bindings;
  static_cast<void>(bindings.Bind(GameAction::kCameraOrbit, Key(SDLK_K)));
  CHECK(bindings.Of(GameAction::kCameraOrbit) == Key(KeyOfGesture(MouseGesture::kRightDrag)));
  // Un double-clic, lui, va à une action d'appui.
  static_cast<void>(bindings.Bind(GameAction::kOpenInventory,
                                  Key(KeyOfGesture(MouseGesture::kMiddleDoubleClick))));
  CHECK(bindings.Of(GameAction::kOpenInventory).key ==
        KeyOfGesture(MouseGesture::kMiddleDoubleClick));
}

TEST_CASE("Fichier de raccourcis : un geste s'écrit et se relit", "[app][raccourcis]") {
  using moonclient::KeyOfGesture;
  using moonclient::MouseGesture;
  const ro::test_support::TempDirectory folder;
  Hotkeys written;
  static_cast<void>(
      written.Bind(GameAction::kCameraOrbit, Key(KeyOfGesture(MouseGesture::kMiddleDrag))));
  REQUIRE(SaveHotkeys(folder.path(), written));
  CHECK(Contents(HotkeyFilePath(folder.path())).find("MouseMiddleDrag") != std::string::npos);

  Hotkeys read;
  LoadHotkeys(folder.path(), read);
  CHECK(read.Of(GameAction::kCameraOrbit).key == KeyOfGesture(MouseGesture::kMiddleDrag));
  // Le libellé se lit, il ne montre pas un code.
  CHECK(LabelOfBinding(read.Of(GameAction::kCameraOrbit)).find("0x") == std::string::npos);
}

TEST_CASE("Raccourcis : le libellé d'une touche parle la langue de l'interface, le fichier non",
          "[app][raccourcis]") {
  const ro::test_support::TempDirectory folder;
  Hotkeys written;
  static_cast<void>(written.Bind(GameAction::kOpenChat, Key(SDLK_RETURN)));
  REQUIRE(SaveHotkeys(folder.path(), written));
  CHECK(Contents(HotkeyFilePath(folder.path())).find("Return") != std::string::npos);

  CHECK(LabelOfBinding(Key(SDLK_RETURN)) == "Entrée");
  CHECK(LabelOfBinding(Key(SDLK_PRINTSCREEN)) == "Impr Écran");
  CHECK(LabelOfBinding(Key(SDLK_KP_1)) == "Pavé num. 1");
  CHECK(LabelOfBinding(Key(SDLK_F5)) == "F5");
}

TEST_CASE("Raccourcis : les ordres aux compagnons sont des clics sur le monde",
          "[app][raccourcis]") {
  using moonclient::KeyOfGesture;
  using moonclient::MouseGesture;
  const std::uint32_t left = KeyOfGesture(MouseGesture::kLeftClick);
  const std::uint32_t right = KeyOfGesture(MouseGesture::kRightClick);
  Hotkeys bindings;
  // Les gestes du jeu : Alt + clic droit à l'homoncule, Alt + clic gauche au
  // mercenaire.
  CHECK(bindings.Of(GameAction::kHomunculusOrder) == Key(right, false, false, /*alt=*/true));
  CHECK(bindings.Of(GameAction::kMercenaryOrder) == Key(left, false, false, /*alt=*/true));

  // 🔴 REFUSÉS : un clic nu, Maj (la file), Ctrl + clic gauche (l'attaque
  // continue), une touche du clavier. La liaison d'avant reste.
  CHECK_FALSE(bindings.Bind(GameAction::kHomunculusOrder, Key(right)).has_value());
  CHECK_FALSE(bindings.Bind(GameAction::kHomunculusOrder, Key(right, false, /*shift=*/true, true))
                  .has_value());
  CHECK_FALSE(bindings.Bind(GameAction::kHomunculusOrder, Key(left, /*ctrl=*/true)).has_value());
  CHECK_FALSE(
      bindings.Bind(GameAction::kHomunculusOrder, Key(SDLK_K, false, false, true)).has_value());
  CHECK(bindings.Of(GameAction::kHomunculusOrder) == Key(right, false, false, true));
  // Et un clic ne va pas à une action d'appui.
  CHECK_FALSE(bindings.Bind(GameAction::kOpenInventory, Key(left, false, false, true)).has_value());

  // ACCEPTÉ : Ctrl + clic droit.
  static_cast<void>(bindings.Bind(GameAction::kHomunculusOrder, Key(right, /*ctrl=*/true)));
  CHECK(bindings.Of(GameAction::kHomunculusOrder) == Key(right, true));

  // UN GESTE NE SERT QU'UN COMPAGNON : le prendre le retire à l'autre.
  const std::optional<ActionRef> robbed =
      bindings.Bind(GameAction::kMercenaryOrder, Key(right, true));
  CHECK(robbed == ActionRef(GameAction::kHomunculusOrder));
  CHECK_FALSE(bindings.Of(GameAction::kHomunculusOrder).bound());

  // Et il survit au fichier.
  const ro::test_support::TempDirectory folder;
  REQUIRE(SaveHotkeys(folder.path(), bindings));
  CHECK(Contents(HotkeyFilePath(folder.path())).find("MouseRightClick") != std::string::npos);
  Hotkeys read;
  LoadHotkeys(folder.path(), read);
  CHECK(read.Of(GameAction::kMercenaryOrder) == Key(right, true));
  CHECK(read.ActionFor(right, true, false, false) == ActionRef(GameAction::kMercenaryOrder));
}

TEST_CASE("la marche à la première personne se tient au clavier, flèches par défaut", "[hotkeys]") {
  const Hotkeys bindings;
  CHECK(bindings.Of(GameAction::kStepForward) == Binding{.key = SDLK_UP});
  CHECK(bindings.Of(GameAction::kStepBack) == Binding{.key = SDLK_DOWN});
  CHECK(bindings.Of(GameAction::kStepLeft) == Binding{.key = SDLK_LEFT});
  CHECK(bindings.Of(GameAction::kStepRight) == Binding{.key = SDLK_RIGHT});
  CHECK(bindings.Of(GameAction::kFlyUp) == Binding{.key = SDLK_SPACE});
  CHECK(bindings.Of(GameAction::kFlyDown) == Binding{.key = SDLK_LCTRL});
  // LE SAUT ET LE VOL LIBRE N'ONT PAS DE TOUCHE D'USINE : le joueur leur en
  // donne une.
  CHECK_FALSE(bindings.Of(GameAction::kJump).bound());
  CHECK_FALSE(bindings.Of(GameAction::kFreeFlight).bound());

  CHECK(moonclient::InputOfAction(GameAction::kStepForward) == moonclient::ActionInput::kKeyHold);
  CHECK(moonclient::BindingFits(GameAction::kStepForward, Binding{.key = SDLK_Z}));
  CHECK_FALSE(moonclient::BindingFits(
      GameAction::kStepForward,
      Binding{.key = moonclient::KeyOfGesture(moonclient::MouseGesture::kMiddleHold)}));
}
