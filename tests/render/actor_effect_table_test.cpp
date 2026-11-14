#include "ro/render/actor_effect_table.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_scale.h"

// CE QU'UN EFFET CHANGE SUR L'ACTEUR : le huitième chemin, celui des effets qui
// ne dessinent rien.
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
using ro::render::TintAfterEffects;
using ro::render::TintAfterEffectsAt;
using ro::render::TintsActor;
using ro::render::ValueAt;

TEST_CASE("Effets d'acteur : la table est triée et sans doublon", "[render][actoreffect]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable, et un doublon rendrait l'un des deux inatteignable.
  const std::span<const ActorEffectChange> table = ActorEffectChangeTable();
  REQUIRE(table.size() == 137);
  for (std::size_t index = 1; index < table.size(); ++index) {
    CHECK(table[index - 1].effect_id < table[index].effect_id);
  }
  // Chacun doit changer QUELQUE CHOSE : une entrée qui ne pose ni bit, ni
  // teinte, ni écriture, ni ordre ne serait pas un effet relevé mais un trou.
  //
  // ⚠ IL Y EN AVAIT UN, ET IL EST COMBLÉ. `EF_MAGICCRASHER2` (403) teignait
  // bien l'acteur — le relevé voyait l'appel — mais le client CALCULE ses trois
  // composantes, et la table disait « inchangée » plutôt que d'inventer une
  // couleur. Ce qu'il calcule a été lu depuis : un tirage au sort. La table le
  // dit maintenant, et plus aucune entrée n'est vide.
  for (const ActorEffectChange& change : table) {
    const bool ecrit = change.writes[0].field != 0 || change.writes[1].field != 0;
    const bool is_empty = change.render_bits == 0 && !TintsActor(change) && !ecrit &&
                          change.command == 0 && change.client_function.empty();
    CHECK_FALSE(is_empty);
  }
  CHECK(ActorEffectChangeFor(403)->family == ActorEffectFamily::kTint);
}

TEST_CASE("Effets d'acteur : le NOM de l'effet prédit sa couleur", "[render][actoreffect]") {
  // ⚠ C'EST LE SEUL CONTRÔLE DONT LA SOURCE SOIT INDÉPENDANTE DE L'OUTIL. Les
  // noms viennent de l'énumération du serveur, les couleurs du binaire du
  // client, et le relevé ne regarde jamais les noms. La première version de
  // l'extracteur renversait les arguments de la teinte — l'alpha pris pour le
  // bleu — et EF_GREENBODY y devenait un corps noir.
  //
  // L'ordre est celui des arguments du client : alpha, rouge, vert, bleu.
  const ActorEffectChange* vert = ActorEffectChangeFor(538);  // EF_GREENBODY
  REQUIRE(vert != nullptr);
  CHECK(vert->tint == std::array<std::int16_t, 4>{kTintUnchanged, 0, 255, 0});

  const ActorEffectChange* violet = ActorEffectChangeFor(768);  // EF_PURPLEBODY
  REQUIRE(violet != nullptr);
  CHECK(violet->tint == std::array<std::int16_t, 4>{kTintUnchanged, 255, 0, 255});

  const ActorEffectChange* white = ActorEffectChangeFor(1065);  // EF_WHITEBODY
  REQUIRE(white != nullptr);
  CHECK(white->tint == std::array<std::int16_t, 4>{kTintUnchanged, 255, 255, 255});

  const ActorEffectChange* grey = ActorEffectChangeFor(963);  // EF_GRAYBODY
  REQUIRE(grey != nullptr);
  CHECK(grey->tint == std::array<std::int16_t, 4>{kTintUnchanged, 105, 105, 105});

  // EF_CLOAKING ne change QUE l'alpha. C'est lui qui prouve l'ordre : renversé,
  // il deviendrait un corps noir au lieu d'un corps transparent.
  const ActorEffectChange* cloaked = ActorEffectChangeFor(120);  // EF_CLOAKING
  REQUIRE(cloaked != nullptr);
  CHECK(cloaked->tint ==
        std::array<std::int16_t, 4>{50, kTintUnchanged, kTintUnchanged, kTintUnchanged});
}

TEST_CASE("Effets d'acteur : les tailles de corps disent leur bit et leur durée",
          "[render][actoreffect]") {
  // Ces quatre-là étaient déjà portés, par un autre chemin : `net` en déduisait
  // une BodySize. La table les rend au même endroit que les quatre-vingt-deux
  // autres, et le relevé les recoupe.
  const ActorEffectChange* bebe = ActorEffectChangeFor(420);  // EF_BABYBODY
  REQUIRE(bebe != nullptr);
  CHECK(bebe->family == ActorEffectFamily::kRenderFlag);
  CHECK(bebe->render_bits == 0x10);
  // `cmp tic, 2Dh ; jg` : le client écrit encore au quarante-cinquième tic, et
  // sort au quarante-sixième.
  CHECK(bebe->life_ticks == 46);
  // Le paramètre est l'HORLOGE de l'effet : c'est elle qui fait la rampe.
  CHECK(bebe->writes[0].field == 0xF0);
  CHECK(bebe->writes[0].from_clock);

  // Sa variante « répétée » pose le même bit, mais sa valeur est FIXE : le
  // corps y est déjà à sa taille, il n'a plus à y aller.
  const ActorEffectChange* baby_body2_change = ActorEffectChangeFor(421);  // EF_BABYBODY2
  REQUIRE(baby_body2_change != nullptr);
  CHECK(baby_body2_change->render_bits == 0x10);
  CHECK(baby_body2_change->writes[0].field == 0xF0);
  CHECK_FALSE(baby_body2_change->writes[0].from_clock);
  CHECK(baby_body2_change->writes[0].value == 45);

  const ActorEffectChange* geant = ActorEffectChangeFor(422);  // EF_GIANTBODY
  REQUIRE(geant != nullptr);
  CHECK(geant->render_bits == 0x400);
  CHECK(geant->writes[0].field == 0xF1);
}

TEST_CASE("Effets d'acteur : le « _DEL » relève le même bit, et fait redescendre le paramètre",
          "[render][actoreffect]") {
  // EF_UNDEADBODY et son « _DEL » posent le MÊME bit : le second ne l'éteint
  // pas, il ramène l'alpha de la copie fantôme vers zéro en quarante tics, puis
  // cesse de le relever.
  const ActorEffectChange* pose = ActorEffectChangeFor(655);    // EF_UNDEADBODY
  const ActorEffectChange* retire = ActorEffectChangeFor(656);  // EF_UNDEADBODY_DEL
  REQUIRE(pose != nullptr);
  REQUIRE(retire != nullptr);
  CHECK(pose->render_bits == 0x80000);
  CHECK(retire->render_bits == pose->render_bits);
  CHECK(pose->writes[0].field == 0xF2);
  CHECK(retire->writes[0].field == 0xF2);
  CHECK(retire->life_ticks == 40);
}

TEST_CASE("Effets d'acteur : la secousse d'écran est un ORDRE", "[render][actoreffect]") {
  // Elle ne touche ni au sprite ni au mot de rendu : elle passe une commande au
  // mode de jeu, et c'est `app` qui la route vers la caméra.
  const ActorEffectChange* secousse = ActorEffectChangeFor(563);  // EF_SCREEN_QUAKE
  REQUIRE(secousse != nullptr);
  CHECK(secousse->family == ActorEffectFamily::kOrder);
  CHECK(secousse->command == 0x7A);
  CHECK(secousse->render_bits == 0);
}

TEST_CASE("Effets d'acteur : un effet ordinaire n'y est pas", "[render][actoreffect]") {
  // La table ne porte QUE le huitième chemin. Un effet qui dessine n'y est pas,
  // et y répondre le ferait traiter deux fois.
  CHECK(ActorEffectChangeFor(89) == nullptr);  // Storm Gust dessine
  CHECK(ActorEffectChangeFor(0) == nullptr);
  CHECK(ActorEffectChangeFor(100000) == nullptr);
}

TEST_CASE("Effets d'acteur : la teinte se compose, et la dernière gagne", "[render][actoreffect]") {
  // C'est ici que dix-sept identifiants deviennent visibles pour une seule
  // pièce de code : la couleur est une DONNÉE de la table, la composition est
  // la même pour tous.
  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};

  // Sans effet, rien ne change.
  CHECK(TintAfterEffects(kWhite, {}) == kWhite);

  // Un effet qui ne teint pas non plus : EF_REFLECTBODY ne pose qu'un bit.
  const std::array<std::uint32_t, 1> reflet = {419};
  CHECK(TintAfterEffects(kWhite, reflet) == kWhite);

  // Une teinte pleine, dans l'ordre rouge, vert, bleu, alpha.
  const std::array<std::uint32_t, 1> vert = {538};  // EF_GREENBODY
  CHECK(TintAfterEffects(kWhite, vert) == std::array<std::uint8_t, 4>{0, 255, 0, 255});

  // ⚠ EF_CLOAKING NE TOUCHE QUE L'ALPHA, et c'est lui qui juge la composition :
  // une composante « inchangée » doit garder celle de la base. Prise pour un
  // zéro, elle rendrait un corps NOIR là où le client rend un corps
  // transparent.
  const std::array<std::uint32_t, 1> cloaked = {120};  // EF_CLOAKING
  CHECK(TintAfterEffects(kWhite, cloaked) == std::array<std::uint8_t, 4>{255, 255, 255, 50});

  // LE DERNIER GAGNE, composante par composante : le camouflage garde son alpha
  // par-dessus le vert, parce que le vert ne dit rien de l'alpha.
  const std::array<std::uint32_t, 2> both = {538, 120};
  CHECK(TintAfterEffects(kWhite, both) == std::array<std::uint8_t, 4>{0, 255, 0, 50});

  // Et dans l'autre sens, le vert recouvre les couleurs du camouflage — qui
  // n'en pose aucune — sans lui reprendre son alpha.
  const std::array<std::uint32_t, 2> a_l_envers = {120, 538};
  CHECK(TintAfterEffects(kWhite, a_l_envers) == std::array<std::uint8_t, 4>{0, 255, 0, 50});

  // Un effet qui n'est pas de la table ne change rien.
  const std::array<std::uint32_t, 1> elsewhere = {89};
  CHECK(TintAfterEffects(kWhite, elsewhere) == kWhite);
}

TEST_CASE("Effets d'acteur : la teinte se compose aussi bien un effet à la fois",
          "[render][actoreffect]") {
  // `app` passe les effets d'un acteur un par un, en reprenant chaque résultat,
  // pour ne pas recopier leurs numéros dans une liste à chaque image. C'est
  // juste parce que la composition est un PLI : l'ordre compte, la coupure non.
  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
  const std::array<std::uint32_t, 5> suite = {120, 419, 538, 89, 120};
  std::array<std::uint8_t, 4> un_a_un = kWhite;
  for (const std::uint32_t each_effect_id : suite) {
    un_a_un = TintAfterEffects(un_a_un, std::span<const std::uint32_t>(&each_effect_id, 1));
  }
  CHECK(un_a_un == TintAfterEffects(kWhite, suite));
  CHECK(un_a_un == std::array<std::uint8_t, 4>{0, 255, 0, 50});
}

TEST_CASE("Effets d'acteur : une teinte qui clignote alterne avec le blanc, un tic sur deux",
          "[render][actoreffect]") {
  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
  constexpr std::array<std::uint8_t, 4> kMagenta = {255, 0, 255, 255};
  const auto a_l_age = [](std::uint32_t each_effect_id, std::uint32_t age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{each_effect_id, age}};
    return TintAfterEffectsAt(kWhite, played);
  };

  // L'enchantement mortel : le corps luit et clignote en magenta pendant
  // quatre-vingt-un tics.
  const ActorEffectChange* mortel = ActorEffectChangeFor(493);  // EF_EDP
  REQUIRE(mortel != nullptr);
  CHECK(mortel->family == ActorEffectFamily::kTint);
  CHECK(mortel->render_bits == 0x1U);
  CHECK(mortel->life_ticks == 81);
  CHECK(a_l_age(493, 0) == kMagenta);
  CHECK(a_l_age(493, 1) == kWhite);
  CHECK(a_l_age(493, 2) == kMagenta);
  CHECK(a_l_age(493, 79) == kWhite);
  CHECK(a_l_age(493, 80) == kMagenta);
  // Passé sa vie, la teinte de son dernier tic, pair, revient vers le blanc de
  // huit par tic : le vert, seul à ne pas y être, remonte de zéro.
  CHECK(a_l_age(493, 81) == std::array<std::uint8_t, 4>{255, 8, 255, 255});
  CHECK(a_l_age(493, 500) == kWhite);

  // Les corps vert, violet et bleu clignotent de même pendant trente et un tics.
  const std::array<std::pair<std::uint32_t, std::array<std::uint8_t, 4>>, 3> couleurs = {{
      {538, {0, 255, 0, 255}},    // EF_GREENBODY
      {768, {255, 0, 255, 255}},  // EF_PURPLEBODY
      {500, {0, 0, 255, 255}},
  }};
  for (const auto& [each_effect_id, color] : couleurs) {
    INFO("effet " << each_effect_id);
    CHECK(ActorEffectChangeFor(each_effect_id)->life_ticks == 31);
    CHECK(a_l_age(each_effect_id, 0) == color);
    CHECK(a_l_age(each_effect_id, 29) == kWhite);
    CHECK(a_l_age(each_effect_id, 30) == color);
    // Plus écrite, la couleur remonte de huit par tic : ni figée, ni effacée
    // d'un coup.
    CHECK(a_l_age(each_effect_id, 31) != color);
    CHECK(a_l_age(each_effect_id, 31) != kWhite);
    CHECK(a_l_age(each_effect_id, 62) == kWhite);
    // Sans âge, la composition rend la teinte des tics pairs.
    const std::array<std::uint32_t, 1> single = {each_effect_id};
    CHECK(TintAfterEffects(kWhite, single) == color);
  }

  // Un effet qui ne clignote pas garde sa teinte à tout âge.
  CHECK(a_l_age(120, 1) == std::array<std::uint8_t, 4>{255, 255, 255, 50});  // EF_CLOAKING

  // Le blanc est une teinte écrite : un effet plus récent passe devant.
  const std::array<ActorEffectPlay, 2> both = {ActorEffectPlay{493, 1}, ActorEffectPlay{538, 0}};
  CHECK(TintAfterEffectsAt(kWhite, both) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
}

TEST_CASE("Effets d'acteur : une teinte qui clignote peut attendre son premier tic",
          "[render][actoreffect]") {
  // Le corps d'origine, gris pour que « intouché » ne se confonde pas avec le
  // blanc des tics impairs.
  constexpr std::array<std::uint8_t, 4> kGrey = {90, 90, 90, 255};
  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
  const auto a_l_age = [](std::uint32_t each_effect_id, std::uint32_t age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{each_effect_id, age}};
    return TintAfterEffectsAt(kGrey, played);
  };

  struct Window {
    std::uint32_t each_effect_id;
    std::uint16_t first;
    std::uint16_t life;
    std::array<std::uint8_t, 4> color;
  };
  // Tous clignotent entre leur couleur, aux tics pairs, et le blanc. 602 n'a
  // pas de fin : le client ne borne que son départ.
  const std::array<Window, 7> fenetres = {{
      {496, 10, 31, {250, 250, 250, 255}},
      {782, 10, 31, {250, 250, 0, 255}},
      {505, 10, 36, {250, 250, 100, 255}},
      {521, 10, 51, {255, 0, 0, 255}},
      {599, 5, 26, {250, 250, 100, 255}},
      {602, 21, 0, {250, 50, 50, 255}},
      {502, 0, 141, {250, 250, 100, 255}},
  }};
  for (const Window& window : fenetres) {
    INFO("effet " << window.each_effect_id);
    const ActorEffectChange* change = ActorEffectChangeFor(window.each_effect_id);
    REQUIRE(change != nullptr);
    CHECK(change->first_tick == window.first);
    CHECK(change->life_ticks == window.life);
    if (window.first > 0) {
      // ⚠ AVANT SON PREMIER TIC, LE CORPS N'EST PAS TOUCHÉ, ni en couleur ni
      // en blanc : le client sort du cas avant d'écrire.
      CHECK(a_l_age(window.each_effect_id, 0) == kGrey);
      CHECK(a_l_age(window.each_effect_id, window.first - 1U) == kGrey);
    }
    const std::uint32_t pair = window.first + (window.first % 2U);
    CHECK(a_l_age(window.each_effect_id, pair) == window.color);
    CHECK(a_l_age(window.each_effect_id, pair + 1U) == kWhite);
  }

  // Passé la vie, la teinte du DERNIER tic écrit revient vers le blanc, de
  // huit par tic. Celui de 599 est le vingt-cinquième, impair : blanc déjà.
  CHECK(a_l_age(599, 25) == kWhite);
  CHECK(a_l_age(599, 400) == kWhite);
  // Celui de 521 est le cinquantième, rouge : un tic plus tard, le vert et le
  // bleu ont remonté de huit ; trente-deux tics plus tard, le corps est blanc.
  CHECK(a_l_age(521, 51) == std::array<std::uint8_t, 4>{255, 8, 8, 255});
  CHECK(a_l_age(521, 82) == kWhite);
  // 602 n'a pas de vie bornée : il écrit tant que son instance vit, soixante
  // tics, puis revient vers le blanc à partir de son cinquante-neuvième.
  CHECK(a_l_age(602, 4001) == kWhite);

  // ⚠ LE PREMIER TIC RETIENT AUSSI LES BITS. Le corps de 602 ne tremble qu'à
  // partir du vingt et unième tic, et la lueur qui respire autour de 521 ne
  // paraît qu'au dixième.
  const auto pose = [](std::uint32_t each_effect_id, std::uint32_t age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{each_effect_id, age}};
    return BodyPoseAfterEffects(played);
  };
  CHECK(pose(602, 20).jitter_pixels == 0.0F);
  CHECK(pose(602, 21).jitter_pixels > 0.0F);
  CHECK(pose(521, 9).additive_parts == 0);
  CHECK(pose(521, 10).additive_parts == ro::render::kBodyAndHeadwearParts);
  const auto copies = [](std::uint32_t each_effect_id, std::uint32_t age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{each_effect_id, age}};
    std::array<ro::render::ActorBodyCopy, 4> sortie{};
    return BodyCopiesAfterEffects(played, sortie);
  };
  CHECK(copies(521, 9) == 0);
  CHECK(copies(521, 10) == 2);

  // Et c'est ce qui retient une rampe : `EF_JUMPBODY` écrit `tic − 35` à
  // partir du trente-sixième tic, et lue plus tôt elle enfoncerait le corps.
  CHECK(pose(445, 20).up_pixels == 0.0F);
  CHECK(pose(445, 35).up_pixels == 0.0F);
  CHECK(pose(445, 36).up_pixels > 0.0F);
}

TEST_CASE("Effets d'acteur : la rampe est lue, et ce n'est jamais le tic nu",
          "[render][actoreffect]") {
  // ⚠ SANS LA RAMPE, UN BIT PORTÉ MONTRE UNE IMAGE FIGÉE. Ces trois-là n'ont
  // ni la même pente ni le même signe.
  const ActorEffectChange* retour = ActorEffectChangeFor(540);  // EF_BABYBODY_BACK
  REQUIRE(retour != nullptr);
  CHECK(retour->writes[0].slope == -3);
  CHECK(retour->writes[0].offset == 45);
  // 45 au premier tic, 0 au quinzième : le corps revient à sa taille.
  CHECK(ValueAt(retour->writes[0], 0, retour->life_ticks) == 45);
  CHECK(ValueAt(retour->writes[0], 15, retour->life_ticks) == 0);

  const ActorEffectChange* erased = ActorEffectChangeFor(656);  // EF_UNDEADBODY_DEL
  REQUIRE(erased != nullptr);
  CHECK(ValueAt(erased->writes[0], 0, erased->life_ticks) == 200);
  CHECK(ValueAt(erased->writes[0], 39, erased->life_ticks) == 5);

  // ⚠ ET L'ÂGE SE FIGE AU DERNIER TIC ÉCRIT, le trente-neuvième : à sa vie, le
  // client cesse d'écrire et le champ garde la valeur d'avant. Laisser l'âge
  // courir rendrait ici un alpha NÉGATIF, c'est-à-dire, une fois converti, un
  // corps redevenu opaque.
  CHECK(ValueAt(erased->writes[0], 40, erased->life_ticks) == 5);
  CHECK(ValueAt(erased->writes[0], 1000, erased->life_ticks) == 5);

  // Un plafond vient d'un cmov, pas d'un calcul : EF_UNDEADBODY monte jusqu'à
  // 200 et s'y arrête.
  const ActorEffectChange* pose = ActorEffectChangeFor(655);  // EF_UNDEADBODY
  REQUIRE(pose != nullptr);
  CHECK(pose->writes[0].cap == 200);
  CHECK(ValueAt(pose->writes[0], 50, pose->life_ticks) == 50);
  CHECK(ValueAt(pose->writes[0], 5000, pose->life_ticks) == 200);

  // Une valeur FIXE ne dépend pas de l'âge.
  const ActorEffectChange* fixed = ActorEffectChangeFor(421);  // EF_BABYBODY2
  REQUIRE(fixed != nullptr);
  CHECK(ValueAt(fixed->writes[0], 0, fixed->life_ticks) == 45);
  CHECK(ValueAt(fixed->writes[0], 99, fixed->life_ticks) == 45);
}

TEST_CASE("Effets d'acteur : les rémanences disent leur période", "[render][actoreffect]") {
  // Huit identifiants, une seule chose : une copie du sprite semée derrière
  // l'acteur. Ce qui les distingue est la PÉRIODE, et elle est tenue à la main :
  // un « tic modulo N » se compile en multiplication magique, que le relevé ne
  // suit pas.
  CHECK(ActorEffectChangeFor(166)->clone_period_ticks == 5);   // EF_MAKEBLUR
  CHECK(ActorEffectChangeFor(617)->clone_period_ticks == 20);  // EF_BUNSINJYUTSU
  CHECK(ActorEffectChangeFor(752)->clone_period_ticks == 10);  // EF_BUNSINJYUTSU2
  CHECK(ActorEffectChangeFor(1081)->clone_period_ticks == 5);  // EF_ESCAPE

  // ⚠ TOUS LES CLONES EN ONT UNE, et le générateur s'arrête si l'un en manque :
  // sans période il ne sème rien, et la table dirait qu'il ne fait rien.
  std::size_t semeurs = 0;
  for (const ActorEffectChange& change : ActorEffectChangeTable()) {
    if (change.family == ActorEffectFamily::kSpriteClone) {
      CHECK(change.clone_period_ticks > 0);
      ++semeurs;
    }
    if (change.clone_period_ticks > 0) {
      CHECK(change.family == ActorEffectFamily::kSpriteClone);
    }
  }
  CHECK(semeurs == 8);
}

TEST_CASE("Effets d'acteur : un ordre dit A QUEL TIC il part", "[render][actoreffect]") {
  // ⚠ ZÉRO EST UN TIC, PAS UNE ABSENCE, et c'est tout l'objet du champ.
  // `EF_SCREEN_QUAKE` relance son ordre un tic sur cinq depuis le premier et
  // jusqu'à sa borne de vie : il n'a donc pas de tic à lui.
  const ActorEffectChange* secousse = ActorEffectChangeFor(563);  // EF_SCREEN_QUAKE
  REQUIRE(secousse != nullptr);
  CHECK(secousse->command == 0x7A);
  CHECK(secousse->command_tick == ro::render::kCommandRepeats);
  CHECK(secousse->life_ticks == 100);

  // ⚠ ET DEUX AUTRES LA DEMANDENT, une seule fois et TARD. `EF_MAGICCRASHER` et
  // `EF_FALCONASSAULT` secouent l'écran à leur trentième tic — une demi-seconde
  // après leur départ, quand le coup touche. Les poser au premier tic ferait
  // trembler l'écran avant le coup.
  for (const std::uint32_t effect_id : {380U, 387U}) {
    const ActorEffectChange* strike = ActorEffectChangeFor(effect_id);
    REQUIRE(strike != nullptr);
    CHECK(strike->command == 0x7A);
    CHECK(strike->command_tick == 30);
  }

  // Les chiffres de dégâts, eux, partent au premier tic.
  CHECK(ActorEffectChangeFor(652)->command_tick == 0);  // EF_DAMAGE1

  // Ce qui ne donne aucun ordre ne prétend pas à un tic.
  const ActorEffectChange* cloaked = ActorEffectChangeFor(120);  // EF_CLOAKING
  REQUIRE(cloaked != nullptr);
  CHECK(cloaked->command == 0);
  CHECK(cloaked->command_tick == ro::render::kCommandRepeats);
}

TEST_CASE("Effets d'acteur : une teinte CALCULÉE n'est pas une teinte inconnue",
          "[render][actoreffect]") {
  // `EF_MAGICCRASHER2` refait ses trois composantes à chaque tic d'un
  // `rand() % 255`, jusqu'à son soixantième tic compris. Le relevé ne voyait
  // qu'un calcul ; ce qu'il calcule a été lu, et la table le dit.
  const ActorEffectChange* crasher = ActorEffectChangeFor(403);
  REQUIRE(crasher != nullptr);
  CHECK(crasher->tint == std::array<std::int16_t, 4>{kTintUnchanged, ro::render::kTintRandom,
                                                     ro::render::kTintRandom,
                                                     ro::render::kTintRandom});
  CHECK(crasher->life_ticks == 61);
  // Il TEINT, au sens de la mesure : sans cela il resterait compté comme un
  // effet qui ne change rien.
  CHECK(TintsActor(*crasher));

  // ⚠ ET `render` NE TIRE RIEN : une composante tirée au sort ne se compose pas
  // ici, sans quoi deux appels de suite rendraient deux images différentes et
  // aucune comparaison ne tiendrait. La couleur rendue est celle de départ.
  const std::array<std::uint32_t, 1> played = {403};
  const std::array<std::uint8_t, 4> base = {10, 20, 30, 40};
  CHECK(TintAfterEffects(base, played) == base);

  // Trois effets portent ce tirage, et le générateur s'arrête si un autre
  // calcule une composante sans qu'on l'ait lue.
  std::size_t tires = 0;
  for (const ActorEffectChange& change : ActorEffectChangeTable()) {
    if (std::ranges::any_of(change.tint, [](std::int16_t component) {
          return component == ro::render::kTintRandom;
        })) {
      ++tires;
    }
  }
  CHECK(tires == 3);

  // `EF_MAGICCRASHER` tire les siennes du trentième au soixantième tic
  // seulement : avant, le corps n'a pas encore reçu le coup.
  const ActorEffectChange* first = ActorEffectChangeFor(380);
  REQUIRE(first != nullptr);
  CHECK(first->tint[1] == ro::render::kTintRandom);
  CHECK(first->first_tick == 30);
  CHECK(first->life_ticks == 61);
  CHECK_FALSE(ro::render::ActsAt(*first, 29));
  CHECK(ro::render::ActsAt(*first, 30));
  CHECK(ro::render::ActsAt(*first, 60));
  CHECK_FALSE(ro::render::ActsAt(*first, 61));
  CHECK(ro::render::ActsAt(*crasher, 0));
  CHECK_FALSE(ro::render::ActsAt(*crasher, 61));
}

TEST_CASE("Effets d'acteur : l'ordre porte SA valeur et SA couleur", "[render][actoreffect]") {
  // Trois effets font paraître un chiffre au-dessus de leur cible sans qu'aucun
  // paquet ne porte de valeur : c'est le CLIENT qui la décide, et elle vaut un.
  //
  // ⚠ ET LA COULEUR N'EST PAS DÉDUITE D'UNE RÈGLE : deux d'entre eux écrivent
  // en ROUGE et le troisième en MAGENTA. Rien dans leur numéro ne le dit, et le
  // relevé les a lus au site d'appel.
  for (const std::uint32_t effect_id : {652U, 653U}) {  // EF_DAMAGE1, EF_DAMAGE1_2
    const ActorEffectChange* chiffre = ActorEffectChangeFor(effect_id);
    REQUIRE(chiffre != nullptr);
    CHECK(chiffre->command == 0x58);
    CHECK(chiffre->command_value == 1);
    CHECK(chiffre->command_color == 0xFFFF0000U);
  }
  CHECK(ActorEffectChangeFor(654)->command_color == 0xFFFF00FFU);  // EF_DAMAGE1_3

  // La secousse d'écran commande sans rien faire paraître.
  CHECK(ActorEffectChangeFor(563)->command_value == 0);
}

TEST_CASE("Effets d'acteur : les cinq plantes ont chacune leur espèce", "[render][actoreffect]") {
  // ⚠ LES CINQ PARTAGENT UNE SEULE CASE et se départagent en comparant leur
  // propre numéro. Lue en bloc, elle rendait le même métier aux cinq — celui de
  // la dernière branche, et quatre effets auraient montré la mauvaise plante.
  const std::array<std::array<std::uint32_t, 2>, 5> especes = {
      {{1602U, 1080U}, {1603U, 1079U}, {1604U, 1081U}, {1605U, 1082U}, {1606U, 1078U}}};
  for (const std::array<std::uint32_t, 2>& espece : especes) {
    const std::uint32_t effect_id = espece[0];
    const std::uint32_t job = espece[1];
    const ActorEffectChange* planted = ActorEffectChangeFor(effect_id);
    REQUIRE(planted != nullptr);
    CHECK(planted->family == ActorEffectFamily::kAppearance);
    CHECK(static_cast<std::uint32_t>(planted->appearance_job) == job);
  }

  // ⚠ ET MOINS UN N'EST PAS UN MÉTIER. Kagemusha passe cette valeur à sa copie
  // pour dire « pas de classe imposée » ; la rendre telle quelle donnerait une
  // classe de soixante-cinq mille.
  CHECK(ActorEffectChangeFor(1004)->appearance_job == 0);  // EF_KAGEMUSYA

  // Cinq, et pas un de plus.
  std::size_t plantes = 0;
  for (const ActorEffectChange& change : ActorEffectChangeTable()) {
    if (change.appearance_job != 0) {
      ++plantes;
    }
  }
  CHECK(plantes == 5);
}

TEST_CASE("Effets d'acteur : un effet en ÉTEINT un autre", "[render][actoreffect]") {
  // ⚠ C'EST LA SEULE EXTINCTION QUI EXISTE hors de la destruction de l'acteur.
  // Un drapeau d'apparence ne se baisse jamais (ADR 0024) ; ce que le client
  // sait faire, c'est RETIRER un effet de la liste de son acteur, et un effet
  // peut le lui demander.
  //
  // L'oracle est le NOM, et il vient du serveur : 605 s'appelle
  // `EF_NPC_STOP2_DEL`, et le numéro qu'il passe — 604 — est `EF_NPC_STOP2`.
  const ActorEffectChange* annule = ActorEffectChangeFor(605);
  REQUIRE(annule != nullptr);
  CHECK(annule->family == ActorEffectFamily::kOrder);
  CHECK(annule->command == 0x6D);
  CHECK(annule->command_value == 604);

  // Un seul effet du lot annule, et aucun autre ordre ne porte ce message.
  std::size_t annulent = 0;
  for (const ActorEffectChange& change : ActorEffectChangeTable()) {
    if (change.command == 0x6D) {
      ++annulent;
    }
  }
  CHECK(annulent == 1);
}

TEST_CASE("Effets d'acteur : les cent trente et un CHANGENT tous quelque chose",
          "[render][actoreffect]") {
  // ⚠ LE LOT EST CLOS, ET CE TEST EN EST LA GARDE. Chacun des cent trente-six
  // porte au moins une chose que `libs/render` sait rendre : un bit, une teinte,
  // une écriture, un ordre, une période de rémanence, une classe, ou sa famille
  // pour la rotation. Une entrée qui n'aurait plus rien serait un effet perdu
  // par le relevé, et non un effet qui ne fait rien.
  const std::span<const ActorEffectChange> table = ActorEffectChangeTable();
  // ⚠ DEUX SONT ENTRES LE 2026-09-13, quand le releve des cas a cesse de
  // s'arreter au saut : EF_MADNESS_RED et EF_SHRINK font leur travail dans une
  // QUEUE partagee, et le releve les rangeait « il ne fait que tiquer ».
  // ⚠ ET QUARANTE-QUATRE SONT ENTRÉS PAR L'ENTONNOIR : des cas qui bâtissent un
  // nœud ET changent l'acteur, que le tri des cas sans bâtisseur ne pouvait pas
  // voir. Le quarante-quatrième est Shadow Form (809), dont le cas ne fait
  // qu'appeler la fonction qui teint : l'entonnoir suit depuis un niveau de
  // délégation.
  // ⚠ ET QUATRE DE PLUS PAR LE TROISIÈME RÉPARTITEUR, que l'entonnoir ne
  // parcourait pas : 303, 361, 366 et 369, la teinte de Concentration du Lord
  // Knight.
  // ⚠ ET UN, LU À LA MAIN : Berserk Pitcher (220), dont le gestionnaire teint
  // son porteur par un MESSAGE à l'acteur plutôt que par le teinturier.
  CHECK(table.size() == 137);
  for (const ActorEffectChange& change : table) {
    const bool ecrit = change.writes[0].field != 0 || change.writes[1].field != 0;
    const bool dit_quelque_chose = change.render_bits != 0 || TintsActor(change) || ecrit ||
                                   change.command != 0 || change.clone_period_ticks != 0 ||
                                   change.appearance_job != 0 ||
                                   change.family == ActorEffectFamily::kSpin;
    CHECK(dit_quelque_chose);
  }
}

TEST_CASE("Effets d'acteur : ceux dont le travail vit dans une QUEUE", "[render][actoreffect]") {
  // ⚠ UN CAS QUI SAUTE VERS DU CODE PARTAGÉ ÉTAIT LU TRONQUÉ. La borne d'un cas
  // est le cas SUIVANT dans l'ordre des adresses ; celui d'EF_MADNESS_RED
  // calcule son bit puis saute sur trois instructions communes qui l'écrivent.
  // Le relevé le rangeait « il n'écrit que son horloge », et son site
  // d'émission dormait dans la pile de ceux qu'on n'avait pas lus.
  const ActorEffectChange* red = ActorEffectChangeFor(626);  // EF_MADNESS_RED
  REQUIRE(red != nullptr);
  CHECK(red->family == ActorEffectFamily::kRenderFlag);
  CHECK(red->render_bits == 0x40000);
  // Le MÊME paramètre que son frère bleu, à la même valeur : c'est ce qui les
  // apparie, et c'est ce qui manquait.
  CHECK(red->writes[0].field == 0xF6);
  CHECK(red->writes[0].value == 20);
  const ActorEffectChange* blue = ActorEffectChangeFor(625);  // EF_MADNESS_BLUE
  REQUIRE(blue != nullptr);
  CHECK(blue->writes[0].field == red->writes[0].field);
  CHECK(blue->writes[0].value == red->writes[0].value);
  // ⚠ ET LEURS BITS DIFFÈRENT D'UN FACTEUR SEIZE, pas d'un rang : le bleu pose
  // 0x4000, le rouge 0x40000. Les croire voisins en aurait fait un seul.
  CHECK(blue->render_bits == 0x4000);

  // EF_SHRINK teint l'acteur dans sa queue, après avoir joué son son. Le relevé
  // le rangeait « ne joue qu'un son ».
  const ActorEffectChange* retrecit = ActorEffectChangeFor(599);  // EF_SHRINK
  REQUIRE(retrecit != nullptr);
  CHECK(TintsActor(*retrecit));
}

TEST_CASE("Effets d'acteur : l'entonnoir rend les cas à bâtisseur qui changent l'acteur",
          "[render][actoreffect]") {
  constexpr std::array<std::uint8_t, 4> kGrey = {90, 90, 90, 255};
  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
  const auto a_l_age = [](std::uint32_t each_effect_id, std::uint32_t age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{each_effect_id, age}};
    return TintAfterEffectsAt(kGrey, played);
  };

  // ⚠ LA COULEUR PAIRE SE LIT SUR SON CHEMIN. Les deux branches se rejoignent
  // sur un même appel, et remonter le texte depuis lui rendait le blanc des
  // impairs : onze effets clignotaient blanc sur blanc.
  struct Clignote {
    std::uint32_t each_effect_id;
    std::uint16_t first;
    std::uint16_t life;
    std::array<std::uint8_t, 4> color;
  };
  const std::array<Clignote, 7> clignotants = {{
      {777, 0, 61, {155, 155, 255, 255}},
      {791, 6, 41, {0, 0, 255, 255}},
      {928, 6, 41, {0, 0, 255, 255}},
      {832, 4, 53, {5, 5, 255, 255}},
      {847, 4, 91, {255, 89, 182, 255}},
      {848, 4, 91, {182, 89, 255, 255}},
      {943, 4, 53, {55, 255, 55, 255}},
  }};
  for (const Clignote& expected : clignotants) {
    INFO("effet " << expected.each_effect_id);
    const ActorEffectChange* change = ActorEffectChangeFor(expected.each_effect_id);
    REQUIRE(change != nullptr);
    CHECK(change->first_tick == expected.first);
    CHECK(change->life_ticks == expected.life);
    const std::uint32_t pair = expected.first + (expected.first % 2U);
    CHECK(a_l_age(expected.each_effect_id, pair) == expected.color);
    CHECK(a_l_age(expected.each_effect_id, pair + 1U) == kWhite);
  }

  // Un chemin impair qui pousse la même couleur ne fait pas clignoter : 750
  // n'y saute que par-dessus son bit.
  const ActorEffectChange* concentre = ActorEffectChangeFor(750);
  REQUIRE(concentre != nullptr);
  CHECK(a_l_age(750, 9) == kGrey);
  CHECK(a_l_age(750, 10) == std::array<std::uint8_t, 4>{155, 155, 255, 255});
  CHECK(a_l_age(750, 11) == std::array<std::uint8_t, 4>{155, 155, 255, 255});

  // 392 clignote sur six tics : trois gris, trois presque blancs.
  constexpr std::array<std::uint8_t, 4> kCent = {100, 100, 100, 255};
  constexpr std::array<std::uint8_t, 4> kNearWhite = {250, 250, 250, 255};
  CHECK(ActorEffectChangeFor(392)->blink_period_ticks == 6);
  // Les tics 1 et 4 départagent une période de six d'un tic sur deux.
  CHECK(a_l_age(392, 0) == kCent);
  CHECK(a_l_age(392, 1) == kCent);
  CHECK(a_l_age(392, 2) == kCent);
  CHECK(a_l_age(392, 3) == kNearWhite);
  CHECK(a_l_age(392, 4) == kNearWhite);
  CHECK(a_l_age(392, 6) == kCent);
  // Son dernier tic écrit est le vingt-neuvième, dans la seconde moitié ; un
  // tic plus tard, la couleur remonte de huit.
  CHECK(a_l_age(392, 30) == std::array<std::uint8_t, 4>{255, 255, 255, 255});
  CHECK(a_l_age(392, 500) == std::array<std::uint8_t, 4>{255, 255, 255, 255});

  // 812 calcule sa parité : noir aux pairs, blanc aux impairs, du sixième au
  // soixantième tic.
  CHECK(a_l_age(812, 5) == kGrey);
  CHECK(a_l_age(812, 6) == std::array<std::uint8_t, 4>{0, 0, 0, 255});
  CHECK(a_l_age(812, 7) == kWhite);

  // 1209 garde ses bits à l'envers : il n'en lève aucun, et clignote vers le gris.
  const ActorEffectChange* inverse = ActorEffectChangeFor(1209);
  REQUIRE(inverse != nullptr);
  CHECK(inverse->render_bits == 0U);
  CHECK(a_l_age(1209, 1) == std::array<std::uint8_t, 4>{50, 50, 50, 255});

  // Une fenêtre fermée par un saut qui reste dans le cas se lit aussi : 385 ne
  // teint qu'à partir de son quarantième tic.
  CHECK(ActorEffectChangeFor(385)->first_tick == 40);
  CHECK(a_l_age(385, 39) == kGrey);
  CHECK(a_l_age(385, 40) == std::array<std::uint8_t, 4>{200, 150, 50, 255});

  // Le trait de lumière secoue l'écran à son quarante-quatrième tic.
  for (const std::uint32_t each_effect_id : {512U, 835U}) {
    const ActorEffectChange* trait = ActorEffectChangeFor(each_effect_id);
    REQUIRE(trait != nullptr);
    CHECK(trait->command == 0x7A);
    CHECK(trait->command_tick == 44);
    CHECK(trait->first_tick == 20);
  }

  // Vingt-cinq effets clignotent, et chacun est une teinte.
  std::size_t clignotent = 0;
  for (const ActorEffectChange& change : ActorEffectChangeTable()) {
    if (std::ranges::any_of(change.odd_tick_tint,
                            [](std::int16_t component) { return component != kTintUnchanged; })) {
      ++clignotent;
      CHECK(change.family == ActorEffectFamily::kTint);
    }
  }
  CHECK(clignotent == 25);
}

TEST_CASE("Effets d'acteur : 383, 1207 et 1208 bleuissent le porteur en rampe",
          "[render][actoreffect]") {
  // La teinte `(−1, 2·(125 − âge), 2·(125 − âge), 250)` du cinquantième au
  // quatre-vingtième tic : le rouge et le vert descendent de 150 à 90, le
  // bleu reste 250 ; passé la vie, la teinte du dernier tic remonte vers le
  // blanc, de huit par tic.
  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
  for (const std::uint32_t each_effect_id : {383U, 1207U, 1208U}) {
    CAPTURE(each_effect_id);
    const auto a_l_age = [each_effect_id](std::uint32_t age) {
      const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{each_effect_id, age}};
      return TintAfterEffectsAt(kWhite, played);
    };
    CHECK(a_l_age(49) == kWhite);
    CHECK(a_l_age(50) == std::array<std::uint8_t, 4>{150, 150, 250, 255});
    CHECK(a_l_age(65) == std::array<std::uint8_t, 4>{120, 120, 250, 255});
    CHECK(a_l_age(80) == std::array<std::uint8_t, 4>{90, 90, 250, 255});
    CHECK(a_l_age(81) == std::array<std::uint8_t, 4>{98, 98, 255, 255});
    CHECK(a_l_age(200) == kWhite);
  }
}

TEST_CASE("Effets d'acteur : Shadow Form noircit son porteur et le rend translucide",
          "[render][actoreffect]") {
  // Le cas de Shadow Form ne fait qu'appeler une fonction, et c'est elle qui
  // écrit la couleur du porteur à chaque tic : alpha 50, rouge, vert et bleu à
  // zéro. Le camouflage écrit le même alpha mais laisse les trois autres : l'un
  // est un corps noir, l'autre un corps transparent.
  const ActorEffectChange* shadow = ActorEffectChangeFor(809);
  REQUIRE(shadow != nullptr);
  CHECK(shadow->family == ActorEffectFamily::kTint);
  CHECK(shadow->render_bits == 0U);
  CHECK(shadow->tint == std::array<std::int16_t, 4>{50, 0, 0, 0});
  CHECK(shadow->life_ticks == 0);
  CHECK(shadow->first_tick == 0);

  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
  constexpr std::array<std::uint8_t, 4> kOmbre = {0, 0, 0, 50};
  const auto a_l_age = [](std::uint32_t age) {
    const std::array<ActorEffectPlay, 1> played = {ActorEffectPlay{809, age}};
    return TintAfterEffectsAt(kWhite, played);
  };
  CHECK(a_l_age(0) == kOmbre);
  CHECK(a_l_age(1) == kOmbre);
  // Sa durée est l'infini du client : l'instance ne meurt jamais d'elle-même,
  // et la couleur ne remonte pas.
  CHECK(a_l_age(100'000) == kOmbre);
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{744, 100'000}};
  CHECK(TintAfterEffectsAt(kWhite, cloaked) == std::array<std::uint8_t, 4>{255, 255, 255, 50});
}
