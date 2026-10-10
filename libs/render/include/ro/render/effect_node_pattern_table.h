#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "ro/render/effect_haunt.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_supplement.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_ring_pattern.h"
#include "ro/render/skill_effect_table.h"

namespace ro::render {

// LE PATRON de la troupe de nœuds qu'un EFFET sème.
//
// Un effet procédural n'a aucun fichier : le client le dessine en code, et nous
// le portons nœud par nœud — voir `docs/adr/0014`.
//
// ⚠ INDEXÉ PAR L'EFFET, et non par la compétence. C'est ce que fait le client —
// son répartiteur est un switch sur l'identifiant d'effet — et
// c'est ce qui ouvre les DEUX chemins par lesquels un effet se montre : une
// compétence qui le nomme, et le paquet d'effet NU (0x01F3) qui l'annonce par
// son seul identifiant. Une table indexée par compétence ne pouvait pas porter
// le second : les impacts Hit3 et Hit4, que l'on voit à chaque coup porté, ne
// sont la compétence de personne.

// PAR QUEL DESSINATEUR le nœud se montre.
//
// Un effet en pose souvent de plusieurs sortes à la fois — Heal dresse une
// colonne de lumière ET lâche des particules —, et les deux ne se peignent pas
// par le même chemin : l'un est un sprite d'acteur, l'autre de la géométrie du
// monde.
enum class EffectNodeShape : std::uint8_t {
  kSprite,
  // Un tronc de cône debout, entre deux cercles séparés par une hauteur.
  kRingBand,
  // Une couronne PLATE, entre deux rayons du même plan.
  kRingCylinder,
  // DEUX quads perpendiculaires, qui se croisent le long d'un axe commun.
  kCrossedPlanes,
  // UN quad, orienté par la seconde matrice du nœud.
  kOrientedQuad,
  // UNE CROIX DE POINTES : quatre triangles dressés sur un carré, sous-type 9.
  //
  // ⚠ ELLE ENTRE DANS CETTE TABLE ET NON DANS UNE FAMILLE À PART, parce que ses
  // neuf bâtisseurs n'écrivent aucun champ de piste : tout ce qu'elle dessine
  // vient des champs du nœud, que cette table porte déjà.
  kCrossSpikes,
  // UNE SPHÈRE UV, sous-type 14 : cinquante quads pôle à pôle, dont le sol
  // mange la moitié — le dôme de feu de Magnum Break. Voir `effect_uv_sphere.h`.
  //
  // ⚠ L'EXTRACTION DES ANNEAUX NE CONNAÎT PAS CE DESSINATEUR : son nœud est
  // décrit à la main, voir `NOEUDS_D_ANNEAU_LUS_A_LA_MAIN`.
  kUvSphere,
};

// LES PLAGES QU'UN PATRON TIRE, et dont il porte le NOMBRE DE VALEURS : voir
// `EffectNodeRange::values`. Une vie ou une cadence n'y figurent pas : ce sont
// des entiers du client, dont la plage dit déjà toutes les valeurs.
enum class EffectDrawnRange : std::uint8_t {
  kSpriteScale,
  kAlphaStart,
  kLaunchYaw,
  kLaunchPitch,
  kLaunchDistance,
  kStep,
  kFallSpeed,
  kSwingSpeed,
  kHalfWidth,
  kHalfHeight,
  kLean,
  kOffsetX,
  kOffsetZ,
  kBurial,
  kSpacing,
  kCount,
};
inline constexpr std::size_t kEffectDrawnRangeCount =
    static_cast<std::size_t>(EffectDrawnRange::kCount);

struct EffectNodePattern {
  std::uint32_t effect_id = 0;
  // LE NOM DU BÂTISSEUR qui pose ce nœud chez le client.
  //
  // Il TRACE : d'un effet qui se montre mal on remonte à la description de son
  // bâtisseur. Et il NOMME — « ShockwaveTrap » se lit, « 93 » se cherche.
  std::string_view client_function;
  EffectNodeShape shape = EffectNodeShape::kSprite;
  // CE QUE LE NŒUD MONTRE, selon sa forme : pour un sprite, le couple .spr et
  // .act SANS extension — les deux portent le même nom, et les séparer
  // inviterait à en désaccorder un ; pour une figure du monde, les chemins
  // complets de ses textures dans l'archive.
  //
  // ⚠ PLUSIEURS, et l'index d'image du nœud y choisit : le client tient un
  // TABLEAU et le fait défiler. N'en garder qu'une fige un éclair qui devrait
  // battre. Elles sont rangées dans une LISTE À PLAT, comme les textures d'un
  // effet en fichier ; voir `EffectNodePatternTextures`.
  std::uint16_t first_texture = 0;
  std::uint16_t texture_count = 0;
  // L'INDEX D'ACTION : QUELLE animation du `.act` ce nœud joue.
  //
  // ⚠ DEUX EFFETS PEUVENT PARTAGER LEUR SPRITE ET NE DIFFÉRER QUE PAR LUI. Les
  // identifiants 363 et 364 chargent le même couple `vallentine` et jouent les
  // actions zéro et un ; sans ce champ ils se montreraient à l'identique, le
  // sprite juste et l'animation fausse pour l'un des deux.
  //
  // ⚠ IL SE POSE PAR UN APPEL INDIRECT, si bien qu'aucune rangée ne le tient
  // de l'extraction automatique. Trente-deux bâtisseurs le posent, dix-sept
  // avec autre chose que zéro. Les rangées à zéro ne sont donc pas toutes
  // VÉRIFIÉES : zéro est aussi le défaut du client, et c'est ce que quinze de
  // ces bâtisseurs passent. Voir
  // `scripts/releve_pieces_du_sprite_de_la_saint_valentin.py`.
  std::int32_t action = 0;
  std::int32_t life_ticks = 0;
  // LA BORNE HAUTE D'UNE VIE TIRÉE, et zéro quand il n'y a pas de tirage.
  //
  // ⚠ VINGT-CINQ POINTES NE S'ÉTEIGNENT PAS ENSEMBLE, et cela se voit tout de
  // suite : le client tire la vie de chacune sur une fenêtre de trente tics.
  // Une vie unique les fait toutes disparaître au même instant, ce qui est le
  // contraire d'une figure qui retombe.
  //
  // ⚠ ET C'EST BIEN UNE VIE, pas un porteur : un même bâtisseur y écrit 240,
  // puis pose cette vie moins dix comme tic de sortie — 240 et 230. Un autre
  // bâtisseur DIVISE même par elle, ce qu'on ne fait pas d'un pointeur.
  std::int32_t life_ticks_high = 0;
  // Le tic où la sortie commence, et MOINS UN quand le client n'en pose pas —
  // son constructeur — : le nœud tient alors sa pose jusqu'au bout et s'efface
  // d'un coup. ⚠ ZÉRO EST UN TIC : la particule fond dès sa naissance, sur toute
  // sa vie, comme les étincelles des impacts.
  std::int32_t fade_out_tick = -1;
  // À QUEL TIC DE L'EFFET ce nœud naît : le premier, le dernier, et la période.
  //
  // ⚠ UN EFFET N'EST PAS UN SEMIS MAIS UNE SUITE DE SEMIS. Le client rappelle
  // son bâtisseur à CHAQUE tic et garde chaque semis derrière une condition sur
  // le compteur : Heal pose sa colonne au tic zéro et lâche une étincelle tous
  // les neuf tics jusqu'au centième. Tout semer d'un coup fait naître ensemble
  // ce qui devait couler, et cent quatorze identifiants sont dans ce cas.
  //
  // Une période NULLE veut dire « une seule fois, au premier tic » — et c'est le
  // cas le plus fréquent. Une période n'est jamais DÉDUITE : elle vient d'un
  // modulo du client, faute de quoi le nœud naît une fois.
  std::int32_t first_tick = 0;
  std::int32_t last_tick = 0;
  std::int32_t period_ticks = 0;
  // COMBIEN DE FOIS LE CLIENT SÈME CE NŒUD, d'après la boucle qui entoure son
  // bâtisseur.
  //
  // 🔴 UN COMPTE FAUX SE VOIT : une nuée trop dense, une gerbe trop maigre —
  // une étincelle d'impact au lieu de quatre ne ressemble pas à un effet
  // incomplet mais à un effet FIGÉ.
  //
  // ⚠ UN VEUT DIRE DEUX CHOSES : le client sème vraiment une fois, OU la boucle
  // n'a pas su se lire. Un compte CONNU passe donc devant, et l'unité laisse la
  // place à la règle par défaut — douze nœuds dès qu'un patron porte une
  // dispersion, un sinon.
  std::uint16_t copies = 1;
  // LES TEXTURES SE DISTRIBUENT SUR LES EXEMPLAIRES, une chacune, en boucle.
  //
  // ⚠ À NE PAS CONFONDRE avec le défilement d'images d'un même nœud, que porte
  // `texture_count` : ici chaque exemplaire garde UNE texture pour toute sa
  // vie, et c'est son RANG dans la boucle qui la choisit. `EF_HIT2` alterne
  // ainsi `lens1` et `lens2` sur ses huit reflets, selon la parité du rang.
  // Faux pour tout le reste, et c'est le cas ordinaire.
  bool textures_per_copy = false;
  float sprite_scale = 1.0F;
  // LA BORNE HAUTE D'UNE ÉCHELLE TIRÉE AU SORT, et zéro quand il n'y a pas de
  // tirage. Vingt-huit nœuds connus tirent la leur, et sur de larges
  // fourchettes — de six dixièmes à un et demi pour la plus fréquente. Une
  // valeur unique ferait une nuée de sprites tous de la même taille, ce qui se
  // voit tout de suite : le client en fait une nuée de tailles mêlées.
  float sprite_scale_high = 0.0F;
  // DE COMBIEN L'ÉCHELLE CHANGE À CHAQUE TIC : les mouvements des sous-types
  // cinq et sept l'ajoutent à l'échelle. La boule de feu de Sight
  // part de deux et demi et perd un dixième par tic ; sans ce pas, la traînée
  // de vingt boules avait la même taille du bout à l'autre.
  float sprite_scale_step = 0.0F;
  // Le plafond d'alpha. Le client ne le pose pas toujours ; sans lui le nœud
  // paraît d'emblée à plein.
  float alpha_max = 255.0F;
  // L'ALPHA DE DÉPART, et de combien il monte à chaque tic.
  //
  // ⚠ UN NŒUD NE PART PAS TOUJOURS À SON PLAFOND. Vingt-six des soixante-dix-neuf
  // nœuds d'anneau connus partent d'un alpha bas et MONTENT — la colonne de Heal
  // met trente tics à paraître —, et les poser d'emblée à plein remplace une
  // apparition par un surgissement.
  //
  // Le départ vaut 255 par défaut parce que c'est ce que pose le constructeur du
  // client : un nœud qui ne dit rien de son alpha est déjà là.
  float alpha_start = 255.0F;
  float alpha_step = 0.0F;
  // Le rayon où les nœuds se dispersent autour du point, en unités du monde.
  float scatter = 0.0F;
  // ── LE LANCER ────────────────────────────────────────────────────────────
  //
  // 🔴 CE QUI SÉPARE UNE NUÉE D'UNE GERBE. Sans ces champs, les particules se
  // posent à plat autour du point et n'en partent jamais : l'effet montre
  // toutes ses pièces et paraît FIGÉ. Voir `EffectNodeSeed` pour le modèle.
  //
  // L'écart de cap est relatif à la face du PORTEUR, à laquelle l'appelant
  // l'ajoute : le client compose les deux.
  float launch_yaw_low = 0.0F;
  float launch_yaw_high = 0.0F;
  float launch_pitch_low = 0.0F;
  float launch_pitch_high = 0.0F;
  float launch_distance_low = 0.0F;
  float launch_distance_high = 0.0F;
  float launch_drop = 0.0F;
  // LE PAS, et la part de lui-même qu'il perd par tic — rapportée à la vie.
  float step_low = 0.0F;
  float step_high = 0.0F;
  float step_decay_share = 0.0F;
  // LA CHUTE, et elle seule sépare la famille qui S'ÉCARTE de celle qui
  // RETOMBE : les deux partagent sprite, échelle et pas.
  float fall_speed_low = 0.0F;
  float fall_speed_high = 0.0F;
  float fall_acceleration_share = 0.0F;
  // LE BALAYAGE DE NAISSANCE : un nœud qui naît sur un cercle, à un angle qui
  // AVANCE avec l'âge de l'effet — et non tiré au sort. La boule de feu de Sight
  // naît un tic sur deux à moins cinq degrés par tic, sur un rayon de quinze :
  // c'est la succession des naissances qui fait le tour, chaque sprite restant
  // où il est né. Nul quand le nœud naît à son point.
  float birth_sweep_radius = 0.0F;
  float birth_sweep_degrees_per_tick = 0.0F;
  // LE PROJECTILE VISÉ : voir `EffectNodeSeed::aims_at_target`. Le pas et le cap
  // de ces nœuds-là sont calculés — les deux viennent de la cible.
  bool aims_at_target = false;
  // 🔴 ET LE SPRITE SE TOURNE SUR SON VOL, quand son bâtisseur écrit la FACE du
  // nœud. Viser n'est pas s'orienter : un nœud visé PART vers sa cible, et son
  // image peut très bien rester droite. Neuf bâtisseurs du client font les deux
  // — ils tournent le nœud vers la cible puis écrivent sa face à « cent
  // quatre-vingts moins la face » —, et trois d'entre eux posent un
  // sprite que nous portons : les boules de feu de Fire Ball (24), le projectile
  // (80) et les flammes de Fire Bolt (751).
  //
  // ⚠ SANS LUI, LA BOULE POINTE VERS LE HAUT quelle que soit la direction du
  // tir : son image est dessinée debout, et c'est la rotation qui la couche le
  // long du vol. Le joueur l'a vu en se tenant DE CÔTÉ par rapport à sa cible —
  // le cas où l'écart entre l'image droite et l'image tournée est le plus grand.
  //
  // ⚠ ET L'ANGLE SE CALCULE À CHAQUE IMAGE, pas au semis : c'est un angle
  // d'ÉCRAN, et il change quand la caméra tourne. Le figer à la naissance —
  // comme le fait la flèche d'arc — donne une image juste tant qu'on ne tourne
  // pas la vue.
  bool turns_with_flight = false;
  // 🔴 VRAI QUAND LE LACET DU NŒUD EST CELUI DU PORTEUR, et faux quand le
  // bâtisseur écrit le sien. Il ne concerne que les figures qui lisent leurs
  // angles comme une ORIENTATION — un anneau, une croix ; une particule
  // lancée compose toujours avec son porteur, le client ne connaissant pas
  // d'écart de cap qui n'y soit relatif.
  //
  // ⚠ LA DISTINCTION SE LIT, ELLE NE SE DEVINE PAS, et l'ajouter partout serait
  // pire que de ne l'ajouter nulle part : sur les quatre-vingt-douze nœuds
  // d'anneau connus, vingt-trois seulement sont « calculés » ; neuf portent une
  // constante — dont un quarante-cinq —, huit tirent leur lacet au sort, et
  // cinquante-deux n'y touchent pas du tout.
  bool node_yaw_follows_owner = false;
  // 🔴 VRAI QUAND LA VIE DU NŒUD EST CELLE DE L'EFFET, et non une constante que
  // ce patron porterait. Cela se dit par nœud — `vie_tics: "calculé"` —,
  // et c'est la BONNE granularité : un bâtisseur peut copier la durée pour un
  // nœud et poser cinquante pour le suivant, si bien que son « espèce » ne
  // répond pas pour lui.
  //
  // ⚠ SANS CELA, UN EFFET QUI DURE S'ÉTEINT. `EF_FIREPILLARON` vit huit mille
  // tics chez le client et mourait chez nous aux cent du constructeur — une
  // seconde et demie —, ce qui se voit comme un effet qui DISPARAÎT et non
  // comme une vie mal lue. Voir `EffectNodeLifeFor`.
  bool life_from_effect = false;
  // 🔴 LE REBOND AU SOL, et il appartient au SOUS-TYPE 6 tout entier. Sa mise à
  // jour RETOURNE la vitesse verticale divisée par un et demi dès que la
  // particule repasse sous le terrain. Sans lui, une gerbe qui retombe traverse
  // le sol et s'éteint dessous.
  bool bounces_at_floor = false;
  // ET LE BRUIT DE CE REBOND, quand le nœud le porte : seul EF_COIN le pose
  // sur ses pièces.
  //
  // ⚠ CE N'EST PAS UN SON D'EFFET ORDINAIRE : ceux-là se jouent au répartiteur,
  // au bâtisseur ou à l'émetteur, et celui-ci est joué par la mise à jour du
  // NŒUD — un quatrième étage, dont c'est le seul cas connu.
  //
  // ⚠ ET LE NOM SE FORME : `%d` y prend un tirage de un à `bounce_sound_draws`
  // (`rand() % 3 + 1`). Le figer sur une variante ferait sonner les
  // cinq pièces à l'identique.
  std::string_view bounce_sound;
  std::uint8_t bounce_sound_draws = 0;
  // LA TEINTE, de zéro à 255 par canal, et de quoi elle monte à chaque tic.
  //
  // BLANCHE par défaut, comme le constructeur du client la pose : un nœud sans
  // teinte ne se colore pas, sa texture parle seule. Celle de Heal est de
  // celles-là ; celle de Blessing ne l'est pas.
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  std::array<float, 3> color_step = {};
  // Vide pour un sprite.
  EffectRingPattern ring;

  // LE FREIN LE LONG DE L'AXE : le mouvement du sous-type 8 ne fait pas TOMBER
  // sa vitesse verticale, il la RETRANCHE à son pas — `avance = pas − chute`.
  // Une boule de Soul Strike part donc lentement et accélère jusqu'à sa cible,
  // sans descendre. Lue comme une chute, la même vitesse la faisait plonger.
  bool brakes_along_axis = false;
  // LE BALANCEMENT : un écart perpendiculaire au vol, `Rot(axe, roulis) ·
  // (0, tangage, 0)`, dont le tangage part à sa vitesse initiale par tic et
  // freine de `−(vitesse / vie) · part`. C'est lui qui fait l'arc
  // d'une boule, et le roulis qui éventaille N boules autour de leur axe.
  float swing_speed_low = 0.0F;
  float swing_speed_high = 0.0F;
  float swing_acceleration_share = 0.0F;
  // LES SEMIS SUIVENT LE PARAMÈTRE DE L'INSTANCE, que le gestionnaire de coup
  // pose au NOMBRE DE COUPS du paquet pour un type multi-coups, et à un sinon.
  // Le bâtisseur n'en sème
  // que `min(paramètre, plafond)`, et AUCUN à zéro — c'est son `switch`.
  bool sowings_follow_param = false;
  std::uint8_t param_cap = 0;
  // LA PHASE DE LA PÉRIODE : le client écrit `(âge + 5) % 11` pour Soul Strike,
  // et le semis tombe donc au sixième tic, non au onzième. Voir
  // `EffectNodeSchedule::phase_ticks`. Nulle pour tout patron sans lecture.
  std::int32_t period_phase_ticks = 0;
  // COMBIEN DE TRACES LA TRAÎNÉE GARDE — UN par défaut : le sprite se dessine
  // alors une fois. Le dessinateur des sprites peint le nœud à chacune de ses N
  // dernières positions, alpha et échelle décroissants, et c'est ce qui fait
  // le sillage d'une boule. Douze bâtisseurs l'élèvent, de deux à douze : Soul
  // Strike en pose douze, les impacts Hit3 et Hit4 trois. Voir
  // `effect_trail_history.h`.
  //
  // ⚠ CE N'EST PAS UN NOMBRE D'EXEMPLAIRES : les N copies sont le MÊME nœud à
  // ses N derniers tics.
  std::int32_t trail_trace_count = 1;
  // LE SON DE L'EFFET SE JOUE À CHAQUE SEMIS DE CE PATRON, et non une fois à la
  // naissance de l'effet : le bâtisseur de Soul Strike joue son son DANS la
  // garde du semis, juste avant de bâtir la boule — cinq coups, cinq sons, et
  // rien au tic zéro. Faux partout ailleurs : le son se joue alors une fois,
  // au départ de l'effet.
  bool sound_at_sowing = false;
  // À QUEL TIC DE L'EFFET le nœud meurt, quel que soit le tic où il naît —
  // zéro quand le bâtisseur ne le dit pas, ce qui est le cas de presque tous.
  //
  // 🔴 CE N'EST PAS UNE DURÉE MAIS UN RENDEZ-VOUS. Le bâtisseur de Storm Gust
  // écrit « deux cent quinze moins l'âge » dans chaque cristal :
  // ses quatre vagues s'éteignent donc ENSEMBLE, à la fin du blizzard. Une
  // durée fixe les fait retomber l'une après l'autre, et la dernière survit à
  // la tempête.
  std::int32_t life_until_tick = 0;
  // DE COMBIEN DE DEGRÉS LE NŒUD TOURNE AUTOUR DE SON POINT DE NAISSANCE À
  // CHAQUE TIC.
  //
  // 🔴 CE N'EST PAS UNE ROTATION SUR SOI-MÊME. Le client fait tourner le CAP du
  // nœud, et sa position se recalcule depuis ce cap et sa distance au centre :
  // la particule ORBITE. Les étincelles d'un point de téléportation en vivent —
  // sans ce champ elles restent où elles sont nées, ce que le joueur a nommé
  // « particules immobiles ».
  //
  // ⚠ IL NE FAIT RIEN SANS DISPERSION : une particule née au centre tourne
  // autour d'elle-même, c'est-à-dire ne bouge pas. Voir `EffectNodePattern::scatter`.
  //
  // ⚠ ET LE CLIENT EN TIRE LE SIGNE ET LA GRANDEUR par nœud — ±(2,5 − 0,1 ×
  // rand()%11), soit un et demi à deux degrés et demi dans un sens ou l'autre.
  // Nous n'en portons qu'UNE valeur : toutes les particules d'un effet tournent
  // donc du même côté et à la même vitesse. L'écart se voit sur une troupe
  // dense ; il est écrit ici plutôt que tu.
  float orbit_degrees_per_tick = 0.0F;
  // DE COMBIEN LE BRAS DE L'ORBITE S'ALLONGE À CHAQUE TIC — le mouvement
  // l'intègre dans la distance au centre avant de recomposer la position. Son
  // propre freinage vaut zéro par défaut : le bras s'allonge d'un pas constant.
  float orbit_radius_step_per_tick = 0.0F;
  // LE BÂTISSEUR A-T-IL BAKÉ LA DISPERSION EN DÉCALAGE FIXE, en plus du bras
  // qui tourne ?
  //
  // 🔴 ALORS LA DISPERSION COMPTE DEUX FOIS. Le client range la position d'un
  // sprite en trois morceaux — `parent + décalage + RotY(cap) · (0, 0, rayon)`
  // — et certains bâtisseurs écrivent `RotY(cap) · (0, 0, rayon)` DANS le
  // décalage à la naissance. Le mouvement ajoute ensuite le même vecteur une
  // seconde fois : le nœud naît à deux fois sa dispersion du centre, et son
  // cercle est centré non sur le centre de l'effet mais sur le point baké.
  //
  // ⚠ SANS ORBITE, LES DEUX SE CONFONDENT — un bras qui ne tourne pas est une
  // dispersion doublée, et c'est ainsi qu'EF_HEALSP et EF_EXIT le portent, par
  // une dispersion écrite au double dans la lecture à la main. Dès que le cap
  // tourne, les deux morceaux divergent et il faut les tenir séparés.
  bool scatter_counts_twice = false;
  // 🔴 L'ANIMATION DU SPRITE, qu'aucun champ ne portait : un sprite de nœud
  // restait sur la PREMIÈRE image de son `.act`. Le client fait avancer l'image
  // de son action à chaque tic de mouvement, sous trois réglages du nœud :
  //
  //   · tous les combien de tics l'image change — quatre par défaut, que ZÉRO
  //     désigne ici ;
  //   · la boucle, active par défaut : l'action BOUCLE. Sans elle, le mouvement
  //     rend faux à la dernière image et le nœud MEURT, quelle que soit sa vie —
  //     sauf un mouvement qui ne lit pas ce retour : voir
  //     `EffectNodeSupplement::rests_on_first_frame` ;
  //   · le bit 0x800 des drapeaux, qui le FIGE sur cette dernière image à la
  //     place.
  //
  // Le compte d'images, lui, n'est pas ici : c'est le `.act` qui le dit.
  std::int32_t sprite_frame_hold_ticks = 0;
  bool sprite_plays_once = false;
  bool sprite_holds_last_frame = false;
  // LES BORNES HAUTES DE DEUX TIRAGES, et zéro quand il n'y en a pas : la
  // cadence d'image (`rand() % 5 + 3`) et l'alpha de DÉPART (`rand() % 71 +
  // 70`). Chaque nœud tire les siens.
  std::int32_t sprite_frame_hold_ticks_high = 0;
  float alpha_start_high = 0.0F;
  // 🔴 LE PAS D'ÉCHELLE DÉRIVÉ : `pas = part × échelle / vie`, calculé sur
  // l'échelle et la vie TIRÉES du nœud. Les étincelles des impacts écrivent une
  // part de moins un (Hit3) : elles se réduisent à rien sur leur
  // vie. Aucun nombre fixe ne le dit — ses deux termes sont tirés —, et c'est le
  // semis qui le calcule. Zéro : pas de pas dérivé, `sprite_scale_step` vaut seul.
  float sprite_scale_step_share = 0.0F;
  // L'ACTION TIRÉE, quand le bâtisseur la tire. Deux formes chez la fabrique
  // commune : une PLAGE — `rand() % 8`, de `action` à `action_high` — ou PILE
  // OU FACE entre deux — `rand() & 1 ? 4 : 0`, entre `action` et
  // `action_alternate`. Moins un : pas d'alternative.
  std::int32_t action_high = 0;
  std::int32_t action_alternate = -1;
  // 🔴 LA LOI DE L'AMPOULE : un SEUL tirage décide de la couleur ET de
  // l'allumage. L'effet 165, les guirlandes de Lutie,
  // tire `rand() % 10` puis en sort deux choses — l'action par la moitié du
  // tirage, soit l'une des cinq couleurs d'ampoule du sprite, et une échelle de
  // un CENTIÈME quand le reste par trois vaut un, ce qui éteint l'ampoule.
  //
  // ⚠ CE N'EST NI LA PLAGE NI LE PILE OU FACE ci-dessus : leurs lois diffèrent
  // (`rand() % 8`, `rand() & 1`), et les emprunter fausserait le tirage — sept
  // ampoules allumées sur dix, réparties sur cinq couleurs, est un compte qui se
  // voit sur une guirlande.
  //
  // ⚠ ET IL SE REJOUE À CHAQUE SEMIS. Le semeur de la carte relance l'effet
  // toutes les cent vingt-six pas, soit deux secondes : c'est ce qui fait
  // CHANGER les couleurs au lieu de les figer.
  bool draws_bulb = false;
  // 🔴 L'ÉCHELLE DU SPRITE VIENT DU PREMIER PARAMÈTRE DE L'OBJET DU RSW.
  //
  // Les quatre paramètres d'un objet d'effet de la carte arrivent sur
  // l'instance, et l'effet 47, les torches, prend le
  // premier pour échelle — un s'il est nul ou négatif. Il va de 0,04 à 1,7
  // selon la torche : `sprite_scale` ne peut donc pas le porter.
  //
  // ⚠ CE N'EST PAS UNE LOI GÉNÉRALE. Chaque cas lit ce qu'il veut de ces quatre
  // nombres : les halos de minerai de manuk y prennent le RAYON de leur arc, et
  // les guirlandes de Lutie n'y touchent pas — leur échelle se tire. L'appliquer
  // partout écraserait les deux autres lois.
  //
  // ⚠ ET SANS SEMEUR, IL N'Y A PAS DE PARAMÈTRE : une torche lancée autrement
  // que par la carte garde `sprite_scale`, c'est-à-dire le un du client.
  //
  // ⚠ LE SECOND PARAMÈTRE, lui, donne la cadence : voir
  // `sprite_frame_hold_from_instance`.
  bool sprite_scale_from_instance = false;
  // LES DRAPEAUX D'UN SPRITE, réduits aux bits dont le sens est VÉRIFIÉ pour
  // lui — un drapeau ne veut dire quelque chose que pour son dessinateur
  // (ADR 0017) :
  //   · 0x80, le VA-ET-VIENT d'alpha entre ses bornes, que la mise à jour de
  //     couleur applique à tout nœud — les étincelles de Blessing clignotent
  //     ainsi ;
  //   · 0x200, la COULEUR DU CALQUE : le dessinateur des sprites ignore alors la
  //     teinte et l'alpha du nœud.
  // Les autres bits commandent le mouvement du sous-type, et ce sont ses
  // champs de lancer qui le portent.
  std::uint32_t sprite_flags = 0;
  // LE MOT DE RENDU : un par défaut, puis ce que le bâtisseur
  // pose ou lève (`|=`). Il décide du mélange, de l'épreuve de profondeur et de
  // l'ordre de peinture — voir `effect_render_word.h`.
  std::uint32_t render_word = kEffectRenderWordOfConstructor;
  // LA VARIATION ABSOLUE DU PAS, quand le bâtisseur la pose en dur au lieu d'une
  // part de la vie : les rayons d'EF_DECAGI partent à l'arrêt et prennent 0,015
  // par tic. Voir `EffectNodeSeed::step_change_per_tick`.
  float step_change_per_tick = 0.0F;
  // LE PAS D'UN PROJECTILE VISÉ, en multiples de `distance / vie` : un pour
  // l'âme de Soul Strike, qui arrive à sa mort ; DEUX pour les arcs de
  // particules lancés vers une cible (`pas = distance/vie + distance/vie`).
  float aim_step_factor = 1.0F;
  // LE ROULIS D'UN EXEMPLAIRE SELON SON RANG DANS LA BOUCLE du bâtisseur :
  // `départ + pas × rang`. Les arcs en sèment jusqu'à trois d'un coup, roulés
  // de −90, 0 et +90. Zéro et zéro : aucun.
  float copy_roll_start_degrees = 0.0F;
  float copy_roll_step_degrees = 0.0F;
  // 🔴 CE QUE LE BÂTISSEUR AJOUTE À LA VIE QU'IL COPIE. `life_from_effect` dit
  // que la vie du nœud est celle de l'instance ; il ne dit pas que le bâtisseur
  // n'y ajoute rien. La grille de pointes de terre écrit
  // `vie du nœud = vie de l'effet − 10 + tirage(30)` : la pointe la plus
  // courte meurt AVANT son effet, et c'est justement celle-là qui garde son
  // opacité jusqu'au bout.
  //
  // ⚠ IL LUI FAUT SA PROPRE COLONNE, et le porter dans la vie du patron ne
  // suffirait pas : le semis REFAIT la substitution — `EffectNodeLifeFor` à la
  // place de `life_ticks` — et l'écraserait. Zéro veut dire « telle quelle »,
  // le cas de toutes les autres rangées.
  std::int32_t life_offset_from_effect = 0;
  // LE SPRITE TOURNE SUR LUI-MÊME, de tant de degrés par tic, autour du centre de
  // chacun de ses calques — l'angle s'ajoute à celui que chaque calque porte
  // déjà. Une bouffée de fumée tourne ainsi lentement pendant qu'elle monte.
  // Zéro : le sprite ne tourne pas.
  float sprite_spin_per_tick = 0.0F;
  // LE NOMBRE D'EXEMPLAIRES SE TIRE à chaque semis, de `copies` à `copies_high`
  // compris : une fumée lâche ainsi d'une à quatre bouffées. Une borne qui ne
  // dépasse pas `copies` laisse le compte fixe.
  std::uint16_t copies_high = 0;
  // 🔴 L'INCLINAISON SE COMPOSE AVANT LE CAP. Le lancer ordinaire tourne d'abord
  // selon le cap, puis incline dans le plan que le cap a choisi : un cap de
  // cent quatre-vingts degrés y retourne donc la particule. Avec ce drapeau, la
  // particule s'incline d'abord autour de l'axe latéral, puis le cap fait tourner
  // le tout autour de la verticale : une inclinaison proche du quart de tour la
  // fait MONTER quel que soit le cap, penchée vers lui.
  bool launch_pitch_before_yaw = false;
  // LA RONDE D'UNE APPARITION, quand le nœud en est une : le sprite tourne,
  // flotte, paraît et choisit son action lui-même. Voir `effect_haunt.h`.
  EffectHauntFlight haunt_flight = EffectHauntFlight::kNone;
  // 🔴 L'EFFET SUIT SON PORTEUR, SES NŒUDS NON. Chaque semis part de la place
  // où le porteur est À CET INSTANT, mais le nœud né garde la sienne : son
  // mouvement repart du point figé à sa naissance, jamais de celui du porteur.
  // La neige qui accompagne le joueur tombe ainsi là où elle est née, au lieu
  // de glisser avec lui quand il marche.
  bool nodes_stay_where_sown = false;
  // 🔴 LE VOL RETOURNÉ : le nœud naît sur la CIBLE et vise l'instance. Les
  // boules bleutées de l'effet 378 partent de ce que le lanceur vise et
  // reviennent vers lui ; semées au lanceur, elles volaient dans le mauvais
  // sens. Le pas reste `facteur × distance / vie` — elles arrivent à leur mort.
  bool flies_from_target = false;
  // LA TEINTE TIRÉE : chaque canal se tire de `color` à `color_high` compris,
  // à chaque nœud. Un canal dont la borne haute ne dépasse pas la basse garde
  // sa valeur. Tout à zéro : pas de tirage — le cas de presque tous.
  std::array<std::uint8_t, 3> color_high = {0, 0, 0};
  // ET UN SEUL TIRAGE POUR LES TROIS CANAUX : la teinte reste alors un GRIS, du
  // sombre au clair. Tirés à part, les trois canaux feraient une nuée bariolée.
  bool color_draw_shared = false;
  // LE CAP D'UN EXEMPLAIRE SELON SON RANG DANS LA BOUCLE : l'exemplaire k naît
  // à l'angle du client `k × pas`, au lieu d'un cap tiré, et dans la direction
  // `(sin, 0, −cos)` que son bâtisseur en tire — voir
  // `EffectClientSinCosHeadingInWorld`. Les huit boules de Sight Trasher
  // partent ainsi en rosace, un exemplaire tous les moins quarante-cinq degrés.
  // Zéro : le cap se tire, comme pour toute nuée.
  float copy_heading_step_degrees = 0.0F;
  // 🔴 CE QUI DÉPEND DU TIC DE SEMIS. Un bâtisseur peut écrire dans chaque nœud
  // une valeur tirée de l'âge de l'effet au moment où il le sème : la vague
  // suivante naît plus loin, plus haut, plus petite. Les deux diviseurs sont
  // ENTIERS, comme chez le client — `tic / 3` vaut 1 au tic cinq, et le lire
  // en flottant décalerait chaque vague d'une fraction qui n'existe pas.
  // Zéro : la valeur du patron vaut pour toutes les vagues.
  //   · la dispersion gagne `tic / diviseur` ;
  //   · la chute au départ gagne `tic / diviseur` ;
  //   · l'échelle gagne `pas × tic`, en flottant.
  std::int32_t scatter_sowing_tick_divisor = 0;
  std::int32_t launch_drop_sowing_tick_divisor = 0;
  float sprite_scale_per_sowing_tick = 0.0F;
  // 🔴 LA VOIE RADIALE ACCÉLÉRÉE : le nœud file le long de son cap, sa vitesse
  // s'intègre à chaque tic, puis — au rendez-vous — sa vitesse est REMPLACÉE
  // par une fraction d'elle-même et une dérive constante s'y ajoute. C'est le
  // bit 0x400 des drapeaux du sous-type 5 chez le client.
  //
  // ⚠ UN RENDEZ-VOUS, PAS UNE DURÉE : la bascule tombe au même tic de l'EFFET
  // pour toutes les vagues, comme `life_until_tick`. Une vague semée plus tard
  // bascule donc plus tôt dans sa propre vie.
  //
  // ⚠ LA DÉRIVE EST DANS LE REPÈRE DU CLIENT, la même pour tous les caps : la
  // rosace glisse d'un bloc vers un côté du monde après la bascule.
  //
  // Une vitesse nulle : pas de voie, le cas de tous les autres patrons.
  float radial_speed = 0.0F;
  std::int32_t radial_switch_until_tick = 0;
  float radial_speed_after_switch = 0.0F;
  std::array<float, 3> radial_drift_after_switch = {};
  // 🔴 LA CADENCE D'IMAGE VIENT DU SECOND PARAMÈTRE DE L'OBJET DU RSW : le cas
  // l'écrit tronqué, borné à un tic au moins, par-dessus `sprite_frame_hold_ticks`.
  // Sans semeur le paramètre est nul, et la borne donne un. Les torches (47) et
  // les guirlandes de Lutie (165) la lisent ainsi.
  bool sprite_frame_hold_from_instance = false;
  // LE CAP TIRÉ SUR LA GRILLE 7×7 DU CLIENT, avant tout autre tirage du nœud, à
  // la place du cap continu du lancer. Voir `DrawEffectClientGridHeading`.
  bool launch_heading_on_client_grid = false;
  // L'ÉCHELLE TIRÉE EN PAS ÉGAUX : `rand() % n` pas de `sprite_scale` à
  // `sprite_scale_high`, bornes comprises — `rand() % 6 × 0,1 + 1` en fait six.
  // Zéro : le tirage reste continu.
  std::int32_t sprite_scale_steps = 0;
  // 🔴 LE CAP VISÉ PASSE PAR LA TABLE DU CLIENT : le nœud se tourne vers la
  // cible, la face reçoit `aim_face_offset_degrees`, et le cap de vol est
  // l'opposé de cette face, TRONQUÉ au degré et lu dans la table — un angle
  // négatif s'y lit un degré à côté. Le pas, négatif, ramène le vol vers la
  // cible. Voir `EffectClientAimedHeading`.
  bool aim_heading_on_client_table = false;
  float aim_face_offset_degrees = 0.0F;
  // LE SPRITE VISÉ MEURT À TROIS UNITÉS AU SOL DU POINT VISÉ, dans le tic où il
  // y passe — et non au dernier tic de sa vie : un vol écarté de la cible ne
  // l'approche jamais assez et vit alors toute sa vie.
  bool dies_near_target = false;
  // 🔴 LE NOMBRE DE VALEURS DE CHAQUE PLAGE TIRÉE, rangé par `EffectDrawnRange`.
  // Zéro : le relevé ne l'a pas lu, et la plage se tire en continu. Voir
  // `EffectNodePatternRange`.
  std::array<std::uint16_t, kEffectDrawnRangeCount> draw_values = {};
  // LE CAP TIRÉ SE PASSE DE LA FACE DU PORTEUR : le bâtisseur écrit
  // `rand() % 360`, sans la face, là où les gerbes d'impact écrivent
  // `face − 40 + rand() % 80`.
  bool launch_yaw_is_absolute = false;
  // L'ACTION DU SPRITE MONTRE LA DIRECTION DU POINT VISÉ, vue de la caméra :
  // voir `EffectAimQuadrantAction`.
  bool action_from_aim_quadrant = false;
  // Et l'action de chaque quart : la fabrique en a deux tables.
  EffectActionsByQuarter aim_quadrant_actions = kEffectArrowActionsByQuarter;
  // CE QUE LES PROJECTILES ET LES ANNEAUX ÉCRIVENT ENCORE : voir `EffectNodeSupplement`.
  EffectNodeSupplement supplement;
};

// UNE PLAGE DU PATRON, avec son nombre de valeurs.
[[nodiscard]] inline EffectNodeRange EffectNodePatternRange(const EffectNodePattern& pattern,
                                                            EffectDrawnRange which, float low,
                                                            float high) {
  return {low, high, pattern.draw_values[static_cast<std::size_t>(which)]};
}

// LE ROULIS D'UNE BOULE SELON LE PARAMÈTRE : `base + écart × rang`, lu dans le
// `switch` d'un bâtisseur — sept cas pour Soul Strike. Le rang est
// celui du semis, un pour le premier.
struct EffectRollByParam {
  std::uint32_t effect_id = 0;
  std::uint8_t param = 0;
  float base_degrees = 0.0F;
  float spread_degrees = 0.0F;
};

// LA TEINTE ET L'ALPHA D'UN SEMIS SELON SON RANG — un pour le premier.
//
// 🔴 UNE TRAÎNÉE S'ÉTEINT PAR L'ARRIÈRE, et c'est le rang qui le dit. Le
// bâtisseur de Fire Ball garde sa PREMIÈRE boule blanche et pleine — il ne
// touche ni à sa couleur ni à son alpha, qui restent ceux du constructeur — puis
// teinte les trois suivantes d'orange en baissant leur alpha de cent quatre-vingts
// à quatre-vingts, sous la garde « tic différent de vingt ». Une seule teinte
// pour les quatre donne quatre boules
// identiques, et la tête ne mène plus rien.
//
// ⚠ CELA NE TIENT PAS DANS LE PATRON, qui n'a qu'une teinte et qu'un alpha : le
// rang n'existe qu'au moment du semis. C'est la même raison qui a donné son
// existence à `EffectRollByParam`.
struct EffectTintByRank {
  std::uint32_t effect_id = 0;
  std::uint16_t rank = 0;
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  float alpha = 254.0F;
  // VRAI QUAND LE RANG NE CHANGE QUE L'ALPHA : les trois boules d'EF_FIREBALL
  // gardent chacune SA teinte — bleue, verte, rouge — et ne s'effacent que par
  // l'alpha. Imposer une couleur de rang les rendrait pareilles.
  bool keeps_color = false;
  // VRAI QUAND CE SEMIS SEUL GARDE LES COULEURS DE SES CALQUES : la tête de la
  // flèche verte de 935 se dessine sans teinte, ses suivantes en vert. Voir
  // `kEffectNodeKeepsLayerColor`, que le patron ne porte donc pas.
  bool keeps_layer_color = false;
};

// Ce que le semis de ce rang porte, ou RIEN quand cet effet ne change pas de
// teinte d'un semis à l'autre — le cas de tous sauf un.
[[nodiscard]] const EffectTintByRank* EffectTintByRankFor(std::uint32_t effect_id,
                                                          std::int32_t rank);

// D'une COMPÉTENCE à l'effet procédural qu'elle joue.
//
// Cette jointure-là ne se déduit pas de `SkillEffectTable`, qui ne garde que
// les effets ayant un FICHIER — ceux-ci n'en ont aucun. Sans elle, une
// compétence procédurale ne trouverait jamais ses nœuds.
struct ProceduralSkillEffect {
  std::uint16_t skill_id = 0;
  std::uint32_t effect_id = 0;
  // SUR LE LANCEUR OU SUR LA CIBLE. Sans ce champ, toute la famille
  // procédurale naîtrait sur la cible, et les compétences que le client joue
  // sur le lanceur partiraient du mauvais bout.
  SkillEffectOn on = SkillEffectOn::kTarget;
};

// Les textures d'un patron, dans l'ordre où le client les fait défiler.
[[nodiscard]] std::span<const std::string_view> EffectNodePatternTextures(
    const EffectNodePattern& pattern);

// LE NŒUD qu'un patron décrit, prêt à vivre et à être composé.
//
// Ici plutôt que chez l'appelant, et c'est ce qui permet de l'ÉPROUVER : la
// table entière se parcourt au banc, et l'on vérifie que chaque patron donne
// une figure qui a une aire. Laissée dans la fenêtre, cette traduction ne se
// vérifierait qu'à l'œil, un effet à la fois.
// ⚠ LE GÉNÉRATEUR N'EST PAS UN DÉTAIL : certaines extensions se TIRENT AU SORT,
// et c'est lui qui dit lesquelles. Voir `EffectRingPattern::half_width_high`.
[[nodiscard]] EffectNode EffectNodeFromPattern(const EffectNodePattern& pattern,
                                               std::uint32_t& generator);

// La même, quand l'appelant ne tient pas de générateur. La graine vient de
// l'IDENTIFIANT DE L'EFFET : la figure est alors la même d'une exécution à
// l'autre, et un test peut la nommer.
[[nodiscard]] EffectNode EffectNodeFromPattern(const EffectNodePattern& pattern);

// VRAI SI CE PATRON SÈME À CE TIC DE L'EFFET.
//
// Un patron sans période sème une seule fois, à son premier tic ; un patron
// périodique sème à chaque multiple de sa période compris entre son premier tic
// et son dernier. Le multiple se compte sur le tic de l'EFFET et non depuis le
// premier semis : c'est ainsi que le client le fait — un modulo sur son
// compteur —, et compter depuis le premier décalerait toute la suite.
[[nodiscard]] bool EffectNodePatternSowsAt(const EffectNodePattern& pattern, std::int32_t tick);

// LE DERNIER TIC OÙ CE PATRON PEUT ENCORE SEMER. C'est lui qui dit à l'effet
// quand se retirer ; sans cette borne, une période sèmerait sans fin.
[[nodiscard]] std::int32_t EffectNodePatternLastSowTick(const EffectNodePattern& pattern);

// COMBIEN DE TICS IL RESTE au nœud semé à ce tic de l'effet.
//
// 🔴 UN NŒUD NE SURVIT PAS À SON EFFET. Le client détruit l'instance et ses
// enfants ENSEMBLE : un rayon semé au soixante-quinzième tic d'un effet qui en
// vit quatre-vingts n'en voit que cinq, et non les cinquante que son patron lui
// donne. Sans cette borne, les rayons d'EF_ENHANCE montaient une seconde
// entière après que leur colonne se soit éteinte.
//
// ⚠ ET SEULEMENT QUAND LA VIE DE L'EFFET EST MESURÉE. Borner contre le défaut
// du constructeur couperait les nœuds de tous les effets dont la durée n'est
// pas connue, sur un nombre qui ne veut rien dire : sans mesure, la réponse
// est « autant qu'il voudra ».
//
// ⚠ ET LE RENDEZ-VOUS DU BÂTISSEUR PASSE DEVANT LA VIE DE L'EFFET, parce qu'il
// est plus précis : voir `EffectNodePattern::life_until_tick`.
[[nodiscard]] std::int32_t EffectNodePatternTicksLeft(const EffectNodePattern& pattern,
                                                      std::int32_t tick);

// LA MÊME, QUAND LE PARAMÈTRE DE L'INSTANCE COMPTE : un patron dont les semis
// suivent le paramètre n'en sème que `min(param, plafond)`, et aucun à zéro.
// Les autres patrons ne le lisent pas.
[[nodiscard]] bool EffectNodePatternSowsAtParam(const EffectNodePattern& pattern, std::int32_t tick,
                                                std::int32_t param);

// 🔴 UN COMPTEUR QUE TOUTES LES INSTANCES D'UN EFFET PARTAGENT. Les anneaux verts
// de Blitz Beat (115) ne se comptent pas par instance : le bâtisseur remet UN
// compteur commun à zéro à la naissance de CHAQUE instance, sème tant qu'il reste
// sous le paramètre de l'instance qui sème, et l'avance d'un par anneau. Deux
// Blitz Beat qui se chevauchent sèment donc contre un seul compte, et la
// seconde relance la première. Aucun plafond : l'horaire seul borne les semis.
// Le compteur vit chez l'appelant ; ceci ne dit que QUI le partage.
[[nodiscard]] bool EffectParamCounterIsShared(std::uint32_t effect_id);

// LE RANG DU SEMIS à ce tic, un pour le premier : c'est lui qui choisit le
// roulis d'une boule. Zéro quand le patron ne sème pas là.
[[nodiscard]] std::int32_t EffectNodePatternSowingRank(const EffectNodePattern& pattern,
                                                       std::int32_t tick);

// LA DISPERSION ET L'ÉCHELLE DU SEMIS de ce tic, celles du patron corrigées de
// ce que le tic de semis y ajoute. Voir
// `EffectNodePattern::scatter_sowing_tick_divisor`.
[[nodiscard]] float EffectNodePatternScatterAt(const EffectNodePattern& pattern, std::int32_t tick);
[[nodiscard]] float EffectNodePatternSpriteScaleAt(const EffectNodePattern& pattern,
                                                   std::int32_t tick);

// VRAI QUAND LE SON DE L'EFFET SE JOUE À CHAQUE SEMIS d'un de ses patrons, et
// non une fois à sa naissance — voir `EffectNodePattern::sound_at_sowing`. Celui
// qui joue le son du départ s'en abstient alors ; c'est la troupe qui sonne.
[[nodiscard]] bool EffectSoundPlaysAtSowing(std::uint32_t effect_id);

[[nodiscard]] std::span<const EffectRollByParam> EffectRollByParamTable();

// Le roulis de cet effet pour ce paramètre, ou rien quand aucun bâtisseur ne
// l'écrit.
[[nodiscard]] const EffectRollByParam* EffectRollByParamFor(std::uint32_t effect_id,
                                                            std::int32_t param);

// Tous les patrons connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectNodePattern> EffectNodePatternTable();

// TOUS les patrons d'un effet, ou un span VIDE — et c'est le cas le plus
// fréquent : la table ne porte que ce qui est complet.
//
// Un effet en a souvent plusieurs, et ne poser que le premier le tronque sans
// rien dire : Heal n'aurait plus que ses particules, ou plus que sa colonne.
[[nodiscard]] std::span<const EffectNodePattern> EffectNodePatternsFor(std::uint32_t effect_id);

// Les compétences qui jouent un effet procédural, par identifiant.
[[nodiscard]] std::span<const ProceduralSkillEffect> ProceduralSkillEffectTable();

// TOUS les effets procéduraux d'une compétence, dans l'ordre du client.
//
// ⚠ CE N'EST PAS CE QU'ELLE MONTRE : le client attache plusieurs effets à une
// même compétence, et nous ne savons dessiner qu'une part d'entre eux. Pour
// l'effet montré, voir `DrawableProceduralEffectOfSkill`.
[[nodiscard]] std::span<const ProceduralSkillEffect> ProceduralEffectsOfSkill(
    std::uint16_t skill_id);

// Savons-nous faire l'image de cet effet ? TREIZE familles de patrons peuvent
// répondre oui, et la question n'a de sens qu'en les interrogeant toutes.
//
// ⚠ ELLE SE POSE AU DESSIN ET NON AU GÉNÉRATEUR. Chaque famille naît d'un
// générateur à part ; aucun d'eux ne voit les douze autres, et celui qui
// prétendait trancher ne voyait que la sienne.
[[nodiscard]] bool KnowsProceduralEffect(std::uint32_t effect_id);

// L'effet qu'une compétence MONTRE : le premier, dans l'ordre du client, dont
// une table sache faire l'image. NUL si aucun.
//
// Sa réponse change dès qu'une famille de plus est portée, et c'est voulu : la
// jointure suit le dessin sans qu'on ait à la réengendrer.
[[nodiscard]] const ProceduralSkillEffect* DrawableProceduralEffectOfSkill(std::uint16_t skill_id);

}  // namespace ro::render
