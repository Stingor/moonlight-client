// LA MISE EN PAGE DE LA TABLÉE SUR LE DISQUE : son format, lu et écrit, et la
// galerie de décors.

#include "shell/seat_layout_file.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using moonclient::ComposeGallery;
using moonclient::EmitSeatLayout;
using moonclient::ParseSeatLayout;
using moonclient::SeatLayoutBook;
using moonclient::SeatLayoutPreset;
using ro::ui::BackgroundOrigin;
using ro::ui::kBanquetSeats;

namespace {

// UN FICHIER ÉCRIT À LA MAIN : un en-tête, des cartes en flux, et une mise en
// page enregistrée. Deux sièges seulement — les autres gardent
// l'usine.
constexpr const char* kHandWrittenFile =
    "# Mise en page de l'écran de sélection de personnage.\n"
    "version: 1\n"
    "background: lobby\\taverne.bmp\n"
    "hide_empty_seats: true\n"
    "empty_seat_alpha: 0.4\n"
    "seats:\n"
    "  - {nx: 0.25, ny: 0.5, scale: 0.2, dir: 3, anim: 0, head_dir: 1, animate: false, "
    "frame: -1}\n"
    "  - {nx: 0.75, ny: 0.6, scale: 0.1, dir: 7, anim: 4, head_dir: -1, animate: true, "
    "frame: 2}\n"
    "anchors:\n"
    "  - {name: titre, nx: 0.5, ny: 0.05}\n"
    "  - {name: sortie, nx: 0.8, ny: 0.9}\n"
    "presets:\n"
    "  - name: Ma taverne\n"
    "    background: lobby\\cave.bmp\n"
    "    hide_empty_seats: false\n"
    "    empty_seat_alpha: 1\n"
    "    seats:\n"
    "      - {nx: 0.1, ny: 0.2, scale: 0.3, dir: 1, anim: 8, head_dir: 2, animate: false, "
    "frame: -1}\n"
    "    anchors: []\n";

}  // namespace

TEST_CASE("Tablée : un fichier écrit à la main se relit tel quel", "[app][seats][layout]") {
  const SeatLayoutBook book = ParseSeatLayout(kHandWrittenFile);
  CHECK(book.current.background == "lobby\\taverne.bmp");
  CHECK(book.current.hide_free_seats);
  CHECK(book.current.free_seat_alpha == 0.4F);
  CHECK(book.current.seats[0].x == 0.25F);
  CHECK(book.current.seats[0].height == 0.2F);
  CHECK(book.current.seats[0].direction == 3);
  CHECK(book.current.seats[0].animation == ro::ui::kSeatAnimStand);
  CHECK(book.current.seats[0].head_turn == 1);
  CHECK(book.current.seats[1].animated);
  CHECK(book.current.seats[1].frame == 2);
  // Les sièges que le fichier ne nomme pas gardent l'usine.
  CHECK(book.current.seats[2].x == kBanquetSeats[2].x);
  REQUIRE(book.current.anchors.size() == 2);
  CHECK(book.current.anchors[1].name == "sortie");
  CHECK(book.current.anchors[1].x == 0.8F);

  REQUIRE(book.presets.size() == 1);
  CHECK(book.presets[0].name == "Ma taverne");
  CHECK(book.presets[0].layout.background == "lobby\\cave.bmp");
  CHECK(book.presets[0].layout.seats[0].animation == ro::ui::kSeatAnimDie);
  CHECK(book.presets[0].layout.anchors.empty());
}

TEST_CASE("Tablée : écrire puis relire rend la même mise en page", "[app][seats][layout]") {
  SeatLayoutBook book;
  book.current.background = "lobby\\taverne.bmp";
  book.current.free_seat_alpha = 0.5F;
  book.current.seats[24].x = 0.123F;
  book.current.seats[24].head_turn = 2;
  book.current.seats[24].animated = true;
  book.current.anchors.push_back({.name = "pages", .x = 0.4F, .y = 0.7F});
  book.presets.push_back(SeatLayoutPreset{.name = "Salon", .layout = book.current});

  const SeatLayoutBook back = ParseSeatLayout(EmitSeatLayout(book));
  CHECK(back.current.background == book.current.background);
  CHECK(back.current.free_seat_alpha == 0.5F);
  CHECK(back.current.seats[24].x == 0.123F);
  CHECK(back.current.seats[24].head_turn == 2);
  CHECK(back.current.seats[24].animated);
  REQUIRE(back.current.anchors.size() == 1);
  CHECK(back.current.anchors[0].name == "pages");
  REQUIRE(back.presets.size() == 1);
  CHECK(back.presets[0].name == "Salon");
  CHECK(back.presets[0].layout.seats[24].x == 0.123F);
}

TEST_CASE("Tablée : un champ abîmé ne coûte que lui", "[app][seats][layout]") {
  // 🔴 UN FICHIER À MOITIÉ ÉCRIT DÉGRADE UNE PLACE, pas l'écran.
  const SeatLayoutBook book = ParseSeatLayout(
      "background: ..\\..\\secret.bmp\n"
      "empty_seat_alpha: 7\n"
      "seats:\n"
      "  - {nx: 40, ny: 0.5, scale: 0.2, anim: 99, dir: 12, head_dir: 5}\n"
      "  - {nx: 0.3, ny: 0.4, scale: 0.2}\n");
  CHECK(book.current.background == "lobby_hall.bmp");
  CHECK(book.current.free_seat_alpha == 1.0F);
  // Position et taille vont ensemble : une seule hors bornes les garde toutes.
  CHECK(book.current.seats[0].x == kBanquetSeats[0].x);
  CHECK(book.current.seats[0].height == kBanquetSeats[0].height);
  CHECK(book.current.seats[0].animation == ro::ui::kSeatAnimSit);
  CHECK(book.current.seats[0].direction == 0);
  CHECK(book.current.seats[0].head_turn == ro::ui::kHeadAlongBody);
  CHECK(book.current.seats[1].x == 0.3F);
}

TEST_CASE("Tablée : un texte qui n'est pas du YAML rend l'usine", "[app][seats][layout]") {
  const SeatLayoutBook book = ParseSeatLayout("seats: [ {nx: 0.3,, ]]");
  CHECK(book.current.seats[0].x == kBanquetSeats[0].x);
  CHECK(book.presets.empty());
}

TEST_CASE("Tablée : enregistrer sous un nom", "[app][seats][layout]") {
  std::vector<SeatLayoutPreset> presets;
  ro::ui::SeatLayout layout;
  layout.background = "lobby\\a.bmp";
  CHECK(moonclient::StorePreset(presets, "  Salon  ", layout) == 0);
  REQUIRE(presets.size() == 1);
  CHECK(presets[0].name == "Salon");

  // ⚠ LE MÊME NOM MET À JOUR, il ne double pas.
  layout.background = "lobby\\b.bmp";
  CHECK(moonclient::StorePreset(presets, "Salon", layout) == 0);
  CHECK(presets.size() == 1);
  CHECK(presets[0].layout.background == "lobby\\b.bmp");

  CHECK(moonclient::StorePreset(presets, "   ", layout) == -1);
  CHECK(moonclient::StorePreset(presets, std::string(moonclient::kPresetNameMax + 1, 'x'),
                                layout) == -1);

  for (std::size_t rank = presets.size(); rank < ro::ui::kPresetsMax; ++rank) {
    CHECK(moonclient::StorePreset(presets, "n" + std::to_string(rank), layout) >= 0);
  }
  CHECK(moonclient::StorePreset(presets, "de trop", layout) == -1);
  CHECK(presets.size() == ro::ui::kPresetsMax);
}

TEST_CASE("Tablée : appliquer ne touche pas aux places libres", "[app][seats][layout]") {
  std::vector<SeatLayoutPreset> presets;
  ro::ui::SeatLayout saved;
  saved.seats[0].x = 0.9F;
  saved.background = "lobby\\cave.bmp";
  saved.anchors.push_back({.name = "titre", .x = 0.1F, .y = 0.1F});
  saved.hide_free_seats = true;
  REQUIRE(moonclient::StorePreset(presets, "Cave", saved) == 0);

  ro::ui::SeatLayout current;
  current.free_seat_alpha = 0.3F;
  REQUIRE(moonclient::ApplyPreset(presets, 0, current));
  CHECK(current.seats[0].x == 0.9F);
  CHECK(current.background == "lobby\\cave.bmp");
  CHECK(current.anchors.size() == 1);
  CHECK_FALSE(current.hide_free_seats);
  CHECK(current.free_seat_alpha == 0.3F);

  CHECK_FALSE(moonclient::ApplyPreset(presets, 1, current));
  CHECK_FALSE(moonclient::DeletePreset(presets, -1));
  CHECK(moonclient::DeletePreset(presets, 0));
  CHECK(presets.empty());
}

TEST_CASE("Tablée : la galerie", "[app][seats][layout]") {
  const std::vector<std::string> disk = {"Zeste.bmp", "Taverne.BMP",
                                         "d\xC3\xA9"
                                         "cor.bmp",
                                         "notes.txt"};
  const std::vector<std::string> packed = {
      "data\\texture\\lobby\\taverne.bmp",  // le même que sur le disque
      "data\\texture\\lobby\\cave.bmp",
      "data\\texture\\lobby\\vieux\\grenier.bmp",  // un sous-dossier
      "data\\texture\\lobby_hall.bmp"};            // hors du dossier
  const moonclient::BackgroundGallery gallery = ComposeGallery(disk, packed);

  REQUIRE(gallery.entries.size() == 4);
  // L'usine en tête.
  CHECK(gallery.entries[0].origin == BackgroundOrigin::kFactory);
  CHECK(gallery.entries[0].path == "lobby_hall.bmp");
  // Puis triés par libellé, le disque gagnant sur l'archive.
  CHECK(gallery.entries[1].label == "Taverne");
  CHECK(gallery.entries[1].origin == BackgroundOrigin::kDisk);
  CHECK(gallery.entries[1].path == "lobby\\Taverne.BMP");
  CHECK(gallery.entries[2].label == "Zeste");
  CHECK(gallery.entries[3].label == "cave");
  CHECK(gallery.entries[3].origin == BackgroundOrigin::kPacked);
  CHECK(gallery.entries[3].path == "lobby\\cave.bmp");
  CHECK(gallery.packed == 1);
}

TEST_CASE("Tablée : l'usine tant que le fichier manque", "[app][seats][layout]") {
  const ro::test_support::TempDirectory root;
  // Premier lancement : l'usine.
  CHECK(moonclient::LoadSeatLayout(root.path()).current.background == "lobby_hall.bmp");

  SeatLayoutBook ours;
  ours.current.background = "lobby\\cave.bmp";
  REQUIRE(moonclient::SaveSeatLayout(root.path(), ours));
  CHECK(moonclient::LoadSeatLayout(root.path()).current.background == "lobby\\cave.bmp");
}
