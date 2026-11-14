#include "ro/render/skill_effect_table.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/effect_table.h"
#include "ro/render/plain_hit_effect_table.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_named_table.h"
#include "ro/render/skill_suite_table.h"

using ro::render::EffectFile;
using ro::render::SkillEffectEntry;
using ro::render::SkillEffectTable;
using ro::render::SkillImplFor;

TEST_CASE("Effets de compétence : la table est triée et sans doublon", "[render][skilleffect]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable, et un doublon rendrait l'un des deux inatteignable.
  const std::span<const SkillEffectEntry> table = SkillEffectTable();
  REQUIRE(!table.empty());
  std::uint16_t previous = 0;
  bool first = true;
  for (const SkillEffectEntry& entry : table) {
    CHECK(entry.effect_id != 0);
    if (!first) {
      CHECK(previous < entry.skill_id);
    }
    previous = entry.skill_id;
    first = false;
  }
}

TEST_CASE("Effets de compétence : chaque effet se dessine OU s'entend", "[render][skilleffect]") {
  // C'est tout l'intérêt du crible, et il compte TROIS façons : un effet entre
  // ici s'il a un FICHIER, si l'une de nos tables sait le DESSINER, ou s'il
  // fait un BRUIT. Un identifiant qui n'aurait aucune des trois n'arriverait à
  // aucun sens du joueur, et rien ne dirait pourquoi.
  //
  // 🔴 LA TROISIÈME COMPTE AUTANT QUE LES DEUX AUTRES.
  // `SkillImplFor(id).EffectId()` est aussi ce que consulte le joueur de SONS :
  // une compétence écartée d'ici deviendrait muette en plus d'être invisible.
  // 21 compétences sont dans ce cas, dont les quatre chants du barde. Le son
  // n'est pas l'otage du dessin : ces 21 entrées ne sèment rien et s'entendent.
  //
  // ⚠ N'EXIGER QUE LE FICHIER LAISSAIT 59 COMPÉTENCES SUR 182 au chemin des
  // dégâts ; elles sont 164 depuis que le crible compte les deux façons —
  // Triple Attack, Arrow Vulcan et les trois quarts de la mêlée en étaient.
  //
  // ⚠ ET CE TEST GARDE L'AUTRE BORD. Le crible a d'abord compté comme dessiné
  // tout ce qui se MONTRE, effets d'acteur compris — une teinte, un bit du mot
  // de rendu, une pose ne posent aucune géométrie, et `PlaySkillOn` n'a rien à
  // semer pour eux. Quarante-huit compétences sont entrées ainsi, et ce sont
  // ces quarante-huit qui ont fait échouer ce test.
  for (const SkillEffectEntry& entry : SkillEffectTable()) {
    CHECK((!EffectFile(entry.effect_id).empty() ||
           ro::render::KnowsProceduralEffect(entry.effect_id) ||
           !ro::render::EffectSounds(entry.effect_id).empty()));
  }
}

TEST_CASE("Effets de compétence : le troisième cran RELÂCHE ce qu'on apprend à dessiner",
          "[render][skilleffect]") {
  // ⚠ LE TROISIÈME CRAN NE DOIT PAS SE CONFONDRE AVEC LES DEUX AUTRES. Il ne
  // prétend pas que l'effet se dessine : il dit qu'il s'entend, et rien de plus.
  //
  // NV_FIRSTAID (142) joue l'effet 309, l'étincelle de soin. Son nœud de
  // sous-type 58 a un mouvement porté : l'effet se DESSINE, et il sonne aussi.
  // L'essai prouve qu'on sort du cran de repli dès que le dessin est porté.
  //
  // C'est la forme que doit prendre un cran de repli : on y entre par ce qui
  // manque, on en sort en le portant, et l'essai suit le mouvement au lieu de
  // figer un état.
  const std::uint16_t first_heal = SkillImplFor(142).EffectId();
  CHECK(first_heal == 309);
  // Sans fichier : tout son visuel est PROCÉDURAL.
  CHECK(EffectFile(first_heal).empty());
  CHECK(ro::render::KnowsProceduralEffect(first_heal));
  // Et il sonne toujours : les deux ne s'excluent pas.
  REQUIRE_FALSE(ro::render::EffectSounds(first_heal).empty());
  CHECK(ro::render::EffectSounds(first_heal).front().sound == "_heal_effect.wav");
}

TEST_CASE("Effets de compétence : le nom de l'effet suit celui de la compétence",
          "[render][skilleffect]") {
  // Ces couples jugent la table : un identifiant tiré du mauvais cas donnerait
  // un effet sans rapport avec la compétence.
  // Les trois sorts au sol du sorcier dans leur variante PLEINE, celle du
  // réglage par défaut : `/mineffect` leur substitue MeteorStorm, Quagmire et
  // lord_of_vermillion, que le chemin de la case choisit à part.
  CHECK(EffectFile(SkillImplFor(83).EffectId()).starts_with("Meteor"));    // WZ_METEOR
  CHECK(EffectFile(SkillImplFor(92).EffectId()).starts_with("Quagmire"));  // WZ_QUAGMIRE
  CHECK(EffectFile(SkillImplFor(85).EffectId()) == "Lord.str");            // WZ_VERMILION

  // Et les compétences classiques, que la SECONDE table du client nomme : sans
  // elle, on concluait à tort qu'elles n'avaient toutes qu'un effet procédural.
  CHECK(EffectFile(SkillImplFor(20).EffectId()) == "Lightning.str");        // MG_LIGHTNINGBOLT
  CHECK(EffectFile(SkillImplFor(21).EffectId()) == "ThunderStorm.str");     // MG_THUNDERSTORM
  CHECK(EffectFile(SkillImplFor(56).EffectId()) == "Pierce.str");           // KN_PIERCE
  CHECK(EffectFile(SkillImplFor(58).EffectId()) == "SpearStab.str");        // KN_SPEARSTAB
  CHECK(EffectFile(SkillImplFor(59).EffectId()) == "SpearBoomerang.str");   // KN_SPEARBOOMERANG
  CHECK(EffectFile(SkillImplFor(80).EffectId()) == "FirePillar.str");       // WZ_FIREPILLAR
  CHECK(EffectFile(SkillImplFor(89).EffectId()) == "StormGust.str");        // WZ_STORMGUST
  CHECK(EffectFile(SkillImplFor(136).EffectId()) == "SonicBlow.str");       // AS_SONICBLOW
  CHECK(EffectFile(SkillImplFor(153).EffectId()) == "CartRevolution.str");  // MC_CARTREVOLUTION
  CHECK(EffectFile(SkillImplFor(175).EffectId()) == "suicide.str");         // NPC_SUICIDE
  CHECK(EffectFile(SkillImplFor(253).EffectId()) == "holy_cross.str");      // CR_HOLYCROSS
}

TEST_CASE("Effets de compétence : zéro veut dire « rien à montrer »", "[render][skilleffect]") {
  // Zéro ne dit plus « pas de fichier » mais « rien que nous sachions dessiner
  // » : une compétence que la table ne connaît pas, ou dont aucun effet n'a de
  // table.
  CHECK(SkillImplFor(0).EffectId() == 0);
  CHECK(SkillImplFor(1).EffectId() == 0);
  CHECK(SkillImplFor(100000).EffectId() == 0);
  // ⚠ ET COLD BOLT N'EN EST PLUS. Son effet est dessiné en code, sans le
  // moindre fichier — c'est pour cela qu'il servait d'exemple à l'ancienne
  // règle —, et nous savons le faire depuis le portage des familles
  // procédurales.
  CHECK(SkillImplFor(14).EffectId() == 26);
  CHECK(ro::render::KnowsProceduralEffect(SkillImplFor(14).EffectId()));
  CHECK(EffectFile(SkillImplFor(14).EffectId()).empty());
}

TEST_CASE("Effets de compétence : la famille acolyte et prêtre a enfin les siens",
          "[render][skilleffect]") {
  // Elles ne portent AUCUN coup : le serveur les annonce par son paquet de
  // compétence sans dégâts, et c'est un autre répartiteur du client qui en fait
  // naître l'effet. Tant qu'on ne lisait que celui des dégâts, toute la famille
  // restait muette — et rien ne disait pourquoi.
  //
  // Chaque couple se vérifie par son nom, comme les autres.
  CHECK(EffectFile(SkillImplFor(66).EffectId()) == "Impositio.str");   // PR_IMPOSITIO
  CHECK(EffectFile(SkillImplFor(68).EffectId()) == "Aspersio.str");    // PR_ASPERSIO
  CHECK(EffectFile(SkillImplFor(71).EffectId()) == "slowp.str");       // PR_SLOWPOISON
  CHECK(EffectFile(SkillImplFor(72).EffectId()) == "Recovery.str");    // PR_STRECOVERY
  CHECK(EffectFile(SkillImplFor(74).EffectId()) == "Magnificat.str");  // PR_MAGNIFICAT
  CHECK(EffectFile(SkillImplFor(75).EffectId()) == "Gloria.str");      // PR_GLORIA
  CHECK(EffectFile(SkillImplFor(76).EffectId()) == "LexDivina.str");   // PR_LEXDIVINA
  CHECK(EffectFile(SkillImplFor(78).EffectId()) == "LexAeterna.str");  // PR_LEXAETERNA
  CHECK(EffectFile(SkillImplFor(361).EffectId()) == "asum.str");       // HP_ASSUMPTIO

  // 🔴 ASSUMPTIO NAÎT SUR LA CIBLE : ses deux effets — 375 et 440 — naissent sur
  // l'acteur du `targetAID`, donc sur qui REÇOIT le renfort. Voir
  // `SkillEffectOn`.
  CHECK(SkillImplFor(361).EffectOn() == ro::render::SkillEffectOn::kTarget);

  // ⚠ HEAL ET BLESSING ONT ENFIN LEUR IMAGE, et par l'autre chemin : leurs
  // effets n'ont de fichier dans aucune des deux tables du client — ils sont
  // dessinés en code —, et ce sont nos tables procédurales qui les portent
  // aujourd'hui. Ils servaient d'exemple au manque ; ils servent maintenant de
  // preuve que le second chemin compte.
  CHECK(SkillImplFor(28).EffectId() != 0);
  CHECK(EffectFile(SkillImplFor(28).EffectId()).empty());
  CHECK(ro::render::KnowsProceduralEffect(SkillImplFor(28).EffectId()));
  CHECK(SkillImplFor(34).EffectId() != 0);
  CHECK(ro::render::KnowsProceduralEffect(SkillImplFor(34).EffectId()));
}

TEST_CASE("Effets de compétence : les coups multiples suivent le même chemin",
          "[render][skilleffect]") {
  // ⚠ TRIPLE ATTACK, ARROW VULCAN ET SONIC BLOW NAISSENT DU MÊME RÉPARTITEUR —
  // celui des DÉGÂTS — et tous trois sur le LANCEUR. Arrow Vulcan joue même un
  // effet de la famille de Triple Attack, le trait gainé rose.
  //
  // Aucun des trois n'était en table tant que le crible exigeait un fichier :
  // deux n'en ont pas, et le troisième perdait le sien au profit du `.str`.
  CHECK(SkillImplFor(263).EffectId() == 329);  // MO_TRIPLEATTACK
  CHECK(SkillImplFor(263).EffectOn() == ro::render::SkillEffectOn::kCaster);
  CHECK(SkillImplFor(394).EffectId() == 393);  // CG_ARROWVULCAN
  CHECK(SkillImplFor(394).EffectOn() == ro::render::SkillEffectOn::kCaster);
  // ⚠ SONIC BLOW EN A DEUX — 121 et 143 —, et la table n'en porte qu'un : sa
  // forme donne une entrée par compétence, et `MergedFacts` avance index par
  // index. Le fichier garde la préséance, donc c'est `SonicBlow.str` qui sort.
  CHECK(SkillImplFor(136).EffectId() == 143);  // AS_SONICBLOW
  CHECK(EffectFile(SkillImplFor(136).EffectId()) == "SonicBlow.str");
}

TEST_CASE("Chemin des dégâts : une compétence a jusqu'à trois effets, un par chemin",
          "[render][skilleffect]") {
  using ro::render::SkillDamageEffectOf;
  // 🔴 MAGNUM BREAK MONTRE SES ANNEAUX UNE FOIS, à l'annonce du sort et sur le
  // lanceur : c'est le répartiteur des compétences SANS DÉGÂTS qui les pose.
  // Ce que son coup pose, c'est l'anneau de choc ordinaire, EF_BASH.
  CHECK(SkillImplFor(7).EffectId() == 17);  // EF_MAGNUMBREAK, à l'annonce
  const ro::render::SkillDamageEffectEntry* hit = SkillDamageEffectOf(7);
  REQUIRE(hit != nullptr);
  CHECK(hit->effect_id == 16);  // EF_BASH, à l'impact
  CHECK(hit->on == ro::render::SkillEffectOn::kCaster);

  // ⚠ ET HEAL EN A DEUX, DIFFÉRENTS. Il s'annonce par un effet (325) et frappe
  // les morts-vivants par un autre (EF_HEAL3, 320) : une seule réponse par
  // compétence ne peut pas dire les deux, et la table fondue donnait la
  // préséance au coup — un soin ordinaire montrait donc l'effet du coup.
  const ro::render::SkillDamageEffectEntry* heal = SkillDamageEffectOf(28);
  REQUIRE(heal != nullptr);
  CHECK(heal->effect_id == 320);
  CHECK(heal->on == ro::render::SkillEffectOn::kTarget);
  const ro::render::SkillAnnouncedEffectEntry* dit = ro::render::SkillAnnouncedEffectOf(28);
  REQUIRE(dit != nullptr);
  CHECK(dit->effect_id == 325);
  CHECK(dit->on == ro::render::SkillEffectOn::kTarget);
  // Et Magnum Break s'annonce bien par ses anneaux, sur qui le lance.
  const ro::render::SkillAnnouncedEffectEntry* rings = ro::render::SkillAnnouncedEffectOf(7);
  REQUIRE(rings != nullptr);
  CHECK(rings->effect_id == 17);  // EF_MAGNUMBREAK

  // ⚠ ET UNE COMPÉTENCE QUI NE FRAPPE PAS N'A RIEN À CE CHEMIN. Blessing
  // s'annonce et c'est tout ; quatre cent trente-deux des quatre cent
  // cinquante-quatre compétences du répartiteur d'annonce sont dans ce cas, et
  // c'est autant d'effets que le coup ne doit pas rejouer.
  CHECK(SkillDamageEffectOf(34) == nullptr);  // AL_BLESSING
  CHECK(SkillDamageEffectOf(74) == nullptr);  // PR_MAGNIFICAT
  // Et l'inverse : Cold Bolt frappe et ne s'annonce pas.
  CHECK(ro::render::SkillAnnouncedEffectOf(14) == nullptr);
}

TEST_CASE("Chemin de l'annonce : le porteur se lit sur la VALEUR, pas sur le registre",
          "[render][skilleffect]") {
  using ro::render::SkillAnnouncedEffectOf;
  using ro::render::SkillEffectOn;
  // 🔴 UNE COMPÉTENCE SANS DÉGÂTS A DEUX ACTEURS : celui du `srcAID`, LE
  // LANCEUR, et celui du `targetAID`, LA CIBLE. Chaque effet naît sur l'un ou
  // sur l'autre, et le chemin des dégâts ne suit pas la même convention : les
  // confondre inverse la colonne.
  //
  // Les témoins ci-dessous se vérifient par le jeu, dans les deux sens.

  // Stone Curse pétrifie ce qu'il vise, et sa pierre — l'effet 23 — tombe là.
  const ro::render::SkillAnnouncedEffectEntry* pierre = SkillAnnouncedEffectOf(16);
  REQUIRE(pierre != nullptr);
  CHECK(pierre->effect_id == 23);  // EF_STONECURSE
  CHECK(pierre->on == SkillEffectOn::kTarget);

  // CE QUI TOMBE SUR QUI REÇOIT : le soin, le renfort, la résurrection.
  for (const auto& [skill, each_effect_id] : std::initializer_list<std::pair<int, int>>{
           {28, 325},   // AL_HEAL
           {29, 37},    // AL_INCAGI
           {34, 42},    // AL_BLESSING
           {54, 77},    // ALL_RESURRECTION
           {361, 440},  // HP_ASSUMPTIO
       }) {
    const ro::render::SkillAnnouncedEffectEntry* table_entry =
        SkillAnnouncedEffectOf(static_cast<std::uint16_t>(skill));
    REQUIRE(table_entry != nullptr);
    CHECK(table_entry->effect_id == each_effect_id);
    CHECK(table_entry->on == SkillEffectOn::kTarget);
  }

  // ET CE QUI NAÎT SUR QUI LANCE : l'explosion qu'on porte, la posture qu'on
  // prend, l'eau bénite qu'on fabrique.
  for (const auto& [skill, each_effect_id] : std::initializer_list<std::pair<int, int>>{
           {7, 17},   // SM_MAGNUM
           {8, 11},   // SM_ENDURE
           {31, 39},  // AL_HOLYWATER
           {32, 40},  // AL_CRUCIS
           {33, 41},  // AL_ANGELUS
       }) {
    const ro::render::SkillAnnouncedEffectEntry* table_entry =
        SkillAnnouncedEffectOf(static_cast<std::uint16_t>(skill));
    REQUIRE(table_entry != nullptr);
    CHECK(table_entry->effect_id == each_effect_id);
    CHECK(table_entry->on == SkillEffectOn::kCaster);
  }

  // ⚠ ET LA COLONNE PORTE LES DEUX RÉPONSES. Une table qui ne dirait qu'un mot
  // aurait l'air juste tout en ne l'étant pas.
  int on_caster = 0;
  int on_target = 0;
  for (const ro::render::SkillAnnouncedEffectEntry& table_entry :
       ro::render::SkillAnnouncedEffectTable()) {
    (table_entry.on == SkillEffectOn::kCaster ? on_caster : on_target) += 1;
  }
  CHECK(on_caster > 100);
  CHECK(on_target > 100);
}

TEST_CASE("Chemin des dégâts : l'effet visé naît sur la SOURCE du coup", "[render][skilleffect]") {
  using ro::render::SkillDamageEffectOf;
  // Le client le fait naître sur l'acteur qui reçoit le message 37 — et pour
  // une compétence posée au sol, cette source est l'UNITÉ, donc la case visée.
  const ro::render::SkillDamageEffectEntry* gust = SkillDamageEffectOf(89);
  REQUIRE(gust != nullptr);
  CHECK(gust->effect_id == 89);  // WZ_STORMGUST
  CHECK(gust->on == ro::render::SkillEffectOn::kCaster);

  // ⚠ CE QUI VIENT DU RÉPARTITEUR DES DÉGÂTS GARDE SON PROPRE BOUT, et il n'est
  // pas toujours le lanceur : les flèches de Cold Bolt tombent sur ce qu'elles
  // gèlent, et la table le dit compétence par compétence.
  const ro::render::SkillDamageEffectEntry* bolt = SkillDamageEffectOf(14);
  REQUIRE(bolt != nullptr);
  CHECK(bolt->effect_id == 26);
  CHECK(bolt->on == ro::render::SkillEffectOn::kTarget);
  // Et les boules de Soul Strike partent bien du mage.
  const ro::render::SkillDamageEffectEntry* strike = SkillDamageEffectOf(13);
  REQUIRE(strike != nullptr);
  CHECK(strike->effect_id == 15);
  CHECK(strike->on == ro::render::SkillEffectOn::kCaster);
}

TEST_CASE("Chemin des dégâts : la table est triée et sans doublon", "[render][skilleffect]") {
  const std::span<const ro::render::SkillDamageEffectEntry> table =
      ro::render::SkillDamageEffectTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].skill_id < table[rang].skill_id);
  }
  // Et tout ce qu'elle nomme arrive à un sens du joueur : un effet en table qui
  // ne pose rien ET ne fait aucun bruit laisserait le coup nu sans que rien ne
  // le dise. Le troisième terme est le même que celui de la table fondue — voir
  // « chaque effet se dessine OU s'entend ».
  for (const ro::render::SkillDamageEffectEntry& table_entry : table) {
    CHECK((!EffectFile(table_entry.effect_id).empty() ||
           ro::render::KnowsProceduralEffect(table_entry.effect_id) ||
           !ro::render::EffectSounds(table_entry.effect_id).empty()));
  }
}

TEST_CASE("Effet de frappe : l'élément du sort décide de l'impact", "[render][skillhit]") {
  using ro::render::SkillHitEffect;
  // Cold Bolt et Frost Diver posent la FUMÉE BLANCHE DU FROID sur ce qu'ils
  // touchent — EF_COLDHIT, une colonne de glace et une fumée qui grossit —, et
  // c'est un SECOND effet : les flèches de glace, elles, sont l'effet 26.
  CHECK(SkillHitEffect(14, true, false) == 51);
  CHECK(SkillHitEffect(15, true, false) == 51);
  // Le feu pose le sien, la foudre le sien.
  CHECK(SkillHitEffect(7, true, false) == 49);   // SM_MAGNUM
  CHECK(SkillHitEffect(17, true, false) == 49);  // MG_FIREBALL
  CHECK(SkillHitEffect(19, true, false) == 49);  // MG_FIREBOLT
  CHECK(SkillHitEffect(20, true, false) == 52);  // MG_LIGHTNINGBOLT
  // Et tout le reste du classique la gerbe ordinaire, EF_HIT1 — qui vaut ZÉRO,
  // ce que rien ne distingue d'un trou si l'on n'y prend garde.
  CHECK(SkillHitEffect(13, true, false) == 0);  // MG_SOULSTRIKE
  CHECK(SkillHitEffect(11, true, false) == 0);  // MG_NAPALMBEAT
  CHECK(SkillHitEffect(21, true, false) == 0);  // MG_THUNDERSTORM

  // LA FUMÉE DU FROID ET LA GERBE ORDINAIRE SE DESSINENT : un impact connu que
  // nous ne saurions pas montrer ne se distinguerait pas d'un impact absent, et
  // la table dirait pourtant oui.
  CHECK(ro::render::KnowsProceduralEffect(51));  // EF_COLDHIT
  CHECK(ro::render::KnowsProceduralEffect(0));   // EF_HIT1
  // ⚠ L'ÉCLAT DU FEU ET CELUI DE LA FOUDRE NE SONT PAS PROCÉDURAUX : ce sont
  // des fichiers, `FireHit%d.str` et `WindHit%d.str`, à trois variantes, que le
  // chemin des fichiers dessine. La troupe ne les connaît donc pas, et le
  // catalogue les nomme.
  CHECK(!ro::render::KnowsProceduralEffect(49));  // EF_FIREHIT
  CHECK(!ro::render::KnowsProceduralEffect(52));  // EF_WINDHIT
  CHECK(!ro::render::EffectFile(49).empty());
  CHECK(!ro::render::EffectFile(52).empty());
}

TEST_CASE("Effet de frappe : une UNITÉ au sol a sa propre table", "[render][skillhit]") {
  using ro::render::SkillUnitHitEffect;
  // 🔴 LE BLIZZARD NE RENAÎT PAS À CHAQUE BATTEMENT. Quand la source d'un coup
  // est une unité posée au sol, le client délivre le message à un autre
  // répartiteur, qui ne pose AUCUN effet de compétence : Storm Gust n'y laisse
  // que la fumée du froid sur ce qu'il gèle.
  CHECK(SkillUnitHitEffect(89) == 51);   // WZ_STORMGUST -> EF_COLDHIT
  CHECK(SkillUnitHitEffect(18) == 49);   // MG_FIREWALL -> EF_FIREHIT
  CHECK(SkillUnitHitEffect(85) == 52);   // WZ_VERMILION -> EF_WINDHIT
  CHECK(SkillUnitHitEffect(79) == 152);  // WZ_QUAGMIRE
  CHECK(SkillUnitHitEffect(428) == 583);
  CHECK(SkillUnitHitEffect(5321) == 2131);  // par la carte des dégâts

  // ⚠ ET SA RÈGLE PAR DÉFAUT N'EST PAS CELLE DE L'AUTRE TABLE : une seule
  // borne, pas de plage muette au milieu.
  CHECK(SkillUnitHitEffect(87) == 0);   // la gerbe ordinaire
  CHECK(SkillUnitHitEffect(440) == 0);  // là où l'autre table se tait
  CHECK(SkillUnitHitEffect(493) == -1);
  CHECK(SkillUnitHitEffect(3000) == -1);
  // Les deux tables diffèrent bel et bien sur Storm Gust : par le chemin des
  // acteurs il n'a que la gerbe, par celui des unités il a le froid.
  CHECK(ro::render::SkillHitEffect(89, true, false) == 0);
}

TEST_CASE("Effet de frappe : la règle par défaut a deux plages muettes", "[render][skillhit]") {
  using ro::render::SkillHitEffect;
  // Les identifiants de 411 à 472 ne posent RIEN, et le moderne non plus.
  CHECK(ro::render::SkillHitEffect(411, true, false) == -1);
  CHECK(ro::render::SkillHitEffect(472, true, false) == -1);
  CHECK(ro::render::SkillHitEffect(3000, true, false) == -1);
  // Mais 467 et 468 sont dans la plage muette ET nommés : le nom l'emporte.
  CHECK(SkillHitEffect(467, true, false) == 549);
  CHECK(SkillHitEffect(468, true, false) == 549);
  // Juste avant et juste après la plage, la gerbe revient.
  CHECK(SkillHitEffect(410, true, false) == 0);
  CHECK(SkillHitEffect(473, true, false) == 0);
  CHECK(SkillHitEffect(492, true, false) == 0);
  // ⚠ 397 SE TAIT, et une lecture à l'œil l'avait manqué : le client l'envoie
  // au même point que les cas 56 à 59. C'est l'oracle qui l'a rendu.
  CHECK(SkillHitEffect(397, true, false) == -1);
  CHECK(SkillHitEffect(347, true, false) == -1);
  CHECK(SkillHitEffect(129, true, false) == -1);
}

TEST_CASE("Effet de frappe : deux compétences lisent le coup et non leur numéro",
          "[render][skillhit]") {
  using ro::render::SkillHitEffect;
  // 54 ne pose son effet que si la frappe a fait des dégâts.
  CHECK(SkillHitEffect(54, true, false) == 152);
  CHECK(SkillHitEffect(54, false, false) == 0);
  // 2323 ne le pose que si QUELQU'UN D'AUTRE l'a lancée.
  CHECK(SkillHitEffect(2323, true, false) == 1708);
  CHECK(SkillHitEffect(2323, true, true) == -1);
}

TEST_CASE("Effet de frappe : cinq sortes de dégâts sur sept le montrent", "[render][skillhit]") {
  using ro::render::DamageTypeShowsHitEffect;
  constexpr std::array<std::uint8_t, 5> kMontrent = {4, 6, 8, 9, 13};
  for (const std::uint8_t kind : kMontrent) {
    CHECK(DamageTypeShowsHitEffect(kind));
  }
  // La gerbe et la quatorzième repartent avant le choix de l'effet.
  CHECK(!DamageTypeShowsHitEffect(5));
  CHECK(!DamageTypeShowsHitEffect(14));
  // Et un coup ordinaire ne passe pas par ce répartiteur du tout.
  CHECK(!DamageTypeShowsHitEffect(0));
  CHECK(!DamageTypeShowsHitEffect(10));
}

TEST_CASE("Effet de frappe : une poignée de compétences le montrent aussi à la sorte 5",
          "[render][skillhit]") {
  using ro::render::DamageTypeShowsHitEffect;
  constexpr std::uint16_t kNommee = 5215;
  constexpr std::uint16_t kBash = 5;
  CHECK(DamageTypeShowsHitEffect(5, kNommee));
  // Pas à la quatorzième, que le client ne lui ouvre pas.
  CHECK(!DamageTypeShowsHitEffect(14, kNommee));
  // Et une compétence ordinaire reste muette à la sorte 5.
  CHECK(!DamageTypeShowsHitEffect(5, kBash));
  CHECK(DamageTypeShowsHitEffect(4, kBash));
  const std::span<const ro::render::SplashHitSkill> table = ro::render::SplashHitSkillTable();
  CHECK(std::ranges::is_sorted(table, {}, &ro::render::SplashHitSkill::skill_id));
}

TEST_CASE("Effet de frappe : les éclaboussés d'Arrow Shower et de Napalm Beat le reçoivent",
          "[render][skillhit]") {
  using ro::render::DamageTypeShowsHitEffect;
  // Ces compétences laissent leurs victimes alentour au chemin ordinaire, aux
  // deux sortes qui éclaboussent.
  constexpr std::uint16_t kArrowShower = 47;
  constexpr std::uint16_t kNapalmBeat = 11;
  for (const std::uint16_t skill_id : {kArrowShower, kNapalmBeat}) {
    CHECK(DamageTypeShowsHitEffect(5, skill_id));
    CHECK(DamageTypeShowsHitEffect(14, skill_id));
  }
}

TEST_CASE("Effet de frappe : la table est triée et sans doublon", "[render][skillhit]") {
  const std::span<const ro::render::SkillHitEffectEntry> table = ro::render::SkillHitEffectTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].skill_id < table[rang].skill_id);
  }
  // Les entrées conditionnelles vivent À PART : les ranger avec les autres les
  // ferait répondre sans qu'on ait lu le coup.
  for (const ro::render::SkillHitEffectWhen& selon : ro::render::SkillHitEffectConditions()) {
    CHECK(std::ranges::find(table, selon.skill_id, &ro::render::SkillHitEffectEntry::skill_id) ==
          table.end());
  }
}

TEST_CASE("Effets de compétence : le message de dégâts en porte un SECOND",
          "[render][skilleffect]") {
  // 🔴 LE MÊME MESSAGE PORTE DEUX EFFETS, et ils s'ajoutent. Bash pose la gerbe
  // ordinaire par la règle de l'effet de frappe ET l'effet 1 par la carte du
  // message ; c'est le second qui sonne, et sans lui Bash serait muet.
  CHECK(ro::render::SkillHitEffect(5, true, true) == 0);  // EF_HIT1, la gerbe
  CHECK(ro::render::SkillDamageMsgEffect(5) == 1);        // EF_HIT2, qui sonne

  // Des couples témoins, qui prouvent qu'aucun n'est pris au mauvais cran : la
  // valeur est écrite APRÈS l'appel d'insertion, et la lire avant décalerait
  // toute la table.
  CHECK(ro::render::SkillDamageMsgEffect(14) == -1);    // MG_COLDBOLT : rien
  CHECK(ro::render::SkillDamageMsgEffect(15) == 28);    // MG_FROSTDIVER
  CHECK(ro::render::SkillDamageMsgEffect(16) == 23);    // MG_STONECURSE
  CHECK(ro::render::SkillDamageMsgEffect(11) == 1);     // MG_NAPALMBEAT
  CHECK(ro::render::SkillDamageMsgEffect(17) == 1);     // MG_FIREBALL
  CHECK(ro::render::SkillDamageMsgEffect(89) == 51);    // WZ_STORMGUST, la fumée du froid
  CHECK(ro::render::SkillDamageMsgEffect(136) == 122);  // AS_SONICBLOW

  CHECK(ro::render::SkillDamageMsgEffect(7) == 17);  // SM_MAGNUM

  // ⚠ LA CARTE NOMME AUSSI L'IDENTIFIANT ZÉRO, et elle lui donne l'effet ZÉRO —
  // qui est EF_HIT1, pas « rien ». Le chemin des coups n'y arrive jamais : il
  // écarte les frappes sans compétence bien avant. Le dire ici garde la table
  // fidèle au client plutôt que « propre ».
  CHECK(ro::render::SkillDamageMsgEffect(0) == 0);

  // Une compétence ABSENTE ne pose rien : il n'y a PAS de règle par défaut ici,
  // à la différence de l'effet de frappe.
  CHECK(ro::render::SkillDamageMsgEffect(60000) == -1);
  CHECK(ro::render::SkillDamageMsgEffect(3004) == -1);  // KO_JYUMONJIKIRI, qui passe par le Lua
}

TEST_CASE("Effets de compétence : la carte du message est triée et sans doublon",
          "[render][skilleffect]") {
  // La recherche est dichotomique, et le client écrit sept compétences deux
  // fois : celle qui gagne est la DERNIÈRE écrite.
  const std::span<const ro::render::SkillHitEffectEntry> table =
      ro::render::SkillDamageMsgEffectTable();
  REQUIRE(table.size() == 205);
  std::uint16_t prior = 0;
  bool first_match = true;
  for (const ro::render::SkillHitEffectEntry& table_entry : table) {
    // ⚠ ZÉRO EST UN EFFET, pas une absence : c'est EF_HIT1. Seul −1 veut dire «
    // rien », et la table ne garde pas les −1.
    CHECK(table_entry.effect_id >= 0);
    if (!first_match) {
      CHECK(prior < table_entry.skill_id);
    }
    prior = table_entry.skill_id;
    first_match = false;
  }
}

TEST_CASE("Impact : un coup ORDINAIRE en pose un, lui aussi", "[render][skilleffect]") {
  // 🔴 L'IMPACT NE VAUT PAS QUE POUR UNE COMPÉTENCE : la gerbe qu'on voit à
  // chaque coup au corps à corps en est un. Le client la choisit dans le
  // traitement du coup reçu, et remplit les deux mêmes champs.
  constexpr std::uint8_t kPlainBlow = 0;
  constexpr std::uint8_t kCriticalBlow = 10;
  constexpr std::uint8_t kCriticalMultipleBlows = 13;
  constexpr std::int32_t kHit1 = 0;
  constexpr std::int32_t kHit2 = 1;
  constexpr std::uint16_t kNoviceJob = 0;
  constexpr std::uint16_t kDoramJob = 4302;
  constexpr std::uint16_t kLateJob = 22383;

  // L'effet de FRAPPE : rien, sauf sur les deux sortes critiques.
  CHECK(ro::render::PlainHitStrikeEffect(kPlainBlow) < 0);
  CHECK(ro::render::PlainHitStrikeEffect(kCriticalBlow) == kHit2);
  CHECK(ro::render::PlainHitStrikeEffect(kCriticalMultipleBlows) == kHit2);

  // Celui du MESSAGE : la gerbe ordinaire pour presque toute cible. ⚠ ZÉRO EST
  // UN EFFET, et c'est tout le piège de ce chemin : le prendre pour une absence
  // le fait disparaître.
  CHECK(ro::render::PlainHitMsgEffect(kNoviceJob, false) == kHit1);
  CHECK(ro::render::PlainHitMsgEffect(kDoramJob, false) == 434);
  CHECK(ro::render::PlainHitMsgEffect(kLateJob, false) == 2420);

  // Et rien du tout quand on se frappe soi-même : le client met −2, ce qui fait
  // taire le son avec l'image.
  CHECK(ro::render::PlainHitMsgEffect(kNoviceJob, true) < 0);
  CHECK(ro::render::PlainHitMsgEffect(kDoramJob, true) < 0);
}

TEST_CASE("Impact : les tables du coup ordinaire tiennent ce que le relevé dit",
          "[render][skilleffect]") {
  // Deux sortes de coup, seize classes, et trois valeurs par défaut : si la
  // table change, c'est ici qu'on s'en aperçoit.
  const std::span<const ro::render::PlainHitByDamageType> sortes =
      ro::render::PlainHitStrikeEffectTable();
  REQUIRE(sortes.size() == 2);
  for (const ro::render::PlainHitByDamageType& table_entry : sortes) {
    CHECK(table_entry.effect_id == 1);
  }

  const std::span<const ro::render::PlainHitByJob> classes = ro::render::PlainHitMsgEffectTable();
  REQUIRE(classes.size() == 16);
  std::uint16_t precedente = 0;
  bool earliest = true;
  for (const ro::render::PlainHitByJob& table_entry : classes) {
    CHECK((table_entry.effect_id == 434 || table_entry.effect_id == 2420));
    if (!earliest) {
      CHECK(precedente < table_entry.job);
    }
    precedente = table_entry.job;
    earliest = false;
  }

  const ro::render::PlainHitEffectDefaults defauts = ro::render::PlainHitDefaults();
  CHECK(defauts.strike == -1);
  CHECK(defauts.message == 0);
  CHECK(defauts.self_inflicted == -2);
}

TEST_CASE("Heaven's Drive : la grille de pointes ne naît pas au coup", "[render][skilleffect]") {
  // La carte des effets visés du client ne connaît pas Heaven's Drive : ni le
  // répartiteur des dégâts ni le semis procédural d'une compétence ne doivent
  // donc faire naître l'effet 142 quand un coup de la compétence arrive. Seul
  // le second impact, celui du message de dégâts, reste au coup.
  constexpr std::uint16_t kHeavensDriveSkill = 91;
  constexpr std::uint32_t kSpikeGridEffect = 142;
  for (const ro::render::SkillSuiteStep& step :
       ro::render::SkillSuiteOf(kHeavensDriveSkill, ro::render::SkillSuiteMoment::kImpact)) {
    CHECK(step.effect_id != kSpikeGridEffect);
  }
  const ro::render::SkillDamageEffectEntry* on_hit =
      ro::render::SkillDamageEffectOf(kHeavensDriveSkill);
  CHECK((on_hit == nullptr || on_hit->effect_id != kSpikeGridEffect));
  for (const ro::render::ProceduralSkillEffect& sown :
       ro::render::ProceduralEffectsOfSkill(kHeavensDriveSkill)) {
    CHECK(sown.effect_id != kSpikeGridEffect);
  }
}

TEST_CASE("Heaven's Drive : la grille de pointes naît sur la case visée", "[render][skilleffect]") {
  // Le visuel d'une compétence posée sur une case fait naître l'effet 142 à cet
  // endroit, sans variante sous les effets minimaux. Le chemin de la case le
  // demande à `SkillImplFor` quand la compétence n'a aucun effet nommé à ce
  // moment, et l'effet se sème par ses propres patrons de nœuds.
  constexpr std::uint16_t kHeavensDriveSkill = 91;
  constexpr std::uint16_t kSpikeGridEffect = 142;
  CHECK(ro::render::SkillNamedEffects(kHeavensDriveSkill, ro::render::kSkillNamedOnGround).empty());
  CHECK(SkillImplFor(kHeavensDriveSkill).EffectId() == kSpikeGridEffect);
  CHECK(ro::render::KnowsProceduralEffect(kSpikeGridEffect));
  CHECK_FALSE(ro::render::EffectNodePatternsFor(kSpikeGridEffect).empty());
}

TEST_CASE("Case visée : le lanceur de l'effet vit plus longtemps que ses nœuds",
          "[render][skilleffect]") {
  // Le visuel d'une compétence posée sur une case confie son effet à un
  // lanceur sans corps. Ce lanceur vit la durée de l'effet plus dix pas d'une
  // horloge à vingt-quatre millisecondes, et l'effet meurt avec lui. Nous
  // semons sans lanceur : cela ne vaut que tant qu'aucun nœud des effets que
  // ce chemin joue ne survit à ce lanceur.
  constexpr std::int32_t kLauncherStepMs = 24;
  constexpr std::int32_t kLauncherExtraSteps = 10;
  struct GroundCase {
    std::uint16_t skill_id;
    std::uint16_t effect_id;
  };
  constexpr std::array<GroundCase, 7> kGroundCases = {{
      {21, 30},
      {89, 89},
      {91, 142},
      {111, 98},
      {459, 98},
      {694, 91},
      {723, 89},
  }};
  for (const GroundCase& ground : kGroundCases) {
    CAPTURE(ground.skill_id, ground.effect_id);
    REQUIRE(SkillImplFor(ground.skill_id).EffectId() == ground.effect_id);
    const std::int32_t launcher_life_ms =
        (ro::render::EffectDurationTicks(ground.effect_id) + kLauncherExtraSteps) * kLauncherStepMs;
    for (const ro::render::EffectNodePattern& pattern :
         ro::render::EffectNodePatternsFor(ground.effect_id)) {
      const std::int32_t node_life_ms =
          std::max(pattern.life_ticks, pattern.life_ticks_high) * ro::render::kEffectTickMs;
      CHECK(node_life_ms < launcher_life_ms);
    }
  }
}

TEST_CASE("Effet du message : le niveau change le coup de combo de Combo Attack",
          "[render][skillhit]") {
  constexpr std::uint16_t kNpcComboAttack = 171;
  CHECK(ro::render::SkillDamageMsgEffect(kNpcComboAttack) == 186);
  CHECK(ro::render::SkillDamageMsgEffect(kNpcComboAttack, 1) == 186);
  CHECK(ro::render::SkillDamageMsgEffect(kNpcComboAttack, 2) == 187);
  CHECK(ro::render::SkillDamageMsgEffect(kNpcComboAttack, 5) == 190);
  // Au-delà de cinq, le premier coup revient.
  CHECK(ro::render::SkillDamageMsgEffect(kNpcComboAttack, 6) == 186);
  // Et les autres compétences n'y prennent pas garde.
  CHECK(ro::render::SkillDamageMsgEffect(5, 3) == ro::render::SkillDamageMsgEffect(5));
}
