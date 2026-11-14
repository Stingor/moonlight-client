#include "chat/banned_input.h"

#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/sim/banned_words.h"

// LES SAISIES ÉPROUVÉES : une saisie qui porte un mot interdit reçoit le refus
// que le client d'origine montre pour elle, une autre passe.
//
// ⚠ LA LISTE EST SYNTHÉTIQUE : le mot ne sert qu'à l'essai.

using moonclient::BannedInput;
using moonclient::BannedInputColor;
using moonclient::BannedInputRefusal;
using moonclient::CharacterNameRefusal;
using moonclient::CompanionNameRefusal;
using ro::sim::BannedWords;

namespace {

[[nodiscard]] BannedWords Banned() { return BannedWords(std::vector<std::string>{"brocoli"}); }

}  // namespace

TEST_CASE("Saisie éprouvée : chaque saisie a son refus", "[app][banned]") {
  const BannedWords banned = Banned();
  CHECK(BannedInputRefusal(banned, "Le Brocoli", BannedInput::kCharacterName) ==
        std::string(ro::core::Tr("Langage grossier détecté.")));
  CHECK(BannedInputRefusal(banned, "brocoli", BannedInput::kSentence) ==
        std::string(ro::core::Tr("Langage grossier détecté.")));
  CHECK(BannedInputRefusal(banned, "brocoli", BannedInput::kGuildName) ==
        std::string(
            ro::core::Tr("Un nom qui contient des mots interdits ne peut pas être enregistré.")));
  CHECK(BannedInputRefusal(banned, "brocoli", BannedInput::kMail) ==
        std::string(ro::core::Tr("Grossièreté détectée.\nVérifiez votre texte.")));
  CHECK(BannedInputRefusal(banned, "brocoli", BannedInput::kCompanionName) ==
        std::string(ro::core::Tr("Ce nom n'est pas disponible.")));
  CHECK(BannedInputRefusal(banned, "brocoli", BannedInput::kTalkbox) ==
        std::string(ro::core::Tr("Filtre de discussion : non, ça ne passera pas.")));
  // Le refus d'un nom de guilde s'écrit en saumon, les autres en rouge.
  CHECK(BannedInputColor(BannedInput::kGuildName) != BannedInputColor(BannedInput::kSentence));
}

TEST_CASE("Saisie éprouvée : sans mot interdit, ou sans liste, rien n'est refusé",
          "[app][banned]") {
  const BannedWords banned = Banned();
  CHECK_FALSE(BannedInputRefusal(banned, "Chou-fleur", BannedInput::kSentence).has_value());
  // ⚠ UN LIEN D'OBJET NE S'ÉPROUVE PAS.
  CHECK_FALSE(
      BannedInputRefusal(banned, "<ITEML>brocoli</ITEML>", BannedInput::kSentence).has_value());
  const BannedWords none;
  CHECK_FALSE(BannedInputRefusal(none, "brocoli", BannedInput::kCharacterName).has_value());
  // Sans installation, la liste est vide.
  CHECK(moonclient::LoadBannedWords(nullptr).empty());
}

TEST_CASE("Nom de compagnon : une balise de lien d'objet le refuse avant les mots",
          "[app][banned]") {
  const BannedWords banned = Banned();
  const std::string tag_refusal(
      ro::core::Tr("Un nom qui porte cette balise ne peut pas être employé."));
  // N'importe où dans le nom, l'une ou l'autre suffit.
  CHECK(CompanionNameRefusal(&banned, "Mi<ITEML>nou") == tag_refusal);
  CHECK(CompanionNameRefusal(&banned, "Minou</ITEML>") == tag_refusal);
  // ⚠ LA BALISE PASSE AVANT LES MOTS, qu'un lien d'objet fait sauter.
  CHECK(CompanionNameRefusal(&banned, "<ITEML>brocoli</ITEML>") == tag_refusal);
  // Sans liste, la balise refuse encore.
  CHECK(CompanionNameRefusal(nullptr, "<ITEML>") == tag_refusal);
}

TEST_CASE("Nom de compagnon : la casse et une balise incomplète passent, un mot refuse",
          "[app][banned]") {
  const BannedWords banned = Banned();
  CHECK_FALSE(CompanionNameRefusal(&banned, "<iteml>Minou").has_value());
  CHECK_FALSE(CompanionNameRefusal(&banned, "<ITEML Minou").has_value());
  CHECK_FALSE(CompanionNameRefusal(&banned, "Minou").has_value());
  CHECK_FALSE(CompanionNameRefusal(nullptr, "brocoli").has_value());
  CHECK(CompanionNameRefusal(&banned, "brocoli") ==
        std::string(ro::core::Tr("Ce nom n'est pas disponible.")));
}

TEST_CASE("Nom de personnage : une balise de lien d'objet le refuse avant les mots",
          "[app][banned]") {
  const BannedWords banned = Banned();
  const std::string tag_refusal(
      ro::core::Tr("Un nom qui porte cette balise ne peut pas être employé."));
  CHECK(CharacterNameRefusal(banned, "Ar<ITEML>thur") == tag_refusal);
  CHECK(CharacterNameRefusal(banned, "Arthur</ITEML>") == tag_refusal);
  // ⚠ LA BALISE PASSE AVANT LES MOTS.
  CHECK(CharacterNameRefusal(banned, "<ITEML>brocoli") == tag_refusal);
  // Sans liste, la balise refuse encore.
  CHECK(CharacterNameRefusal(BannedWords(), "</ITEML>") == tag_refusal);
}

TEST_CASE("Nom de personnage : la casse et une balise incomplète passent, un mot refuse",
          "[app][banned]") {
  const BannedWords banned = Banned();
  CHECK_FALSE(CharacterNameRefusal(banned, "<iteml>Arthur").has_value());
  CHECK_FALSE(CharacterNameRefusal(banned, "Arthur</Iteml>").has_value());
  CHECK_FALSE(CharacterNameRefusal(banned, "<ITEML Arthur").has_value());
  CHECK_FALSE(CharacterNameRefusal(banned, "Arthur").has_value());
  CHECK(CharacterNameRefusal(banned, "Le Brocoli") ==
        std::string(ro::core::Tr("Langage grossier détecté.")));
}

TEST_CASE("Nom de personnage : moins de quatre octets du serveur le refusent avant tout",
          "[app][banned]") {
  const BannedWords banned = Banned();
  const std::string short_refusal(
      ro::core::Tr("Le nom du personnage doit compter au moins 4 caractères."));
  CHECK(CharacterNameRefusal(banned, "Art") == short_refusal);
  CHECK_FALSE(CharacterNameRefusal(banned, "Arto").has_value());
  // ⚠ DES OCTETS DU SERVEUR, PAS D'UTF-8 : « Éva » en compte quatre en UTF-8,
  // trois une fois convertie ; « Élie » en compte quatre.
  CHECK(CharacterNameRefusal(banned, "Éva") == short_refusal);
  CHECK_FALSE(CharacterNameRefusal(banned, "Élie").has_value());
  // La longueur passe avant la balise.
  CHECK(CharacterNameRefusal(banned, "<IT") == short_refusal);
}
