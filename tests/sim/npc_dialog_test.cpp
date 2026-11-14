#include "ro/sim/npc_dialog.h"

#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

using ro::sim::kNpcPageIdleMs;
using ro::sim::NpcMenuOption;
using ro::sim::NpcPrompt;
using ro::sim::SplitNpcMenu;
using ro::sim::World;

TEST_CASE("Menu de PNJ : une option VIDE ne prend PAS de rang", "[sim][npc]") {
  // ⚠ C'EST L'ESSAI QUI COMPTE LE PLUS DE CETTE FAMILLE, et il dit l'inverse de
  // ce qu'on croyait. Le serveur ne compte pas les entrées vides :
  // `menu_countoptions` (script.cpp) n'incrémente son compteur que pour une
  // entrée non vide, et `clif_parse_NpcSelectMenu` refuse tout choix supérieur à
  // ce compte — en APPELANT `clif_GM_kick`. Envoyer la position absolue d'une
  // option qui suit une entrée vide ne déclenche donc pas la mauvaise branche du
  // script : cela DÉCONNECTE le joueur, sans un mot.
  const std::vector<NpcMenuOption> options = SplitNpcMenu("Acheter::Vendre:Partir");
  REQUIRE(options.size() == 3);
  CHECK(options[0].text == "Acheter");
  CHECK(options[0].rank == 1);
  CHECK(options[1].text == "Vendre");
  CHECK(options[1].rank == 2);
  CHECK(options[2].text == "Partir");
  CHECK(options[2].rank == 3);
}

TEST_CASE("Menu de PNJ : une seule option, et le menu vide", "[sim][npc]") {
  const std::vector<NpcMenuOption> single = SplitNpcMenu("Partir");
  REQUIRE(single.size() == 1);
  CHECK(single[0].text == "Partir");
  CHECK(single[0].rank == 1);

  CHECK(SplitNpcMenu("").empty());

  // Un deux-points final laisse une entrée vide, qui ne s'affiche pas et ne
  // décale rien.
  const std::vector<NpcMenuOption> finale = SplitNpcMenu("Oui:Non:");
  REQUIRE(finale.size() == 2);
  CHECK(finale[1].rank == 2);
}

TEST_CASE("Menu de PNJ : au-delà du dernier rang, l'option se lit sans se choisir", "[sim][npc]") {
  // Un octet de choix ne porte pas plus loin que deux cent cinquante-quatre, et
  // le serveur le dit lui-même. L'option reste affichée — la retirer changerait
  // ce que le joueur LIT —, mais son rang nul interdit l'envoi.
  std::string menu;
  for (int rank = 0; rank < 260; ++rank) {
    if (rank > 0) {
      menu.push_back(':');
    }
    menu += "choix";
  }
  const std::vector<NpcMenuOption> options = SplitNpcMenu(menu);
  REQUIRE(options.size() == 260);
  CHECK(options[253].rank == 254);
  CHECK(options[254].rank == 0);
  CHECK(options.back().rank == 0);
}

TEST_CASE("Dialogue : la page ne s'affiche qu'à son TERMINATEUR", "[sim][npc]") {
  // 🔴 C'EST LA FORME MÊME DU DIALOGUE. Le serveur envoie une page en plusieurs
  // paquets ; les afficher au fil de l'eau fait grandir le texte sur toute la
  // hauteur, puis le RABOTER quand le menu arrive. La page attend donc son
  // dernier paquet et paraît d'un seul tenant.
  World world;
  CHECK_FALSE(world.npc_dialog().has_value());

  world.SayNpcDialog(110'000, "Bonjour.", 0);
  world.SayNpcDialog(110'000, "Que puis-je pour vous ?", 0);
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->lines.empty());
  CHECK(world.npc_dialog()->pending_lines.size() == 2);
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNone);

  CHECK(world.PromptNpcDialog(110'000, NpcPrompt::kNext, 0));
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNext);
  REQUIRE(world.npc_dialog()->lines.size() == 2);
  CHECK(world.npc_dialog()->pending_lines.empty());
  // La génération a bougé : l'affichage sait que sa mise en page ne vaut plus.
  CHECK(world.npc_dialog()->page == 1);
}

TEST_CASE("Dialogue : un « suivant » n'efface qu'à la page SUIVANTE", "[sim][npc]") {
  World world;
  world.SayNpcDialog(110'000, "Bonjour.", 0);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kNext, 0));

  // ⚠ LE CLIC N'EFFACE PAS : il ARME l'effacement, et la page reste lisible —
  // INERTE — le temps de l'aller-retour. L'effacer tout de suite laisserait une
  // fenêtre vide, et son pied de page sauterait sous le curseur.
  world.AdvanceNpcDialog();
  CHECK(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->awaiting_reply);
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNext);

  world.SayNpcDialog(110'000, "Voici la suite.", 0);
  // Toujours rien de changé à l'écran : la nouvelle page n'est pas complète.
  CHECK(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->pending_replaces);

  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kClose, 0));
  REQUIRE(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->lines[0] == "Voici la suite.");
  CHECK_FALSE(world.npc_dialog()->awaiting_reply);
}

TEST_CASE("Dialogue : un `mes` à plusieurs lignes en fait plusieurs", "[sim][npc]") {
  World world;
  world.SayNpcDialog(110'000, "Premiere\nDeuxieme\r\nTroisieme", 0);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kNext, 0));
  REQUIRE(world.npc_dialog()->lines.size() == 3);
  CHECK(world.npc_dialog()->lines[1] == "Deuxieme");
  // Le retour chariot ne reste pas : gardé, il s'afficherait en carré.
  CHECK(world.npc_dialog()->lines[2] == "Troisieme");
}

TEST_CASE("Dialogue : une page sans terminateur finit par s'afficher", "[sim][npc]") {
  // Le filet : un script qui écrit puis dort, ou qui s'arrête sur `end`.
  World world;
  world.SayNpcDialog(110'000, "Je reflechis...", 100);
  CHECK_FALSE(world.CommitNpcPageIfIdle(100));
  // Un transfert lent mais RÉGULIER ne le déclenche pas : chaque paquet repousse
  // l'échéance.
  CHECK_FALSE(world.CommitNpcPageIfIdle(100 + kNpcPageIdleMs - 1));
  CHECK(world.CommitNpcPageIfIdle(100 + kNpcPageIdleMs));
  REQUIRE(world.npc_dialog()->lines.size() == 1);
  // Rien en attente : le filet ne se redéclenche pas.
  CHECK_FALSE(world.CommitNpcPageIfIdle(100 + 10 * kNpcPageIdleMs));
}

TEST_CASE("Dialogue : un autre PNJ remplace le précédent", "[sim][npc]") {
  World world;
  world.SayNpcDialog(110'000, "Le premier.", 0);
  world.SayNpcDialog(110'001, "Le second.", 0);
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->npc_id == 110'001);
  REQUIRE(world.PromptNpcDialog(110'001, NpcPrompt::kNext, 0));
  REQUIRE(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->lines[0] == "Le second.");

  // Et un bouton adressé à CELUI QUI N'EST PLUS LÀ ne se pose nulle part.
  CHECK_FALSE(world.PromptNpcDialog(110'000, NpcPrompt::kClose, 0));
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNext);
}

TEST_CASE("Dialogue : un menu ouvre une fenêtre même sans une ligne", "[sim][npc]") {
  // Un script peut appeler `select` sans avoir rien dit.
  World world;
  world.OfferNpcMenu(110'000, SplitNpcMenu("Oui:Non"), 0);
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->lines.empty());
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kMenu);
  CHECK(world.npc_dialog()->options.size() == 2);

  // La liste répondue RESTE affichée, inerte, jusqu'à ce que la suite arrive :
  // la vider tout de suite escamoterait la question.
  world.AnswerNpcMenu();
  CHECK(world.npc_dialog()->options.size() == 2);
  CHECK(world.npc_dialog()->menu_answered);
  CHECK(world.npc_dialog()->awaiting_reply);

  world.SayNpcDialog(110'000, "Autre chose ?", 0);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kNext, 0));
  CHECK(world.npc_dialog()->options.empty());
  CHECK_FALSE(world.npc_dialog()->menu_answered);
  REQUIRE(world.npc_dialog()->lines.size() == 1);

  world.CloseNpcDialog();
  CHECK_FALSE(world.npc_dialog().has_value());
}

TEST_CASE("Dialogue : `clear` n'efface ni ne publie", "[sim][npc]") {
  // 🔴 `clear` est le PREMIER paquet de la réponse du serveur. Y publier ferait
  // tomber le menu de la page précédente à cet instant ; y effacer laisserait un
  // cadre vide le temps que la rafale de `mes` arrive.
  World world;
  world.SayNpcDialog(110'000, "Premiere page.", 0);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kNext, 0));

  CHECK(world.ClearNpcDialogText(110'000, 0));
  REQUIRE(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->lines[0] == "Premiere page.");

  world.SayNpcDialog(110'000, "Seconde page.", 0);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kClose, 0));
  REQUIRE(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->lines[0] == "Seconde page.");
}

TEST_CASE("Dialogue : « fermer » sans rien d'ouvert ne fait rien", "[sim][npc]") {
  // Le serveur envoie ZC_CLOSE_DIALOG à la fin de TOUT script, même quand aucune
  // fenêtre n'est ouverte : c'est le cas le plus fréquent, et ce n'est pas une
  // anomalie.
  World world;
  CHECK_FALSE(world.PromptNpcDialog(110'000, NpcPrompt::kClose, 0));
  CHECK_FALSE(world.npc_dialog().has_value());
}

TEST_CASE("Dialogue : une saisie tient jusqu'à la réponse", "[sim][npc]") {
  World world;
  world.SayNpcDialog(110'000, "Combien ?", 0);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kNumber, 0));
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNumber);

  world.SubmitNpcInput();
  CHECK(world.npc_dialog()->awaiting_reply);
  // ⚠ UNE SAISIE N'ARME PAS L'EFFACEMENT : la question doit rester lisible
  // au-dessus du champ pendant qu'on y répond.
  CHECK_FALSE(world.npc_dialog()->clear_on_next_line);
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNumber);
}

TEST_CASE("Dialogue : changer de carte ferme la fenêtre du PNJ", "[sim][npc]") {
  // Le PNJ est resté là-bas : aucun clic sur sa page n'a plus de sens, et le
  // client d'origine détruit la sienne au changement de carte.
  //
  // ⚠ CE QU'IL RESTE À DIRE AU SERVEUR N'EST PAS ICI : le script, lui, reste
  // attaché au personnage. Voir `net::NpcFarewellDue`.
  World world;
  world.OfferNpcMenu(110'000, SplitNpcMenu("Oui:Non"), 0);
  REQUIRE(world.npc_dialog()->prompt == NpcPrompt::kMenu);

  world.EnterMap("prontera", 150, 150);
  CHECK_FALSE(world.npc_dialog().has_value());
}

TEST_CASE("Dialogue : le monde se clique de nouveau dès le « fermer »", "[sim][npc]") {
  // 🔴 LE BOUTON « FERMER » RESTE À L'ÉCRAN, ET LE MONDE SE CLIQUE DÉJÀ. Le
  // client ne retient le clic que tant que le PNJ parle, attend un « suivant »
  // ou propose un menu.
  World world;
  world.SayNpcDialog(110'000, "Bonjour.", 0);
  CHECK(world.npc_dialog()->holds_world);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kClose, 0));
  CHECK_FALSE(world.npc_dialog()->holds_world);

  // Une saisie demandée après le « fermer » ne le retient pas…
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kNumber, 0));
  CHECK_FALSE(world.npc_dialog()->holds_world);
  // … un menu, si.
  world.OfferNpcMenu(110'000, SplitNpcMenu("Oui:Non"), 0);
  CHECK(world.npc_dialog()->holds_world);
  // Et une saisie qui suit ce qu'il a dit le garde retenu.
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kText, 0));
  CHECK(world.npc_dialog()->holds_world);
}
