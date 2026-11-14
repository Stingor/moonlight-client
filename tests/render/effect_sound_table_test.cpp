#include "ro/render/effect_sound_table.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_duration.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_sound_table.h"
#include "ro/render/skill_unit_table.h"

// LE BRUIT D'UN EFFET : ce que le client joue quand un effet paraît.
using ro::render::EffectSound;
using ro::render::EffectSoundEntry;
using ro::render::EffectSoundTable;
using ro::render::SkillImplFor;

TEST_CASE("Sons d'effet : la table est triée, un effet à plusieurs bruits les tient à la suite",
          "[render][effectsound]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable. Deux rangées du même identifiant se suivent, et c'est
  // `EffectSounds` qui les rend ensemble — le même bruit peut y revenir à un
  // AUTRE tic, Lex Divina le joue cinq fois. Au même tic, un seul effet le
  // fait : le lecteur de `.str` met trois fois le même bruit en file pour 1424,
  // et le client les joue tous les trois.
  const std::span<const EffectSoundEntry> table = EffectSoundTable();
  REQUIRE(table.size() > 100);
  std::vector<std::uint32_t> repetes;
  for (std::size_t index = 1; index < table.size(); ++index) {
    CHECK(table[index - 1].effect_id <= table[index].effect_id);
    if (table[index - 1].effect_id == table[index].effect_id) {
      CHECK(table[index - 1].delay_ticks <= table[index].delay_ticks);
      if (table[index - 1].sound == table[index].sound &&
          table[index - 1].delay_ticks == table[index].delay_ticks &&
          std::ranges::find(repetes, table[index].effect_id) == repetes.end()) {
        repetes.push_back(table[index].effect_id);
      }
    }
  }
  CHECK(repetes == std::vector<std::uint32_t>{1424});
  for (const EffectSoundEntry& entry : table) {
    CHECK(!entry.sound.empty());
    // UN « %d » VA AVEC SES VARIANTES, et seulement avec elles : un nom formé
    // sans nombre de fichiers ne se chargerait pas, un nombre sans marque ne
    // tirerait rien.
    CHECK((entry.sound.find("%d") != std::string_view::npos) == (entry.variants > 0));
  }
}

TEST_CASE("Sons d'effet : les pétales tirent leur bruit parmi trois, à chaque bruit",
          "[render][effectsound]") {
  // Le client écrit `rand() % 3 + 1` pour chacun des quatre bruits des tics 12
  // à 15 : les trois fichiers sortent, et deux bruits voisins ne sont pas
  // condamnés au même.
  constexpr std::uint32_t kPetals = 618;
  constexpr std::uint32_t kDraws = 64;
  const std::span<const EffectSoundEntry> petales = ro::render::EffectSounds(kPetals);
  REQUIRE(petales.size() == 4);
  std::set<std::string> seen;
  int voisins_differents = 0;
  for (std::uint32_t draw = 0; draw < kDraws; ++draw) {
    const std::string file = ro::render::EffectSoundFile(petales.front(), draw);
    seen.insert(file);
    voisins_differents += file != ro::render::EffectSoundFile(petales.front(), draw + 1) ? 1 : 0;
  }
  CHECK(seen == std::set<std::string>{"effect\\EF_FireArrow1.wav", "effect\\EF_FireArrow2.wav",
                                      "effect\\EF_FireArrow3.wav"});
  CHECK(voisins_differents > 0);
  CHECK(ro::render::EffectSoundVariantFile(petales.front(), 2) == "effect\\EF_FireArrow2.wav");
  // Un nom fixe se rend tel quel, quel que soit le tirage.
  const EffectSoundEntry tempete = ro::render::EffectSounds(89).front();
  CHECK(ro::render::EffectSoundFile(tempete, 7) == tempete.sound);
}

TEST_CASE("Sons d'effet : chaque nom parle de son effet", "[render][effectsound]") {
  // C'est ce qui juge le relevé : un identifiant tiré du mauvais cas aurait
  // donné un bruit sans rapport.
  CHECK(EffectSound(89) == "effect\\wizard_stormgust.wav");
  CHECK(EffectSound(92) == "effect\\wizard_meteor.wav");
  CHECK(EffectSound(96) == "effect\\wizard_fire_pillar_a.wav");
  CHECK(EffectSound(83) == "effect\\priest_sanctuary.wav");
  CHECK(EffectSound(87) == "effect\\priest_lexdivina.wav");
  CHECK(EffectSound(122) == "effect\\assasin_sonicblow.wav");
  CHECK(EffectSound(42) == "effect\\EF_Blessing.wav");
}

TEST_CASE("Sons d'effet : chaque bruit sonne À SON TIC, lu dans les gardes du bâtisseur",
          "[render][effectsound]") {
  // Lu par `scripts/tics_des_sons.py` : `âge == 37` pour l'onde de Bash,
  // `== 60` pour Impositio Manus, `== 12` pour la flèche de glace, dont le nom
  // est une variable que le relevé des sons connaît.
  const auto tick_of = [](std::uint32_t effect_id) {
    const std::span<const EffectSoundEntry> sounds = ro::render::EffectSounds(effect_id);
    REQUIRE(sounds.size() == 1);
    return sounds[0].delay_ticks;
  };
  CHECK(tick_of(82) == 37);
  CHECK(tick_of(84) == 60);
  CHECK(tick_of(26) == 12);
  CHECK(EffectSound(26) == "effect\\EF_IceArrow%d.wav");
  // Et un même bruit peut revenir à plusieurs tics — Lex Divina cinq fois, Fire
  // Ball à vingt, vingt-quatre, vingt-huit et trente-deux et JAMAIS à la
  // naissance : ces effets ne s'écartent pas en bloc, chaque tic compte.
  const std::span<const EffectSoundEntry> lex = ro::render::EffectSounds(87);
  REQUIRE(lex.size() == 5);
  CHECK(lex[0].delay_ticks == 0);
  CHECK(lex[1].delay_ticks == 20);
  CHECK(lex[4].delay_ticks == 70);
  const std::span<const EffectSoundEntry> ball = ro::render::EffectSounds(24);
  REQUIRE(ball.size() == 4);
  CHECK(ball[0].delay_ticks == 20);
  CHECK(ball[3].delay_ticks == 32);
  // ⚠ SOUL STRIKE SONNE À SIX, ET NON À ZÉRO : sa garde est `(âge + 5) % 11`,
  // et le relevé donne le DÉCALAGE — cinq — là où la rangée veut la PHASE. Les
  // confondre posait le premier coup cinq tics trop tôt. Son bruit reste joué
  // par le SEMIS, une boule un coup : voir `EffectSoundPlaysAtSowing`.
  CHECK(tick_of(15) == 6);
}

TEST_CASE("Sons d'effet : Napalm Vulcan ne sonne qu'aux âges où son cas appelle le bâtisseur",
          "[render][effectsound]") {
  // Le cas de 401 n'appelle le bâtisseur des reflets qu'aux âges 20 à 60, de
  // dix en dix, et ce bâtisseur ne joue son coup qu'à l'âge zéro : il ne sonne
  // jamais. Restent les cinq battements, un à chacun de ces âges.
  constexpr std::uint32_t kNapalmVulcan = 401;
  const std::span<const EffectSoundEntry> sounds = ro::render::EffectSounds(kNapalmVulcan);
  REQUIRE(sounds.size() == 5);
  std::uint32_t expected_tick = 20;
  for (const EffectSoundEntry& son : sounds) {
    CHECK(std::string(son.sound) == "effect\\EF_NapalmBeat.wav");
    CHECK(son.delay_ticks == expected_tick);
    expected_tick += 10;
  }
}

TEST_CASE("Sons d'effet : l'attaque empoisonnée en joue DEUX d'un coup", "[render][effectsound]") {
  // L'attaque empoisonnée joue deux sons de suite à son tic zéro, sans branche
  // entre les deux ; n'en jouer qu'un laisserait l'autre de côté.
  const std::span<const EffectSoundEntry> sounds = ro::render::EffectSounds(20);
  REQUIRE(sounds.size() == 2);
  CHECK(sounds[0].sound == "effect\\assasin_enchantpoison.wav");
  CHECK(sounds[1].sound == "effect\\EF_PoisonAttack.wav");
  // Le premier reste celui de `EffectSound`, pour qui n'en veut qu'un.
  CHECK(EffectSound(20) == "effect\\assasin_enchantpoison.wav");
  // Et un effet muet rend un vide, pas une rangée.
  CHECK(ro::render::EffectSounds(19).empty());
}

TEST_CASE("Sons d'effet : l'attaque empoisonnée ne sonne que pour son paramètre zéro",
          "[render][effectsound]") {
  // 20 et 125 la bâtissent avec zéro et sonnent ; 772 et 924 avec un, et se
  // taisent. Leur bruit est celui de 771, un tic plus tard.
  for (const std::uint32_t effect_id : {20U, 125U}) {
    INFO("effet " << effect_id);
    const std::span<const EffectSoundEntry> sounds = ro::render::EffectSounds(effect_id);
    REQUIRE(sounds.size() == 2);
    CHECK(sounds[0].delay_ticks == 0);
    CHECK(sounds[1].delay_ticks == 0);
  }
  CHECK(ro::render::EffectSounds(772).empty());
  CHECK(ro::render::EffectSounds(924).empty());
  const std::span<const EffectSoundEntry> compagnon = ro::render::EffectSounds(771);
  REQUIRE(compagnon.size() == 2);
  CHECK(compagnon[0].sound == "effect\\assasin_enchantpoison.wav");
  CHECK(compagnon[1].sound == "effect\\EF_PoisonAttack.wav");
  CHECK(compagnon[0].delay_ticks == 1);
  CHECK(compagnon[1].delay_ticks == 1);
}

TEST_CASE("Sons d'effet : un bâtisseur partagé ne joue que le bruit que son site choisit",
          "[render][effectsound]") {
  const auto sound_only = [](std::uint32_t effect_id) {
    const std::span<const EffectSoundEntry> sounds = ro::render::EffectSounds(effect_id);
    REQUIRE(sounds.size() == 1);
    CHECK(sounds[0].delay_ticks == 0);
    return sounds[0].sound;
  };

  // L'anneau au sol des compétences : un bruit par argument, et aucun pour
  // trois, six, sept et huit.
  CHECK(sound_only(119) == "effect\\hunter_detecting.wav");
  CHECK(sound_only(725) == "effect\\priest_benedictio.wav");
  CHECK(sound_only(780) == "effect\\cru_grand cross.wav");
  CHECK(sound_only(786) == "effect\\ANALYZE.wav");
  for (const std::uint32_t effect_id : {722U, 778U, 812U}) {
    INFO("effet " << effect_id);
    CHECK(ro::render::EffectSounds(effect_id).empty());
  }
  // 724 garde le bruit que son cas joue lui-même, un tic plus tard.
  const std::span<const EffectSoundEntry> sienna = ro::render::EffectSounds(724);
  REQUIRE(sienna.size() == 1);
  CHECK(sienna[0].sound == "effect\\wl_siennaexecrate.wav");
  CHECK(sienna[0].delay_ticks == 1);

  // Le soin : zéro souffle, un aspire, deux et trois se taisent.
  CHECK(sound_only(331) == "_heal_effect.wav");
  CHECK(sound_only(332) == "effect\\흡기.wav");
  CHECK(ro::render::EffectSounds(785).empty());
  CHECK(ro::render::EffectSounds(793).empty());

  // L'agilité monte pour un, descend pour zéro.
  CHECK(sound_only(2288) == "effect\\EF_IncAgility.wav");
  CHECK(sound_only(2289) == "effect\\EF_DecAgility.wav");

  // Les bruits du seul paramètre zéro.
  CHECK(sound_only(347) == "effect\\wedding.wav");
  CHECK(ro::render::EffectSounds(1038).empty());
  CHECK(sound_only(267) == "effect\\mon_침투경.wav");
  CHECK(ro::render::EffectSounds(606).empty());
  CHECK(ro::render::EffectSounds(607).empty());
  CHECK(sound_only(313) == "_heal_effect.wav");
  CHECK(ro::render::EffectSounds(2427).empty());

  // Deux noms que le bâtisseur choisit au lieu de celui qu'il joue d'ordinaire :
  // l'âme pour trois, la flèche de glace — formée au tirage — pour deux.
  CHECK(sound_only(777) == "effect\\EF_IncAgility.wav");
  CHECK(sound_only(779) == "effect\\소울 체인지.wav");
  CHECK(sound_only(79) == "effect\\wizard_earthspike.wav");
  CHECK(sound_only(619) == "effect\\EF_IceArrow%d.wav");
}

TEST_CASE("Sons d'effet : le BÂTISSEUR en joue autant que le répartiteur",
          "[render][effectsound]") {
  // ⚠ DEUX FONCTIONS DU CLIENT FONT LE MÊME BRUIT, et l'on n'en lisait qu'une.
  // Le répartiteur de mise à jour en joue cent trente ; les autres sont joués
  // par la fonction qui BÂTIT l'effet, et le relevé des gestionnaires les
  // portait sans que personne ne les lise.
  //
  // LE SOIN DU PRÊTRE EN EST : ses quatre effets sonnent le même fichier, et
  // tous les quatre étaient muets alors que leur bruit était mesuré.
  CHECK(EffectSound(312) == "_heal_effect.wav");
  CHECK(EffectSound(313) == "_heal_effect.wav");
  CHECK(EffectSound(320) == "_heal_effect.wav");
  CHECK(EffectSound(325) == "_heal_effect.wav");

  // Et la table a plus que doublé : cent trente bruits, deux cent soixante-neuf.
  // Le compte est un PLANCHER, pas une égalité — il monte quand le relevé lit
  // mieux, il ne doit pas retomber.
  CHECK(EffectSoundTable().size() >= 269);
}

TEST_CASE("Sons d'effet : un effet muet ne rend rien", "[render][effectsound]") {
  CHECK(EffectSound(0).empty());
  CHECK(EffectSound(6).empty());
  CHECK(EffectSound(100000).empty());
}

TEST_CASE("Sons d'effet : les compétences classiques trouvent enfin le leur",
          "[render][effectsound]") {
  // Le bruit d'une compétence est celui de son EFFET, et c'est pourquoi une
  // table rangée par compétence restait muette sur elles : leur bruit n'y a
  // jamais été.
  CHECK(EffectSound(SkillImplFor(89).EffectId()) == "effect\\wizard_stormgust.wav");
  CHECK(EffectSound(SkillImplFor(80).EffectId()) == "effect\\wizard_fire_pillar_a.wav");
  CHECK(EffectSound(SkillImplFor(21).EffectId()) == "effect\\magician_thunderstorm.wav");
  // Et une compétence dont l'effet est muet reste muette : toutes n'ont pas
  // de bruit, et en inventer un serait pire que le silence.
  CHECK(EffectSound(SkillImplFor(56).EffectId()).empty());
}

TEST_CASE("Sons d'effet : la casse ne fait pas deux fichiers", "[render][effectsound]") {
  // 🔴 LE VFS NORMALISE TOUT CHEMIN EN MINUSCULES (`NormalizeGrfPath`). Deux
  // graphies d'un même `.wav` s'ouvrent donc TOUTES LES DEUX, se chargent sous
  // deux clés, et `HearEffects` — qui joue chaque rangée d'un effet — les lance
  // ensemble. Le joueur entend un son deux fois plus fort, et rien ne manque :
  // c'est un défaut qui ne se voit ni dans un journal ni à l'écran.
  //
  // Vingt-trois rangées étaient dans ce cas, le relevé des gestionnaires ayant
  // perdu la casse du client que les deux autres gardent. Safety Wall en était.
  //
  // ⚠ CE N'EST PAS « UN SON PAR EFFET » : Lex Divina joue le sien à cinq tics,
  // et c'est légitime. Ce qui ne l'est pas, c'est DEUX GRAPHIES du même fichier
  // sur un même effet — quelle que soit leur paire de tics.
  std::map<std::pair<std::uint16_t, std::string>, std::string> graphie;
  for (const EffectSoundEntry& table_entry : EffectSoundTable()) {
    std::string minuscule(table_entry.sound);
    for (char& lettre : minuscule) {
      lettre = static_cast<char>(std::tolower(static_cast<unsigned char>(lettre)));
    }
    const auto [known, neuve] =
        graphie.try_emplace({table_entry.effect_id, minuscule}, std::string(table_entry.sound));
    INFO("effet " << table_entry.effect_id << " : « " << known->second << " » puis « "
                  << table_entry.sound << " »");
    CHECK((neuve || known->second == table_entry.sound));
  }

  // Le mur porte la graphie du CLIENT, celle que le relevé des sons et celui
  // des émetteurs ont gardée — et une seule fois.
  CHECK(ro::render::EffectSounds(315).size() == 1);
  CHECK(EffectSound(315) == "effect\\EF_GlassWall.wav");
}

TEST_CASE("Sons d'effet : un bruit PÉRIODIQUE dit sa période, et elle est tenable",
          "[render][effectsound]") {
  // 🔴 UN BRUIT PEUT ÊTRE UN BOURDON. Le portail prêt joue son `.wav` sous
  // `!(âge % 14)` — aucune autre garde — donc
  // le portail carillonne tant qu'il tient sa case. Le relevé lisait la garde
  // depuis toujours et le générateur la JETAIT, au motif que « c'est le semis
  // qui le joue, ou rien » : vrai pour Soul Strike, faux pour le portail.
  //
  // ⚠ CE QUI EST TENU ICI EST LA CONDITION DU BOURDON, non le bourdon :
  // `ActorSounds::Hum` vit dans l'exécutable, hors de portée des essais. Il lui
  // faut, pour savoir QUAND SE TAIRE, la durée de l'effet — et une durée
  // inconnue rend zéro, ce qui l'éteindrait à l'instant de sa naissance.
  std::size_t bourdons = 0;
  for (const EffectSoundEntry& table_entry : EffectSoundTable()) {
    if (table_entry.period_ticks == 0) {
      continue;
    }
    ++bourdons;
    INFO("effet " << table_entry.effect_id << " : " << table_entry.sound);
    // Une période nulle serait « à chaque image » ; une phase au-delà de la
    // période serait un premier coup qui n'arrive jamais.
    CHECK(table_entry.delay_ticks < table_entry.period_ticks);
    REQUIRE(ro::render::EffectDurationIsKnown(table_entry.effect_id));
    CHECK(ro::render::EffectDurationTicks(table_entry.effect_id) > table_entry.period_ticks);
  }
  CHECK(bourdons == 3);

  // Le portail prêt : quatorze tics, soit deux cent vingt-quatre millisecondes.
  const std::span<const EffectSoundEntry> portal = ro::render::EffectSounds(316);
  REQUIRE(portal.size() == 1);
  CHECK(portal[0].sound == "effect\\EF_ReadyPortal.wav");
  CHECK(portal[0].delay_ticks == 0);
  CHECK(portal[0].period_ticks == 14);

  // 🔴 ET C'EST BIEN PLUS RAPIDE QUE LA RELANCE DE SON UNITÉ, sans quoi le
  // correctif n'apporterait rien : l'unité 129 — un portail tel qu'on vient de
  // le lancer, `UNT_WARP_ACTIVE` chez le serveur — resème son effet tous les
  // `durée − retrait` tics, et c'est à CE rythme-là que nous sonnions. Treize
  // fois trop lentement.
  const ro::render::SkillUnitVisualEntry* unit = ro::render::SkillUnitVisual(129);
  REQUIRE(unit != nullptr);
  CHECK(unit->effect_id == 316);
  const std::int32_t relance = ro::render::EffectDurationTicks(316) - unit->withhold_ticks;
  CHECK(relance == 192);
  CHECK(portal[0].period_ticks < relance);
}

TEST_CASE("un bruit posé sur l'auditeur ne se place pas sur l'effet", "[render][sound]") {
  // La boule de 891 passe trois zéros au client : elle sonne sur l'auditeur.
  // Celle d'EF_FIREBALL (24) passe l'écart à l'instance.
  const std::span<const EffectSoundEntry> ball = ro::render::EffectSounds(891);
  REQUIRE(ball.size() == 1);
  CHECK(ball.front().delay_ticks == 5);
  CHECK(ball.front().at_listener);
  for (const EffectSoundEntry& table_entry : ro::render::EffectSounds(24)) {
    CHECK_FALSE(table_entry.at_listener);
  }
}

TEST_CASE("un objet lancé sonne à la naissance de sa classe", "[render][sound]") {
  // Les quatre lancers du ninja — shuriken, kunai, Huuma, pièces — sonnent le
  // même bruit, et Root Twist le sien.
  for (const std::uint32_t throw_effect : {613U, 614U, 615U, 616U}) {
    INFO("effet " << throw_effect);
    CHECK(EffectSound(throw_effect) == "effect\\닌자_던지기.wav");
  }
  CHECK(EffectSound(1110) == "effect\\su_roottwist.wav");
  // NJ_SYURIKEN ne joue pas le son de Blessing : 523 est l'eau bénite dans le
  // répartiteur des OBJETS employés, pas une compétence.
  CHECK(ro::render::SkillSound(523).empty());
}

TEST_CASE("le choix du joueur passe devant le relevé", "[render][sound]") {
  // Le flacon d'Acid Terror sonne comme un soin chez le natif ; le joueur a
  // choisi une flèche de Fire Bolt.
  constexpr std::uint32_t kAcidTerrorFlask = 298;
  const std::span<const EffectSoundEntry> flask = ro::render::EffectSounds(kAcidTerrorFlask);
  REQUIRE(flask.size() == 1);
  CHECK(flask.front().sound == "effect\\EF_FireArrow%d.wav");
}
