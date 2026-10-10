#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/number_look.h"

namespace ro::render {

// Le MOT qu'un coup écrit sur sa cible quand il n'y a pas de nombre à montrer.
//
// Le client ne l'écrit pas plus qu'il n'écrit ses nombres : c'est un sprite, de
// la taille d'une émoticône, qu'il accroche à la cible en sprite-enfant.
//
// ⚠ LE NOM DU FICHIER NE VIENT PAS DU CODE, il vient d'une table Lua. Le client
// lui demande les trois ressources de la « peau de dégâts » choisie et colle
// « %s.spr » et « %s.act » autour de celle du milieu. La peau par défaut,
// dans data\luafiles514\lua files\damageskin\damageskininfo.lub, donne :
//
//   BaseNumber = 이팩트\숫자      les chiffres de dégâts
//   BaseMsg    = 이팩트\msg       les marques — celles-ci
//   BaseBlue   = 이팩트\bluemsg   leur variante bleue
//
// Trois autres peaux existent (NewNumber, Han, Invi) et le joueur en change en
// jeu : les chiffres et les marques changent alors ENSEMBLE. Suivre la table
// plutôt que ce chemin est ce qu'il restera à faire.
//
// Ne pas confondre avec 이팩트\miss, une grande étoile rouge de 101 sur 85 —
// c'est un autre effet, et l'y prendre donne un astre en travers de l'écran.
inline constexpr std::string_view kDamageMarkSpritePath = "data\\sprite\\이팩트\\msg";

// La couleur d'une marque portée par un monstre ou un PNJ, puis par un joueur.
//
// ⚠ PAR SON PORTEUR, ET NON PAR LA CIBLE DU COUP : le client lit le JOB de
// l'acteur qui reçoit le message et teinte le sprite en conséquence — blanc
// pour un monstre ou un PNJ, rouge pour tout le reste. Comme le « Miss » naît sur celui
// qui a RATÉ, un joueur qui manque son coup le porte en rouge.
//
// Blanc veut dire SANS TEINTE : une marque qui porte déjà ses couleurs, comme
// la bulle de l'esquive, se rend telle quelle.
inline constexpr std::array<std::uint8_t, 3> kMarkOnOther = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kMarkOnPlayer = {255, 0, 0};

// L'ÉTOILE, ELLE, NAÎT GRISE, et ce n'est pas la cible qui le décide : le
// message qui pose le fichier teinte tout sprite-enfant de genre quinze en
// 0xFFAAAAAA (message 14, genres 15 et 25). La teinte est MULTIPLICATIVE — le
// quad reçoit `calque × teinte / 255` sur chaque canal —, si bien que l'étoile du fichier,
// qui est rouge vif, se rend assombrie d'un tiers.
inline constexpr std::array<std::uint8_t, 3> kMarkOnStar = {170, 170, 170};

// Ce que le coup écrit. Chaque valeur est une action du même fichier.
enum class DamageMark : std::uint8_t {
  // « Miss » : le coup n'a rien fait.
  //
  // C'est la seule condition que le client pose — dégâts NULS, quelle que soit
  // la sorte du coup —, et il pose alors sa marque au lieu d'un nombre. La
  // fonction qui la pose n'écrit aucune action, si bien que le sprite reste sur
  // la première (message 89 : le sprite naît, puis rejoint les enfants de
  // l'acteur).
  //
  // 🔴 IL NAÎT SUR CELUI QUI A RATÉ, pas sur sa cible : le répartiteur résout
  // l'acteur de l'ATTAQUANT et lui envoie le message 89 ; sans cet
  // acteur, rien ne paraît. La bulle de l'esquive fait l'inverse — elle va à
  // celui qui a esquivé. Voir `StrikePlan::mark_on_attacker`.
  //
  // ⚠ ELLE NE S'ANIME PAS. Le client la range dans un genre d'enfant — le
  // treizième — qui ne fait jamais avancer l'image : le mot part de sa place,
  // MONTE et PÂLIT, puis s'éteint au bout de quatre-vingts tics. Les neuf images que
  // porte son action ne servent donc à rien, et jouer l'animation ferait
  // clignoter le mot au lieu de le faire monter.
  kMiss,
  // « LUCKY!! » dans une bulle : une esquive chanceuse.
  //
  // Elle passe AVANT le coup à zéro, et l'exclut : le répartiteur des messages
  // de dégâts la reconnaît en tête et renvoie aussitôt, si bien qu'aucun nombre
  // ni aucune autre marque ne paraît. Le chemin est plus long que celui du
  // « Miss » — le traitement des dégâts traduit la sorte de coup 11 en
  // message 94, que le répartiteur retourne en message 95, qui demande la
  // RECETTE 13 au créateur de sprites-enfants : le même fichier, action trois.
  //
  // ⚠ ELLE S'ANIME, ET SON GENRE EST LE DIX-SEPTIÈME, PAS LE TREIZIÈME.
  // L'argument du créateur de sprites-enfants est un indice de RECETTE et non
  // un genre : chaque cas y pose le fichier, l'ACTION (trois) et le GENRE
  // (dix-sept), qui partent ensuite par deux messages distincts. Le genre
  // dix-sept fait AVANCER l'image et meurt au bout d'une passe, sans monter ni
  // pâlir.
  //
  // 🔴 ET LA GELER LA RÉDUIT À UN POINT : la première image de son action est
  // une bulle de six pixels sur cinq, sans texte — le mot « LUCKY!! » n'entre
  // qu'à la troisième, quand la bulle a fini de gonfler.
  kLucky,
  // L'ÉTOILE d'impact : un coup critique.
  //
  // Elle ne remplace pas le nombre, elle l'accompagne — c'est la seule marque
  // qui paraisse en même temps qu'un chiffre. Le répartiteur des messages de
  // dégâts pose l'une puis l'autre : message 92, qui demande la RECETTE 12 —
  // action deux, genre QUINZE —, puis le nombre jaune.
  //
  // ⚠ LE GENRE QUINZE VIT COMME UN NOMBRE JAILLISSANT, et c'est ce qui la
  // distingue des deux autres marques : elle naît quatre unités au-dessus des
  // pieds, décrit la MÊME cloche que le nombre qu'elle porte, DÉRIVE avec lui,
  // pâlit et meurt à soixante-dix tics — un comportement qu'il partage avec
  // les genres 14, 16 et 21.
  //
  // 🔴 SA TAILLE ET SA CADENCE SONT LES SIENNES, ET NON CELLES DU NOMBRE : elle
  // naît à l'échelle TROIS quand le nombre naît à cinq, rétrécit deux fois plus
  // lentement, s'arrête à un quand il s'arrête à un et trois dixièmes, et son
  // image tourne d'une DEMIE par tic — deux fois plus vite que ce que son
  // fichier déclare — pour se figer sur la dernière. Lui donner la pose du
  // nombre la ferait naître presque deux fois trop grosse et tourner deux fois
  // trop lentement.
  kCritical,
  // L'ÉTOILE D'UN COUP DE PIED : la même image que celle du critique — l'action
  // deux —, mais un autre genre, le VINGT-CINQUIÈME, et donc une autre vie.
  //
  // Le client la pose sous le PREMIER nombre montant qu'une poignée de
  // compétences font paraître sur un monstre — les coups de pied du Taekwon et
  // leurs cousins : voir `IsRisingSkill`. Elle ne tourne pas : son image est
  // remise à la PREMIÈRE à chaque tic. Elle monte avec le nombre, du même
  // dixième d'unité par tic depuis la même hauteur, vit cent vingt tics comme
  // lui, et pâlit comme lui à partir du quatre-vingt-dixième — mais depuis
  // deux cent dix, et de huit points par tic. Elle naît à l'échelle trois et
  // tombe à deux dès le premier tic, pour n'en plus bouger.
  //
  // ⚠ SA TEINTE EST REPOSÉE À CHAQUE TIC, et c'est un bleu pâle : cent
  // cinquante de rouge et de vert, le plein de bleu. Elle remplace le gris que
  // la naissance donne aux deux étoiles.
  kRisingStar,
};

// La sorte de coup que le serveur annonce pour une esquive chanceuse :
// DMG_LUCY_DODGE, dans e_damage_type de src/map/clif.hpp.
inline constexpr std::uint8_t kLuckyDodgeBlow = 11;

// L'action du fichier que cette marque joue.
//
// Le fichier en porte cinq : « Miss », « Guard », l'étoile d'impact, la bulle
// « LUCKY!! », puis un second « Miss » d'une autre animation. Trois d'entre
// elles ont leur déclencheur ici ; le « Guard » n'est pas une marque de coup —
// son message va par paires création/retrait, ce qui est la signature d'un
// effet attaché à un ÉTAT.
[[nodiscard]] std::size_t DamageMarkAction(DamageMark mark);

// Vrai pour la marque de genre treize — le mot « Miss », le seul à rester sur sa
// première image, à monter et à pâlir. Les deux autres jouent leur animation.
[[nodiscard]] constexpr bool IsStillMark(DamageMark mark) noexcept {
  return mark == DamageMark::kMiss;
}

// Le tic de l'horloge des marques, en millisecondes : celui de tous les
// sprites-enfants du client.
inline constexpr float kMarkTickMs = 24.0F;

// Combien de tics le mot « Miss » vit, de combien d'UNITÉS DU MONDE il monte à
// chaque tic — sept pixels de sprite l'unité, voir `floating_number.h` —, et
// combien d'opacité il perd. Ce sont ceux du genre treize.
inline constexpr float kMissLifeTicks = 80.0F;
inline constexpr float kMissUnitsPerTick = 0.54F;
inline constexpr float kMissFadePerTick = 3.0F;

// ── L'ÉTOILE, genre quinze ──────────────────────────────────────────────────
//
// Combien de tics elle vit — autant que la cloche qu'elle accompagne —, de
// combien d'images elle tourne par tic, et de combien d'opacité elle perd,
// depuis le PLEIN et non depuis les deux cent cinquante du mot.
//
// Son échelle naît à trois (message 14, genres 15 et 25)
// et décroît des vingt-quatre centièmes par tic de la cloche, RALENTIS de six
// dixièmes, jusqu'au plancher d'un — celui du nombre étant d'un et deux ou
// trois dixièmes. C'est le seul endroit où ce ralentissement paraisse, et il
// s'écrit comme le client le calcule.
inline constexpr float kStarLifeTicks = 70.0F;
inline constexpr float kStarFramesPerTick = 0.5F;
inline constexpr float kStarFadePerTick = 3.45F;
inline constexpr float kStarInitialScale = 3.0F;
inline constexpr float kStarFinalScale = 1.0F;
inline constexpr float kStarShrinkSlowdown = 0.6F;
inline constexpr float kStarShrinkPerTick = 0.24F * kStarShrinkSlowdown;

// ── L'ÉTOILE D'UN COUP DE PIED, genre vingt-cinq ────────────────────────────
//
// Sa vie, celle du nombre montant qu'elle porte ; son échelle, qui tombe de
// trois à deux dès le premier tic ; son opacité, pleine à la naissance puis
// de deux cent dix, qu'elle perd de huit points par tic à partir du
// quatre-vingt-dixième ; et la teinte que chaque tic lui repose.
inline constexpr float kRisingStarLifeTicks = 120.0F;
inline constexpr float kRisingStarBirthScale = 3.0F;
inline constexpr float kRisingStarScale = 2.0F;
inline constexpr float kRisingStarAlpha = 210.0F;
inline constexpr float kRisingStarFadeStartTicks = 90.0F;
inline constexpr float kRisingStarFadePerTick = 8.0F;
inline constexpr std::array<std::uint8_t, 3> kMarkOnRisingStar = {150, 150, 255};

// Combien de temps la marque vit, étirée de `stretch`.
//
// Le mot et l'étoile ont chacun la durée de LEUR genre, qui ne vient pas du
// fichier — quatre-vingts tics, soixante-dix ; seule la bulle joue son animation
// entière, UNE fois, et meurt avec elle. Zéro si le fichier n'a pas cette
// action, auquel cas rien ne se dessine.
[[nodiscard]] float DamageMarkLifeMs(const formats::ActAnimation& animation, DamageMark mark,
                                     float stretch = 1.0F);

// LE TEMPS D'UNE ÉTOILE, PRIS AU NOMBRE QU'ELLE ACCOMPAGNE. Les mots n'en ont
// pas : ils gardent la vie du client.
//
// `stretch` étire son animation comme la vie réglée du nombre étire celle du
// client ; `birth_ratio` la grandit à la naissance comme la naissance réglée
// grandit la sienne, et revient à un en `birth_settle_ms`.
struct MarkPace {
  float stretch = 1.0F;
  float birth_ratio = 1.0F;
  float birth_settle_ms = 0.0F;
};

// La marque à cet instant, en pixels de sprite autour des pieds de la cible.
// Vide une fois éteinte.
//
// ⚠ ELLE SUIT LES CURSEURS COMMUNS DES NOMBRES — l'échelle la grandit, l'opacité
// la voile — et la trajectoire de son type la déplace. Le mot monte en plus de
// lui-même, comme chez le client.
//
// 🔴 L'ÉTOILE N'A ICI QUE SA TAILLE, SON IMAGE ET SON OPACITÉ : sa place, elle,
// est celle du nombre qu'elle accompagne, et c'est `ComposeNumberStar` qui l'y
// porte. Lui donner en plus une hauteur ou une arrivée la déplacerait deux fois.
[[nodiscard]] ComposedActor ComposeDamageMark(const formats::SprSprite& sprite,
                                              const formats::ActAnimation& animation,
                                              DamageMark mark,
                                              const std::array<std::uint8_t, 3>& color,
                                              float elapsed_ms, const NumberLook& look = {},
                                              const MarkPace& pace = {});

}  // namespace ro::render
