// LES SONS D'IMAGE TUS : la clé d'un fichier, et ce que la page en fait.
//
// ⚠ UNE CLÉ MAL FORMÉE NE SE VOIT PAS : le son qu'on a coché continue de jouer,
// et rien ne dit pourquoi.

#include "options/sprite_sound_options.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "world/sprite_sound_mutes.h"

using moonclient::IsSpriteSoundRow;
using moonclient::kDeathSounds;
using moonclient::kSpriteSoundRowBase;
using moonclient::SilencesFile;
using moonclient::SoundFileKey;
using moonclient::SpriteSoundMutes;
using moonclient::SpriteSoundOptions;
using moonclient::SpriteSoundRow;

namespace {

[[nodiscard]] int RowOf(SpriteSoundRow row) { return kSpriteSoundRowBase + static_cast<int>(row); }

}  // namespace

TEST_CASE("Sons tus : la clé est le nom seul, en minuscules", "[app][son]") {
  CHECK(SoundFileKey("doppleganger_die.wav") == "doppleganger_die.wav");
  CHECK(SoundFileKey("Doppleganger_DIE.wav") == "doppleganger_die.wav");
  CHECK(SoundFileKey("effect\\ef_hit2.wav") == "ef_hit2.wav");
  CHECK(SoundFileKey("monster/Wolf_Die.wav") == "wolf_die.wav");
}

TEST_CASE("Sons tus : un fichier tu se tait où qu'il soit rangé", "[app][son]") {
  const std::vector<std::string> files = {"doppleganger_die.wav"};
  const SpriteSoundMutes mutes{.files = files};
  CHECK(SilencesFile(mutes, "doppleganger_die.wav"));
  CHECK(SilencesFile(mutes, "monster\\DOPPLEGANGER_DIE.wav"));
  CHECK_FALSE(SilencesFile(mutes, "baphomet_die.wav"));
  CHECK_FALSE(SilencesFile(SpriteSoundMutes{}, "doppleganger_die.wav"));
}

TEST_CASE("Sons tus : les réglages du login reconnaissent toutes les lignes", "[app][son]") {
  // ⚠ LE DERNIER SON DE MORT COMPRIS : c'est lui qu'une borne fausse d'un cran
  // laisserait au login.
  CHECK(IsSpriteSoundRow(RowOf(SpriteSoundRow::kPetIdle)));
  CHECK(IsSpriteSoundRow(RowOf(SpriteSoundRow::kDeathIntro)));
  CHECK(IsSpriteSoundRow(RowOf(SpriteSoundRow::kFirstDeath) +
                         static_cast<int>(kDeathSounds.size()) - 1));
  CHECK_FALSE(IsSpriteSoundRow(RowOf(SpriteSoundRow::kCount)));
  CHECK_FALSE(IsSpriteSoundRow(kSpriteSoundRowBase - 1));
}

TEST_CASE("Sons tus : la page coupe les morts et l'attente des pets", "[app][son]") {
  SpriteSoundOptions page;
  // Rien n'est tu par défaut.
  CHECK(page.mutes().files.empty());
  CHECK_FALSE(page.mutes().pet_idle);

  CHECK(page.Change({.changed = RowOf(SpriteSoundRow::kFirstDeath), .on = true}));
  CHECK(page.Change({.changed = RowOf(SpriteSoundRow::kPetIdle), .on = true}));
  const SpriteSoundMutes mutes = page.mutes();
  CHECK(mutes.pet_idle);
  // Le premier de la table, celui du Doppelganger et du Bio Lab.
  CHECK(SilencesFile(mutes, kDeathSounds.front().file));
  CHECK_FALSE(SilencesFile(mutes, kDeathSounds.back().file));

  // Une ligne d'une autre page n'est pas la sienne.
  CHECK_FALSE(page.Change({.changed = kSpriteSoundRowBase - 1, .on = true}));
}
