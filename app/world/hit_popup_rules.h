#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/damage_mark.h"
#include "ro/render/floating_number.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"

#include "world/hit_popup_look.h"

namespace moonclient {

// CE QUE LE RÉGLAGE FAIT D'UN COUP : ce qui se montre, de quelle couleur, à
// quelle allure, en combien de nombres, et où.
//
// 🔴 DES FONCTIONS PURES, SANS MONDE NI SPRITE, et c'est ce qui les rend
// essayables : la priorité des couleurs, l'interrupteur qui tait les coups des
// autres, le partage d'un coup multiple sont trois règles muettes en jeu — un
// nombre de la mauvaise couleur ne se voit qu'à l'œil, et un nombre absent ne
// se voit pas du tout. C'est aussi là que vit CHAQUE règle qui décide d'un
// nombre : `HitPopups` ne fait plus que les poser et les faire vivre.

// De quel côté du coup est le joueur, et d'où il part. ⚠ DES FAITS ET NON DEUX
// ACTEURS : ce que la règle a besoin de savoir, et rien de ce qu'il faudrait un
// monde pour donner.
struct StrikeSides {
  bool target_is_player = false;
  bool target_is_self = false;
  bool attacker_is_self = false;
  // CE QU'EST L'ATTAQUANT, parce que le mot « Miss » naît sur LUI et prend SA
  // couleur — voir `StrikePlan::mark_on_attacker`.
  //
  // « Connu » veut dire : le monde a son acteur. Le client cherche l'acteur du
  // GID que porte le coup et RENONCE quand il ne le trouve pas ; un coup venu d'une unité au
  // sol ou d'un acteur hors de vue ne laisse donc aucun mot.
  bool attacker_is_known = false;
  bool attacker_is_player = false;
  // Vrai quand le coup part d'une UNITÉ POSÉE AU SOL — le blizzard de Storm
  // Gust, la pluie de météores — et non d'un acteur. Le client délivre ce
  // coup-là à un autre répartiteur, celui des unités au sol, qui
  // porte le cumul À CHAQUE FOIS et découpe le montant sur le nombre de coups
  // annoncé sans regarder la sorte du coup.
  bool attacker_is_ground_unit = false;
  // LE MÉTIER DE L'ATTAQUANT, ou zéro quand il n'est pas connu : il décide si la
  // seconde main d'un coup ordinaire paraît. Voir `ro::render::ShowsSecondHand`.
  std::uint32_t attacker_job = 0;
  // Vrai quand la cible est le joueur et qu'il porte un des états qui changent
  // son « Miss » en bouclier : voir `MissShowsGuardShield`.
  bool target_carries_miss_guard = false;
};

// LE BOUCLIER QUI REMPLACE LE « MISS » : l'effet 336.
inline constexpr std::uint32_t kMissGuardShieldEffect = 336;

// Vrai quand ces états changent le « Miss » de leur porteur en bouclier :
// Kyrie Eleison et Parrying.
[[nodiscard]] bool CarriesMissGuard(const ro::sim::Actor& actor) noexcept;

// VRAI QUAND UN COUP MANQUÉ MONTRE LE BOUCLIER AU LIEU DU MOT « MISS ».
//
// Un coup à zéro dégât qu'un MONSTRE porte au joueur, pendant que le joueur
// porte Kyrie Eleison ou Parrying : le client envoie alors à la cible, et non
// à l'attaquant, le message qui fait naître l'effet 336 sur elle — et aucun
// mot ne paraît. Kyrie absorbe ainsi le coup sous les yeux du joueur. Un
// joueur qui rate garde son « Miss » rouge : le client ne consulte les états
// que pour un attaquant dont la CLASSE est celle d'une créature ou d'un PNJ —
// ce qu'un homoncule ou un mercenaire n'est pas.
//
// ⚠ SEUL LE JOUEUR : le client ne connaît que SES propres états.
[[nodiscard]] bool MissShowsGuardShield(const ro::sim::Strike& strike,
                                        const StrikeSides& sides) noexcept;

// Les trois octets d'une couleur ABGR, et sa transparence en facteur de zéro à
// un.
[[nodiscard]] std::array<std::uint8_t, 3> RgbOf(std::uint32_t abgr) noexcept;
[[nodiscard]] float OpacityOf(std::uint32_t abgr) noexcept;

// Vrai si le réglage laisse paraître ce coup, d'après qui frappe qui. Un coup
// qu'on se porte à soi-même est un coup REÇU.
[[nodiscard]] bool StrikeShown(const HitPopupLook& look, const StrikeSides& sides) noexcept;

// La couleur qu'un coup prend d'après ses deux côtés, hors critique et magie :
// voir l'ordre dans `HitPopupLook`.
[[nodiscard]] std::uint32_t TargetColor(const StrikeSides& sides,
                                        const HitPopupLook& look) noexcept;

// La couleur du mot « Miss » : celle de CELUI QUI LE PORTE, l'attaquant.
//
// Le client la choisit sur le JOB de l'attaquant — blanc pour un monstre ou un
// PNJ, rouge pour tout le reste (0xFFFFFFFF ou 0xFFFF0000 en ARGB). C'est la même règle que
// `TargetColor`, appliquée à l'autre bout du coup.
[[nodiscard]] std::uint32_t MissColor(const StrikeSides& sides, const HitPopupLook& look) noexcept;

// Ce qu'un coup fait paraître.
struct StrikePlan {
  // Faux : rien ne paraît — le réglage l'écarte, ou le coup n'a rien à montrer.
  bool shown = false;
  // Le MOT, quand le coup n'entame pas ; vide quand c'est un nombre.
  std::optional<ro::render::DamageMark> mark;
  // 🔴 QUI PORTE LE MOT — et les deux mots ne le portent pas au même bout.
  //
  // Le « Miss » naît sur CELUI QUI A RATÉ : le client envoie son message 89 à
  // l'ATTAQUANT, qui accroche le sprite. La bulle « LUCKY!! » naît sur
  // celui qui a ESQUIVÉ, donc sur la cible — message 95, envoyé à la cible
  // elle-même. Les poser toutes deux sur la cible ferait paraître un « Miss »
  // blanc sur le monstre au lieu du rouge sur le joueur.
  bool mark_on_attacker = false;
  // La couleur du mot ou du nombre, l'allure et la transparence du nombre. Sa
  // valeur, elle, vient de `StrikeValueAt` : elle change d'un nombre à l'autre.
  ro::render::FloatingNumber number;
  // LE TYPE DU NOMBRE — ou du mot —, qui choisit sa trajectoire. Le total, à
  // côté, est de `DamagePath::kTotal`.
  DamagePath path = DamagePath::kOnMonster;
  // L'étoile d'impact derrière le nombre : un critique, et l'étoile est voulue.
  bool star = false;
  // Combien de nombres naissent, et comment ils se partagent le montant.
  std::uint16_t count = 1;
  HitSplit split = HitSplit::kCumulative;
  // LE CUMUL À CÔTÉ DE CHAQUE PART, quand le partage le veut et que la cible
  // est un monstre : sa couleur et son allure, la valeur venant de
  // `StrikeTotalAt`. Vide sinon.
  std::optional<ro::render::FloatingNumber> total;
  // L'ÉTOILE D'UN COUP DE PIED sous le PREMIER nombre, et sous lui seul : voir
  // `IsRisingSkill`. Les nombres suivants montent sans elle.
  bool rising_star = false;
  // PAR COMBIEN LE MONTANT DU FIL SE DIVISE pour faire la part — qui n'est pas
  // toujours le nombre de nombres qui paraissent.
  //
  // 🔴 L'IMPACT D'UNE COMPÉTENCE DIVISE PAR LE COMPTE ANNONCÉ, QUELLE QUE SOIT
  // LA SORTE : un coup encaissé qui annonce trois frappes n'écrit qu'UN nombre,
  // et ce nombre est le tiers du montant. Le reste ne paraît nulle part. Voir
  // `SkillImpactDivides`.
  std::uint16_t divisor = 1;
  // LA SECONDE MAIN : un nombre de plus, qui écrit les dégâts de la main
  // gauche, et qui change le pas des frappes. Voir `ro/render/second_hand.h`.
  bool second_hand = false;
};

// L'instant du nombre de rang `index` depuis le premier, pour un pas `step_ms`.
[[nodiscard]] std::uint32_t StrikeHitOffsetMs(const StrikePlan& plan, std::uint16_t index,
                                              std::uint32_t step_ms) noexcept;

// ── LES COMPÉTENCES QUE LE CLIENT MONTRE AUTREMENT ──────────────────────────
//
// Deux listes écrites en dur dans l'impact d'une compétence, et qui changent
// ce qu'un coup fait paraître sur un MONSTRE. Toutes deux ne valent qu'aux
// sortes de coup que l'impact traite par cette voie — encaissé, compétence,
// multiple, multiple encaissé, multiple critique (4, 6, 8, 9 et 13) — et pour
// un coup porté par un acteur, jamais par une unité au sol.

// LES COUPS DE PIED, et les compétences que le client range avec eux : chaque
// frappe écrit la SOMME des frappes déjà portées — un tiers, deux tiers, le
// tout —, en un seul nombre qui MONTE, de la couleur de la cible ; sans part à
// côté, sans total jaune, et même quand le coup est critique. Le premier
// nombre porte une étoile. Sur un joueur, rien ne change : les parts, en rouge.
[[nodiscard]] bool IsRisingSkill(std::uint16_t skill_id) noexcept;

// LES COMPÉTENCES QUI PORTENT LEUR TOTAL MÊME SEULES : le client passe à leur
// premier nombre le cumul qu'il ne passe d'ordinaire qu'aux coups multiples,
// si bien qu'une frappe unique montre DEUX nombres du même montant — la part
// qui jaillit et le total jaune qui monte —, comme sous une unité au sol.
[[nodiscard]] bool CarriesTotalOnEveryHit(std::uint16_t skill_id) noexcept;

// VRAI QUAND LE COUP PASSE PAR L'IMPACT D'UNE COMPÉTENCE, qui le découpe à sa
// façon : une compétence, portée par un acteur — jamais par une unité au sol —,
// d'une des sept sortes que l'impact traite (encaissé, éclaboussure,
// compétence, multiple, multiple encaissé, multiple critique, éclaboussure
// encaissée).
//
// Ce chemin-là divise TOUJOURS le montant par le compte annoncé, ramené à un
// hors d'un à trente-six, et ne fait paraître plusieurs nombres que pour les
// sortes multiples ; celui des coups ordinaires ne divise que ce qu'il montre
// en plusieurs nombres. Et il ne teint jamais un nombre en bleu : l'octet
// « magie » du message de dégâts n'y est pas posé.
[[nodiscard]] bool SkillImpactDivides(const ro::sim::Strike& strike,
                                      const StrikeSides& sides) noexcept;

// VRAI QUAND LE COUP DONNE LIEU À DES MESSAGES DE DÉGÂTS — nombres, mots, bruits
// de frappe et lignes de combat. Toujours pour un coup ordinaire et pour celui
// d'une unité au sol ; pour une compétence portée par un acteur, seulement
// quand le client connaît cet acteur — l'impact est un message qu'il lui envoie
// — et que la sorte est l'une des sept que l'impact traite. Aux autres sortes,
// l'impact rend la main sans rien écrire.
[[nodiscard]] bool StrikeReachesDamageMessages(const ro::sim::Strike& strike,
                                               const StrikeSides& sides) noexcept;

[[nodiscard]] StrikePlan PlanStrike(const ro::sim::Strike& strike, const StrikeSides& sides,
                                    const HitPopupLook& look);

// ── LE TOTAL QUI SURVIT D'UN COUP À L'AUTRE ─────────────────────────────────

// Ce qu'une cible a déjà encaissé, et quand : l'état que le total continu
// demande, et le seul de toute la section — les autres règles sont pures.
struct TargetTally {
  std::uint32_t total = 0;
  std::uint32_t last_ms = 0;
};

// La somme dont repart un coup qui touche à `lands_ms` : celle que la cible
// porte déjà, ou zéro quand son dernier coup est trop vieux pour qu'on s'en
// souvienne. Un coup qui touche AVANT le précédent appartient à la même rafale
// — les durées d'animation les rendent dans le désordre —, et n'oublie rien.
[[nodiscard]] std::uint32_t CarriedTotal(const TargetTally& tally, std::uint32_t lands_ms,
                                         float forget_ms) noexcept;

// Ce que le nombre de rang `index` écrit, d'après le partage choisi.
[[nodiscard]] std::uint32_t StrikeValueAt(const ro::sim::Strike& strike, const StrikePlan& plan,
                                          std::uint16_t index) noexcept;

// La somme des coups portés jusqu'au rang `index` compris : ce que le cumul
// écrit à côté de la part de ce rang.
[[nodiscard]] std::uint32_t StrikeTotalAt(const ro::sim::Strike& strike, const StrikePlan& plan,
                                          std::uint16_t index) noexcept;

// Le nombre d'une compétence sans dégâts, ou rien : deux compétences seulement
// en portent un — le soin et la récupération de magie —, et le réglage peut les
// taire. La valeur reste à poser par l'appelant.
[[nodiscard]] std::optional<ro::render::FloatingNumber> SupportNumber(std::uint16_t skill_id,
                                                                      const HitPopupLook& look);

// Le nombre d'un effet à valeur, ou rien : dix numéros seulement en font
// paraître un. Les deux qui nomment ce qu'ils comptent prennent les couleurs du
// soin et de la magie rendue ; les huit qui ne nomment qu'une couleur gardent
// la leur, choisie par le script du serveur.
[[nodiscard]] std::optional<ro::render::FloatingNumber> EffectNumber(std::uint32_t effect_id,
                                                                     const HitPopupLook& look);

// Ce que le chiffre d'un effet à valeur écrit. Le sprite des chiffres n'a pas
// de signe, et le client ramène un montant négatif à ZÉRO avant de l'écrire —
// un montant nul s'écrit donc « 0 », et ne se tait pas.
[[nodiscard]] std::uint32_t ShownEffectAmount(std::int32_t value) noexcept;

// ── LE POINT D'ARRIVÉE ──────────────────────────────────────────────────────

// L'ÉCART AU HASARD d'un nombre autour du point d'arrivée de sa trajectoire, en
// pixels de sprite — x vers la droite, y vers le haut. Nul sans dispersion.
//
// 🔴 TIRÉ D'UN HACHAGE, PAS D'UN GÉNÉRATEUR. Le même germe rend le même écart :
// un nombre ne saute pas d'une image à l'autre, et la règle s'essaie. Le germe
// est ce qui IDENTIFIE le nombre — voir `ScatterSeed` —, jamais l'instant du
// dessin.
[[nodiscard]] std::array<float, 2> ScatterOf(const NumberPath& path, std::uint64_t seed) noexcept;

// Le germe d'un nombre : sa cible, l'instant de sa naissance, et son rang dans
// un coup multiple — trois nombres nés d'un même coup ne tombent pas au même
// endroit.
[[nodiscard]] std::uint64_t ScatterSeed(std::uint32_t target_id, std::uint32_t born_ms,
                                        std::uint16_t index) noexcept;

// ── L'ORDRE DE DESSIN ───────────────────────────────────────────────────────

// Range les nombres dans l'ordre où ils se dessinent : par NAISSANCE, le
// dernier né en dernier, donc par-dessus les autres.
//
// 🔴 JAMAIS PAR DISTANCE. Le client pose tous ses nombres et ses mots à une
// même profondeur, en avant de la scène : ni le décor ni l'éloignement ne les
// départagent. Triés par distance, un nombre ancien que sa dérive a rapproché
// de la caméra passait devant celui qui venait de sortir.
//
// ⚠ ET LA LISTE N'EST PAS DÉJÀ DANS CET ORDRE : un coup multiple y range ses
// frappes à venir dès qu'il arrive, et un coup plus tardif peut naître entre
// elles. Le tri est stable : une part et son cumul, nés au même instant,
// gardent l'ordre où on les a posés.
template <typename Popups>
void OrderByBirth(Popups& popups) {
  std::ranges::stable_sort(popups, {}, [](const auto& popup) { return popup.born_ms; });
}

// ── LA DÉRIVE DU NOMBRE JAILLISSANT, DANS NOTRE MONDE ───────────────────────

// L'ANGLE DU CLIENT pour une direction de boussole du serveur — nord, nord-ouest,
// ouest… dans le sens inverse des aiguilles d'une montre, comme les tables
// dirx et diry de src/map/unit.cpp. Le client mesure ses angles par
// atan2(dx, −dz) dans SON repère, où le sud vaut zéro et l'est quatre-vingt-dix ;
// le nord vaut donc cent
// quatre-vingts. Huit valeurs, tabulées : le repère du client, dont la
// profondeur est l'opposée de la nôtre, n'entre qu'ici.
[[nodiscard]] float ClientFacingDegrees(std::uint8_t compass_direction) noexcept;

// L'ANGLE QUE LE CLIENT TIENT pour le corps d'un acteur qui montre cette
// direction de boussole — et il dépend de QUI la lui a donnée.
//
// 🔴 UNE DIRECTION DU SERVEUR N'EST PAS RAMENÉE AU TOUR. Le client pose alors
// quarante-cinq fois la direction plus quatre : de cent quatre-vingts pour le
// nord à quatre cent quatre-vingt-quinze pour le nord-est. Le sud y vaut trois
// cent soixante, et non zéro. Une marche ou une visée, elles, passent par
// atan2 et restent dans le tour ; la visée garde en outre l'angle EXACT vers ce
// qu'elle vise, que la boussole arrondit.
//
// Toutes rendent la même image du corps ; seule la dérive des nombres
// jaillissants les distingue, et elle ne ramène pas l'angle au tour.
[[nodiscard]] float ClientBodyDegrees(const ro::sim::Actor& actor,
                                      std::uint8_t shown_facing) noexcept;

// La diagonale de dérive d'un nombre jaillissant, DANS NOTRE MONDE — x puis z,
// en unités par unité de chemin —, pour une cible dont le client tient cet
// angle — voir `ClientBodyDegrees`. C'est `BurstDriftClientDiagonal`, la
// profondeur retournée.
[[nodiscard]] std::array<float, 2> BurstDriftInWorld(float client_body_degrees) noexcept;

// ── L'APERÇU EN BOUCLE ──────────────────────────────────────────────────────
//
// Des coups FACTICES, joués sur le personnage tant que l'aperçu est allumé,
// pour régler la section sans chercher un monstre. Chaque entrée est un coup
// tel que le monde l'aurait porté, avec ses côtés : les règles ci-dessus s'y
// appliquent à l'identique, et ce que l'aperçu montre est ce que le jeu
// montrera.
struct Rehearsal {
  ro::sim::Strike strike;
  StrikeSides sides;
  // Ou une compétence SANS dégâts — le soin, la magie rendue — quand elle est
  // non nulle : `strike` ne compte alors pas.
  std::uint16_t support_skill_id = 0;
  std::int32_t support_amount = 0;
};

// Combien d'entrées la boucle enchaîne avant de recommencer. L'intervalle entre
// deux est un réglage : `HitPopupLook::rehearsal_interval_ms`.
inline constexpr std::size_t kRehearsalCount = 11;

// L'intervalle entre deux coups factices, tel que la boucle l'emploie : jamais
// sous `kShortestRehearsalIntervalMs`, quoi que le fichier porte — un
// intervalle nul ferait naître un coup à chaque image.
[[nodiscard]] std::uint32_t RehearsalStepMs(const HitPopupLook& look) noexcept;

// L'entrée de ce rang ; passé le compte, la boucle recommence.
[[nodiscard]] Rehearsal RehearsalAt(std::size_t index) noexcept;

// LE COUP QUI MONTRE UN TYPE dans le cadre d'essai des trajectoires, pris dans
// la même boucle : un coup ordinaire, un skill, un coup reçu, un critique, un
// coup multiple — dont le total —, un soin, un « Miss ».
[[nodiscard]] Rehearsal PathRehearsal(DamagePath path) noexcept;

}  // namespace moonclient
