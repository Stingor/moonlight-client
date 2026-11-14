#include "ro/render/skill_cast_table.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/auto_counter.h"
#include "ro/render/skill_cast_extra.h"
#include "ro/render/skill_hit_extra.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_lua_effect.h"
#include "ro/render/skill_sound_table.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/state_effect.h"

using ro::render::ActionAnimationFor;
using ro::render::CastEffectForElement;
using ro::render::CastElementalEffects;
using ro::render::CastStateEntry;
using ro::render::CastStateTable;
using ro::render::kAnimationAttack2;
using ro::render::kAnimationCast;
using ro::render::kCastEffectDefault;
using ro::render::kCastEffectElemental;
using ro::render::kCastEffectNone;
using ro::render::kCastStateDefault;
using ro::render::kCastStateNone;
using ro::render::SkillCastEntry;
using ro::render::SkillCastTable;

using ro::render::kSkillUseEffectDefault;
using ro::render::kSkillUseEffectNone;
using ro::render::kSkillUseStateDefault;
using ro::render::SkillImplFor;
using ro::render::SkillUseEffectEntry;
using ro::render::SkillUseEffectFor;
using ro::render::SkillUseEffectShowsOnHit;
using ro::render::SkillUseEffectTable;
using ro::render::SkillUseMotionEntry;
using ro::render::SkillUseMotionTable;
using ro::render::SkillUseStateFor;
using ro::render::SpriteLayout;

namespace {

// Un métier que la table des exceptions ne nomme pas : il reçoit le rang commun.
// Zéro est le novice, et il n'y figure pas.
constexpr std::uint32_t kJobSansException = 0;
// Le barde, et la danseuse : deux des métiers qui GARDENT au lieu d'incanter.
constexpr std::uint32_t kJobBarde = 19;
constexpr std::uint32_t kJobDanseuse = 20;
// Un métier monté, qui ATTAQUE au lieu d'incanter.
constexpr std::uint32_t kMountedJob = 4218;

}  // namespace

TEST_CASE("Incantation : la table est triée et sans doublon", "[render][skillcast]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable, et un doublon rendrait l'un des deux inatteignable.
  const std::span<const SkillCastEntry> table = SkillCastTable();
  REQUIRE(!table.empty());
  std::uint16_t previous = 0;
  bool first = true;
  for (const SkillCastEntry& entry : table) {
    if (!first) {
      CHECK(previous < entry.skill_id);
    }
    previous = entry.skill_id;
    first = false;
  }
}

TEST_CASE("Incantation : aucune entrée ne redit les deux défauts", "[render][skillcast]") {
  // La table ne porte que ce qui S'ÉCARTE. Une entrée qui redirait les deux
  // défauts serait du bruit, et signalerait que le crible du générateur a sauté.
  for (const SkillCastEntry& entry : SkillCastTable()) {
    CHECK_FALSE((entry.effect_id == kCastEffectDefault && entry.state == kCastStateDefault));
  }
}

TEST_CASE("Incantation : la table des états est triée et sans doublon", "[render][skillcast]") {
  const std::span<const CastStateEntry> table = CastStateTable();
  REQUIRE(!table.empty());
  std::int8_t previous = 0;
  bool first = true;
  for (const CastStateEntry& entry : table) {
    // L'état qui ne joue rien n'a pas de rang : il n'a pas d'entrée non plus.
    CHECK(entry.state != kCastStateNone);
    if (!first) {
      CHECK(previous < entry.state);
    }
    previous = entry.state;
    first = false;
  }
}

TEST_CASE("Incantation : une compétence inconnue prend les deux défauts", "[render][skillcast]") {
  // Rien ne doit obliger l'appelant à redire « si la table ne sait pas ».
  const ro::render::SkillImpl& unknown = SkillImplFor(64000);
  CHECK(unknown.CastEffectId() == kCastEffectDefault);
  CHECK(unknown.CastState() == kCastStateDefault);
  // Et le défaut, c'est bien le rang d'incantation du .act.
  CHECK(ActionAnimationFor(unknown.CastState(), SpriteLayout::kPlayer, kJobSansException) ==
        std::optional{kAnimationCast});
}

TEST_CASE("Incantation : les entrées relevées à la main", "[render][skillcast]") {
  // BASH et MAGNUM BREAK frappent au lieu d'incanter, et c'est ce que dit leur
  // état : deux, l'état d'ATTAQUE. C'est le témoin qui prouve que le relevé a
  // lu le bon des deux pointeurs de sortie — l'autre aurait donné un effet.
  CHECK(SkillImplFor(5).CastState() == 2);  // SM_BASH
  CHECK(SkillImplFor(7).CastState() == 2);  // SM_MAGNUM
  CHECK(ActionAnimationFor(2, SpriteLayout::kPlayer, kJobSansException) ==
        std::optional{kAnimationAttack2});

  // SONIC BLOW ne montre RIEN à l'incantation : tout son visuel vient du chemin
  // des dégâts, et le client y pose l'effet « aucun ».
  CHECK(SkillImplFor(136).CastEffectId() == kCastEffectNone);  // AS_SONICBLOW
  CHECK(SkillImplFor(136).CastState() == 7);

  // TOUTE la famille des sorts de mage demande l'aura ÉLÉMENTAIRE, et garde la
  // pose ordinaire.
  for (const std::uint16_t skill_id :
       {std::uint16_t{14}, std::uint16_t{19}, std::uint16_t{20}, std::uint16_t{21}}) {
    CHECK(SkillImplFor(skill_id).CastEffectId() == kCastEffectElemental);
    CHECK(SkillImplFor(skill_id).CastState() == kCastStateDefault);
  }
}

TEST_CASE("Incantation : l'élément choisit l'aura", "[render][skillcast]") {
  // Le seul endroit où l'élément agit. Les six auras sont distinctes : deux
  // éléments qui rendraient la même auraient été lus au mauvais cas.
  const std::span<const std::int16_t> auras = CastElementalEffects();
  REQUIRE(auras.size() == 7);
  CHECK(auras[0] == kCastEffectElemental);
  for (std::size_t element = 1; element < auras.size(); ++element) {
    CHECK(auras[element] != kCastEffectElemental);
    for (std::size_t other = element + 1; other < auras.size(); ++other) {
      CHECK(auras[element] != auras[other]);
    }
  }
  // Cold Bolt est de l'eau, Fire Bolt du feu : la même compétence change d'aura
  // avec ce que le serveur annonce.
  const std::int16_t cold = SkillImplFor(14).CastEffectId();  // MG_COLDBOLT
  CHECK(CastEffectForElement(cold, 1) == auras[1]);
  CHECK(CastEffectForElement(cold, 3) == auras[3]);
  CHECK(CastEffectForElement(cold, 1) != CastEffectForElement(cold, 3));
}

TEST_CASE("Incantation : ce que l'élément ne touche pas", "[render][skillcast]") {
  // Un effet qui n'est pas l'aura générique traverse sans rien devoir à
  // l'élément — sans quoi Sonic Blow se mettrait à montrer une aura de feu.
  CHECK(CastEffectForElement(kCastEffectNone, 3) == kCastEffectNone);
  CHECK(CastEffectForElement(kCastEffectDefault, 3) == kCastEffectDefault);
  // Et un élément que la table ne nomme pas laisse l'aura générique, comme
  // chez le client, dont le switch n'a que six cas.
  CHECK(CastEffectForElement(kCastEffectElemental, 99) == kCastEffectElemental);
}

TEST_CASE("Incantation : ce qui ne change pas la posture", "[render][skillcast]") {
  // Soixante-quatre compétences laissent le lanceur sur ce qu'il faisait, et
  // c'est un cas à part entière : leur rendre le rang d'attente le planterait
  // debout au milieu d'un geste.
  CHECK_FALSE(
      ActionAnimationFor(kCastStateNone, SpriteLayout::kPlayer, kJobSansException).has_value());
  // Une CRÉATURE n'a pas les treize rangs d'un joueur : la table ne lui pose
  // rien, voir `CreatureActionAnimationFor`.
  CHECK_FALSE(ActionAnimationFor(kCastStateDefault, SpriteLayout::kCreature, kJobSansException)
                  .has_value());
}

TEST_CASE("États : le MÉTIER change le rang de trois d'entre eux", "[render][skillcast]") {
  // ⚠ ET L'UN DES TROIS EST LE SEPT, que 1 088 compétences prennent par défaut
  // au chemin de l'EMPLOI. Ces exceptions étaient relevées depuis le lot 16 et
  // jamais portées : tant que seul le chemin d'incantation les employait, elles
  // ne touchaient qu'une poignée de compétences.
  CHECK(ActionAnimationFor(7, SpriteLayout::kPlayer, kJobSansException) ==
        std::optional{kAnimationCast});
  // Le barde et la danseuse GARDENT — rang 4 — au lieu d'incanter.
  CHECK(ActionAnimationFor(7, SpriteLayout::kPlayer, kJobBarde) == std::optional{std::size_t{4}});
  CHECK(ActionAnimationFor(7, SpriteLayout::kPlayer, kJobDanseuse) ==
        std::optional{std::size_t{4}});
  // Un métier monté ATTAQUE — rang 10.
  CHECK(ActionAnimationFor(7, SpriteLayout::kPlayer, kMountedJob) ==
        std::optional{kAnimationAttack2});
  // Et l'exception ne déborde pas sur les autres états : le deux frappe pour
  // tout le monde, monté ou non.
  CHECK(ActionAnimationFor(2, SpriteLayout::kPlayer, kMountedJob) ==
        std::optional{kAnimationAttack2});
}

TEST_CASE("États : la table par métier est triée et n'invente rien", "[render][skillcast]") {
  const std::span<const ro::render::CastStateJobRank> table = ro::render::CastStateJobRankTable();
  REQUIRE(!table.empty());
  const std::span<const CastStateEntry> commune = CastStateTable();
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    const auto before = std::pair{table[rang - 1].state, table[rang - 1].job};
    CHECK(before < std::pair{table[rang].state, table[rang].job});
  }
  for (const ro::render::CastStateJobRank& table_entry : table) {
    // Une exception porte sur un état que la table COMMUNE nomme, et elle en
    // diffère : redire le rang commun serait du bruit, et l'attacher à un état
    // que personne ne joue serait une mesure sans objet.
    const auto found = std::ranges::find(commune, table_entry.state, &CastStateEntry::state);
    REQUIRE(found != commune.end());
    CHECK(found->animation != table_entry.animation);
  }
}

TEST_CASE("Emploi : la table est triée et sans doublon", "[render][skilluse]") {
  // La recherche est DICHOTOMIQUE : un identifiant hors d'ordre serait
  // introuvable, et un doublon rendrait l'un des deux inatteignable.
  const std::span<const SkillUseMotionEntry> table = SkillUseMotionTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].skill_id < table[rang].skill_id);
  }
}

TEST_CASE("Emploi : une compétence que le switch ne nomme pas prend le SEPT",
          "[render][skilluse]") {
  // ⚠ CE DÉFAUT EST UNE MESURE, pas un pis-aller : le résolveur pose sept en
  // entrant, et 1 088 des compétences de ce serveur ressortent avec. C'est ce
  // qui fait que la très grande majorité des compétences prend la pose
  // d'incantation quand on l'emploie, barre ou pas.
  CHECK(SkillUseStateFor(64000) == kSkillUseStateDefault);
  CHECK(kSkillUseStateDefault == 7);
  CHECK(ActionAnimationFor(kSkillUseStateDefault, SpriteLayout::kPlayer, kJobSansException) ==
        std::optional{kAnimationCast});
}

TEST_CASE("Emploi : NV_FIRSTAID prend la pose d'incantation", "[render][skilluse]") {
  // 🔴 LE PREMIER CONTRÔLE EN JEU DU PROTOCOLE. Le joueur a signalé que First
  // Aid ne montrait ni effet, ni son, ni pose ; la pose manquait parce que
  // notre client ne la jouait QUE pendant une barre d'incantation, et que cette
  // compétence-là est instantanée. Le client, lui, la pose par l'autre
  // résolveur. Voir `docs/protocole-de-controle-des-effets.md`.
  CHECK(SkillUseStateFor(142) == kSkillUseStateDefault);
  CHECK(ActionAnimationFor(SkillUseStateFor(142), SpriteLayout::kPlayer, kJobSansException) ==
        std::optional{kAnimationCast});
}

TEST_CASE("Emploi : BASH et MAGNUM BREAK frappent au lieu d'incanter", "[render][skilluse]") {
  // Le témoin qui prouve que l'oracle a lu le bon des deux pointeurs de sortie :
  // l'autre aurait rendu un effet. Les deux résolveurs s'accordent là-dessus, ce
  // qui est un second recoupement — ils ont été relevés séparément.
  CHECK(SkillUseStateFor(5) == 2);  // SM_BASH
  CHECK(SkillUseStateFor(7) == 2);  // SM_MAGNUM
  CHECK(ActionAnimationFor(SkillUseStateFor(5), SpriteLayout::kPlayer, kJobSansException) ==
        std::optional{kAnimationAttack2});
  CHECK(ro::render::SkillImplFor(5).CastState() == SkillUseStateFor(5));
}

TEST_CASE("Incantation et emploi : une compétence sans cas prend ceux de son ACTION",
          "[render][skillcast][skilluse]") {
  // Une compétence de quatrième classe qu'aucun cas du répartiteur ne nomme
  // passe par l'action que le registre enregistre pour elle. Le 16 par défaut
  // lui donnait une aura d'incantation que le client ne montre pas.
  constexpr std::uint16_t kSansAura = 5202;
  constexpr std::uint16_t kStriker = 5203;
  CHECK(ro::render::SkillImplFor(kSansAura).CastEffectId() == -1);
  CHECK(SkillUseStateFor(kStriker) == 2);
}

TEST_CASE("Emploi : quarante-cinq compétences ne changent RIEN à la posture",
          "[render][skilluse]") {
  // L'état moins un : le changement de posture rend la main dès son entrée, et
  // l'acteur garde ce qu'il faisait. Leur rendre le rang d'attente le planterait
  // debout au milieu d'un geste.
  //
  // Trente-trois le tiennent du RÉSOLVEUR, une de l'ACTION que le registre
  // enregistre pour elle ; les onze autres n'y sont même pas envoyées — voir
  // l'essai suivant.
  std::size_t sans_posture = 0;
  for (const SkillUseMotionEntry& table_entry : SkillUseMotionTable()) {
    if (table_entry.state == kCastStateNone) {
      ++sans_posture;
      CHECK_FALSE(ActionAnimationFor(table_entry.state, SpriteLayout::kPlayer, kJobSansException)
                      .has_value());
    }
  }
  CHECK(sans_posture == 45);
}

TEST_CASE("Emploi : le saut du RÉPARTITEUR prime sur le résolveur", "[render][skilluse]") {
  // 🔴 ONZE COMPÉTENCES NE SONT MÊME PAS ENVOYÉES AU RÉSOLVEUR.
  // Le répartiteur d'effets les compare une à une et saute par-dessus la
  // posture : le lanceur garde la sienne.
  //
  // ⚠ ET QUATRE D'ENTRE ELLES ONT UNE ENTRÉE CHEZ LE RÉSOLVEUR — 263, 5316 et
  // 5322 à l'état 2, qui FRAPPE, et 2327 à l'état 18. Les laisser gagner
  // remettrait le geste à zéro au milieu d'un enchaînement : elles sont TOUTES
  // des compétences d'ENCHAÎNEMENT, dont l'animation est déjà en cours quand le
  // paquet arrive. C'est le sens de la liste qui confirme sa lecture.
  constexpr std::array<std::uint16_t, 11> kSansPosture = {263,  272,  273,  371,  2327, 2328,
                                                          2329, 2579, 5316, 5322, 5327};
  for (const std::uint16_t skill_id : kSansPosture) {
    CHECK(SkillUseStateFor(skill_id) == kCastStateNone);
    CHECK_FALSE(
        ActionAnimationFor(SkillUseStateFor(skill_id), SpriteLayout::kPlayer, kJobSansException)
            .has_value());
  }
  // ⚠ ET LES DEUX CHEMINS NE DISENT PAS LA MÊME CHOSE DE LA MÊME COMPÉTENCE.
  // Triple Attack en est le témoin : son entrée au relevé de l'EMPLOI porte
  // l'état 2 — l'attaque —, que le répartiteur ne lui demande jamais ; et son
  // entrée au chemin de l'INCANTATION porte le 8, l'incantation ordinaire, qui
  // vaut toujours quand une barre tourne. Les confondre donnerait à l'un ce que
  // l'autre décide.
  CHECK(SkillUseStateFor(263) == kCastStateNone);  // l'emploi : rien
  CHECK(SkillImplFor(263).CastState() == 8);       // l'incantation : le rang 12
}

TEST_CASE("Emploi : aucune entrée ne redit le défaut", "[render][skilluse]") {
  // ⚠ LA TABLE NE PORTE QUE CE QUI EST POSÉ. Une entrée au sept serait
  // indiscernable du défaut — sauf qu'elle l'est : le relevé distingue « le
  // switch a posé sept » de « personne n'a rien posé », et 179 compétences sont
  // dans le premier cas. Elles ont donc leur entrée, et c'est voulu : si le
  // registre d'effets vient un jour remplir le second cas, ces 179-là devront
  // rester au sept.
  std::size_t au_sept = 0;
  for (const SkillUseMotionEntry& table_entry : SkillUseMotionTable()) {
    if (table_entry.state == kSkillUseStateDefault) {
      ++au_sept;
    }
  }
  CHECK(au_sept == 179);
}

TEST_CASE("Emploi : le résolveur rend DEUX valeurs, et l'effet en est une", "[render][skilluse]") {
  // 🔴 CET EFFET A MANQUÉ UN AN. Le relevé le portait, le générateur le jetait —
  // « nous ne savons pas encore où le client le consomme ». Il le consomme au
  // COUP, par un second appel du résolveur.
  //
  // ⚠ SON DÉFAUT EST SEIZE, ET SEIZE EST `EF_BASH` : une vraie gerbe, avec son
  // son. Il n'est pas neutre, et c'est tout le sujet.
  CHECK(kSkillUseEffectDefault == 16);
  CHECK(SkillUseEffectFor(64000) == kSkillUseEffectDefault);
}

TEST_CASE("Emploi : Bash tient son effet du DÉFAUT, et c'est ce qui manquait",
          "[render][skilluse]") {
  // Son cas du switch ne touche que l'ÉTAT — il le met à deux — si bien que
  // l'effet garde le seize posé en entrant : c'est l'effet 16 qui est lancé par
  // défaut.
  CHECK(SkillUseStateFor(5) == 2);
  CHECK(SkillUseEffectFor(5) == kSkillUseEffectDefault);
  // Magnum Break partage sa ligne du switch, et donc son effet.
  CHECK(SkillUseEffectFor(7) == kSkillUseEffectDefault);
}

TEST_CASE("Emploi : un boulon de mage pose l'effet douze, et NON au coup", "[render][skilluse]") {
  // Les onze identifiants 11 à 21 sont écartés d'un bloc par la garde du coup :
  // leur effet vient du répartiteur des dégâts, et le poser deux fois le
  // montrerait deux fois.
  CHECK(SkillUseEffectFor(14) == 12);
  CHECK(SkillUseEffectFor(19) == 12);
  CHECK_FALSE(SkillUseEffectShowsOnHit(14));
  CHECK_FALSE(SkillUseEffectShowsOnHit(19));
  CHECK_FALSE(SkillUseEffectShowsOnHit(11));
  CHECK_FALSE(SkillUseEffectShowsOnHit(21));
  // Juste en dehors du bloc, des deux côtés.
  CHECK(SkillUseEffectShowsOnHit(10));
  CHECK(SkillUseEffectShowsOnHit(22));
  // Et Bash, lui, passe.
  CHECK(SkillUseEffectShowsOnHit(5));
}

TEST_CASE("Emploi : « aucun effet » ne se confond pas avec « absent »", "[render][skilluse]") {
  // ⚠ LA DISTINCTION QUI COMPTE, et elle est l'inverse de nos autres tables :
  // un identifiant ABSENT prend seize, un identifiant à MOINS UN ne montre rien.
  // Les fondre donnerait `EF_BASH` aux sept cent quarante qui n'ont rien à
  // montrer.
  const std::span<const SkillUseEffectEntry> table = SkillUseEffectTable();
  const auto without_effect = std::ranges::count_if(
      table, [](const SkillUseEffectEntry& table_entry) { return table_entry.effect_id < 0; });
  CHECK(without_effect == 740);
  CHECK(kSkillUseEffectNone == -1);
  // Un cas nommé : le premier soin venu ne montre rien de ce côté.
  CHECK(SkillUseEffectFor(28) == kSkillUseEffectNone);
}

TEST_CASE("la garde d'Auto Counter se depense sur les sept sortes de coup") {
  // 🔴 CE N'EST PAS LA GARDE DES IMPACTS DE COMPÉTENCE, et les confondre était
  // le piège : `DamageTypeShowsHitEffect` vient d'un AUTRE répartiteur du
  // client, et il écarte des sortes que celui-ci accepte. Les sept que voici
  // sont les sortes de coup qui, dans le traitement des dégâts d'un acteur,
  // mènent au bloc de l'effet.
  constexpr std::array<std::uint8_t, 7> depensent = {0, 4, 8, 9, 10, 11, 13};
  for (const std::uint8_t kind : depensent) {
    CHECK(ro::render::AutoCounterSpentBy(kind));
  }
  // Les trois sortes que ce switch connaît sans qu'elles portent de dégâts : un
  // ramassage, une assise, un relevé. Elles ne dépensent rien.
  constexpr std::array<std::uint8_t, 3> sans_degats_types = {1, 2, 3};
  for (const std::uint8_t sans_degats : sans_degats_types) {
    CHECK_FALSE(ro::render::AutoCounterSpentBy(sans_degats));
  }
  // Et une sorte que le switch ne connaît pas du tout.
  CHECK_FALSE(ro::render::AutoCounterSpentBy(7));
}

TEST_CASE("l'effet qui arme la garde n'est pas celui qui la depense") {
  // Deux identifiants distincts, et c'est tout le mécanisme : le premier pose
  // la posture sans rien montrer, le second jaillit au coup suivant.
  CHECK(ro::render::kAutoCounterGuardEffect == 61);
  CHECK(ro::render::kAutoCounterStrikeEffect == 131);
  CHECK(ro::render::kAutoCounterGuardEffect != ro::render::kAutoCounterStrikeEffect);
  // ⚠ ET CELUI QUI DÉPENSE N'EST LIÉ À AUCUNE COMPÉTENCE : le chercher dans la
  // table des effets d'emploi ne rend rien, parce que ce chemin-là ne passe pas
  // par une compétence. C'est pourquoi les trois relevés l'ont tous manqué.
  CHECK(SkillUseEffectFor(59) == kSkillUseEffectNone);
}

TEST_CASE("une competence peut poser DEUX extras d'incantation") {
  // 🔴 C'EST POURQUOI LA TABLE EST UNE LISTE DE PAIRES. WL_SOULEXPANSION en
  // pose deux d'un coup — 1589 et 1590 —, et une table à une entrée par
  // compétence en aurait perdu un en silence.
  const std::span<const ro::render::SkillCastExtraEntry> two = ro::render::SkillCastExtrasFor(2202);
  REQUIRE(two.size() == 2);
  CHECK(two[0].effect_id == 1589);
  CHECK(two[1].effect_id == 1590);
  for (const ro::render::SkillCastExtraEntry& table_entry : two) {
    CHECK(table_entry.on == ro::render::SkillEffectOn::kTarget);
  }
}

TEST_CASE("l'extra d'incantation sait sur QUI il naît") {
  // CH_SOULCOLLECT rassemble ses esprits sur LUI : l'effet 402, les anneaux.
  const std::span<const ro::render::SkillCastExtraEntry> esprits =
      ro::render::SkillCastExtrasFor(401);
  REQUIRE(esprits.size() == 1);
  CHECK(esprits[0].effect_id == 402);
  CHECK(esprits[0].on == ro::render::SkillEffectOn::kCaster);
  // NC_FLAMELAUNCHER aussi, et c'est le second des deux : les cinq autres
  // naissent sur ce qui est visé.
  const std::span<const ro::render::SkillCastExtraEntry> lance =
      ro::render::SkillCastExtrasFor(2259);
  REQUIRE(lance.size() == 1);
  CHECK(lance[0].on == ro::render::SkillEffectOn::kCaster);
  // Et la quasi-totalité des compétences n'en pose aucun.
  CHECK(ro::render::SkillCastExtrasFor(5).empty());
  CHECK(ro::render::SkillCastExtrasFor(0).empty());
}

TEST_CASE("les extras d'une meme competence sont CONTIGUS dans la table") {
  // ⚠ `SkillCastExtrasFor` rend une TRANCHE : elle cherche la première entrée
  // puis s'arrête au premier identifiant différent. Une table où deux entrées
  // d'une même compétence seraient séparées en perdrait une sans rien dire, et
  // rien d'autre ne le verrait.
  std::uint16_t prior = 0;
  for (const ro::render::SkillCastExtraEntry& table_entry : ro::render::kSkillCastExtras) {
    CHECK(table_entry.skill_id >= prior);
    prior = table_entry.skill_id;
  }
  // La preuve par le compte : chaque compétence nommée rend autant d'entrées
  // que la table en porte pour elle.
  std::size_t reunies = 0;
  constexpr std::array<std::uint16_t, 7> nommees = {401, 490, 2202, 2214, 2236, 2259, 5284};
  for (const std::uint16_t skill : nommees) {
    reunies += ro::render::SkillCastExtrasFor(skill).size();
  }
  CHECK(reunies == ro::render::kSkillCastExtras.size());
}

TEST_CASE("les cas particuliers du coup sont contigus et complets") {
  // Même invariant que les extras d'incantation, et pour la même raison : la
  // recherche rend une TRANCHE, et deux entrées séparées en perdraient une.
  std::uint16_t prior = 0;
  for (const ro::render::SkillHitExtraEntry& table_entry : ro::render::kSkillHitExtras) {
    CHECK(table_entry.skill_id >= prior);
    prior = table_entry.skill_id;
  }
  constexpr std::array<std::uint16_t, 9> nommees = {272,  273,  370,  371, 372,
                                                    2328, 2329, 2332, 2559};
  std::size_t reunies = 0;
  for (const std::uint16_t skill : nommees) {
    const std::size_t count = ro::render::SkillHitExtrasFor(skill).size();
    CHECK(count > 0);
    reunies += count;
  }
  CHECK(reunies == ro::render::kSkillHitExtras.size());
  // Et rien pour les autres : Bash n'a que son effet d'emploi.
  CHECK(ro::render::SkillHitExtrasFor(5).empty());
}

TEST_CASE("les deux gardes du coup portent sur TOUS les effets de leur competence") {
  // 🔴 C'EST L'INVARIANT QUI COMPTE. SR_EARTHSHAKER renonce à ses DEUX effets
  // quand il en trouve un déjà là, et SR_RAMPAGEBLASTER à ses TROIS quand il se
  // vise lui-même. Une garde posée sur une seule entrée laisserait les autres
  // se jouer, et le défaut serait invisible hors du jeu.
  const std::span<const ro::render::SkillHitExtraEntry> terre = ro::render::SkillHitExtrasFor(2328);
  REQUIRE(terre.size() == 2);
  for (const ro::render::SkillHitExtraEntry& table_entry : terre) {
    CHECK(table_entry.guard == ro::render::HitExtraGuard::kNotIfAlreadyThere);
    CHECK(table_entry.on == ro::render::SkillEffectOn::kCaster);
  }
  // L'effet que sa garde cherche est l'un des deux qu'elle pose : le trouver
  // veut dire « je viens de me jouer ».
  const auto carries_marker =
      std::ranges::any_of(terre, [](const ro::render::SkillHitExtraEntry& table_entry) {
        return table_entry.effect_id == ro::render::kEarthshakerAlreadyThereEffect;
      });
  CHECK(carries_marker);

  const std::span<const ro::render::SkillHitExtraEntry> rampage =
      ro::render::SkillHitExtrasFor(2332);
  REQUIRE(rampage.size() == 3);
  for (const ro::render::SkillHitExtraEntry& table_entry : rampage) {
    CHECK(table_entry.guard == ro::render::HitExtraGuard::kNotOnSelf);
  }
}

TEST_CASE("un cas du coup sait sur QUI chacun de ses effets nait") {
  // MO_CHAINCOMBO en pose trois, et ils ne vont PAS tous au même endroit : deux
  // sur ce qu'il touche, un sur le moine. Les confondre mettrait la gerbe du
  // frappeur sur sa victime.
  const std::span<const ro::render::SkillHitExtraEntry> chaine = ro::render::SkillHitExtrasFor(272);
  REQUIRE(chaine.size() == 3);
  CHECK(chaine[0].on == ro::render::SkillEffectOn::kTarget);
  CHECK(chaine[1].on == ro::render::SkillEffectOn::kCaster);
  CHECK(chaine[2].on == ro::render::SkillEffectOn::kTarget);
  // ⚠ ET RL_QD_SHOT HÉRITE DU SECOND PAR UN `goto`. Son cas pose le 262 puis
  // tombe dans le label que MO_CHAINCOMBO partage, d'où le 273. Lire un cas
  // jusqu'à son `break` ne suffit pas.
  const std::span<const ro::render::SkillHitExtraEntry> qd = ro::render::SkillHitExtrasFor(2559);
  REQUIRE(qd.size() == 2);
  CHECK(qd[0].effect_id == 262);
  CHECK(qd[1].effect_id == 273);
  CHECK(chaine[0].effect_id == qd[0].effect_id);
  CHECK(chaine[2].effect_id == qd[1].effect_id);
}

TEST_CASE("les effets du Lua sont contigus par competence ET par emplacement") {
  // ⚠ `SkillLuaEffectsFor` rend une TRANCHE : elle cherche la première entrée
  // qui accorde compétence ET emplacement, puis s'arrête à la première qui ne
  // les accorde plus. Une table où deux entrées d'un même couple seraient
  // séparées en perdrait une sans rien dire.
  const std::span<const ro::render::SkillLuaEffectEntry> table = ro::render::SkillLuaEffectTable();
  CHECK(table.size() == 73);
  std::uint16_t prior = 0;
  for (const ro::render::SkillLuaEffectEntry& table_entry : table) {
    CHECK(table_entry.skill_id >= prior);
    prior = table_entry.skill_id;
  }
  // La preuve par le compte : chaque couple rend autant que la table en porte.
  std::size_t reunies = 0;
  std::uint16_t view = 0;
  for (const ro::render::SkillLuaEffectEntry& table_entry : table) {
    if (table_entry.skill_id == view) {
      continue;
    }
    view = table_entry.skill_id;
    for (const ro::render::SkillLuaSlot slot :
         {ro::render::SkillLuaSlot::kCaster, ro::render::SkillLuaSlot::kTarget,
          ro::render::SkillLuaSlot::kGround, ro::render::SkillLuaSlot::kBegin}) {
      reunies += ro::render::SkillLuaEffectsFor(view, slot).size();
    }
  }
  CHECK(reunies == table.size());
}

TEST_CASE("aucune competence du Lua n'a de son dans le releve de l'executable") {
  // 🔴 C'EST CE QUI JUSTIFIE QUE LE LUA REMPLACE. Le code de `PlaySkillSound`
  // affirme qu'aucune de ces soixante-six ne figure dans la table de sons tirée
  // de l'exécutable ; si l'une d'elles y entrait un jour, le remplacement
  // masquerait le son que le client joue vraiment, et rien ne le dirait.
  const std::span<const ro::render::SkillLuaSoundEntry> sounds = ro::render::SkillLuaSoundTable();
  CHECK(sounds.size() == 37);
  for (const ro::render::SkillLuaSoundEntry& table_entry : sounds) {
    CHECK_FALSE((table_entry.caster_wave.empty() && table_entry.target_wave.empty()));
    INFO("competence " << table_entry.skill_id);
    CHECK(ro::render::SkillSound(table_entry.skill_id).empty());
  }
  // Et une compétence ordinaire n'a rien de ce côté-ci.
  const ro::render::SkillLuaSoundEntry bash = ro::render::SkillLuaSoundFor(5);
  CHECK(bash.caster_wave.empty());
  CHECK(bash.target_wave.empty());
}

TEST_CASE("le drapeau onTarget ne deplace QUE l'effet de lanceur") {
  // KO_JYUMONJIKIRI : un effet de CIBLE, un son de lanceur, et `onTarget`.
  CHECK(ro::render::SkillLuaPutsCasterEffectOnTarget(3004));
  const std::span<const ro::render::SkillLuaEffectEntry> target =
      ro::render::SkillLuaEffectsFor(3004, ro::render::SkillLuaSlot::kTarget);
  REQUIRE(target.size() == 1);
  CHECK(target[0].effect_id == 996);
  CHECK(ro::render::SkillLuaEffectsFor(3004, ro::render::SkillLuaSlot::kCaster).empty());
  CHECK_FALSE(ro::render::SkillLuaSoundFor(3004).caster_wave.empty());
  // Vingt-deux le portent, et la recherche est dichotomique : la liste doit
  // être triée, sinon elle répondrait faux sans jamais lever.
  const std::span<const std::uint16_t> with = ro::render::SkillLuaOnTargetSkills();
  CHECK(with.size() == 22);
  CHECK(std::ranges::is_sorted(with));
  for (const std::uint16_t skill : with) {
    CHECK(ro::render::SkillLuaPutsCasterEffectOnTarget(skill));
  }
  CHECK_FALSE(ro::render::SkillLuaPutsCasterEffectOnTarget(5));
}

namespace {

// Une table retournée : à chaque effet, les états qui le font naître.
std::map<std::uint16_t, std::set<std::uint16_t>> ByEffect(
    std::span<const ro::render::StateEffectEntry> table) {
  std::map<std::uint16_t, std::set<std::uint16_t>> sortie;
  for (const ro::render::StateEffectEntry& table_entry : table) {
    sortie[table_entry.effect_id].insert(table_entry.state_id);
  }
  return sortie;
}

// Et dans l'autre sens : à chaque état, ce qu'il pose.
std::map<std::uint16_t, std::set<std::uint16_t>> ByState(
    std::span<const ro::render::StateEffectEntry> table) {
  std::map<std::uint16_t, std::set<std::uint16_t>> sortie;
  for (const ro::render::StateEffectEntry& table_entry : table) {
    sortie[table_entry.state_id].insert(table_entry.effect_id);
  }
  return sortie;
}

}  // namespace

TEST_CASE("les effets d'un meme etat sont contigus dans les DEUX tables") {
  for (const std::span<const ro::render::StateEffectEntry> table :
       {ro::render::StateEffectTable(), ro::render::StateRebuildEffectTable()}) {
    std::uint16_t prior = 0;
    for (const ro::render::StateEffectEntry& table_entry : table) {
      CHECK(table_entry.state_id >= prior);
      prior = table_entry.state_id;
    }
    // Une tranche par état, et la somme des tranches égale la table.
    std::size_t reunies = 0;
    std::uint16_t vu = 0;
    for (const ro::render::StateEffectEntry& table_entry : table) {
      if (table_entry.state_id == vu) {
        continue;
      }
      vu = table_entry.state_id;
      const bool pose = &table.front() == &ro::render::StateEffectTable().front();
      reunies += pose ? ro::render::StateEffectsFor(vu).size()
                      : ro::render::StateRebuildEffectsFor(vu).size();
    }
    CHECK(reunies == table.size());
  }
}

TEST_CASE("aucun effet n'est attribue a une foule d'etats") {
  // 🔴 LA SIGNATURE DU DÉFAUT QUE CETTE TABLE A PORTÉ PENDANT DES SEMAINES :
  // soixante cas écrivent leur identifiant dans une case de pile puis SAUTENT
  // vers un semis commun, et la remontée depuis ce semis rendait quarante-deux
  // états crédités du même effet 368 — quatre-vingt-dix couples faux sur 175.
  // ⚠ UN EFFET PARTAGÉ N'EST PAS SUSPECT EN SOI : 166 sert sept états, et c'est
  // mesuré. C'est la DIZAINE qui ne s'explique que par une queue partagée.
  constexpr std::size_t kPlausibleReach = 10;
  for (const std::span<const ro::render::StateEffectEntry> table :
       {ro::render::StateEffectTable(), ro::render::StateRebuildEffectTable()}) {
    for (const auto& [effect_id, etats] : ByEffect(table)) {
      INFO("effet " << effect_id);
      CHECK(etats.size() <= kPlausibleReach);
    }
  }

  // Les deux effets que la remontée avait répandus, et leurs porteurs mesurés.
  // ⚠ 368 EN A DEUX : l'état 107 vide toutes ses instances puis saute dans le
  // corps du 464, qui le repose — un redémarrage, pas une seconde source.
  // Et le rebâtissage le dit pareil : deux relevés qui dériveraient ne
  // tomberaient pas ensemble.
  for (const std::span<const ro::render::StateEffectEntry> table :
       {ro::render::StateEffectTable(), ro::render::StateRebuildEffectTable()}) {
    const std::map<std::uint16_t, std::set<std::uint16_t>> by_effect = ByEffect(table);
    CHECK(by_effect.at(1913) == std::set<std::uint16_t>{1196});
    CHECK(by_effect.at(368) == std::set<std::uint16_t>{107, 464});
  }
}

TEST_CASE("les dix etats dont l'effet est un DECALAGE") {
  // 🔴 CES DIX-LÀ N'ONT AUCUN NOMBRE ÉCRIT : le client calcule `état + 135`.
  // Un relevé qui ne lit que les littéraux les rend muets, sans rien signaler.
  constexpr std::uint16_t kTenOffset = 135;
  for (std::uint16_t state = 1016; state <= 1025; ++state) {
    const std::span<const ro::render::StateEffectEntry> own = ro::render::StateEffectsFor(state);
    INFO("état " << state);
    REQUIRE(own.size() == 1);
    CHECK(own[0].effect_id == state + kTenOffset);
  }
  // Et le voisin immédiat n'est PAS de la famille : le décalage s'arrête où le
  // `case` s'arrête, il ne se prolonge pas par régularité.
  CHECK(ro::render::StateEffectsFor(1026).empty());
}

TEST_CASE("les deux chemins des etats se jugent l'un l'autre") {
  // 🔴 LA MEILLEURE PREUVE QU'ON AIT SANS LANCER LE JEU : deux fonctions
  // écrites séparément, relevées séparément, doivent dire la même chose là où
  // elles parlent du même état. L'ancienne table ne le faisait qu'à moitié.
  const std::map<std::uint16_t, std::set<std::uint16_t>> pose =
      ByState(ro::render::StateEffectTable());
  const std::map<std::uint16_t, std::set<std::uint16_t>> rebati =
      ByState(ro::render::StateRebuildEffectTable());
  std::size_t communs = 0;
  std::size_t identiques = 0;
  std::size_t se_touchent = 0;
  for (const auto& [state, effects] : rebati) {
    const auto chez_la_pose = pose.find(state);
    if (chez_la_pose == pose.end()) {
      continue;
    }
    ++communs;
    identiques += chez_la_pose->second == effects ? 1U : 0U;
    se_touchent += std::ranges::any_of(effects,
                                       [&chez_la_pose](std::uint16_t effect_id) {
                                         return chez_la_pose->second.contains(effect_id);
                                       })
                       ? 1U
                       : 0U;
  }
  REQUIRE(communs >= 70);
  // Neuf dixièmes d'accord parfait : le reste tient à des états que l'un des
  // deux chemins montre autrement, pas à un relevé qui dérive.
  CHECK(identiques * 10 >= communs * 9);
  CHECK(se_touchent == communs);
}

TEST_CASE("l'instant d'un effet d'etat") {
  // 🔴 LE DRAPEAU « POSÉ » NE SE LIT PAS DANS LA CONDITION SEULE : le
  // répartiteur écrit `if (!posé) return;` puis la suite, et une lecture par
  // les seules BRANCHES rangeait cent trente semis sous « toujours » — donc les
  // faisait naître AUSSI à la chute de l'état.
  std::map<ro::render::StateEffectMoment, std::size_t> comptes;
  std::set<std::pair<std::uint16_t, std::uint16_t>> a_la_levee;
  for (const ro::render::StateEffectEntry& table_entry : ro::render::StateEffectTable()) {
    ++comptes[table_entry.moment];
    if (table_entry.moment == ro::render::StateEffectMoment::kOnRelease) {
      a_la_levee.emplace(table_entry.state_id, table_entry.effect_id);
    }
  }
  CHECK(comptes[ro::render::StateEffectMoment::kOnApply] >
        comptes[ro::render::StateEffectMoment::kAlways]);
  // Deux effets seulement naissent à la LEVÉE : un éclat de fin, pas une aura.
  CHECK(a_la_levee ==
        std::set<std::pair<std::uint16_t, std::uint16_t>>{
            {std::uint16_t{97}, std::uint16_t{656}}, {std::uint16_t{1339}, std::uint16_t{2303}}});
}

TEST_CASE("le paquet d'etat cree aussi, et il ne redit pas la pose") {
  // Six des onze effets du paquet sont ceux que la pose porte déjà : le client
  // les RETIRE PUIS LES REPOSE, ce qui fait une instance et non deux.
  const auto redit = [](const ro::render::StateEffectEntry& table_entry) {
    return std::ranges::any_of(ro::render::StateEffectsFor(table_entry.state_id),
                               [&table_entry](const ro::render::StateEffectEntry& pose) {
                                 return pose.effect_id == table_entry.effect_id;
                               });
  };
  CHECK(std::ranges::count_if(ro::render::StatePacketEffectTable(), redit) == 6);
  CHECK(ro::render::StatePacketEffectTable().size() == 11);
  // Et les quatre sons, chemins compris : le client les écrit en CP949, et un
  // décodage manqué les tronquait à « effect\ ». Le rebond est lu à la main.
  REQUIRE(ro::render::StateSoundTable().size() == 4);
  CHECK(ro::render::StateSoundFor(422) == "effect\\manhole.wav");
  CHECK(ro::render::StateSoundFor(134) == "effect\\T_벽튕김.wav");
  CHECK(ro::render::StateSoundFor(182) == "lava_golem_move.wav");
  CHECK(ro::render::StateSoundFor(0).empty());
}

TEST_CASE("un etat peut poser DEUX effets, et la plupart n'en posent aucun") {
  // EFST_EXPLOSIONSPIRITS — l'aura du moine en concentration : deux effets.
  const std::span<const ro::render::StateEffectEntry> esprits = ro::render::StateEffectsFor(86);
  REQUIRE(esprits.size() == 2);
  CHECK(esprits[0].effect_id == 166);
  CHECK(esprits[1].effect_id == 203);
  // EFST_TWOHANDQUICKEN — un seul, et c'est le même 166 que l'aura précédente :
  // un effet peut servir plusieurs états, la table n'est donc pas bijective.
  const std::span<const ro::render::StateEffectEntry> speed = ro::render::StateEffectsFor(2);
  REQUIRE(speed.size() == 1);
  CHECK(speed[0].effect_id == 166);
  // Et la grande majorité des états n'a rien à montrer de ce côté.
  CHECK(ro::render::StateEffectsFor(0).empty());
  CHECK(ro::render::StateEffectsFor(1).empty());
}

TEST_CASE("Incantation : Asura Strike d'un Champion montre ses propres glyphes",
          "[render][skill_cast]") {
  // 328 pour un Monk, 510 à partir de la classe 4016 : la table ne donne que
  // le cas général, l'exception par classe passe par `CastEffectForJob`.
  constexpr std::uint16_t kAsuraStrike = 271;
  constexpr std::int16_t kMonkGlyphs = 328;
  constexpr std::int16_t kChampionGlyphs = 510;
  constexpr std::uint16_t kMonkJob = 15;
  constexpr std::uint16_t kChampionJob = 4016;
  CHECK(ro::render::CastEffectForJob(kAsuraStrike, kMonkGlyphs, kMonkJob) == kMonkGlyphs);
  CHECK(ro::render::CastEffectForJob(kAsuraStrike, kMonkGlyphs, kChampionJob) == kChampionGlyphs);
}
