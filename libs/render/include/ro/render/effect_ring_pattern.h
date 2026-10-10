#pragma once

#include <array>
#include <cstdint>

// CE QU'UN PATRON D'ANNEAU AJOUTE au patron d'un nœud.
//
// Sorti d'`effect_node_pattern_table.h`, qui l'inclut, au plafond de huit cents
// lignes. La coupure suit une frontière : `EffectNodePattern` décrit ce que TOUT
// nœud porte, celui-ci ce que seuls les nœuds en anneau emploient — un sprite
// laisse la structure entière à ses défauts.

namespace ro::render {

// 🔴 UN TIRAGE ENTIER DU CLIENT : `(rand() % modulo + ajout) / diviseur + biais`,
// la division ENTIÈRE. `(rand() % 10 + 1) / 5` ne rend que zéro, un ou deux — et
// pas avec la même fréquence : quatre fois sur dix, cinq fois, une fois. Un
// tirage continu entre les mêmes bornes montre des valeurs que le client ne
// montre jamais. Un modulo nul : pas de tirage entier.
struct EffectClientIntegerDraw {
  std::int32_t modulus = 0;
  std::int32_t addend = 0;
  std::int32_t divisor = 1;
  std::int32_t bias = 0;
};

// Ce qu'un patron d'ANNEAU ajoute, et que le sprite n'emploie pas.
//
// Chaque grandeur porte ses trois étages, comme dans le client : la valeur, son
// pas par tic, et l'accélération de ce pas. Un anneau qui ne pose que sa valeur
// est un anneau immobile, ce qui est un cas et non un oubli.
struct EffectRingPattern {
  float radius_at_anchor = 0.0F;
  float radius_at_anchor_growth = 0.0F;
  float radius_at_anchor_acceleration = 0.0F;
  float radius_at_height = 0.0F;
  float radius_at_height_growth = 0.0F;
  float radius_at_height_acceleration = 0.0F;
  // Le rayon d'une COURONNE PLATE, et de combien il s'étend par tic. Il part
  // souvent de zéro : c'est une onde, et c'est son pas qui la fait vivre.
  float ring_radius = 0.0F;
  float ring_radius_growth = 0.0F;
  // ET SON FREINAGE, par tic. ⚠ CALCULÉ CHEZ PRESQUE TOUS : `−(pas / vie) · k`,
  // une part du pas rapportée à la vie. Sans lui l'onde s'étend d'un pas
  // constant, et son rayon final dépasse d'un tiers celui du client — EF_WARP
  // ferait cent deux unités là où il en fait soixante-dix-sept.
  float ring_radius_acceleration = 0.0F;
  // La demi-largeur, que seuls les plans croisés emploient — l'extension de
  // l'axe qu'ils partagent.
  //
  // ⚠ CHAQUE DEMI-EXTENSION EST SUIVIE DE SA BORNE HAUTE, et zéro veut dire
  // « pas de tirage ». Le client tire certaines d'entre elles — « (rand() % 60
  // + 30) / 10 » pour toute la famille ac_center2 —, et la valeur porte alors la
  // borne BASSE. Deux nœuds d'un même patron ne sont donc pas de la même
  // taille : c'est ce qui donne son épaisseur à une aura, et une valeur figée
  // les empilerait en une seule figure.
  float half_width = 0.0F;
  float half_width_high = 0.0F;
  // ET ELLE POUSSE, comme la hauteur. Le mouvement l'intègre à chaque tic, et
  // sans ces deux étages un quad garde la taille de sa naissance : la rune au
  // sol de Skill Ground Ring s'étend chez le client et restait figée chez nous.
  float half_width_growth = 0.0F;
  float half_width_acceleration = 0.0F;
  // LA SECONDE IMAGE-CLÉ — voir `EffectNode::second_keyframe_tick`. Le crible
  // n'accepte un nœud à bascule que si elle ne change QUE ces SIX champs : le
  // client en remplace neuf, et laisser passer un nœud dont la bascule touche
  // les trois autres le ferait bouger faux au tic dit.
  std::int32_t second_keyframe_tick = -1;
  float second_half_width_growth = kEffectNodeKeyframeUnset;
  float second_half_width_acceleration = kEffectNodeKeyframeUnset;
  float second_height_growth = kEffectNodeKeyframeUnset;
  float second_height_acceleration = kEffectNodeKeyframeUnset;
  // ⚠ ET LE PAS EN EST, DEPUIS QU'IL A ÉTÉ MESURÉ. Le client remplace NEUF
  // champs à la bascule ; nous en portons SIX. Les deux derniers sont le pas du
  // nœud et sa variation — ce qui fait qu'une pointe de terre jaillit puis
  // REDESCEND, son pas passant de un à moins un et deux dixièmes. Sans eux, la
  // pointe montait et restait.
  float second_step_length = kEffectNodeKeyframeUnset;
  float second_step_length_change = kEffectNodeKeyframeUnset;
  float height = 0.0F;
  float height_high = 0.0F;
  float height_growth = 0.0F;
  float height_acceleration = 0.0F;
  float height_ceiling = kEffectNodeDefaultHalfHeightCeiling;
  float step_degrees = kEffectNodeDefaultRingStepDegrees;
  // La TRANCHE de texture d'un quad orienté, et ses drapeaux d'ancrage. L'image
  // entière par défaut.
  float texture_v_min = 0.0F;
  float texture_v_max = 1.0F;
  // LE PAS DU NŒUD, et de combien il varie à chaque tic.
  //
  // ⚠ CE N'EST PAS UNE TAILLE MAIS UNE AVANCE. Les demi-extensions disent
  // combien la figure MESURE ; celui-ci dit de combien elle se DÉPLACE — une
  // pointe de terre jaillit à une unité par tic, puis redescend à moins un et
  // deux dixièmes quand sa seconde image-clé le remplace.
  //
  // ⚠ ET IL A ÉTÉ AJOUTÉ AVEC SON REMPLAÇANT, pas après. Porter le second sans
  // le premier remplaçait un pas qui valait toujours zéro : la pointe ne
  // montait pas, puis descendait. Un test chiffré l'a dit tout de suite, et
  // c'est pour cela qu'il regarde les deux.
  float step_length = 0.0F;
  float step_length_change = 0.0F;
  // DE COMBIEN LE CAP DU NŒUD AVANCE PAR TIC, et de combien ce pas change.
  // Champs 816 et 828 du client. Voir `PooledEffectNode::node_yaw_step`.
  float yaw_step_degrees = 0.0F;
  float yaw_step_change = 0.0F;

  // DE COMBIEN LE NŒUD NAÎT SOUS LE SOL, en unités du monde.
  //
  // ⚠ LE CLIENT L'ÉCRIT EN POSITIF PARCE QUE SON ORDONNÉE DESCEND. Recopier son
  // signe ferait naître la pointe EN L'AIR ; ce champ dit une PROFONDEUR, et
  // c'est le semis qui la soustrait.
  //
  // Sans elle, une pointe qui jaillit part de la surface et finit trop haut :
  // le client l'enterre de dix unités — deux cellules — et ce qu'on voit sortir
  // est la différence entre sa montée et son enfouissement.
  float burial_depth = 0.0F;
  // LA PLACE DU NŒUD dans le plan du sol, relative au point de l'effet.
  //
  // 🔴 SANS ELLE, un nœud que le client écarte de son point naîtrait chez nous
  // À SON POINT. L'ordonnée, elle, est `burial_depth`.
  float offset_x = 0.0F;
  float offset_z = 0.0F;

  // LE TIC OÙ L'AVANCE S'ARRÊTE NET, et la TROISIÈME image-clé qui la reprend
  // trente tics avant la fin. Voir `EffectNode::motion_stop_tick` et
  // `EffectNode::sinking_step_length`.
  std::int32_t motion_stop_tick = -1;
  float sinking_step_length = kEffectNodeKeyframeUnset;
  float sinking_step_length_change = kEffectNodeKeyframeUnset;

  // COMBIEN DE FOIS CE NŒUD SE SÈME, et où les exemplaires se posent.
  //
  // ⚠ UN N'EST PAS TOUJOURS LE COMPTE, et la comparaison en jeu l'a montré d'un
  // coup : le client pose VINGT-CINQ pics de terre en grille pour l'effet 142
  // et SEPT pour le 79, là où nous en posions un. La figure et la place étaient
  // justes, et l'effet n'y ressemblait pas.
  //
  // ⚠ CE N'EST PAS LE CHAMP `exemplaires` DU RELEVÉ, qui compte les appels
  // d'UNE passe et vaut un pour presque tout le monde. Celui-ci se lit dans la
  // BOUCLE du bâtisseur, et il n'y a rien à en déduire : elle y est ou elle n'y
  // est pas.
  std::uint16_t instance_count = 1;
  EffectNodeInstancePlacement instance_placement = EffectNodeInstancePlacement::kSingle;
  // Le pas de la grille, ou le rayon du cercle, en unités du monde.
  float instance_spacing = 0.0F;
  // LA BORNE HAUTE D'UN RAYON TIRÉ, et zéro veut dire « fixe », comme pour
  // toutes les fourchettes de ce patron.
  //
  // 🔴 UN RAYON MOYEN POSE UNE COURONNE LÀ OÙ LE CLIENT SÈME UN DISQUE. Les
  // cristaux de Storm Gust naissent de trois à vingt-sept unités du centre :
  // écrire le milieu les aligne tous sur le même cercle, et cinq cellules
  // d'écart se voient.
  float instance_spacing_high = 0.0F;

  // DE COMBIEN LA FIGURE PENCHE, et le tirage qui l'écarte.
  //
  // ⚠ C'EST L'ÉCART AU QUART DE TOUR, pas l'angle du client. Son bâtisseur pose
  // une inclinaison de quatre-vingts à quatre-vingt-dix-neuf degrés, et le
  // quart de tour est déjà replié dans la figure — voir `effect_cross_spikes.h`.
  // Ce qui reste est le PENCHANT : de moins dix à plus neuf degrés. Porter
  // l'angle brut coucherait la figure une seconde fois.
  float lean_degrees = 0.0F;
  float lean_degrees_high = 0.0F;
  // Le LACET se tire sur le tour ENTIER quand il se tire : il n'a pas de borne
  // à porter, seulement un oui ou un non.
  bool draws_its_yaw = false;

  std::uint32_t flags = 0;

  // LE PAS SE RELANCE TOUS LES SIX TICS — le sous-type 117, et lui seul. Voir
  // `EffectNode::step_cycles`.
  //
  // ⚠ IL SE LIT DANS LE SOUS-TYPE ET NON DANS LA FIGURE, à la différence de la
  // translation axiale : le 9 et le 117 mènent au MÊME dessinateur, et c'est le
  // répartiteur de mouvement qui les sépare. Le déduire de la forme donnerait le
  // cycle aux pointes de terre.
  bool step_cycles = false;

  // LA BORNE HAUTE D'UNE PLACE TIRÉE, en abscisse et en profondeur : zéro veut
  // dire « pas de tirage », comme pour toutes les fourchettes de ce patron. Les
  // flèches de glace naissent de vingt-cinq à trente-quatre sur l'abscisse et de
  // quinze à vingt-quatre sur la profondeur ; une place fixe les ferait toutes
  // tomber sur la même ligne.
  float offset_x_high = 0.0F;
  float offset_z_high = 0.0F;
  // 🔴 VRAI QUAND LE NŒUD VISE SON POINT D'ANCRAGE : son cap et son inclinaison
  // ne se lisent pas, ils se CALCULENT de sa place de naissance vers le point de
  // l'effet — l'arc tangente du décalage, en degrés, un tirage de deux unités
  // en plus —, et son pas l'y mène. C'est ainsi qu'une flèche tombe sur sa
  // cible depuis soixante unités de haut et de côté, quel que soit le porteur.
  // Sans ce calcul, le nœud partirait à plat dans la direction du regard.
  bool aims_at_anchor = false;
  // TOUS LES COMBIEN DE TICS L'IMAGE CHANGE — ou ZÉRO pour le défaut
  // du constructeur, quatre. La flèche de feu fait défiler ses six images une
  // par tic.
  std::int32_t frame_hold_ticks = 0;
  // TOUS LES COMBIEN DE TICS LE PAS DE HAUTEUR SE RETOURNE, sous le bit 0x20
  // des drapeaux — ou ZÉRO quand le bâtisseur ne l'écrit pas. Voir
  // `EffectNode::height_flip_period_ticks`. Il se renseigne à la main.
  std::int32_t height_flip_period_ticks = 0;
  // 🔴 LE TROISIÈME ANGLE DU NŒUD, ET C'EST LUI QUI COUCHE UN QUAD.
  //
  // La matrice du nœud se bâtit en TROIS temps et non deux : une rotation
  // autour de Z avec ce champ, puis autour de X avec l'inclinaison, puis
  // autour de Y avec le cap. Le roulis s'applique donc EN PREMIER aux
  // coordonnées locales de la figure.
  //
  // ⚠ UN QUAD ORIENTÉ NAÎT DEBOUT : ses quatre points sont dans le plan de
  // l'abscisse locale nulle. Un roulis de quatre-vingt-dix degrés le pose À
  // PLAT — c'est ainsi que l'anneau du classement PVP, celui du groupe et le
  // réticule de visée se couchent au sol.
  //
  // ⚠ ET CE N'EST PAS UNE CONVENTION DE REPÈRE, à la différence du quart de tour
  // de la couronne plate : treize nœuds l'écrivent, dix-neuf autres du même
  // sous-type ne l'écrivent pas et restent DEBOUT — dont les quatre murs
  // d'EF_GLASSWALL, qui ferment une boîte. Il se renseigne par effet.
  float roll_degrees = 0.0F;
  // LA RETENUE DE LA TEINTE QUI TOURNE, sous le bit 0x800 des sous-types quinze
  // et seize : tant qu'un canal vaut 254, les pas de couleur ne s'appliquent
  // qu'une fois tous les tant de tics. Zéro, ils s'appliquent à chaque tic.
  // Voir `EffectNode::color_hold_ticks`. Il se renseigne à la main.
  std::int32_t color_hold_ticks = 0;

  // 🔴 LE PAS DES SOUS-TYPES QUINZE ET SEIZE avance le nœud le long de la
  // PROFONDEUR de sa matrice : chaque tic, le pas gagne sa variation, puis la
  // place gagne `pas × troisième ligne`. Sans cela, une traînée qui tombe reste
  // où elle naît. Voir `EffectNodeDepthAxis`.
  //
  // ⚠ À LA MAIN, ET NON DÉDUIT DE LA FIGURE : les autres nœuds de ces deux
  // figures font déjà ce chemin par leur LANCER, et le leur donner deux fois
  // doublerait leur course.
  bool advances_along_depth_axis = false;
  // L'AVANCE DÉJÀ FAITE À LA NAISSANCE, le long du même axe que le pas : le
  // bâtisseur pose la figure quinze unités devant le porteur, puis le pas la
  // ramène. Zéro : elle naît à son point.
  float axial_translation_start = 0.0F;
  // LA BORNE HAUTE D'UN ENFOUISSEMENT TIRÉ. ⚠ ICI ZÉRO VEUT DIRE « PAS DE
  // TIRAGE » MÊME QUAND LA BORNE BASSE EST NÉGATIVE : sans cette règle, tout
  // nœud posé en l'air se tirerait entre sa hauteur et le sol.
  float burial_depth_high = 0.0F;
  // 🔴 LE CAP EST L'ANGLE DU CORPS DU PORTEUR, et non l'angle de base de
  // l'effet : ce sont deux angles symétriques — `180 − base` —, et le prendre
  // pour l'autre envoie la figure du côté opposé dès que le porteur ne regarde
  // pas le nord ou le sud. Voir `EffectOwnerFacingDegrees`.
  bool yaw_is_owner_body_angle = false;
  // LA TEINTE TIRÉE D'UN ANNEAU : chaque canal va de la couleur du patron à
  // cette borne haute. Un canal dont la borne haute vaut sa basse garde sa
  // valeur ; tout à zéro, rien ne se tire — le cas de presque tous.
  std::array<std::uint8_t, 3> color_high = {0, 0, 0};
  // LES CANAUX QUI PARTAGENT UN SEUL TIRAGE, un bit par canal, rouge en bit
  // zéro. Rouge et vert tirés ensemble au-dessus d'un bleu plein donnent du bleu
  // pur au blanc ; tirés à part, du violet et du cyan que le client ne montre
  // jamais.
  std::uint8_t color_shared_channels = 0;
  // ET LA TEINTE À DEUX VALEURS : chaque canal prend sa borne haute UNE fois
  // sur tant, sa basse sinon — un pastel au hasard, jamais un intermédiaire.
  // Zéro : le tirage est continu.
  std::uint8_t color_high_one_in = 0;
  // 🔴 LE NŒUD VOLE VERS LA CIBLE DE L'EFFET, à plat et à vitesse constante :
  // son pas est la distance au sol divisée par sa vie. Sans cible, il reste à
  // son point.
  bool flies_to_target = false;
  // LA PLACE ET L'ENFOUISSEMENT, quand le client les tire en ENTIERS : ils
  // remplacent alors les fourchettes continues. Voir `EffectClientIntegerDraw`.
  EffectClientIntegerDraw offset_x_draw;
  EffectClientIntegerDraw burial_depth_draw;
  // LES TIRAGES QUE LE BÂTISSEUR FAIT ENSUITE SANS LES LIRE : ils avancent le
  // générateur, et ce qui se tire après ne tombe pas sur les mêmes restes.
  std::uint8_t draws_discarded_after_burial = 0;
  // 🔴 LE CERCLE DE NAISSANCE ENTIER, tiré AVANT tout le reste du nœud : un cap
  // `rand() % 360`, puis le rayon — `birth_radius_draw`, ou `launch_distance_low`
  // quand il est fixe. Faux : l'ancien cercle continu, tiré après le nœud.
  bool birth_circle_is_integer = false;
  EffectClientIntegerDraw birth_radius_draw;
  // COMBIEN D'EXEMPLAIRES SE POSENT SUR CHAQUE PLACE de la disposition, l'un
  // après l'autre. La grille de grêle en pose quatre par case : la
  // disposition compte alors les CASES, `instance_count` les exemplaires, et le
  // côté de la grille se déduit des cases. Un veut dire un par place.
  //
  // ⚠ ET LES TEXTURES SE PARTAGENT LES EXEMPLAIRES D'UNE PLACE par parts
  // égales, dans l'ordre, quand `EffectNodePattern::textures_per_copy` le dit :
  // deux textures pour quatre exemplaires donnent la première aux deux premiers.
  std::uint8_t copies_per_place = 1;
};

}  // namespace ro::render
