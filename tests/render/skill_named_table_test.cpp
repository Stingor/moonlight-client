#include "ro/render/skill_named_table.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <tuple>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/core/ascii.h"
#include "ro/render/skill_effect_table.h"

using ro::render::kSkillNamedOnCast;
using ro::render::kSkillNamedOnHit;
using ro::render::kSkillNamedOnUseSkill;
using ro::render::NamedEffectAnchor;
using ro::render::NamedEffectVerb;
using ro::render::SkillNamedEffectEntry;
using ro::render::SkillNamedEffects;
using ro::render::SkillNamedEffectTable;

TEST_CASE("Compétences nommées : la table tient ses invariants", "[render][skillnamed]") {
  const std::span<const SkillNamedEffectEntry> table = SkillNamedEffectTable();
  REQUIRE(!table.empty());

  // Triée par compétence PUIS par slot : la recherche rend une plage, et une
  // entrée hors d'ordre la couperait en deux.
  for (std::size_t index = 1; index < table.size(); ++index) {
    const SkillNamedEffectEntry& before = table[index - 1];
    const SkillNamedEffectEntry& after = table[index];
    CHECK(before.skill_id <= after.skill_id);
    if (before.skill_id == after.skill_id) {
      CHECK(before.slot <= after.slot);
    }
  }
  for (const SkillNamedEffectEntry& entry : table) {
    CHECK(entry.skill_id != 0);
    // Une entrée sans nom est un effet PAR IDENTIFIANT, et rien d'autre.
    CHECK(entry.name.empty() == (entry.effect_id != 0));
    // Le client abaisse la clé avant de la ranger, et ces noms servent de clé.
    CHECK(entry.name == ro::core::AsciiLower(entry.name));
  }
}

TEST_CASE("Compétences nommées : le SLOT est le moment", "[render][skillnamed]") {
  // NW_WILD_FIRE (5409) joue au slot 12 — un chemin que nous ne déclenchons pas
  // encore —, et rien au moment de ZC_USE_SKILL. Le vérifier garde l'invariant
  // qui compte : un slot ne se confond pas avec un autre.
  const std::span<const SkillNamedEffectEntry> par_usage =
      SkillNamedEffects(5409, kSkillNamedOnUseSkill);
  CHECK(par_usage.empty());
  const std::span<const SkillNamedEffectEntry> au_douze = SkillNamedEffects(5409, 12);
  REQUIRE(au_douze.size() == 2);
  CHECK(au_douze[0].name == "wild_fire");
  CHECK(au_douze[1].name == "wild_fire_bottom");

  // ⚠ L'ORDRE DANS UN SLOT EST CELUI DE LA FONCTION, et il n'est ni alphabétique
  // ni celui des données : le fond part avant ce qui se tient dessus.
  const std::span<const SkillNamedEffectEntry> unlimit =
      SkillNamedEffects(2255, kSkillNamedOnUseSkill);
  if (!unlimit.empty()) {
    CHECK(unlimit[0].name.starts_with("unlimit"));
  }

  // Une compétence que rien ne nomme ne rend rien, à tous les moments.
  CHECK(SkillNamedEffects(5, kSkillNamedOnUseSkill).empty());
  CHECK(SkillNamedEffects(5, kSkillNamedOnCast).empty());
  CHECK(SkillNamedEffects(0, kSkillNamedOnUseSkill).empty());
}

TEST_CASE("Compétences nommées : cesser n'est pas semer", "[render][skillnamed]") {
  // Le relevé porte les trois verbes ; les confondre ferait reparaître un effet
  // à l'instant où il doit disparaître. On vérifie que la table en porte au
  // moins deux sortes, sans quoi le générateur les aurait aplatis.
  const std::span<const SkillNamedEffectEntry> table = SkillNamedEffectTable();
  bool sown = false;
  bool cesse = false;
  for (const SkillNamedEffectEntry& entry : table) {
    sown = sown || entry.verb == NamedEffectVerb::kSow;
    cesse = cesse || entry.verb != NamedEffectVerb::kSow;
  }
  CHECK(sown);
  CHECK(cesse);
}

TEST_CASE("Compétences nommées : chaque moment garde sa signature", "[render][skillnamed]") {
  // ⚠ CE QUI A TRANCHÉ LES MOMENTS, et rien d'autre ne le dit : le code appelle
  // les six slots sans les distinguer, ce sont les NOMS et l'ANCRAGE qui les
  // séparent. Ces trois signatures sont donc la mesure elle-même, et un relevé
  // refait qui les perdrait aurait déplacé un moment sur un autre.
  std::size_t cast_suffixe = 0;
  std::size_t cast_total = 0;
  std::size_t hit_suffixe = 0;
  std::size_t hit_total = 0;
  std::size_t sol_en_position = 0;
  std::size_t sol_total = 0;
  std::size_t cast_by_number = 0;
  for (const SkillNamedEffectEntry& entry : SkillNamedEffectTable()) {
    // ⚠ UN EFFET PAR NUMÉRO N'A PAS DE NOM : ce sont les noms qui signent un
    // moment, et les cercles d'incantation posés par numéro à la pose se
    // reconnaissent à leur VIE, prise sur la durée annoncée.
    if (entry.effect_id != 0) {
      if (entry.slot == ro::render::kSkillNamedOnCastSpawn && entry.life_from_cast) {
        cast_by_number++;
      }
      continue;
    }
    if (entry.slot == ro::render::kSkillNamedOnCastSpawn) {
      cast_total++;
      if (entry.name.ends_with("_cast") || entry.name.ends_with("_cast_bottom")) {
        cast_suffixe++;
      }
    } else if (entry.slot == ro::render::kSkillNamedOnHit) {
      hit_total++;
      if (entry.name.ends_with("_hit")) {
        hit_suffixe++;
      }
    } else if (entry.slot == ro::render::kSkillNamedOnGround) {
      sol_total++;
      if (entry.anchor == ro::render::NamedEffectAnchor::kPosition) {
        sol_en_position++;
      }
    }
  }
  REQUIRE(cast_total > 0);
  REQUIRE(hit_total > 0);
  REQUIRE(sol_total > 0);
  // L'incantation : la grande majorité des noms le disent, et les cercles par
  // numéro — près d'une centaine, un par action de quatrième classe — vivent
  // la durée annoncée.
  CHECK(cast_suffixe * 4 > cast_total * 3);
  constexpr std::size_t kAtLeastCastCircles = 80;
  CHECK(cast_by_number >= kAtLeastCastCircles);
  // L'impact : de même.
  CHECK(hit_suffixe * 4 > hit_total * 3);
  // ⚠ ET LE SOL SE RECONNAÎT À SON ANCRAGE, non à son nom : c'est le seul moment
  // dont les effets sont laissés à une POSITION quand tous les autres sont
  // attachés à l'acteur.
  CHECK(sol_en_position * 4 > sol_total * 3);
}

TEST_CASE("États nommés : le slot 0 pose, le slot 1 retire", "[render][skillnamed]") {
  const std::span<const ro::render::StateNamedEffectEntry> table =
      ro::render::StateNamedEffectTable();
  REQUIRE(!table.empty());
  for (std::size_t index = 1; index < table.size(); ++index) {
    const ro::render::StateNamedEffectEntry& before = table[index - 1];
    const ro::render::StateNamedEffectEntry& after = table[index];
    CHECK(before.state_id <= after.state_id);
    if (before.state_id == after.state_id) {
      CHECK(before.slot <= after.slot);
    }
  }

  // ⚠ CE QUI DONNE SON SENS AUX DEUX SLOTS, et rien d'autre ne le dit : aucun
  // chemin du client ne les distingue, ce sont les VERBES qui tranchent. Le slot
  // 0 sème massivement, le slot 1 retire massivement — et si cette table venait
  // à s'inverser, nos états poseraient leur effet en tombant.
  std::size_t sown_at_placement = 0;
  std::size_t retire_a_la_pose = 0;
  std::size_t sown_at_lift = 0;
  std::size_t retire_a_la_levee = 0;
  // ⚠ UNE FAMILLE NUMÉROTÉE NE COMPTE QU'UNE FOIS : un compteur balaie ses
  // onze rangs à chaque valeur, et pèserait à lui seul plus que tous les
  // autres états.
  std::set<std::tuple<std::uint16_t, std::uint8_t, bool, std::string>> familles;
  for (const ro::render::StateNamedEffectEntry& entry : table) {
    std::string family(entry.name);
    while (!family.empty() && std::isdigit(static_cast<unsigned char>(family.back())) != 0) {
      family.pop_back();
    }
    const bool sown = entry.verb == ro::render::NamedEffectVerb::kSow;
    if (!familles.emplace(entry.state_id, entry.slot, sown, family).second) {
      continue;
    }
    if (entry.slot == ro::render::kStateNamedOnGained) {
      (sown ? sown_at_placement : retire_a_la_pose)++;
    } else {
      (sown ? sown_at_lift : retire_a_la_levee)++;
    }
  }
  CHECK(sown_at_placement > retire_a_la_pose * 8);
  CHECK(retire_a_la_levee > sown_at_lift * 8);

  // BLOCK_SEAL (1454) sème son départ et ses deux boucles, puis retire
  // exactement ces deux boucles. C'est la signature qui a permis de trancher.
  // Sa case pose et retire aussi la classe 2400 PAR NUMÉRO, après les noms.
  const std::span<const ro::render::StateNamedEffectEntry> pose =
      ro::render::StateNamedEffects(1454, ro::render::kStateNamedOnGained);
  REQUIRE(pose.size() == 4);
  CHECK(pose[0].name == "npc_block_seal_start");
  CHECK(pose[3].effect_id == 2400);
  const std::span<const ro::render::StateNamedEffectEntry> levee =
      ro::render::StateNamedEffects(1454, ro::render::kStateNamedOnLost);
  REQUIRE(levee.size() == 3);
  for (const ro::render::StateNamedEffectEntry& entry : levee) {
    CHECK(entry.verb == ro::render::NamedEffectVerb::kCut);
  }
  CHECK(levee[2].effect_id == 2400);

  // ⚠ CE QUI REND LE SUIVI INDISPENSABLE : un effet d'état est presque toujours
  // ATTACHÉ à celui qui le porte — 111 sur 112 —, et un buff qui resterait à la
  // case où il est né se détacherait du joueur au premier pas.
  std::size_t on_actor = 0;
  for (const ro::render::StateNamedEffectEntry& entry : table) {
    if (entry.anchor == ro::render::NamedEffectAnchor::kActor) {
      on_actor++;
    }
  }
  CHECK(on_actor * 10 > table.size() * 9);

  // ⚠ UN ÉTAT N'EST PAS UNE COMPÉTENCE. IG_GUARD_STANCE existe des deux côtés :
  // l'état sous 1202, la compétence sous 5255. Les confondre jouerait l'un pour
  // l'autre, et c'est la traduction par le nom qui l'aurait fait.
  CHECK_FALSE(ro::render::StateNamedEffects(1202, ro::render::kStateNamedOnGained).empty());
  CHECK(ro::render::StateNamedEffects(5255, ro::render::kStateNamedOnGained).empty());
  CHECK_FALSE(SkillNamedEffects(5255, kSkillNamedOnUseSkill).empty());
}

TEST_CASE("Compétences nommées : elles ne montrent rien par l'autre chemin",
          "[render][skillnamed]") {
  // ⚠ C'EST LA RAISON D'ÊTRE DE CETTE TABLE. Ces compétences-là tombent au
  // DÉFAUT du répartiteur du client, et la table des effets de compétence — qui
  // vient des cas de ce même répartiteur — ne les connaît donc pas. Si l'une
  // s'y trouvait, l'un des deux relevés aurait dérapé, et nous jouerions deux
  // effets l'un sur l'autre.
  const std::span<const ro::render::SkillEffectEntry> by_id = ro::render::SkillEffectTable();
  for (const SkillNamedEffectEntry& entry : SkillNamedEffectTable()) {
    if (entry.slot != kSkillNamedOnUseSkill) {
      continue;
    }
    bool also_by_id = false;
    for (const ro::render::SkillEffectEntry& other : by_id) {
      if (other.skill_id == entry.skill_id) {
        also_by_id = true;
        break;
      }
    }
    CHECK_FALSE(also_by_id);
  }
}

TEST_CASE("Compétences nommées : le paquet des dégâts sème sur la source OU la cible",
          "[render][skillnamed]") {
  // Crescive Bolt part de qui tire ; Chasing Shot éclate sur qui est touché.
  constexpr std::uint16_t kCresciveBolt = 5334;
  constexpr std::uint16_t kChasingShot = 6514;
  const std::span<const SkillNamedEffectEntry> bolt =
      SkillNamedEffects(kCresciveBolt, ro::render::kSkillNamedOnDamagePacket);
  REQUIRE(bolt.size() == 1);
  CHECK(bolt.front().on_source);
  const std::span<const SkillNamedEffectEntry> chasing =
      SkillNamedEffects(kChasingShot, ro::render::kSkillNamedOnDamagePacket);
  REQUIRE(chasing.size() == 1);
  CHECK_FALSE(chasing.front().on_source);
}

TEST_CASE("Compétences nommées : une méthode choisit son bras, et ne cesse rien pour le choisir",
          "[render][skillnamed]") {
  using ro::render::NamedEffectGate;
  constexpr std::uint16_t kPhantom = 5204;
  constexpr std::uint16_t kDemolition = 5205;
  // Le signe du servant n'est que CHERCHÉ : aucune entrée ne le coupe ni ne
  // l'arrête, et les cinq semis se partagent entre les deux bras.
  const std::span<const SkillNamedEffectEntry> phantom =
      SkillNamedEffects(kPhantom, ro::render::kSkillNamedOnDamagePacket);
  REQUIRE(phantom.size() == 5);
  int if_carries = 0;
  for (const SkillNamedEffectEntry& entry : phantom) {
    CHECK(entry.verb == NamedEffectVerb::kSow);
    CHECK(entry.gate_name == "servant_sign");
    if_carries += entry.gate == NamedEffectGate::kIfTargetCarries ? 1 : 0;
  }
  CHECK(if_carries == 3);
  // ⚠ LE BRAS IMPAIR TIRE, LE PAIR RELIT : l'ordre de la table porte le tirage.
  const std::span<const SkillNamedEffectEntry> demolition =
      SkillNamedEffects(kDemolition, ro::render::kSkillNamedOnDamagePacket);
  REQUIRE(demolition.size() == 3);
  CHECK(demolition[1].gate == NamedEffectGate::kOddDraw);
  CHECK(demolition[2].gate == NamedEffectGate::kEvenDraw);
}

TEST_CASE("Compétences nommées : Kunaikaiten sème quatre tours, et le premier part sans délai",
          "[render][skillnamed]") {
  constexpr std::uint16_t kKunaikaiten = 5486;
  constexpr std::size_t kRounds = 4;
  constexpr std::size_t kNamesPerRound = 4;
  constexpr std::int32_t kRoundGapMs = 480;
  const std::span<const SkillNamedEffectEntry> kunai =
      SkillNamedEffects(kKunaikaiten, kSkillNamedOnUseSkill);
  REQUIRE(kunai.size() == kRounds * kNamesPerRound);
  for (std::size_t rank = 0; rank < kunai.size(); ++rank) {
    CHECK(kunai[rank].start_delay_ms ==
          static_cast<std::int32_t>(rank / kNamesPerRound) * kRoundGapMs);
  }
}

TEST_CASE("Compétences nommées : le tour d'une boucle remplit le trou du nom",
          "[render][skillnamed]") {
  // Hasty Fire in the Hole sème ses deux noms trois fois, 288 ms d'écart, et
  // chaque tour porte son numéro : aucun nom de la table ne garde son trou.
  constexpr std::uint16_t kHastyFire = 5411;
  constexpr std::int32_t kRoundGapMs = 288;
  const std::span<const SkillNamedEffectEntry> hasty =
      SkillNamedEffects(kHastyFire, ro::render::kSkillNamedOnGround);
  REQUIRE(hasty.size() == 6);
  CHECK(hasty[0].name == "hasty_fire_in_the_hole_0");
  CHECK(hasty[3].name == "hasty_fire_in_the_hole_bottom_1");
  CHECK(hasty[5].name == "hasty_fire_in_the_hole_bottom_2");
  for (std::size_t rank = 0; rank < hasty.size(); ++rank) {
    CHECK(hasty[rank].name.find('%') == std::string_view::npos);
    CHECK(hasty[rank].start_delay_ms == static_cast<std::int32_t>(rank / 2) * kRoundGapMs);
  }
  // Le tirage de Shadow Stab ne peut rendre qu'un.
  const std::span<const SkillNamedEffectEntry> stab = SkillNamedEffects(5291, kSkillNamedOnHit);
  REQUIRE(stab.size() == 1);
  CHECK(stab.front().name == "shadow_stab_hit1");
}

TEST_CASE("Compétences nommées : Storm Slash prend le niveau dans son nom, et naît sur le lanceur",
          "[render][skillnamed]") {
  const std::span<const SkillNamedEffectEntry> storm =
      SkillNamedEffects(5213, kSkillNamedOnUseSkill);
  REQUIRE(storm.size() == 1);
  CHECK(storm.front().name == "stormslash_{}");
  CHECK(storm.front().name_takes_level);
  CHECK(storm.front().on_source);
  // Aucun autre nom ne demande le niveau : leur trou vient d'ailleurs.
  for (const SkillNamedEffectEntry& entry : SkillNamedEffectTable()) {
    if (entry.skill_id != 5213) {
      CHECK_FALSE(entry.name_takes_level);
    }
  }
}

TEST_CASE("Compétences nommées : Midnight Fallen pose trois éclats sur un cercle de dix",
          "[render][skillnamed]") {
  constexpr float kRadius = 10.0F;
  constexpr float kTolerance = 1.0e-3F;
  constexpr std::int32_t kGapMs = 96;
  const std::span<const SkillNamedEffectEntry> fallen =
      SkillNamedEffects(5501, ro::render::kSkillNamedOnGround);
  REQUIRE(fallen.size() == 3);
  for (std::size_t rank = 0; rank < fallen.size(); ++rank) {
    const ro::render::NamedEffectOffset& around = fallen[rank].offset;
    // Un décalage FIXE : les bornes se confondent, et il n'y a rien à tirer.
    CHECK(around.x_min == around.x_max);
    CHECK(around.z_min == around.z_max);
    CHECK(std::hypot(around.x_min, around.z_min) == Catch::Approx(kRadius).margin(kTolerance));
    CHECK(fallen[rank].start_delay_ms == static_cast<std::int32_t>(rank) * kGapMs);
  }
  // Le premier à 45° : x et z égaux et positifs.
  CHECK(fallen[0].offset.x_min == Catch::Approx(fallen[0].offset.z_min).margin(kTolerance));
  CHECK(fallen[0].offset.x_min > 0.0F);
}

TEST_CASE("Compétences nommées : les éclats du Grand Judgement tirent leur quadrant",
          "[render][skillnamed]") {
  constexpr std::int32_t kGapMs = 140;
  const std::span<const SkillNamedEffectEntry> judgement =
      SkillNamedEffects(5263, kSkillNamedOnUseSkill);
  REQUIRE(judgement.size() == 10);
  // Les deux premiers sur la cible, sans décalage ; les huit éclats ensuite.
  CHECK(judgement[0].anchor == NamedEffectAnchor::kActor);
  for (std::size_t rank = 2; rank < judgement.size(); ++rank) {
    const SkillNamedEffectEntry& sub = judgement[rank];
    CHECK(sub.name == "new_grand_judgement_sub");
    CHECK(sub.start_delay_ms == static_cast<std::int32_t>(rank - 1) * kGapMs);
    CHECK(sub.offset.height_min == 0.0F);
    CHECK(sub.offset.height_max == 0.0F);
  }
  // Le premier éclat de chaque tour de quatre tombe au centre ; le troisième
  // part vers +x et -z.
  CHECK(judgement[2].offset.x_max == 0.0F);
  CHECK(judgement[4].offset.x_max > 0.0F);
  CHECK(judgement[4].offset.z_min < 0.0F);
  CHECK(judgement[4].offset.z_max == 0.0F);
}

TEST_CASE("Compétences nommées : un effet de décor se tait en effets allégés",
          "[render][skillnamed]") {
  using ro::render::NamedEffectGate;
  const std::span<const SkillNamedEffectEntry> swing =
      SkillNamedEffects(6507, kSkillNamedOnUseSkill);
  REQUIRE(swing.size() == 2);
  CHECK(swing[0].gate == NamedEffectGate::kUnlessMinimalEffects);
  CHECK(swing[0].on_source);
  CHECK(swing[1].gate == NamedEffectGate::kAlways);
  CHECK_FALSE(swing[1].on_source);
}

TEST_CASE("Compétences nommées : les cases des actions posent aussi des effets par numéro",
          "[render][skillnamed]") {
  // Shadow Exceed, à l'emploi : l'ombre 1867 sur le lanceur, et rien de nommé.
  const std::span<const SkillNamedEffectEntry> exceed =
      SkillNamedEffects(5285, kSkillNamedOnUseSkill);
  REQUIRE(exceed.size() == 1);
  CHECK(exceed.front().effect_id == 1867);
  CHECK(exceed.front().on_source);
  CHECK(exceed.front().name.empty());
  CHECK_FALSE(exceed.front().life_from_cast);
  // Meteor Storm Buster, à la pose : le cercle 55, qui vit la durée annoncée.
  const std::span<const SkillNamedEffectEntry> buster =
      SkillNamedEffects(5455, ro::render::kSkillNamedOnCastSpawn);
  const auto circle = std::ranges::find_if(
      buster, [](const SkillNamedEffectEntry& entry) { return entry.effect_id == 55; });
  REQUIRE(circle != buster.end());
  CHECK(circle->life_from_cast);
  CHECK_FALSE(circle->on_source);
  // Gale Storm, à l'emploi : la tempête 1973 sur la cible.
  const std::span<const SkillNamedEffectEntry> gale =
      SkillNamedEffects(5330, kSkillNamedOnUseSkill);
  REQUIRE(gale.size() == 1);
  CHECK(gale.front().effect_id == 1973);
  CHECK_FALSE(gale.front().on_source);
  // Shield Shooting, à l'emploi : le bouclier 2183 part du lanceur et vise la cible.
  const std::span<const SkillNamedEffectEntry> shooting =
      SkillNamedEffects(5265, kSkillNamedOnUseSkill);
  const auto shield = std::ranges::find_if(
      shooting, [](const SkillNamedEffectEntry& entry) { return entry.effect_id == 2183; });
  REQUIRE(shield != shooting.end());
  CHECK(shield->on_source);
  CHECK(shield->aimed_at_target);
  // Frost Field, à l'impact : 1310 sur qui reçoit le coup.
  const std::span<const SkillNamedEffectEntry> frost = SkillNamedEffects(790, kSkillNamedOnHit);
  REQUIRE(frost.size() == 1);
  CHECK(frost.front().effect_id == 1310);
  CHECK_FALSE(frost.front().on_source);
}

TEST_CASE("États nommés : les cases posent et retirent aussi par numéro", "[render][skillnamed]") {
  using ro::render::kStateNamedOnGained;
  using ro::render::kStateNamedOnLost;
  using ro::render::StateNamedEffectEntry;
  using ro::render::StateNamedEffects;
  // SHADOW_EXCEED (1192) : l'ombre 1004 à la pose, sans fin ; retirée à la levée.
  const std::span<const StateNamedEffectEntry> exceed =
      StateNamedEffects(1192, kStateNamedOnGained);
  REQUIRE(exceed.size() == 1);
  CHECK(exceed.front().effect_id == 1004);
  CHECK(exceed.front().verb == NamedEffectVerb::kSow);
  CHECK(exceed.front().endless);
  CHECK(exceed.front().name.empty());
  const std::span<const StateNamedEffectEntry> exceed_lost =
      StateNamedEffects(1192, kStateNamedOnLost);
  REQUIRE(exceed_lost.size() == 1);
  CHECK(exceed_lost.front().effect_id == 1004);
  CHECK(exceed_lost.front().verb == NamedEffectVerb::kCut);
  // T_FIRST_GOD (1360) : la marque 2355 RETIRÉE puis reposée, avec son rang ;
  // le quatrième dieu pose le rang quatre.
  const std::span<const StateNamedEffectEntry> first_god =
      StateNamedEffects(1360, kStateNamedOnGained);
  REQUIRE(first_god.size() == 2);
  CHECK(first_god[0].verb == NamedEffectVerb::kCut);
  CHECK(first_god[0].effect_id == 2355);
  CHECK(first_god[1].verb == NamedEffectVerb::kSow);
  CHECK(first_god[1].effect_id == 2355);
  CHECK(first_god[1].effect_param == 1);
  const std::span<const StateNamedEffectEntry> fourth_god =
      StateNamedEffects(1363, kStateNamedOnGained);
  REQUIRE(fourth_god.size() == 2);
  CHECK(fourth_god[1].effect_param == 4);
  // GRADUAL_GRAVITY (1014) : 779, qui finit seul, retiré par message à la levée.
  const std::span<const StateNamedEffectEntry> gravity =
      StateNamedEffects(1014, kStateNamedOnGained);
  REQUIRE(gravity.size() == 1);
  CHECK(gravity.front().effect_id == 779);
  CHECK_FALSE(gravity.front().endless);
  CHECK(gravity.front().effect_param < 0);
  REQUIRE(StateNamedEffects(1014, kStateNamedOnLost).size() == 1);
  CHECK(StateNamedEffects(1014, kStateNamedOnLost).front().effect_id == 779);
  // CHARGINGPIERCE_COUNT (1175) : la classe 1764 que la case pose est tenue par
  // le compte des charges, lu à la main — aucun numéro à la pose ; la levée la
  // retire par numéro.
  for (const StateNamedEffectEntry& entry : StateNamedEffects(1175, kStateNamedOnGained)) {
    CHECK(entry.effect_id == 0);
  }
  const std::span<const StateNamedEffectEntry> pierce_lost =
      StateNamedEffects(1175, kStateNamedOnLost);
  REQUIRE(pierce_lost.size() == 1);
  CHECK(pierce_lost.front().effect_id == 1764);
  CHECK(pierce_lost.front().verb == NamedEffectVerb::kCut);
  // ChangeSizeNPC (1420) : 2374 sans fin.
  const std::span<const StateNamedEffectEntry> size = StateNamedEffects(1420, kStateNamedOnGained);
  REQUIRE(size.size() == 1);
  CHECK(size.front().effect_id == 2374);
  CHECK(size.front().endless);
  // Une entrée par numéro n'a pas de nom et ne lit pas la valeur de l'état.
  std::size_t numbered = 0;
  for (const StateNamedEffectEntry& entry : ro::render::StateNamedEffectTable()) {
    if (entry.effect_id == 0) {
      continue;
    }
    ++numbered;
    CHECK(entry.name.empty());
    CHECK_FALSE(ro::render::StateNamedEntryReadsValue(entry));
    CHECK(entry.anchor == NamedEffectAnchor::kActor);
  }
  CHECK(numbered == 36);
}
