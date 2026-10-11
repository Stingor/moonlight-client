#pragma once

#include <cstdint>

namespace ro::render {

// LES FORMES DU RÉSERVOIR, dans leur propre en-tête : l'énumération porte
// une fiche par dessinateur, et c'est elle qui tenait l'en-tête du réservoir
// au plafond. Le réservoir l'inclut ; rien d'autre n'a à la nommer.

// PAR QUEL DESSINATEUR un nœud de la troupe se montre.
//
// La troupe les mêle, et il le faut : un même effet sème une colonne de lumière
// et des particules, elles vivent de la même horloge et meurent ensemble. Les
// ranger dans deux troupes les ferait vieillir séparément.
enum class PooledNodeShape : std::uint8_t {
  kSprite,
  kRingBand,
  kRingCylinder,
  kCrossedPlanes,
  kOrientedQuad,
  // Une JUPE d'anneau, dessinée depuis les quatre pistes du nœud et non depuis
  // ses champs.
  kRingTracks,
  // QUATRE CARRÉS sur une sphère qui grandit, eux aussi tirés des pistes.
  //
  // ⚠ LA FORME CHOISIT LE MOUVEMENT autant que la figure : deux formes qui
  // lisent la même piste ne la font pas vivre de la même façon, et les faire
  // battre par le même mouvement donnerait à l'une le rythme de l'autre.
  kRisingParticles,
  // UN QUAD SUR UN ARC DE CERCLE, tiré de l'unique piste vive du nœud.
  //
  // ⚠ CELLE-CI N'EN ALLUME QU'UNE SUR QUATRE, à la différence des deux
  // précédentes. Voir `effect_arc_ribbon_seed.h`.
  kArcRibbons,
  // UN CARRÉ TEINTÉ, tiré des pistes du nœud et animé par la CASCADE.
  //
  // ⚠ SON MOUVEMENT TRAVAILLE SUR LES QUATRE PISTES À LA FOIS, seul de la
  // famille : chacune reçoit la précédente tous les six tics. Voir
  // `AdvanceCascadingTrailTracks`.
  kTintedQuads,
  // UN CHAMP DISPERSÉ : quatre particules qui vivent, meurent et RENAISSENT
  // ailleurs, dessinées par la même figure que le carré teinté.
  //
  // ⚠ SES QUATRE PISTES NAISSENT ÉTEINTES, alpha nul : rien ne se montre tant
  // que son mouvement n'a pas tourné. Voir `effect_scatter_seed.h`.
  kScatterField,
  // UN OBJET QUI TOMBE, suivi de trois crans de plus en plus pâles. Même figure
  // que le carré teinté, et un mouvement qui déplie sa traînée d'un cran par
  // tic. Voir `effect_falling_seed.h`.
  kFallingItem,
  // UN SPRITE DE MONSTRE qui paraît et s'efface, avec son éclat.
  //
  // ⚠ C'EST LE PREMIER NŒUD DE LA FAMILLE QUI PASSE PAR LES ACTEURS et non par
  // les quads du monde. Voir `effect_glow_sprite_seed.h`.
  kGlowSprite,
  // UN ÉVENTAIL : dix carrés emboîtés, chacun à son propre éclat, et pour trois
  // styles un halo coloré par-dessus chacun.
  //
  // ⚠ IL NAÎT INVISIBLE, alpha nul et dix lames éteintes : c'est son mouvement
  // qui l'ouvre lame par lame. Voir `effect_spiral_fan_seed.h`.
  kSpiralFan,
  // UN CÔNE DE PARTICULES : la même figure que l'anneau à pistes, un AUTRE
  // mouvement.
  //
  // ⚠ DEUX FORMES POUR UNE FIGURE, ET C'EST VOULU. La forme lie la figure et le
  // mouvement ; le sous-type 23 dessine comme le 30 et bouge autrement. Les
  // réunir sous une seule forme ferait une figure juste qui bouge faux — ce qui
  // est arrivé, et ce que cette forme-ci répare.
  kParticleConeRings,
  // UNE GERBE À DEUX MODES : encore la même figure, encore un autre mouvement.
  // Voir `AdvanceDualModeSprayTracks`.
  kDualModeSprayRings,
  // DES GROUPES RADIAUX : la même figure encore, et un mouvement où chaque case
  // de la piste suit sa propre règle. Voir `AdvanceRadialParticleGroupTracks`.
  kRadialParticleGroups,
  // L'ÉVENTAIL TOURNÉ VERS LA CAMÉRA, sous-type 68 : la figure de l'éventail du
  // sous-type 147 — les deux y mènent par le MÊME bloc du répartiteur de
  // dessin —, et un mouvement à lui. Voir `AdvanceCameraFanTracks`.
  //
  // ⚠ ET SES TROIS TRAÎNÉES NE PARTAGENT PAS LEUR IMAGE : chacune lit la sienne
  // dans sa marque zéro, si bien que ce nœud ne tient pas dans un seul lot.
  kCameraFan,
  // LES ANNEAUX DE TÉLÉPORTATION, sous-type 57 : encore la figure de l'anneau,
  // et un mouvement qui ne fait pas CROÎTRE le rayon mais le fait RESPIRER.
  //
  // ⚠ ET CHAQUE PISTE Y TOURNE À SA VITESSE, selon son rang. Lui donner la forme
  // du sous-type 30 ouvrirait les quatre anneaux ensemble et ne les refermerait
  // jamais. Voir `AdvanceTeleportRingTracks`.
  kTeleportRings,
  // LE PROJECTILE GUIDÉ, sous-type 42 : même figure que `kTintedQuads`, un vol
  // à lui. Voir `AdvanceHomingTrailTracks`.
  //
  // ⚠ MÊME FIGURE, AUTRE MOUVEMENT, DONC AUTRE FORME. Lui donner celle du
  // sous-type 47 lui ferait faire une cascade tous les six tics sans jamais
  // virer : il partirait droit et ne reviendrait pas.
  kHomingTrail,
  // L'OBJET LANCÉ, sous-type 53 : même figure que `kTintedQuads`, et le FRÈRE du
  // sous-type 42 — même cascade, mais un jet au lieu d'une poursuite, et une
  // hauteur en demi-sinus au lieu d'une pente. Voir `AdvanceThrownItemTracks`.
  //
  // ⚠ IL LUI FAUT UNE CIBLE, et sans elle il ne se sème pas : le bâtisseur tire
  // du couple lanceur-cible le pas, la pente et la durée du vol.
  kThrownItem,
  // UN QUARTIER DE TARTE, sous-type 162 : un triangle de dix degres, pointe au
  // centre. Douze d'entre eux couvrent le tour, semes sur trois tics.
  //
  // ⚠ SON MOUVEMENT EST LE PLUS COURT DE LA FAMILLE : il suit son porteur et
  // avance le compteur de chaque piste vive, rien d'autre. Tout le reste est
  // dans le SEMIS, qui disperse les quartiers. Voir `AdvanceEmitterFrameTracks`.
  kPieWedge,
  // UN FAISCEAU EN PIQUE, sous-type 164 : un ruban plat qui part de l ancre et
  // file droit, en jusqu a six segments. Douze d entre eux ferment le tour, semes
  // sur trois tics comme les quartiers de tarte.
  //
  // ⚠ SON MOUVEMENT LE FAIT GRANDIR EN RECULANT, et fait tourner son image. Voir
  // `AdvanceFourLayerTimerTracks`.
  kSpikeBeam,
  // UNE COLONNE D AURA, sous-type 131 : dix carres emboites qui s ouvrent couche
  // par couche, chacun avec son propre alpha. Voir `AdvanceRingExpansionTracks`.
  kAuraColumn,
  // UN GRAIN QUI REMONTE ET SON HALO, sous-type 157 : quatre grains qui partent
  // loin sous leur noeud, remontent droit et s'eteignent une fois arrives.
  //
  // ⚠ CETTE FORME NE SUIT PAS SON PORTEUR, seule de sa generation : chaque
  // bouffee reste figee la ou son emetteur l'a semee, et c'est ce qui fait une
  // pluie plutot qu'une aura. Voir `AdvanceCameraFacingRadialTracks`.
  kRainParticle,
  // UNE AURA EN ANNEAU DOUBLE, sous-type 106 : un anneau debout qui s'ouvre d'un
  // point au cercle plein, puis RECOMMENCE, et dont chaque segment est peint
  // deux fois pour doubler sa lumiere.
  //
  // ⚠ ELLE SORT PAR LA VOIE B, et c'est la variante du noeud qui decide de sa
  // couleur — pas sa figure. Voir `AdvanceRadialFanTracks`.
  kDoubleRibbon,
  // UNE GERBE DE FEUX D'ARTIFICE, sous-type 52 : quatre grains qui tombent
  // invisibles, s'allument d'un coup et filent en laissant une trace.
  //
  // ⚠ AUCUNE DE SES PISTES NE S'ETEINT : elles RENAISSENT au deux millieme tic.
  // C'est le noeud qui finit par mourir. Voir AdvanceSwirlOrbitTracks.
  kSpiralTrail,
  // UN ARC-EN-CIEL, sous-type 93 : sept bandes de couleur emboitees, tracees
  // segment par segment sur un demi-tour.
  //
  // ⚠ SON PLAN EST FIGE A LA NAISSANCE, et ne suit ni la camera ni le porteur :
  // le batisseur y range le cap de la vue une fois pour toutes. Voir
  // `AdvanceCameraAlignedArcTracks`.
  kRainbowTrail,
  // UNE TRAINEE QUI S EFFACE, sous-type 134 : quatre flocons qui volent du
  // lanceur vers sa cible, chacun suivi de quatre copies de plus en plus pales.
  //
  // ⚠ ELLE A BESOIN DE LA CIBLE : l ancre d une piste est le point VISE, et c est
  // la distance entre les deux qui regle la vitesse. Voir
  // AdvanceFadingStreakTracks.
  kFadingStreak,
  // UN SPRITE QUI ENFLE, sous-type 88 : meme figure que le carre qui respire et
  // que le disque au sol — les trois sous-types menent au meme dessinateur —, et
  // un mouvement qui multiplie son rayon au lieu de le faire respirer.
  //
  // ⚠ SA PISTE PEUT NAITRE EN RETARD, compteur negatif : c'est ce qui echelonne
  // les six boules de foudre d'EF_STASIS. Voir `AdvanceGrowingSpriteTracks`.
  kGrowingSprite,
  // UNE NAPPE AU SOL, sous-type 72 : soixante carreaux jointifs, poses a plat et
  // tournes vers la camera. Voir `AdvanceParticleFrameTracks`.
  kGridPlane,
  // UNE JUPE DONT L'IMAGE DEFILE, sous-type 144 : la jupe d'anneau deja portee,
  // dont le quad choisit son image par le compteur de tics de la piste.
  //
  // ⚠ QUATRE PISTES VIVES, echelonnees par leurs alphas et leurs cles, et NON par
  // leurs images : le noeud n'en porte qu'un jeu. Voir `AdvanceGravityWaveTracks`.
  kCyclingSkirt,
  // LE CARRÉ TEINTÉ QUI MONTE, sous-type 59.
  //
  // ⚠ MÊME FIGURE QUE `kTintedQuads`, AUTRE MOUVEMENT, et c'est pourquoi c'est
  // une forme à part. La forme d'un nœud lie sa figure ET son mouvement : les
  // ranger ensemble ferait monter en cascade ce qui doit s'élever, et le
  // dessinateur partagé ne le signalerait pas. Voir `AdvanceRisingSparkTracks`.
  kRisingSparks,
  // L'ÉTINCELLE DU SOIN, sous-type 58 : une seule piste, qui paraît en vingt
  // tics, tient jusqu'au centième, puis s'efface.
  //
  // ⚠ MÊME FIGURE QUE `kTintedQuads`, AUTRE MOUVEMENT, comme le sous-type 59
  // juste au-dessus. Un seul bâtisseur la monte et un seul effet l'emploie —
  // EF_FIRSTAID. Voir `AdvanceHealSparkleTracks`.
  kHealSparkle,
  // LES CARRÉS QUI MONTENT, sous-type 49 : même figure encore, et un mouvement
  // qui les allume, les fait monter en s'éteignant, puis les reprend au
  // porteur. Voir `AdvanceRisingQuadTracks`.
  kRisingQuads,
  // UNE BANDE QUI DÉFILE, sous-type 79 : cinq quads bout à bout qui glissent le
  // long de leur axe. Ce sont les cascades du client.
  //
  // ⚠ SON MOUVEMENT EST LE PLUS COURT DE LA FAMILLE — il n'avance qu'un compteur
  // par piste —, et c'est la FIGURE qui fait tout le reste à partir de ce
  // compteur. Voir `AdvanceScrollingBandTracks`.
  kScrollingBand,
  // UN VOILE SOMBRE, sous-type 71 : un disque clair en quatre quadrants, cerné
  // de noir, qui s'épaissit d'une unité d'alpha par tic. Ce client le peint à
  // l'écran : voir `ComposeScreenDarkVeilQuads`.
  //
  // ⚠ IL NAÎT INVISIBLE et suit son porteur. Le semer plein ferait tomber la
  // nuit d'un coup ; le planter ferait marcher le joueur hors de son ombre.
  // Voir `AdvanceDarkVeilTracks`.
  kDarkVeil,
  // UN RUBAN TYPÉ, sous-type 149 : le ruban de traînée peint deux fois, la
  // seconde selon un type, et dont les cases ONDULENT.
  //
  // ⚠ IL NAÎT SANS LONGUEUR NI ALPHA : les vingt et une cases de chaque piste
  // sont à zéro. C'est le mouvement qui y met l'onde. Voir
  // `AdvanceSpinningRibbonTracks`.
  kTypedRibbon,
  // UNE TRAÎNÉE PENTAGONALE, sous-type 66 : quatre couches d'un tube à cinq
  // côtés, tendues entre la position de l'effet et un point plus haut.
  //
  // ⚠ SON MOUVEMENT NE FAIT RESPIRER QU'UNE PISTE SUR DEUX. Voir
  // `AdvancePentagonTrailTracks`.
  kPentagonTrail,
  // UN BILLBOARD DE PALETTE, sous-type 132 : un quad debout dont la teinte
  // croise une palette et une nuance.
  //
  // ⚠ SA TAILLE N'EST PAS SEMÉE mais CALCULÉE à chaque tic, et ses pistes
  // RENAISSENT au lieu de mourir. Voir `AdvancePaletteBillboardTracks`.
  kPaletteBillboard,
  // UNE PILE EN SPIRALE, sous-type 103 : des carrés empilés qui tournent et
  // rétrécissent. La spirale vit dans la FIGURE ; le mouvement ne fait tourner
  // que l'ensemble. Voir `AdvanceSpiralStackTracks`.
  kSpiralStack,
  // UN CARRÉ QUI RESPIRE, sous-type 39 : quatre coins sur un cercle dont le
  // rayon oscille au rythme de l'INCLINAISON, et non du compteur de tics.
  // Voir `AdvanceBreathingQuadTracks`.
  kBreathingQuad,
  // UNE EMPREINTE, sous-type 67 : la figure du carré qui respire, sans souffle,
  // qui reste où elle est née et s'efface. Voir `AdvanceFootprintTracks`.
  kFootprint,
  // UN DISQUE AU SOL, sous-type 26 : le cercle magique des cartes, posé à plat
  // et qui tourne d'un degré par tic.
  //
  // ⚠ IL PARTAGE SA FIGURE AVEC LE CARRÉ QUI RESPIRE — les deux sous-types
  // mènent au même dessinateur — mais NI SON SEMIS NI SON MOUVEMENT : son
  // bâtisseur pose d'autres rayons, et son mouvement lit la case de variante
  // pour décider du SENS de rotation. Voir `AdvanceGroundDiscTracks`.
  kGroundDisc,
  // UN COIN DE CÔNE, sous-type 89 : deux triangles concentriques, l'un dans
  // l'autre, et jamais de la même teinte.
  //
  // ⚠ UN EFFET EN POSE PLUSIEURS, un par tour de la boucle de son site : c'est
  // l'éventail. Voir `AdvanceConeWedgeTracks`.
  kConeWedge,
  // UN RUBAN AU SOL, sous-type 56 : un trait posé à plat qui GLISSE vers une
  // cible, et dont le glissement accélère pour la moitié des styles.
  // Voir `AdvanceGroundRibbonTracks`.
  kGroundRibbon,
  // UNE CROIX DE POINTES, sous-type 9 : quatre triangles dressés sur un carré.
  //
  // ⚠ ELLE NE LIT PRESQUE AUCUNE PISTE — tout vient des champs du nœud —, et
  // c'est ce qui l'a rendue si bon marché malgré ses neuf bâtisseurs.
  kCrossSpikes,
  // DES ANNEAUX QUI PULSENT, sous-type 63 : la JUPE D'ANNEAU encore, et un
  // mouvement de plus. C'est la forme qui lie les deux — la figure ne coûtait
  // plus rien, le mouvement a tout coûté.
  //
  // ⚠ TROIS PISTES SUR QUATRE SE DESSINENT ; la quatrième est un REGISTRE qui
  // porte le mode et l'âge global. Voir `AdvanceRadialBurstTracks`.
  kPulsingRings,
  // LES ANNEAUX D'UN PORTAIL DE TÉLÉPORTATION, sous-type 65 : la JUPE D'ANNEAU
  // encore, et un mouvement de plus. Quatre anneaux dont le rayon se referme
  // d'un vingtième par tic et se REMBOBINE à dix quand il passe le centre —
  // c'est ce rembobinage qui rend la figure perpétuelle.
  //
  // ⚠ SES QUATRE PISTES SE DESSINENT, là où les anneaux qui pulsent en gardent
  // une comme registre. Voir `AdvanceWarpRingTracks`.
  kWarpRings,
  // UNE COLONNE QUI TOURNE, sous-type 137 : la jupe d'anneau dressée à la
  // VERTICALE, peinte deux fois — une passe par voie, et c'est le seul
  // dessinateur du lot dans ce cas.
  //
  // ⚠ ELLE N'APPARAÎT QU'AU CENTIÈME TIC de sa piste, et son semis décale les
  // quatre par un retard TIRÉ AU SORT. Voir `AdvanceSpiralColumnTracks`.
  kSpiralColumn,
  // UN CARRÉ CONCENTRIQUE, sous-type 32 : le même carré deux à quatre fois, à
  // des échelles différentes, qui tremble et orbite.
  //
  // ⚠ UN EFFET EN POSE UNE GERBE, et le compte vient de l'EXTÉRIEUR : ce n'est
  // pas le fichier .str qui le donne. Voir `AdvanceOrbitJitterTracks`.
  kConcentricSquare,
  // UN MUR TRIPLE, sous-type 75 : un rectangle debout peint TROIS fois à un
  // degré d'écart, dont deux au tiers de l'alpha. Le halo n'est pas une teinte
  // mais cette répétition. Voir `AdvanceTripleWallTracks`.
  kTripleWall,
  // UNE COQUILLE SPHÉRIQUE, sous-type 61 : quinze panneaux TANGENTS à une
  // sphère, peints sur TROIS rayons voisins — un bouclier bombé, ouvert
  // derrière.
  //
  // ⚠ SON SEMIS N'ALLUME QU'UNE PISTE SUR QUATRE, et son mouvement recale
  // l'azimut sur le PORTEUR à chaque tic. Voir `AdvanceSphereShellTracks`.
  kSphereShell,
  // UNE SPHÈRE UV, sous-type 14, bâtie des seuls champs du nœud ; le sol en
  // mange la moitié basse — le dôme de Magnum Break. Voir `effect_uv_sphere.h`.
  kUvSphere,
  // LE RUBAN A QUATRE BRAS DE GRAND CROSS, sous-type 34 : quatre pistes qui
  // balaient chacune un quart de tour et pendent vers le bas de la longueur que
  // leur mouvement leur donne. Voir `AdvanceGrandCrossRibbonTracks`.
  //
  // ⚠ SES QUATRE PISTES NE DIFFÈRENT PAS QUE PAR L'AZIMUT : leur RANG entre
  // dans l'angle du bras, et leurs deux premières marques — signées — les
  // décalent aux quatre coins. C'est cela qui en fait une croix.
  kGrandCrossRibbon,
  // LE CADRE TOURNANT DE GRAND CROSS, sous-type 37 : un rectangle tourné,
  // extrudé vers le bas, plus deux diagonales à mi-alpha. DEUX pistes.
  kGrandCrossFrame,
  // L'ANNEAU DONT LA CLÉ ONDULE, sous-type 139 : la même jupe que le trente,
  // le même code aux mêmes constantes, et deux différences — son onde bat
  // autour de sa CLÉ et non de son rayon, et l'échelle du nœud saute d'un cran
  // à chaque quart de cent tics. Voir `AdvanceOscillatingRadiusParticleTracks`.
  kOscillatingRadiusParticles,
  // L'EVENTAIL QUI S'OUVRE UNE FOIS, sous-type 108 : chaque segment croit puis
  // se FIGE a sa pleine longueur, marque a l'appui. Voir
  // AdvanceRadialSpreadParticleTracks.
  kRadialSpreadParticles,
  // LA SPIRALE QUI S'OUVRE ET SE FIGE, sous-type 118 : meme figure que la jupe,
  // une cle qui croit geometriquement et un azimut qui derive d'un tirage. Sa
  // QUATRIEME piste a ses propres constantes. Voir AdvanceSpiralParticleTracks.
  kSpiralParticles,
  // L'ARCHE QUI TOURNE ET QUI PULSE, sous-type 24 : meme jupe, et une onde qui
  // MULTIPLIE sa base au lieu de s'y ajouter. Les rangs pairs tournent d'un
  // degre par tic, les impairs de deux. Voir AdvanceTurningArchTracks.
  kTurningArch,
  // UN ESSAIM QUI POURSUIT, sous-types 155 et 170 : des carrés tournés d'un
  // huitième de tour, en DEUX nuées qui se séparent, dont les caps chassent une
  // cible sans cesse renouvelée.
  //
  // ⚠ UNE SEULE FORME POUR DEUX SOUS-TYPES, parce qu'ils ont la même figure et
  // que leurs mouvements ne diffèrent que par trois nombres. Le patron dit
  // lequel des deux. Voir `AdvanceBillboardSwarmTracks` et `EffectSwarmKind`.
  kBillboardSwarm,
  // UNE TENTURE AU SOL, sous-type 48 : un rectangle DEBOUT dont la base est un
  // segment posé au sol, et dont la hauteur RESPIRE au rythme d'un sinus.
  //
  // ⚠ L'OCTET DE SON SITE CHOISIT UNE DISPOSITION ENTIÈRE — gerbe, treillis ou
  // croix —, son alpha et le nombre de pistes vives. Voir
  // `AdvanceGroundCurtainTracks` et `effect_ground_curtain_seed.h`.
  kGroundCurtain,
  // UN TUBE D'ÉTINCELLE, sous-type 100 : un tube à TROIS FACES balayé le long
  // d'une courbe, et la courbe est l'HISTORIQUE d'un angle qui monte.
  //
  // ⚠ SES COINS SONT ABSOLUS — l'ancre de chaque piste est un point du monde,
  // que le mouvement y recopie. Voir `AdvanceSparkTubeTracks`.
  kSparkTube,
  // UNE TAILLADE QUI GLISSE, sous-types 86 et 92 : un CARRÉ PLAT, tourné d'un
  // huitième de tour, qui file dans une direction et ACCÉLÈRE en s'effaçant.
  //
  // ⚠ DEUX FORMES POUR UNE SEULE FIGURE, et c'est voulu : leurs mouvements ne
  // diffèrent que par une branche — le 86 VISE au premier tic, le 92 garde
  // l'azimut de son site —, et RIEN DANS L'ÉTAT DU NŒUD ne les distingue. Le
  // deviner d'un champ ferait viser une taillade sur deux à l'envers ; deux
  // énumérateurs le disent, et le compilateur le vérifie.
  // Voir `AdvanceGlidingSlashTracks`.
  kGlidingSlash,
  kAimedGlidingSlash,
  // UNE RAFALE DE QUADS TEINTÉS, sous-type 29 : QUATRE particules par piste,
  // seize par nœud, chacune le même carré tourné vers la caméra et translaté
  // par son propre décalage qui dérive.
  //
  // ⚠ SA VIE NE FINIT PAS : ce sont des marques de CARTE, que le client
  // renouvelle lui-même. Voir `AdvanceQuadBurstTracks`.
  kQuadBurst,
  // UN ÉCLAT QUI FUIT, sous-type 44 : un BARREAU posé dans un plan VERTICAL,
  // incliné d'un angle dans ce plan, et dont la distance au centre croît à
  // chaque tic.
  //
  // ⚠ IL PART DE L'AUTRE CÔTÉ DU CENTRE — rayon moins dix — et le traverse.
  // Voir `AdvanceFlyingShardTracks`.
  kFlyingShard,
  // UN GRAIN QUI DÉRIVE, sous-type 96 : UN carré par piste, tourné vers la
  // caméra, qui monte en s'écartant de côté et s'éteint par son alpha.
  //
  // ⚠ IL NAÎT SUR UN CERCLE et non au centre, et il NE RENAÎT PAS.
  // Voir `AdvanceDriftingMoteTracks`.
  kDriftingMote,
  // UN TRAIT GAINÉ, sous-type 69 : un prisme à CINQ FACES tendu de l'ancre
  // jusqu'à un bout lointain, peint DEUX FOIS — une âme mince, puis une gaine
  // à peine plus large qui s'ajoute.
  //
  // ⚠ IL NE BOUGE PAS : seul son alpha vit. Voir `AdvanceSheathedBoltTracks`.
  kSheathedBolt,
  // UN CADRE ET UNE BARRE, sous-type 90 : les quatre murs d'une boîte carrée,
  // et une barre qui court sur le périmètre d'un carré à peine plus large.
  //
  // ⚠ UN SEUL NŒUD PORTE LES DEUX FIGURES : ses deux premières pistes sont le
  // cadre, ses deux dernières la barre. Voir `AdvanceFrameAndBarTracks`.
  kFrameAndBar,
  // UNE BOÎTE AU SOL, sous-type 64 : les quatre murs d'une boîte carrée posée
  // sous un personnage, et dont la hauteur RESPIRE.
  //
  // ⚠ UNE SEULE PISTE VIVE, qui ne tourne pas, et ce n'est pas un losange.
  // Voir `AdvanceGroundBoxTracks`.
  kGroundBox,
  // UNE ÉTINCELLE QUI CONVERGE, sous-type 159 : quatre étincelles naissent sur
  // une sphère autour du lanceur, foncent droit vers lui, et tournoient en
  // arrivant.
  //
  // ⚠ SA MARQUE EST UNE MACHINE À ÉTATS : trois phases y vivent, et la teinte
  // n'en est que la troisième case. Voir `AdvanceHomingSparkTracks`.
  kHomingSpark,
  // TROIS PLANS, sous-types 111 et 115 : trois carrés tournés vers la caméra,
  // chacun à son propre décalage et de son propre alpha.
  //
  // ⚠ DEUX FORMES POUR UNE SEULE FIGURE, et c'est voulu : leurs mouvements ne
  // se ressemblent pas — l'un fait dériver ses plans en poursuivant des
  // angles, l'autre les envoie en ligne droite —, et RIEN DANS L'ÉTAT DU NŒUD
  // ne les distingue sûrement. Deux énumérateurs le disent, et le compilateur
  // le vérifie. Voir `AdvanceDriftingTriPlaneTracks` et son jumeau.
  kDriftingTriPlane,
  kFlyingTriPlane,
  // UNE BOURRASQUE AU SOL, sous-type 119 : un carré plat qui file vers sa
  // cible, suivi de trois échos de plus en plus pâles.
  //
  // ⚠ UNE SEULE PISTE VIT ; LES TROIS AUTRES SONT SA TRAÎNÉE. Elles n'ont
  // ni compteur ni alpha propres et reçoivent, tous les trois tics, ce que
  // leur voisine portait. Voir `AdvanceGroundGustTracks`.
  kGroundGust,
  // LES BOURRASQUES QUI POURSUIVENT UN ACTEUR, sous-types 122 et 125 : la
  // figure du 119, et un cap qui se corrige à chaque tic vers la cible.
  //
  // ⚠ DEUX FORMES POUR UNE FIGURE, comme les trois plans : leurs mouvements ne
  // se ressemblent pas — l'un part de côté et RENTRE quand l'acteur disparaît,
  // l'autre part d'un écart tiré et s'éteint dès qu'il vise —, et rien dans
  // l'état du nœud ne les distingue. Voir `effect_homing_gust.h`.
  kHomingGustSteered,
  kHomingGustSpread,
  // LA ROUE DE VENT, sous-type 126 : le carré du 119 DRESSÉ dans le plan de son
  // cap, avec le mouvement du 119 à quatre nombres d'alpha près. Voir
  // `effect_rolling_gust.h`.
  kRollingGust,
  // UN TRAIT TENDU ENTRE DEUX ACTEURS, sous-type 38 : un tube à dix faces
  // qui pousse du porteur vers sa cible, peint deux fois.
  //
  // ⚠ C'EST LA PREMIÈRE FORME QUI VEUT UN SECOND POINT, celui de l'acteur
  // visé, et sans lui elle s'éteint. Voir `AdvanceTetheredBeamTracks`.
  kTetheredBeam,
  // UN RUBAN QUI TOURNE, sous-types 77 et 91 : un arc large d'une demi-largeur
  // fixe, que le mouvement fait pivoter sur lui-même.
  //
  // ⚠ DEUX FORMES POUR UNE FIGURE, comme les trois plans : le 77 ne fait
  // que tourner, le 91 naît invisible, en retard, et monte en enflant.
  // Voir `AdvanceSpinRibbonTracks` et son jumeau.
  kSpinRibbon,
  kRisingSpinRibbon,
  // UN ARC QUI SERPENTE, sous-type 70 : un tube à trois faces qui bombe
  // d'un côté puis de l'autre et se raccourcit de sept dixièmes par rang.
  //
  // ⚠ SA PISTE EST UN HISTORIQUE : les vingt et une valeurs et les vingt et
  // une marques reculent d'un cran à chaque tic. Voir
  // `AdvanceSnakingArcTracks`.
  kSnakingArc,
  // UNE RONDE, sous-type 27 : quatre carrés qui tournent autour du nœud, à
  // hauteur fixe, en tremblant chacun dans son quadrant.
  //
  // ⚠ SIXIÈME FIGURE DU LOT BÂTIE SUR LE CARRÉ À QUARANTE-CINQ DEGRÉS, et
  // quatrième à poursuivre des angles — avec d'autres pas que ses sœurs.
  // Voir `AdvanceSpinningRoundTracks`.
  kSpinningRound,
  // DES LUCIOLES, sous-type 130 : quatre carrés qui errent autour du nœud,
  // s'éloignent, se replacent au hasard, et SCINTILLENT.
  //
  // ⚠ MÊME DISPOSITION DE PARTICULES QUE LA RONDE, et rien d'autre en commun :
  // pas de garde de hauteur, un éclat par luciole, et une errance qui fuit.
  // Voir `AdvanceWanderingMoteTracks`.
  kWanderingMote,
  // UNE GERBE, sous-type 146 : une jupe d'anneau qui balaie sept huitièmes de
  // tour, DÉRIVE au fil des tics, et dont l'image DÉFILE.
  //
  // ⚠ ELLE RECOMMENCE PLUS BAS au lieu de mourir, trois fois de suite. Voir
  // `AdvanceAnchoredSkirtTracks`.
  kAnchoredSkirt,
  // UN ÉCLAIR, sous-type 101 : un tube à trois faces qui part du nœud, se
  // courbe, et TREMBLE.
  //
  // ⚠ SA FIGURE TIRE AU SORT, seule du lot : le client re-tire son
  // tremblement à chaque image, dans le dessinateur et non dans le
  // mouvement. Voir `ComposeLightningFanQuads`.
  kLightningFan,
  // L'AURA DU PERSONNAGE À TROIS COUCHES, sous-type 99 : quatre étincelles
  // qui tournent autour du porteur, montent, s'éteignent et RENAISSENT plus
  // haut, chacune peinte trois fois à trois échelles.
  //
  // ⚠ SON MOUVEMENT EST UN CYCLE DE VIE, le premier du lot : une particule
  // n'a pas d'âge commun avec ses voisines et se renouvelle seule. Voir
  // `AdvanceJobAuraTracks`.
  kLayeredAura,
  // LA MÊME AURA À UNE SEULE COUCHE, sous-type 35.
  //
  // ⚠ MÊME FIGURE ET MÊME MOUVEMENT QUE LA PRÉCÉDENTE, et c'est pourtant une
  // forme à part : celle-ci ne plafonne pas son alpha par les tics, et sa
  // teinte DÉBORDE d'une piste sur les suivantes. Les réunir ferait naître
  // l'une en fondu et l'autre d'un coup, sans que rien ne le signale.
  kPlainAura,
  // UN ÉVENTAIL DE SPRITES, sous-type 167 : quatre sprites qui s'écartent du
  // nœud, chacun dans sa direction.
  //
  // ⚠ C'EST LA SECONDE FORME DE LA FAMILLE SPRITE, et elle ne compose pas
  // comme la première : le sprite éclatant rend UNE pose par cran de
  // traînée, celui-ci CROISE ses crans avec ses quatre pistes. Voir
  // `ComposeSpriteFanPoses`.
  kSpriteFan,
  // UN ANNEAU DE NUAGE, sous-type 62 : la jupe d'anneau qui S'OUVRE, de
  // trente degrés à cent vingt ou cent quatre-vingts.
  //
  // ⚠ TROISIÈME VARIANTE DU MÊME BALAYAGE, et c'est la FORME qui la
  // distingue : sa clé est une hauteur, son pas se divise par vingt et un, et
  // son mouvement déroule le ruban au lieu de le faire onduler. Voir
  // `ComposeCloudRingQuads` et `AdvanceCloudRingTracks`.
  kCloudRing,
  // DEUX DISQUES, sous-type 28 : deux carrés qui respirent, posés à cent
  // quatre-vingts degrés d'inclinaison l'un de l'autre.
  //
  // ⚠ MÊME FIGURE QUE LE SOUS-TYPE 39 — le carré qui respire —, et c'est
  // pourtant une forme à part : son mouvement ne fait tourner QUE les deux
  // premières inclinaisons, là où celui du 39 fait tout autre chose. La forme
  // lie la figure ET le mouvement. Voir `AdvanceTwinDiscTracks`.
  kTwinDisc,
  // QUATRE ANNEAUX, sous-type 41 : quatre jupes qui s'ouvrent l'une après
  // l'autre, en sinus, et se referment huit cents tics plus tard.
  //
  // ⚠ MÊME FIGURE QUE L'ANNEAU À PISTES, et une forme à part pour la même
  // raison que toujours : son mouvement n'est pas l'ondulation de ses
  // sœurs mais une OUVERTURE, et ses quatre pistes partent décalées de
  // trente tics. Voir `AdvanceRingQuartetTracks`.
  kRingQuartet,
  // LA GERBE D'ANNEAUX, sous-type 107 : quatre jupes dont le cercle s'élargit
  // d'un dixième par tic pendant que le ruban s'ouvre puis se referme.
  //
  // ⚠ MÊME FIGURE QUE LE QUATUOR, et une forme à part pour la même raison :
  // son mouvement n'ouvre pas la jupe en sinus, il fait GRANDIR son cercle et
  // la couche à mesure. Voir `AdvanceRingStarburstTracks`.
  kRingStarburst,
  // UNE ONDE DE RUBAN, sous-type 36 : un anneau qui respire, parcouru d'une
  // vague.
  //
  // ⚠ MÊME FIGURE QUE LE RUBAN DE TRAÎNÉE, et une forme à part pour la raison
  // habituelle : son mouvement n'est celui d'aucune de ses sœurs. Il RÉÉCRIT
  // les vingt et une longueurs à chaque tic, en demi-sinusoïde le long du ruban
  // et en sinus dans le temps, et fait respirer l'alpha sur une horloge que
  // tient le rayon. Voir `AdvanceRibbonWaveTracks`.
  kRibbonWave,
  // UN ANNEAU QUI TOURNE, sous-type 50 : le frère du précédent.
  //
  // ⚠ MÊME FIGURE ET MÊME ONDE que l'onde de ruban — la boucle qui réécrit les
  // vingt et une longueurs est identique instruction pour instruction —, et
  // pourtant pas un autre champ en commun. Celui-ci fait PULSER son rayon en
  // sinus d'un compteur à part et TOURNER son azimut de trois degrés par tic,
  // là où son frère fait respirer l'alpha et descendre l'inclinaison. C'est la
  // preuve que la forme doit lier la figure ET le mouvement.
  //
  // ⚠ ET « QUI PULSE » ÉTAIT DÉJÀ PRIS par le sous-type 63 : ce qui sépare les
  // deux à l'œil est justement ce qui nomme celui-ci.
  kTurningRing,
  // LES ANNEAUX EN CŒUR, sous-type 110 : la figure du RUBAN de traînée, et le
  // mouvement des groupes radiaux.
  //
  // ⚠ LES DEUX ÉTAIENT PORTÉS CHACUN DE SON CÔTÉ — le ruban vient du sous-type
  // 36, le mouvement du soixante, que le cent dix partage avec lui — et rien ne
  // les réunissait. Donner à ces nœuds la forme des groupes radiaux les
  // dessinerait en JUPE d'anneau, ce qui est l'autre figure.
  kHeartRings,
  // LE SPRITE QUI S'ÉLÈVE, sous-type 85 : l'ange, ses ailes et ses plumes.
  //
  // ⚠ C'EST LA TROISIÈME FORME DE LA FAMILLE SPRITE, et son mouvement ne touche
  // à AUCUNE piste : il mène l'alpha du nœud, son index d'image et sa position.
  // Voir `AdvanceFadingSpriteNode`.
  kFadingSprite,
  // LA GRILLE DE VISÉE, sous-type 112 : sept cases sur sept peintes À MÊME LE
  // TERRAIN, dont l'image tourne et respire.
  //
  // ⚠ ELLE NE REND AUCUN QUAD, seule de toutes les formes : son dessinateur
  // appelle la peinture de CELLULE du client — celle du curseur au sol — et la
  // géométrie vient donc du fichier de carte, pas de la figure. C'est pourquoi
  // elle sort de la troupe par une voie à elle. Voir `effect_magic_target.h`.
  kMagicTargetGrid,
  // LE CARRÉ QUI FILE EN TOURNOYANT, sous-type 116 : la figure du carré teinté,
  // une meneuse qui file selon son cap, et trois échos qui la recopient tous
  // les trois tics. Voir `effect_swirl_trail.h`.
  kSwirlTrail,
  // LES ICÔNES QUI FLOTTENT, sous-type 123 : la figure du carré teinté, quatre
  // pistes qui tournent, dérivent, s'allument puis s'éteignent chacune pour soi.
  // Voir `effect_drifting_icon.h`.
  kDriftingIcons,
  // LA DISPERSION D'ÉCLAIRS, sous-type 135 : le joint et le tremblement de
  // l'éclair du 101, mais un seul tube rouge et trois NAPPES tendues vers lui.
  //
  // ⚠ SES QUATRE PISTES SE COMPOSENT ENSEMBLE : les nappes s'accrochent au
  // bord que la piste zéro vient de poser. Voir `ComposeElectricArcQuads`.
  kElectricArc,
  // DEUX DISQUES QUI S'ÉTEIGNENT, sous-type 161 : la figure du carré qui
  // respire et le semis du 28, à trois écritures près.
  //
  // ⚠ C'EST LE MOUVEMENT QUI EN FAIT UNE FORME À PART : il compte les tics de
  // chaque piste et en retire l'alpha pendant les vingt-cinq derniers. Voir
  // `AdvanceFadingTwinDiscTracks`.
  kFadingTwinDisc,
  // L'ANNEAU QUI S'ÉLARGIT ET RENAÎT, sous-type 169 : la figure du ruban de
  // traînée (`ComposeEffectTrackRibbon`), et un mouvement qui fait croître
  // chaque rayon d'une unité au-delà de sa base avant de l'y ramener. Voir
  // `effect_expanding_ring.h`.
  kExpandingRing,
  // LE CYLINDRE DE NOTES DE SEVERE RAINSTORM, sous-type 173 : la jupe qui
  // défile du 144, abaissée de cinq, et un mouvement qui tourne les quatre arcs
  // d'un degré par tic en les faisant onduler. Voir
  // `effect_rainstorm_cylinder.h`.
  kRainstormCylinder,
  // LES ÉTINCELLES QUI JAILLISSENT DE LA CIBLE, sous-type 45 : la figure du
  // carré teinté, et un mouvement qui donne à chaque piste UN cap — du nœud vers
  // le point visé, à trente degrés près — le long duquel elle file en
  // s'allumant puis en pâlissant. Voir `effect_thunder_scatter.h`.
  kThunderScatter,
  // LE CŒUR QUI ERRE ET SES ÉCLATS DE GEL, sous-type 145 : deux carrés tournés
  // vers la caméra qui font le tour du porteur en tremblant, et quatre éclats
  // debout autour d'eux qui grandissent, pâlissent et renaissent. DEUX IMAGES
  // par nœud. Voir `effect_thunder_freeze.h`.
  kThunderFreeze,
  // LE TOURBILLON DE TETRA VORTEX, sous-type 140 : la figure de l'arc de ruban,
  // posée au point visé, et un mouvement qui fait DÉFILER huit images puis
  // pâlit la dernière. HUIT IMAGES par nœud. Voir `effect_tetra_vortex.h`.
  kTetraVortex,
  // LES PÉTALES QUI S'ÉCARTENT, sous-type 94 : un carré tourné vers la caméra par
  // piste, qui file quatre-vingt-dix tics le long de son cap en faisant UN saut,
  // s'allume jusqu'à cinquante puis pâlit. Voir `effect_radial_petal.h`.
  kRadialPetal,
  // LA LUEUR BLANCHE QUI PASSE EN ARC, sous-type 109 : un carré tourné vers la
  // caméra qui s'allume, passe d'un côté à l'autre du porteur en ondulant,
  // s'arrête puis pâlit. Voir `effect_arcing_light.h`.
  kArcingLight,
  // LES DEUX RAYONS POINTÉS VERS LA CIBLE, sous-type 154 : deux triangles
  // couchés à vingt-cinq degrés du cap, peints deux fois chacun, qui s'allument
  // en quinze tics puis pâlissent. Voir `effect_aimed_spoke.h`.
  kAimedSpokes,
  // LES RAIS DE L'ONDE DE CHOC, sous-type 54 : deux triangles fins par piste,
  // du centre au rayon, redressés vers la caméra, qui s'allument en dix tics
  // puis pâlissent. Voir `effect_shockwave_ray.h`.
  kShockwaveRays,
  // LES FEUILLES QUI S'ÉTIRENT, sous-type 141 : quatre rectangles empilés
  // au-dessus du porteur, redressés vers la caméra, qui s'étirent l'un après
  // l'autre puis pâlissent. Voir `effect_tapered_sheet.h`.
  kTaperedSheets,
  // LES COURONNES DE MAGNUM BREAK, sous-type 73 : deux rubans circulaires par
  // nœud, relevés d'une clé inclinée, qui s'allument en huit tics puis meurent.
  // Voir `effect_break_ring.h`.
  kBreakRings,
  // L'ÉMETTEUR EN SPIRALE, sous-type 151 : il ne peint rien, et fait naître à
  // chaque peinture un effet sur l'acteur qu'il désigne. Voir
  // `effect_spiral_emitter.h`.
  kSpiralEmitter,
  // LE SEMEUR QUI MARCHE, sous-type 120 : un porteur invisible qui marche vers
  // la cible et fait naître un effet là où il passe. Voir
  // `effect_walking_sower.h`.
  kWalkingSower,
  // LE CARRÉ QUI TOURNE À PLAT, sous-type 143 : torches, poussière et eau.
  // Voir `effect_floor_spinner.h`.
  kFloorSpinner,
  // LA NOTE QUI FLOTTE, sous-type 156 : une note face à la caméra et son halo.
  // Voir `effect_bobbing_note.h`.
  kBobbingNote,
  // LE VOILE TEINTÉ, sous-type 172 : un grand carré coloré peint par-dessus
  // tout. Voir `effect_colored_veil.h`.
  kColoredVeil,
  // LA PETITE BOÎTE QUI TOURNE, sous-type 114 : six faces bleues au-dessus du
  // porteur. Voir `effect_spinning_box.h`.
  kSpinningBox,
};

// VRAI quand cette forme se dessine par un SPRITE et non par une texture.
//
// ⚠ LES DEUX CHEMINS DE COMPOSITION NE SE RATTRAPENT PAS L'UN L'AUTRE. La
// fenêtre cherche l'image d'un nœud dans son cache de TEXTURES, sauf pour les
// formes nommées ici, dont elle va chercher le couple `.spr`/`.act` dans son
// cache de SPRITES. Une forme de sprite absente de cette liste cherche donc son
// `.spr` parmi les `.tga`, ne l'y trouve pas, et se compte comme « sans
// texture » — elle ne se dessine jamais, et la mesure la croit portée.
//
// C'est arrivé à l'éventail de sprites, qui était dans ce cas depuis son
// portage : trois identifiants en table, aucun à l'écran.
[[nodiscard]] bool EffectNodeShapeDrawsASprite(PooledNodeShape shape);

// LE MOT QUE LA FILE DE RENDU REÇOIT pour un nœud de cette forme, quand le nœud
// porte `node_word`. Voir `ro/render/effect_render_word.h`.
//
// 🔴 CE N'EST PAS TOUJOURS CELUI DU NŒUD : le dessinateur choisit. La couronne
// plate et le quad orienté soumettent leurs triangles sous le mot du nœud ; le
// tronc de cône, les plans croisés, la croix de pointes et la sphère sous un
// mot EN DUR, le mot ordinaire — si bien que le 0x200 qu'un bâtisseur lève sur
// eux ne leur retire jamais l'épreuve de profondeur. Les trois pointes de
// Grimtooth naissent quarante unités sous le sol : sous le mot du nœud, elles
// se voyaient à travers lui.
//
// ⚠ LA CROIX DE POINTES À PART (`kCrossSpikeDrawnColor` sur `first_track_mark`)
// soumet sous le mot du mélange propre, translucide : elle s'ajoute dans le
// seau des fichiers `.str`, toujours éprouvée.
[[nodiscard]] std::uint32_t EffectNodeQueuedRenderWord(PooledNodeShape shape,
                                                       std::uint32_t node_word,
                                                       std::uint8_t first_track_mark);

}  // namespace ro::render
