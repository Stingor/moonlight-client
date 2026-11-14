// Le journal de conversation sur le disque : ce qu'il écrit, et où.

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "chat/chat_log_file.h"
#include "support/temp_directory.h"

namespace {

using moonclient::ChatLogDump;
using moonclient::ChatLogPart;
using moonclient::ChatLogWriter;
using moonclient::ChatTab;
using moonclient::DumpChatLogs;
using moonclient::MaskOf;
using moonclient::PastChatLine;
using ro::sim::ChatType;

[[nodiscard]] PastChatLine Said(ChatType type, std::string text, std::uint8_t hour) {
  return PastChatLine{
      .type = type, .text = std::move(text), .hour = hour, .minute = 7, .second = 3};
}

[[nodiscard]] ChatTab Tab(std::string name, moonclient::ChatTypeMask accepted) {
  return ChatTab{.name = std::move(name), .accepted = accepted};
}

// Le contenu brut d'un fichier, BOM compris : c'est justement lui qu'on vérifie.
[[nodiscard]] std::string RawContent(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

}  // namespace

TEST_CASE("Journal : un nom d'onglet ne sort pas de son dossier", "[app][journal]") {
  // ⚠ LE NOM EST LIBRE : le joueur l'écrit. Écrire hors du dossier voulu à cause
  // d'un nom d'onglet serait le genre de faute qu'on ne remarque qu'une fois le
  // fichier ailleurs.
  CHECK(moonclient::SafeFileStem("Guilde") == "Guilde");
  // Ce qui compte n'est pas que les points disparaissent — ils sont légitimes
  // dans un nom — mais qu'aucun SÉPARATEUR ne survive : sans lui, « .. » ne
  // remonte nulle part.
  CHECK(moonclient::SafeFileStem("..\\..\\Windows") == ".._.._Windows");
  CHECK(moonclient::SafeFileStem("../../etc") == ".._.._etc");
  CHECK(moonclient::SafeFileStem("a:b*c?d") == "a_b_c_d");
  // Un nom qui finit par un espace ou un point n'est pas créable sous Windows,
  // et le fichier rendu serait alors silencieusement celui d'à côté.
  CHECK(moonclient::SafeFileStem("Commerce . ") == "Commerce");
  // Et il reste toujours un nom : un onglet nommé « ... » n'en laisse rien.
  CHECK(moonclient::SafeFileStem("...") == "chat");
  CHECK(moonclient::SafeFileStem("") == "chat");
}

TEST_CASE("Journal : un fichier par onglet, filtré comme à l'écran", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::vector<ChatTab> tabs = {
      Tab("Tout", MaskOf(ChatType::kPublicChat) | MaskOf(ChatType::kGuildChat) |
                      MaskOf(ChatType::kWhisper)),
      Tab("Guilde", MaskOf(ChatType::kGuildChat))};
  const std::vector<PastChatLine> lines = {Said(ChatType::kPublicChat, "bonsoir", 21),
                                           Said(ChatType::kGuildChat, "on y va", 22),
                                           Said(ChatType::kWhisper, "secret", 23)};
  const std::array<ChatLogPart, 1> parts = {ChatLogPart{.heading = {}, .lines = lines}};

  const ChatLogDump done = DumpChatLogs(folder.path(), tabs, parts);
  REQUIRE(done.files == 2);
  CHECK(done.folder == folder.path() / "Chat");

  const std::string all = RawContent(done.folder / "Chat_Tout.txt");
  const std::string guild = RawContent(done.folder / "Chat_Guilde.txt");
  CHECK(all.find("bonsoir") != std::string::npos);
  CHECK(all.find("secret") != std::string::npos);
  // ⚠ CE QUE L'ONGLET MONTRE, ET RIEN DE PLUS : ce que le joueur enregistre doit
  // être ce qu'il lit.
  CHECK(guild.find("on y va") != std::string::npos);
  CHECK(guild.find("bonsoir") == std::string::npos);
}

TEST_CASE("Journal : le fichier se lit hors du jeu", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::vector<ChatTab> tabs = {Tab("Tout", MaskOf(ChatType::kPublicChat))};
  const std::vector<PastChatLine> lines = {Said(ChatType::kPublicChat, "été", 9)};
  const std::array<ChatLogPart, 1> parts = {ChatLogPart{.heading = {}, .lines = lines}};

  REQUIRE(DumpChatLogs(folder.path(), tabs, parts).files == 1);
  const std::string content = RawContent(folder.path() / "Chat" / "Chat_Tout.txt");
  // 🔴 LE BOM UTF-8 : sans lui, le Bloc-notes lit nos lignes dans l'encodage du
  // système, et tous les accents sortent en charabia.
  CHECK(content.starts_with("\xEF\xBB\xBF"));
  // ⚠ CRLF, même sur le système qui n'en a pas besoin : ce fichier est fait pour
  // être ouvert par le joueur, avec l'éditeur qu'il a sous la main.
  CHECK(content.ends_with("\r\n"));
  // L'heure en tête, parce qu'un journal sans heure ne se recoupe avec rien.
  CHECK(content.find("[09:07:03] été") != std::string::npos);
}

TEST_CASE("Journal : un onglet sans ligne n'a pas de fichier", "[app][journal]") {
  // Un fichier vide n'apprend rien, et se confond avec une écriture qui a raté.
  const ro::test_support::TempDirectory folder;
  const std::vector<ChatTab> tabs = {Tab("Groupe", MaskOf(ChatType::kPartyChat))};
  const std::vector<PastChatLine> lines = {Said(ChatType::kPublicChat, "bonsoir", 21)};
  const std::array<ChatLogPart, 1> parts = {ChatLogPart{.heading = {}, .lines = lines}};

  CHECK(DumpChatLogs(folder.path(), tabs, parts).files == 0);
  CHECK_FALSE(std::filesystem::exists(folder.path() / "Chat" / "Chat_Groupe.txt"));
}

TEST_CASE("Journal : deux enregistrements ne s'écrasent pas", "[app][journal]") {
  // ⚠ C'EST TYPIQUEMENT CE QU'ON FAIT AVANT ET APRÈS UN ÉVÈNEMENT. La rotation
  // est celle du client : le nom simple, puis _001, _002…
  const ro::test_support::TempDirectory folder;
  const std::vector<ChatTab> tabs = {Tab("Tout", MaskOf(ChatType::kPublicChat))};
  const std::vector<PastChatLine> first = {Said(ChatType::kPublicChat, "avant", 20)};
  const std::vector<PastChatLine> second = {Said(ChatType::kPublicChat, "après", 23)};

  const std::array<ChatLogPart, 1> one = {ChatLogPart{.heading = {}, .lines = first}};
  REQUIRE(DumpChatLogs(folder.path(), tabs, one).files == 1);
  const std::array<ChatLogPart, 1> two = {ChatLogPart{.heading = {}, .lines = second}};
  REQUIRE(DumpChatLogs(folder.path(), tabs, two).files == 1);

  CHECK(RawContent(folder.path() / "Chat" / "Chat_Tout.txt").find("avant") != std::string::npos);
  CHECK(RawContent(folder.path() / "Chat" / "Chat_Tout_001.txt").find("après") !=
        std::string::npos);
}

TEST_CASE("Journal : le repère de session coiffe son bloc", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::vector<ChatTab> tabs = {Tab("Tout", MaskOf(ChatType::kPublicChat)),
                                     Tab("Guilde", MaskOf(ChatType::kGuildChat))};
  const std::vector<PastChatLine> past = {Said(ChatType::kPublicChat, "hier", 23),
                                          Said(ChatType::kGuildChat, "hier aussi", 23)};
  const std::vector<PastChatLine> now = {Said(ChatType::kPublicChat, "ce soir", 21)};
  const std::array<ChatLogPart, 2> parts = {
      ChatLogPart{.heading = "---- session du 2026-09-14 ----", .lines = past},
      ChatLogPart{.heading = {}, .lines = now}};

  REQUIRE(DumpChatLogs(folder.path(), tabs, parts).files == 2);
  const std::string all = RawContent(folder.path() / "Chat" / "Chat_Tout.txt");
  // ⚠ IL PARAÎT DANS TOUS LES ONGLETS quel que soit leur filtre : il ne décrit
  // pas la conversation, il décrit le journal. Sans lui, « 23:07 » se lirait
  // comme ce soir.
  CHECK(all.find("---- session du 2026-09-14 ----") != std::string::npos);
  CHECK(all.find("session") < all.find("hier"));
  CHECK(RawContent(folder.path() / "Chat" / "Chat_Guilde.txt")
            .find("---- session du 2026-09-14 ----") != std::string::npos);
}

TEST_CASE("Journal : un repère sans rien à coiffer ne s'écrit pas", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::vector<ChatTab> tabs = {Tab("Groupe", MaskOf(ChatType::kPartyChat))};
  const std::vector<PastChatLine> past = {Said(ChatType::kPublicChat, "hier", 23)};
  const std::vector<PastChatLine> now = {Said(ChatType::kPartyChat, "ce soir", 21)};
  const std::array<ChatLogPart, 2> parts = {
      ChatLogPart{.heading = "---- session du 2026-09-14 ----", .lines = past},
      ChatLogPart{.heading = {}, .lines = now}};

  REQUIRE(DumpChatLogs(folder.path(), tabs, parts).files == 1);
  const std::string content = RawContent(folder.path() / "Chat" / "Chat_Groupe.txt");
  // Le bloc d'hier n'a rien donné à cet onglet-là : son repère seul ne dirait
  // rien de ce qui suit.
  CHECK(content.find("session") == std::string::npos);
  CHECK(content.find("ce soir") != std::string::npos);
}

TEST_CASE("Journal continu : rien de dit, rien sur le disque", "[app][journal]") {
  // ⚠ UNE PARTIE OÙ PERSONNE NE PARLE NE LAISSE RIEN. Ouvrir le fichier à la mise
  // en service donnerait un fichier vide de plus à chaque lancement, et un repère
  // de session qui ne coiffe rien.
  const ro::test_support::TempDirectory folder;
  ChatLogWriter writer;
  writer.Open(folder.path(), "2026-09-15", "---- session du 2026-09-15 ----");
  CHECK(writer.open());
  CHECK(writer.path().empty());
  CHECK_FALSE(std::filesystem::exists(folder.path() / "Chat"));
}

TEST_CASE("Journal continu : minuit passé, le jour suivant a son fichier", "[app][journal]") {
  // 🔴 LA VUE ROUVRE LE JOURNAL AU CHANGEMENT DE DATE, et c'est le même
  // écrivain qui sert : le fichier de la veille se ferme, celui du jour
  // s'ouvre à la ligne suivante, coiffé de son repère.
  const ro::test_support::TempDirectory folder;
  ChatLogWriter writer;
  writer.Open(folder.path(), "2026-09-15", "---- session du 2026-09-15 23:58:00 ----");
  writer.Write(Said(ChatType::kPublicChat, "avant minuit", 21));
  CHECK(writer.day() == "2026-09-15");
  writer.Open(folder.path(), "2026-09-16", "---- session du 2026-09-16 00:00:01 ----");
  writer.Write(Said(ChatType::kPublicChat, "après minuit", 22));
  CHECK(writer.day() == "2026-09-16");

  const std::string eve = RawContent(folder.path() / "Chat" / "ChatLog_2026-09-15.txt");
  const std::string day = RawContent(folder.path() / "Chat" / "ChatLog_2026-09-16.txt");
  CHECK(eve.find("avant minuit") != std::string::npos);
  CHECK(eve.find("après minuit") == std::string::npos);
  CHECK(day.find("---- session du 2026-09-16") < day.find("après minuit"));
}

TEST_CASE("Journal continu : il s'ajoute, et son BOM ne revient pas", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  {
    ChatLogWriter writer;
    // ⚠ LE DOSSIER SE CRÉE RÉCURSIVEMENT, à la première ligne : c'est le `_mkdir`
    // non récursif du client qui rend son /savechat muet quand un intermédiaire
    // manque.
    writer.Open(folder.path() / "jeu", "2026-09-15", "---- session du 2026-09-15 ----");
    writer.Write(Said(ChatType::kPublicChat, "première", 21));
    REQUIRE_FALSE(writer.path().empty());
  }
  {
    ChatLogWriter writer;
    writer.Open(folder.path() / "jeu", "2026-09-15", "---- reprise ----");
    writer.Write(Said(ChatType::kGuildChat, "seconde", 22));
  }

  const std::string content = RawContent(folder.path() / "jeu" / "Chat" / "ChatLog_2026-09-15.txt");
  CHECK(content.starts_with("\xEF\xBB\xBF"));
  // ⚠ UNE SEULE FOIS : remis en rouvrant, le BOM paraîtrait au MILIEU du
  // fichier, où il n'est plus une marque d'encodage mais un caractère invisible
  // que l'éditeur montre.
  CHECK(content.find("\xEF\xBB\xBF", 1) == std::string::npos);
  // Et la journée se suit : deux parties dans le même fichier, séparées par leur
  // repère.
  CHECK(content.find("première") < content.find("---- reprise ----"));
  CHECK(content.find("---- reprise ----") < content.find("seconde"));
}

TEST_CASE("Journal continu : fermé, il n'écrit plus", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  ChatLogWriter writer;
  writer.Open(folder.path(), "2026-09-15", {});
  writer.Write(Said(ChatType::kPublicChat, "gardée", 21));
  writer.Close();
  CHECK_FALSE(writer.open());
  // ⚠ SANS SE PLAINDRE : l'appelant écrit à chaque ligne moissonnée et n'a pas à
  // savoir si l'option est cochée.
  writer.Write(Said(ChatType::kPublicChat, "perdue", 21));

  const std::string content = RawContent(folder.path() / "Chat" / "ChatLog_2026-09-15.txt");
  CHECK(content.find("gardée") != std::string::npos);
  CHECK(content.find("perdue") == std::string::npos);
}

TEST_CASE("Journal : le fil d'un salon a son fichier, qui le nomme", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::vector<PastChatLine> lines = {Said(ChatType::kPublicChat, "Alice : bonsoir", 21),
                                           Said(ChatType::kPublicChat, "Bob : salut", 22)};

  const ChatLogDump done = moonclient::DumpChatRoomLog(folder.path(), "Vente/achat", lines);
  REQUIRE(done.files == 1);
  CHECK(done.folder == folder.path() / "Chat");
  // ⚠ LE TITRE EST LIBRE, comme un nom d'onglet : sa barre oblique ne mène
  // nulle part ailleurs.
  const std::string room = RawContent(done.folder / "Chat_Salon_Vente_achat.txt");
  CHECK(room.starts_with("\xEF\xBB\xBF"));
  CHECK(room.find("[21:07:03] Alice : bonsoir\r\n") != std::string::npos);
  CHECK(room.find("[22:07:03] Bob : salut\r\n") != std::string::npos);
  // Et il tourne comme les autres : un second enregistrement n'écrase rien.
  REQUIRE(moonclient::DumpChatRoomLog(folder.path(), "Vente/achat", lines).files == 1);
  CHECK(std::filesystem::exists(done.folder / "Chat_Salon_Vente_achat_001.txt"));
}

TEST_CASE("Journal : un salon sans ligne n'a pas de fichier", "[app][journal]") {
  const ro::test_support::TempDirectory folder;
  CHECK(moonclient::DumpChatRoomLog(folder.path(), "Calme", {}).files == 0);
  CHECK_FALSE(std::filesystem::exists(folder.path() / "Chat" / "Chat_Salon_Calme.txt"));
}
