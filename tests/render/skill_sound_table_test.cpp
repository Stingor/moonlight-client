#include "ro/render/skill_sound_table.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_sound_table.h"

using ro::render::EffectSoundEntry;
using ro::render::SkillSound;
using ro::render::SkillSoundEntry;
using ro::render::SkillSounds;
using ro::render::SkillSoundTable;

TEST_CASE("Sons de compétence : la table est triée et sans doublon", "[render][skillsound]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable, et un doublon rendrait l'un des deux inatteignable.
  const std::span<const SkillSoundEntry> table = SkillSoundTable();
  REQUIRE(!table.empty());
  std::uint16_t previous = 0;
  bool first = true;
  for (const SkillSoundEntry& entry : table) {
    CHECK(entry.sound.ends_with(".wav"));
    if (!first) {
      CHECK(previous < entry.skill_id);
    }
    previous = entry.skill_id;
    first = false;
  }
}

TEST_CASE("Sons de compétence : le nom du son suit celui de la compétence",
          "[render][skillsound]") {
  // Ce sont ces couples-là qui jugent la table : un identifiant
  // tiré du mauvais switch aurait donné un son sans rapport.
  CHECK(SkillSound(2038) == "effect\\AB_judex.wav");     // AB_JUDEX
  CHECK(SkillSound(2040) == "effect\\AB_adoramus.wav");  // AB_ADORAMUS
  CHECK(SkillSound(2243) == "wug_strike.wav");           // RA_WUGSTRIKE
  CHECK(SkillSound(2309) == "effect\\LG_TRAMPLE.wav");   // LG_TRAMPLE
}

TEST_CASE("Sons de compétence : le répartiteur SANS DÉGÂTS a le sien", "[render][skillsound]") {
  // 🔴 LE SON D'UNE COMPÉTENCE SANS DÉGÂTS N'EST DANS AUCUNE TABLE DU CLIENT :
  // son répartiteur le joue EN DUR, dans le corps de son cas, en son 3D. Sans
  // cette entrée, Stone Curse tombe sans bruit : ni cette table ni celle des
  // effets ne répond.
  CHECK(SkillSound(16) == "_stonecurse.wav");  // MG_STONECURSE

  // Et il se juge comme le reste : par le nom.
  CHECK(SkillSound(2039) == "effect\\AB_ancilla.wav");        // AB_ANCILLA
  CHECK(SkillSound(2287) == "effect\\shadowform.wav");        // SC_SHADOWFORM
  CHECK(SkillSound(2318) == "effect\\LG_PRESTIGE.wav");       // LG_PRESTIGE
  CHECK(SkillSound(2517) == "effect\\SR_HOWLINGOFLION.wav");  // SR_HOWLINGOFLION

  // ⚠ ET LE SILENCE DE L'EFFET EST CE QUI LE REND NÉCESSAIRE. `SkillSounds`
  // descend à l'effet quand la compétence n'a pas de son propre ; l'effet 23
  // n'en a aucun, si bien que le chemin entier restait muet.
  CHECK(ro::render::EffectSounds(23).empty());
  const std::vector<EffectSoundEntry> pierre = SkillSounds(16);
  REQUIRE(pierre.size() == 1);
  CHECK(pierre.front().sound == "_stonecurse.wav");
}

TEST_CASE("Sons de compétence : le silence n'est pas une absence de bruit",
          "[render][skillsound]") {
  // La plupart des compétences classiques ne sont pas dans cette table : leur
  // bruit vient de l'ANIMATION qui les joue, pas du code du client. Rendre une
  // chaîne vide dit « pas ici », et non « muette ».
  CHECK(SkillSound(0).empty());
  CHECK(SkillSound(5).empty());
  CHECK(SkillSound(100000).empty());
}

TEST_CASE("Sons de compétence : un effet sonne à ses tics, pas tout au départ",
          "[render][skillsound]") {
  // 🔴 LE TIC FAIT PARTIE DU SON. Le bâtisseur de Provoke joue le même fichier
  // DEUX fois, à deux instants ; les rendre sans leur tic les jouait ensemble
  // dans la même image — un coup épais au lieu de deux battements.
  const std::vector<EffectSoundEntry> provoke = SkillSounds(6);  // SM_PROVOKE
  REQUIRE(provoke.size() == 2);
  CHECK(provoke[0].sound == "effect\\swordman_provoke.wav");
  CHECK(provoke[1].sound == provoke[0].sound);
  CHECK(provoke[0].delay_ticks != provoke[1].delay_ticks);
}

TEST_CASE("Sons de compétence : la troupe qui sonne sonne seule", "[render][skillsound]") {
  // ⚠ Soul Strike joue son bruit DANS LA GARDE DU SEMIS, une fois par boule.
  // Le rendre aussi au départ ferait entendre six coups pour cinq, et c'est la
  // règle que le chemin des effets nus appliquait déjà seul.
  CHECK(SkillSounds(13).empty());  // MG_SOULSTRIKE, effet 15
}

TEST_CASE("Sons de compétence : le son propre remplace celui de l'effet", "[render][skillsound]") {
  // Le client joue celui-ci dans le cas de son switch, avant d'atteindre le
  // répartiteur d'effets : il ne s'ajoute pas, il prend la place.
  const std::vector<EffectSoundEntry> wug = SkillSounds(2243);  // RA_WUGSTRIKE
  REQUIRE(wug.size() == 1);
  CHECK(wug.front().sound == SkillSound(2243));
  CHECK(wug.front().delay_ticks == 0);
}

TEST_CASE("Sons de compétence : aucune compétence ne veut dire aucun bruit",
          "[render][skillsound]") {
  // Zéro est ce que porte un coup ORDINAIRE : il a son propre bruit, et
  // chercher celui d'une compétence absente rendrait celui de l'effet zéro.
  CHECK(SkillSounds(0).empty());
}

TEST_CASE("Sons de compétence : un chemin qui ne sème rien ne sonne pas", "[render][skillsound]") {
  using ro::render::SkillSoundsOnPath;
  constexpr std::uint16_t kHiding = 51;
  constexpr std::uint16_t kResurrection = 54;
  // L'effet des deux existe, et il sonne : le repli le trouvait.
  CHECK_FALSE(SkillSounds(kHiding).empty());
  CHECK_FALSE(SkillSounds(kResurrection).empty());
  // 🔴 Hiding n'a d'effet que sur le chemin des dégâts : son annonce — à
  // l'entrée comme à la sortie — faisait entendre l'effet 16 qu'elle ne pose
  // pas.
  CHECK(SkillSoundsOnPath(kHiding, /*on_hit=*/false).empty());
  // 🔴 Resurrection n'a d'effet qu'à l'annonce : le coup porté à un
  // mort-vivant faisait entendre la compétence.
  CHECK(SkillSoundsOnPath(kResurrection, /*on_hit=*/true).empty());
  CHECK_FALSE(SkillSoundsOnPath(kResurrection, /*on_hit=*/false).empty());
}
