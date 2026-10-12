#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/render/floating_number.h"
#include "ro/render/sprite_catalog.h"

namespace ro::render {

// Le BRUIT d'un coup porté : l'impact d'une arme sur ce qu'elle touche.
//
// Il ne vient pas de l'animation — le .act d'un personnage n'en porte pas —
// mais du code du client, qui le choisit sur QUI FRAPPE et AVEC QUOI, depuis
// le répartiteur des messages de dégâts.
//
// Quatre cas, et quatre seulement :
//
//   une créature      quatre bruits sourds, tirés au hasard
//   un joueur à mains nues   quatre bruits de poing, tirés au hasard
//   un joueur armé    le bruit de sa CLASSE d'arme, dans une table du client
//   une COMPÉTENCE    les quatre bruits sourds pour presque toutes les sortes
//                     de coup, quoi que tienne le lanceur — mais quatre sortes
//                     sonnent comme un coup ordinaire, voir
//                     `SkillBlowUsesEnemySounds`
//
// Un coup MANQUÉ ne sonne pas : le répartiteur écarte les dégâts nuls avant
// d'arriver au choix du son.

// Les quatre bruits d'une créature qui frappe, et les quatre d'un poing.
inline constexpr std::array<std::string_view, 4> kEnemyHitSounds = {
    "_enemy_hit_normal1.wav", "_enemy_hit_normal2.wav", "_enemy_hit_normal3.wav",
    "_enemy_hit_normal4.wav"};
inline constexpr std::array<std::string_view, 4> kFistHitSounds = {
    "_hit_fist1.wav", "_hit_fist2.wav", "_hit_fist3.wav", "_hit_fist4.wav"};

// Le bruit de chaque classe d'arme est le `hit_sound` de weapon_sprites.yml,
// que le catalogue des sprites lit déjà (`SpriteTables::weapon_hit_sounds`).

// ── LE BRUIT D'UN JOUEUR QU'ON FRAPPE ───────────────────────────────────────
//
// 🔴 CELUI-CI N'EST PAS DANS UN LUA : le client le porte en dur, dans un
// vecteur rempli au démarrage — voir `class_hit_sound_table.h`. Le chercher
// dans les tables du client fait retomber sur les quatre bruits sourds : un
// joueur frappé sonnerait comme un monstre, sans le `player_clothes.wav`
// qu'on entend à chaque coup.
//
// LES BORNES SONT CELLES DU CLIENT :
// une classe de 31 à 999, de 1001 à 3998, de 20021 à 31999, ou négative, ne
// passe PAS par cette table — elle retombe sur celle des armes, et sur les
// quatre bruits sourds quand celle-ci ne répond rien.
inline constexpr std::int32_t kLastBasicClass = 30;
inline constexpr std::int32_t kNoviceHighClass = 1000;
inline constexpr std::int32_t kFirstAdvancedClass = 3999;
// Au-delà de cette classe, l'index du vecteur est `classe − 3950`.
inline constexpr std::int32_t kAdvancedClassIndexBase = 3950;
// La plage que le client écarte en bloc, telle qu'il l'écrit : les classes de
// monture et de costume.
inline constexpr std::int32_t kFirstShownOnlyClass = 20021;
inline constexpr std::int32_t kShownOnlyClassCount = 0x2ECA + 1;

// Vrai quand cette classe lit son bruit dans la table des classes.
[[nodiscard]] bool ClassHasHitSound(std::int32_t job) noexcept;

// LE BRUIT D'UN JOUEUR DE CETTE CLASSE QU'ON FRAPPE, ou une chaîne VIDE quand
// sa classe sort des bornes ci-dessus. Presque toutes rendent le défaut.
[[nodiscard]] std::string_view ClassHitSound(std::int32_t job);

// La classe d'arme d'un combattant à MAINS NUES.
inline constexpr std::uint32_t kBareHandsWeapon = 0;

// LE BRUIT DE L'ARME QUI TOUCHE, ou une chaîne VIDE quand la table du client ne
// dit rien de ce seau.
//
// 🔴 DEUX TRADUCTIONS SÉPARENT L'APPARENCE DE LA TABLE, et les sauter rend le
// coup MUET. Ce que le serveur envoie est l'IDENTIFIANT DE L'OBJET ; le client
// en tire la classe d'arme, puis le SEAU d'animation de cette classe, et c'est
// par ce seau qu'il indexe sa table des bruits — trente entrées, là où un
// bâton porte l'objet 1601.
//
// `draw` choisit parmi les quatre bruits d'un même jeu : le client y tire au
// hasard, et nous prenons un nombre pour que le choix se rejoue à l'identique.
// ⚠ UN OBJET NÉGATIF DIT « SOURCE INTROUVABLE », et ce n'est pas un cas
// d'erreur : le client garde sa classe d'arme à MOINS UN quand il
// ne retrouve pas l'acteur qui frappe, ce qui rend les quatre bruits sourds.
// C'est le cas de TOUTE unité posée au sol, qui ne vit pas dans sa liste
// d'acteurs — le blizzard de Storm Gust sonne par là.
[[nodiscard]] std::string WeaponHitSound(const SpriteCatalog& catalog, std::int32_t weapon_item,
                                         std::uint32_t draw);

// Le bruit d'un coup ORDINAIRE porté sur quelqu'un.
//
// ⚠ C'EST LA CIBLE QUI DÉCIDE, ET NON QUI FRAPPE : le client lit le métier
// d'affichage de la CIBLE. Un joueur touché sonne de sa CLASSE,
// tout le reste sonne de l'ARME qui le touche — d'où `target_job`, qui ne sert
// que dans le premier cas.
[[nodiscard]] std::string AttackSound(const SpriteCatalog& catalog, bool target_is_player,
                                      std::int32_t target_job, std::int32_t attacker_weapon,
                                      std::uint32_t draw);

// LES SORTES DE COUP QUI SONNENT COMME UN COUP ORDINAIRE, MÊME PORTÉES PAR UNE
// COMPÉTENCE.
//
// 🔴 LE NUMÉRO DE LA COMPÉTENCE NE DÉCIDE DE RIEN ICI. Ce que le client regarde
// est la SORTE du coup — `e_damage_type`, ce que le serveur envoie —, dont il
// tire l'identifiant interne de son message de dégâts :
//
//   sorte 4 encaissé, 9 multiple-encaissé        → message 39
//   sorte 10 critique                            → message 93
//   sorte 13 multiple-critique                   → message 148
//   sorte 6 simple                               → message 97
//   tout le reste, dont 8 à coups multiples      → message 96
//
// Et SEULS les messages 96 et 97 posent la classe d'arme à moins un pour aller
// droit aux quatre bruits sourds. Les trois autres rejoignent le
// corps commun des messages de dégâts, celui qui lit le métier de la CIBLE —
// donc `player_clothes.wav` sur un joueur.
inline constexpr std::array<std::uint8_t, 4> kPlainSoundingDamageTypes = {4, 9, 10, 13};

// 🔴 ET LE CUMUL D'UN COUP MULTIPLE REQUALIFIE LE MESSAGE EN 111, ce qui rend
// la sorte muette : 111 n'est ni 96 ni 97, donc il rejoint lui aussi le corps
// commun. Quand le compte de coups annoncé n'est pas un — borné à [1, 36]
// comme le client le borne —, le répartiteur des compétences envoie la PART
// en dégâts et le CUMUL en argument supplémentaire, et le message 27 qui le
// porte devient 111.
//
// Ainsi un Cold Bolt de High Wizard sur un High Wizard arrive en sorte 8, que
// la table ci-dessus range en 96 — mais le message délivré est 111, et c'est
// le bruit de la classe 4010 qui sonne.

// Vrai quand ce coup de compétence prend les quatre bruits sourds.
//
// ⚠ LE COMPTE DE COUPS EST AUSSI DÉCISIF QUE LA SORTE : sans lui, toute
// compétence à coups multiples — et elles sont deux cent soixante-sept sur ce
// serveur — sonne comme un monstre sur un joueur.
[[nodiscard]] bool SkillBlowUsesEnemySounds(std::uint8_t damage_type,
                                            std::uint16_t announced_hits) noexcept;

// LE BRUIT D'UN COUP DE COMPÉTENCE, à chaque message de dégâts : le client
// demande la classe d'arme MOINS UN pour ses messages 96 et 97,
// ce qui rend l'un des quatre bruits sourds des créatures, quoi que tienne le
// lanceur — dix coups de Cold Bolt, dix impacts. `draw` tire comme ci-dessus.
//
// ⚠ IL NE VAUT QUE POUR CES DEUX MESSAGES : la sorte du coup se demande d'abord
// à `SkillBlowUsesEnemySounds`, sans quoi un critique de compétence perd le
// bruit de la classe qu'il frappe.
[[nodiscard]] std::string SkillBlowSound(std::uint32_t draw);

}  // namespace ro::render
