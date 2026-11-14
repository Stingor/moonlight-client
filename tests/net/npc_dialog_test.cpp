#include "ro/net/npc_dialog.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/npc_dialog.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// LE DIALOGUE D'UN PNJ, des deux côtés : les trames qui arrivent, et le contrôle
// du rang, qui est ce que cette famille a de dangereux.

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::NpcPrompt;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

// Fait dire une page ENTIÈRE : le texte, puis le paquet qui le termine. Une
// ligne seule ne s'affiche pas — voir `sim::NpcDialog::pending_lines`.
void SayAndWait(World& world, std::uint32_t npc_id, const char* line) {
  ro::proto::ZcSayDialog said;
  said.NpcID = npc_id;
  said.message = line;
  REQUIRE(ApplyFrame(FrameOf(Encode(said)), world, 0) == FrameOutcome::kApplied);
  ro::proto::ZcWaitDialog next;
  next.NpcID = npc_id;
  REQUIRE(ApplyFrame(FrameOf(Encode(next)), world, 0) == FrameOutcome::kApplied);
}

}  // namespace

TEST_CASE("Traduction : un PNJ parle, puis tend un bouton", "[net][npc]") {
  World world;

  ro::proto::ZcSayDialog said;
  said.NpcID = 110'000;
  said.message = "Bonjour, aventurier.";
  CHECK(ApplyFrame(FrameOf(Encode(said)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->npc_id == 110'000);
  // ⚠ RIEN N'EST ENCORE AFFICHÉ : la page attend le paquet qui la termine.
  CHECK(world.npc_dialog()->lines.empty());

  ro::proto::ZcWaitDialog next;
  next.NpcID = 110'000;
  CHECK(ApplyFrame(FrameOf(Encode(next)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNext);
  REQUIRE(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->lines[0] == "Bonjour, aventurier.");

  ro::proto::ZcCloseDialog close;
  close.npcId = 110'000;
  CHECK(ApplyFrame(FrameOf(Encode(close)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kClose);
}

TEST_CASE("Traduction : le dialogue SECONDAIRE parle dans la même fenêtre", "[net][npc]") {
  // Son seul champ de plus désigne la fenêtre où le serveur voudrait écrire ;
  // nous n'en tenons qu'une, et la distinguer montrerait deux cadres pour une
  // seule conversation.
  World world;
  ro::proto::ZcSayDialog2 said;
  said.NpcID = 110'000;
  said.type = 1;
  said.message = "Un aparte.";
  CHECK(ApplyFrame(FrameOf(Encode(said)), world, 0) == FrameOutcome::kApplied);

  ro::proto::ZcWaitDialog2 next;
  next.NpcID = 110'000;
  next.type = 1;
  CHECK(ApplyFrame(FrameOf(Encode(next)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNext);
  REQUIRE(world.npc_dialog()->lines.size() == 1);
  CHECK(world.npc_dialog()->lines[0] == "Un aparte.");
}

TEST_CASE("Traduction : un « fermer » sans dialogue est compris et sans effet", "[net][npc]") {
  // ⚠ CE PAQUET NE FERME PAS, il pose un BOUTON. Le serveur l'envoie à la fin de
  // tout script : tant qu'on ne parle à personne, il arrive et ne trouve rien.
  World world;
  ro::proto::ZcCloseDialog close;
  close.npcId = 110'000;
  CHECK(ApplyFrame(FrameOf(Encode(close)), world, 0) == FrameOutcome::kIgnored);
  CHECK_FALSE(world.npc_dialog().has_value());
}

TEST_CASE("Traduction : un menu SEUL se ferme, un texte reçoit un bouton", "[net][npc]") {
  // ⚠ C'EST LA RÈGLE DU CLIENT, PAS CELLE DU SERVEUR, et elle a deux branches.
  // ZC_CLOSE_DIALOG pose un bouton « fermer » tant qu'une fenêtre de TEXTE est
  // ouverte ; sinon il ferme tout. Un agent de téléportation n'offre qu'un menu :
  // sans la seconde branche, sa fenêtre survivait au voyage.
  World world;
  ro::proto::ZcMenuList menu;
  menu.npcId = 110'000;
  menu.menu = "Prontera:Geffen";
  REQUIRE(ApplyFrame(FrameOf(Encode(menu)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.npc_dialog()->lines.empty());

  ro::proto::ZcCloseDialog close;
  close.npcId = 110'000;
  CHECK(ApplyFrame(FrameOf(Encode(close)), world, 0) == FrameOutcome::kApplied);
  CHECK_FALSE(world.npc_dialog().has_value());

  // Avec du TEXTE, le même paquet ne ferme pas : il tend un bouton.
  SayAndWait(world, 110'001, "Bon voyage.");
  close.npcId = 110'001;
  CHECK(ApplyFrame(FrameOf(Encode(close)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kClose);
}

TEST_CASE("Traduction : `clear` seul arme l'effacement, après un « fermer » il FERME",
          "[net][npc]") {
  // 🔴 LA SÉQUENCE « fermer » PUIS `clear` EST CELLE DU MINUTEUR D'INACTIVITÉ :
  // 0x00b6 seul échange le bouton et laisse la LISTE DE CHOIX à l'écran, et le
  // joueur revenu de son absence y cliquait — sur un script déjà mort, donc une
  // réponse partie sur la session suivante, donc l'éjection.
  World world;
  SayAndWait(world, 110'000, "Vous etes toujours la ?");

  // Seul, il n'efface rien : la page reste lisible jusqu'à ce que la suivante la
  // remplace.
  ro::proto::ZcClearDialog cleared;
  cleared.GID = 110'000;
  CHECK(ApplyFrame(FrameOf(Encode(cleared)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->lines.size() == 1);

  // Derrière un « fermer », c'est le serveur qui met fin à la session.
  ro::proto::ZcCloseDialog close;
  close.npcId = 110'000;
  REQUIRE(ApplyFrame(FrameOf(Encode(close)), world, 0) == FrameOutcome::kApplied);
  CHECK(ApplyFrame(FrameOf(Encode(cleared)), world, 0) == FrameOutcome::kApplied);
  CHECK_FALSE(world.npc_dialog().has_value());

  // Redit, il ne trouve plus rien, et ce n'est pas une panne.
  CHECK(ApplyFrame(FrameOf(Encode(cleared)), world, 0) == FrameOutcome::kIgnored);
}

TEST_CASE("Traduction : la taille et la position imposées au dialogue sont ignorées",
          "[net][npc]") {
  // Ignorées et non « non traitées » : la fenêtre se dispose comme les autres.
  World world;
  ro::proto::ZcDialogWindowSize size;
  size.width = 400;
  size.height = 300;
  CHECK(ApplyFrame(FrameOf(Encode(size)), world, 0) == FrameOutcome::kIgnored);

  ro::proto::ZcDialogWindowPos pos;
  pos.x = 100;
  pos.y = 50;
  CHECK(ApplyFrame(FrameOf(Encode(pos)), world, 0) == FrameOutcome::kIgnored);
  CHECK_FALSE(world.npc_dialog().has_value());
}

TEST_CASE("Traduction : les deux demandes de saisie", "[net][npc]") {
  World world;
  ro::proto::ZcOpenEditdlg number;
  number.npcId = 110'000;
  CHECK(ApplyFrame(FrameOf(Encode(number)), world, 0) == FrameOutcome::kApplied);
  REQUIRE(world.npc_dialog().has_value());
  // ⚠ UNE SAISIE OUVRE UN DIALOGUE À ELLE SEULE : un script peut appeler `input`
  // sans avoir rien dit.
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kNumber);

  ro::proto::ZcOpenEditdlgstr text;
  text.npcId = 110'000;
  CHECK(ApplyFrame(FrameOf(Encode(text)), world, 0) == FrameOutcome::kApplied);
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kText);
}

TEST_CASE("Réponse : répondre à un menu n'efface pas la question", "[net][npc]") {
  // ⚠ RÉPONDRE N'EST PAS « SUIVANT ». Le premier arme l'effacement de la page,
  // et la liste RESTE à l'écran, inerte, le temps de l'aller-retour : la vider
  // tout de suite escamoterait la question et ferait sauter le pied de fenêtre.
  World world;
  ro::net::Connection link;

  SayAndWait(world, 110'000, "Ou allez-vous ?");
  ro::proto::ZcMenuList menu;
  menu.npcId = 110'000;
  menu.menu = "Prontera:Geffen";
  REQUIRE(ApplyFrame(FrameOf(Encode(menu)), world, 0) == FrameOutcome::kApplied);

  // La connexion est fermée : l'envoi ne part pas, mais le contrôle du rang, lui,
  // a bien eu lieu — c'est lui qu'on éprouve ici.
  static_cast<void>(ro::net::AnswerNpcMenu(link, world, 1));
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->options.size() == 2);
  CHECK(world.npc_dialog()->menu_answered);
  CHECK(world.npc_dialog()->awaiting_reply);
  // Le texte est intact.
  REQUIRE(world.npc_dialog()->lines.size() == 1);

  // ⚠ ET UNE SECONDE RÉPONSE NE PART PAS. Le premier choix a fait avancer le
  // script ; le second viserait une liste qui n'a plus la même longueur — « got
  // 14, valid [1..3] », c'est-à-dire l'éjection.
  CHECK_FALSE(ro::net::AnswerNpcMenu(link, world, 0).has_value());
}

TEST_CASE("Traduction : un menu arrive découpé, sans ses entrées vides", "[net][npc]") {
  World world;
  ro::proto::ZcMenuList menu;
  menu.npcId = 110'000;
  menu.menu = "Acheter::Partir";
  CHECK(ApplyFrame(FrameOf(Encode(menu)), world, 0) == FrameOutcome::kApplied);

  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->prompt == NpcPrompt::kMenu);
  REQUIRE(world.npc_dialog()->options.size() == 2);
  CHECK(world.npc_dialog()->options[1].text == "Partir");
  // 🔴 ET SON RANG EST DEUX, PAS TROIS : le serveur ne compte pas l'entrée vide.
  CHECK(world.npc_dialog()->options[1].rank == 2);
}

TEST_CASE("Réponse : un rang hors du menu ne part pas", "[net][npc]") {
  // ⚠ LE SEUL ENDROIT QUI PUISSE REFUSER. La couche des paquets ne connaît pas
  // le menu ; l'interface ne sait pas qu'un mauvais rang fait ÉJECTER le joueur.
  // Ici, on a les deux.
  World world;
  ro::net::Connection link;

  // Sans dialogue, rien ne part non plus.
  CHECK_FALSE(ro::net::AnswerNpcNext(link, world).has_value());
  CHECK_FALSE(ro::net::AnswerNpcMenu(link, world, 0).has_value());
  CHECK_FALSE(ro::net::AnswerNpcNumber(link, world, 1).has_value());
  CHECK_FALSE(ro::net::AnswerNpcText(link, world, "bonjour").has_value());
  CHECK_FALSE(ro::net::LeaveNpcDialog(link, world).has_value());

  world.OfferNpcMenu(110'000, ro::sim::SplitNpcMenu("Oui:Non"), 0);
  CHECK_FALSE(ro::net::AnswerNpcMenu(link, world, 2).has_value());
  CHECK_FALSE(ro::net::AnswerNpcMenu(link, world, 99).has_value());
  // Un nombre n'est pas ce qu'un menu attend.
  CHECK_FALSE(ro::net::AnswerNpcNumber(link, world, 3).has_value());
  // Le dialogue est intact : un refus n'efface rien.
  REQUIRE(world.npc_dialog().has_value());
  CHECK(world.npc_dialog()->options.size() == 2);
}

TEST_CASE("Réponse : quitter un menu l'ANNULE, quitter un texte le ferme", "[net][npc]") {
  // 🔴 UN SCRIPT ARRÊTÉ SUR UN `select` ATTEND UN RANG, PAS UNE FERMETURE. Lui
  // envoyer CZ_CLOSE_DIALOG laisse `sd->npc_id` posé : le personnage ne lance
  // plus aucune compétence jusqu'au changement de carte.
  World world;
  ro::net::Connection link;

  world.OfferNpcMenu(110'000, ro::sim::SplitNpcMenu("Oui:Non"), 0);
  REQUIRE(world.npc_dialog()->prompt == NpcPrompt::kMenu);
  static_cast<void>(ro::net::LeaveNpcDialog(link, world));
  CHECK_FALSE(world.npc_dialog().has_value());

  SayAndWait(world, 110'001, "Au revoir.");
  static_cast<void>(ro::net::LeaveNpcDialog(link, world));
  CHECK_FALSE(world.npc_dialog().has_value());
}

TEST_CASE("Réponse : un nombre et un texte trouvent leur demande", "[net][npc]") {
  World world;
  ro::net::Connection link;

  world.SayNpcDialog(110'000, "Combien ?", 0);
  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kNumber, 0));
  static_cast<void>(ro::net::AnswerNpcNumber(link, world, 42));
  CHECK(world.npc_dialog()->awaiting_reply);
  // Une seconde soumission ne part pas : la première n'a pas encore de réponse.
  CHECK_FALSE(ro::net::AnswerNpcNumber(link, world, 43).has_value());

  REQUIRE(world.PromptNpcDialog(110'000, NpcPrompt::kText, 0));
  static_cast<void>(ro::net::AnswerNpcText(link, world, "Bonjour"));
  CHECK(world.npc_dialog()->awaiting_reply);
}

TEST_CASE("Réponse : parler à un PNJ s'encode comme le serveur le lit", "[net][npc]") {
  // ⚠ CE PAQUET EST CE QUI OUVRE LA CONVERSATION. Sans lui, tout le reste de
  // cette famille est écrit pour rien : le serveur n'a aucune raison de parler,
  // et la fenêtre n'apparaît jamais. L'essai le garde parce que son absence ne
  // casse RIEN de visible — elle ne fait qu'un silence.
  ro::proto::CzContactnpc packet;
  packet.AID = 110'000;
  packet.type = 0;
  const std::vector<std::uint8_t> frame = ro::proto::Encode(packet);
  REQUIRE(frame.size() == ro::proto::CzContactnpc::kWireSize);
  CHECK(frame[0] == 0x90);
  CHECK(frame[1] == 0x00);
}

TEST_CASE("Réponse : le rang nul ne s'encode pas", "[net][npc]") {
  // Le rang part à UN ; zéro est ce que le serveur traite comme une
  // manipulation du protocole. L'encodeur est le dernier filet.
  ro::proto::NpcMenuChoice choice;
  choice.npc_id = 110'000;
  choice.choice = 0;
  CHECK_FALSE(ro::proto::Encode(choice).has_value());

  choice.choice = ro::sim::kNpcMenuFirstChoice;
  const auto first = ro::proto::Encode(choice);
  REQUIRE(first.has_value());
  CHECK(first->size() == ro::proto::NpcMenuChoice::kWireSize);
  CHECK(first->back() == ro::sim::kNpcMenuFirstChoice);

  // L'annulation a sa valeur convenue, et elle passe.
  choice.choice = ro::sim::kNpcMenuCancelled;
  const auto annulation = ro::proto::Encode(choice);
  REQUIRE(annulation.has_value());
  CHECK(annulation->back() == ro::sim::kNpcMenuCancelled);
}

TEST_CASE("Réponse : une saisie VIDE porte quand même un octet", "[net][npc]") {
  // 🔴 `clif_parse_NpcStringInput` calcule la longueur du corps et RETOURNE sans
  // rien faire si elle est nulle : un champ validé à vide laisserait le script
  // suspendu pour toujours.
  ro::proto::CzInputEditdlgstr packet;
  packet.GID = 110'000;
  packet.value = std::string(1, '\0');
  const std::vector<std::uint8_t> frame = ro::proto::Encode(packet);
  REQUIRE(frame.size() == ro::proto::CzInputEditdlgstr::kWireSize + 1);
}

TEST_CASE("Réponse : le congé se lit AVANT que la carte change", "[net][npc]") {
  // 🔴 LE MONDE FERME LA FENÊTRE EN CHANGEANT DE CARTE : lu après, il n'y a plus
  // rien à interroger, et le mot dû serait perdu. C'est pourquoi la boucle le lit
  // avant d'appliquer la trame.
  World world;
  ro::net::Connection link;
  CHECK_FALSE(ro::net::NpcFarewellDue(world).has_value());

  world.OfferNpcMenu(110'000, ro::sim::SplitNpcMenu("Oui:Non"), 0);
  std::optional<ro::net::NpcFarewell> due = ro::net::NpcFarewellDue(world);
  REQUIRE(due.has_value());
  CHECK(due->npc_id == 110'000);
  // ⚠ LA LISTE ATTENDAIT ENCORE : ce n'est pas une fermeture qu'on doit au
  // serveur, c'est un RANG — voir `kNpcMenuCancelled`.
  CHECK(due->menu_pending);
  CHECK(ro::net::SayNpcFarewell(link, *due).has_value());

  world.EnterMap("prontera", 150, 150);
  CHECK_FALSE(ro::net::NpcFarewellDue(world).has_value());

  // Une page qui attend « suivant » se quitte par une fermeture, pas par un
  // rang : le menu est la SEULE forme qui en attende un.
  SayAndWait(world, 110'001, "Au revoir.");
  due = ro::net::NpcFarewellDue(world);
  REQUIRE(due.has_value());
  CHECK_FALSE(due->menu_pending);
  CHECK(ro::net::SayNpcFarewell(link, *due).has_value());
}
