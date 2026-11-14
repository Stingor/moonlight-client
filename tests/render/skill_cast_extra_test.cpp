#include "ro/render/skill_cast_extra.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::render::CastOutlivesInstantExit;
using ro::render::kLockOnReticleEffect;
using ro::render::kNoLockOnSkill;
using ro::render::ShowsLockOnReticle;
using ro::render::SkillCastExtrasFor;
using ro::render::SkillEffectOn;

namespace {

// Deux acteurs quelconques : seul leur RAPPORT compte pour la règle.
constexpr std::uint32_t kCasterId = 2000001;
constexpr std::uint32_t kTarget = 110000123;
// Une incantation ordinaire : Cold Bolt rang 1, huit cents millisecondes.
constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint32_t kDurationMs = 800;

}  // namespace

TEST_CASE("Incantation : le réticule se pose sur ce qu'on vise", "[render][skillcast]") {
  // 🔴 IL N'APPARTIENT À AUCUNE COMPÉTENCE. Le client l'envoie à la cible avant
  // d'entrer dans son switch : c'est le cas ORDINAIRE, et les compétences
  // nommées ne font que s'y ajouter. Le chercher dans la table des extras — où
  // il n'est pas — ferait conclure qu'il n'existe pas.
  CHECK(ShowsLockOnReticle(kColdBolt, kCasterId, kTarget, kDurationMs));
  CHECK(kLockOnReticleEffect == 60);
  // Et la table des extras ne le porte pas, pour cette raison même.
  CHECK(SkillCastExtrasFor(kColdBolt).empty());
}

TEST_CASE("Incantation : les quatre cas SANS réticule", "[render][skillcast]") {
  // Une durée nulle n'atteint jamais la fonction : la sortie du client la
  // précède. Le serveur annonce pourtant l'incantation d'un sort instantané.
  CHECK_FALSE(ShowsLockOnReticle(kColdBolt, kCasterId, kTarget, 0));

  // ⚠ SANS CIBLE, L'AUTRE BRANCHE POSE UN REPÈRE AU SOL, pas un réticule. Le
  // poser quand même le ferait paraître sur le lanceur, faute de cible.
  CHECK_FALSE(ShowsLockOnReticle(kColdBolt, kCasterId, 0, kDurationMs));

  // Sur soi-même, la fonction sort avant tout geste.
  CHECK_FALSE(ShowsLockOnReticle(kColdBolt, kCasterId, kCasterId, kDurationMs));

  // ⚠ ET UNE SEULE COMPÉTENCE Y ÉCHAPPE, par un saut qui passe PAR-DESSUS le
  // message. Les autres cas nommés, eux, le laissent s'exécuter avant le leur :
  // RA_AIMEDBOLT pose son 1651 EN PLUS du réticule, pas à sa place.
  CHECK_FALSE(ShowsLockOnReticle(kNoLockOnSkill, kCasterId, kTarget, kDurationMs));
  CHECK(ShowsLockOnReticle(2236, kCasterId, kTarget, kDurationMs));
}

TEST_CASE("Incantation : les extras nommés restent ce qu'ils sont", "[render][skillcast]") {
  // WL_SOULEXPANSION en pose DEUX : c'est ce qui oblige la table à être une
  // liste de paires plutôt qu'une entrée par compétence.
  CHECK(SkillCastExtrasFor(2202).size() == 2);
  // NC_FLAMELAUNCHER est le seul dont l'effet naisse sur le LANCEUR.
  const auto mecanicien = SkillCastExtrasFor(2259);
  REQUIRE(mecanicien.size() == 1);
  CHECK(mecanicien.front().on == SkillEffectOn::kCaster);
  CHECK(mecanicien.front().effect_id == 787);
}

TEST_CASE("Incantation : Asura Strike passe la sortie sur durée nulle", "[render][skillcast]") {
  constexpr std::uint16_t kAsuraStrike = 271;
  constexpr std::uint16_t kComboFinish = 273;
  // En combo, sa durée nulle montre quand même ses glyphes.
  CHECK(CastOutlivesInstantExit(kAsuraStrike));
  CHECK_FALSE(CastOutlivesInstantExit(kComboFinish));
  CHECK_FALSE(CastOutlivesInstantExit(kColdBolt));
  // Mais le réticule qu'il pose alors a une vie nulle : rien ne se voit.
  CHECK_FALSE(ShowsLockOnReticle(kAsuraStrike, kCasterId, kTarget, 0));
}
