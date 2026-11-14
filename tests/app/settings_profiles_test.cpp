// LES PROFILS DE RÉGLAGES : ce que `SettingsFile` en relève et en pose, le
// fichier qui les range, et le gardien commun aux deux familles de profils —
// éprouvé ici par un port factice, sans fenêtre ni disque.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <SDL3/SDL_keycode.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/shortcut_bar.h"

#include "game/profile_keeper.h"
#include "game/settings_file.h"
#include "game/settings_profile_file.h"
#include "game/settings_profile_port.h"
#include "input/hotkey_file.h"
#include "input/hotkeys.h"
#include "support/temp_directory.h"

namespace {

using moonclient::kDefaultProfileName;
using moonclient::ProfileBook;
using moonclient::SettingsFile;
using moonclient::SettingsValues;

// UN ÉTAT RÉDUIT À UN ENTIER, et un fichier en mémoire.
using Toy = int;
constexpr Toy kToyDefault = 0;

class ToyPort final : public moonclient::ProfilePort<Toy> {
 public:
  Toy state = kToyDefault;
  std::uint64_t revision = 0;
  ProfileBook<Toy> disk;
  int saves = 0;

  void Change(Toy to) {
    state = to;
    ++revision;
  }

  [[nodiscard]] std::uint64_t Revision() override { return revision; }
  [[nodiscard]] Toy Capture() override { return state; }
  void Pose(const Toy& profile) override { Change(profile); }
  [[nodiscard]] Toy Defaults() override { return kToyDefault; }
  [[nodiscard]] bool Load(ProfileBook<Toy>& into) override {
    into = disk;
    return true;
  }
  void Save(const ProfileBook<Toy>& from) override {
    disk = from;
    ++saves;
  }
};

[[nodiscard]] moonclient::ProfileFamilyText Text() {
  return moonclient::ProfileFamilyText{.title = "t", .help = "h", .id = "toy"};
}
// Assez pour que le calme soit revenu.
constexpr std::uint32_t kQuietLater = 5000;

[[nodiscard]] const Toy* Stored(const ToyPort& port, std::string_view name) {
  const moonclient::Named<Toy>* named = port.disk.Find(name);
  return named != nullptr ? &named->profile : nullptr;
}

}  // namespace

TEST_CASE("Réglages : la machine ne suit pas le profil", "[app][profils]") {
  const ro::test_support::TempDirectory folder;
  bool fog = true;
  int volume = 50;
  std::string language = "fr";
  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("fog", &fog);
  {
    const SettingsFile::MachineScope machine(file);
    file.Declare("volume", &volume);
    file.Declare("language", &language);
  }
  file.Load();

  const SettingsValues shared = file.SharedValues();
  CHECK(shared.contains("fog"));
  CHECK_FALSE(shared.contains("volume"));
  CHECK_FALSE(shared.contains("language"));

  // Poser un profil laisse la machine telle quelle.
  fog = false;
  volume = 80;
  file.AssignShared(shared);
  CHECK(fog);
  CHECK(volume == 80);
}

TEST_CASE("Réglages : une clé absente du profil reprend son défaut", "[app][profils]") {
  const ro::test_support::TempDirectory folder;
  int lines = 7;
  std::string name = "défaut";
  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("lignes", &lines);
  file.Declare("nom", &name);
  file.Load();

  lines = 12;
  name = "Il dit \"oui\"";
  const SettingsValues kept = file.SharedValues();
  lines = 3;
  name = "autre";
  file.AssignShared(kept);
  CHECK(lines == 12);
  CHECK(name == "Il dit \"oui\"");

  file.AssignShared(SettingsValues{});
  CHECK(lines == 7);
  CHECK(name == "défaut");
}

TEST_CASE("Réglages : chaque changement fait grandir la révision", "[app][profils]") {
  const ro::test_support::TempDirectory folder;
  SettingsFile file(folder.path() / "reglages.yaml");
  const std::uint64_t before = file.revision();
  file.Touch();
  CHECK(file.revision() != before);
}

TEST_CASE("Profils de réglages : un aller-retour fidèle", "[app][profils]") {
  moonclient::SettingsProfileBook written;
  written.Put(kDefaultProfileName, SettingsValues{{"fog", "true"}, {"lignes", "7"}});
  written.Put("PvP", SettingsValues{{"nom", "Stingor"}});
  written.Bind("Stingor", "PvP");

  moonclient::SettingsProfileBook read;
  REQUIRE(moonclient::ReadSettingsProfiles(moonclient::WriteSettingsProfiles(written), read));
  CHECK(read == written);
  CHECK_FALSE(moonclient::ReadSettingsProfiles("version: 7\n", read));
}

TEST_CASE("Profils de réglages : un texte délicat revient tel quel", "[app][profils]") {
  // Guillemets, deux-points, accents : le fichier des profils les récrit à sa
  // façon, et `SettingsFile` doit relire la même valeur.
  const ro::test_support::TempDirectory folder;
  std::string name = "Il dit \"oui\" : déjà";
  SettingsFile file(folder.path() / "reglages.yaml");
  file.Declare("nom", &name);
  file.Load();

  moonclient::SettingsProfileBook written;
  written.Put("PvP", file.SharedValues());
  moonclient::SettingsProfileBook read;
  REQUIRE(moonclient::ReadSettingsProfiles(moonclient::WriteSettingsProfiles(written), read));
  CHECK(read == written);

  const std::string kept = name;
  name = "autre";
  file.AssignShared(read.Find("PvP")->profile);
  CHECK(name == kept);
}

TEST_CASE("Profils : le livre renomme, et les personnages suivent", "[app][profils]") {
  ProfileBook<Toy> book;
  book.Put("Farm", 1);
  book.Bind("Stingor", "Farm");
  CHECK(book.Rename("Farm", "Ferme"));
  CHECK(book.ProfileOf("Stingor") == "Ferme");
  CHECK_FALSE(book.Rename("Ferme", kDefaultProfileName));
  CHECK_FALSE(book.Rename(kDefaultProfileName, "Autre"));
  book.Put("Pris", 2);
  CHECK_FALSE(book.Rename("Ferme", "Pris"));
}

TEST_CASE("Profils : le plafond compte « Default » même jamais écrit", "[app][profils]") {
  ProfileBook<Toy> book;
  for (std::size_t rank = 1; rank < moonclient::kProfileMost; ++rank) {
    book.Put("p" + std::to_string(rank), 0);
  }
  CHECK(book.ShownCount() == moonclient::kProfileMost);
  CHECK_FALSE(book.RoomFor("encore"));
  CHECK(book.RoomFor(kDefaultProfileName));
  CHECK(book.RoomFor("p1"));
}

TEST_CASE("Gardien : l'état présent s'enregistre dans le profil posé", "[app][profils]") {
  ToyPort port;
  moonclient::ProfileKeeper<Toy> keeper(port, &Text);
  keeper.Start(true);
  port.Change(4);
  keeper.Tick(1);
  CHECK(Stored(port, kDefaultProfileName) == nullptr);  // pas encore : il attend le calme
  keeper.Tick(kQuietLater);
  REQUIRE(Stored(port, kDefaultProfileName) != nullptr);
  CHECK(*Stored(port, kDefaultProfileName) == 4);
}

TEST_CASE("Gardien : chaque personnage pose le sien", "[app][profils]") {
  ToyPort port;
  port.disk.Put(kDefaultProfileName, 1);
  port.disk.Put("PvP", 9);
  port.disk.Bind("Stingor", "PvP");
  moonclient::ProfileKeeper<Toy> keeper(port, &Text);
  keeper.Start(true);
  CHECK(port.state == 1);
  keeper.EnterCharacter("Stingor");
  CHECK(port.state == 9);
  CHECK(keeper.active() == "PvP");
  keeper.EnterCharacter("Inconnu");
  CHECK(port.state == 1);
  CHECK(keeper.active() == kDefaultProfileName);
}

TEST_CASE("Gardien : les gestes de la page", "[app][profils]") {
  ToyPort port;
  port.disk.Put(kDefaultProfileName, 1);
  port.disk.Put("Farm", 5);
  moonclient::ProfileKeeper<Toy> keeper(port, &Text);
  keeper.Start(true);
  keeper.EnterCharacter("Stingor");

  // Les gestes passent par la page ; le banc les pose à la place du clic.
  const auto gesture = [&](ro::ui::ProfileGesture what, std::string target, std::string fresh) {
    keeper.Ask(ro::ui::ProfilesPageAction{.gesture = what, .target = target, .new_name = fresh});
    keeper.Tick(1);
  };

  SECTION("charger pose et retient pour le personnage") {
    gesture(ro::ui::ProfileGesture::kLoad, "Farm", "");
    CHECK(port.state == 5);
    CHECK(port.disk.ProfileOf("Stingor") == "Farm");
  }
  SECTION("dupliquer copie sans poser") {
    gesture(ro::ui::ProfileGesture::kDuplicate, "Farm", "Farm 2");
    REQUIRE(Stored(port, "Farm 2") != nullptr);
    CHECK(*Stored(port, "Farm 2") == 5);
    CHECK(port.state == 1);
    CHECK(keeper.active() == kDefaultProfileName);
  }
  SECTION("dupliquer ne remplace pas un profil existant") {
    gesture(ro::ui::ProfileGesture::kDuplicate, "Farm", kDefaultProfileName.data());
    CHECK(*Stored(port, kDefaultProfileName) == 1);
  }
  SECTION("renommer le profil posé le garde posé") {
    gesture(ro::ui::ProfileGesture::kLoad, "Farm", "");
    gesture(ro::ui::ProfileGesture::kRename, "Farm", "Ferme");
    CHECK(keeper.active() == "Ferme");
    CHECK(Stored(port, "Farm") == nullptr);
    CHECK(port.disk.ProfileOf("Stingor") == "Ferme");
  }
  SECTION("réinitialiser le profil posé remet l'état d'origine") {
    port.Change(3);
    gesture(ro::ui::ProfileGesture::kReset, kDefaultProfileName.data(), "");
    CHECK(port.state == kToyDefault);
    CHECK(*Stored(port, kDefaultProfileName) == kToyDefault);
  }
  SECTION("réinitialiser un autre profil ne touche pas l'état") {
    gesture(ro::ui::ProfileGesture::kReset, "Farm", "");
    CHECK(port.state == 1);
    CHECK(*Stored(port, "Farm") == kToyDefault);
  }
  SECTION("écraser « Default » depuis un autre profil le remplace sans le poser") {
    gesture(ro::ui::ProfileGesture::kLoad, "Farm", "");
    port.Change(6);
    gesture(ro::ui::ProfileGesture::kOverwrite, kDefaultProfileName.data(), "");
    CHECK(*Stored(port, kDefaultProfileName) == 6);
    CHECK(keeper.active() == "Farm");
    CHECK(port.disk.ProfileOf("Stingor") == "Farm");
  }
  SECTION("écraser un profil absent n'en crée pas") {
    gesture(ro::ui::ProfileGesture::kOverwrite, "Inconnu", "");
    CHECK(Stored(port, "Inconnu") == nullptr);
  }
  SECTION("supprimer le profil posé ramène à « Default »") {
    gesture(ro::ui::ProfileGesture::kLoad, "Farm", "");
    gesture(ro::ui::ProfileGesture::kDelete, "Farm", "");
    CHECK(Stored(port, "Farm") == nullptr);
    CHECK(keeper.active() == kDefaultProfileName);
    CHECK(port.state == 1);
  }
  SECTION("enregistrer sous pose le nouveau profil") {
    port.Change(7);
    gesture(ro::ui::ProfileGesture::kSaveAs, "", "PvP");
    CHECK(keeper.active() == "PvP");
    CHECK(*Stored(port, "PvP") == 7);
    CHECK(port.disk.ProfileOf("Stingor") == "PvP");
  }
}

TEST_CASE("Gardien : « Default » jamais écrit se retient avant qu'on le quitte", "[app][profils]") {
  // Premier lancement : aucun fichier. On entre, on enregistre sous un autre
  // nom, on y change l'état, puis on recharge « Défaut ».
  ToyPort port;
  port.state = 2;
  moonclient::ProfileKeeper<Toy> keeper(port, &Text);
  keeper.Start(true);
  keeper.EnterCharacter("Stingor");
  REQUIRE(Stored(port, kDefaultProfileName) != nullptr);
  CHECK(*Stored(port, kDefaultProfileName) == 2);

  keeper.Ask(
      ro::ui::ProfilesPageAction{.gesture = ro::ui::ProfileGesture::kSaveAs, .new_name = "PvP"});
  keeper.Tick(1);
  port.Change(8);
  keeper.Tick(kQuietLater);
  keeper.Tick(2 * kQuietLater);
  CHECK(*Stored(port, "PvP") == 8);

  keeper.Ask(ro::ui::ProfilesPageAction{.gesture = ro::ui::ProfileGesture::kLoad,
                                        .target = std::string(kDefaultProfileName)});
  keeper.Tick(3 * kQuietLater);
  CHECK(port.state == 2);
  CHECK(*Stored(port, kDefaultProfileName) == 2);
}

TEST_CASE("Gardien : rien ne s'écrit quand rien n'a changé", "[app][profils]") {
  ToyPort port;
  port.disk.Put(kDefaultProfileName, 1);
  moonclient::ProfileKeeper<Toy> keeper(port, &Text);
  keeper.Start(true);
  const int saves = port.saves;
  ++port.revision;  // une révision sans changement de l'état rangé
  keeper.Tick(1);
  keeper.Tick(kQuietLater);
  CHECK(port.saves == saves);
}

TEST_CASE("Gardien : un geste d'une partie ne passe pas à la suivante", "[app][profils]") {
  ToyPort port;
  port.disk.Put(kDefaultProfileName, 1);
  port.disk.Put("Farm", 5);
  moonclient::ProfileKeeper<Toy> keeper(port, &Text);
  keeper.Start(true);
  keeper.Ask(
      ro::ui::ProfilesPageAction{.gesture = ro::ui::ProfileGesture::kLoad, .target = "Farm"});
  keeper.EnterCharacter("Stingor");
  keeper.Tick(1);
  CHECK(port.state == 1);
}

// ── LES TOUCHES DANS LE PROFIL DE RÉGLAGES ─────────────────────────────────

namespace {

using moonclient::ActionRef;
using moonclient::Binding;
using moonclient::GameAction;
using moonclient::Hotkeys;
using moonclient::SettingsProfilePort;

// Un raccourci d'origine, et une case de barre qui n'en a pas.
constexpr GameAction kInventory = GameAction::kOpenInventory;
const ActionRef kFarSlot = ActionRef::OfShortcut(ro::sim::ShortcutIndex(7, 11));
const Binding kCtrlB{.key = SDLK_B, .ctrl = true};
const Binding kShiftAltQ{.key = SDLK_Q, .shift = true, .alt = true};

// Le banc : un réglage, la table vivante, le port et son gardien, sur un
// dossier de client.
struct HotkeyBench {
  ro::test_support::TempDirectory folder;
  bool fog = true;
  SettingsFile file{folder.path() / "settings.yml"};
  Hotkeys keys;
  std::unique_ptr<SettingsProfilePort> port;
  std::unique_ptr<moonclient::ProfileKeeper<SettingsValues>> keeper;

  // ⚠ LE PORT SE BÂTIT APRÈS QUE LE DISQUE EST PRÊT : il lit `hotkeys.yml`.
  void Open() {
    file.Declare("fog", &fog);
    file.Load();
    port = std::make_unique<SettingsProfilePort>(folder.path(), file, keys);
    keeper = std::make_unique<moonclient::ProfileKeeper<SettingsValues>>(*port, &Text);
    keeper->Start(false);
  }

  [[nodiscard]] moonclient::SettingsProfileBook Disk() const {
    moonclient::SettingsProfileBook book;
    REQUIRE(moonclient::LoadSettingsProfiles(folder.path(), book));
    return book;
  }

  void Ask(ro::ui::ProfileGesture gesture, std::string target, std::string fresh,
           std::uint32_t now_ms) {
    keeper->Ask(ro::ui::ProfilesPageAction{
        .gesture = gesture, .target = std::move(target), .new_name = std::move(fresh)});
    keeper->Tick(now_ms);
  }
};

}  // namespace

TEST_CASE("Touches : un changement s'enregistre dans le profil posé", "[app][profils]") {
  HotkeyBench bench;
  bench.Open();
  bench.keeper->EnterCharacter("Stingor");
  static_cast<void>(bench.keys.Bind(kFarSlot, kShiftAltQ));
  bench.keeper->Tick(1);
  bench.keeper->Tick(kQuietLater);

  const moonclient::SettingsProfileBook disk = bench.Disk();
  const moonclient::Named<SettingsValues>* kept = disk.Find(kDefaultProfileName);
  REQUIRE(kept != nullptr);
  const std::string key =
      std::string(moonclient::kProfileHotkeyPrefix) + moonclient::SavedKeyOfAction(kFarSlot);
  REQUIRE(kept->profile.contains(key));
  CHECK(kept->profile.at(key) == "alt+shift+Q");

  // Relu par une autre table, il rend le même raccourci.
  Hotkeys reread;
  SettingsProfilePort other(bench.folder.path(), bench.file, reread);
  other.Pose(kept->profile);
  CHECK(reread.Of(kFarSlot) == kShiftAltQ);
  CHECK(reread.Of(kInventory) == Hotkeys().Of(kInventory));
}

TEST_CASE("Touches : poser un autre profil change les touches", "[app][profils]") {
  HotkeyBench bench;
  bench.Open();
  const Binding factory = bench.keys.Of(kInventory);
  REQUIRE(factory != kCtrlB);

  // Stingor enregistre ses touches sous « PvP » ; Default garde l'origine.
  bench.keeper->EnterCharacter("Stingor");
  bench.Ask(ro::ui::ProfileGesture::kSaveAs, "", "PvP", 1);
  static_cast<void>(bench.keys.Bind(kInventory, kCtrlB));
  bench.keeper->Tick(2);
  bench.keeper->Tick(kQuietLater);

  SECTION("un autre personnage, sur Default, a les touches d'origine") {
    bench.keeper->EnterCharacter("Autre");
    CHECK(bench.keys.Of(kInventory) == factory);
    bench.keeper->EnterCharacter("Stingor");
    CHECK(bench.keys.Of(kInventory) == kCtrlB);
  }
  SECTION("« Charger » en cours de partie pose les touches tout de suite") {
    bench.Ask(ro::ui::ProfileGesture::kLoad, std::string(kDefaultProfileName), "", 2 * kQuietLater);
    CHECK(bench.keys.Of(kInventory) == factory);
    bench.Ask(ro::ui::ProfileGesture::kLoad, "PvP", "", 3 * kQuietLater);
    CHECK(bench.keys.Of(kInventory) == kCtrlB);
  }
  SECTION("réinitialiser rend les touches d'origine") {
    bench.Ask(ro::ui::ProfileGesture::kReset, "PvP", "", 2 * kQuietLater);
    CHECK(bench.keys.Of(kInventory) == factory);
  }
}

TEST_CASE("Touches : un profil sans touches hérite de hotkeys.yml", "[app][profils]") {
  HotkeyBench bench;
  // Ce que le joueur avait avant que les touches n'entrent au profil.
  Hotkeys legacy;
  static_cast<void>(legacy.Bind(kInventory, kCtrlB));
  REQUIRE(moonclient::SaveHotkeys(bench.folder.path(), legacy));
  moonclient::SettingsProfileBook old;
  old.Put(kDefaultProfileName, SettingsValues{{"fog", "false"}});
  old.Put("PvP", SettingsValues{{"fog", "true"}});
  REQUIRE(moonclient::SaveSettingsProfiles(bench.folder.path(), old));

  // La table vivante part de l'origine : c'est le profil qui doit l'emporter.
  bench.Open();
  REQUIRE(bench.keys.Of(kInventory) != kCtrlB);
  bench.keeper->EnterCharacter("Stingor");
  CHECK(bench.keys.Of(kInventory) == kCtrlB);
  CHECK_FALSE(bench.fog);

  // Un profil jamais posé l'emporte aussi : dupliqué, il garde ces touches.
  bench.Ask(ro::ui::ProfileGesture::kDuplicate, "PvP", "PvP 2", 1);
  const moonclient::SettingsProfileBook disk = bench.Disk();
  const moonclient::Named<SettingsValues>* copied = disk.Find("PvP 2");
  REQUIRE(copied != nullptr);
  Hotkeys reread;
  SettingsProfilePort other(bench.folder.path(), bench.file, reread);
  other.Pose(copied->profile);
  CHECK(reread.Of(kInventory) == kCtrlB);
}
