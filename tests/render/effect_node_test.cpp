#include "ro/render/effect_node.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pattern_table.h"

using ro::render::AdvanceEffectNode;
using ro::render::EffectNode;
using ro::render::EffectNodeAlpha;
using ro::render::kEffectNodeAlphaBounces;
using ro::render::kEffectNodeChannelMax;
using ro::render::kEffectNodeFreezesGrowth;
using ro::render::kEffectNodeWandersAngle;

namespace {

// Le nœud de particule d'Increase AGI, tel que le client natif le pose : cinquante
// tics de vie, l'alpha monte jusqu'à deux cents en vingt tics, et la sortie
// commence vingt tics avant la fin.
EffectNode IncreaseAgilityParticle() {
  EffectNode node;
  node.life_ticks = 50;
  node.fade_out_tick = 50 - 20;
  node.alpha = 0.0F;
  node.alpha_max = 200.0F;
  node.alpha_step_per_tick = 200.0F / 20.0F;
  node.step_length = 0.53F;
  node.half_width.half_size = 4.7F;
  node.half_height.half_size = 0.1796F;
  return node;
}

}  // namespace

TEST_CASE("Nœud d'effet : l'accélération pousse la croissance, qui pousse la TAILLE",
          "[render][effectnode]") {
  // Les trois étages sont ceux du client. Ce ne sont PAS des positions : le
  // dessinateur s'en sert comme demi-largeur et demi-hauteur du quad, si bien
  // qu'une accélération fait un halo qui enfle de plus en plus vite.
  EffectNode node;
  node.life_ticks = 100;
  node.half_width.half_size = 10.0F;
  node.half_width.growth_per_tick = 2.0F;
  node.half_width.growth_acceleration_per_tick = 1.0F;
  node.half_height.half_size = 3.0F;

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.half_width.growth_per_tick == Catch::Approx(3.0F));
  CHECK(node.half_width.half_size == Catch::Approx(13.0F));
  CHECK(node.half_height.half_size == Catch::Approx(3.0F));

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.half_width.growth_per_tick == Catch::Approx(4.0F));
  CHECK(node.half_width.half_size == Catch::Approx(17.0F));
}

TEST_CASE("Nœud d'effet : figer la croissance coupe AUSSI l'accélération", "[render][effectnode]") {
  // Ne remettre que la croissance à zéro laisserait l'accélération la relancer
  // au tic suivant : le quad se remettrait à enfler au lieu de se figer.
  EffectNode node;
  node.life_ticks = 100;
  node.flags = kEffectNodeFreezesGrowth;
  node.motion_period_ticks = 2;
  node.half_width.growth_per_tick = 5.0F;
  node.half_width.growth_acceleration_per_tick = 5.0F;

  REQUIRE(AdvanceEffectNode(node));
  REQUIRE(node.half_width.half_size == Catch::Approx(10.0F));
  REQUIRE(AdvanceEffectNode(node));
  const float frozen_at = node.half_width.half_size;
  REQUIRE(frozen_at == Catch::Approx(25.0F));

  // Le gel tombe ENTRE les deux étages : au tic où il mord, le nœud ne
  // parcourt même pas le pas que sa vitesse lui promettait. Avancer les deux
  // d'un coup le ferait glisser une dernière fois.
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.half_width.growth_per_tick == Catch::Approx(0.0F));
  CHECK(node.half_width.growth_acceleration_per_tick == Catch::Approx(0.0F));
  CHECK(node.half_width.half_size == Catch::Approx(frozen_at));

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.half_width.half_size == Catch::Approx(frozen_at));
}

TEST_CASE("Nœud d'effet : la sortie annule l'alpha PILE à la fin de vie", "[render][effectnode]") {
  // C'est la règle qui rend la famille procédurale jouable sans table de
  // fondus : le pas de sortie se CALCULE à partir de l'alpha courant et du
  // temps qui reste. Aucun nœud ne se coupe net.
  EffectNode node = IncreaseAgilityParticle();

  for (std::int32_t tick = 0; tick < node.fade_out_tick; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
  }
  // Vingt tics à dix montent l'alpha à son plafond, où il tient.
  CHECK(node.alpha == Catch::Approx(200.0F));

  // 🔴 LA DERNIÈRE IMAGE MONTRÉE est celle de l'âge 50, sa vie, à l'alpha que la
  // sortie a mené à zéro : la mise à jour qui le tue porte son âge au-delà.
  float last_shown_alpha = node.alpha;
  while (AdvanceEffectNode(node)) {
    last_shown_alpha = node.alpha;
  }
  CHECK(node.age_ticks == 51);
  CHECK(last_shown_alpha == Catch::Approx(0.0F).margin(0.001F));
  CHECK(EffectNodeAlpha(node) == 0);
}

TEST_CASE("Nœud d'effet : un nœud à peine monté sort plus doucement", "[render][effectnode]") {
  // Le pas de sortie part de l'alpha COURANT, pas du plafond. Deux nœuds de
  // même vie mais d'alpha différent descendent donc à des vitesses différentes,
  // et arrivent tous deux à zéro au même tic.
  EffectNode faint = IncreaseAgilityParticle();
  faint.alpha_step_per_tick = 0.0F;
  faint.alpha = 50.0F;

  for (std::int32_t tick = 0; tick <= faint.fade_out_tick; ++tick) {
    REQUIRE(AdvanceEffectNode(faint));
  }
  CHECK(faint.alpha_step_per_tick == Catch::Approx(-50.0F / 20.0F));

  float last_shown_alpha = faint.alpha;
  while (AdvanceEffectNode(faint)) {
    last_shown_alpha = faint.alpha;
  }
  CHECK(faint.age_ticks == 51);
  CHECK(last_shown_alpha == Catch::Approx(0.0F).margin(0.001F));
  CHECK(EffectNodeAlpha(faint) == 0);
}

TEST_CASE("Nœud d'effet : l'alpha en va-et-vient rebondit sur ses bornes", "[render][effectnode]") {
  EffectNode node;
  node.life_ticks = 100;
  node.fade_out_tick = 100;
  node.flags = kEffectNodeAlphaBounces;
  node.alpha = 0.0F;
  node.alpha_min = 0.0F;
  node.alpha_max = 20.0F;
  node.alpha_step_per_tick = 10.0F;

  REQUIRE(AdvanceEffectNode(node));
  REQUIRE(node.alpha == Catch::Approx(10.0F));
  REQUIRE(AdvanceEffectNode(node));
  REQUIRE(node.alpha == Catch::Approx(20.0F));

  // Arrivé au plafond, le pas s'INVERSE — le nœud redescend au lieu de rester
  // collé à sa borne.
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.alpha_step_per_tick == Catch::Approx(-10.0F));
  CHECK(node.alpha == Catch::Approx(10.0F));
}

TEST_CASE("Nœud d'effet : le rouge s'ENROULE là où le vert et le bleu se bornent",
          "[render][effectnode]") {
  // Relevé tel quel dans le client. L'asymétrie ressemble à une inversion de
  // branches restée en place ; la reproduire est ce qui garde la parité, et
  // c'est ce test qui empêche de la « corriger » par mégarde.
  EffectNode node;
  node.life_ticks = 100;
  node.color = {250.0F, 250.0F, 5.0F};
  node.color_step_per_tick = {10.0F, 10.0F, -10.0F};

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.color[0] == Catch::Approx(0.0F));
  CHECK(node.color[1] == Catch::Approx(kEffectNodeChannelMax));
  CHECK(node.color[2] == Catch::Approx(0.0F));
}

TEST_CASE("Nœud d'effet : l'orbite avance d'un pas long comme sa longueur de pas",
          "[render][effectnode]") {
  // L'orbite n'est pas la taille : le nœud avance d'un pas dans la direction de
  // son angle, et ce pas s'accumule. Un angle nul pousse vers la droite, sans
  // rien en ordonnée ; et l'ordonnée DESCEND, comme à l'écran.
  EffectNode node;
  node.life_ticks = 100;
  node.step_length = 2.0F;

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.orbit_x == Catch::Approx(2.0F));
  CHECK(node.orbit_y == Catch::Approx(0.0F).margin(0.001F));

  node.orbit_degrees = 90.0F;
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.orbit_x == Catch::Approx(2.0F).margin(0.001F));
  CHECK(node.orbit_y == Catch::Approx(-2.0F).margin(0.001F));
}

TEST_CASE("Nœud d'effet : les textures épuisées tuent un nœud qui ne boucle pas",
          "[render][effectnode]") {
  EffectNode node;
  node.life_ticks = 1000;
  node.texture_count = 3;
  node.frame_hold_ticks = 2;
  // Le client fait BOUCLER par défaut : il faut le dire pour éprouver la fin.
  node.loops_frames = false;

  // Une image tous les deux tics, et le nœud meurt à la troisième plutôt que
  // de tenir sa dernière pose jusqu'à la fin de vie.
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.frame == 1);
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.frame == 1);
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.frame == 2);
  REQUIRE(AdvanceEffectNode(node));
  CHECK_FALSE(AdvanceEffectNode(node));

  EffectNode looping = node;
  looping.frame = 0;
  looping.age_ticks = 0;
  looping.loops_frames = true;
  for (std::int32_t tick = 0; tick < 12; ++tick) {
    REQUIRE(AdvanceEffectNode(looping));
  }
  CHECK(looping.frame < 3);
}

TEST_CASE("Nœud d'effet : sans boucle, le bit 0x800 FIGE la dernière image au lieu de tuer",
          "[render][effectnode]") {
  // Sans boucle et le bit 0x800 des drapeaux levé, la dernière image reste, et
  // le nœud vit sa vie.
  EffectNode node;
  node.life_ticks = 1000;
  node.texture_count = 3;
  node.frame_hold_ticks = 2;
  node.loops_frames = false;
  node.holds_last_frame = true;

  for (std::int32_t tick = 0; tick < 20; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
  }
  CHECK(node.frame == 2);
}

TEST_CASE("Nœud d'effet : l'errance est REJOUABLE", "[render][effectnode]") {
  // Le client secoue l'angle avec rand(), dont la suite ne se reproduit pas. Le
  // nœud porte donc sa graine : deux nœuds identiques errent pareil, ce qui
  // rend le mouvement vérifiable au lieu d'être seulement plausible.
  EffectNode node;
  node.life_ticks = 200;
  node.flags = kEffectNodeWandersAngle;
  node.motion_period_ticks = 1;
  node.wander_seed = 7;

  EffectNode twin = node;
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
    REQUIRE(AdvanceEffectNode(twin));
  }
  CHECK(node.orbit_degrees == Catch::Approx(twin.orbit_degrees));
  CHECK(node.motion_period_ticks == twin.motion_period_ticks);

  // Une autre graine donne un autre mouvement, sans quoi la graine ne servirait
  // à rien.
  EffectNode other;
  other.life_ticks = 200;
  other.flags = kEffectNodeWandersAngle;
  other.motion_period_ticks = 1;
  other.wander_seed = 99;
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    REQUIRE(AdvanceEffectNode(other));
  }
  CHECK(other.orbit_degrees != Catch::Approx(node.orbit_degrees));

  // La secousse reste petite : elle dévie l'angle, elle ne le retourne pas.
  CHECK(node.orbit_degrees < 40.0F * 8.0F);
  CHECK(node.orbit_degrees > -40.0F * 8.0F);
}

TEST_CASE("Nœud d'effet : la rotation a sa propre accélération", "[render][effectnode]") {
  // Trois étages là aussi, et INDÉPENDANTS de la croissance : un quad peut
  // partir lentement et s'emballer pendant que sa taille reste figée.
  EffectNode node;
  node.life_ticks = 100;
  node.flags = kEffectNodeFreezesGrowth;
  node.motion_period_ticks = 0;
  node.half_width.growth_per_tick = 5.0F;
  node.spin_speed_per_tick = 2.0F;
  node.spin_acceleration_per_tick = 1.0F;

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.spin_degrees == Catch::Approx(3.0F));
  CHECK(node.half_width.half_size == Catch::Approx(0.0F));

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.spin_degrees == Catch::Approx(7.0F));
  CHECK(node.half_width.growth_per_tick == Catch::Approx(0.0F));
}

TEST_CASE("Nœud d'effet : les défauts sont ceux du CLIENT", "[render][effectnode]") {
  // Ce sont les valeurs initiales d'un nœud du client. Ce ne sont pas des
  // valeurs de commodité : un effet qui ne pose pas un champ
  // COMPTE sur celle-ci, et en choisir une autre change ce qu'il montre sans
  // que rien ne le dise.
  const EffectNode node;

  // ⚠ LE PAS D'ORBITE EST NUL, et c'est le plus important. Un nœud tire souvent
  // son angle au hasard sans poser de pas ; un pas non nul le ferait alors
  // dériver dans une direction quelconque, et une nuée partirait en étoile au
  // lieu de tenir en place.
  CHECK(node.step_length == Catch::Approx(0.0F));

  // Le plafond d'alpha s'arrête à 254, pas à 255.
  CHECK(node.alpha_max == Catch::Approx(254.0F));
  CHECK(node.life_ticks == 100);
  CHECK(node.frame_hold_ticks == 4);
  CHECK(node.texture_count == 1);
  // Il BOUCLE, et c'est ce qui rend les quatre tics vivables : une seule
  // texture sans boucle épuiserait ses images au premier tic.
  CHECK(node.loops_frames);

  EffectNode fresh;
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    REQUIRE(AdvanceEffectNode(fresh));
  }
  CHECK(fresh.orbit_x == Catch::Approx(0.0F));
  CHECK(fresh.orbit_y == Catch::Approx(0.0F));
}

TEST_CASE("Nœud d'effet : le pas qui CYCLE fait flotter au lieu de jaillir",
          "[render][effectnode]") {
  // La pierre de EF_GRAVITATION : elle part d'un pas de UN qui grandit d'un
  // centième, retombe à moins un et deux dixièmes tous les six tics, et repart
  // d'un et dix-huit centièmes trois tics plus tard.
  EffectNode pierre;
  pierre.life_ticks = 250;
  pierre.translates_along_axis = true;
  pierre.step_cycles = true;
  pierre.step_length = 1.0F;
  pierre.step_length_change_per_tick = 0.01F;
  pierre.second_step_length = -1.2F;
  pierre.second_step_length_change = 0.0F;
  // ⚠ LE TIC DE BASCULE EST POSÉ ET NE DOIT RIEN FAIRE : le sous-type 117 ne le
  // lit jamais. S'il jouait, il consommerait la réserve au douzième tic et le
  // cycle ne relancerait plus rien.
  pierre.second_keyframe_tick = 12;

  std::array<float, 7> step{};
  for (std::int32_t tick_index = 1; tick_index <= 7; ++tick_index) {
    pierre.age_ticks = tick_index;
    REQUIRE(AdvanceEffectNode(pierre));
    step[static_cast<std::size_t>(tick_index - 1)] = pierre.step_length;
  }
  // Le premier tic ne relance rien : le pas court sous sa variation.
  CHECK(step[0] == Catch::Approx(1.01F));
  // ⚠ LA PHASE DEUX RETOURNE À LA RÉSERVE, et la variation tombe avec elle : les
  // trois tics suivants gardent le même pas de descente.
  CHECK(step[1] == Catch::Approx(-1.2F));
  CHECK(step[2] == Catch::Approx(-1.2F));
  CHECK(step[3] == Catch::Approx(-1.2F));
  // ⚠ LA PHASE CINQ RELANCE LA MONTÉE, et sa variation repart aussi.
  CHECK(step[4] == Catch::Approx(1.19F));
  CHECK(step[5] == Catch::Approx(1.20F));
  CHECK(step[6] == Catch::Approx(1.21F));

  // ⚠ ET LE BILAN D'UN CYCLE EST PRESQUE NUL : trois tics de descente contre
  // trois de montée. C'est ce qui fait FLOTTER la pierre au lieu de la faire
  // jaillir comme une pointe de terre.
  const float descent = step[1] + step[2] + step[3];
  const float rise = step[4] + step[5] + step[6];
  CHECK(descent + rise == Catch::Approx(0.0F).margin(0.01F));

  // ⚠ ET LA RÉSERVE EST INTACTE, donc le cycle peut la relancer : la seconde
  // image-clé ne l'a pas consommée au douzième tic.
  CHECK(pierre.second_step_length == Catch::Approx(-1.2F));
  CHECK(pierre.second_keyframe_tick == 12);
}

TEST_CASE("Nœud d'effet : sans cycle, la bascule joue UNE fois", "[render][effectnode]") {
  // La pointe de terre : même réserve, mais elle bascule au tic douze et n'y
  // revient jamais.
  EffectNode tip;
  tip.translates_along_axis = true;
  tip.step_length = 1.0F;
  tip.step_length_change_per_tick = 0.0F;
  tip.second_step_length = -1.2F;
  tip.second_step_length_change = 0.0F;
  tip.second_keyframe_tick = 12;

  for (std::int32_t tick_index = 1; tick_index <= 11; ++tick_index) {
    tip.age_ticks = tick_index;
    REQUIRE(AdvanceEffectNode(tip));
  }
  CHECK(tip.step_length == Catch::Approx(1.0F));
  tip.age_ticks = 12;
  REQUIRE(AdvanceEffectNode(tip));
  CHECK(tip.step_length == Catch::Approx(-1.2F));
  // La bascule s'est éteinte : elle ne rejouera pas.
  CHECK(tip.second_keyframe_tick == -1);
}

TEST_CASE("Nœud d'effet : une période d'image fractionnaire suit l'âge, et la dernière tue",
          "[render][effectnode]") {
  // Le coup de la Mandragore : quatre images à une cadence et demie de nos
  // tics. Le client divise l'âge par la période — l'image ne se compte pas,
  // elle se déduit — et meurt quand le quotient atteint le nombre d'images.
  EffectNode node;
  node.life_ticks = 100;
  node.texture_count = 4;
  node.frame_period_ticks = 1.5F;
  node.loops_frames = false;

  const std::array<std::int32_t, 5> expected_frames = {0, 1, 2, 2, 3};
  for (const std::int32_t frame : expected_frames) {
    REQUIRE(AdvanceEffectNode(node));
    CHECK(node.frame == frame);
  }
  // Six tics pour quatre images d'une période et demie : le sixième tue.
  CHECK_FALSE(AdvanceEffectNode(node));

  // En boucle, l'image revient au début et le nœud vit.
  EffectNode looping;
  looping.life_ticks = 100;
  looping.texture_count = 4;
  looping.frame_period_ticks = 1.5F;
  for (std::int32_t tick = 0; tick < 12; ++tick) {
    REQUIRE(AdvanceEffectNode(looping));
    CHECK(looping.frame < 4);
  }
  CHECK(looping.frame == 0);
}

TEST_CASE("Nœud d'effet : sous son bit, le pas de hauteur se retourne à chaque période",
          "[render][effectnode]") {
  // Les murs de Safety Wall : quarante de haut, un quart d'unité par tic, et le
  // signe du pas retourné tous les cinquante tics.
  // Le tic zéro intègre un pas MONTANT puis retourne le signe : le haut
  // descend jusqu'à vingt-huit, remonte à quarante, et ainsi de suite — sans
  // jamais grimper au plafond de cent.
  EffectNode node;
  node.life_ticks = 10000;
  node.half_height.half_size = 40.0F;
  node.half_height.growth_per_tick = 0.25F;
  node.flags = ro::render::kEffectNodeHeightGrowthFlips;
  node.height_flip_period_ticks = 50;

  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.half_height.half_size == Catch::Approx(40.25F));
  CHECK(node.half_height.growth_per_tick == Catch::Approx(-0.25F));
  float highest = node.half_height.half_size;
  float lowest = node.half_height.half_size;
  for (std::int32_t tick = 1; tick <= 300; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
    highest = std::max(highest, node.half_height.half_size);
    lowest = std::min(lowest, node.half_height.half_size);
    if (tick == 50) {
      CHECK(node.half_height.half_size == Catch::Approx(27.75F));
    }
    if (tick == 100) {
      CHECK(node.half_height.half_size == Catch::Approx(40.25F));
    }
  }
  CHECK(highest == Catch::Approx(40.25F));
  CHECK(lowest == Catch::Approx(27.75F));

  // Sans le bit, la période ne fait rien : la hauteur grimpe au plafond.
  EffectNode sans_bit;
  sans_bit.life_ticks = 10000;
  sans_bit.half_height.half_size = 40.0F;
  sans_bit.half_height.growth_per_tick = 0.25F;
  sans_bit.height_flip_period_ticks = 50;
  for (std::int32_t tick = 0; tick < 300; ++tick) {
    REQUIRE(AdvanceEffectNode(sans_bit));
  }
  CHECK(sans_bit.half_height.half_size ==
        Catch::Approx(ro::render::kEffectNodeDefaultHalfHeightCeiling));
}

TEST_CASE("Nœud d'effet : l'échelle d'un sprite avance de son pas", "[render][effectnode]") {
  // La boule de feu de Sight : deux et demi au départ, un dixième de moins par
  // tic (`792 += 796`), un demi à la vingtième image. Sans ce pas, sa traînée
  // gardait la même taille du bout à l'autre.
  EffectNode node;
  node.life_ticks = 100;
  node.sprite_scale = 2.5F;
  node.sprite_scale_step = -0.1F;
  for (std::int32_t tick = 0; tick < 20; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
  }
  CHECK(node.sprite_scale == Catch::Approx(0.5F));

  // Sans pas, elle ne bouge pas : c'est le défaut de tout autre nœud.
  EffectNode fixed;
  fixed.life_ticks = 100;
  fixed.sprite_scale = 0.55F;
  for (std::int32_t tick = 0; tick < 20; ++tick) {
    REQUIRE(AdvanceEffectNode(fixed));
  }
  CHECK(fixed.sprite_scale == Catch::Approx(0.55F));
}

TEST_CASE("toute entree de la table sait rendre ses textures", "[render][effect_node]") {
  // 🔴 LE JEU EST TOMBÉ ICI. `EffectNodePatternTextures` découpe le tableau des
  // textures par `first_texture`/`texture_count`, et `subspan` fait sauter le
  // contrôle de bornes de la bibliothèque standard en Debug — un `__debugbreak`,
  // faute 0x80000003, qui ressemble à tout sauf à une table fausse.
  //
  // L'essai refait le geste pour CHAQUE entrée : si la table compilée porte une
  // borne qui sort, il tombe ici plutôt qu'en jeu.
  std::size_t with_texture = 0;
  for (const ro::render::EffectNodePattern& pattern : ro::render::EffectNodePatternTable()) {
    const std::span<const std::string_view> textures =
        ro::render::EffectNodePatternTextures(pattern);
    CHECK(textures.size() == pattern.texture_count);
    if (!textures.empty()) {
      ++with_texture;
    }
  }
  CHECK(with_texture > 0);
}

TEST_CASE("la recherche par effet reste DANS la table", "[render][effect_node]") {
  // 🔴 `EffectNodePatternsFor` fait un `equal_range`, qui EXIGE une plage triée.
  // Sur une table mal ordonnée il est un comportement indéfini : il peut rendre
  // un couple qui sort du tableau, et le semis marche alors dans la mémoire
  // d'à côté — ce qui ressemble à une borne de texture fausse, très loin de la
  // vraie cause.
  const std::span<const ro::render::EffectNodePattern> table = ro::render::EffectNodePatternTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &ro::render::EffectNodePattern::effect_id));

  // Le geste exact du jeu, pour chaque identifiant qu'il peut demander — ceux
  // de la table, leurs voisins immédiats, et les deux bouts.
  std::size_t trouves = 0;
  for (const ro::render::EffectNodePattern& connu : table) {
    for (const std::uint32_t request :
         {connu.effect_id, connu.effect_id + 1U, connu.effect_id - 1U}) {
      const std::span<const ro::render::EffectNodePattern> rendus =
          ro::render::EffectNodePatternsFor(request);
      // Le sous-ensemble rendu est CONTENU dans la table, et chacun de ses
      // patrons sait rendre ses textures.
      REQUIRE(rendus.size() <= table.size());
      for (const ro::render::EffectNodePattern& pattern : rendus) {
        REQUIRE(&pattern >= table.data());
        REQUIRE(&pattern < table.data() + table.size());
        CHECK(pattern.effect_id == request);
        CHECK(ro::render::EffectNodePatternTextures(pattern).size() == pattern.texture_count);
      }
      trouves += rendus.size();
    }
  }
  CHECK(trouves >= table.size());
  CHECK(ro::render::EffectNodePatternsFor(0xFFFFFFFFU).empty());

  // 🔴 ET TOUS LES IDENTIFIANTS QUE LE JEU PEUT DEMANDER, pas seulement ceux
  // qu'on connaît : le serveur en annonce que la table ignore, et c'est
  // justement sur l'un d'eux que la vue rendue pourrait sortir du tableau.
  for (std::uint32_t request = 0; request <= 3000; ++request) {
    const std::span<const ro::render::EffectNodePattern> rendus =
        ro::render::EffectNodePatternsFor(request);
    if (rendus.empty()) {
      continue;
    }
    REQUIRE(rendus.data() >= table.data());
    REQUIRE(rendus.data() + rendus.size() <= table.data() + table.size());
  }
}

TEST_CASE(
    "Nœud d'effet : la première image se tient, puis l'avance va d'une par tic jusqu'à la "
    "dernière",
    "[render][effectnode]") {
  // La flèche d'un arc : huit mises à jour sur sa première image, puis une
  // image par mise à jour, figée sur la dernière et sans mourir.
  EffectNode node;
  node.life_ticks = 100;
  node.texture_count = 4;
  node.frame_hold_ticks = 1;
  node.loops_frames = false;
  node.holds_last_frame = true;
  node.first_frame_hold_ticks = 8;
  for (int update = 1; update <= 8; ++update) {
    REQUIRE(AdvanceEffectNode(node));
    CHECK(node.frame == 0);
  }
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.frame == 1);
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.frame == 2);
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.frame == 3);
  REQUIRE(AdvanceEffectNode(node));
  CHECK(node.frame == 3);

  // Sans tenue, l'avance commence au premier tic : la munition en sprite d'un
  // monstre change d'image à chaque mise à jour, et boucle.
  EffectNode bullet;
  bullet.life_ticks = 100;
  bullet.texture_count = 3;
  bullet.frame_hold_ticks = 1;
  bullet.loops_frames = true;
  REQUIRE(AdvanceEffectNode(bullet));
  CHECK(bullet.frame == 1);
  REQUIRE(AdvanceEffectNode(bullet));
  CHECK(bullet.frame == 2);
  REQUIRE(AdvanceEffectNode(bullet));
  CHECK(bullet.frame == 0);
}

TEST_CASE("Nœud d'effet : passe sa derniere image, il revient a la premiere et s'y tient",
          "[render][effectnode]") {
  // 🔴 LE MOUVEMENT DE PANG VOICE ET DE WINK CHARM NE LIT PAS CE QUE L'AVANCE
  // D'IMAGE LUI REND, et l'image d'un rang hors de l'action est la première :
  // le sprite reste sur elle jusqu'à sa vie, au lieu de mourir.
  EffectNode node;
  node.life_ticks = 20;
  node.texture_count = 3;
  node.frame_hold_ticks = 1;
  node.loops_frames = false;
  node.rests_on_first_frame = true;
  for (std::int32_t tick = 0; tick < 2; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
  }
  CHECK(node.frame == 2);
  for (std::int32_t tick = 2; tick < 20; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
    CHECK(node.frame == 0);
  }
}
