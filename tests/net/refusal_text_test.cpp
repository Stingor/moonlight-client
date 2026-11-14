#include "ro/net/refusal_text.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::net::BanText;
using ro::net::LoginRefusalText;

namespace {

// Les codes que CE serveur émet vraiment. Ils viennent des sites d'appel, pas
// de la table : `clif_authfail_fd` en documente une trentaine, et la plupart
// n'ont aucun émetteur dans ces sources.
constexpr std::uint8_t kBanServerClosed = 1;
constexpr std::uint8_t kBanAlreadyLoggedIn = 2;
constexpr std::uint8_t kBanIntegrityKick = 3;
constexpr std::uint8_t kBanLastConnectionHeld = 8;
constexpr std::uint8_t kBanByStaff = 15;

// Un code que rien n'émet ici : l'un des états de facturation de Gravity.
constexpr std::uint8_t kBanPaymentSuspended = 11;

constexpr std::uint32_t kLoginUnknownAccount = 0;
constexpr std::uint32_t kLoginWrongPassword = 1;
constexpr std::uint32_t kLoginBanned = 6;

}  // namespace

TEST_CASE("Refus : chaque motif émis par ce serveur a sa phrase", "[net][refusal]") {
  // ⚠ ON ÉPROUVE QUE LES PHRASES DIFFÈRENT, et non ce qu'elles disent :
  // comparer un texte au mot près ferait échouer l'essai à la première
  // retouche de style, sans qu'aucun défaut ne soit apparu.
  const std::string closed = BanText(kBanServerClosed);
  const std::string taken = BanText(kBanAlreadyLoggedIn);
  const std::string integrity = BanText(kBanIntegrityKick);
  const std::string held = BanText(kBanLastConnectionHeld);
  const std::string staff = BanText(kBanByStaff);

  CHECK_FALSE(closed.empty());
  CHECK(closed != taken);
  CHECK(taken != integrity);
  CHECK(integrity != held);
  CHECK(held != staff);
}

TEST_CASE("Refus : un motif inconnu se dit quand même, avec son numéro", "[net][refusal]") {
  // Se taire laisserait le joueur devant une session coupée sans un mot ; le
  // numéro est ce qui permet de nous la rapporter.
  const std::string unknown = BanText(kBanPaymentSuspended);
  CHECK_FALSE(unknown.empty());
  CHECK(unknown.find("11") != std::string::npos);
}

TEST_CASE("Refus : le bannissement ne promet pas une date qu'il n'a pas", "[net][refusal]") {
  // 🔴 CE SERVEUR N'ENVOIE JAMAIS LA DATE : son `logclif_auth_failed` la reçoit
  // en paramètre puis écrit la chaîne VIDE à sa place. Coller le champ tel quel
  // donnerait « banni jusqu'au » suivi de rien — la question posée sans la
  // réponse. Le champ existe pourtant, et un autre serveur le remplit.
  const std::string without = LoginRefusalText(kLoginBanned, "");
  const std::string with = LoginRefusalText(kLoginBanned, "2026-10-01 12:00:00");

  CHECK_FALSE(without.empty());
  CHECK(without.find("jusqu'au ") == std::string::npos);
  CHECK(with.find("2026-10-01 12:00:00") != std::string::npos);
}

TEST_CASE("Refus : le compte inconnu ne se confond pas avec le mauvais mot de passe",
          "[net][refusal]") {
  // Les deux codes se suivent, et c'est la distinction que le joueur attend en
  // premier. Le serveur les journalise séparément ; les fondre reviendrait à
  // jeter ce qu'il a pris la peine de dire.
  CHECK(LoginRefusalText(kLoginUnknownAccount, "") != LoginRefusalText(kLoginWrongPassword, ""));
}

TEST_CASE("Refus : la création refusée ÉNUMÈRE ses causes", "[net][refusal]") {
  // 🔴 QUATRE CODES POUR QUINZE CAUSES. `chclif_createnewchar_refuse` ne
  // traduit que quatre retours de `char_make_new_char`, et le 0xFF ramasse
  // tout le reste : nom vide, nom trop court, caractères de contrôle, dièse en
  // tête, lettres interdites, emplacement pris, plafond atteint, classe
  // refusée, panne de base. « Création refusée » tout court laisserait le
  // joueur changer son nom au hasard sans savoir ce qu'il corrige.
  const std::string catchall = ro::net::MakeCharRefusalText(0xFF);
  CHECK(catchall.find("trop court") != std::string::npos);
  CHECK(catchall.find("interdits") != std::string::npos);
  CHECK(catchall.find("plafond") != std::string::npos);

  // Les trois codes précis disent chacun leur cause, et aucun ne ressemble au
  // fourre-tout.
  CHECK(ro::net::MakeCharRefusalText(0x00) != catchall);
  CHECK(ro::net::MakeCharRefusalText(0x01) != catchall);
  CHECK(ro::net::MakeCharRefusalText(0x03) != catchall);
  CHECK(ro::net::MakeCharRefusalText(0x00) != ro::net::MakeCharRefusalText(0x03));

  // ⚠ ET UN CODE INCONNU SE DIT QUAND MÊME, avec son numéro : se taire
  // laisserait le joueur devant un bouton qui ne fait rien.
  const std::string unknown = ro::net::MakeCharRefusalText(0x42);
  CHECK(unknown.find("66") != std::string::npos);
}
