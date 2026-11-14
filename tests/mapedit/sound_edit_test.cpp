// LES SONS D'AMBIANCE DU DOCUMENT D'ÉDITION : lus, écrits, refusés, et la
// retouche qui vise sa source.

#include <array>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/mapedit/map_edit.h"

using ro::mapedit::AddedSound;
using ro::mapedit::LitHours;
using ro::mapedit::MapEdit;
using ro::mapedit::SoundEdit;

namespace {

constexpr const char* kMap = "prontera";

// Un document sans autre opération que ses sons : `body` est la fin du texte.
std::string DocumentWith(const std::string& body) {
  return "version: 1\nmap: 'prontera'\nmodels: []\n" + body;
}

}  // namespace

TEST_CASE("Sons : le document les écrit et les relit à l'identique", "[mapedit]") {
  MapEdit edit;
  edit.SetSoundEdit(SoundEdit{.sound_index = 3,
                              .wave_file = "_ambience\\fontaine.wav",
                              .origin = {10.0F, -2.0F, 30.5F},
                              .position = std::array<float, 3>{12.0F, -2.0F, 31.0F},
                              .hidden = false,
                              .volume = 0.4F,
                              .range = 80.0F,
                              .cycle_seconds = 6.5F,
                              .night_only = true,
                              .hours = LitHours{.from_hour = 21.0F, .to_hour = 5.5F},
                              .why = "la fontaine s'entend trop loin"});
  edit.SetSoundEdit(SoundEdit{
      .sound_index = 1, .wave_file = "vent.wav", .origin = {}, .hidden = true, .why = "muet"});
  edit.AddSound(AddedSound{.wave_file = "_ambience\\grillon.wav",
                           .position = {5.0F, 0.0F, -7.25F},
                           .volume = 0.75F,
                           .width = 20,
                           .height = 0,
                           .range = 150.0F,
                           .cycle_seconds = 3.0F,
                           .night_only = true,
                           .why = "les grillons du parc"});
  edit.AddSound(AddedSound{.wave_file = "foule.wav", .position = {}, .why = "le marché"});
  CHECK_FALSE(edit.empty());
  // Rangées par rang, quel que soit l'ordre de pose.
  REQUIRE(edit.sound_edits().size() == 2);
  CHECK(edit.sound_edits()[0].sound_index == 1);

  const std::string text = ro::mapedit::MapEditToYaml(edit, kMap);
  auto reread = ro::mapedit::ParseMapEdit(text, kMap);
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);

  // Retirer le premier fait remonter le second ; la retouche se retire par rang.
  edit.RemoveSound(0);
  REQUIRE(edit.sounds().size() == 1);
  CHECK(edit.sounds()[0].why == "le marché");
  edit.RemoveSoundEdit(3);
  CHECK(edit.SoundEditAt(3) == nullptr);
  CHECK(edit.SoundEditAt(1) != nullptr);
}

TEST_CASE("Sons : une opération malformée refuse le document", "[mapedit]") {
  // Un son ajouté sans portée, à portée nulle, à volume hors de 0 à 1, à cycle
  // nul, sans raison, à emprise négative.
  for (const char* body : {
           "sounds:\n  - file: 'a.wav'\n    position: [0, 0, 0]\n    why: \"x\"\n",
           "sounds:\n  - file: 'a.wav'\n    position: [0, 0, 0]\n    range: 0\n    why: \"x\"\n",
           "sounds:\n  - file: 'a.wav'\n    position: [0, 0, 0]\n    range: 5\n"
           "    volume: 1.5\n    why: \"x\"\n",
           "sounds:\n  - file: 'a.wav'\n    position: [0, 0, 0]\n    range: 5\n"
           "    cycle: 0\n    why: \"x\"\n",
           "sounds:\n  - file: 'a.wav'\n    position: [0, 0, 0]\n    range: 5\n    why: \"\"\n",
           "sounds:\n  - file: 'a.wav'\n    position: [0, 0, 0]\n    range: 5\n"
           "    size: [-1, 4]\n    why: \"x\"\n",
           // Une retouche sans origine, à position de deux composantes, à plage
           // d'heures hors du jour.
           "map_sounds:\n  - index: 0\n    file: 'a.wav'\n    why: \"x\"\n",
           "map_sounds:\n  - index: 0\n    file: 'a.wav'\n    origin: [0, 0, 0]\n"
           "    position: [1, 2]\n    why: \"x\"\n",
           "map_sounds:\n  - index: 0\n    file: 'a.wav'\n    origin: [0, 0, 0]\n"
           "    hours: [20, 30]\n    why: \"x\"\n",
           "map_sounds: 3\n",
       }) {
    CAPTURE(body);
    CHECK_FALSE(ro::mapedit::ParseMapEdit(DocumentWith(body), kMap).has_value());
  }
}

TEST_CASE("Sons : un son ajouté prend ses défauts pour ce qu'il ne dit pas", "[mapedit]") {
  auto read = ro::mapedit::ParseMapEdit(
      DocumentWith("sounds:\n  - file: 'a.wav'\n    position: [1, 2, 3]\n    range: 40\n"
                   "    why: \"x\"\n"),
      kMap);
  REQUIRE(read.has_value());
  REQUIRE(read->sounds().size() == 1);
  const AddedSound& sound = read->sounds()[0];
  CHECK(sound.volume == ro::mapedit::kSoundVolumeMost);
  CHECK(sound.width == AddedSound::kDefaultFootprint);
  CHECK(sound.range == 40.0F);
  CHECK(sound.cycle_seconds == ro::formats::kRswDefaultSoundCycleSeconds);
  CHECK_FALSE(sound.night_only);
}

TEST_CASE("Sons : la retouche ne vise que sa source, fichier et origine", "[mapedit]") {
  const SoundEdit retouch{
      .sound_index = 0, .wave_file = "eau.wav", .origin = {10.0F, 0.0F, 20.0F}, .why = "x"};
  CHECK(ro::mapedit::SoundEditTargets(retouch, "eau.wav", {10.0F, 0.0F, 20.0F}));
  // L'arrondi d'une position recopiée passe.
  CHECK(ro::mapedit::SoundEditTargets(retouch, "eau.wav", {10.005F, 0.0F, 20.0F}));
  // Un autre fichier, une source déplacée par un patch : la retouche ne vaut plus.
  CHECK_FALSE(ro::mapedit::SoundEditTargets(retouch, "vent.wav", {10.0F, 0.0F, 20.0F}));
  CHECK_FALSE(ro::mapedit::SoundEditTargets(retouch, "eau.wav", {11.0F, 0.0F, 20.0F}));
}
