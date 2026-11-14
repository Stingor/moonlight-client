// LE BATTLE MODE : ce qu'Entrée, Échap et une lettre font de la barre de
// saisie. Ces règles sont de la logique PURE — elles s'essaient donc sans
// fenêtre, sans ImGui et sans clavier, ce qui est tout l'intérêt de les avoir
// sorties du dessin.

#include "ro/ui/chat_bar.h"

#include <catch2/catch_test_macros.hpp>

using ro::ui::ChatBarEnter;
using ro::ui::ChatBarEnterDoes;
using ro::ui::ChatBarFoldsOnEscape;
using ro::ui::ChatBarShown;
using ro::ui::ChatBarTakesTyping;

TEST_CASE("Barre : hors battle mode elle est toujours là", "[ui][chatbox]") {
  // Le dépliement ne veut rien dire hors battle mode : la barre est permanente,
  // et le lire seul la ferait disparaître sans que personne l'ait demandé.
  CHECK(ChatBarShown(/*battle_mode=*/false, /*unfolded=*/false));
  CHECK(ChatBarShown(/*battle_mode=*/false, /*unfolded=*/true));
  CHECK_FALSE(ChatBarShown(/*battle_mode=*/true, /*unfolded=*/false));
  CHECK(ChatBarShown(/*battle_mode=*/true, /*unfolded=*/true));
}

TEST_CASE("Barre : les cinq règles d'Entrée", "[ui][chatbox]") {
  // Repliée : on l'ouvre, avec le clavier.
  CHECK(ChatBarEnterDoes(/*battle_mode=*/true, /*unfolded=*/false, /*has_text=*/false) ==
        ChatBarEnter::kUnfold);
  // Dépliée et vide : la SORTIE, en une frappe.
  CHECK(ChatBarEnterDoes(/*battle_mode=*/true, /*unfolded=*/true, /*has_text=*/false) ==
        ChatBarEnter::kFold);
  // Dépliée et pleine : on envoie.
  CHECK(ChatBarEnterDoes(/*battle_mode=*/true, /*unfolded=*/true, /*has_text=*/true) ==
        ChatBarEnter::kSend);
}

TEST_CASE("Barre : hors battle mode, une phrase part d'UNE Entrée", "[ui][chatbox]") {
  // 🔴 LE DÉFAUT QUE CETTE RÈGLE CORRIGE. Exiger le dépliement hors battle mode
  // renverrait toujours vers « on l'ouvre » : une barre pleine mais sans clavier
  // demanderait alors DEUX Entrée, la première ne servant qu'à reprendre le
  // focus. C'est ce qui bloque un lien d'objet posé pendant un dialogue, où la
  // touche est confisquée puis remise à la barre.
  CHECK(ChatBarEnterDoes(/*battle_mode=*/false, /*unfolded=*/false, /*has_text=*/true) ==
        ChatBarEnter::kSend);
  // Et vide, elle ne se replie PAS : il n'y a rien à replier, le clavier lui
  // revient simplement.
  CHECK(ChatBarEnterDoes(/*battle_mode=*/false, /*unfolded=*/false, /*has_text=*/false) ==
        ChatBarEnter::kFocus);
}

TEST_CASE("Barre : Échap n'appartient à la barre qu'en battle mode", "[ui][chatbox]") {
  CHECK(ChatBarFoldsOnEscape(/*battle_mode=*/true, /*unfolded=*/true));
  // Repliée, il n'y a rien à refermer : la touche doit continuer de fermer ce
  // que le joueur regarde.
  CHECK_FALSE(ChatBarFoldsOnEscape(/*battle_mode=*/true, /*unfolded=*/false));
  // ⚠ ET HORS BATTLE MODE JAMAIS : la barre est permanente, lui donner Échap
  // confisquerait la touche pour rien.
  CHECK_FALSE(ChatBarFoldsOnEscape(/*battle_mode=*/false, /*unfolded=*/true));
}

TEST_CASE("Barre : taper n'écrit dedans que dans l'état intermédiaire", "[ui][chatbox]") {
  // Dépliée, et personne au clavier : c'est là que les lettres lui reviennent.
  CHECK(ChatBarTakesTyping(/*battle_mode=*/true, /*unfolded=*/true, /*text_elsewhere=*/false));
  // Une zone de texte écrit — la sienne, une fois focalisée, ou une autre : les
  // caractères lui arrivent par le chemin ordinaire, rien à confisquer.
  CHECK_FALSE(ChatBarTakesTyping(/*battle_mode=*/true, /*unfolded=*/true, /*text_elsewhere=*/true));
  // Repliée : la lettre appartient au jeu, c'est tout le sens du battle mode.
  CHECK_FALSE(
      ChatBarTakesTyping(/*battle_mode=*/true, /*unfolded=*/false, /*text_elsewhere=*/false));
  // 🔴 ET HORS BATTLE MODE JAMAIS. La barre y est là en permanence :
  // confisquer chaque lettre parce qu'un champ traîne à l'écran retirerait au
  // joueur toutes ses touches de déplacement, sans qu'il ait rien demandé.
  CHECK_FALSE(
      ChatBarTakesTyping(/*battle_mode=*/false, /*unfolded=*/false, /*text_elsewhere=*/false));
  CHECK_FALSE(
      ChatBarTakesTyping(/*battle_mode=*/false, /*unfolded=*/true, /*text_elsewhere=*/false));
}
