#include "ro/render/effect_life_table.h"

#include <algorithm>
#include <map>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_table.h"
#include "ro/render/effect_visibility.h"
#include "ro/render/state_effect.h"

using ro::render::EffectLifeEntry;
using ro::render::EffectLifeTable;
using ro::render::EffectNodeLifeFor;
using ro::render::EffectNodeLifeIsKnown;
using ro::render::kEffectLifeForever;
using ro::render::kEffectNodeDefaultLifeTicks;

TEST_CASE("la table des vies est triee et sans doublon", "[render][effect_life]") {
  const std::span<const EffectLifeEntry> table = EffectLifeTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &EffectLifeEntry::effect_id));
  // ⚠ LE TRI SEUL NE SUFFIT PAS : `EffectNodeLifeFor` cherche par dichotomie, et
  // deux entrées de même identifiant lui feraient rendre la première des deux
  // sans que rien ne dise laquelle.
  CHECK(std::ranges::adjacent_find(table, {}, &EffectLifeEntry::effect_id) == table.end());
  for (const EffectLifeEntry& entry : table) {
    CHECK(entry.life_ticks > 0);
  }
}

TEST_CASE("une vie relevee a la main se retrouve dans la table", "[render][effect_life]") {
  // Quatre entrées posées à la main, dont deux INFINIES. Voir ADR 0042 pour
  // le procédé.
  CHECK(EffectNodeLifeFor(297) == kEffectLifeForever);  // EF_POKJUK, les feux d'artifice
  CHECK(EffectNodeLifeFor(381) == kEffectLifeForever);
  CHECK(EffectNodeLifeFor(348) == 1000);
  CHECK(EffectNodeLifeFor(523) == 300);
}

TEST_CASE("ce que le batisseur ecrit dans son instance gagne sur la table",
          "[render][effect_life]") {
  // 🔴 LA TABLE DU CLIENT DIT CINQUANTE POUR EF_HEALSP, ET L'EFFET EN VIT
  // CENT : son bâtisseur écrit cent dans la vie de l'instance à son tic zéro,
  // là même où la durée de la table est entrée, et ses deux derniers anneaux
  // copient cette valeur-là. Borner le semis sur cinquante couperait la
  // colonne à mi-course et tairait la moitié des étincelles.
  CHECK(EffectNodeLifeIsKnown(14));
  CHECK(EffectNodeLifeFor(14) == 100);
  // Le soin écrit la même chose, et n'est pas dans la table — son bâtisseur
  // POSE la vie de ses nœuds — : sans cela ses étincelles n'auraient aucune
  // borne, et la dernière survivrait à sa colonne.
  CHECK(EffectNodeLifeIsKnown(7));
  CHECK(EffectNodeLifeFor(7) == 100);
  // ⚠ UN PLANCHER SE LIT COMME UN PLANCHER. Le bâtisseur des anneaux à quatre
  // pistes relève la vie à cinquante-six « si elle est plus courte » :
  // quarante devient cinquante-six, et mille reste mille.
  CHECK(EffectNodeLifeFor(12) == 56);
  CHECK(EffectNodeLifeFor(342) == 1000);
  // Le bâtisseur des anneaux en couches relève à soixante-dix : la
  // table dit cent pour 172, et cent tient.
  CHECK(EffectNodeLifeFor(172) == 100);
  // 🔴 UNE DURÉE ÉGALE À SA CLÉ EST UNE DURÉE : la table du client écrit
  // 1286 pour 1286, et le plancher de cent vingt la laisse telle quelle.
  CHECK(EffectNodeLifeIsKnown(1286));
  CHECK(EffectNodeLifeFor(1286) == 1286);
}

TEST_CASE("une instance laissee a la table borne aussi les noeuds qui ne la copient pas",
          "[render][effect_life]") {
  // 🔴 L'EFFET DE BD_ADAPTATION POSE CINQUANTE TICS DANS CHAQUE NŒUD et en sème
  // un tous les deux tics, pendant les cent que vit son instance. Il ne touche
  // pas à cette vie, qui reste la durée de la table du client. Elle borne
  // pourtant ses nœuds : sans elle, la moitié tardive de l'effet lui survivait
  // d'une demi-vie.
  constexpr std::uint32_t kAdaptationEffect = 848;
  constexpr std::int32_t kAdaptationInstanceLife = 100;
  constexpr std::int32_t kAdaptationNodeLife = 50;
  CHECK(ro::render::EffectInstanceLifeIsKnown(kAdaptationEffect));
  CHECK(ro::render::EffectInstanceLifeFor(kAdaptationEffect) == kAdaptationInstanceLife);
  // ⚠ ELLE BORNE, ELLE NE SE DONNE PAS : la vie que reçoit un nœud qui copie
  // reste celle du constructeur pour cet effet, que la table des vies ignore.
  CHECK_FALSE(EffectNodeLifeIsKnown(kAdaptationEffect));
  CHECK(EffectNodeLifeFor(kAdaptationEffect) == kEffectNodeDefaultLifeTicks);
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(kAdaptationEffect);
  REQUIRE(patterns.size() == 1);
  const ro::render::EffectNodePattern& plans = patterns.front();
  REQUIRE_FALSE(plans.life_from_effect);
  REQUIRE(plans.life_ticks == kAdaptationNodeLife);

  // CE QUE LA TROUPE DONNE AU NŒUD SEMÉ AU PASSAGE `sown` : une mise à jour par
  // passage tant que l'instance vit, avec ce qui lui reste, son âge déjà crû.
  const auto highest_alpha = [&plans](std::int32_t sown, std::int32_t instance_life) {
    ro::render::EffectNode node = ro::render::EffectNodeFromPattern(plans);
    node.fades_with_its_instance = true;
    float highest = node.alpha;
    for (std::int32_t passage = sown; passage < instance_life - 1; ++passage) {
      node.instance_ticks_left = instance_life - (passage + 1);
      if (!ro::render::AdvanceEffectNode(node)) {
        break;
      }
      highest = std::max(highest, node.alpha);
    }
    return highest;
  };

  // Semé au tic cinquante, il s'éteint avec l'instance sans qu'elle le borne :
  // sa première mise à jour lit 49 tics à l'instance, et il n'en a que 50.
  constexpr std::int32_t kLastBoundlessSowing = 50;
  CHECK(highest_alpha(kLastBoundlessSowing, kAdaptationInstanceLife) > 0.0F);

  // 🔴 SEMÉ DEUX TICS PLUS TARD, il dépasserait l'instance. Son pas d'alpha
  // devient alors moins l'alpha divisé par ce qu'il reste à l'instance, et cet
  // alpha est NUL à la naissance : le nœud reste invisible, et meurt avec elle.
  constexpr std::int32_t kFirstBoundSowing = 52;
  CHECK(highest_alpha(kFirstBoundSowing, kAdaptationInstanceLife) == 0.0F);
}

TEST_CASE("les instances qui ne font que borner forment une table a part",
          "[render][effect_life]") {
  const std::span<const EffectLifeEntry> bounding = ro::render::EffectInstanceOnlyLifeTable();
  REQUIRE_FALSE(bounding.empty());
  CHECK(std::ranges::is_sorted(bounding, {}, &EffectLifeEntry::effect_id));
  CHECK(std::ranges::adjacent_find(bounding, {}, &EffectLifeEntry::effect_id) == bounding.end());
  // ⚠ AUCUNE N'EST DANS LA TABLE DES VIES : une instance y est soit donnée, soit
  // seulement bornante, jamais les deux.
  for (const EffectLifeEntry& entry : bounding) {
    INFO("effet " << entry.effect_id);
    CHECK_FALSE(EffectNodeLifeIsKnown(entry.effect_id));
    CHECK(ro::render::EffectInstanceLifeIsKnown(entry.effect_id));
    CHECK(ro::render::EffectInstanceLifeFor(entry.effect_id) == entry.life_ticks);
    CHECK(entry.life_ticks >= 0);
  }
  // L'effet de l'option Ruwach se resème tous les deux cents tics, et c'est sa
  // vie d'instance.
  constexpr std::uint32_t kRuwachEffect = 33;
  constexpr std::int32_t kRuwachInstanceLife = 200;
  CHECK(ro::render::EffectInstanceLifeFor(kRuwachEffect) == kRuwachInstanceLife);
  // 🔴 ET UNE DURÉE NULLE EST UNE MORT IMMÉDIATE, pas une absence de borne :
  // l'instance d'EF_DARKATTACK meurt à sa première mise à jour, ses nœuds avec
  // elle, avant tout dessin.
  constexpr std::uint32_t kDarkAttackEffect = 184;
  CHECK(ro::render::EffectInstanceLifeIsKnown(kDarkAttackEffect));
  CHECK(ro::render::EffectInstanceLifeFor(kDarkAttackEffect) == 0);
  // Un identifiant que ni l'une ni l'autre ne porte n'a pas de borne.
  CHECK_FALSE(ro::render::EffectInstanceLifeIsKnown(0));
}

TEST_CASE("un sprite qui copie la vie de son effet la recoit au semis", "[render][effect_life]") {
  // Le feu de Demonstration (302) recopie la durée de son instance dans son
  // nœud : il vit autant que l'unité. Le semis des sprites prenait le défaut
  // du constructeur, cent tics, et l'éteignait en une seconde et demie.
  constexpr std::uint32_t kDemonstrationFire = 302;
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(kDemonstrationFire);
  REQUIRE_FALSE(patterns.empty());
  REQUIRE(patterns.front().life_from_effect);
  const ro::render::EffectNodeSeed seed =
      ro::render::EffectNodeSeedFromPattern(patterns.front(), 0, 0, 1);
  CHECK(seed.life_ticks == EffectNodeLifeFor(kDemonstrationFire));
  CHECK(seed.life_ticks > kEffectNodeDefaultLifeTicks);
}

TEST_CASE("un identifiant hors table rend le defaut du CONSTRUCTEUR", "[render][effect_life]") {
  // ⚠ CE N'EST PAS UN REPLI, C'EST LA BONNE RÉPONSE. Un bâtisseur qui n'écrit
  // rien dans la vie de son nœud laisse celle du constructeur, et le client fait
  // de même. Rendre la durée de l'effet à tout le monde changerait la vie de
  // trois cent soixante-six identifiants que le client laisse au défaut.
  const std::span<const EffectLifeEntry> table = EffectLifeTable();
  std::uint32_t absent = 0;
  while (std::ranges::find(table, absent, &EffectLifeEntry::effect_id) != table.end()) {
    ++absent;
  }
  CHECK(EffectNodeLifeFor(absent) == kEffectNodeDefaultLifeTicks);

  // ⚠ ET LES IDENTIFIANTS A DEUX ESPECES DE BATISSEUR EN FONT PARTIE : la table
  // les écarte plutôt que de choisir pour eux. Voir effect_life_table.h.
  //
  // 🔴 ILS SONT TROIS. Un bâtisseur qui recopie la durée par une variable
  // TEMPORAIRE y écrit bel et bien : 618 et 718 ont deux bâtisseurs qui
  // s'accordent, et la table peut répondre pour eux.
  for (const std::uint32_t mixte : {248U, 669U, 786U}) {
    CHECK(EffectNodeLifeFor(mixte) == kEffectNodeDefaultLifeTicks);
  }
}

TEST_CASE("une AURA d'etat se reconnait a sa duree infinie", "[render][effect_life]") {
  // 🔴 C'EST CE QUI SÉPARE UNE AURA D'UN ÉCLAT, et nous ne le savions pas : nos
  // effets d'état se jouaient UNE FOIS — deux secondes de `.str` pour un Steel
  // Body qui dure une minute. Le client, lui, déclare la durée de chaque effet,
  // et `kEffectLifeForever` y est son écriture de l'infini.
  //
  // La liste se lit d'elle-même. EFST_STEELBODY pose deux effets sans fin ;
  // EFST_TWOHANDQUICKEN pose le 166, qui dure 9 999 et s'éteint seul.
  const std::span<const ro::render::StateEffectEntry> body = ro::render::StateEffectsFor(87);
  REQUIRE(body.size() == 2);
  for (const ro::render::StateEffectEntry& entry : body) {
    INFO("effet " << entry.effect_id);
    CHECK(entry.endless);
  }
  const std::span<const ro::render::StateEffectEntry> speed = ro::render::StateEffectsFor(2);
  REQUIRE(speed.size() == 1);
  CHECK_FALSE(speed[0].endless);
}

TEST_CASE("les deux tables de vie s'accordent sur l'infini", "[render][effect_life]") {
  // 🔴 DEUX RELEVÉS INDÉPENDANTS, UNE SEULE RÉPONSE. Celui des NŒUDS lit ce que
  // le bâtisseur écrit dans son instance ; celui des ÉTATS lit la table de
  // durées du client. Là où les deux parlent du même effet, ils doivent dire la
  // même chose — et un désaccord désignerait celui des deux qui a dérivé.
  std::size_t croises = 0;
  for (const std::span<const ro::render::StateEffectEntry> table :
       {ro::render::StateEffectTable(), ro::render::StatePacketEffectTable(),
        ro::render::StateRebuildEffectTable()}) {
    for (const ro::render::StateEffectEntry& entry : table) {
      if (!EffectNodeLifeIsKnown(entry.effect_id)) {
        continue;
      }
      ++croises;
      INFO("effet " << entry.effect_id);
      CHECK(entry.endless == (EffectNodeLifeFor(entry.effect_id) == kEffectLifeForever));
    }
  }
  // ⚠ ET LE CROISEMENT DOIT AVOIR LIEU. Un accord sur l'ensemble vide passerait
  // sans rien garder.
  CHECK(croises >= 20);
}

TEST_CASE("sans fin est une propriete de l'EFFET, pas du couple", "[render][effect_life]") {
  // Le même effet servi par deux états ne peut pas être une aura pour l'un et un
  // éclat pour l'autre : ce serait le signe que le drapeau vient d'ailleurs que
  // de la table des durées.
  std::map<std::uint16_t, bool> vu;
  std::size_t partages = 0;
  for (const std::span<const ro::render::StateEffectEntry> table :
       {ro::render::StateEffectTable(), ro::render::StatePacketEffectTable(),
        ro::render::StateRebuildEffectTable(), ro::render::StateRemovalTable()}) {
    for (const ro::render::StateEffectEntry& entry : table) {
      const auto [place, fresh] = vu.emplace(entry.effect_id, entry.endless);
      if (!fresh) {
        ++partages;
        INFO("effet " << entry.effect_id);
        CHECK(place->second == entry.endless);
      }
    }
  }
  CHECK(partages >= 50);
}

TEST_CASE("deux etats calculent leur effet sur leur VALEUR", "[render][effect_life]") {
  // 🔴 AUCUNE TABLE DE COUPLES NE PEUT LES PORTER. Rolling Cutter montre un
  // anneau par tour et la malédiction du Devil assombrit par paliers : c'est le
  // `val1` du paquet d'état qui décide, et notre `Actor` le jetait.
  const std::span<const ro::render::StateValueEffectEntry> table =
      ro::render::StateValueEffectTable();
  REQUIRE(table.size() == 2);
  CHECK(std::ranges::is_sorted(table, {}, &ro::render::StateValueEffectEntry::state_id));
  for (const ro::render::StateValueEffectEntry& entry : table) {
    INFO("état " << entry.state_id);
    CHECK(entry.min_value <= entry.max_value);
    CHECK(entry.offset > 0);
    // Aucun rang ne doit tomber hors du domaine des effets du client.
    CHECK(entry.offset + entry.max_value < 2500);
  }

  // EFST_ROLLINGCUTTER : `valeur + 1299`, de un à dix tours, sur n'importe qui.
  // ⚠ 1299 ET NON 1466 : le client prend la seconde famille quand la bascule
  // des « effets minimaux » est levée, et nous dessinons les effets pleins.
  const ro::render::StateValueEffectEntry* tours = ro::render::StateValueEffectFor(339);
  REQUIRE(tours != nullptr);
  CHECK(tours->offset == 1299);
  CHECK(tours->min_value == 1);
  CHECK(tours->max_value == 10);
  CHECK_FALSE(tours->own_player_only);
  CHECK_FALSE(tours->endless);

  // EFST_DEVIL1 : `valeur + 473`, de deux à onze, SUR SOI SEULEMENT, et toute
  // la famille vit jusqu'au retrait.
  const ro::render::StateValueEffectEntry* devil = ro::render::StateValueEffectFor(152);
  REQUIRE(devil != nullptr);
  CHECK(devil->offset == 473);
  CHECK(devil->min_value == 2);
  CHECK(devil->max_value == 11);
  CHECK(devil->own_player_only);
  CHECK(devil->endless);

  CHECK(ro::render::StateValueEffectFor(0) == nullptr);
}

TEST_CASE("la liste blanche de /effect est celle du client", "[render][effet_bascule]") {
  // ⚠ ELLE VIT DANS LES DONNÉES, PAS DANS LE CODE. Le portier du client ne
  // teste qu'un octet de l'instance ; c'est `forcerendereffect.lub` qui le
  // pose.
  const std::span<const std::uint32_t> blanche = ro::render::ForcedVisibleEffects();
  CHECK(blanche.size() == 114);
  CHECK(std::ranges::is_sorted(blanche));
  CHECK(std::ranges::adjacent_find(blanche) == blanche.end());

  // Les deux bouts, tels que le fichier du client les porte.
  CHECK(ro::render::IsForcedVisibleEffect(35));
  CHECK(ro::render::IsForcedVisibleEffect(2402));
  // Et un voisin immédiat qui n'y est pas : la recherche cherche, elle ne borne
  // pas.
  CHECK_FALSE(ro::render::IsForcedVisibleEffect(34));
  CHECK_FALSE(ro::render::IsForcedVisibleEffect(2403));
  CHECK_FALSE(ro::render::IsForcedVisibleEffect(0));
}

TEST_CASE("le crible de /effect touche les DEUX familles", "[render][effet_bascule]") {
  // 🔴 LA LISTE BLANCHE N'EST PAS FAITE QUE D'EFFETS EN FICHIER ; elle est à
  // peu près par moitiés. Cribler une seule
  // famille couperait vingt-cinq effets que le client garde exprès — et le
  // défaut ne se verrait qu'en côte à côte, sur les repères de jeu.
  std::size_t in_file = 0;
  std::size_t procedurale = 0;
  for (const std::uint32_t effect_id : ro::render::ForcedVisibleEffects()) {
    if (!ro::render::EffectFile(effect_id).empty()) {
      ++in_file;
    }
    if (ro::render::KnowsProceduralEffect(effect_id)) {
      ++procedurale;
    }
  }
  CHECK(in_file >= 20);
  CHECK(procedurale >= 20);
}

TEST_CASE("/mineffect a une famille jumelle a prendre", "[render][effet_bascule]") {
  // La bascule 229 du client ne préfixe PAS un chemin : elle choisit un AUTRE
  // identifiant. Rolling Cutter en donne le couple.
  const ro::render::StateValueEffectEntry* tours = ro::render::StateValueEffectFor(339);
  REQUIRE(tours != nullptr);
  CHECK(tours->offset == 1299);
  CHECK(tours->minimal_offset == 1466);

  // ⚠ LES DEUX FAMILLES NE SE CHEVAUCHENT PAS, et c'est ce qui permet de balayer
  // les deux avant de poser un rang : si elles se recouvraient, effacer l'une
  // éteindrait un anneau de l'autre qu'on vient de poser.
  CHECK(tours->offset + tours->max_value < tours->minimal_offset + tours->min_value);

  // EFST_DEVIL1 n'a PAS de jumelle : le zéro dit « pas de variante », et non
  // « variante à l'identifiant zéro ».
  const ro::render::StateValueEffectEntry* devil = ro::render::StateValueEffectFor(152);
  REQUIRE(devil != nullptr);
  CHECK(devil->minimal_offset == 0);
}

TEST_CASE("Mise à jour : un nœud qui survivrait à son instance s'éteint avec elle, en fondu",
          "[render][effectnode]") {
  // Dès que ce qui reste au nœud dépasse ce qui reste à l'instance plus un,
  // l'alpha descend jusqu'à zéro sur ce reste, et le va-et-vient s'arrête. Il
  // ne disparaît pas net.
  ro::render::EffectNodeSeed seed;
  seed.life_ticks = 150;
  seed.alpha_max = 254.0F;
  seed.alpha_start = 200.0F;
  seed.flags = ro::render::kEffectNodeAlphaBounces;
  std::uint32_t generator = 1;
  ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
  REQUIRE(node.fades_with_its_instance);
  CHECK(node.owner_fade_at_age == ro::render::kEffectNodeOwnerFadeUnjudged);
  // Ce que la troupe lui donne : un passage de moins à chaque mise à jour.
  constexpr std::int32_t kSeenAtFirstUpdate = 20;
  const auto advance = [&node](std::int32_t update) {
    node.instance_ticks_left = kSeenAtFirstUpdate - update;
    return ro::render::AdvanceEffectNode(node);
  };
  REQUIRE(advance(0));
  // 🔴 LE PAS CHANGE, LA VIE NON : le client n'écrit que le pas. C'est la mort de
  // l'instance, dans la troupe, qui emporte le nœud.
  CHECK(node.life_ticks == 150);
  CHECK(node.owner_fade_at_age == 0);
  CHECK(node.owner_fade_over_ticks == kSeenAtFirstUpdate);
  CHECK((node.flags & ro::render::kEffectNodeAlphaBounces) == 0U);
  for (std::int32_t update = 1; update < 10; ++update) {
    REQUIRE(advance(update));
  }
  CHECK(node.alpha == Catch::Approx(100.0F));
  // ⚠ IL NE S'ENGAGE QU'UNE FOIS : un reste plus court ne refait pas le pas.
  CHECK(node.owner_fade_over_ticks == kSeenAtFirstUpdate);

  // Un tic de marge : le client ne compte « qui survit » qu'au-delà de un.
  ro::render::EffectNode barely = ro::render::SowEffectNode(seed, generator);
  barely.instance_ticks_left = 149;
  REQUIRE(ro::render::AdvanceEffectNode(barely));
  CHECK(barely.owner_fade_at_age == ro::render::kEffectNodeOwnerFadeUnjudged);
  // Et un reste inconnu — zéro — ne change rien.
  ro::render::EffectNode unknown = ro::render::SowEffectNode(seed, generator);
  REQUIRE(ro::render::AdvanceEffectNode(unknown));
  CHECK(unknown.owner_fade_at_age == ro::render::kEffectNodeOwnerFadeUnjudged);
}

TEST_CASE("Mise à jour : le fondu de l'instance peut s'engager après la naissance",
          "[render][effectnode]") {
  // 🔴 JUGÉ À CHAQUE PASSAGE, ET NON AU SEMIS : une instance qui vieillit plus
  // vite que son nœud — elle bat à chaque image, lui au tic — finit par lui
  // laisser moins qu'il ne lui reste. Le fondu s'engage alors, sur ce reste.
  ro::render::EffectNodeSeed seed;
  seed.life_ticks = 50;
  seed.alpha_start = 200.0F;
  std::uint32_t generator = 1;
  ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
  node.instance_ticks_left = 49;
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.owner_fade_at_age == ro::render::kEffectNodeOwnerFadeUnjudged);
  // Une image sans tic a fait vieillir l'instance, pas le nœud.
  node.instance_ticks_left = 47;
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.owner_fade_at_age == 1);
  CHECK(node.owner_fade_over_ticks == 47);
  CHECK(node.alpha == Catch::Approx(200.0F - 200.0F / 47.0F));
}

TEST_CASE("Semis : la sortie propre du nœud reprend la main après le fondu de l'instance",
          "[render][effectnode]") {
  // 🔴 LE CLIENT RECALCULE LE PAS À SA PROPRE SORTIE, sur sa propre vie, même
  // quand l'instance a déjà imposé le sien. Un nœud de cent cinquante tics qui
  // sort au cent-dixième, dans une instance qui en a encore cent vingt : le
  // fondu commence à la naissance, sur cent vingt tics, puis se refait au
  // cent-dixième sur les quarante qui restent au nœud.
  ro::render::EffectNodeSeed seed;
  seed.life_ticks = 150;
  seed.fade_out_ticks = 40;
  seed.alpha_max = 254.0F;
  seed.alpha_start = 240.0F;
  std::uint32_t generator = 1;
  ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
  REQUIRE(node.fade_out_tick == 110);
  constexpr std::int32_t kSeenAtFirstUpdate = 120;
  for (std::int32_t tick = 0; tick < 110; ++tick) {
    node.instance_ticks_left = kSeenAtFirstUpdate - tick;
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  const float at_own_fade_out = node.alpha;
  CHECK(at_own_fade_out == Catch::Approx(240.0F - 240.0F * 110.0F / 120.0F).margin(1.0F));
  node.instance_ticks_left = kSeenAtFirstUpdate - 110;
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.alpha_step_per_tick == Catch::Approx(-at_own_fade_out / 40.0F));
}

TEST_CASE("EF_SPHERE : son instance se recrit une vie a chaque semis", "[render][effect_life]") {
  // 🔴 SON BÂTISSEUR ÉCRIT `500 − ÂGE` À CHAQUE ÂGE MULTIPLE DE DIX : l'instance
  // meurt au passage 250, où son âge, une fois crû, rejoint la vie écrite — et
  // non aux cinq cents de la table du client. Sa vie fixe équivalente est donc
  // 251. Ses nœuds pâlissent au premier semis où leur reste dépasse celui de
  // l'instance plus un.
  constexpr std::uint32_t kSphereEffect = 72;
  constexpr std::int32_t kSphereDeathPassage = 250;
  CHECK(ro::render::EffectInstanceLifeFor(kSphereEffect) == kSphereDeathPassage + 1);
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(kSphereEffect);
  REQUIRE(patterns.size() == 1);
  const ro::render::EffectNodePattern& sphere = patterns.front();
  CHECK(sphere.supplement.instance_life_rewrite_base == 500);
  CHECK(sphere.supplement.instance_life_rewrite_period_ticks == 10);
  // ⚠ IL SÈME ENCORE AU PASSAGE DE SA MORT, et ce nœud-là meurt avant sa
  // première mise à jour.
  CHECK(sphere.last_tick == kSphereDeathPassage);

  const auto fade_of = [&sphere](std::int32_t sown) {
    ro::render::EffectNode node = ro::render::EffectNodeFromPattern(sphere);
    const ro::render::EffectNodeSeed seed = ro::render::EffectNodeSeedFromPattern(
        sphere, ro::render::EffectNodePatternTicksLeft(sphere, sown), sown, 0);
    ro::render::EffectNodeFadesWithARewritingInstance(node, seed.instance_rewrite);
    return node;
  };
  // Semé au tic zéro, il garde son pas jusqu'au semis du tic 210 : la vie écrite
  // y vaut 290, l'instance a déjà l'âge 211, et il reste 90 tics au nœud contre
  // 79 à l'instance.
  const ro::render::EffectNode first = fade_of(0);
  CHECK(first.owner_fade_at_age == 210);
  CHECK(first.owner_fade_over_ticks == 79);
  // Semé au tic 110, il pâlit dès sa naissance sur les 279 tics que la vie
  // écrite promet à l'instance — qui mourra pourtant au passage 250.
  const ro::render::EffectNode late = fade_of(110);
  CHECK(late.owner_fade_at_age == 0);
  CHECK(late.owner_fade_over_ticks == 279);
  // Semé au tic 100, il attend le semis suivant.
  const ro::render::EffectNode middle = fade_of(100);
  CHECK(middle.owner_fade_at_age == 10);
  CHECK(middle.owner_fade_over_ticks == 279);
  // ⚠ ET LA TROUPE NE REJUGE JAMAIS sur la vie de la table, que l'instance ne
  // garde pas : le nœud du dernier semis, qu'aucune mise à jour n'atteint, reste
  // jugé une fois pour toutes.
  CHECK(fade_of(kSphereDeathPassage).owner_fade_at_age == ro::render::kEffectNodeOwnerFadeNever);
}

TEST_CASE("Incantation : l'anneau vit la duree de l'incantation, a son plancher",
          "[render][effect_life]") {
  // 🔴 LE CLIENT POSE LA DURÉE DE L'INCANTATION SUR L'INSTANCE, puis son
  // bâtisseur la relève à son plancher et la recopie dans ses nœuds : 56 pour
  // les anneaux à quatre pistes, 70 pour les anneaux en couches.
  constexpr std::uint32_t kSpellRing = 12;
  constexpr std::uint32_t kGestureRing = 58;
  constexpr std::uint32_t kFireLayers = 54;
  constexpr std::uint32_t kBashBurst = 16;
  CHECK(ro::render::EffectLifeUnderCast(kGestureRing, 2000) == 125);
  CHECK(ro::render::EffectLifeUnderCast(kSpellRing, 500) == 56);
  CHECK(ro::render::EffectLifeUnderCast(kFireLayers, 500) == 70);
  CHECK(ro::render::EffectLifeUnderCast(kFireLayers, 3200) == 200);
  // Un bâtisseur qui ne recopie pas la vie de son instance garde sa table.
  CHECK(ro::render::EffectLifeUnderCast(kBashBurst, 2000) == 0);
  // ⚠ ET `/mineffect` FAIT SORTIR BASH ENTIER, ni gerbe ni bruit.
  CHECK(ro::render::EffectVanishesWithMinimalEffects(kBashBurst));
  CHECK_FALSE(ro::render::EffectVanishesWithMinimalEffects(kGestureRing));
}
