// LA PÉRIODE DE RELANCE D'UN VISUEL D'UNITÉ AU SOL.
//
// 🔴 ELLE EST CELLE DE L'EFFET, QUOI QUE L'EFFET CONTIENNE. Le tic d'une unité
// fait, pour les quatre-vingt-six unités, le même geste :
//
//     if (compteur >= durée(effet) − retrait) { compteur = 0; poser l'effet; }
//
// Il ne regarde jamais ce qu'il y a dedans. Nous avons pris DEUX mauvaises
// mesures à sa place, l'une après l'autre : la vie de la TROUPE — qui valait
// zéro pour une famille non couverte, d'où un semis à chaque image —, puis la
// longueur du FICHIER, qui faisait repartir Safety Wall toutes les neuf cent
// soixante millisecondes, indéfiniment.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_duration.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/effect_table.h"
#include "ro/render/skill_unit_sound_table.h"
#include "ro/render/skill_unit_table.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"

TEST_CASE("Visuel d'unité : chaque unité a une durée RELEVÉE", "[app][effets]") {
  // ⚠ LE CONTRÔLE PORTE SUR TOUTE LA TABLE, et non sur le seul mur qui a révélé
  // le défaut : une durée absente rend ZÉRO, et un zéro n'est pas une période
  // courte. C'est le genre de trou qu'un cas nommé ne garde pas.
  for (const ro::render::SkillUnitVisualEntry& visuel : ro::render::SkillUnitVisualTable()) {
    INFO("unité " << visuel.unit_id << ", effet " << visuel.effect_id);
    REQUIRE(ro::render::EffectDurationIsKnown(visuel.effect_id));
    // Un tic de période veut dire « resemé à chaque image ». Ce qui est traqué
    // ici est le zéro, pas une valeur précise.
    CHECK(ro::render::EffectDurationTicks(visuel.effect_id) > visuel.withhold_ticks + 1);
  }
}

TEST_CASE("Visuel d'unité : Safety Wall est l'unité 126, et son mur tient", "[app][effets]") {
  // 🔴 CENT VINGT-SIX, PAS CENT VINGT-HUIT. Le serveur numérote
  // `UNT_SAFETYWALL = 0x7E`, puis FIREWALL, puis WARP_WAITING — c'est cette
  // troisième-là qui porte l'effet 317. Deux jours de mesures ont été attribuées
  // au portail de téléportation avant que le joueur ne le dise.
  //
  // Les deux tables du dépôt le disaient déjà, chacune à sa façon : l'effet 315
  // joue `SafetyWall.str` et sonne `EF_GlassWall.wav`.
  const ro::render::SkillUnitVisualEntry* wall = ro::render::SkillUnitVisual(126);
  REQUIRE(wall != nullptr);
  CHECK(wall->effect_id == 315);
  CHECK(wall->shape == ro::render::SkillUnitVisualShape::kRepeated);
  CHECK(wall->withhold_ticks == 7);

  // 🔴 NEUF MILLE NEUF CENT QUATRE-VINGT-DIX-NEUF, dans la table que le
  // client garnit au démarrage. La relance tombe donc à 9 992 tics, soit deux
  // minutes et demie : aucun mur ne vit assez longtemps pour la voir, et l'effet
  // est posé UNE FOIS.
  CHECK(ro::render::EffectDurationTicks(315) == 9999);
  CHECK(ro::render::SkillUnitRespawnMs(wall->effect_id, wall->withhold_ticks) == 9992U * 16U);

  // ⚠ ET SON VOISIN N'EST PAS LUI : l'unité 128 est le portail en attente.
  const ro::render::SkillUnitVisualEntry* portal = ro::render::SkillUnitVisual(128);
  REQUIRE(portal != nullptr);
  CHECK(portal->effect_id == 317);
}

TEST_CASE("Visuel d'unité : le fichier ne donne PAS la période", "[app][effets]") {
  // 🔴 `SafetyWall.str` FAIT SOIXANTE ET UNE IMAGES, et nous rejouions le mur à
  // cette longueur-là : cinquante-quatre tics, soit huit cent soixante-quatre
  // millisecondes, en boucle tant que l'unité tenait sa case. Le joueur l'a vu
  // et l'a nommé « ça respawn en boucle ».
  //
  // Le contrôle ne lit pas le fichier — les essais n'ont pas d'archive — mais il
  // tient l'écart d'ordre de grandeur : la période d'un mur se compte en
  // MINUTES, et aucune animation de `.str` ne dure cela.
  const ro::render::SkillUnitVisualEntry* wall = ro::render::SkillUnitVisual(126);
  REQUIRE(wall != nullptr);
  REQUIRE(!ro::render::EffectFile(wall->effect_id).empty());
  CHECK(ro::render::EffectDurationTicks(wall->effect_id) > 1000);

  // ⚠ ET LE MUR DE FEU, LUI, SE RELANCE VRAIMENT : quarante tics de durée, sept
  // de retrait, donc un demi-tic de seconde. Les traiter pareil aurait figé le
  // feu sur sa dernière image.
  const ro::render::SkillUnitVisualEntry* feu = ro::render::SkillUnitVisual(127);
  REQUIRE(feu != nullptr);
  CHECK(feu->effect_id == 25);
  CHECK(ro::render::EffectDurationTicks(25) == 40);
  CHECK(ro::render::SkillUnitRespawnMs(feu->effect_id, feu->withhold_ticks) == 33U * 16U);

  // ⚠ ET UNE DURÉE ABSENTE NE RELANCE PAS DU TOUT. Zéro dit « une fois » ; un
  // plancher à un tic dirait « soixante fois par seconde ».
  CHECK_FALSE(ro::render::EffectDurationIsKnown(999999U));
  CHECK(ro::render::SkillUnitRespawnMs(999999U, 0) == 0U);
}

TEST_CASE("Visuel d'unité : le son de l'UNITÉ et celui de l'EFFET ne se croisent jamais",
          "[app][effets]") {
  // 🔴 DEUX CHEMINS, DEUX TABLES, ET C'EST CE QUI REND SÛR DE JOUER LES DEUX.
  // Le client sonne d'un côté au tic de l'unité et de l'autre au tic zéro de
  // l'effet, que son bâtisseur fait sonner lui-même. Nous ne jouions que le premier, et ONZE unités
  // restaient muettes — dont Safety Wall, que le joueur a entendu manquer.
  //
  // ⚠ CE CONTRÔLE EST LA CONDITION DE LA CORRECTION, pas son illustration : si
  // une unité venait à porter les DEUX sons, `SowUnitVisual` la ferait sonner
  // deux fois. Tant que les tables restent disjointes, rien ne peut doubler.
  std::size_t by_effect = 0;
  for (const ro::render::SkillUnitVisualEntry& visuel : ro::render::SkillUnitVisualTable()) {
    const bool unit_sound = ro::render::SkillUnitPlacementSound(visuel.unit_id) != nullptr ||
                            ro::render::SkillUnitSustainSound(visuel.unit_id) != nullptr;
    const bool effect_sound = !ro::render::EffectSound(visuel.effect_id).empty();
    INFO("unité " << visuel.unit_id << ", effet " << visuel.effect_id);
    // ⚠ LES PARENTHÈSES SONT EXIGÉES : Catch2 refuse `&&` nu dans une assertion,
    // et le refus est une erreur de COMPILATION — que MSBuild rend en laissant
    // l'ancien binaire en place. Le compte de cas l'a dit, pas le build.
    CHECK_FALSE((unit_sound && effect_sound));
    if (effect_sound) {
      ++by_effect;
    }
  }
  // ⚠ ET IL Y EN A, sans quoi ce contrôle passerait sur une table vide. Le
  // nuage de poison (unité 225) n'en est pas : son effet, 772, est l'attaque
  // empoisonnée de paramètre un, qui ne sonne pas.
  CHECK(by_effect >= 10);

  // Le mur est de ceux-là : son son vit sur l'effet 315, pas sur l'unité 126.
  CHECK(ro::render::SkillUnitPlacementSound(126) == nullptr);
  CHECK(ro::render::SkillUnitSustainSound(126) == nullptr);
  CHECK(ro::render::EffectSound(315) == "effect\\EF_GlassWall.wav");
  // ⚠ ET LE MUR DE FEU EST DE L'AUTRE ESPÈCE : son bruit vient de l'unité — de
  // sa table de POSE, et non de celle d'entretien. Les deux existent, et les
  // confondre fait conclure qu'une unité est muette quand elle ne l'est pas.
  CHECK(ro::render::SkillUnitPlacementSound(127) != nullptr);
  CHECK(ro::render::EffectSound(25).empty());
}

TEST_CASE("Visuel d'unité : le tatami de Tatamigaeshi ne se resème pas", "[app][effets]") {
  // L'unité 188 pose l'effet 631, que la fabrique de sprite bâtit une seule
  // fois. C'est le seul visuel permanent qu'elle bâtit : la liste est fermée.
  constexpr std::uint16_t kTatamiUnit = 188;
  std::uint32_t sown_once = 0;
  for (const ro::render::SkillUnitVisualEntry& visuel : ro::render::SkillUnitVisualTable()) {
    if (ro::render::SkillUnitVisualSownOnce(visuel)) {
      ++sown_once;
      CHECK(visuel.unit_id == kTatamiUnit);
    }
  }
  CHECK(sown_once == 1);
  // Un visuel qui se relance par nature ne l'est jamais.
  constexpr std::uint16_t kSafetyWallUnit = 126;
  const ro::render::SkillUnitVisualEntry* wall = ro::render::SkillUnitVisual(kSafetyWallUnit);
  REQUIRE(wall != nullptr);
  CHECK_FALSE(ro::render::SkillUnitVisualSownOnce(*wall));
}

TEST_CASE("Visuel d'unité : Quagmire a son remplaçant sous /mineffect, et lui seul",
          "[app][effets]") {
  constexpr std::uint16_t kQuagmireUnit = 142;
  constexpr std::uint16_t kQuagmireMinimal = 1161;
  const ro::render::SkillUnitVisualEntry* quagmire = ro::render::SkillUnitVisual(kQuagmireUnit);
  REQUIRE(quagmire != nullptr);
  CHECK(quagmire->minimal_effect_id == kQuagmireMinimal);
  for (const ro::render::SkillUnitVisualEntry& visuel : ro::render::SkillUnitVisualTable()) {
    if (visuel.unit_id != kQuagmireUnit) {
      INFO("unité " << visuel.unit_id);
      CHECK(visuel.minimal_effect_id == 0);
    }
  }
}
