#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ro/render/effect_sound_table.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

#include "survivors/survivors_gems.h"

// LA TABLE D'APPARENCE DES ACTEURS DU MODE SURVIVORS : ce que la simulation
// nomme par des identifiants (espèce, objet, classe) devient une apparence que
// la garde-robe habille. La simulation n'en sait rien ; `app` seul la lit.
namespace moonclient {

// Un monstre ou une plante : un sprite unique, nommé par sa classe.
[[nodiscard]] ro::render::ActorLook FoeLook(std::uint16_t foe_id) noexcept;

// Un objet au sol, gemme comprise : le sprite que la table des objets donne.
[[nodiscard]] ro::render::ActorLook ItemLook(std::uint16_t item_id) noexcept;

// LA CARTE AU SOL (Q6) : le sprite RO d'une carte posée, le même pour toutes.
// La Poring Card le porte comme les cartes de Gravity ; une carte dont la
// fiche nomme une autre ressource (la Mastering Card du serveur) ne se
// dessine pas autrement.
inline constexpr std::uint16_t kGroundCardItem = 4001;

// L'objet qui montre un objet au sol : le sien, la carte au sol pour une carte.
[[nodiscard]] std::uint16_t PickupLookOf(const ro::sim::SurvivorsPickup& pickup) noexcept;

// LE HÉROS (U7) : la tête, la coiffure, les coiffes, la cape et les couleurs
// du personnage (`player`), sur le corps de la classe du mode et son arme ; le
// corps passe à celui de l'évolution dès la première (`evolved`).
//
// ⚠ CE QUI CHANGERAIT LE CORPS EST RETIRÉ : costume, monture, habit de GM,
// combinaison, tête d'orc et bouclier sont ceux du personnage, pas du mode.
[[nodiscard]] ro::render::ActorLook HeroLook(const ro::render::ActorLook& player,
                                             const ro::sim::ClassRule& hero_class,
                                             bool evolved) noexcept;

// ── LA TABLE D'APPARENCE DES SKILLS ─────────────────────────────────────────

// LES TROIS REPLIS ÉCRITS : ces skills ne montrent rien par les tables du
// client, parce que leur effet y naît d'un état (Sight), d'une suite sans
// dégâts (Napalm Beat) ou d'une unité posée (Fire Wall, l'unité 127).
inline constexpr std::uint16_t kSightSkill = 10;
inline constexpr std::uint16_t kNapalmBeatSkill = 11;
inline constexpr std::uint16_t kFireWallSkill = 18;
inline constexpr std::uint32_t kSightFallbackEffect = 22;
inline constexpr std::uint32_t kNapalmBeatFallbackEffect = 32;
inline constexpr std::uint32_t kFireWallFallbackEffect = 25;

// L'UNITÉ QU'UN SKILL DE ZONE POSE AU SOL dans le jeu : la zone du mode en
// montre le visuel entretenu (`SkillLook::ground_effect_id`) tant qu'elle dure.
// Fire Pillar pose le pilier en attente, dont le cercle rouge marque la case.
inline constexpr std::uint16_t kFirePillarSkill = 80;
inline constexpr std::uint32_t kFirePillarWaitingUnit = 135;

// L'EFFET D'UNE ÉVOLUTION : le changement de métier, sur le héros.
inline constexpr std::uint32_t kJobChangeEffect = 156;

// L'ÉCLAT D'UNE CARTE RAMASSÉE (Q6) : l'étincelle du soin, brève, sur le
// héros qui vient de la prendre.
inline constexpr std::uint32_t kCardPickupEffect = 309;

// L'ÉTOILE DE COUP : la gerbe ordinaire, que la table des impacts donne par
// défaut, et la seule que « Effets réduits » garde.
inline constexpr std::uint32_t kPlainHitEffect = 0;

// OÙ L'EFFET D'UN SKILL NAÎT.
//
// 🔴 JAMAIS SUR UN MONSTRE : la horde n'entre pas dans le monde des effets. Ce
// que le client fait naître sur la cible se pose au sol, là où elle se tient.
enum class SkillBearer : std::uint8_t {
  // Porté par le héros, qu'il suit.
  kHero,
  // Posé au sol, au point que la nouvelle nomme.
  kGround,
};

struct SkillLook {
  std::uint16_t skill_id = 0;
  // L'effet du sort, toujours connu pour un skill des règles livrées.
  std::uint32_t effect_id = 0;
  SkillBearer bearer = SkillBearer::kGround;
  // L'effet vient d'un des trois replis écrits, et non des tables.
  bool fallback = false;
  // L'IMPACT sur un monstre touché : la gerbe ordinaire quand la table n'en
  // nomme pas d'autre. « Effets réduits » le remplace par l'étoile de coup.
  std::uint32_t hit_effect_id = 0;
  // CE QUE SA ZONE MONTRE AU SOL tant qu'elle dure, sous les flammes : le
  // visuel entretenu de l'unité que le skill pose dans le jeu, ou zéro.
  std::uint32_t ground_effect_id = 0;
  // Le skill dont l'icône se montre : le sien.
  std::uint16_t icon_skill_id = 0;
};

// L'apparence d'un skill des règles : l'effet du coup, sinon celui de la table
// ordinaire, sinon celui de l'annonce, sinon le repli écrit. Où il naît se lit
// sur l'archétype : une orbite, un projectile qui part du héros et une nova
// sont portés par lui, tout le reste se pose au sol. L'effet au sol d'une zone
// est celui que la table des unités entretient pour l'unité du skill.
[[nodiscard]] SkillLook SkillLookOf(const ro::sim::SurvivorsSkillRule& skill);

// LES FICHIERS QU'UN LANCER FAIT ENTENDRE, variantes tirées de `draw` : ceux de
// la compétence, sinon ceux de son effet. Rien pour un sort qui n'en a aucun :
// il sonne à l'impact (`SkillThudsOnHit`).
//
// 🔴 UN NOM À VARIANTES SE FORME ICI : le `%d` qu'il porte à la place du numéro
// ne désigne aucun fichier, et un bolt joué sous ce nom resterait muet.
//
// ⚠ RIEN POUR UN EFFET DONT LA TROUPE SONNE : chaque pièce semée fait son bruit
// (`ArenaEffects::TakeSounds`), et en ajouter un au lancer serait un doublon —
// voir `ro::render::EffectSoundPlaysAtSowing`.
[[nodiscard]] std::vector<std::string> SkillSoundFilesOf(const SkillLook& look, std::uint32_t draw);

// Vrai pour un sort sans bruit, ni à lui, ni à son effet, ni à la troupe de son
// effet : chaque coup qu'il porte fait entendre le coup sourd du jeu à
// l'impact d'une compétence (`ro::render::SkillBlowSound`), et un lancer qui
// ne touche rien reste muet.
[[nodiscard]] bool SkillThudsOnHit(const SkillLook& look);

}  // namespace moonclient
