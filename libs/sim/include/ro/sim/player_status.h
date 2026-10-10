#pragma once

#include <cstdint>

namespace ro::sim {

// CE QUE LE SERVEUR DIT DU PERSONNAGE QU'ON JOUE, et de lui seul.
//
// ⚠ LES NOMS SONT CEUX DU SERVEUR, À LA LETTRE. `str`, `int_`, `base_exp`,
// `status_point` : ce sont les champs de `mmo_charstatus` (`src/common/mmo.hpp`)
// et de `map_session_data` (`src/map/pc.hpp`), recopiés tels quels — `int_`
// compris, qui n'est laid que parce que `int` est un mot réservé chez eux aussi.
//
// C'est une DÉROGATION DÉLIBÉRÉE à la règle « contenu, unité, origine » de
// CLAUDE.md, et elle s'achète quelque chose de précis : tout ce qui se trouve
// ici se recoupe contre le serveur, et un nom identique rend la recherche
// immédiate dans les deux sens. Un nom traduit obligerait à entretenir une
// table de correspondance dans la tête de qui lit.
//
// La contrepartie est que `sp` désigne les points de MAGIE, alors que
// `status_point` et `skill_point` vivent à côté : trois « SP » pour trois
// choses. C'est le défaut du serveur, on l'hérite, et ce commentaire est le
// seul endroit où il est dit.
//
// ⚠ CE N'EST PAS `mmo_charstatus` POUR AUTANT. Le registre du serveur est sa
// PERSISTANCE : il porte le point de sauvegarde, les amis, le partenaire, que
// le client n'apprend jamais. On garde ses NOMS, pas sa liste — celle-ci ne
// contient que ce que les paquets livrent réellement.

// ⚠ « PAS ENCORE DIT » N'EST PAS « ZÉRO », et c'est la règle qui tient tout le
// reste. Une bourse à zéro et une bourse dont le serveur n'a pas parlé ne se
// montrent pas pareil : la première s'affiche, la seconde ne s'affiche pas. Le
// patron vient d'`Actor::health`, où moins un veut déjà dire « on ne sait pas »
// et non « mort ».
inline constexpr std::int64_t kUnknownStat = -1;

struct PlayerStatus {
  // ⚠ `hp` ET `max_hp` NE SONT PAS ICI, bien que le serveur les range dans
  // `mmo_charstatus`. Ils vivent sur `Actor`, où toute la carte les porte
  // déjà — c'est ce qui dessine les barres au-dessus des têtes, et le joueur
  // n'y fait pas exception. Les dédoubler ferait diverger sa barre de celle des
  // autres le jour où l'une des deux voies manquerait un paquet.

  // Les points de MAGIE. SP_SP, SP_MAXSP.
  std::int64_t sp = kUnknownStat;
  std::int64_t max_sp = kUnknownStat;

  std::int64_t base_level = kUnknownStat;
  std::int64_t job_level = kUnknownStat;

  // L'expérience ACQUISE au niveau en cours, et celle qu'il faut pour le
  // suivant. Le serveur donne les deux séparément ; en déduire une fraction est
  // l'affaire de qui l'affiche.
  //
  // ⚠ SUR SOIXANTE-QUATRE BITS, comme le `t_exp` du serveur, et ce n'est pas de
  // la prudence : il a un paquet À PART pour ces valeurs — `ZC_LONGLONGPAR_
  // CHANGE` — précisément parce qu'elles débordent de trente-deux bits. Les
  // ranger plus court les tronquerait en silence.
  std::int64_t base_exp = kUnknownStat;
  std::int64_t next_base_exp = kUnknownStat;
  std::int64_t job_exp = kUnknownStat;
  std::int64_t next_job_exp = kUnknownStat;

  std::int64_t zeny = kUnknownStat;

  // Le poids porté et ce qu'on peut porter, en dixièmes d'unité comme le
  // serveur les stocke.
  std::int64_t weight = kUnknownStat;
  std::int64_t max_weight = kUnknownStat;

  // LA PART DU POIDS MAXIMUM AU-DELÀ DE LAQUELLE ON EST SURCHARGÉ, en pour cent.
  //
  // ⚠ CE N'EST NI UN POIDS NI UNE CARACTÉRISTIQUE : c'est un RÉGLAGE du serveur
  // (`natural_heal_weight_rate`), qu'il annonce une fois par connexion et par un
  // paquet à lui. Au-delà, la régénération naturelle s'arrête — et c'est ce que
  // la jauge du sac montre en rouge.
  std::int64_t overweight_percent = kUnknownStat;

  std::int64_t status_point = kUnknownStat;
  std::int64_t skill_point = kUnknownStat;

  // Les six caractéristiques, aux noms du serveur.
  std::int64_t str = kUnknownStat;
  std::int64_t agi = kUnknownStat;
  std::int64_t vit = kUnknownStat;
  std::int64_t int_ = kUnknownStat;
  std::int64_t dex = kUnknownStat;
  std::int64_t luk = kUnknownStat;

  // Le BONUS que l'équipement ajoute à chacune. Il arrive par
  // `ZC_COUPLESTATUS`, qui donne la base et le bonus ensemble.
  std::int64_t str_bonus = kUnknownStat;
  std::int64_t agi_bonus = kUnknownStat;
  std::int64_t vit_bonus = kUnknownStat;
  std::int64_t int_bonus = kUnknownStat;
  std::int64_t dex_bonus = kUnknownStat;
  std::int64_t luk_bonus = kUnknownStat;

  // Ce qu'un point de plus COÛTE. SP_USTR et ses voisins.
  //
  // Il arrive par ses propres paramètres : le serveur le recalcule à chaque
  // montée, et un client qui le déduirait de la valeur se tromperait dès qu'une
  // règle change là-bas.
  std::int64_t str_cost = kUnknownStat;
  std::int64_t agi_cost = kUnknownStat;
  std::int64_t vit_cost = kUnknownStat;
  std::int64_t int_cost = kUnknownStat;
  std::int64_t dex_cost = kUnknownStat;
  std::int64_t luk_cost = kUnknownStat;

  // La portée d'attaque, en cases : `ZC_ATTACK_RANGE`, champ `currentAttRange`.
  std::int64_t attack_range = kUnknownStat;

  // LES VALEURS DÉRIVÉES DU COMBAT, telles que le serveur les CALCULE.
  //
  // ⚠ ELLES NE SE DÉDUISENT PAS DES CARACTÉRISTIQUES. L'attaque, la défense et
  // l'esquive sortent de règles que le serveur applique — arme, raffinage,
  // cartes, états — et un client qui les recalculerait afficherait autre chose
  // que ce qui le frappe. On range ce qu'il envoie, on ne le refait pas.
  //
  // Chaque paire se montre comme le jeu la montre : « base + bonus ».
  std::int64_t atk1 = kUnknownStat;  // SP_ATK1, le côté gauche
  std::int64_t atk2 = kUnknownStat;  // SP_ATK2, le côté droit
  // 🔴 CES DEUX-LÀ SONT DANS L'AUTRE SENS QUE LEURS VOISINES, et c'est le
  // serveur qui l'impose : `SP_MATK1` porte le côté DROIT — le MAXIMUM — là où
  // `SP_ATK1`, `SP_DEF1` et `SP_MDEF1` portent tous le côté gauche
  // (`clif_updatestatus`, src/map/clif.cpp). Les noms restent ceux du serveur,
  // comme tout le reste de cette structure ; c'est à l'affichage d'écrire le
  // minimum d'abord.
  std::int64_t matk1 = kUnknownStat;  // SP_MATK1, le MAXIMUM
  std::int64_t matk2 = kUnknownStat;  // SP_MATK2, le minimum
  std::int64_t def1 = kUnknownStat;   // SP_DEF1
  std::int64_t def2 = kUnknownStat;   // SP_DEF2
  std::int64_t mdef1 = kUnknownStat;  // SP_MDEF1
  std::int64_t mdef2 = kUnknownStat;  // SP_MDEF2

  std::int64_t hit = kUnknownStat;       // SP_HIT
  std::int64_t flee1 = kUnknownStat;     // SP_FLEE1, l'esquive
  std::int64_t flee2 = kUnknownStat;     // SP_FLEE2, l'esquive PARFAITE
  std::int64_t critical = kUnknownStat;  // SP_CRITICAL

  // ⚠ CE N'EST PAS LA VITESSE D'ATTAQUE AFFICHÉE, et le nom le dit pour cela.
  // `SP_ASPD` porte `amotion` — le DÉLAI de motion, en millisecondes, mesuré
  // dans `clif.cpp` : `clif_par_change(sd, type, sd.battle_status.amotion)`. Le
  // nombre que le jeu montre en dérive, et cette dérivation appartient au
  // CLIENT : c'est `DisplayedAspd`, plus bas.
  std::int64_t amotion = kUnknownStat;  // SP_ASPD

  // ⚠ DEUX DE CES VALEURS ARRIVENT DÉJÀ DIVISÉES PAR DIX, et c'est mesuré :
  // `SP_FLEE2` envoie `flee2/10`, `SP_CRITICAL` envoie `cri/10`. Les rediviser
  // à l'affichage montrerait un dixième de la vraie valeur.
};

// LA VITESSE D'ATTAQUE TELLE QUE LE JEU LA MONTRE, dérivée du délai de motion.
//
// Le serveur n'envoie jamais ce nombre : il envoie `amotion`, et le client le
// retourne. Deux secondes de délai valent zéro, et chaque centième de seconde
// gagné vaut un point.
//
// ⚠ « PAS ENCORE DIT » N'EST PAS « INFINIMENT LENT ». Sans ce test, un
// personnage dont le délai n'est pas encore arrivé afficherait 200 — une valeur
// plausible, donc une valeur qu'on ne mettrait pas en doute.
[[nodiscard]] std::int64_t DisplayedAspd(std::int64_t amotion_ms) noexcept;

// LES IDENTIFIANTS DE PARAMÈTRE, tels que le serveur les numérote.
//
// C'est `enum _sp`, dans `src/map/map.hpp` du serveur. Les numéros ne se
// déduisent d'aucune règle — l'ordre est historique, avec des trous nommés
// `SP_0a` et `SP_1a` —, donc chaque entrée porte le nom du serveur et c'est là
// qu'on la revérifie.
enum class ServerParameter : std::uint16_t {
  kSpeed = 0,         // SP_SPEED
  kBaseExp = 1,       // SP_BASEEXP
  kJobExp = 2,        // SP_JOBEXP
  kKarma = 3,         // SP_KARMA
  kManner = 4,        // SP_MANNER
  kHp = 5,            // SP_HP
  kMaxHp = 6,         // SP_MAXHP
  kSp = 7,            // SP_SP
  kMaxSp = 8,         // SP_MAXSP
  kStatusPoint = 9,   // SP_STATUSPOINT
  kBaseLevel = 11,    // SP_BASELEVEL
  kSkillPoint = 12,   // SP_SKILLPOINT
  kStr = 13,          // SP_STR
  kAgi = 14,          // SP_AGI
  kVit = 15,          // SP_VIT
  kInt = 16,          // SP_INT
  kDex = 17,          // SP_DEX
  kLuk = 18,          // SP_LUK
  kClass = 19,        // SP_CLASS
  kZeny = 20,         // SP_ZENY
  kSex = 21,          // SP_SEX
  kNextBaseExp = 22,  // SP_NEXTBASEEXP
  kNextJobExp = 23,   // SP_NEXTJOBEXP
  kWeight = 24,       // SP_WEIGHT
  kMaxWeight = 25,    // SP_MAXWEIGHT
  kUStr = 32,         // SP_USTR
  kUAgi = 33,         // SP_UAGI
  kUVit = 34,         // SP_UVIT
  kUInt = 35,         // SP_UINT
  kUDex = 36,         // SP_UDEX
  kULuk = 37,         // SP_ULUK
  kAtk1 = 41,         // SP_ATK1
  kAtk2 = 42,         // SP_ATK2
  kMatk1 = 43,        // SP_MATK1
  kMatk2 = 44,        // SP_MATK2
  kDef1 = 45,         // SP_DEF1
  kDef2 = 46,         // SP_DEF2
  kMdef1 = 47,        // SP_MDEF1
  kMdef2 = 48,        // SP_MDEF2
  kHit = 49,          // SP_HIT
  kFlee1 = 50,        // SP_FLEE1
  kFlee2 = 51,        // SP_FLEE2
  kCritical = 52,     // SP_CRITICAL
  kAspd = 53,         // SP_ASPD
  kJobLevel = 55,     // SP_JOBLEVEL
};

// CE QU'UN PARAMÈTRE EST DEVENU, et pourquoi il y a trois cas et non deux.
//
// ⚠ « CONNU MAIS INUTILISÉ » ET « INCONNU » NE SE CONFONDENT PAS. Le premier
// est une décision qu'on a prise et qu'on peut relire ; le second est du
// travail qui reste. Les mélanger ferait disparaître le second du relevé des
// trames, c'est-à-dire de la seule liste qui dise ce qu'il reste à porter.
enum class ParameterOutcome : std::uint8_t {
  // Rangé : quelque chose a changé.
  kStored,
  // Reconnu, et rien à en faire. DEUX raisons, et elles se valent pour le
  // relevé : soit le client n'en montre rien — le karma, la politesse —, soit
  // il n'y a personne à qui l'appliquer, le joueur n'étant pas encore posé.
  //
  // ⚠ CE N'EST PAS « INCONNU », et c'est le point : une trame qui arrive avant
  // l'entrée en jeu est normale, et la ranger dans le travail restant ferait
  // croire à un paquet non porté.
  kKnownButUnused,
  // Jamais classé. C'est ce cas-là qui doit rester visible.
  kUnknown,
};

// Range une valeur de paramètre.
//
// ⚠ NE TRAITE NI LA VIE NI LA VITESSE. Elles vivent sur l'acteur, et cette
// fonction ne connaît pas le monde : elle rend `kUnknown` pour elles, et c'est
// à `World` de les router. Voir `World::SetPlayerParameter`, qui est le seul
// endroit où les deux voies se rejoignent.
[[nodiscard]] ParameterOutcome ApplyParameter(PlayerStatus& status, std::uint16_t parameter,
                                              std::int64_t value);

// Le BONUS d'une caractéristique, celui que l'équipement ajoute.
//
// Séparé parce qu'il arrive par un autre paquet — `ZC_COUPLESTATUS`, qui donne
// la base et le bonus ensemble — et que son `statusType` est numéroté comme
// `enum _sp`, donc SP_STR pour la force et non zéro.
//
// Rend faux pour un numéro qui n'est pas une caractéristique.
[[nodiscard]] bool ApplyAttributeBonus(PlayerStatus& status, std::uint16_t parameter,
                                       std::int64_t bonus);

// DES POINTS DE STATUT À PLACER : une caractéristique au moins coûte ce qu'il
// reste, ou moins.
//
// ⚠ UN COÛT NUL NE COMPTE PAS : le serveur l'annonce pour une caractéristique
// à son maximum, et un personnage au plafond garde ses points sans pouvoir les
// placer. Un coût encore inconnu ne compte pas davantage.
[[nodiscard]] bool StatusPointsSpendable(const PlayerStatus& status) noexcept;

// Des points de skill à placer : il en reste un au moins.
[[nodiscard]] bool SkillPointsSpendable(const PlayerStatus& status) noexcept;

}  // namespace ro::sim
