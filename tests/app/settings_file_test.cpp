#include "game/settings_file.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "support/temp_directory.h"

namespace {

using moonclient::kSettingsFlushDelayMs;
using moonclient::SettingsFile;

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

// Un instant assez loin pour que l'attente soit écoulée. Passer par une
// constante plutôt qu'un nombre : le délai peut changer, l'essai ne doit pas.
constexpr std::uint32_t kLater = kSettingsFlushDelayMs + 1;

}  // namespace

TEST_CASE("un réglage absent du fichier garde son défaut", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  bool music = true;
  int lines = 7;

  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("music", &music);
  file.Declare("lignes", &lines);
  file.Load();  // le fichier n'existe pas : ce n'est pas une erreur

  CHECK(music);
  CHECK(lines == 7);
}

TEST_CASE("la première partie crée le fichier, sans qu'on ait rien réglé", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  bool music = true;

  SettingsFile file(path);
  file.Declare("music", &music);
  file.Load();
  file.Flush();

  // ⚠ CE QUI COMPTE : un fichier qu'on ne trouve pas n'existe pas. Le joueur à
  // qui l'on dit « corrige-le à la main » doit avoir quelque chose à ouvrir dès
  // sa première partie.
  REQUIRE(std::filesystem::exists(path));
  CHECK(Contents(path).find("music: true") != std::string::npos);
}

TEST_CASE("une partie qui ne change rien ne réécrit pas le fichier", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  folder.WriteText("reglages.yaml", "music: true\n");

  bool music = true;
  SettingsFile file(path);
  file.Declare("music", &music);
  file.Load();

  // Poser dans le moteur ce qu'on vient de lire n'est PAS un changement : le
  // compter comme tel réécrirait le fichier à chaque fermeture de partie.
  CHECK_FALSE(file.waiting());
}

TEST_CASE("ce que le fichier porte écrase le défaut", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  folder.WriteText("reglages.yaml", "music: false\nvolume: 0.25\nnom: sting\n");

  bool music = true;
  float volume = 1.0F;
  std::string name = "anonyme";

  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("music", &music);
  file.Declare("volume", &volume);
  file.Declare("nom", &name);
  file.Load();

  CHECK_FALSE(music);
  CHECK_THAT(volume, Catch::Matchers::WithinAbs(0.25, 1e-6));
  CHECK(name == "sting");
}

TEST_CASE("une couleur survit à l'aller-retour, canal alpha compris", "[app][reglages]") {
  // 🔴 C'EST POUR ÇA QUE LE TYPE EST NON SIGNÉ. Une couleur opaque a son
  // octet de poids fort à 0xFF : rangée dans un `int`, elle ressort négative du
  // fichier, et la teinte relue n'est plus celle qu'on avait choisie.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  constexpr std::uint32_t kOpaqueWhite = 0xFFFFFFFFU;

  {
    std::uint32_t color = 0U;
    SettingsFile file(path);
    file.Declare("couleur", &color);
    file.Load();
    color = kOpaqueWhite;
    file.Touch();
    file.Flush();
  }

  std::uint32_t relue = 0U;
  SettingsFile again(path);
  again.Declare("couleur", &relue);
  again.Load();
  CHECK(relue == kOpaqueWhite);
}

TEST_CASE("le fichier se range en groupes, et se relit quand même", "[app][reglages]") {
  // 🔴 SIX CENTS CLÉS D'AFFILÉE NE SE LISENT PAS, et ce fichier est fait pour
  // être ouvert dans un éditeur de texte — c'est ce qui a fait choisir YAML.
  // L'intertitre est un COMMENTAIRE : le document reste le même mapping plat, et
  // rien de ce qu'un joueur a réglé ne change de chemin.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";

  bool sound = true;
  int width = 60;
  {
    SettingsFile file(path);
    file.Section("Le son");
    file.Declare("sound_effects", &sound);
    file.Section("Les barres");
    file.Declare("barre_largeur", &width);
    file.Load();
    sound = false;
    width = 96;
    file.Touch();
    file.Flush();
  }

  std::ifstream written(path);
  const std::string text((std::istreambuf_iterator<char>(written)),
                         std::istreambuf_iterator<char>());
  CHECK(text.find("# ── Le son ") != std::string::npos);
  CHECK(text.find("# ── Les barres ") != std::string::npos);
  // ⚠ ET CE QUI COMPTE VRAIMENT : le fichier se relit à l'identique.
  bool sound_again = true;
  int width_again = 0;
  SettingsFile reread(path);
  reread.Declare("sound_effects", &sound_again);
  reread.Declare("barre_largeur", &width_again);
  reread.Load();
  CHECK_FALSE(sound_again);
  CHECK(width_again == 96);
}

TEST_CASE("un groupe sans réglage ne laisse pas de titre orphelin", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";

  bool kept = false;
  SettingsFile file(path);
  file.Section("Un groupe qui reste vide");
  // ⚠ RIEN DÉCLARÉ SOUS CE TITRE : le suivant le remplace, et le premier ne doit
  // pas s'écrire — un intertitre sans rien dessous se lit comme un réglage perdu.
  file.Section("Le son");
  file.Declare("sound_effects", &kept);
  file.Load();
  file.Touch();
  file.Flush();

  std::ifstream written(path);
  const std::string text((std::istreambuf_iterator<char>(written)),
                         std::istreambuf_iterator<char>());
  CHECK(text.find("Un groupe qui reste vide") == std::string::npos);
  CHECK(text.find("# ── Le son ") != std::string::npos);
}

TEST_CASE("une clé écrite en double se nettoie à la réécriture", "[app][reglages]") {
  // 🔴 LE FICHIER DU JOUEUR EN PORTAIT SEPT. YAML tolère un mapping à clés
  // répétées, et l'ancienne écriture — qui remplissait un arbre relu — mettait à
  // jour la PREMIÈRE et recopiait les six autres à chaque fois. Le réglage
  // restait donc juste, mais le fichier grossissait d'un fossile qu'on ne
  // pouvait plus enlever à la main sans qu'il revienne.
  //
  // ⚠ ET C'EST LA LECTURE QUI DIT LAQUELLE COMPTE : ce test fixe aussi cela.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  {
    std::ofstream(path) << "journal: true\njournal: false\njournal: false\n";
  }

  {
    bool kept = false;
    SettingsFile file(path);
    file.Declare("journal", &kept);
    file.Load();
    CHECK(kept);  // la première ligne, celle que l'écriture tenait à jour
    file.Touch();
    file.Flush();
  }

  int seen = 0;
  std::ifstream written(path);
  for (std::string line; std::getline(written, line);) {
    if (line.starts_with("journal:")) {
      ++seen;
    }
  }
  CHECK(seen == 1);
}

TEST_CASE("une clé déclarée après la lecture ne s'écrit pas deux fois", "[app][reglages]") {
  // 🔴 CE QUE L'ÉCRITURE DIRECTE REND POSSIBLE, ET QU'IL FAUT FERMER. Ce qu'on
  // ne connaît pas est relevé À LA LECTURE et réécrit tel quel ; une clé
  // déclarée APRÈS serait donc à la fois dans ce relevé et dans la table, et
  // sortirait sur deux lignes. L'arbre d'avant fondait les deux sans rien dire.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  {
    std::ofstream(path) << "tardive: 3\n";
  }

  int value = 0;
  SettingsFile file(path);
  file.Load();
  // ⚠ APRÈS `Load`, ce qui est l'ordre à ne pas suivre — mais rien n'empêche de
  // l'écrire, et le fichier ne doit pas en pâtir.
  file.Declare("tardive", &value);
  value = 7;
  file.Touch();
  file.Flush();

  int seen = 0;
  std::ifstream written(path);
  for (std::string line; std::getline(written, line);) {
    if (line.starts_with("tardive:")) {
      ++seen;
    }
  }
  CHECK(seen == 1);
}

TEST_CASE("un flottant survit à l'aller-retour, à la décimale près", "[app][reglages]") {
  // 🔴 CE QUE L'ÉMISSION À LA MAIN POURRAIT PERDRE. Le fichier ne passe plus par
  // un arbre YAML — c'était trois cents millisecondes par écriture —, et un
  // nombre écrit à six décimales fixes ne se relit PAS à l'identique. Un écart
  // d'un millième sur une opacité ne se voit pas ; sur une position de cadre
  // relue mille fois, il dérive.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  constexpr float kAwkward = 0.12345678F;

  {
    float value = 0.0F;
    SettingsFile file(path);
    file.Declare("part", &value);
    file.Load();
    value = kAwkward;
    file.Touch();
    file.Flush();
  }

  float reread_value = 0.0F;
  SettingsFile again(path);
  again.Declare("part", &reread_value);
  again.Load();
  CHECK(reread_value == kAwkward);
}

TEST_CASE("une chaîne piégeuse se relit telle quelle", "[app][reglages]") {
  // 🔴 SANS GUILLEMETS, YAML RELIT AUTRE CHOSE. « oui » redevient un booléen,
  // « 12 » un nombre, « ~ » un nul — et la chaîne qu'on avait écrite n'est plus
  // là. C'est l'émetteur de yaml-cpp qui citait pour nous ; il ne le fait plus.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  const std::string awkward = R"(oui: "12" ~ \ #)";

  {
    std::string value;
    SettingsFile file(path);
    file.Declare("texte", &value);
    file.Load();
    value = awkward;
    file.Touch();
    file.Flush();
  }

  std::string reread_value;
  SettingsFile again(path);
  again.Declare("texte", &reread_value);
  again.Load();
  CHECK(reread_value == awkward);
}

TEST_CASE("une valeur du mauvais type laisse le défaut en place", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  // ⚠ CE QU'ON VÉRIFIE : pas d'exception, et pas de valeur inventée. Un fichier
  // corrigé à la main peut porter n'importe quoi ; le réglage voisin ne doit
  // pas en pâtir.
  folder.WriteText("reglages.yaml", "volume: beaucoup\nmusic: false\n");

  float volume = 0.5F;
  bool music = true;
  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("volume", &volume);
  file.Declare("music", &music);
  file.Load();

  CHECK_THAT(volume, Catch::Matchers::WithinAbs(0.5, 1e-6));
  CHECK_FALSE(music);
}

TEST_CASE("l'écriture attend que le joueur s'arrête", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  float volume = 1.0F;

  SettingsFile file(path);
  file.Declare("volume", &volume);
  file.Load();

  // Un curseur qu'on fait glisser : « changé » à chaque image.
  for (std::uint32_t image = 0; image < 10; ++image) {
    volume = static_cast<float>(image) / 10.0F;
    file.Touch();
    file.Tick(image);
  }
  // ⚠ RIEN N'EST ENCORE ÉCRIT : c'est tout l'objet de l'anti-rebond. Dix images
  // de glissement feraient dix sérialisations et dix cycles disque.
  CHECK_FALSE(std::filesystem::exists(path));

  file.Tick(10);  // le décompte part de cette image
  file.Tick(10 + kLater);
  REQUIRE(std::filesystem::exists(path));
  CHECK(Contents(path).find("volume: 0.9") != std::string::npos);
}

TEST_CASE("chaque changement relance l'attente", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  bool music = true;

  SettingsFile file(path);
  file.Declare("music", &music);
  file.Load();

  file.Touch();
  file.Tick(0);
  file.Tick(kLater - 2);  // l'attente touche à sa fin…
  file.Touch();           // … et un nouveau changement la relance
  file.Tick(kLater - 1);
  file.Tick(kLater);
  CHECK_FALSE(std::filesystem::exists(path));

  file.Tick(kLater - 1 + kLater);
  CHECK(std::filesystem::exists(path));
}

TEST_CASE("l'écriture immédiate n'attend pas", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  bool music = true;

  SettingsFile file(path);
  file.Declare("music", &music);
  file.Load();
  music = false;
  file.Touch();
  // Ce que fait la fermeture du jeu : le délai n'a plus le temps d'expirer.
  file.Flush();

  REQUIRE(std::filesystem::exists(path));
  CHECK(Contents(path).find("music: false") != std::string::npos);
}

TEST_CASE("une clé inconnue survit à une réécriture", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  // Écrite par une version plus récente, qui connaît un réglage de plus.
  folder.WriteText("reglages.yaml", "music: true\nreglage_de_demain: 42\n");

  bool music = true;
  SettingsFile file(path);
  file.Declare("music", &music);
  file.Load();
  music = false;
  file.Touch();
  file.Flush();

  const std::string written = Contents(path);
  CHECK(written.find("music: false") != std::string::npos);
  // ⚠ CE QUI COMPTE : jouer une partie avec cette version-ci ne doit pas coûter
  // au joueur les réglages qu'il a posés avec l'autre.
  CHECK(written.find("reglage_de_demain: 42") != std::string::npos);
}

TEST_CASE("une clé retirée disparaît du fichier, sans qu'on ait rien réglé", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  // Écrite par une version plus ANCIENNE, qui avait un réglage de plus.
  folder.WriteText("reglages.yaml", "music: true\nreglage_d_hier: false\nreglage_de_demain: 42\n");

  bool music = true;
  SettingsFile file(path);
  file.Declare("music", &music);
  file.Retire("reglage_d_hier");
  file.Load();

  // 🔴 SANS QUE LE JOUEUR TOUCHE À RIEN : une clé morte trouvée met le fichier
  // en attente d'écriture, sinon elle survivrait jusqu'au jour où l'on ouvre les
  // options — c'est-à-dire indéfiniment pour qui n'y va jamais.
  CHECK(file.waiting());
  file.Flush();

  const std::string written = Contents(path);
  CHECK(written.find("reglage_d_hier") == std::string::npos);
  // ⚠ ET CE QU'ON NE CONNAÎT PAS RESTE : retirer une clé n'est pas jeter les
  // autres. Une version plus récente doit toujours retrouver les siennes.
  CHECK(written.find("reglage_de_demain: 42") != std::string::npos);
  CHECK(written.find("music: true") != std::string::npos);
}

TEST_CASE("une clé retirée après la lecture disparaît quand même", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  folder.WriteText("reglages.yaml", "music: true\nreglage_d_hier: false\n");

  bool music = true;
  SettingsFile file(path);
  file.Declare("music", &music);
  file.Load();
  // ⚠ L'ORDRE À SUIVRE EST L'AUTRE, mais rien ne l'impose : sans la garde, la
  // clé relevée comme étrangère serait réécrite telle quelle, pour toujours.
  file.Retire("reglage_d_hier");
  file.Touch();
  file.Flush();

  CHECK(Contents(path).find("reglage_d_hier") == std::string::npos);
}

TEST_CASE("le retour aux défauts rend les valeurs de la déclaration", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  folder.WriteText("reglages.yaml", "music: false\nvolume: 0.1\n");

  bool music = true;
  float volume = 1.0F;
  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("music", &music);
  file.Declare("volume", &volume);
  file.Load();
  REQUIRE_FALSE(music);

  file.RestoreDefaults();
  // ⚠ LES DÉFAUTS SONT CEUX DU CODE — l'initialisateur du champ —, et non ceux
  // du fichier qu'on vient de lire.
  CHECK(music);
  CHECK_THAT(volume, Catch::Matchers::WithinAbs(1.0, 1e-6));
}

TEST_CASE("le retour d'une seule clé laisse les autres en place", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  folder.WriteText("reglages.yaml", "music: false\nvolume: 0.1\n");

  bool music = true;
  float volume = 1.0F;
  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("music", &music);
  file.Declare("volume", &volume);
  file.Load();

  file.RestoreDefault("volume");
  CHECK_THAT(volume, Catch::Matchers::WithinAbs(1.0, 1e-6));
  CHECK_FALSE(music);
  // Une clé que personne n'a déclarée ne rend rien, et ne casse rien.
  file.RestoreDefault("absente");
  CHECK_FALSE(music);
}

TEST_CASE("après une lecture abîmée, on n'écrit plus", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path path = folder.path() / "reglages.yaml";
  // Un document que yaml-cpp refuse.
  folder.WriteText("reglages.yaml", "music: true\n  mauvais: [\n");

  bool music = true;
  SettingsFile file(path);
  file.Declare("music", &music);
  file.Load();

  music = false;
  file.Touch();
  file.Flush();

  // ⚠ CE QU'ON PROTÈGE : le fichier n'a été lu QU'EN PARTIE, donc l'état en
  // mémoire n'en représente qu'une partie. Réécrire depuis là rendrait la perte
  // définitive — mieux vaut un réglage qui ne s'enregistre pas qu'un fichier
  // qu'on ne peut plus réparer à la main.
  CHECK(Contents(path).find("mauvais") != std::string::npos);
}
