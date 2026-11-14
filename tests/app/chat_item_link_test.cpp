#include "chat/chat_item_link.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/style_code.h"
#include "ro/ui/chat_link.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"
#include "items/item_facts_asker.h"

using moonclient::DecorateChatLine;
using moonclient::DecoratedChatLine;
using moonclient::MayCarryChatTags;
using moonclient::PendingChatLink;
using moonclient::PruneChatLinks;
using moonclient::ResolveChatLinks;
using ro::vfs::BuildItemLink;
using ro::vfs::ItemEntry;
using ro::vfs::ItemLink;
using ro::vfs::ItemTable;
using ro::vfs::ItemWords;

namespace {

// Une installation minuscule : deux objets et le mot d'une carte.
[[nodiscard]] ItemTable SmallTable() {
  ItemTable table;
  table.Put(1201, ItemEntry{.name = "Knife", .card_slots = 3});
  table.Put(501, ItemEntry{.name = "Red Potion"});
  table.Put(4001, ItemEntry{.name = "Poring Card"});
  return table;
}

[[nodiscard]] ItemWords SmallWords() {
  ItemWords words;
  words.cards[4001] = ro::vfs::CardWord{.text = "Poring", .after_name = false};
  return words;
}

[[nodiscard]] ItemLink KnifeLink() {
  ItemLink link;
  link.item_id = 1201;
  link.equipable = true;
  link.wearable_at = 2;
  return link;
}

}  // namespace

TEST_CASE("Lien du fil : une ligne sans balise n'est pas recopiée", "[app][lien]") {
  // ⚠ C'EST LA QUASI-TOTALITÉ DES LIGNES. Rendre `false` laisse l'appelant garder
  // sa vue sur le texte du monde : rien n'est alloué, rien n'est copié.
  DecoratedChatLine out;
  CHECK_FALSE(DecorateChatLine("bonjour tout le monde", SmallTable(), SmallWords(), &out));
}

TEST_CASE("Lien du fil : la question se pose avant de bâtir la ligne décorée", "[app][lien]") {
  // 🔴 LE TRI DU FIL LA POSE À CHAQUE LIGNE, À CHAQUE IMAGE, et presque toutes
  // répondent non : sans « < », il n'y a rien à décorer, ni rien à bâtir.
  CHECK_FALSE(MayCarryChatTags("bonjour tout le monde"));
  CHECK_FALSE(MayCarryChatTags(""));
  // ⚠ LES DEUX ESPÈCES DE BALISE PASSENT : une balise arrêtée ici resterait à
  // l'écran telle quelle.
  CHECK(MayCarryChatTags("regarde " + BuildItemLink(KnifeLink(), 3)));
  CHECK(MayCarryChatTags(
      ro::sim::BuildStyleTag("Lune", ro::sim::EncodeStyleCode(ro::sim::StyleRecipe{}))));
}

TEST_CASE("Lien du fil : la balise devient un nom entre chevrons", "[app][lien]") {
  // 🔴 SANS CELA LE JOUEUR LIT LA BALISE. Ce n'est pas un ornement manquant :
  // n'importe quel joueur qui lie un objet remplirait notre fil de charabia.
  const std::string raw = "regarde " + BuildItemLink(KnifeLink(), 3) + " !";
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), SmallWords(), &out));
  CHECK(out.text == "regarde <Knife [3]> !");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].item_id == 1201);
  // ⚠ LES CHEVRONS SONT HORS DU MORCEAU CLIQUABLE : ils encadrent le lien, ils
  // n'en font pas partie.
  CHECK(out.text.substr(out.links[0].offset, out.links[0].length) == "Knife [3]");
}

TEST_CASE("Lien du fil : le nom porte ce que la balise transporte", "[app][lien]") {
  // Raffinement et cartes voyagent dans la balise, et c'est tout l'intérêt : le
  // lien décrit L'OBJET DE QUELQU'UN, pas une entrée de catalogue.
  ItemLink link = KnifeLink();
  link.refine_level = 7;
  link.cards = {4001, 0, 0, 0};

  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(BuildItemLink(link, 3), SmallTable(), SmallWords(), &out));
  // ⚠ LE RAFFINEMENT PASSE DEVANT LE MOT DE CARTE, et c'est l'ordre du client :
  // « +7 Poring Knife [3] ».
  CHECK(out.text == "<+7 Poring Knife [3]>");
}

TEST_CASE("Lien du fil : l'objet cassé garde son ombre", "[app][lien]") {
  // Le champ privé de la balise n'a d'autre raison d'être que celle-ci.
  ItemLink link = KnifeLink();
  link.damaged = true;
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(BuildItemLink(link, 3), SmallTable(), SmallWords(), &out));
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].damaged);
}

TEST_CASE("Lien du fil : plusieurs balises dans une phrase", "[app][lien]") {
  ItemLink potion;
  potion.item_id = 501;
  const std::string raw =
      BuildItemLink(KnifeLink(), 3) + " contre " + BuildItemLink(potion, 0) + " ?";

  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), SmallWords(), &out));
  CHECK(out.text == "<Knife [3]> contre <Red Potion> ?");
  REQUIRE(out.links.size() == 2);
  CHECK(out.links[0].item_id == 1201);
  CHECK(out.links[1].item_id == 501);
  // 🔴 LES BORNES DU SECOND SONT CELLES DU TEXTE RÉÉCRIT, pas de la ligne reçue.
  // C'est la raison d'être de cette fonction : les deux n'ont plus rien à voir.
  CHECK(out.text.substr(out.links[1].offset, out.links[1].length) == "Red Potion");
}

TEST_CASE("Lien du fil : un objet que la table ignore reste CLIQUABLE", "[app][lien]") {
  // 🔴 CE REPLI N'EST PAS DE CE FICHIER, il vient de la composition des noms, qui
  // écrit « #12345 » pour ce qu'elle ne connaît pas — là où le client écrit une
  // espace, c'est-à-dire rien du tout. Le lien reste donc lisible ET cliquable :
  // sa description montrera au moins l'identifiant et son lien de base de
  // données. J'attendais que la balise reste telle quelle ; elle fait mieux.
  ItemLink unknown;
  unknown.item_id = 99'999;

  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(BuildItemLink(unknown, 0), SmallTable(), SmallWords(), &out));
  CHECK(out.text == "<#99999>");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].item_id == 99'999);
}

TEST_CASE("Lien du fil : une balise tronquée ne fait pas tourner la boucle", "[app][lien]") {
  // 🔴 C'EST UN GEL QU'ON ÉVITE ICI, pas un défaut d'affichage. Le serveur coupe
  // les lignes trop longues ; sans avancer au-delà de la balise sans fermante, on
  // la relirait indéfiniment.
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine("tiens <ITEML>0000", SmallTable(), SmallWords(), &out));
  CHECK(out.text == "tiens <ITEML>0000");
  CHECK(out.links.empty());
}

// ── Poser un lien dans la saisie ────────────────────────────────────────────

TEST_CASE("Lien posé : ce qui part n'est pas ce qui est tapé", "[app][lien]") {
  // 🔴 C'EST TOUTE LA MÉCANIQUE. La balise fait une cinquantaine d'octets de
  // charabia : l'écrire dans la saisie rendrait la ligne illisible pendant qu'on
  // la compose. Le joueur voit le nom, la ligne porte la balise.
  std::vector<PendingChatLink> pending = {
      PendingChatLink{.shown = "<Knife [3]>", .wire = "<ITEML>knife</ITEML>"}};

  CHECK(ResolveChatLinks("regarde <Knife [3]> !", pending) == "regarde <ITEML>knife</ITEML> !");
}

TEST_CASE("Lien posé : deux fois le même objet donnent deux balises distinctes", "[app][lien]") {
  // ⚠ UNE FOIS CHACUN, ET DANS L'ORDRE. Remplacer toutes les occurrences d'un
  // coup enverrait deux fois la première balise — et les deux objets n'ont pas
  // forcément le même raffinement.
  std::vector<PendingChatLink> pending = {
      PendingChatLink{.shown = "<Knife [3]>", .wire = "<ITEML>un</ITEML>"},
      PendingChatLink{.shown = "<Knife [3]>", .wire = "<ITEML>deux</ITEML>"}};

  CHECK(ResolveChatLinks("<Knife [3]> et <Knife [3]>", pending) ==
        "<ITEML>un</ITEML> et <ITEML>deux</ITEML>");
}

TEST_CASE("Lien posé : celui que le joueur a effacé est simplement sauté", "[app][lien]") {
  std::vector<PendingChatLink> pending = {
      PendingChatLink{.shown = "<Knife [3]>", .wire = "<ITEML>un</ITEML>"},
      PendingChatLink{.shown = "<Red Potion>", .wire = "<ITEML>deux</ITEML>"}};

  CHECK(ResolveChatLinks("j'ai <Red Potion>", pending) == "j'ai <ITEML>deux</ITEML>");
}

TEST_CASE("Lien posé : l'élagage débouche le plafond", "[app][lien]") {
  // ⚠ SANS LUI LE PLAFOND SE BOUCHE : le joueur qui efface un lien doit pouvoir
  // en reposer un autre. Trois liens et un seul reste dans la saisie.
  std::vector<PendingChatLink> pending = {
      PendingChatLink{.shown = "<A>", .wire = "<ITEML>a</ITEML>"},
      PendingChatLink{.shown = "<B>", .wire = "<ITEML>b</ITEML>"},
      PendingChatLink{.shown = "<C>", .wire = "<ITEML>c</ITEML>"}};
  REQUIRE(pending.size() == moonclient::kMostChatLinks);

  PruneChatLinks("il me reste <B>", &pending);
  REQUIRE(pending.size() == 1);
  CHECK(pending[0].wire == "<ITEML>b</ITEML>");
}

TEST_CASE("Lien posé : une saisie vidée oublie tout", "[app][lien]") {
  std::vector<PendingChatLink> pending = {
      PendingChatLink{.shown = "<A>", .wire = "<ITEML>a</ITEML>"}};
  PruneChatLinks("", &pending);
  CHECK(pending.empty());
  // Et rien à substituer ne touche à rien.
  CHECK(ResolveChatLinks("bonjour", pending) == "bonjour");
}

// ── LA SECONDE ESPÈCE DE LIEN : UN STYLE ────────────────────────────────────

TEST_CASE("Lien de style : la balise devient « [Style: Pseudo] »", "[app][lien]") {
  // 🔴 LE LIBELLÉ SE COMPOSE CHEZ LE LECTEUR, jamais transporté tout fait :
  // chacun le lit dans SA langue. Et ce sont des CROCHETS, pas les chevrons
  // d'un objet — deux espèces qui se ressembleraient promettraient le même
  // menu.
  const std::string code = ro::sim::EncodeStyleCode(ro::sim::StyleRecipe{});
  const std::string raw = "regarde " + ro::sim::BuildStyleTag("Lune", code) + " !";

  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), SmallWords(), &out));
  CHECK(out.text == "regarde [Style: Lune] !");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ro::ui::ChatLinkKind::kStyle);
  // ⚠ ZÉRO, ET C'EST UNE RÉPONSE JUSTE : un style n'a pas d'identifiant, c'est
  // le jeton qui le retrouve.
  CHECK(out.links[0].item_id == 0);
  CHECK(out.text.substr(out.links[0].offset, out.links[0].length) == "[Style: Lune]");
  REQUIRE(out.carried.size() == 1);
  // 🔴 LE CODE EN ENTIER, TEL QU'IL A VOYAGÉ : c'est ce qui distingue ce lien
  // de tous les autres. Un objet existe chez le lecteur ; un style n'existe
  // nulle part hors de son porteur.
  CHECK(out.carried[0].code == code);
  CHECK(out.carried[0].owner == "Lune");
}

TEST_CASE("Lien de style : un code périmé garde le nom et perd le lien", "[app][lien]") {
  // 🔴 LE REFUS SE DÉCIDE À L'ANALYSE, PAS AU CLIC : un lien qui échoue au
  // moment où on l'ouvre est une promesse trahie. Et cent dix caractères
  // d'hexadécimal n'apprendraient rien — contrairement à une balise d'objet,
  // qui est courte et reste donc telle quelle.
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine("tiens <STYL>Lune:6:00000000:-1:-1:-1:</STYL> !", SmallTable(),
                           SmallWords(), &out));
  CHECK(out.text == "tiens [Lune] !");
  CHECK(out.links.empty());
}

TEST_CASE("Lien de style : une balise tronquée ne fait pas tourner la boucle", "[app][lien]") {
  // Le même gel que pour un objet : sans avancer au-delà d'une balise sans
  // fermante, on la relirait indéfiniment.
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine("tiens <STYL>Lune:7", SmallTable(), SmallWords(), &out));
  CHECK(out.text == "tiens <STYL>Lune:7");
  CHECK(out.links.empty());
}

TEST_CASE("Lien de style : sans étiquette, pas de deux-points orphelin", "[app][lien]") {
  const std::string code = ro::sim::EncodeStyleCode(ro::sim::StyleRecipe{});
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(ro::sim::BuildStyleTag("", code), SmallTable(), SmallWords(), &out));
  CHECK(out.text == "[Style]");
  REQUIRE(out.links.size() == 1);
  CHECK(out.carried[0].owner.empty());
}

TEST_CASE("Lien de style : les deux espèces cohabitent dans une phrase", "[app][lien]") {
  // 🔴 LA PREMIÈRE DES DEUX BALISES GAGNE, et il faut les chercher ENSEMBLE :
  // traiter les objets d'abord découperait la balise de style qui les précède,
  // et le morceau restant s'afficherait au milieu de la phrase.
  const std::string code = ro::sim::EncodeStyleCode(ro::sim::StyleRecipe{});
  const std::string raw = ro::sim::BuildStyleTag("Lune", code) + " avec " +
                          BuildItemLink(KnifeLink(), 3) + " et " +
                          ro::sim::BuildStyleTag("Soir", code);

  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), SmallWords(), &out));
  CHECK(out.text == "[Style: Lune] avec <Knife [3]> et [Style: Soir]");
  REQUIRE(out.links.size() == 3);
  CHECK(out.links[0].kind == ro::ui::ChatLinkKind::kStyle);
  CHECK(out.links[1].kind == ro::ui::ChatLinkKind::kItem);
  CHECK(out.links[2].kind == ro::ui::ChatLinkKind::kStyle);
  // 🔴 UNE SEULE TABLE POUR LES DEUX ESPÈCES : le jeton est un RANG, et deux
  // numérotations parallèles feraient ouvrir au clic sur un style la
  // description de l'objet de même rang.
  REQUIRE(out.carried.size() == 3);
  CHECK(out.carried[0].owner == "Lune");
  CHECK(out.carried[1].item.item_id == 1201);
  CHECK(out.carried[2].owner == "Soir");
  // Et les bornes du dernier désignent bien le texte RÉÉCRIT.
  CHECK(out.text.substr(out.links[2].offset, out.links[2].length) == "[Style: Soir]");
}

TEST_CASE("Lien du fil : un item forgé se nomme par son forgeron", "[app][lien]") {
  constexpr std::uint32_t kForgeMark = 0xFF;
  constexpr std::uint32_t kSmith = 150001;
  constexpr std::uint32_t kWordMask = 0xFFFF;
  constexpr unsigned kWordBits = 16;
  ItemLink link = KnifeLink();
  link.cards = {kForgeMark, 0, kSmith & kWordMask, kSmith >> kWordBits};
  ItemWords words = SmallWords();
  words.nameless_creator = "Sans nom";
  const std::string raw = BuildItemLink(link, 3);

  // Sans la poignée, ou tant que le serveur n'a pas répondu : « Sans nom ».
  DecoratedChatLine unnamed;
  REQUIRE(DecorateChatLine(raw, SmallTable(), words, &unnamed));
  CHECK(unnamed.text.find("Sans nom") != std::string::npos);

  moonclient::ItemFactsAsker asker;
  const moonclient::CreatorNames creators(&asker);
  DecoratedChatLine waiting;
  REQUIRE(DecorateChatLine(raw, SmallTable(), words, &waiting, &creators));
  CHECK(waiting.text.find("Sans nom") != std::string::npos);

  // La réponse arrivée : le forgeron à sa place.
  asker.facts().TellCreator(kSmith, "Forgeron");
  DecoratedChatLine named;
  REQUIRE(DecorateChatLine(raw, SmallTable(), words, &named, &creators));
  CHECK(named.text.find("Forgeron") != std::string::npos);
  CHECK(named.text.find("Sans nom") == std::string::npos);
}

// ── Un respawn de MVP ───────────────────────────────────────────────────────

namespace {

constexpr std::int64_t kMvpKillS = 1'790'000'000;
constexpr std::int64_t kMvpMinuteS = 60;

[[nodiscard]] moonclient::MvpLink BaphometRespawn() {
  return moonclient::MvpLink{.monster_id = 1039,
                             .map = "prt_maze03",
                             .kill_time_s = kMvpKillS,
                             .delay1_min = 120,
                             .delay2_min = 10,
                             .tomb_x = 101,
                             .tomb_y = 47,
                             .name = "Baphomet"};
}

}  // namespace

TEST_CASE("Lien du fil : un respawn de MVP devient son libellé, crochets compris", "[app][lien]") {
  const moonclient::MvpLink link = BaphometRespawn();
  const std::string label = moonclient::MvpLinkLabel(link);
  const std::string raw = "vu " + moonclient::BuildMvpLink(link) + " !";
  REQUIRE(MayCarryChatTags(raw));
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), SmallWords(), &out));
  CHECK(out.text == "vu " + label + " !");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ro::ui::ChatLinkKind::kMvp);
  CHECK(out.links[0].item_id == 1039);
  // ⚠ LES CROCHETS SONT DANS LE MORCEAU : ils font le libellé.
  CHECK(out.text.substr(out.links[0].offset, out.links[0].length) == label);
  CHECK(label.front() == '[');
  // SANS HORLOGE, JAMAIS ÉCHU : c'est le cas des bulles.
  CHECK_FALSE(out.links[0].spent);
  CHECK(out.links[0].mark_length == 0);
  REQUIRE(out.carried.size() == 1);
  CHECK(out.carried[0].kind == ro::ui::ChatLinkKind::kMvp);
  CHECK(out.carried[0].mvp.map == "prt_maze03");
  CHECK(out.carried[0].mvp.tomb_x == 101);
  // L'expéditeur se pose ailleurs : la ligne seule ne le dit pas toujours.
  CHECK(out.carried[0].sender.empty());
}

TEST_CASE("Lien du fil : un respawn échu porte sa marque avant le crochet", "[app][lien]") {
  const moonclient::MvpLink link = BaphometRespawn();
  const std::string label = moonclient::MvpLinkLabel(link);
  const moonclient::MvpLinkClock late{.server_now_s = kMvpKillS + 131 * kMvpMinuteS};
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(moonclient::BuildMvpLink(link), SmallTable(), SmallWords(), &out,
                           nullptr, nullptr, &late));
  REQUIRE(out.links.size() == 1);
  const ro::ui::ChatLinkSpan& span = out.links[0];
  CHECK(span.spent);
  // LA MARQUE, espace de tête comprise, se glisse à la place du « ] » d'origine.
  CHECK(span.mark_at == label.size() - 1);
  const std::string mark = out.text.substr(span.offset + span.mark_at, span.mark_length);
  CHECK(mark == " · passé");
  CHECK(out.text.ends_with(" · passé]"));
  CHECK(span.length == label.size() + span.mark_length);
  // 🔴 PRIVÉ DE SA MARQUE, LE MORCEAU REDONNE LE LIBELLÉ mot pour mot : c'est la
  // tête du menu.
  std::string unmarked = out.text.substr(span.offset, span.length);
  unmarked.erase(span.mark_at, span.mark_length);
  CHECK(unmarked == label);

  // ENCORE DANS SA FENÊTRE : ni marque ni orange éteint.
  const moonclient::MvpLinkClock early{.server_now_s = kMvpKillS + 125 * kMvpMinuteS};
  DecoratedChatLine fresh;
  REQUIRE(DecorateChatLine(moonclient::BuildMvpLink(link), SmallTable(), SmallWords(), &fresh,
                           nullptr, nullptr, &early));
  REQUIRE(fresh.links.size() == 1);
  CHECK_FALSE(fresh.links[0].spent);
  CHECK(fresh.links[0].mark_length == 0);
  CHECK(fresh.text == label);
}

TEST_CASE("Lien du fil : un respawn illisible survit en texte", "[app][lien]") {
  // UN CORPS QUI N'APPREND RIEN : ses crochets, sans lien.
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine("a <MVPL>1039:prt_maze03:0:0:0:0:-1:-1:Baphomet</MVPL> b", SmallTable(),
                           SmallWords(), &out));
  CHECK(out.text == "a [1039:prt_maze03:0:0:0:0:-1:-1:Baphomet] b");
  CHECK(out.links.empty());
  // UNE FERMANTE ABSENTE : l'ouvrante reste du texte.
  DecoratedChatLine cut;
  REQUIRE(DecorateChatLine("a <MVPL>1039:prt", SmallTable(), SmallWords(), &cut));
  CHECK(cut.text == "a <MVPL>1039:prt");
  CHECK(cut.links.empty());
  // 🔴 ET LA LECTURE CONTINUE : le lien qui suit se rend encore.
  DecoratedChatLine then;
  REQUIRE(DecorateChatLine("a <MVPL>1039:prt " + BuildItemLink(KnifeLink(), 3), SmallTable(),
                           SmallWords(), &then));
  CHECK(then.text == "a <MVPL>1039:prt <Knife [3]>");
  REQUIRE(then.links.size() == 1);
  CHECK(then.links[0].kind == ro::ui::ChatLinkKind::kItem);
}

TEST_CASE("Lien du fil : un respawn et un objet, dans l'ordre de la ligne", "[app][lien]") {
  const moonclient::MvpLink link = BaphometRespawn();
  const std::string raw = moonclient::BuildMvpLink(link) + " drop " + BuildItemLink(KnifeLink(), 3);
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), SmallWords(), &out));
  CHECK(out.text == moonclient::MvpLinkLabel(link) + " drop <Knife [3]>");
  REQUIRE(out.links.size() == 2);
  CHECK(out.links[0].kind == ro::ui::ChatLinkKind::kMvp);
  CHECK(out.links[1].kind == ro::ui::ChatLinkKind::kItem);
  REQUIRE(out.carried.size() == 2);
  CHECK(out.carried[0].mvp.name == "Baphomet");
  CHECK(out.carried[1].item.item_id == 1201);
  CHECK(out.text.substr(out.links[1].offset, out.links[1].length) == "Knife [3]");
}

TEST_CASE("Lien du fil : une recherche de navigation et un monstre, dans l'ordre de la ligne",
          "[app][lien][navi]") {
  const std::string raw =
      "<NAVS>1:prontera:Kafra Employee</NAVS> et <MOBL>1039:2:Baphomet</MOBL> ici";
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(raw, SmallTable(), SmallWords(), &out));
  // SANS TABLE DE NOMS, la carte se montre par son nom interne.
  CHECK(out.text == "[PNJ: Kafra Employee (prontera)] et <[MVP] Baphomet> ici");
  REQUIRE(out.links.size() == 2);
  CHECK(out.links[0].kind == ro::ui::ChatLinkKind::kNaviSearch);
  CHECK(out.links[0].navi.family == 1);
  CHECK(out.links[0].navi.bounded);
  CHECK(out.text.substr(out.links[0].offset, out.links[0].length) ==
        "[PNJ: Kafra Employee (prontera)]");
  CHECK(out.links[1].kind == ro::ui::ChatLinkKind::kMonster);
  REQUIRE(out.carried.size() == 2);
  CHECK(out.carried[0].navi_search.term == "Kafra Employee");
  CHECK(out.carried[0].navi_search.map == "prontera");
  CHECK(out.carried[1].monster.monster_id == 1039);
}

TEST_CASE("Lien du fil : une recherche refusée devient texte, une vide disparaît",
          "[app][lien][navi]") {
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine("a <NAVS>7::Bob</NAVS> b <NAVS>2::</NAVS>c", SmallTable(), SmallWords(),
                           &out));
  CHECK(out.text == "a [Bob] b c");
  CHECK(out.links.empty());
  // SANS « : », L'OUVRANTE RESTE DU TEXTE, et le lien qui suit se rend encore.
  DecoratedChatLine then;
  REQUIRE(DecorateChatLine("a <NAVS>Bob</NAVS> <NAVS>2::Poring</NAVS>", SmallTable(), SmallWords(),
                           &then));
  CHECK(then.text == "a <NAVS>Bob</NAVS> [Monstre: Poring]");
  REQUIRE(then.links.size() == 1);
  CHECK(then.links[0].kind == ro::ui::ChatLinkKind::kNaviSearch);
}

TEST_CASE("Pose d'un lien : au bout de la saisie, séparé de ce qui précède", "[app][chat]") {
  ro::ui::ChatInputState input;
  const std::string typed = "regarde";
  std::ranges::copy(typed, input.text.begin());
  input.text[typed.size()] = '\0';
  std::vector<PendingChatLink> links;
  REQUIRE(moonclient::PlaceChatLinkLabel("<Knife>", input, &links));
  CHECK(std::string(input.text.data()) == "regarde <Knife>");
  CHECK(input.pending_insert.empty());
}

TEST_CASE("Pose d'un lien : une ligne retenue dans le dépôt garde ses liens, et passe devant",
          "[app][chat]") {
  // La ligne retenue attend dans le dépôt, la saisie est vide. Le lien posé
  // élague contre texte ET dépôt, et s'ajoute au dépôt derrière la ligne : ses
  // liens à elle restent posés, dans leur ordre de pose.
  ro::ui::ChatInputState input;
  input.pending_insert = "aaa <Pomme>";
  std::vector<PendingChatLink> links = {
      {.shown = "<Pomme>", .wire = "<ITEML>000001</ITEML>", .kind = ro::ui::ChatLinkKind::kItem}};
  REQUIRE(moonclient::PlaceChatLinkLabel("<Knife>", input, &links));
  links.push_back(PendingChatLink{.shown = "<Knife>", .wire = "<ITEML>0004b1</ITEML>"});
  CHECK(input.text[0] == '\0');
  CHECK(input.pending_insert == "aaa <Pomme> <Knife>");
  // L'élagage de l'image suivante, sur ce que la saisie portera, les garde
  // tous deux, dans leur ordre de pose.
  PruneChatLinks(std::string(input.text.data()) + input.pending_insert, &links);
  REQUIRE(links.size() == 2);
  CHECK(links[0].shown == "<Pomme>");
  CHECK(links[1].shown == "<Knife>");
}

TEST_CASE("Pose d'un lien : le plafond se compte dépôt compris, et rien ne déborde",
          "[app][chat]") {
  ro::ui::ChatInputState input;
  input.pending_insert = "<A> <B> <C>";
  std::vector<PendingChatLink> links = {{.shown = "<A>"}, {.shown = "<B>"}, {.shown = "<C>"}};
  CHECK_FALSE(moonclient::PlaceChatLinkLabel("<D>", input, &links));
  CHECK(input.pending_insert == "<A> <B> <C>");
  CHECK(links.size() == 3);

  // Un libellé qui ne tiendrait pas avec le dépôt est refusé, le dépôt intact.
  ro::ui::ChatInputState full;
  full.pending_insert = std::string(ro::ui::kChatInputSize - 4, 'a');
  std::vector<PendingChatLink> none;
  CHECK_FALSE(moonclient::PlaceChatLinkLabel("<Knife>", full, &none));
  CHECK(full.pending_insert.size() == ro::ui::kChatInputSize - 4);
}
