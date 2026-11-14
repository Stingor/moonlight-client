// L'historique de conversation sur le disque : ce qu'il garde, et ce qu'il jette.

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "chat/chat_history_file.h"
#include "support/temp_directory.h"

namespace {

using moonclient::LoadChatHistory;
using moonclient::PastChat;
using moonclient::PastChatLine;
using moonclient::SaveChatHistory;
using moonclient::WhisperPeer;
using ro::sim::ChatType;

[[nodiscard]] PastChatLine Said(ChatType type, std::string text, std::uint8_t hour) {
  return PastChatLine{
      .type = type, .text = std::move(text), .hour = hour, .minute = 30, .second = 5};
}

}  // namespace

TEST_CASE("Historique : il se relit tel qu'on l'a écrit", "[app][historique]") {
  const ro::test_support::TempDirectory folder;
  const std::vector<PastChatLine> written = {Said(ChatType::kPublicChat, "bonsoir", 21),
                                             Said(ChatType::kGuildChat, "on y va", 22)};
  REQUIRE(SaveChatHistory(folder.path(), written, 100, "2026-09-15 22:30:05", {}));

  const PastChat read = LoadChatHistory(folder.path());
  REQUIRE(read.lines.size() == 2);
  CHECK(read.saved_at == "2026-09-15 22:30:05");
  CHECK(read.lines[0].type == ChatType::kPublicChat);
  CHECK(read.lines[0].text == "bonsoir");
  // ⚠ L'HEURE EST ÉCRITE, PAS DÉDUITE : une ligne d'hier soir n'a pas d'horloge
  // de boucle à consulter, et la recalculer lui donnerait une heure
  // d'aujourd'hui.
  CHECK(read.lines[0].hour == 21);
  CHECK(read.lines[0].minute == 30);
  CHECK(read.lines[0].second == 5);
  CHECK(read.lines[1].type == ChatType::kGuildChat);
}

TEST_CASE("Historique : seules les dernières lignes sont gardées", "[app][historique]") {
  const ro::test_support::TempDirectory folder;
  std::vector<PastChatLine> written;
  for (int rank = 0; rank < 60; ++rank) {
    written.push_back(Said(ChatType::kPublicChat, "ligne " + std::to_string(rank), 12));
  }
  // Le plancher du réglage : en deçà de vingt, la relecture n'apprend rien.
  REQUIRE(SaveChatHistory(folder.path(), written, moonclient::kSmallestChatHistory, "hier", {}));

  const PastChat read = LoadChatHistory(folder.path());
  REQUIRE(read.lines.size() == static_cast<std::size_t>(moonclient::kSmallestChatHistory));
  // Ce sont les DERNIÈRES : garder le début d'une soirée et jeter sa fin serait
  // exactement l'inverse de ce qu'on relit un historique pour trouver.
  CHECK(read.lines.front().text == "ligne 40");
  CHECK(read.lines.back().text == "ligne 59");
}

TEST_CASE("Historique : un compte hors bornes est ramené dedans", "[app][historique]") {
  // Il peut venir d'un fichier de réglages corrigé à la main.
  const ro::test_support::TempDirectory folder;
  std::vector<PastChatLine> written;
  for (int rank = 0; rank < 40; ++rank) {
    written.push_back(Said(ChatType::kPublicChat, "x", 12));
  }
  REQUIRE(SaveChatHistory(folder.path(), written, -5, "hier", {}));
  CHECK(LoadChatHistory(folder.path()).lines.size() ==
        static_cast<std::size_t>(moonclient::kSmallestChatHistory));
}

TEST_CASE("Historique : une ligne sans type valable est ignorée", "[app][historique]") {
  // ⚠ IGNORÉE, PAS RATTACHÉE D'OFFICE À UN TYPE PAR DÉFAUT : elle ne sait plus
  // dans quel onglet aller, et la poser partout serait pire que de ne pas la
  // poser. Une ligne aux anciennes clés n'a pas de type ; un numéro hors des
  // vingt-huit cases n'en désigne aucun.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_history.yml",
                   "recipients: [Bob]\n"
                   "lines:\n"
                   "  - genre: parole\n"
                   "    time: \"21:00:00\"\n"
                   "    text: ancienne\n"
                   "  - type: 28\n"
                   "    time: \"21:00:01\"\n"
                   "    text: hors bornes\n"
                   "  - type: 4\n"
                   "    time: \"21:00:02\"\n"
                   "    text: gardee\n");

  const PastChat read = LoadChatHistory(folder.path());
  REQUIRE(read.lines.size() == 1);
  CHECK(read.lines[0].text == "gardee");
  CHECK(read.lines[0].type == ChatType::kGuildChat);
  // ⚠ LE RESTE DU FICHIER SURVIT : seules les lignes s'en vont.
  REQUIRE(read.whispered.size() == 1);
  CHECK(read.whispered[0].name == "Bob");
}

TEST_CASE("Historique : un fichier abîmé ne coûte pas la partie", "[app][historique]") {
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_history.yml", "lines:\n  - [\n");
  CHECK(LoadChatHistory(folder.path()).lines.empty());
}

TEST_CASE("Historique : un fichier absent n'est pas une panne", "[app][historique]") {
  const ro::test_support::TempDirectory folder;
  const PastChat read = LoadChatHistory(folder.path());
  CHECK(read.lines.empty());
  CHECK(read.saved_at.empty());
}

TEST_CASE("Historique : l'oublier efface le fichier", "[app][historique]") {
  // ⚠ C'EST LE GESTE DE QUI DÉCOCHE L'OPTION. Garder sur le disque ce qu'il
  // vient de refuser serait l'inverse de ce qu'il demande — et ce fichier-là
  // porte ses chuchotements en clair.
  const ro::test_support::TempDirectory folder;
  REQUIRE(SaveChatHistory(folder.path(), {Said(ChatType::kWhisper, "secret", 23)}, 50, "hier", {}));
  REQUIRE(std::filesystem::exists(moonclient::ChatHistoryPath(folder.path())));

  moonclient::ForgetChatHistory(folder.path());
  CHECK_FALSE(std::filesystem::exists(moonclient::ChatHistoryPath(folder.path())));
  // Et l'effacer deux fois ne se plaint pas : le fichier peut n'avoir jamais
  // existé.
  moonclient::ForgetChatHistory(folder.path());
}

TEST_CASE("Historique : les destinataires survivent avec le reste", "[app][historique]") {
  // ⚠ DANS LE MÊME FICHIER ET SOUS LA MÊME OPTION : savoir À QUI l'on parle en
  // privé n'est pas moins sensible que ce qu'on leur dit, et deux cases à
  // décocher pour une seule question laisseraient une chance d'en oublier une.
  const ro::test_support::TempDirectory folder;
  const std::vector<WhisperPeer> names = {WhisperPeer{.name = "Bob", .incoming = true},
                                          WhisperPeer{.name = "Alice"}};
  REQUIRE(
      SaveChatHistory(folder.path(), {Said(ChatType::kWhisper, "a plus", 22)}, 50, "hier", names));

  const PastChat read = LoadChatHistory(folder.path());
  REQUIRE(read.whispered.size() == 2);
  // Le plus récent en tête, comme la liste déroulante les montre, et le sens
  // du dernier échange avec lui.
  CHECK(read.whispered[0] == names[0]);
  CHECK(read.whispered[1] == names[1]);
}

TEST_CASE("Historique : une liste de destinataires d'avant se relit en « envoyé »",
          "[app][historique]") {
  // ⚠ LE FICHIER D'UNE VERSION QUI NE GARDAIT PAS LE SENS : sa liste ne nommait
  // que ceux à qui l'on avait chuchoté, et c'est ce qu'elle doit redire.
  const ro::test_support::TempDirectory folder;
  folder.WriteText(std::filesystem::path("savedata") / "chat_history.yml",
                   "saved_at: hier\n"
                   "recipients: [Bob, Élodie, 김철수]\n");

  const PastChat read = LoadChatHistory(folder.path());
  REQUIRE(read.whispered.size() == 3);
  CHECK(read.whispered[0] == WhisperPeer{.name = "Bob"});
  CHECK(read.whispered[1] == WhisperPeer{.name = "Élodie"});
  CHECK(read.whispered[2] == WhisperPeer{.name = "김철수"});
}

TEST_CASE("Historique : le sens s'écrit à part, la liste des noms garde sa forme",
          "[app][historique]") {
  // ⚠ UNE VERSION D'AVANT RELIT ENCORE LES NOMS : elle ignore la clé qu'elle ne
  // connaît pas.
  const ro::test_support::TempDirectory folder;
  const std::vector<WhisperPeer> names = {WhisperPeer{.name = "Bob", .incoming = true},
                                          WhisperPeer{.name = "Alice"}};
  REQUIRE(SaveChatHistory(folder.path(), {}, 50, "hier", names));

  std::ifstream file(moonclient::ChatHistoryPath(folder.path()), std::ios::binary);
  std::ostringstream written;
  written << file.rdbuf();
  CHECK(written.str().find("recipients: [Bob, Alice]") != std::string::npos);
  CHECK(written.str().find("heard_from: [Bob]") != std::string::npos);
}

TEST_CASE("Historique : les lignes TAPÉES survivent dans l'ordre", "[app][historique]") {
  // Ce que la flèche ↑ rappellera au démarrage suivant. L'ordre compte : la
  // plus récente en dernier, comme la barre les empile.
  const ro::test_support::TempDirectory folder;
  const std::vector<std::string> typed = {"@load prontera", "@effect 0", "bonsoir"};
  REQUIRE(SaveChatHistory(folder.path(), {Said(ChatType::kPublicChat, "bonsoir", 21)}, 50, "hier",
                          {}, typed));

  const PastChat read = LoadChatHistory(folder.path());
  REQUIRE(read.typed.size() == 3);
  CHECK(read.typed[0] == "@load prontera");
  CHECK(read.typed[1] == "@effect 0");
  CHECK(read.typed[2] == "bonsoir");
}

TEST_CASE("Historique : les lignes tapées tiennent SANS conversation", "[app][historique]") {
  // 🔴 LE CAS QUI SÉPARE LES DEUX ÉCRITURES, et c'est celui du réglage par
  // défaut : la conversation est décochée, les lignes tapées ne le sont pas. Un
  // écrivain qui renoncerait devant une liste de lignes vide — ou un lecteur qui
  // s'arrêterait avant la clé — perdrait exactement ce que le joueur a demandé
  // de garder, sans que rien ne le dise.
  const ro::test_support::TempDirectory folder;
  const std::vector<std::string> typed = {"@go 0"};
  REQUIRE(SaveChatHistory(folder.path(), {}, 50, "hier", {}, typed));

  const PastChat read = LoadChatHistory(folder.path());
  CHECK(read.lines.empty());
  REQUIRE(read.typed.size() == 1);
  CHECK(read.typed[0] == "@go 0");
}

TEST_CASE("Historique : sans ligne tapée, la clé ne paraît pas", "[app][historique]") {
  const ro::test_support::TempDirectory folder;
  REQUIRE(
      SaveChatHistory(folder.path(), {Said(ChatType::kPublicChat, "bonsoir", 21)}, 50, "hier", {}));

  std::ifstream file(moonclient::ChatHistoryPath(folder.path()), std::ios::binary);
  std::ostringstream written;
  written << file.rdbuf();
  CHECK(written.str().find("typed:") == std::string::npos);
  CHECK(LoadChatHistory(folder.path()).typed.empty());
}

TEST_CASE("Historique : sans destinataire, la clé ne paraît pas", "[app][historique]") {
  // Une clé vide dans un fichier que le joueur peut ouvrir se lit comme une
  // liste qu'on aurait perdue.
  const ro::test_support::TempDirectory folder;
  REQUIRE(
      SaveChatHistory(folder.path(), {Said(ChatType::kPublicChat, "bonsoir", 21)}, 50, "hier", {}));

  std::ifstream file(moonclient::ChatHistoryPath(folder.path()), std::ios::binary);
  std::ostringstream written;
  written << file.rdbuf();
  CHECK(written.str().find("recipients:") == std::string::npos);
  CHECK(LoadChatHistory(folder.path()).whispered.empty());
}
