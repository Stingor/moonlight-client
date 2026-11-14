// La langue de l'interface.

#include "ro/core/i18n.h"

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

namespace {

// Écrit un catalogue et bascule dessus. Rend le chemin, pour les essais qui
// veulent le relire.
[[nodiscard]] std::filesystem::path WriteCatalogue(const ro::test_support::TempDirectory& folder,
                                                   std::string_view content) {
  const std::filesystem::path path = folder.path() / "essai.txt";
  std::ofstream file(path, std::ios::binary);
  file << content;
  return path;
}

}  // namespace

TEST_CASE("sans catalogue, le français se rend TEL QUEL", "[core]") {
  // ⚠ La propriété qui rend ce module gratuit : la langue par défaut ne cherche
  // rien et n'alloue rien. Un client français est exactement aussi rapide que
  // si la traduction n'existait pas.
  ro::core::UseFrench();
  const char* const french = "Journal";
  CHECK(ro::core::Tr(french) == french);  // NOLINT — l'ADRESSE, pas le contenu
  CHECK(ro::core::CurrentLanguage() == "fr");
}

TEST_CASE("un catalogue traduit ce qu'il porte et laisse le reste", "[core]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = WriteCatalogue(folder,
                                                    "# un commentaire\n"
                                                    "fr: Journal\n"
                                                    "tr: Log\n"
                                                    "\n"
                                                    "fr: effacer\n"
                                                    "tr: clear\n");
  REQUIRE(ro::core::LoadLanguage(path, "en").has_value());

  CHECK(std::string(ro::core::Tr("Journal")) == "Log");
  CHECK(std::string(ro::core::Tr("effacer")) == "clear");
  // Un texte absent DÉGRADE en français : une traduction incomplète ne blanchit
  // pas l'interface.
  CHECK(std::string(ro::core::Tr("suivre")) == "suivre");
  CHECK(ro::core::CurrentLanguage() == "en");
  ro::core::UseFrench();
}

TEST_CASE("une traduction VIDE veut dire « pas encore faite »", "[core]") {
  // L'entrée reste dans le fichier, visible du traducteur, et le texte
  // s'affiche en français plutôt qu'en blanc.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = WriteCatalogue(folder, "fr: suivre\ntr:\n");
  REQUIRE(ro::core::LoadLanguage(path, "en").has_value());
  CHECK(std::string(ro::core::Tr("suivre")) == "suivre");
  ro::core::UseFrench();
}

TEST_CASE("le texte est pris VERBATIM, accolades et guillemets compris", "[core]") {
  // ⚠ C'est la raison du format à préfixes : un gabarit porte des accolades, un
  // libellé peut porter un signe égal ou des guillemets, et un format qui
  // découperait sur l'un d'eux abîmerait justement ces textes-là.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path =
      WriteCatalogue(folder, "fr: %zu ligne(s), %zu choisie(s)\ntr: %zu line(s), %zu selected\n");
  REQUIRE(ro::core::LoadLanguage(path, "en").has_value());
  CHECK(std::string(ro::core::Tr("%zu ligne(s), %zu choisie(s)")) == "%zu line(s), %zu selected");
  ro::core::UseFrench();
}

TEST_CASE("les textes demandés et absents se retiennent", "[core]") {
  // ⚠ C'est la CONTREPARTIE du choix « le français est la clé » : un texte
  // retouché dans le code perd sa traduction en silence, et cette liste est ce
  // qui le rattrape.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = WriteCatalogue(folder, "fr: Journal\ntr: Log\n");
  REQUIRE(ro::core::LoadLanguage(path, "en").has_value());

  static_cast<void>(ro::core::Tr("Journal"));
  static_cast<void>(ro::core::Tr("un texte que personne n'a traduit"));
  static_cast<void>(ro::core::Tr("un texte que personne n'a traduit"));

  const std::vector<std::string> absent = ro::core::MissingTexts();
  REQUIRE(absent.size() == 1);
  CHECK(absent.front() == "un texte que personne n'a traduit");
  ro::core::UseFrench();
}

TEST_CASE("un texte sur deux lignes s'exporte, se traduit et se relit", "[core]") {
  // 🔴 L'EXPORT N'ÉCHAPPAIT RIEN : le gabarit coupait le texte à son saut de
  // ligne, et la clé relue ne correspondait plus jamais. L'aller-retour complet
  // — exporter, remplir, recharger — doit rendre la traduction.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path empty = WriteCatalogue(folder, "fr: Journal\ntr: Log\n");
  REQUIRE(ro::core::LoadLanguage(empty, "en").has_value());
  constexpr const char* kTwoLines = "Clic : ouvrir\nC:\\chemin";
  static_cast<void>(ro::core::Tr(kTwoLines));

  const std::filesystem::path exported = folder.path() / "gabarit.txt";
  REQUIRE(ro::core::ExportMissing(exported).has_value());
  std::ifstream read(exported, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(read)), std::istreambuf_iterator<char>());
  CHECK(text.find("fr: Clic : ouvrir\\nC:\\\\chemin\n") != std::string::npos);

  // Le traducteur remplit la ligne `tr:` du gabarit, et on le recharge.
  const std::filesystem::path filled = folder.path() / "rempli.txt";
  std::ofstream(filled, std::ios::binary) << "fr: Clic : ouvrir\\nC:\\\\chemin\n"
                                          << "tr: Click: open\\nC:\\\\path\n";
  REQUIRE(ro::core::LoadLanguage(filled, "en").has_value());
  CHECK(std::string_view(ro::core::Tr(kTwoLines)) == "Click: open\nC:\\path");
  ro::core::UseFrench();
}

TEST_CASE("changer de langue OUBLIE les manquants du catalogue précédent", "[core]") {
  // Sans cela, un catalogue se verrait reprocher les trous d'un autre.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = WriteCatalogue(folder, "fr: Journal\ntr: Log\n");
  REQUIRE(ro::core::LoadLanguage(path, "en").has_value());
  static_cast<void>(ro::core::Tr("absent"));
  REQUIRE(ro::core::MissingTexts().size() == 1);

  REQUIRE(ro::core::LoadLanguage(path, "es").has_value());
  CHECK(ro::core::MissingTexts().empty());
  ro::core::UseFrench();
}

TEST_CASE("un catalogue abîmé est refusé EN ENTIER", "[core]") {
  // ⚠ À moitié chargé, personne ne saurait quelle moitié : une traduction sans
  // son français rangée sous la clé précédente donnerait une interface qui ment.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = WriteCatalogue(folder, "tr: Log\nfr: Journal\ntr: Log\n");
  CHECK_FALSE(ro::core::LoadLanguage(path, "en").has_value());
  // Et la langue n'a pas bougé.
  CHECK(ro::core::CurrentLanguage() == "fr");
}

TEST_CASE("un catalogue introuvable ne change pas la langue", "[core]") {
  const ro::test_support::TempDirectory folder;
  CHECK_FALSE(ro::core::LoadLanguage(folder.path() / "absent.txt", "en").has_value());
  CHECK(ro::core::CurrentLanguage() == "fr");
}

TEST_CASE("le compte des changements de langue avance à chaque bascule", "[core]") {
  // Il sert à qui GARDE un texte traduit d'une image à l'autre — le nom d'une
  // fenêtre — pour ne le recomposer qu'au bon moment.
  const std::size_t before = ro::core::LanguageSerial();
  ro::core::UseFrench();
  CHECK(ro::core::LanguageSerial() == before + 1);
}
