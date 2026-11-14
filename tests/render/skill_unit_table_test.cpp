#include "ro/render/skill_unit_table.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/ascii.h"
#include "ro/render/effect_table.h"
#include "ro/render/skill_unit_sound_table.h"

using ro::render::kModelFolder;
using ro::render::SkillUnitEffect;
using ro::render::SkillUnitEffectEntry;
using ro::render::SkillUnitEffectTable;
using ro::render::SkillUnitEntry;
using ro::render::SkillUnitModelPath;
using ro::render::SkillUnitTable;
using ro::render::SkillUnitVisual;
using ro::render::SkillUnitVisualEntry;
using ro::render::SkillUnitVisualShape;
using ro::render::SkillUnitVisualTable;

TEST_CASE("Unités au sol : la table est triée et sans doublon", "[render][skillunit]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable, et un doublon rendrait l'un des deux inatteignable. Le relevé
  // se refait quand le client change ; rien ne garantit qu'il sortira dans le
  // même ordre.
  const std::span<const SkillUnitEntry> table = SkillUnitTable();
  REQUIRE(!table.empty());
  std::uint16_t previous = 0;
  bool first = true;
  for (const SkillUnitEntry& entry : table) {
    // Le chemin porte son extension : le client emploie les deux versions du
    // format, et la deviner nous ferait chercher le mauvais fichier.
    CHECK((entry.model.ends_with(".rsm") || entry.model.ends_with(".rsm2")));
    if (!first) {
      CHECK(previous < entry.unit_id);
    }
    previous = entry.unit_id;
    first = false;
  }
}

TEST_CASE("Unités au sol : un identifiant connu donne son modèle", "[render][skillunit]") {
  // Plusieurs unités partagent un modèle — le client range trois mines sous le
  // même dessin —, et les pièges de quatrième classe sont d'une autre version
  // du format.
  CHECK(SkillUnitModelPath(145) == std::string(kModelFolder) + "외부소품\\트랩01.rsm");
  CHECK(SkillUnitModelPath(144) == std::string(kModelFolder) + "외부소품\\트랩02.rsm");
  CHECK(SkillUnitModelPath(143) == SkillUnitModelPath(218));
  CHECK(SkillUnitModelPath(20854) ==
        std::string(kModelFolder) + "effect\\swifttrap\\swi_trap.rsm2");
}

TEST_CASE("Unités au sol : une unité que rien ne dessine ne rend rien", "[render][skillunit]") {
  // Toutes les unités n'ont pas de modèle : un mur de feu, un sanctuaire se
  // voient par leur effet. Et l'unité MUETTE est celle par laquelle le serveur
  // cache ce qu'il ne veut pas montrer.
  CHECK(SkillUnitModelPath(0).empty());
  CHECK(SkillUnitModelPath(126).empty());
  CHECK(SkillUnitModelPath(100000).empty());
}

TEST_CASE("Unités au sol : l'effet se range sous l'UNITÉ, pas sous la compétence",
          "[render][skillunit]") {
  // Le client range cet effet avec le modèle de l'unité, dans le même
  // répartiteur. Chercher par compétence ne le trouve pas — c'est ainsi que
  // Pneuma restait sans image alors que son fichier est dans l'archive.
  const std::span<const SkillUnitEffectEntry> table = SkillUnitEffectTable();
  REQUIRE(!table.empty());
  for (std::size_t index = 1; index < table.size(); ++index) {
    CHECK(table[index - 1].unit_id < table[index].unit_id);
  }
  for (const SkillUnitEffectEntry& entry : table) {
    CHECK(entry.effect_id != 0);
    // Le crible du générateur : ne garder que ce qui a un fichier.
    CHECK_FALSE(ro::render::EffectFile(entry.effect_id).empty());
  }
}

TEST_CASE("Unités au sol : chaque effet parle de son unité", "[render][skillunit]") {
  // Les deux couples classiques, vérifiés par leur nom : l'unité 133 est celle
  // de Pneuma, la 148 celle du piège à onde de choc.
  CHECK(ro::render::EffectFile(SkillUnitEffect(133)).starts_with("Pneuma"));
  CHECK(ro::render::EffectFile(SkillUnitEffect(148)) == "ShockWave.str");

  // Une unité sans effet ne rend rien, et un modèle n'en tient pas lieu : les
  // deux vont ensemble quand ils sont là.
  CHECK(SkillUnitEffect(0) == 0);
  CHECK(SkillUnitEffect(100000) == 0);
  CHECK_FALSE(SkillUnitModelPath(145).empty());
}

TEST_CASE("Unités au sol : la table des visuels entretenus tient ses invariants",
          "[render][skillunit]") {
  const std::span<const SkillUnitVisualEntry> table = SkillUnitVisualTable();
  REQUIRE(!table.empty());
  for (std::size_t index = 1; index < table.size(); ++index) {
    CHECK(table[index - 1].unit_id < table[index].unit_id);
  }
  for (const SkillUnitVisualEntry& entry : table) {
    // Une entrée sans effet n'aurait rien à entretenir : le générateur ne la
    // garde pas, et une table qui en porterait une ferait semer du vide.
    CHECK(entry.effect_id != 0);
    // Un effet PERMANENT ne se relance pas chez le client : son retrait n'a
    // donc aucun sens, et le relevé le pose à zéro.
    if (entry.shape == SkillUnitVisualShape::kPermanent) {
      CHECK(entry.withhold_ticks == 0);
    }
  }
}

TEST_CASE("Unités au sol : les chants et les danses se montrent, comme chez le joueur",
          "[render][skillunit]") {
  // Le client du joueur est patché : les dix-huit unités de chant suivent le
  // chemin de Land Protector, chacune avec son effet. Sans cela, un barde qui
  // chante ne laisse rien au sol.
  const SkillUnitVisualEntry* protector = SkillUnitVisual(157);
  REQUIRE(protector != nullptr);
  CHECK(protector->effect_id == 242);

  const SkillUnitVisualEntry* first_song = SkillUnitVisual(158);
  REQUIRE(first_song != nullptr);
  CHECK(first_song->effect_id == 278);
  CHECK(first_song->shape == SkillUnitVisualShape::kPermanent);

  // ⚠ LA NEUVIÈME N'EST PAS DANS LA SUITE : l'unité 166 prend 277, et la
  // numérotation reprend à 286 derrière elle.
  const SkillUnitVisualEntry* ninth = SkillUnitVisual(166);
  REQUIRE(ninth != nullptr);
  CHECK(ninth->effect_id == 277);
  const SkillUnitVisualEntry* last_song = SkillUnitVisual(175);
  REQUIRE(last_song != nullptr);
  CHECK(last_song->effect_id == 294);

  // L'unité qui suit n'est pas une chanson, et le patch ne la touche pas.
  CHECK(SkillUnitVisual(176) == nullptr);
}

TEST_CASE("Unités au sol : l'entretien n'est pas la pose", "[render][skillunit]") {
  // Le client sépare les deux moments, et dans deux fonctions différentes :
  // le répartiteur de l'unité joue ce qui se voit à l'APPARITION, son tic
  // entretient ce qui se voit ENSUITE. Confondre les deux fait chercher le mur
  // de feu dans la table de la pose, où il n'a jamais été.
  const SkillUnitVisualEntry* firewall = SkillUnitVisual(127);
  REQUIRE(firewall != nullptr);
  CHECK(firewall->effect_id == 25);
  CHECK(firewall->shape == SkillUnitVisualShape::kRepeated);
  CHECK(firewall->withhold_ticks == 7);
  CHECK(SkillUnitEffect(127) == 0);

  // Le sanctuaire, lui, pose un nœud que le client garde : rien à relancer.
  const SkillUnitVisualEntry* sanctuary = SkillUnitVisual(131);
  REQUIRE(sanctuary != nullptr);
  CHECK(sanctuary->effect_id == 319);
  CHECK(sanctuary->shape == SkillUnitVisualShape::kPermanent);

  // Et l'inverse existe : Pneuma se voit à la pose et rien ne l'entretient.
  CHECK(SkillUnitEffect(133) != 0);
  CHECK(SkillUnitVisual(133) == nullptr);

  CHECK(SkillUnitVisual(0) == nullptr);
  CHECK(SkillUnitVisual(100000) == nullptr);
}

TEST_CASE("Unités au sol : un visuel peut dépendre du NIVEAU", "[render][skillunit]") {
  // Celui-là n'est pas dans le cas du tic : l'apparition choisit l'identifiant
  // selon le niveau et le range dans un champ de l'acteur, que le tic consomme.
  // Le chercher dans la table ordinaire fait conclure que l'unité n'a rien.
  CHECK(ro::render::SkillUnitVisual(268) == nullptr);
  CHECK(ro::render::SkillUnitLevelVisual(268, 1) == 1602);
  CHECK(ro::render::SkillUnitLevelVisual(268, 3) == 1604);
  CHECK(ro::render::SkillUnitLevelVisual(268, 5) == 1606);

  // LE NIVEAU DÉBORDE vers le dernier connu : le serveur hausse des compétences
  // au-delà de ce que le client relevé connaît, et une unité muette serait pire
  // qu'une image d'un rang trop bas.
  CHECK(ro::render::SkillUnitLevelVisual(268, 11) == 1606);

  CHECK(ro::render::SkillUnitLevelVisual(127, 1) == 0);
  CHECK(ro::render::SkillUnitLevelVisual(0, 1) == 0);
}

TEST_CASE("Unités au sol : le bruit d'une unité n'est pas celui de son effet",
          "[render][skillunit]") {
  // Le client le joue depuis le cas de l'UNITÉ, et non depuis la fonction de
  // l'effet : la table des sons d'effet n'en couvre aucun, et les y chercher ne
  // donnait rien.
  const ro::render::SkillUnitSoundEntry* firewall = ro::render::SkillUnitPlacementSound(127);
  REQUIRE(firewall != nullptr);
  CHECK(firewall->sound == "effect\\EF_FireWall.wav");
  CHECK(firewall->one_in == 1);

  // Un son TIRÉ AU SORT : le client masque son rand par 0x80000007, et celui-ci
  // ne s'entend donc qu'une fois sur huit. Un piège qui crépiterait à chaque
  // passe serait insupportable là où il y en a dix.
  const ro::render::SkillUnitSoundEntry* drawn = ro::render::SkillUnitPlacementSound(189);
  REQUIRE(drawn != nullptr);
  CHECK(drawn->one_in == 8);

  // La pose et l'entretien sont deux tables : Severe Rainstorm ne sonne qu'en
  // durant, Firewall ne sonne qu'en arrivant.
  CHECK(ro::render::SkillUnitSustainSound(127) == nullptr);
  const ro::render::SkillUnitSoundEntry* pluie = ro::render::SkillUnitSustainSound(219);
  REQUIRE(pluie != nullptr);
  CHECK(pluie->sound == "effect\\rainstorm.wav");
  CHECK(ro::render::SkillUnitPlacementSound(219) == nullptr);

  for (const ro::render::SkillUnitSoundEntry& entry : ro::render::SkillUnitPlacementSoundTable()) {
    CHECK_FALSE(entry.sound.empty());
    CHECK(entry.one_in >= 1);
  }
  for (std::size_t index = 1; index < ro::render::SkillUnitSustainSoundTable().size(); ++index) {
    CHECK(ro::render::SkillUnitSustainSoundTable()[index - 1].unit_id <
          ro::render::SkillUnitSustainSoundTable()[index].unit_id);
  }
}

TEST_CASE("Unités au sol : les effets NOMMÉS d'une fabrique", "[render][skillunit]") {
  const std::span<const ro::render::SkillUnitNamedEffectEntry> table =
      ro::render::SkillUnitNamedEffectTable();
  REQUIRE(!table.empty());

  // Triée par unité PUIS par moment : les deux recherches rendent une PLAGE, et
  // une entrée hors d'ordre couperait cette plage en deux.
  for (std::size_t index = 1; index < table.size(); ++index) {
    const ro::render::SkillUnitNamedEffectEntry& before = table[index - 1];
    const ro::render::SkillUnitNamedEffectEntry& after = table[index];
    CHECK(before.unit_id <= after.unit_id);
    if (before.unit_id == after.unit_id) {
      CHECK(before.moment <= after.moment);
    }
  }
  for (const ro::render::SkillUnitNamedEffectEntry& entry : table) {
    // Une entrée par NUMÉRO n'a pas de nom, et c'est ce qui la distingue.
    if (entry.effect_id != 0) {
      CHECK(entry.name.empty());
      continue;
    }
    CHECK_FALSE(entry.name.empty());
    // Le client abaisse la clé avant de la ranger, et ces noms servent de clé.
    CHECK(entry.name == ro::core::AsciiLower(entry.name));
  }

  // ⚠ L'ORDRE DANS UNE UNITÉ EST CELUI DES SEMIS de sa fabrique, et il n'est ni
  // alphabétique ni celui des données : la fabrique de Jack Frost Nova sème le
  // fond du départ, puis le départ, puis le fond de la boucle, puis la boucle.
  const std::span<const ro::render::SkillUnitNamedEffectEntry> nova =
      ro::render::SkillUnitNamedEffects(295, ro::render::SkillUnitNamedMoment::kOnPlace);
  REQUIRE(nova.size() == 4);
  CHECK(nova[0].name == "jack_frost_nova_start_bottom");
  CHECK(nova[1].name == "jack_frost_nova_start");
  CHECK(nova[2].name == "jack_frost_nova_loop_bottom");
  CHECK(nova[3].name == "jack_frost_nova_loop");

  const std::span<const ro::render::SkillUnitNamedEffectEntry> kunai =
      ro::render::SkillUnitNamedEffects(298, ro::render::SkillUnitNamedMoment::kOnPlace);
  REQUIRE(kunai.size() == 2);
  CHECK(kunai[0].name == "kunaiarea_01_start");
  CHECK(kunai[1].name == "kunaiarea_01_loop");

  // ⚠ LA LEVÉE EST UN SECOND MOMENT, et l'ignorer laisse l'unité disparaître
  // sans sa dernière image. Jack Frost Nova ne fait que semer sa fin ; le totem
  // COUPE d'abord ses deux boucles, en les désignant par leur nom, avant de
  // semer les siennes.
  const std::span<const ro::render::SkillUnitNamedEffectEntry> fin_nova =
      ro::render::SkillUnitNamedEffects(295, ro::render::SkillUnitNamedMoment::kOnVanish);
  REQUIRE(fin_nova.size() == 1);
  CHECK(fin_nova[0].name == "jack_frost_nova_end");
  CHECK(fin_nova[0].verb == ro::render::NamedEffectVerb::kSow);

  const std::span<const ro::render::SkillUnitNamedEffectEntry> fin_totem =
      ro::render::SkillUnitNamedEffects(292, ro::render::SkillUnitNamedMoment::kOnVanish);
  REQUIRE(fin_totem.size() == 4);
  CHECK(fin_totem[0].name == "totem_of_tutelary_bottom_loop");
  CHECK(fin_totem[0].verb == ro::render::NamedEffectVerb::kCut);
  CHECK(fin_totem[1].name == "totem_of_tutelary_bottom_end");
  CHECK(fin_totem[1].verb == ro::render::NamedEffectVerb::kSow);
  CHECK(fin_totem[2].verb == ro::render::NamedEffectVerb::kCut);
  CHECK(fin_totem[3].name == "totem_of_tutelary_end");

  // ⚠ CESSER A DEUX FORMES. Ground Gravitation n'est pas coupée : le client
  // CHERCHE ses boucles vivantes et lève le drapeau qui les empêche de
  // repartir, si bien que l'image en cours finit son cycle.
  const std::span<const ro::render::SkillUnitNamedEffectEntry> fin_gravitation =
      ro::render::SkillUnitNamedEffects(296, ro::render::SkillUnitNamedMoment::kOnVanish);
  REQUIRE(fin_gravitation.size() == 2);
  for (const ro::render::SkillUnitNamedEffectEntry& entry : fin_gravitation) {
    CHECK(entry.verb == ro::render::NamedEffectVerb::kStop);
  }

  // Toute unité qui se pose se lève : une table qui n'aurait que le premier
  // moment est exactement le défaut que ce test garde.
  for (const ro::render::SkillUnitNamedEffectEntry& entry : table) {
    if (entry.moment != ro::render::SkillUnitNamedMoment::kOnPlace) {
      continue;
    }
    // Une classe posée par NUMÉRO est attachée à l'unité et meurt avec elle :
    // sa fabrique n'a rien à lever.
    if (entry.effect_id != 0) {
      continue;
    }
    // Cross Rain est la seule dont la fabrique ne joue rien à la levée.
    if (entry.unit_id == 275 || entry.unit_id == 291 || entry.unit_id == 293) {
      continue;
    }
    CHECK_FALSE(ro::render::SkillUnitNamedEffects(entry.unit_id,
                                                  ro::render::SkillUnitNamedMoment::kOnVanish)
                    .empty());
  }

  // Une unité qui ne passe pas par une fabrique n'a aucun nom : Firewall se
  // voit par son entretien, et le chercher ici ne doit rien rendre.
  CHECK(ro::render::SkillUnitNamedEffects(127).empty());

  // ⚠ CES UNITÉS-LÀ N'ONT NI EFFET D'APPARITION NI EFFET D'ENTRETIEN. Leur cas
  // tombe sur le défaut de la table de sauts, et sans le chemin des noms elles
  // ne montrent rien du tout. C'est la raison d'être de cette table.
  for (const ro::render::SkillUnitNamedEffectEntry& entry : table) {
    CHECK(SkillUnitEffect(entry.unit_id) == 0);
    CHECK(SkillUnitVisual(entry.unit_id) == nullptr);
  }
}

TEST_CASE("Unités au sol : une fabrique sème aussi par numéro, à la pose, le niveau en paramètre",
          "[render][skillunit]") {
  using ro::render::NamedEffectVerb;
  using ro::render::SkillUnitNamedEffectEntry;
  using ro::render::SkillUnitNamedEffects;
  using ro::render::SkillUnitNamedMoment;
  // Twinkling Galaxy (286) : la classe 2349 sur l'unité, le niveau en paramètre.
  const std::span<const SkillUnitNamedEffectEntry> galaxy =
      SkillUnitNamedEffects(286, SkillUnitNamedMoment::kOnPlace);
  REQUIRE(galaxy.size() == 1);
  CHECK(galaxy.front().effect_id == 2349);
  CHECK(galaxy.front().level_as_param);
  CHECK(galaxy.front().name.empty());
  CHECK(galaxy.front().verb == NamedEffectVerb::kSow);
  // Cross Rain (275) : son nom, puis 2165 par la méthode que la famille partage.
  const std::span<const SkillUnitNamedEffectEntry> rain =
      SkillUnitNamedEffects(275, SkillUnitNamedMoment::kOnPlace);
  REQUIRE(rain.size() == 2);
  CHECK(rain[0].name == "new_cross_rain_bottom");
  CHECK(rain[1].effect_id == 2165);
  CHECK(rain[1].level_as_param);
  // Le shuriken de NPC_FUUMA (290) naît chez celui qui a posé le piège, sans niveau.
  const std::span<const SkillUnitNamedEffectEntry> fuuma =
      SkillUnitNamedEffects(290, SkillUnitNamedMoment::kOnPlace);
  REQUIRE(fuuma.size() == 1);
  CHECK(fuuma.front().effect_id == 2352);
  CHECK(fuuma.front().born_at_creator);
  CHECK_FALSE(fuuma.front().level_as_param);
  // Mission Bombard (291) : trois noms, puis 2354.
  const std::span<const SkillUnitNamedEffectEntry> bombard =
      SkillUnitNamedEffects(291, SkillUnitNamedMoment::kOnPlace);
  REQUIRE(bombard.size() == 4);
  CHECK(bombard.back().effect_id == 2354);
  CHECK_FALSE(bombard.back().level_as_param);
  // Rien par numéro à la levée, et jamais de nom sur un numéro.
  std::size_t numbered = 0;
  for (const SkillUnitNamedEffectEntry& entry : ro::render::SkillUnitNamedEffectTable()) {
    if (entry.effect_id == 0) {
      continue;
    }
    ++numbered;
    CHECK(entry.moment == SkillUnitNamedMoment::kOnPlace);
    CHECK(entry.name.empty());
  }
  CHECK(numbered == 6);
}
