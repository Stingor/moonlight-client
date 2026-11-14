// Les onglets de conversation : leur filtre, et ce qu'ils deviennent sur disque.

#include "chat/chat_tabs.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "chat/chat_tab_file.h"
#include "support/temp_directory.h"

namespace {

using moonclient::ChatTab;
using moonclient::ChatTabs;
using moonclient::EveryChatType;
using moonclient::kDefaultChatTypes;
using moonclient::MaskOf;
using ro::sim::ChatType;

}  // namespace

TEST_CASE("Onglets : au départ, un seul, et il montre tout sauf Mes skills", "[app][onglets]") {
  // ⚠ C'EST CE QUE FAIT LE CLIENT. En poser quatre d'avance déciderait à la
  // place du joueur de la façon dont il range sa conversation.
  const ChatTabs tabs;
  REQUIRE(tabs.count() == 1);
  CHECK(tabs.tabs()[0].accepted == kDefaultChatTypes);
  // L'armement et le lancer de ses skills noieraient le fil : décochés d'office.
  CHECK_FALSE(tabs.tabs()[0].Accepts(ChatType::kSkillUse));
  CHECK(tabs.tabs()[0].Accepts(ChatType::kSkillFailure));
}

TEST_CASE("Onglets : le dernier ne se retire pas", "[app][onglets]") {
  // ⚠ IL NE RESTERAIT RIEN OÙ LIRE, ni rien où cliquer pour en refaire un.
  ChatTabs tabs;
  tabs.Remove(0);
  CHECK(tabs.count() == 1);

  tabs.Add("Groupe");
  REQUIRE(tabs.count() == 2);
  tabs.Remove(0);
  CHECK(tabs.count() == 1);
  CHECK(tabs.tabs()[0].name == "Groupe");
}

TEST_CASE("Onglets : un filtre ne laisse passer que ce qu'il accepte", "[app][onglets]") {
  ChatTabs tabs;
  const std::size_t rank = tabs.Add("Chuchotements");
  tabs.SetAccepted(rank, MaskOf(ChatType::kWhisper));

  const ChatTab& only = tabs.tabs()[rank];
  CHECK(only.Accepts(ChatType::kWhisper));
  CHECK_FALSE(only.Accepts(ChatType::kPublicChat));
  CHECK_FALSE(only.Accepts(ChatType::kPublic));
}

TEST_CASE("Onglets : un compte de changements, pas un drapeau", "[app][onglets]") {
  // ⚠ LA LISTE SE MODIFIE DEPUIS PLUSIEURS ENDROITS — la fenêtre, le fichier —
  // et c'est ce compte qui dit s'il faut réécrire.
  ChatTabs tabs;
  const std::uint32_t at_rest = tabs.revision();
  tabs.Rename(0, tabs.tabs()[0].name);  // le même nom : rien n'a bougé
  CHECK(tabs.revision() == at_rest);
  tabs.Rename(0, "Autre");
  CHECK(tabs.revision() != at_rest);
}

TEST_CASE("Onglets : ils se relisent tels qu'on les a écrits", "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  ChatTabs written;
  written.Rename(0, "Tout");
  const std::size_t party = written.Add("Groupe");
  written.SetAccepted(party, MaskOf(ChatType::kPartyChat) | MaskOf(ChatType::kGuildChat));
  REQUIRE(moonclient::SaveChatTabs(folder.path(), written));

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 2);
  CHECK(read.tabs()[0].name == "Tout");
  CHECK(read.tabs()[0].accepted == kDefaultChatTypes);
  CHECK(read.tabs()[1].name == "Groupe");
  CHECK(read.tabs()[1].Accepts(ChatType::kPartyChat));
  CHECK(read.tabs()[1].Accepts(ChatType::kGuildChat));
  CHECK_FALSE(read.tabs()[1].Accepts(ChatType::kPublicChat));
}

TEST_CASE("Onglets : une case illisible est décochée, l'onglet reste", "[app][onglets]") {
  // ⚠ UN MOT À LA PLACE D'UN BOOLÉEN ne coûte pas l'onglet entier : la case
  // qu'il occupe est lue décochée, les autres gardent leur valeur.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_tabs.yml",
                   "tabs:\n"
                   "  - name: Commerce\n"
                   "    filter: [true, encheres, true]\n");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 1);
  CHECK(read.tabs()[0].name == "Commerce");
  CHECK(read.tabs()[0].Accepts(ChatType::kPublic));
  CHECK_FALSE(read.tabs()[0].Accepts(ChatType::kPublicChat));
  CHECK(read.tabs()[0].Accepts(ChatType::kWhisper));
}

TEST_CASE("Onglets : une liste vide laisse ceux d'origine", "[app][onglets]") {
  // Une conversation sans onglet n'aurait nulle part où s'écrire.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_tabs.yml", "tabs: []\n");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  CHECK(read.count() == 1);
}

TEST_CASE("Onglets : un fichier abîmé ne coûte pas la conversation", "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_tabs.yml", "tabs:\n  - name: [\n");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  CHECK(read.count() == 1);
  CHECK(read.tabs()[0].accepted == kDefaultChatTypes);
}

TEST_CASE("Onglets : un onglet se déplace devant un autre", "[app][onglets]") {
  // ⚠ LE RANG D'ARRIVÉE EST COMPTÉ DANS LA BANDE TELLE QU'ELLE EST DESSINÉE,
  // celui qu'on déplace COMPRIS — c'est ce que le joueur voit quand il lâche.
  ChatTabs tabs;
  tabs.Rename(0, "Tout");
  tabs.Add("Groupe");
  tabs.Add("Guilde");

  // « Guilde » lâché devant « Groupe » : il se range au rang 1.
  tabs.Move(2, 1);
  REQUIRE(tabs.count() == 3);
  CHECK(tabs.tabs()[0].name == "Tout");
  CHECK(tabs.tabs()[1].name == "Guilde");
  CHECK(tabs.tabs()[2].name == "Groupe");
}

TEST_CASE("Onglets : l'onglet montré reste montré, quel que soit celui qui bouge",
          "[app][onglets]") {
  // 🔴 TOUT CE QUI SE TROUVE ENTRE LES DEUX PLACES GLISSE D'UN RANG. Ne recaler
  // que la languette déplacée faisait passer la vue à son voisin dès qu'on en
  // rangeait une autre par-dessus elle. Chaque geste possible, sur quatre
  // onglets, est rejoué sur le vrai `Move`.
  constexpr int kCount = 4;
  for (int from = 0; from < kCount; ++from) {
    for (int before = 0; before <= kCount; ++before) {
      for (int shown = 0; shown < kCount; ++shown) {
        ChatTabs tabs;
        tabs.Rename(0, "0");
        for (int rank = 1; rank < kCount; ++rank) {
          tabs.Add(std::to_string(rank));
        }
        const std::string seen = tabs.tabs()[static_cast<std::size_t>(shown)].name;
        tabs.Move(static_cast<std::size_t>(from), static_cast<std::size_t>(before));
        const int now = moonclient::ShownAfterMove(shown, from, before);
        CAPTURE(from, before, shown);
        REQUIRE(now >= 0);
        REQUIRE(now < kCount);
        CHECK(tabs.tabs()[static_cast<std::size_t>(now)].name == seen);
      }
    }
  }
}

TEST_CASE("Onglets : reposé là où il était, rien ne bouge", "[app][onglets]") {
  // ⚠ LE RETRAIT DÉCALE D'UN CRAN TOUT CE QUI SUIVAIT : sans la conversion, un
  // onglet lâché sur sa propre place partirait d'un rang, et le compte de
  // changements ferait réécrire le fichier pour rien.
  ChatTabs tabs;
  tabs.Add("Groupe");
  const std::uint32_t at_rest = tabs.revision();
  tabs.Move(0, 0);
  CHECK(tabs.revision() == at_rest);
  tabs.Move(0, 1);  // « devant celui qui me suit » = ma propre place
  CHECK(tabs.revision() == at_rest);
  CHECK(tabs.tabs()[1].name == "Groupe");
}

TEST_CASE("Onglets : un rang hors bande ne dérange rien", "[app][onglets]") {
  ChatTabs tabs;
  tabs.Add("Groupe");
  const std::uint32_t at_rest = tabs.revision();
  tabs.Move(9, 0);
  tabs.Move(0, 9);
  CHECK(tabs.revision() == at_rest);
  CHECK(tabs.count() == 2);
}

TEST_CASE("Onglets : vider masque, et ne détruit rien", "[app][onglets]") {
  // ⚠ LA CONVERSATION EST UN TAMPON PARTAGÉ par tous les onglets : en retirer les
  // lignes d'un onglet les arracherait à tous ceux qui les acceptent aussi. Vider
  // « Guilde » ne doit rien coûter à l'onglet qui montre tout.
  ChatTabs tabs;
  const std::size_t guild = tabs.Add("Guilde");
  tabs.ClearThrough(guild, 12);
  CHECK(tabs.tabs()[guild].cleared_through == 12);
  CHECK(tabs.tabs()[0].cleared_through == 0);  // l'autre onglet n'a rien perdu
  // Et le geste se défait.
  tabs.ClearThrough(guild, 0);
  CHECK(tabs.tabs()[guild].cleared_through == 0);
}

TEST_CASE("Onglets : vider n'est pas un changement à enregistrer", "[app][onglets]") {
  // ⚠ LE RANG NE SURVIT PAS À LA PARTIE — il est réattribué à chaque session, donc
  // un rang relu d'un fichier ne désignerait plus rien. Le champ ne s'écrit pas, et
  // lever le compte de changements ferait réécrire le fichier pour rien.
  ChatTabs tabs;
  const std::uint32_t at_rest = tabs.revision();
  tabs.ClearThrough(0, 42);
  CHECK(tabs.revision() == at_rest);
}

TEST_CASE("Onglets : le vidage ne franchit pas le disque", "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  ChatTabs written;
  written.ClearThrough(0, 99);
  REQUIRE(moonclient::SaveChatTabs(folder.path(), written));

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 1);
  CHECK(read.tabs()[0].cleared_through == 0);
}

// ── LES FENÊTRES : un onglet sait dans laquelle il vit
// ───────────────────

TEST_CASE("Onglets : détacher donne une fenêtre neuve", "[app][onglets]") {
  moonclient::ChatTabs tabs;
  const std::size_t second = tabs.Add("Guilde");

  const std::uint32_t window = tabs.Detach(second);
  CHECK(window != 0);
  CHECK(tabs.tabs()[second].group == window);
  // Le premier n'a pas bougé : détacher ne concerne que l'onglet visé.
  CHECK(tabs.tabs()[0].group == 0);
  // La principale est toujours de la liste, et elle vient d'abord.
  const std::vector<std::uint32_t> living = tabs.Groups();
  REQUIRE(living.size() == 2);
  CHECK(living[0] == 0);
  CHECK(living[1] == window);
}

TEST_CASE("Onglets : un numéro de fenêtre ne se réemploie jamais", "[app][onglets]") {
  // 🔴 IL NOMME LA FENÊTRE, donc ce qui lui garde sa position et sa taille. Un
  // numéro rendu ferait hériter la fenêtre neuve de la géométrie de l'ancienne
  // — elle s'ouvrirait ailleurs que là où le joueur vient de la lâcher.
  moonclient::ChatTabs tabs;
  const std::size_t second = tabs.Add("Guilde");

  const std::uint32_t first_window = tabs.Detach(second);
  tabs.Attach(second, 0);  // il revient à la principale : la fenêtre meurt
  const std::uint32_t second_window = tabs.Detach(second);
  CHECK(second_window != first_window);
}

TEST_CASE("Onglets : détacher ce qui est déjà seul ne consomme rien", "[app][onglets]") {
  moonclient::ChatTabs tabs;
  const std::size_t second = tabs.Add("Guilde");
  const std::uint32_t window = tabs.Detach(second);

  CHECK(tabs.Detach(second) == window);
  CHECK(tabs.Groups().size() == 2);
}

TEST_CASE("Onglets : plusieurs onglets partagent une fenêtre", "[app][onglets]") {
  // ⚠ C'EST CE QUI DISTINGUE UNE FENÊTRE D'UN ONGLET. Le client d'origine ne sait
  // que « dans la principale » ou « dans la sienne » ; ici une flottante se
  // peuple, comme les onglets d'un navigateur.
  moonclient::ChatTabs tabs;
  const std::size_t second = tabs.Add("Guilde");
  const std::size_t third = tabs.Add("Groupe");

  const std::uint32_t window = tabs.Detach(second);
  tabs.Attach(third, window);
  CHECK(tabs.tabs()[third].group == window);
  // Deux onglets, UNE fenêtre : la principale et elle.
  CHECK(tabs.Groups().size() == 2);
}

TEST_CASE("Onglets : on ne rejoint pas une fenêtre qui n'existe pas", "[app][onglets]") {
  // ⚠ SINON L'ONGLET DISPARAÎT SANS QUE RIEN NE LE DISE : il serait rangé dans
  // une fenêtre que personne ne dessine. Un numéro peut venir d'un fichier
  // retouché à la main, ou d'une fenêtre qui vient de se vider.
  moonclient::ChatTabs tabs;
  const std::size_t second = tabs.Add("Guilde");

  tabs.Attach(second, 4242);
  CHECK(tabs.tabs()[second].group == 0);
}

TEST_CASE("Onglets : la fenêtre d'un onglet survit à la partie", "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  {
    moonclient::ChatTabs tabs;
    const std::size_t second = tabs.Add("Guilde");
    const std::size_t third = tabs.Add("Groupe");
    const std::uint32_t window = tabs.Detach(second);
    tabs.Attach(third, window);
    REQUIRE(moonclient::SaveChatTabs(folder.path(), tabs));
  }

  moonclient::ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 3);
  CHECK(read.tabs()[0].group == 0);
  CHECK(read.tabs()[1].group != 0);
  // Les deux qui partageaient une flottante la partagent encore : c'est le
  // numéro qui les réunit, pas leur voisinage dans la liste.
  CHECK(read.tabs()[1].group == read.tabs()[2].group);

  // 🔴 ET LE COMPTEUR SE RELÈVE AU-DESSUS DE CE QUE LE FICHIER PORTE.
  // Repartir de un rendrait à la fenêtre neuve le numéro d'une fenêtre encore
  // ouverte, donc sa position et sa taille.
  const std::uint32_t fresh = read.Detach(0);
  CHECK(fresh > read.tabs()[1].group);
}

// ── L'APPARENCE PROPRE À UN ONGLET ───────────────────────────

TEST_CASE("Onglets : une apparence à lui survit à la partie", "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  {
    moonclient::ChatTabs tabs;
    const std::size_t second = tabs.Add("Guilde");
    tabs.SetLook(second, moonclient::TabLook{.own = true,
                                             .font_percent = 130,
                                             .padding_pixels = 8,
                                             .line_gap_pixels = 5,
                                             .body = 0x80102030U});
    REQUIRE(moonclient::SaveChatTabs(folder.path(), tabs));
  }

  moonclient::ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 2);
  // Le premier n'a rien demandé : il suit les réglages généraux.
  CHECK_FALSE(read.tabs()[0].look.own);
  const moonclient::TabLook& mine = read.tabs()[1].look;
  CHECK(mine.own);
  CHECK(mine.font_percent == 130);
  CHECK(mine.padding_pixels == 8);
  CHECK(mine.line_gap_pixels == 5);
  // ⚠ LA TRANSPARENCE AUSSI : c'est elle qu'on vient régler le plus souvent, et
  // un octet perdu à l'écriture rendrait le fond opaque à la relecture.
  CHECK(mine.body == 0x80102030U);
}

TEST_CASE("Onglets : ce qui suit les réglages généraux ne s'écrit pas", "[app][onglets]") {
  // 🔴 UN DRAPEAU, PAS UNE VALEUR PAR CHAMP. Écrire quatre valeurs mortes sous
  // un drapeau faux ferait croire qu'elles agissent — et un joueur qui retouche
  // le fichier à la main les changerait sans rien voir se produire.
  const ro::test_support::TempDirectory folder;
  moonclient::ChatTabs tabs;
  tabs.SetLook(0, moonclient::TabLook{.own = false, .font_percent = 130});
  REQUIRE(moonclient::SaveChatTabs(folder.path(), tabs));

  std::ifstream file(moonclient::ChatTabFilePath(folder.path()), std::ios::binary);
  std::ostringstream written;
  written << file.rdbuf();
  // ⚠ LA CLÉ, PAS LE MOT : l'en-tête du fichier EXPLIQUE ce qu'est l'apparence
  // d'un onglet, et chercher le mot nu le trouverait là.
  CHECK(written.str().find("look:") == std::string::npos);
}

// ── LES VINGT-HUIT CASES, ET LE FICHIER DES ONGLETS ──────────

TEST_CASE("Onglets : un onglet neuf a toutes ses cases cochées sauf Mes skills", "[app][onglets]") {
  ChatTabs tabs;
  const std::size_t born = tabs.Add("Neuf");
  for (std::size_t rank = 0; rank < ro::sim::kChatTypeCount; ++rank) {
    const auto type = static_cast<ChatType>(rank);
    CHECK(tabs.tabs()[born].Accepts(type) == (type != ChatType::kSkillUse));
  }
  CHECK(tabs.tabs()[born].accepted == kDefaultChatTypes);
  CHECK(kDefaultChatTypes == (EveryChatType() & ~MaskOf(ChatType::kSkillUse)));
}

TEST_CASE("Onglets : Mes skills cochée par le joueur se relit cochée", "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  ChatTabs written;
  written.SetAccepted(0, EveryChatType());
  REQUIRE(moonclient::SaveChatTabs(folder.path(), written));

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 1);
  CHECK(read.tabs()[0].Accepts(ChatType::kSkillUse));
}

TEST_CASE("Onglets : un fichier à vingt-sept cases lit Mes skills décochée", "[app][onglets]") {
  // 🔴 UN FICHIER D'AVANT LA CASE ne la connaît pas : son « Tout » entièrement
  // coché ne doit pas se mettre à montrer chaque sort armé ou lancé.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_tabs.yml",
                   "tabs:\n"
                   "  - name: Tout\n"
                   "    filter: [true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true, true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true, true]\n");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 1);
  CHECK(read.tabs()[0].accepted == kDefaultChatTypes);
  CHECK_FALSE(read.tabs()[0].Accepts(ChatType::kSkillUse));
  CHECK(read.tabs()[0].Accepts(ChatType::kDiscord));
}

TEST_CASE("Onglets : un onglet neuf s'appelle comme au client", "[app][onglets]") {
  // ⚠ NUMÉROTÉ À PARTIR DE DEUX, et le premier numéro libre : les homonymes
  // sont permis, mais en proposer un d'office serait désagréable.
  ChatTabs tabs;
  CHECK(moonclient::NewTabName(tabs.tabs()) == "NewTab_2");
  tabs.Add("NewTab_2");
  tabs.Add("NewTab_4");
  CHECK(moonclient::NewTabName(tabs.tabs()) == "NewTab_3");
}

TEST_CASE("Onglets : le fichier porte les vingt-huit cases sous des clés anglaises",
          "[app][onglets]") {
  const ro::test_support::TempDirectory folder;
  ChatTabs written;
  const std::size_t quests = written.Add("Quetes");
  written.SetAccepted(quests, MaskOf(ChatType::kQuestInfo) | MaskOf(ChatType::kBroadcast));
  REQUIRE(moonclient::SaveChatTabs(folder.path(), written));

  std::ifstream file(moonclient::ChatTabFilePath(folder.path()), std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  const std::string whole = text.str();
  CHECK(whole.find("\ntabs:") != std::string::npos);
  CHECK(whole.find("name: Quetes") != std::string::npos);
  // ⚠ LA LIGNE DES FILTRES DU SECOND ONGLET : vingt-huit valeurs, dont la
  // vingtième (quête) et la vingt-sixième (annonces) seules sont vraies.
  const std::size_t second = whole.find("filter:", whole.find("name: Quetes"));
  REQUIRE(second != std::string::npos);
  const std::string line = whole.substr(second, whole.find('\n', second) - second);
  CHECK(line ==
        "filter: [false, false, false, false, false, false, false, false, false, false, false, "
        "false, false, false, false, false, false, false, false, true, false, false, false, "
        "false, false, true, false, false]");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 2);
  CHECK(read.tabs()[1].accepted == (MaskOf(ChatType::kQuestInfo) | MaskOf(ChatType::kBroadcast)));
}

TEST_CASE("Onglets : un fichier aux anciennes clés rend les onglets d'origine", "[app][onglets]") {
  // 🔴 SES FILTRES NOMMAIENT D'AUTRES CASES QUE LES TYPES : les relire rangerait
  // chaque message ailleurs que là où le joueur l'attend. On repart de l'onglet
  // d'origine, sans planter.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_tabs.yml",
                   "genres_connus: [parole, serveur]\n"
                   "onglets:\n"
                   "  - nom: Systeme\n"
                   "    genres: [serveur, annonce]\n");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 1);
  CHECK(read.tabs()[0].accepted == kDefaultChatTypes);
}

TEST_CASE("Onglets : une liste de cases trop courte ou trop longue se relit", "[app][onglets]") {
  // ⚠ UNE CASE ABSENTE EST DÉCOCHÉE, une case en trop — qu'une version plus
  // récente aurait ajoutée — est ignorée : ni l'une ni l'autre ne coûte l'onglet.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_tabs.yml",
                   "tabs:\n"
                   "  - name: Court\n"
                   "    filter: [false, true, true]\n"
                   "  - name: Long\n"
                   "    filter: [true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true, true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true, true, true, true]\n");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 2);
  // Écrite avant le relai Discord, la liste courte lui donne la valeur du chat
  // public.
  CHECK(read.tabs()[0].accepted ==
        (MaskOf(ChatType::kPublicChat) | MaskOf(ChatType::kWhisper) | MaskOf(ChatType::kDiscord)));
  CHECK(read.tabs()[1].accepted == EveryChatType());
}

TEST_CASE("Onglets : un « Tout » à vingt-six cases montre le relai Discord", "[app][onglets]") {
  // 🔴 SANS CELA, L'ONGLET DU JOUEUR CACHERAIT LE RELAI qu'il n'a jamais
  // décoché : sa liste a été écrite avant que la case existe.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_tabs.yml",
                   "tabs:\n"
                   "  - name: Tout\n"
                   "    filter: [true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true, true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true]\n"
                   "  - name: Guilde\n"
                   "    filter: [false, false, false, false, true, false, false, false, false, "
                   "false, false, false, false, false, false, false, false, false, false, false, "
                   "false, false, false, false, false, false]\n"
                   "  - name: Choisi\n"
                   "    filter: [true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true, true, true, true, true, true, true, true, true, true, true, true, "
                   "true, true, false]\n");

  ChatTabs read;
  moonclient::LoadChatTabs(folder.path(), read);
  REQUIRE(read.count() == 3);
  CHECK(read.tabs()[0].accepted == kDefaultChatTypes);
  CHECK(read.tabs()[0].Accepts(ChatType::kDiscord));
  // Le chat public refusé, le relai l'est aussi.
  CHECK(read.tabs()[1].accepted == MaskOf(ChatType::kGuildChat));
  // ⚠ UNE CASE ÉCRITE SE RESPECTE : décochée par le joueur, elle le reste.
  CHECK_FALSE(read.tabs()[2].Accepts(ChatType::kDiscord));
  CHECK(read.tabs()[2].Accepts(ChatType::kPublicChat));
}

// ── CE QUI EST ARRIVÉ DEPUIS QU'ON A LU ──────────────────────

namespace {

[[nodiscard]] ro::sim::ChatLine LineOf(std::uint64_t seq, ChatType type) {
  ro::sim::ChatLine line;
  line.seq = seq;
  line.type = type;
  line.kind = ro::sim::ChatKind::kPublic;
  line.text = "Ana : salut";
  return line;
}

}  // namespace

TEST_CASE("Onglets : une ligne acceptée et pas encore lue marque l'onglet", "[app][onglets]") {
  ChatTabs tabs;
  const std::size_t guild = tabs.Add("Guilde");
  tabs.SetAccepted(guild, MaskOf(ChatType::kGuildChat));
  const std::vector<ro::sim::ChatLine> chat = {LineOf(1, ChatType::kGuildChat),
                                               LineOf(2, ChatType::kPublicChat)};

  // Jamais montré : la ligne de guilde n'a pas été lue.
  CHECK(moonclient::TabHasUnread(tabs.tabs()[guild], chat, nullptr));
  // Lu jusqu'à elle : la ligne publique qui suit n'est pas pour lui.
  tabs.MarkRead(guild, 1);
  CHECK_FALSE(moonclient::TabHasUnread(tabs.tabs()[guild], chat, nullptr));
  // Ce que l'onglet refuse ne le marque jamais.
  tabs.SetAccepted(guild, MaskOf(ChatType::kWhisper));
  tabs.MarkRead(guild, 0);
  CHECK_FALSE(moonclient::TabHasUnread(tabs.tabs()[guild], chat, nullptr));
}

TEST_CASE("Onglets : ce qui est vidé ou lu ne marque plus l'onglet", "[app][onglets]") {
  ChatTabs tabs;
  const std::vector<ro::sim::ChatLine> chat = {LineOf(3, ChatType::kPublicChat),
                                               LineOf(4, ChatType::kPublicChat)};
  CHECK(moonclient::TabHasUnread(tabs.tabs()[0], chat, nullptr));
  tabs.ClearThrough(0, 4);
  CHECK_FALSE(moonclient::TabHasUnread(tabs.tabs()[0], chat, nullptr));
  tabs.ClearThrough(0, 0);
  tabs.MarkRead(0, 4);
  CHECK_FALSE(moonclient::TabHasUnread(tabs.tabs()[0], chat, nullptr));
  // Lire n'est pas un changement à enregistrer : le rang ne franchit pas le
  // disque.
  const std::uint32_t at_rest = tabs.revision();
  tabs.MarkRead(0, 3);
  CHECK(tabs.revision() == at_rest);
}

TEST_CASE("Onglets : le compteur n'a jamais rien à lire", "[app][onglets]") {
  ChatTabs tabs;
  tabs.DockMeter("Dégâts", 0, tabs.count());
  const std::optional<std::size_t> meter = tabs.MeterRank();
  REQUIRE(meter.has_value());
  const std::vector<ro::sim::ChatLine> chat = {LineOf(1, ChatType::kPublicChat)};
  CHECK_FALSE(moonclient::TabHasUnread(tabs.tabs()[*meter], chat, nullptr));
}

TEST_CASE("Onglets : les types alimentés sont dits en un seul endroit", "[app][onglets]") {
  // Ce qu'un message alimente aujourd'hui ; le reste est en attente.
  CHECK(moonclient::IsFedChatType(ChatType::kPublicChat));
  CHECK(moonclient::IsFedChatType(ChatType::kSkillFailure));
  // Le refus d'équiper ou de retirer une pièce.
  CHECK(moonclient::IsFedChatType(ChatType::kEquipmentOnOff));
  CHECK(moonclient::IsFedChatType(ChatType::kPartyItem));
  CHECK(moonclient::IsFedChatType(ChatType::kDamagedEquipment));
  CHECK(moonclient::IsFedChatType(ChatType::kBroadcast));
  // La parole du champ de bataille, que les onglets montrent par défaut.
  CHECK(moonclient::IsFedChatType(ChatType::kBattlefield));
  // Le canal d'une alliance, que l'auteur de la parole distingue des autres.
  CHECK(moonclient::IsFedChatType(ChatType::kAllianceChat));
  // Les lignes de quête et les plafonds du tribut de guilde.
  CHECK(moonclient::IsFedChatType(ChatType::kQuestInfo));
  CHECK(moonclient::IsFedChatType(ChatType::kRepaymentExp));
  // Les altérations d'état, les siennes et celles du groupe.
  CHECK(moonclient::IsFedChatType(ChatType::kAbnormalStatus));
  CHECK(moonclient::IsFedChatType(ChatType::kPartyStatus));
  // Les annonces de siège.
  CHECK(moonclient::IsFedChatType(ChatType::kSiegeInfo));
  // Ce qu'apporte une pièce posée ou retirée.
  CHECK(moonclient::IsFedChatType(ChatType::kEquipAttributes));
  // Les libellés et les étiquettes existent pour chacun.
  for (std::size_t rank = 0; rank < ro::sim::kChatTypeCount; ++rank) {
    const auto type = static_cast<ChatType>(rank);
    CHECK(std::string(moonclient::ChatTypeLabel(type)).size() > 0);
    CHECK(moonclient::ChatTypeTag(type).size() == 3);
  }
  CHECK(moonclient::ChatTypeTag(ChatType::kBattle) == "t16");
  CHECK_FALSE(moonclient::ChatTypeOf(-1).has_value());
  CHECK_FALSE(moonclient::ChatTypeOf(static_cast<int>(ro::sim::kChatTypeCount)).has_value());
  CHECK(moonclient::ChatTypeOf(25) == ChatType::kBroadcast);
  CHECK(moonclient::ChatTypeOf(27) == ChatType::kSkillUse);
  CHECK(moonclient::ChatTypeTag(ChatType::kSkillUse) == "t27");
  CHECK(moonclient::IsFedChatType(ChatType::kSkillUse));
}
