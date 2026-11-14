// `/where` ET LES ANNONCES DU STAFF : ce que la ligne tapée demande, et le
// texte qui part.

#include "chat/announce_command.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::AnnounceCommandKind;
using moonclient::AnnouncedText;
using moonclient::ReadAnnounceCommand;
using moonclient::WhereLine;

TEST_CASE("Annonces : chaque commande et sa portée", "[app][chat][annonce]") {
  CHECK(ReadAnnounceCommand("/where").kind == AnnounceCommandKind::kWhere);
  CHECK(ReadAnnounceCommand("/WHERE").kind == AnnounceCommandKind::kWhere);
  CHECK(ReadAnnounceCommand("/b salut").kind == AnnounceCommandKind::kBroadcast);
  CHECK(ReadAnnounceCommand("/nb salut").kind == AnnounceCommandKind::kBroadcast);
  CHECK(ReadAnnounceCommand("/lb salut").kind == AnnounceCommandKind::kLocalBroadcast);
  CHECK(ReadAnnounceCommand("/nlb salut").kind == AnnounceCommandKind::kLocalBroadcast);
}

TEST_CASE("Annonces : la commande s'arrête à l'espace", "[app][chat][annonce]") {
  // ⚠ « /bonjour » n'est pas « /b » : il retombe sur la suite du routage.
  CHECK(ReadAnnounceCommand("/bonjour").kind == AnnounceCommandKind::kNone);
  CHECK(ReadAnnounceCommand("/lbx a").kind == AnnounceCommandKind::kNone);
  CHECK(ReadAnnounceCommand("bonjour").kind == AnnounceCommandKind::kNone);
}

TEST_CASE("Annonces : le nom précède le message, sauf aux formes en n", "[app][chat][annonce]") {
  CHECK(AnnouncedText(ReadAnnounceCommand("/b Event à 21h"), "Sting") == "Sting : Event à 21h");
  CHECK(AnnouncedText(ReadAnnounceCommand("/nb Event à 21h"), "Sting") == "Event à 21h");
  CHECK(AnnouncedText(ReadAnnounceCommand("/lb ici"), "Sting") == "Sting : ici");
  CHECK(AnnouncedText(ReadAnnounceCommand("/nlb ici"), "Sting") == "ici");
  // Rien après la commande : rien à annoncer, pas même le nom.
  CHECK(AnnouncedText(ReadAnnounceCommand("/b"), "Sting").empty());
}

TEST_CASE("Annonces : /where écrit la carte puis la case", "[app][chat][annonce]") {
  CHECK(WhereLine("prontera", 150, 180) == "prontera 150 180");
}
