#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ro::render {

// CE QU'UNE ACTION FAIT DU NOM QU'ELLE VIENT DE RÉSOUDRE.
//
// Le nom ne dit pas ce qu'on en fait : le même mécanisme sert à montrer un effet
// et à en faire cesser un. Les traiter tous comme des semis ferait reparaître
// une boucle à l'instant où elle doit disparaître.
//
// ⚠ CESSER A DEUX FORMES, et le client emploie les deux. `kCut` retire l'effet
// sur-le-champ ; `kStop` lève seulement le drapeau qui l'empêche de repartir, si
// bien que l'image en cours va jusqu'au bout de son cycle. Chez nous la
// différence est la même : couper vide ce qui joue, arrêter cesse de réinscrire
// et laisse mourir.
enum class NamedEffectVerb : std::uint8_t {
  kSow,
  kCut,
  kStop,
};

// OÙ L'EFFET NAÎT. C'est le deuxième argument que le client donne à son semeur :
// nul pour un effet laissé à une position, l'acteur pour un effet qui le suit.
enum class NamedEffectAnchor : std::uint8_t {
  kActor,
  kPosition,
};

// LE BRAS QU'UNE MÉTHODE CHOISIT. Certaines sèment un jeu de noms ou un autre :
// selon que la cible porte déjà un effet nommé, ou sur un tirage pair ou
// impair. D'autres taisent un effet de décor quand le joueur a demandé des
// effets allégés.
enum class NamedEffectGate : std::uint8_t {
  kAlways,
  kIfTargetCarries,
  kUnlessTargetCarries,
  kOddDraw,
  kEvenDraw,
  kUnlessMinimalEffects,
};

// Le délai de départ que la fiche de l'effet nommé porte, sans réécriture.
inline constexpr std::int32_t kRecordStartDelay = -1;

// UN DÉCALAGE DE LA POSITION SEMÉE, en unités du monde et DANS LE REPÈRE DU
// CLIENT : x, puis la HAUTEUR — négative vers le haut —, puis z. Chaque axe
// est tiré en entier entre ses deux bornes, bornes comprises ; des bornes
// égales donnent un décalage fixe.
//
// ⚠ C'est au lecteur de le passer dans son repère.
struct NamedEffectOffset {
  float x_min = 0.0F;
  float x_max = 0.0F;
  float height_min = 0.0F;
  float height_max = 0.0F;
  float z_min = 0.0F;
  float z_max = 0.0F;
};

// L'ORIENTATION QUE LA MÉTHODE DONNE À L'EFFET SEMÉ, en degrés.
//
// ⚠ ELLE N'A PAS LE MÊME SENS SELON LE DESSIN. Un effet debout tourne dans le
// plan de l'ÉCRAN, autour du point du sol qui le porte, dans le sens des
// aiguilles d'une montre — et SEULEMENT si l'angle est strictement positif. Un
// effet couché au sol tourne autour de la verticale, depuis la droite de
// l'écran, quel que soit son signe.
enum class NamedEffectTurn : std::uint8_t {
  kNone,
  // `turn_degrees`, tel quel.
  kFixed,
  // L'ANGLE À L'ÉCRAN de la source vers la cible, plus `turn_degrees` : zéro
  // vers la droite, quatre-vingt-dix vers le bas. La source et la cible sont
  // celles que le moment donne — le lanceur et la case visée pour une
  // compétence posée sur une case.
  kSourceToTarget,
};

// LE RETOURNEMENT HORIZONTAL de l'effet semé : ses coins changent de côté
// autour du point qui le porte.
enum class NamedEffectFlip : std::uint8_t {
  kNever,
  // Quand la SOURCE se dessine tournée vers le haut ou la droite de l'écran :
  // sa direction de sprite vaut de quatre à sept.
  kWhenSourceFacesAway,
  // Aux frappes de RANG IMPAIR d'un coup qui en porte plusieurs.
  kOnOddHit,
};

// LE DÉLAI QUE LA MÉTHODE TIRE DE L'ALLURE D'UN ACTEUR, et non d'un nombre
// écrit. L'allure est celle de l'action que l'état d'emploi de la compétence
// vient de lui faire prendre : la CADENCE — ce que dure une image, en pas de
// vingt-quatre millisecondes — et l'IMAGE DU COUP, celle où il porte. Voir
// `NamedEffectPaceOf`.
//
// ⚠ L'ACTEUR EST TOUJOURS LA SOURCE du moment : le lanceur d'une compétence,
// l'attaquant d'un coup.
enum class NamedEffectPace : std::uint8_t {
  kNone,
  // L'image du coup × la cadence × seize millisecondes : les deux tiers de
  // l'instant où le coup porte, qui tombe à vingt-quatre millisecondes par pas.
  kTwoThirdsOfHit,
  // La cadence × cinquante, plus trente millisecondes.
  kCadenceTimesFiftyPlusThirty,
  // La cadence × cent, plus cent millisecondes ; cinq cents quand le coup n'a
  // pas de source.
  kCadenceTimesHundredPlusHundred,
};

// CE QU'UNE MÉTHODE CALCULE ET QU'UNE ENTRÉE NE PEUT PAS ÉCRIRE : un nom choisi
// par l'angle, un nombre d'exemplaires tiré ou pris au niveau, des places
// tirées sans remise. Voir `ro/render/skill_named_recipe.h`.
enum class NamedEffectRecipe : std::uint8_t {
  kNone,
  // L'angle à l'écran choisit le nom — `_horizon` ou `_vertical` —, le
  // retournement et l'orientation.
  kPierceByScreenQuarter,
  // Le trou du nom prend `right` quand la cible est à gauche de la source à
  // l'écran, `left` sinon.
  kSideByScreenAngle,
  // Le trou du nom prend le douzième de tour où tombe l'angle à l'écran, décalé
  // de cent cinq degrés.
  kTwelfthByScreenAngle,
  // Un carré de places tirées SANS REMISE, dont le côté vient du niveau, et un
  // éclat par place, trente-deux millisecondes d'écart ; le trou du nom prend
  // un tirage de trois.
  kShuffledGridByLevel,
  // Trois vagues d'éventail, chacune de quinze à dix-neuf rayons tournés, dont
  // les départs sont tirés sans remise ; le trou prend le rang de la vague.
  kShuffledFanWaves,
  // Huit éclats sur un cercle de quinze ou vingt unités, un par huitième de
  // tour, orientés au hasard, deux cent quarante millisecondes d'écart.
  kScatteredRing,
  // De cinq à sept éclats selon le niveau, dispersés au dixième d'unité dans
  // un carré qui grandit avec leur nombre, quatre-vingts millisecondes d'écart.
  kScatterByLevel,
  // Trois salves de deux éclats, deux cents millisecondes d'écart : un quart
  // de tour tiré, puis celui d'en face, chaque éclat de dix à dix-neuf unités
  // du centre, dans son quart.
  kPairedQuarterBursts,
};

// L'échelle ordinaire d'un effet semé, en centièmes.
inline constexpr std::uint16_t kNamedEffectFullScaleHundredths = 100;

// Un effet qu'une action de compétence désigne par un NOM.
//
// ⚠ LE NOM NE DÉSIGNE AUCUN FICHIER PAR LUI-MÊME : c'est une table de DONNÉES de
// l'installation qui le résout — voir ro::formats::NamedEffectTable. Un nom que
// cette table ne porte pas ne montre rien, chez le client natif non plus.
struct SkillNamedEffectEntry {
  std::uint16_t skill_id = 0;
  // LE SLOT DE LA VTABLE EST LE MOMENT, et rien d'autre ne le dit : chaque pont
  // du registre n'en appelle qu'un, et c'est la fonction du jeu qui déclenche ce
  // pont qui nomme le moment. Les dix-huit moments sont dans
  // scripts/client_named_effect_moments.json.
  std::uint8_t slot = 0;
  std::string_view name;
  NamedEffectVerb verb = NamedEffectVerb::kSow;
  NamedEffectAnchor anchor = NamedEffectAnchor::kActor;
  // Vrai quand l'effet naît sur la SOURCE du coup et non sur sa cible — pour un
  // moment qui reçoit les deux, voir `kSkillNamedOnDamagePacket`.
  bool on_source = false;
  // CE QUI DÉCIDE SI L'EFFET NAÎT, quand la méthode choisit entre deux bras.
  NamedEffectGate gate = NamedEffectGate::kAlways;
  // L'effet nommé que la cible porte, ou non, pour `kIfTargetCarries` et
  // `kUnlessTargetCarries`.
  std::string_view gate_name;
  // LE DÉLAI DE DÉPART que la méthode pose sur l'effet semé. Il REMPLACE celui
  // de la fiche : le client réécrit le même champ après la création, zéro
  // compris. Négatif : celui de la fiche.
  std::int32_t start_delay_ms = kRecordStartDelay;
  // Où l'effet naît autour du point que l'ancrage désigne. Il ne vaut que pour
  // un effet laissé à une POSITION : un effet attaché suit son porteur.
  NamedEffectOffset offset;
  // Vrai quand le nom porte un TROU que remplit le niveau de la compétence —
  // `stormslash_{}` devient `stormslash_5`. Voir la note sur les compteurs.
  bool name_takes_level = false;
  // ⚠ LES CHAMPS QUI SUIVENT SONT EN QUEUE, et c'est une contrainte : le
  // générateur écrit les entrées ordinaires par liste POSITIONNELLE.
  //
  // L'ORIENTATION, et le hasard qui s'y ajoute : un entier tiré de zéro à
  // `turn_drawn_span` exclu. Zéro n'ajoute rien.
  NamedEffectTurn turn = NamedEffectTurn::kNone;
  float turn_degrees = 0.0F;
  std::uint16_t turn_drawn_span = 0;
  NamedEffectFlip flip_x = NamedEffectFlip::kNever;
  // L'ÉCHELLE, en centièmes tirés entre deux bornes comprises. Elle grandit
  // l'effet autour du point du sol qui le porte, et non autour de son ancre.
  std::uint16_t scale_hundredths_min = kNamedEffectFullScaleHundredths;
  std::uint16_t scale_hundredths_max = kNamedEffectFullScaleHundredths;
  // COUCHÉ AU SOL : l'image se pose à plat, chaque coin à deux unités
  // au-dessus du relief, et ses pixels valent des unités du monde.
  bool laid_on_ground = false;
  // Le HASARD qui s'ajoute au délai de départ : un entier tiré de zéro à
  // `start_delay_drawn_span_ms` exclu.
  std::uint16_t start_delay_drawn_span_ms = 0;
  // Un délai que l'allure d'un acteur donne ; il REMPLACE `start_delay_ms`.
  NamedEffectPace pace = NamedEffectPace::kNone;
  NamedEffectRecipe recipe = NamedEffectRecipe::kNone;
  // Vrai quand l'effet naît sur l'UNITÉ AU SOL que le coup frappe, pour un
  // paquet des dégâts dont la cible n'est pas un acteur.
  bool on_fallback_unit = false;
  // UN EFFET PAR IDENTIFIANT que la méthode pose au même instant, et non un nom
  // — `name` est alors vide. Zéro pour une entrée nommée.
  std::uint16_t effect_id = 0;
  // Vrai quand cet effet VISE la cible : il reçoit sa position au sol.
  bool aimed_at_target = false;
  // L'ENTRÉE NE VAUT QUE POUR CE NIVEAU : la méthode choisit son nom par un
  // aiguillage sur le niveau que le paquet annonce. Négatif : pour tous.
  std::int16_t only_at_level = -1;
  // L'ENTRÉE NE VAUT QUE QUAND LES SPHÈRES DE LA SOURCE SONT DE CET ÉLÉMENT —
  // voir `ro::sim::Actor::spirit_charm_element`. Négatif : quel que soit.
  std::int16_t only_with_source_charm = -1;
  // LE CERCLE D'INCANTATION DE L'ACTION : un effet par identifiant que la case
  // de la pose sème sur le lanceur avec la DURÉE ANNONCÉE pour vie — le client
  // la lui donne en tics, seize millisecondes chacun. Il se joue et se relance
  // comme l'aura que la table des incantations donne.
  bool life_from_cast = false;
};

// L'INCANTATION, ET ELLE A DEUX SLOTS. Les deux partent du visuel d'incantation
// du client : le premier directement, le second par le répartiteur d'effets
// qu'il appelle ensuite.
//
// ⚠ LE GROS EST DANS LE SECOND, et s'arrêter au premier ne montrerait presque
// rien : 61 des 65 effets du slot 8 finissent en `_cast` ou `_cast_bottom`,
// contre deux effets sans suffixe au slot 9. Ce sont les NOMS qui le disent — le
// code, lui, appelle les deux sans rien distinguer.
inline constexpr std::uint8_t kSkillNamedOnCast = 9;
inline constexpr std::uint8_t kSkillNamedOnCastSpawn = 8;

// LA COMPÉTENCE SANS DÉGÂTS, celle que le serveur annonce par ZC_USE_SKILL
// (0x09CB). Son pont se déclenche au DÉFAUT du répartiteur d'annonce —
// c'est-à-dire pour toute compétence dont ce répartiteur n'a pas fait un cas à
// part, ce que sont toutes les modernes.
inline constexpr std::uint8_t kSkillNamedOnUseSkill = 11;

// ⚠ DES NOMS DE CETTE TABLE PORTENT UN COMPTEUR — `stormslash_{}`,
// `hells_drive_%02d`, `radiant_spear_{}down` — et le compteur n'a pas la même
// source d'une méthode à l'autre.
//
// LE NIVEAU, pour le slot 11 : c'est le champ `level` de ZC_USE_SKILL, que le
// répartiteur d'annonce passe jusqu'au slot, et que `ro::sim::SupportHit::amount`
// porte. Storm Slash formate son nom avec lui — `name_takes_level`.
//
// LE TOUR D'UNE BOUCLE, ou un tirage dont l'issue est fixe : ces noms-là sont
// écrits en entier dans la table, un par tour — Hasty Fire in the Hole sème
// `_0`, `_1` et `_2`.
//
// L'ANGLE À L'ÉCRAN, OU UN TIRAGE RÉEL : c'est une RECETTE qui remplit ces
// trous-là, au semis — voir `NamedEffectRecipe`.

// LA COMPÉTENCE POSÉE SUR UNE CASE. Deux indices concordent : son pont est le
// défaut d'un répartiteur de paquet, et 18 des 19 effets de ce slot sont ancrés
// à une POSITION quand tous les autres slots le sont à l'acteur. Ce sont les
// compétences qui visent un endroit — Wild Fire, Meteor Storm Buster.
inline constexpr std::uint8_t kSkillNamedOnGround = 12;

// L'IMPACT, sur qui reçoit le coup. Les noms le disent : 42 des 49 effets de ce
// slot finissent en `_hit`, et aucun autre slot n'en porte plus de trois.
inline constexpr std::uint8_t kSkillNamedOnHit = 16;

// LE PAQUET DES DÉGÂTS, sur la SOURCE du coup. Son pont part à chaque
// ZC_NOTIFY_SKILL, AVANT le répartiteur et quoi qu'il décide : ce n'est pas un
// défaut. Il reçoit la source et la cible ; Crescive Bolt sème sur la source,
// tourné vers la cible.
inline constexpr std::uint8_t kSkillNamedOnDamagePacket = 15;

// La table entière, triée par identifiant de compétence puis par slot.
[[nodiscard]] std::span<const SkillNamedEffectEntry> SkillNamedEffectTable();

// Ce qu'une compétence joue à un moment donné, dans l'ordre de sa fonction, ou
// une plage VIDE.
[[nodiscard]] std::span<const SkillNamedEffectEntry> SkillNamedEffects(std::uint32_t skill_id,
                                                                       std::uint8_t slot);

// Un effet qu'une action d'ÉTAT désigne par un nom.
//
// ⚠ UN ÉTAT N'EST PAS UNE COMPÉTENCE, et leurs identifiants ne vivent pas dans
// le même espace — c'est pourquoi cette table est séparée. Les noms se
// ressemblent pourtant à s'y méprendre : l'état `IG_GUARD_STANCE` s'inscrit sous
// 1202, la compétence du même nom sous 5255.
struct StateNamedEffectEntry {
  std::uint16_t state_id = 0;
  std::uint8_t slot = 0;
  std::string_view name;
  NamedEffectVerb verb = NamedEffectVerb::kSow;
  NamedEffectAnchor anchor = NamedEffectAnchor::kActor;
  // ⚠ EN QUEUE, pour la même raison que chez les compétences.
  //
  // LE TROU DU NOM, `%02d`, que remplit la VALEUR que le serveur donne à
  // l'état.
  bool name_takes_value = false;
  // L'ENTRÉE NE VAUT QUE POUR CETTE VALEUR. Négatif : pour toutes.
  std::int32_t only_at_value = -1;
  // UN COMPTE DE CHARGES : la valeur dit combien de copies tiennent au-dessus
  // de la tête de l'acteur — voir `ro/render/charge_count_effect.h`. `name`
  // est alors vide. ⚠ SEULEMENT QUAND LA SECONDE VALEUR NE DÉSIGNE PERSONNE.
  bool counts_charges = false;
  // SUR L'ACTEUR QUE LA SECONDE VALEUR DÉSIGNE, et seulement quand elle en
  // désigne un : son identifiant y est écrit.
  bool on_designated_actor = false;
  // UN EFFET PAR IDENTIFIANT que la case pose ou retire au même instant, et non
  // un nom — `name` est alors vide. Zéro pour une entrée nommée. Un retrait
  // (`kCut`) précède les poses de sa case : un état qui repose le même numéro
  // le retire d'abord.
  std::uint16_t effect_id = 0;
  // LE PARAMÈTRE que la case donne à cet effet — le rang d'un talisman —, ou
  // moins un quand elle n'en donne pas : la valeur de l'état tient lieu.
  std::int32_t effect_param = -1;
  // Vrai quand le client déclare cet effet SANS FIN : il vit jusqu'à la levée.
  bool endless = false;
};

// VRAI QUAND L'ENTRÉE LIT LA VALEUR DE SON ÉTAT : son moment se rejoue alors
// chaque fois que la valeur change, l'état tenant toujours. Le serveur redit un
// état à chaque nouveau compte, et le client rejoue le moment de la pose à
// chaque fois.
[[nodiscard]] constexpr bool StateNamedEntryReadsValue(const StateNamedEffectEntry& entry) {
  return entry.name_takes_value || entry.only_at_value >= 0 || entry.counts_charges ||
         entry.on_designated_actor;
}

// L'état vient d'être POSÉ sur l'acteur.
//
// ⚠ LES VERBES LE DISENT : sur les 38 actions
// d'état, ce slot SÈME 58 fois et ne retire que 2 fois, tandis que le suivant
// RETIRE 44 fois pour un seul semis — et il retire exactement les noms que
// celui-ci a semés. Les deux exceptions s'expliquent : `GenderChange` montre son
// effet quand l'état tombe, et `INTENSIVE_AIM_COUNT` est un compteur qui retire
// son cran précédent avant de poser le suivant.
inline constexpr std::uint8_t kStateNamedOnGained = 0;

// L'état vient d'être RETIRÉ.
inline constexpr std::uint8_t kStateNamedOnLost = 1;

// La table entière, triée par identifiant d'état puis par slot.
[[nodiscard]] std::span<const StateNamedEffectEntry> StateNamedEffectTable();

// Ce qu'un état joue à un moment donné, ou une plage VIDE.
[[nodiscard]] std::span<const StateNamedEffectEntry> StateNamedEffects(std::uint32_t state_id,
                                                                       std::uint8_t slot);

}  // namespace ro::render
