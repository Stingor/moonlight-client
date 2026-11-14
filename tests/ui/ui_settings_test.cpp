// Les réglages d'interface, ÉCRITS ET RELUS, sans fenêtre ni carte graphique.
//
// ⚠ C'est le seul moyen de vérifier ce chemin-là. Ce que ces essais couvrent —
// un fichier écrit au bon endroit, une fenêtre qui rouvre là où on l'avait
// laissée — ne se voit autrement qu'en relançant le jeu deux fois et en
// regardant, ce qui n'est pas une vérification mais une impression.

#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/log_window.h"
#include "ro/ui/ui_context.h"
#include "ro/ui/window_places.h"

#include "imgui.h"
#include "support/temp_directory.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Une trame vide suffit : c'est elle qui déclenche la LECTURE du fichier.
void RunOneFrame(ro::ui::UiContext& ui) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  ui.EndFrame();
}

// Forcer l'écriture, comme ImGui le demanderait après un déplacement de
// fenêtre. Sans ce drapeau, `SaveSettingsIfDirty` ne fait rien — et c'est
// exactement ce qu'on veut d'elle en jeu.
void AskForSave(ro::ui::UiContext& ui) {
  ImGui::SetCurrentContext(ui.handle());
  ImGui::GetIO().WantSaveIniSettings = true;
  ui.SaveSettingsIfDirty();
}

[[nodiscard]] std::string ReadWholeFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

}  // namespace

TEST_CASE("sans fichier nommé, aucun réglage n'est écrit nulle part", "[ui]") {
  // ⚠ La propriété qui protège la suite de tests : un contexte qui n'a pas reçu
  // de chemin ne doit pas semer un `imgui.ini` dans le répertoire courant.
  ro::ui::UiContext ui;
  bool open = true;
  ui.RememberWindow("Journal", &open);
  RunOneFrame(ui);
  AskForSave(ui);

  ImGui::SetCurrentContext(ui.handle());
  CHECK(ImGui::GetIO().IniFilename == nullptr);
}

TEST_CASE("l'état OUVERT d'une fenêtre s'écrit dans le fichier de réglages", "[ui]") {
  // ImGui retient la place et le repli d'une fenêtre, jamais son ouverture :
  // celle-ci est un booléen qui appartient à l'appelant. Elle voyage donc dans
  // le même fichier, sous notre propre rubrique.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path settings = folder.path() / "interface.ini";

  ro::ui::UiContext ui;
  ui.UseSettingsFile(settings);
  bool journal = true;
  bool trames = false;
  ui.RememberWindow("Journal", &journal);
  ui.RememberWindow("Trames", &trames);
  RunOneFrame(ui);
  AskForSave(ui);

  REQUIRE(std::filesystem::exists(settings));
  const std::string written = ReadWholeFile(settings);
  CHECK(written.find("[moonclient][Windows]") != std::string::npos);
  CHECK(written.find("Journal=1") != std::string::npos);
  CHECK(written.find("Trames=0") != std::string::npos);
}

TEST_CASE("une fenêtre laissée ouverte ROUVRE au démarrage suivant", "[ui]") {
  // Le vrai sujet : deux lancements, et le second retrouve ce que le premier a
  // laissé. Un aller sans retour ne prouverait qu'une écriture.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path settings = folder.path() / "interface.ini";

  {
    ro::ui::UiContext first;
    first.UseSettingsFile(settings);
    bool journal = true;
    first.RememberWindow("Journal", &journal);
    RunOneFrame(first);
    AskForSave(first);
  }

  ro::ui::UiContext second;
  second.UseSettingsFile(settings);
  // Fermée au départ, comme elle l'est dans un client neuf.
  bool journal = false;
  second.RememberWindow("Journal", &journal);
  // ⚠ LA LECTURE A LIEU À LA PREMIÈRE TRAME, et pas avant : c'est pourquoi il
  // faut que le gestionnaire soit inscrit d'abord.
  CHECK_FALSE(journal);
  RunOneFrame(second);
  CHECK(journal);
}

TEST_CASE("un réglage inconnu ne fait pas échouer la lecture", "[ui]") {
  // ⚠ Un fichier écrit par une version plus récente ne doit pas empêcher
  // celle-ci de démarrer : ce qu'on ne reconnaît pas se saute.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path settings = folder.path() / "interface.ini";
  {
    std::ofstream file(settings, std::ios::binary);
    file << "[moonclient][Windows]\nJournal=1\nUneFenetreDeDemain=1\nsansEgal\n\n";
  }

  ro::ui::UiContext ui;
  ui.UseSettingsFile(settings);
  bool journal = false;
  ui.RememberWindow("Journal", &journal);
  RunOneFrame(ui);
  CHECK(journal);
}

TEST_CASE("la place d'une fenêtre est rangée sous son IDENTITÉ, pas sous son titre", "[ui]") {
  // ⚠ LA PROPRIÉTÉ QUI REND LA TRADUCTION POSSIBLE. ImGui tire l'identité d'une
  // fenêtre de son titre ; un titre traduit ferait donc une AUTRE fenêtre, qui
  // rouvre au milieu de l'écran sans sa taille ni sa place. Les trois dièses de
  // `Journal###log` séparent les deux, et c'est ce que cet essai vérifie :
  // le fichier ne doit porter QUE la partie stable.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path settings = folder.path() / "interface.ini";

  ro::ui::UiContext ui;
  ui.UseSettingsFile(settings);
  ro::ui::LogWindowState state;
  state.open = true;
  ui.RememberWindow(ro::ui::kLogWindowId, &state.open);

  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawLogWindow(state, {}));
  ui.EndFrame();
  AskForSave(ui);

  REQUIRE(std::filesystem::exists(settings));
  const std::string written = ReadWholeFile(settings);
  // La rubrique d'ImGui porte l'identité seule…
  CHECK(written.find("[Window][log]") != std::string::npos);
  // … et la nôtre la même, pour que les deux se retrouvent.
  CHECK(written.find("log=1") != std::string::npos);
  // Le TITRE, lui, n'a rien à faire dans un fichier de réglages : c'est le seul
  // mot qui changera de langue.
  CHECK(written.find("[Window][Journal]") == std::string::npos);
}

TEST_CASE("une fenêtre ouverte entre dans le profil, et le profil reposé la rouvre",
          "[ui][disposition]") {
  // Deux personnages sur le même profil : ce que l'un a ouvert, l'autre le
  // retrouve ouvert en le posant.
  ro::ui::UiContext ui;
  bool inventory = true;
  bool docked = true;
  ui.RememberWindow("inventory", &inventory);
  ui.RememberWindow("damage_meter.docked", &docked);

  const ro::ui::LayoutProfile captured = ui.CurrentLayout();
  REQUIRE(captured.toggles.contains("inventory"));
  CHECK(captured.toggles.at("inventory"));
  CHECK(captured.toggles.at("damage_meter.docked"));

  inventory = false;
  docked = false;
  ui.UseLayout(captured);
  CHECK(inventory);
  CHECK(docked);
}

TEST_CASE("un profil qui ne nomme pas une fenêtre la laisse telle qu'elle est",
          "[ui][disposition]") {
  // ⚠ LE PROFIL ÉCRIT AVANT QU'UNE FENÊTRE NE S'Y RETIENNE ne doit ni tout
  // fermer ni tout ouvrir : la clé absente garde l'état présent.
  ro::ui::UiContext ui;
  bool inventory = true;
  bool cart = false;
  bool log = false;
  ui.RememberWindow("inventory", &inventory);
  ui.RememberWindow("cart", &cart);
  ui.RememberWindow("log", &log);

  ro::ui::LayoutProfile older;
  older.toggles.emplace("log", true);
  ui.UseLayout(older);
  CHECK(inventory);
  CHECK_FALSE(cart);
  CHECK(log);
}
