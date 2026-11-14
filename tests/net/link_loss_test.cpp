// La perte de liaison : la cause dite au joueur, et le verrou qui la tient.

#include "ro/net/link_loss.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/net/refusal_text.h"

#include "support/temp_directory.h"

using ro::net::EvictionText;
using ro::net::LinkLoss;
using ro::net::LinkLossCause;
using ro::net::LinkLossCauseOf;
using ro::net::LinkLossText;
using ro::net::LinkStatus;

namespace {

// « quelqu'un vient de se connecter avec ce compte » : le motif le plus
// fréquent d'une fin de session décidée par le serveur.
constexpr std::uint8_t kBanAlreadyLoggedIn = 2;

}  // namespace

TEST_CASE("Perte de liaison : chaque état fermé désigne sa cause", "[net][liaison]") {
  CHECK(LinkLossCauseOf(LinkStatus::kClosedByPeer) == LinkLossCause::kClosedByPeer);
  CHECK(LinkLossCauseOf(LinkStatus::kProtocolError) == LinkLossCause::kProtocolError);
  CHECK(LinkLossCauseOf(LinkStatus::kIoError) == LinkLossCause::kIoError);
  // ⚠ UNE LIAISON OUVERTE N'EST PAS UN SILENCE : la cause la plus générale.
  CHECK(LinkLossCauseOf(LinkStatus::kOpen) == LinkLossCause::kIoError);
}

TEST_CASE("Perte de liaison : chaque cause a sa phrase, et aucune ne se ressemble",
          "[net][liaison]") {
  const std::set<std::string> phrases = {
      LinkLossText(LinkLossCause::kClosedByPeer), LinkLossText(LinkLossCause::kSilent),
      LinkLossText(LinkLossCause::kProtocolError), LinkLossText(LinkLossCause::kIoError),
      LinkLossText(LinkLossCause::kZoneMoveFailed)};
  constexpr std::size_t kCauseCount = 5;
  CHECK(phrases.size() == kCauseCount);
  CHECK(LinkLossText(LinkLossCause::kSilent) == "Le serveur ne répond plus.");
  CHECK(LinkLossText(LinkLossCause::kClosedByPeer) == "Le serveur a fermé la connexion.");
}

TEST_CASE("Perte de liaison : une éviction dit le motif du serveur", "[net][liaison]") {
  const std::string evicted = EvictionText(kBanAlreadyLoggedIn);
  CHECK(evicted.find(ro::net::BanText(kBanAlreadyLoggedIn)) != std::string::npos);
}

TEST_CASE("Perte de liaison : la première cause gagne", "[net][liaison]") {
  LinkLoss loss;
  CHECK_FALSE(loss.declared());
  CHECK(loss.Declare("Motif : le serveur ferme."));
  // ⚠ LA FERMETURE QUI SUIT L'ÉVICTION NE REMPLACE PAS SON MOTIF.
  CHECK_FALSE(loss.Declare("Le serveur a fermé la connexion."));
  CHECK(loss.text() == "Déconnecté du serveur.\nMotif : le serveur ferme.");
}

TEST_CASE("Perte de liaison : le motif ne part vers la connexion qu'une fois acquitté",
          "[net][liaison]") {
  // 🔴 LE JOUEUR QUI FERME LA FENÊTRE PENDANT LA BOÎTE VEUT QUITTER : sans
  // acquittement, aucun motif, donc aucun retour au formulaire.
  LinkLoss loss;
  loss.Acknowledge();
  CHECK_FALSE(loss.acknowledged());

  REQUIRE(loss.Declare("Le serveur ne répond plus."));
  CHECK(loss.LoginNotice().empty());

  loss.Acknowledge();
  CHECK(loss.acknowledged());
  CHECK(loss.LoginNotice() == "Déconnecté du serveur.\nLe serveur ne répond plus.");
}

TEST_CASE("Perte de liaison : un gabarit traduit de travers ne fait pas tomber le client",
          "[net][liaison]") {
  // 🔴 LE CATALOGUE SE MODIFIE À LA MAIN, et l'éviction est le pire moment pour
  // planter : c'est le seul où le joueur apprend pourquoi la session finit.
  const std::string why = ro::net::BanText(kBanAlreadyLoggedIn);
  const ro::test_support::TempDirectory folder;
  folder.WriteText("fautif.txt", "fr: Motif : {}.\ntr: Reason: {0:%}} {.\n");
  REQUIRE(ro::core::LoadLanguage(folder.path() / "fautif.txt", "xx").has_value());
  std::string broken;
  CHECK_NOTHROW(broken = EvictionText(kBanAlreadyLoggedIn));
  folder.WriteText("sans_trou.txt", "fr: Motif : {}.\ntr: Reason:\n");
  REQUIRE(ro::core::LoadLanguage(folder.path() / "sans_trou.txt", "xx").has_value());
  const std::string holeless = EvictionText(kBanAlreadyLoggedIn);
  ro::core::UseFrench();
  // LE MOTIF SURVIT AUX DEUX : on ne perd jamais ce que le serveur a dit.
  CHECK(broken.find(why) != std::string::npos);
  CHECK(holeless == "Reason: " + why);
  CHECK(EvictionText(kBanAlreadyLoggedIn) == "Motif : " + why + ".");
}
