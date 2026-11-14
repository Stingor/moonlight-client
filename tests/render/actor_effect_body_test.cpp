#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_scale.h"

// CE QUE LE CORPS FAIT DES EFFETS EN COURS : sa pose, ses copies, ses voiles.
//
// Coupé de `actor_effect_table_test.cpp` au plafond de huit cents lignes, et la
// coupure suit une frontière réelle : là-bas on éprouve ce que la TABLE dit —
// son ordre, ses noms, ses tics, ses rampes —, ici ce qu'on en COMPOSE. Aucun
// cas de ce fichier ne lirait la table sans appeler une fonction de composition,
// et aucun cas de l'autre n'en appelle une.
using ro::render::ActorBodyPose;
using ro::render::ActorEffectChange;
using ro::render::ActorEffectChangeFor;
using ro::render::ActorEffectChangeTable;
using ro::render::ActorEffectFamily;
using ro::render::ActorEffectPlay;
using ro::render::BodyCopiesAfterEffects;
using ro::render::BodyPoseAfterEffects;
using ro::render::BodyScaleAfterEffects;
using ro::render::BodyVeilsAfterEffects;
using ro::render::kTintUnchanged;
using ro::render::ValueAt;

TEST_CASE("Effets d'acteur : le saut monte en s'effaçant, la réception fait l'inverse",
          "[render][actoreffect]") {
  // EF_JUMPBODY et EF_LANDBODY sont une PAIRE : le premier emporte le corps,
  // le second le ramène.
  const std::array<ActorEffectPlay, 1> au_sol = {ActorEffectPlay{445, 0}};
  ActorBodyPose pose = BodyPoseAfterEffects(au_sol);
  // Avant le trente-sixième tic le client sort du cas sans rien écrire : le
  // corps n'a pas encore quitté le sol, et il est entier. Sa rampe, lue si
  // tôt, serait négative et l'enfoncerait dans le sol.
  CHECK(pose.alpha == 255);
  CHECK(pose.up_pixels == 0.0F);

  const std::array<ActorEffectPlay, 1> en_l_air = {ActorEffectPlay{445, 45}};
  pose = BodyPoseAfterEffects(en_l_air);
  CHECK(pose.up_pixels > 0.0F);
  CHECK(pose.alpha < 255);

  // À la fin de sa vie, le corps est parti.
  const std::array<ActorEffectPlay, 1> parti = {ActorEffectPlay{445, 200}};
  const ActorBodyPose fini = BodyPoseAfterEffects(parti);
  CHECK(fini.alpha == 0);
  // ⚠ LE SAUT N'A PAS DE PLAFOND : sa seule comparaison est un PLANCHER, le
  // tic où la rampe commence. Ce qui l'arrête est la mort de son instance, au
  // bout de sa durée : le mot de rendu se refait sans son bit, et le corps
  // reparaît à sa place.
  const std::array<ActorEffectPlay, 1> last_tick = {ActorEffectPlay{445, 499}};
  CHECK(BodyPoseAfterEffects(last_tick).alpha == 0);
  const std::array<ActorEffectPlay, 1> instance_morte = {ActorEffectPlay{445, 500}};
  CHECK(BodyPoseAfterEffects(instance_morte).alpha == 255);
  CHECK(BodyPoseAfterEffects(instance_morte).up_pixels == 0.0F);

  // La réception : haut et transparent au début, au sol et entier à la fin.
  const std::array<ActorEffectPlay, 1> arrive = {ActorEffectPlay{446, 10}};
  const ActorBodyPose begin = BodyPoseAfterEffects(arrive);
  const std::array<ActorEffectPlay, 1> pose_au_sol = {ActorEffectPlay{446, 25}};
  const ActorBodyPose fin = BodyPoseAfterEffects(pose_au_sol);
  CHECK(begin.up_pixels > fin.up_pixels);
  CHECK(begin.alpha < fin.alpha);
  CHECK(fin.up_pixels == 0.0F);
  CHECK(fin.alpha == 255);
}

TEST_CASE("Effets d'acteur : le trou de MANHOLE emporte le corps hors de l'écran",
          "[render][actoreffect]") {
  // Le client l'envoie très loin sous le sol ; le résultat visible est qu'on ne
  // le voit plus, et un drapeau le dit mieux qu'un décalage de vingt-quatre
  // mille.
  const std::array<ActorEffectPlay, 1> trou = {ActorEffectPlay{823, 5}};  // EF_MANHOLE
  CHECK(BodyPoseAfterEffects(trou).hidden);

  // Un effet qui ne fait rien de tel laisse le corps où il est.
  const std::array<ActorEffectPlay, 1> vert = {ActorEffectPlay{538, 5}};
  const ActorBodyPose pose = BodyPoseAfterEffects(vert);
  CHECK_FALSE(pose.hidden);
  CHECK(pose.up_pixels == 0.0F);
  CHECK(pose.alpha == 255);
}

TEST_CASE("Effets d'acteur : la lueur pose deux copies qui respirent", "[render][actoreffect]") {
  // EF_ASSUMPTIO lève le bit 0x8, et le client en tire DEUX quads de plus,
  // élargis d'un nombre de pixels qui bat au rythme d'un sinus. Leurs deux
  // alphas — cent et cent vingt — sont relevés dans les couleurs 0x64FFFFFF et
  // 0x78FFFFFF.
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> copies;
  const std::array<ActorEffectPlay, 1> lueur = {ActorEffectPlay{375, 0}};  // EF_ASSUMPTIO
  REQUIRE(BodyCopiesAfterEffects(lueur, copies) == 2);
  CHECK(copies[0].tint == std::array<std::uint8_t, 4>{255, 255, 255, 100});
  CHECK(copies[1].tint == std::array<std::uint8_t, 4>{255, 255, 255, 120});
  // La seconde est plus large que la première, toujours.
  CHECK(copies[1].grow_pixels > copies[0].grow_pixels);
  // Et toutes deux plus grandes que le corps : c'est un halo, pas une doublure.
  CHECK(copies[0].grow_pixels > 0.0F);
  // ⚠ CHAQUE CALQUE S'ÉLARGIT AUTOUR DE SON CENTRE, et l'échelle ne bouge pas :
  // grandi depuis les pieds, le halo ne monterait que vers le haut.
  CHECK(copies[0].scale == 1.0F);
  // ⚠ ELLE RESTE DERRIÈRE LE CORPS : le client recule sa profondeur. Le contour
  // des liens du Soul Linker (424, bit 0x800), lui, passe devant et teint le
  // corps entier.
  CHECK_FALSE(copies[0].in_front);
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> outline;
  const std::array<ActorEffectPlay, 1> link = {ActorEffectPlay{424, 10}};
  REQUIRE(BodyCopiesAfterEffects(link, outline) == 2);
  CHECK(outline[0].in_front);
  CHECK(outline[1].in_front);

  // ⚠ ELLE RESPIRE : la même copie n'a pas la même taille à deux instants.
  const std::array<ActorEffectPlay, 1> later = {ActorEffectPlay{375, 90}};
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> after;
  REQUIRE(BodyCopiesAfterEffects(later, after) == 2);
  CHECK(after[0].grow_pixels != copies[0].grow_pixels);
}

TEST_CASE("Effets d'acteur : le contour mort-vivant s'efface par son alpha",
          "[render][actoreffect]") {
  // EF_UNDEADBODY et son « _DEL » posent le même bit ; ce qui les sépare est
  // l'alpha du contour, que l'un monte et que l'autre ramène vers zéro avant
  // que son bit ne tombe avec sa vie.
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> copies;
  const std::array<ActorEffectPlay, 1> apparait = {ActorEffectPlay{655, 100}};
  REQUIRE(BodyCopiesAfterEffects(apparait, copies) == 2);
  CHECK(copies[0].tint[3] == 100);
  // Vert, et les deux copies l'une un peu plus grande que l'autre.
  CHECK(copies[0].tint == std::array<std::uint8_t, 4>{5, 100, 5, 100});
  CHECK(copies[1].scale > copies[0].scale);

  // Son dernier tic écrit est le trente-neuvième : 200 − 5 × 39.
  const std::array<ActorEffectPlay, 1> erased = {ActorEffectPlay{656, 39}};
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> fin;
  REQUIRE(BodyCopiesAfterEffects(erased, fin) == 2);
  CHECK(fin[0].tint[3] == 5);
  // Au quarantième, le cas ne relève plus le bit : plus de contour du tout.
  const std::array<ActorEffectPlay, 1> tombe = {ActorEffectPlay{656, 40}};
  CHECK(BodyCopiesAfterEffects(tombe, fin) == 0);

  // ⚠ LE CONTOUR D'ASURA EST BLEU, ET CE N'EST PAS UN DÉTAIL DE GOÛT. Le client
  // empaquette ses couleurs en BLEU, VERT, ROUGE, et l'ordre ne se devine pas
  // d'un nombre : lu à l'endroit, `0x3737FF` donne du rouge. Ce qui tranche est
  // le couple 0x4000 / 0x40000, qui reçoit ses composantes EN CLAIR — six
  // effets nommés bleus d'un côté, trois nommés rouges de l'autre.
  const std::array<ActorEffectPlay, 1> asura = {ActorEffectPlay{424, 50}};  // EF_ASURABODY
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> blue_level;
  REQUIRE(BodyCopiesAfterEffects(asura, blue_level) == 2);
  CHECK(blue_level[0].tint == std::array<std::uint8_t, 4>{55, 55, 255, 50});
}

TEST_CASE("Effets d'acteur : un effet ordinaire ne pose aucune copie", "[render][actoreffect]") {
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> copies;
  // ⚠ PAS EF_GREENBODY : il lève AUSSI le bit 0x8, et il a donc bien un halo.
  // Vingt effets nommés « corps » quelque chose ne disent pas vingt variations
  // — ils disent le même bit avec une couleur de plus.
  const std::array<ActorEffectPlay, 1> white = {ActorEffectPlay{1065, 5}};  // EF_WHITEBODY
  CHECK(BodyCopiesAfterEffects(white, copies) == 0);
  CHECK(BodyCopiesAfterEffects({}, copies) == 0);

  // ⚠ ET LA PLACE DONNÉE BORNE LE RENDU, sans rien écrire au-delà : deux effets
  // à copies sur le même corps en demanderaient quatre.
  const std::array<ActorEffectPlay, 2> two = {ActorEffectPlay{375, 0}, ActorEffectPlay{655, 10}};
  std::array<ro::render::ActorBodyCopy, 3> narrow;
  CHECK(BodyCopiesAfterEffects(two, narrow) == 3);
  CHECK(BodyCopiesAfterEffects(two, copies) == 4);
}

TEST_CASE("Effets d'acteur : le NOM de l'effet prédit la couleur de son VOILE",
          "[render][actoreffect]") {
  // ⚠ C'EST LE DEUXIÈME ORACLE, ET IL A CORRIGÉ UNE FAUTE. Le client empaquette
  // ses couleurs en BLEU, VERT, ROUGE, et rien dans un nombre ne le dit : lu à
  // l'endroit, `0x3737FF` donne du rouge. Ici les composantes sont passées EN
  // CLAIR — (5, 5, 255) pour le bit 0x4000, (255, 5, 5) pour 0x40000 — et six
  // effets nommés « bleu » d'un côté contre trois nommés « rouge » de l'autre
  // tranchent l'ordre sans qu'on ait à le supposer.
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> veils;

  const std::array<ActorEffectPlay, 1> blue_level = {ActorEffectPlay{542, 5}};  // EF_BLUEBODY
  REQUIRE(BodyVeilsAfterEffects(blue_level, veils) == 2);
  CHECK(veils[0].tint[0] == 5);
  CHECK(veils[0].tint[2] == 255);
  // Le voile est le corps à sa taille, pas un halo.
  CHECK(veils[0].scale == 1.0F);
  // Le client soumet le même quad DEUX fois, ce qui l'épaissit.
  CHECK(veils[1].tint == veils[0].tint);

  const std::array<ActorEffectPlay, 1> red = {ActorEffectPlay{544, 5}};  // EF_REDLIGHTBODY
  REQUIRE(BodyVeilsAfterEffects(red, veils) == 2);
  CHECK(veils[0].tint[0] == 255);
  CHECK(veils[0].tint[2] == 5);
}

TEST_CASE("Effets d'acteur : le voile du Gunslinger clignote, un tic sur quatre",
          "[render][actoreffect]") {
  // Madness Cancel (625) en bleu, Adjustment et Gatling Fever (626) en rouge :
  // le voile ne se lève qu'aux tics multiples de quatre, et le corps clignote.
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> veils;
  for (const std::uint32_t gunslinger_veil : {625U, 626U}) {
    INFO("effet " << gunslinger_veil);
    for (const std::uint32_t age_ticks : {0U, 4U, 8U}) {
      const std::array<ActorEffectPlay, 1> raised = {ActorEffectPlay{gunslinger_veil, age_ticks}};
      CHECK(BodyVeilsAfterEffects(raised, veils) == 2);
    }
    for (const std::uint32_t age_ticks : {1U, 2U, 3U, 5U}) {
      const std::array<ActorEffectPlay, 1> lowered = {ActorEffectPlay{gunslinger_veil, age_ticks}};
      CHECK(BodyVeilsAfterEffects(lowered, veils) == 0);
    }
  }
}

TEST_CASE("Effets d'acteur : le voile monte, tient, retombe, puis cesse", "[render][actoreffect]") {
  // La rampe du voile teinté : quinze par pas jusqu'au dixième, cent soixante
  // jusqu'au vingtième, cinq de moins par pas jusqu'au cinquantième, et plus
  // rien au-delà.
  //
  // ⚠ PAS SUR EF_BLUEBODY : sa rampe passe par des flottants que le relevé ne
  // suit pas, et son pas reste donc à zéro. EF_TAE_READY écrit le tic nu, et
  // c'est lui qui met la courbe à l'épreuve.
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> veils;
  const auto alpha_a = [&veils](std::uint32_t ticks) -> int {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{449, ticks}};  // EF_TAE_READY
    if (BodyVeilsAfterEffects(played, veils) == 0) {
      return -1;
    }
    return veils[0].tint[3];
  };
  CHECK(alpha_a(0) == 0);
  CHECK(alpha_a(10) == 150);
  CHECK(alpha_a(15) == 160);
  CHECK(alpha_a(20) == 160);
  CHECK(alpha_a(30) == 105);
  CHECK(alpha_a(50) == 5);
  // ⚠ PUIS IL S'ÉTEINT. Le client écrit encore au cinquantième tic
  // (`cmp tic, 32h ; jg`) et sort au cinquante et unième : la vie de l'effet
  // est de cinquante et un tics, et comme pour tous les autres, le pas se fige
  // au premier tic où le cas n'agit plus. À cinquante et un, c'est la branche
  // « au-delà, plus rien » de la rampe.
  CHECK(ActorEffectChangeFor(449)->life_ticks == 51);
  CHECK(alpha_a(1000) == -1);
}

TEST_CASE("Effets d'acteur : un effet sans voile n'en pose pas", "[render][actoreffect]") {
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> veils;
  const std::array<ActorEffectPlay, 1> white = {ActorEffectPlay{1065, 5}};  // EF_WHITEBODY
  CHECK(BodyVeilsAfterEffects(white, veils) == 0);
  CHECK(BodyVeilsAfterEffects({}, veils) == 0);
}

TEST_CASE("Effets d'acteur : la taille du corps, et le retour", "[render][actoreffect]") {
  // Le serveur n'a pas de champ pour la taille d'un corps : il envoie un effet,
  // et deux bits du mot de rendu portent la chose — `0x10` vers un demi, `0x400`
  // vers un et demi. Chacun a son COMPTEUR, et c'est le compteur qui avance la
  // courbe.
  const std::array<ActorEffectPlay, 1> begin = {ActorEffectPlay{420, 0}};  // EF_BABYBODY
  CHECK(BodyScaleAfterEffects(begin) == Catch::Approx(1.0F));
  const std::array<ActorEffectPlay, 1> fini = {ActorEffectPlay{420, 45}};
  CHECK(BodyScaleAfterEffects(fini) == Catch::Approx(ro::render::kSmallBodyScale));
  // Le compteur se fige à son dernier tic écrit, mais le BIT, levé avant la
  // comparaison d'âge, vit autant que l'instance : le corps reste petit.
  const std::array<ActorEffectPlay, 1> after = {ActorEffectPlay{420, 500}};
  CHECK(BodyScaleAfterEffects(after) == Catch::Approx(ro::render::kSmallBodyScale));

  // La variante RÉÉMISE pose la taille d'un coup : le serveur la redit à chaque
  // apparition, et un corps déjà petit ne doit pas rapetisser une seconde fois.
  const std::array<ActorEffectPlay, 1> redit = {ActorEffectPlay{421, 0}};  // EF_BABYBODY2
  CHECK(BodyScaleAfterEffects(redit) == Catch::Approx(ro::render::kSmallBodyScale));

  const std::array<ActorEffectPlay, 1> geant = {ActorEffectPlay{423, 0}};  // EF_GIANTBODY2
  CHECK(BodyScaleAfterEffects(geant) == Catch::Approx(ro::render::kBigBodyScale));

  // LE RETOUR. Le client RETIRE la réduction, puis joue `EF_BABYBODY_BACK`, qui
  // lève le MÊME `0x10` et ramène son compteur de quarante-cinq à zéro en
  // quinze pas — trois fois plus vite qu'il n'était descendu.
  const std::array<ActorEffectPlay, 1> retour = {ActorEffectPlay{540, 15}};
  CHECK(BodyScaleAfterEffects(retour) == Catch::Approx(1.0F));
  // À mi-retour, le corps est revenu à mi-chemin.
  const std::array<ActorEffectPlay, 1> mi_retour = {ActorEffectPlay{540, 5}};
  CHECK(BodyScaleAfterEffects(mi_retour) > ro::render::kSmallBodyScale);
  CHECK(BodyScaleAfterEffects(mi_retour) < 1.0F);
  // Passé sa borne, le retour ne relève plus rien : taille ordinaire.
  const std::array<ActorEffectPlay, 1> revenu = {ActorEffectPlay{540, 16}};
  CHECK(BodyScaleAfterEffects(revenu) == Catch::Approx(1.0F));
  // ⚠ LE PREMIER QUI LÈVE LE BIT L'EMPORTE. Tant que la réduction vit, le
  // retour trouve le bit déjà levé et n'écrit rien : le corps reste petit.
  const std::array<ActorEffectPlay, 2> sans_retrait = {ActorEffectPlay{420, 45},
                                                       ActorEffectPlay{540, 5}};
  CHECK(BodyScaleAfterEffects(sans_retrait) == Catch::Approx(ro::render::kSmallBodyScale));

  // Aucun effet de taille : la taille ordinaire, et pas de zéro.
  const std::array<ActorEffectPlay, 1> other = {ActorEffectPlay{120, 3}};  // EF_CLOAKING
  CHECK(BodyScaleAfterEffects(other) == Catch::Approx(1.0F));
  CHECK(BodyScaleAfterEffects({}) == Catch::Approx(1.0F));
}

TEST_CASE("Effets d'acteur : les anneaux qui s'ecartent", "[render][actoreffect]") {
  // `EF_REFLECTBODY` pose quatre copies de plus en plus larges, à un quart de
  // cycle les unes des autres, dont l'alpha tombe à mesure qu'elles s'élargissent.
  // Le compteur est celui du CLIENT, pas celui de l'effet : il tourne tant que
  // le bit est levé.
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> copies;
  const std::array<ActorEffectPlay, 1> reflet = {ActorEffectPlay{419, 0}};  // EF_REFLECTBODY
  REQUIRE(BodyCopiesAfterEffects(reflet, copies) == 4);
  // Chacune s'élargit autour du centre de ses calques — l'échelle reste à un,
  // sans quoi elle grandirait depuis les pieds et ne monterait que vers le
  // haut —, aucune ne dépasse la largeur du cycle, et toutes voilent le corps.
  for (std::size_t rang = 0; rang < 4; ++rang) {
    CHECK(copies[rang].scale == 1.0F);
    CHECK(copies[rang].grow_pixels >= 0.0F);
    CHECK(copies[rang].grow_pixels < 20.0F);
    CHECK(copies[rang].offset_pixels == std::array<float, 2>{0.0F, 0.0F});
    CHECK(copies[rang].in_front);
    CHECK_FALSE(copies[rang].additive);
  }
  // ⚠ PLUS LARGE VEUT DIRE PLUS EFFACÉ, et c'est la règle entière : l'anneau
  // qui touche le corps est le plus opaque.
  for (std::size_t rang = 1; rang < 4; ++rang) {
    const bool plus_large = copies[rang].grow_pixels > copies[rang - 1].grow_pixels;
    CHECK((plus_large ? copies[rang].tint[3] < copies[rang - 1].tint[3]
                      : copies[rang].tint[3] > copies[rang - 1].tint[3]));
  }
  // Et le cycle tourne : au bout de deux cents tics il est revenu à son point
  // de départ.
  const std::array<ActorEffectPlay, 1> later = {ActorEffectPlay{419, 200}};
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> still;
  REQUIRE(BodyCopiesAfterEffects(later, still) == 4);
  CHECK(still[0].grow_pixels == Catch::Approx(copies[0].grow_pixels));
}

TEST_CASE("Effets d'acteur : la croix a quatre bras, et attend son heure",
          "[render][actoreffect]") {
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> copies;

  // ⚠ AVANT LE TRENTIÈME TIC, LA CROIX N'EXISTE PAS. Le client sort du cas
  // avant de lever son bit : aucune copie, ni sur le corps ni ailleurs.
  const std::array<ActorEffectPlay, 1> before = {ActorEffectPlay{425, 10}};  // EF_4WAYBODY
  CHECK(BodyCopiesAfterEffects(before, copies) == 0);

  // À son trentième tic, les quatre copies sont sur le corps, à leur opacité
  // de départ.
  const std::array<ActorEffectPlay, 1> start = {ActorEffectPlay{425, 30}};
  REQUIRE(BodyCopiesAfterEffects(start, copies) == 4);
  for (std::size_t rang = 0; rang < 4; ++rang) {
    CHECK(copies[rang].offset_pixels == std::array<float, 2>{0.0F, 0.0F});
    CHECK(copies[rang].tint[3] == 150);
    CHECK(copies[rang].scale == Catch::Approx(1.0F));
  }

  // Dix tics après le départ : quarante pixels d'écart, et l'opacité a baissé.
  const std::array<ActorEffectPlay, 1> pendant = {ActorEffectPlay{425, 40}};
  REQUIRE(BodyCopiesAfterEffects(pendant, copies) == 4);
  CHECK(copies[0].offset_pixels == std::array<float, 2>{40.0F, 0.0F});
  CHECK(copies[1].offset_pixels == std::array<float, 2>{-40.0F, 0.0F});
  CHECK(copies[2].offset_pixels == std::array<float, 2>{0.0F, 40.0F});
  CHECK(copies[3].offset_pixels == std::array<float, 2>{0.0F, -40.0F});
  CHECK(copies[0].tint[3] == 100);

  // Trente tics après : la croix est partie et ne se voit plus.
  const std::array<ActorEffectPlay, 1> after = {ActorEffectPlay{425, 60}};
  REQUIRE(BodyCopiesAfterEffects(after, copies) == 4);
  CHECK(copies[0].tint[3] == 0);
}

TEST_CASE("Effets d'acteur : les quatre corps qui tremblent", "[render][actoreffect]") {
  // Les quatre `EF_QUAKEBODY` lèvent le même bit et tremblent de cinq pixels.
  // La table dit l'amplitude ; le tirage est l'affaire de l'appelant, parce que
  // `render` doit rester sans hasard.
  for (const std::uint32_t each_effect_id : {426U, 458U}) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{each_effect_id, 3}};
    CHECK(BodyPoseAfterEffects(played).jitter_pixels == Catch::Approx(5.0F));
  }
  // 550 ne lève son bit que du trente et unième au cinquantième tic : avant,
  // le client sort du cas sans rien écrire.
  const std::array<ActorEffectPlay, 1> trop_tot = {ActorEffectPlay{550, 3}};
  CHECK(BodyPoseAfterEffects(trop_tot).jitter_pixels == Catch::Approx(0.0F));
  const std::array<ActorEffectPlay, 1> a_l_heure = {ActorEffectPlay{550, 31}};
  CHECK(BodyPoseAfterEffects(a_l_heure).jitter_pixels == Catch::Approx(5.0F));
  // Un effet d'une autre famille ne fait rien trembler.
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{120, 3}};  // EF_CLOAKING
  CHECK(BodyPoseAfterEffects(cloaked).jitter_pixels == Catch::Approx(0.0F));

  // ⚠ ET LE TREMBLEMENT S'ARRÊTE AVEC CE QUI LE RELÈVE. L'acteur refait son mot
  // de rendu à chaque image : 426 tremble tant que vit son instance, quatorze
  // tics, et 550 jusqu'à la borne de son cas, cinquante et un.
  const std::array<ActorEffectPlay, 1> last = {ActorEffectPlay{426, 13}};
  CHECK(BodyPoseAfterEffects(last).jitter_pixels == Catch::Approx(5.0F));
  const std::array<ActorEffectPlay, 1> mort = {ActorEffectPlay{426, 14}};
  CHECK(BodyPoseAfterEffects(mort).jitter_pixels == Catch::Approx(0.0F));
  const std::array<ActorEffectPlay, 1> borne = {ActorEffectPlay{550, 50}};
  CHECK(BodyPoseAfterEffects(borne).jitter_pixels == Catch::Approx(5.0F));
  const std::array<ActorEffectPlay, 1> passe = {ActorEffectPlay{550, 51}};
  CHECK(BodyPoseAfterEffects(passe).jitter_pixels == Catch::Approx(0.0F));
}

TEST_CASE("Effets d'acteur : l'acteur pivote sur lui-même", "[render][actoreffect]") {
  using ro::render::SpinDegreesAfterEffects;

  // `EF_SPINMOVE` avance l'angle de face de douze degrés au premier tic, et
  // l'acteur paraît se retourner. Il ACCUMULE : le client avance son angle, il
  // ne le pose pas.
  const std::array<ActorEffectPlay, 1> begin = {ActorEffectPlay{890, 0}};
  CHECK(SpinDegreesAfterEffects(begin) == Catch::Approx(12.0F));
  const std::array<ActorEffectPlay, 1> huit = {ActorEffectPlay{890, 7}};
  CHECK(SpinDegreesAfterEffects(huit) == Catch::Approx(8 * 12.0F));

  // ⚠ IL ACCÉLÈRE PUIS RALENTIT, et les deux moitiés sont symétriques : douze
  // degrés par tic, puis dix-huit, vingt-quatre, quarante-huit au plus vite,
  // puis les mêmes en redescendant. Neuf cent cinquante-quatre degrés en tout.
  const std::array<ActorEffectPlay, 1> fini = {ActorEffectPlay{890, 45}};
  CHECK(SpinDegreesAfterEffects(fini) == Catch::Approx(954.0F));

  // ⚠ ET IL S'ARRÊTE LÀ OÙ IL EN EST. Le client cesse d'ajouter passé le
  // quarante-cinquième tic, et l'angle reste : l'acteur garde la face qu'il
  // avait en finissant de tourner, jusqu'à ce qu'il se tourne vers autre chose.
  const std::array<ActorEffectPlay, 1> after = {ActorEffectPlay{890, 5000}};
  CHECK(SpinDegreesAfterEffects(after) == Catch::Approx(954.0F));

  // La rotation ne recule jamais.
  float previous = 0.0F;
  for (std::uint32_t age = 0; age <= 60; ++age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{890, age}};
    const float rotated = SpinDegreesAfterEffects(played);
    CHECK(rotated >= previous);
    previous = rotated;
  }

  // Ce qui n'est pas de cette famille ne fait rien tourner.
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{120, 3}};  // EF_CLOAKING
  CHECK(SpinDegreesAfterEffects(cloaked) == Catch::Approx(0.0F));
  CHECK(SpinDegreesAfterEffects({}) == Catch::Approx(0.0F));

  // ⚠ LA FAMILLE SUFFIT, et un seul effet la porte : nommer l'identifiant dans
  // la pièce qui tourne en ferait un second lieu de vérité.
  std::size_t tourneurs = 0;
  for (const ActorEffectChange& change : ActorEffectChangeTable()) {
    if (change.family == ActorEffectFamily::kSpin) {
      ++tourneurs;
    }
  }
  CHECK(tourneurs == 1);
}

TEST_CASE("Effets d'acteur : le corps fait le tour de sa place", "[render][actoreffect]") {
  // `EF_SPINEDBODY` mène la tête sur un cercle de quarante pixels, et fait
  // tourner chaque calque sur lui-même. Au départ du cercle elle est exactement à sa place : le
  // client compense son rayon, et l'oublier ferait sauter le corps de quarante pixels à l'instant
  // où l'effet commence.
  const std::array<ActorEffectPlay, 1> begin = {ActorEffectPlay{414, 0}};  // EF_SPINEDBODY
  const ActorBodyPose at_start = BodyPoseAfterEffects(begin);
  CHECK(at_start.swing_pixels[0] == Catch::Approx(0.0F).margin(0.01F));
  CHECK(at_start.swing_pixels[1] == Catch::Approx(0.0F).margin(0.01F));

  // ⚠ LE TOUR EST COMPLET AU DIX-HUITIÈME TIC, et pas avant : le compteur
  // avance de DEUX par tic, l'angle vaut 180 − 180 cos(5 × compteur), et il
  // atteint le tour entier quand le compteur vaut trente-six. La vie de
  // l'effet est justement dix-huit tics.
  CHECK(ActorEffectChangeFor(414)->life_ticks == 18);
  const std::array<ActorEffectPlay, 1> fini = {ActorEffectPlay{414, 18}};
  const ActorBodyPose at_end = BodyPoseAfterEffects(fini);
  CHECK(at_end.swing_pixels[0] == Catch::Approx(0.0F).margin(0.01F));
  CHECK(at_end.swing_pixels[1] == Catch::Approx(0.0F).margin(0.01F));

  // À mi-chemin il est au plus loin : un demi-tour, donc deux rayons plus bas.
  const std::array<ActorEffectPlay, 1> mi = {ActorEffectPlay{414, 9}};
  const ActorBodyPose halfway = BodyPoseAfterEffects(mi);
  CHECK(halfway.swing_pixels[1] == Catch::Approx(-80.0F).margin(0.5F));
  // Les autres calques tournent sur un cercle de dix : vingt pixels plus bas.
  CHECK(halfway.layer_swing_pixels[1] == Catch::Approx(-20.0F).margin(0.2F));
  // Et chaque calque fait la culbute sur lui-même : tête en bas à mi-chemin,
  // à l'endroit au départ.
  CHECK(halfway.layer_spin_degrees == Catch::Approx(180.0F).margin(0.5F));
  CHECK(at_start.layer_spin_degrees == Catch::Approx(0.0F).margin(0.01F));

  // Et il ne s'éloigne jamais de plus d'un diamètre.
  for (std::uint32_t age = 0; age <= 18; ++age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{414, age}};
    const ActorBodyPose pose = BodyPoseAfterEffects(played);
    CHECK(std::abs(pose.swing_pixels[0]) <= 40.01F);
    CHECK(pose.swing_pixels[1] <= 0.01F);
    CHECK(pose.swing_pixels[1] >= -80.01F);
  }

  // ⚠ ET DEUX DES TROIS N'ENTRENT DANS LE CERCLE QU'AU QUATORZIÈME TIC. Leur
  // rampe vaut « tic moins quatorze » et elle est NÉGATIVE avant : la laisser
  // courir ferait décrire le cercle à l'envers pendant quatorze tics.
  const std::array<ActorEffectPlay, 1> before = {ActorEffectPlay{466, 5}};  // EF_SPINEDBODY2
  const ActorBodyPose pose_before_start = BodyPoseAfterEffects(before);
  CHECK(pose_before_start.swing_pixels[0] == Catch::Approx(0.0F).margin(0.01F));
  CHECK(pose_before_start.swing_pixels[1] == Catch::Approx(0.0F).margin(0.01F));

  // Ce qui ne lève pas ce bit ne tourne pas.
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{120, 3}};  // EF_CLOAKING
  CHECK(BodyPoseAfterEffects(cloaked).swing_pixels[0] == Catch::Approx(0.0F));
}

TEST_CASE("Effets d'acteur : le corps s'AJOUTE au décor", "[render][actoreffect]") {
  // ⚠ CE N'EST PAS UN MODE À INTERPRÉTER, c'est un facteur de mélange Direct3D.
  // L'acteur écrit cinq et deux — ou six — dans les deux facteurs de mélange de
  // son quad, et le dessin de la scène les passe TELS QUELS sous les états 19
  // et 20, `D3DRS_SRCBLEND` et `D3DRS_DESTBLEND`. Deux
  // est `D3DBLEND_ONE` : le corps s'ajoute. Six est `D3DBLEND_INVSRCALPHA` : il
  // recouvre.
  // `EF_DA_SPACE` et `EF_AURABLADE2` lèvent le bit de l'ARME : elle seule
  // s'ajoute.
  const ro::render::ActorPartSet weapon = ro::render::PartBit(ro::render::ActorPart::kWeapon);
  const std::array<ActorEffectPlay, 1> espace = {ActorEffectPlay{667, 3}};  // EF_DA_SPACE
  CHECK(BodyPoseAfterEffects(espace).additive_parts == weapon);
  const std::array<ActorEffectPlay, 1> lame = {ActorEffectPlay{474, 3}};  // EF_AURABLADE2
  CHECK(BodyPoseAfterEffects(lame).additive_parts == weapon);

  // Ce qui ne lève ni l'un ni l'autre de ces deux bits recouvre, comme tout le
  // monde.
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{120, 3}};  // EF_CLOAKING
  CHECK(BodyPoseAfterEffects(cloaked).additive_parts == 0);
  CHECK(BodyPoseAfterEffects({}).additive_parts == 0);
}

TEST_CASE("Effets d'acteur : la lame de lumière double l'ARME, pas le corps",
          "[render][actoreffect]") {
  // `EF_LIGHTBLADE` lève le bit 0x4 : le client soumet l'arme une seconde fois,
  // en addition, une image sur deux. Ce n'est pas une copie du corps entier
  // posée derrière lui.
  const std::array<ActorEffectPlay, 1> blade = {ActorEffectPlay{382, 0}};  // EF_LIGHTBLADE
  const ActorBodyPose pose = BodyPoseAfterEffects(blade);
  CHECK(pose.doubles_weapon);
  CHECK(pose.additive_parts == 0);
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> copies;
  CHECK(BodyCopiesAfterEffects(blade, copies) == 0);
  CHECK_FALSE(BodyPoseAfterEffects({}).doubles_weapon);
}

TEST_CASE("Effets d'acteur : halos et voiles s'AJOUTENT, sur leurs calques",
          "[render][actoreffect]") {
  const ro::render::ActorPartSet garment = ro::render::PartBit(ro::render::ActorPart::kGarment);
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> copies;

  // La lueur qui respire : en addition, sur le corps, la tête, les coiffes et la
  // cape — ni l'arme ni le bouclier.
  const std::array<ActorEffectPlay, 1> glow = {ActorEffectPlay{375, 0}};  // EF_ASSUMPTIO
  REQUIRE(BodyCopiesAfterEffects(glow, copies) == 2);
  CHECK(copies[0].additive);
  CHECK(copies[0].parts == ro::render::kBodyAndHeadwearParts);

  // ⚠ UN SAUT L'ÉTEINT, même porté par un autre effet : le client lit le mot de
  // rendu de l'acteur entier.
  constexpr std::uint32_t kJumpAge = 40;
  const std::array<ActorEffectPlay, 2> jumping = {ActorEffectPlay{375, kJumpAge},
                                                  ActorEffectPlay{445, kJumpAge}};
  REQUIRE((ro::render::RenderBitsAt(*ActorEffectChangeFor(445), kJumpAge) & 0x10000U) != 0);
  CHECK(BodyCopiesAfterEffects(jumping, copies) == 0);

  // Le contour d'Asura : en addition, sur tous les calques.
  const std::array<ActorEffectPlay, 1> asura = {ActorEffectPlay{424, 50}};  // EF_ASURABODY
  REQUIRE(BodyCopiesAfterEffects(asura, copies) == 2);
  CHECK(copies[0].additive);
  CHECK(copies[0].parts == ro::render::kAllActorParts);

  // Le voile bleu : en addition, sur tout sauf la cape.
  const std::array<ActorEffectPlay, 1> blue = {ActorEffectPlay{625, 0}};
  REQUIRE(BodyVeilsAfterEffects(blue, copies) == 2);
  CHECK(copies[0].additive);
  CHECK((copies[0].parts & garment) == 0);
  CHECK(copies[0].parts == (ro::render::kAllActorParts & ~garment));
}

TEST_CASE("Effets d'acteur : le corps écrasé", "[render][actoreffect]") {
  // `EF_PRESSEDBODY` rapproche le bord HAUT du quad de son bord bas : la
  // hauteur tombe d'un cinquième par pas jusqu'au troisième, tient autour de
  // quatre dixièmes, puis revient d'un coup.
  //
  // ⚠ SON COMPTEUR NE PART QU'AU DIX-SEPTIÈME TIC — sa rampe vaut « tic moins
  // dix-sept » —, et il est négatif avant : le laisser courir ferait GRANDIR le
  // corps avant de l'écraser.
  const std::array<ActorEffectPlay, 1> before = {ActorEffectPlay{413, 5}};  // EF_PRESSEDBODY
  CHECK(BodyPoseAfterEffects(before).height_scale == Catch::Approx(1.0F));

  const std::array<ActorEffectPlay, 1> begin = {ActorEffectPlay{413, 17}};
  CHECK(BodyPoseAfterEffects(begin).height_scale == Catch::Approx(1.0F));
  const std::array<ActorEffectPlay, 1> ecrase = {ActorEffectPlay{413, 20}};
  CHECK(BodyPoseAfterEffects(ecrase).height_scale == Catch::Approx(0.4F));
  const std::array<ActorEffectPlay, 1> held = {ActorEffectPlay{413, 40}};
  CHECK(BodyPoseAfterEffects(held).height_scale == Catch::Approx(0.4F));

  // Passé le trente-troisième pas, le corps se redresse.
  const std::array<ActorEffectPlay, 1> fini = {ActorEffectPlay{413, 60}};
  CHECK(BodyPoseAfterEffects(fini).height_scale == Catch::Approx(1.0F));

  // Ce qui ne lève pas ce bit reste debout.
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{120, 3}};  // EF_CLOAKING
  CHECK(BodyPoseAfterEffects(cloaked).height_scale == Catch::Approx(1.0F));
}

TEST_CASE("Effets d'acteur : un effet écrit l'ÉTAT de l'acteur, et il l'écrit DEUX fois",
          "[render][actoreffect]") {
  using ro::render::EffectMotionWrite;
  using ro::render::LastMotionWrite;

  // ⚠ DEUX ÉCRITURES DU MÊME CHAMP, À DEUX TICS. `EF_JUMPKICK` pose l'état
  // vingt-sept au cinquième tic et l'état deux au trente-cinquième. Une table
  // qui ne daterait pas ses écritures les réunirait en une seule — la
  // dernière —, et le coup de pied ne montrerait que sa fin.
  const ActorEffectChange* strike = ActorEffectChangeFor(457);  // EF_JUMPKICK
  REQUIRE(strike != nullptr);
  CHECK(strike->writes[0].at_tick == 5);
  CHECK(strike->writes[1].at_tick == 35);

  // Avant le cinquième tic, rien n'est écrit.
  CHECK_FALSE(LastMotionWrite(ActorEffectPlay{457, 2}).has_value());

  // Puis l'état vingt-sept, puis l'état deux : la DERNIÈRE écriture faite.
  const std::optional<EffectMotionWrite> elan = LastMotionWrite(ActorEffectPlay{457, 10});
  REQUIRE(elan.has_value());
  CHECK(elan->tick == 5);
  CHECK(elan->state == 27);
  const std::optional<EffectMotionWrite> fin = LastMotionWrite(ActorEffectPlay{457, 40});
  REQUIRE(fin.has_value());
  CHECK(fin->tick == 35);
  CHECK(fin->state == 2);

  // Ce qui n'écrit aucun état n'en écrit aucun.
  CHECK_FALSE(LastMotionWrite(ActorEffectPlay{120, 3}).has_value());  // EF_CLOAKING
}

TEST_CASE("Effets d'acteur : la POSE que le coup de pied sauté mène, et qu'il garde",
          "[render][actoreffect]") {
  using ro::render::EffectLedPoseFrameAt;
  using ro::render::HeldPoseFrame;
  using ro::render::kNoLedPoseTick;

  // 🔴 ÉCRIRE L'ÉTAT NE CHANGE PAS L'IMAGE. Ce qui la change, c'est la pose
  // que l'effet mène au même tic : l'image cinq du rang douze — l'élan —, tenue
  // vingt-cinq millisecondes. Les deux jumeaux la mènent à l'identique.
  for (const std::uint32_t effect_id : {457U, 2426U}) {
    const ActorEffectChange* strike = ActorEffectChangeFor(effect_id);
    REQUIRE(strike != nullptr);
    CHECK(strike->led_pose.tick == 5);
    CHECK(strike->led_pose.play_rank == 12);
    CHECK(strike->led_pose.first_frame == 5);
    CHECK(strike->led_pose.last_frame == 5);
    CHECK(strike->led_pose.hold_rank == 12);
    CHECK(strike->led_pose.hold_frame == 5);
    CHECK(strike->led_pose.hold_ms == 25);
  }
  // Un effet qui n'en mène aucune le dit.
  CHECK(ActorEffectChangeFor(120)->led_pose.tick == kNoLedPoseTick);  // EF_CLOAKING

  // ⚠ ET L'ÉCHÉANCE NE REND PAS L'ACTEUR À SON ANIMATION : passé les
  // vingt-cinq millisecondes, l'image tenue reste, jusqu'à la motion suivante.
  const ro::render::EffectLedPose& pose = ActorEffectChangeFor(457)->led_pose;
  constexpr std::uint32_t kTickMs = 16;
  const HeldPoseFrame at_start = EffectLedPoseFrameAt(pose, 0, kTickMs);
  CHECK(at_start.rank == 12);
  CHECK(at_start.frame == 5);
  const HeldPoseFrame long_after = EffectLedPoseFrameAt(pose, 480, kTickMs);
  CHECK(long_after.rank == 12);
  CHECK(long_after.frame == 5);
}

TEST_CASE("Effets d'acteur : le corps soulevé", "[render][actoreffect]") {
  // `EF_KICKEDBODY` déplace les DEUX bords du quad vers le haut d'une part de
  // leur écart : le corps monte sans changer de taille. Il monte par un sinus
  // jusqu'au dix-huitième pas, tient trois dixièmes, puis redescend.
  //
  // ⚠ SON COMPTEUR NE PART QU'AU TRENTIÈME TIC. Avant, rien ne bouge.
  const std::array<ActorEffectPlay, 1> before = {ActorEffectPlay{415, 10}};  // EF_KICKEDBODY
  CHECK(BodyPoseAfterEffects(before).lift_fraction == Catch::Approx(0.0F));

  const std::array<ActorEffectPlay, 1> start = {ActorEffectPlay{415, 30}};
  CHECK(BodyPoseAfterEffects(start).lift_fraction == Catch::Approx(0.0F));

  // Au dix-huitième pas — le quarante-huitième tic — il est au plus haut.
  const std::array<ActorEffectPlay, 1> top = {ActorEffectPlay{415, 48}};
  CHECK(BodyPoseAfterEffects(top).lift_fraction == Catch::Approx(0.3F).margin(0.02F));

  // Et il redescend : passé le vingt-troisième pas, plus rien.
  const std::array<ActorEffectPlay, 1> retombe = {ActorEffectPlay{415, 54}};
  CHECK(BodyPoseAfterEffects(retombe).lift_fraction == Catch::Approx(0.0F));

  // ⚠ C'EST UNE FRACTION, PAS DES PIXELS : le client déplace les bords d'une
  // part de leur écart, et un grand corps monte donc plus haut qu'un petit.
  for (std::uint32_t age = 30; age <= 60; ++age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{415, age}};
    CHECK(BodyPoseAfterEffects(played).lift_fraction >= 0.0F);
    CHECK(BodyPoseAfterEffects(played).lift_fraction <= 0.31F);
  }
}

TEST_CASE("Effets d'acteur : le coureur laisse des pas", "[render][actoreffect]") {
  using ro::render::TrailAfterEffects;

  // ⚠ RIEN NE SE POSE PENDANT LES SEPT PREMIERS TICS. Le semeur du client tourne
  // dès la course, mais attend que l'horloge de l'effet dépasse sa période ; une
  // empreinte posée au premier tic tomberait sous un pied qui n'a pas bougé.
  const std::array<ActorEffectPlay, 1> begin = {ActorEffectPlay{442, 3}};  // EF_RUN
  REQUIRE(TrailAfterEffects(begin).has_value());
  CHECK_FALSE(TrailAfterEffects(begin)->clock_past_period);
  const std::array<ActorEffectPlay, 1> juste = {ActorEffectPlay{442, 7}};
  CHECK_FALSE(TrailAfterEffects(juste)->clock_past_period);

  const std::array<ActorEffectPlay, 1> lance = {ActorEffectPlay{442, 8}};
  CHECK(TrailAfterEffects(lance)->clock_past_period);
  const std::array<ActorEffectPlay, 1> longtemps = {ActorEffectPlay{442, 500}};
  CHECK(TrailAfterEffects(longtemps)->clock_past_period);

  // Un seul effet du lot laisse des traces, et rien d'autre n'en laisse.
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{120, 50}};  // EF_CLOAKING
  CHECK_FALSE(TrailAfterEffects(cloaked).has_value());
  CHECK_FALSE(TrailAfterEffects({}).has_value());

  // Les deux empreintes alternent, et ce sont bien deux effets distincts.
  CHECK(ro::render::kFootprintEffects[0] != ro::render::kFootprintEffects[1]);
}

TEST_CASE("Effets d'acteur : un clignotement n'est additif qu'avec sa teinte principale",
          "[render][actor_effect]") {
  // 🔴 SHRINK (599) CLIGNOTE DU TIC 5 AU TIC 25 : jaune lumineux aux tics pairs,
  // corps normal aux impairs. Le bit additif levé à chaque tic laissait le
  // corps lumineux d'un bout à l'autre, et le clignotement ne se voyait plus.
  constexpr std::uint32_t kShrinkEffect = 599;
  constexpr std::uint32_t kAdditive = 0x1;
  const ActorEffectChange* shrink = ActorEffectChangeFor(kShrinkEffect);
  REQUIRE(shrink != nullptr);
  CHECK((ro::render::RenderBitsAt(*shrink, 6) & kAdditive) != 0);
  CHECK((ro::render::RenderBitsAt(*shrink, 7) & kAdditive) == 0);
  CHECK((ro::render::RenderBitsAt(*shrink, 8) & kAdditive) != 0);

  // Un autre bit du même effet tient tout le tic : 500 pose 0x8 avant de
  // regarder la parité de son âge.
  constexpr std::uint32_t kBlinkWithTwoBits = 500;
  constexpr std::uint32_t kHeldBit = 0x8;
  const ActorEffectChange* both = ActorEffectChangeFor(kBlinkWithTwoBits);
  REQUIRE(both != nullptr);
  CHECK((ro::render::RenderBitsAt(*both, 3) & kHeldBit) != 0);
  CHECK((ro::render::RenderBitsAt(*both, 3) & kAdditive) == 0);
}

TEST_CASE("Effets d'acteur : les voiles de Kaupe et des Warm suivent leur rampe divisée",
          "[render][actoreffect]") {
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> veils;
  // Kaupe (542) écrit le tiers de son âge : au trentième tic, le pas dix, et
  // l'alpha monte à cent cinquante.
  const std::array<ActorEffectPlay, 1> kaupe = {ActorEffectPlay{542, 30}};
  REQUIRE(BodyVeilsAfterEffects(kaupe, veils) == 2);
  CHECK(veils[0].tint[3] == 150);
  // Les Warm (544) en écrivent le huitième jusqu'au tic deux cents, puis
  // vingt-cinq : le rouge tient à cent trente tant que l'état tient.
  const std::array<ActorEffectPlay, 1> warm = {ActorEffectPlay{544, 1000}};
  REQUIRE(BodyVeilsAfterEffects(warm, veils) == 2);
  CHECK(veils[0].tint == std::array<std::uint8_t, 4>{255, 5, 5, 130});
  const std::array<ActorEffectPlay, 1> warm_rising = {ActorEffectPlay{544, 40}};
  REQUIRE(BodyVeilsAfterEffects(warm_rising, veils) == 2);
  CHECK(veils[0].tint[3] == 75);
  // Kaizel et Kaahi (543) tiennent le pas vingt-cinq jusqu'au tic cent
  // cinquante, puis redescendent : le voile dure, au lieu de s'éteindre au
  // cinquantième tic.
  const std::array<ActorEffectPlay, 1> kaizel_held = {ActorEffectPlay{543, 100}};
  REQUIRE(BodyVeilsAfterEffects(kaizel_held, veils) == 2);
  CHECK(veils[0].tint[3] == 130);
  const std::array<ActorEffectPlay, 1> kaizel_falling = {ActorEffectPlay{543, 160}};
  REQUIRE(BodyVeilsAfterEffects(kaizel_falling, veils) == 2);
  CHECK(veils[0].tint[3] == 160);
}
