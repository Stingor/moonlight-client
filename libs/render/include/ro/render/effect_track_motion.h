#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// CE QUI FAIT VIVRE UNE PISTE, un sous-type a la fois.
//
// Sorti de `effect_track.h` au plafond de huit cents lignes, et la coupure suit
// une frontiere que ce depot nomme depuis le lot 7 : le BATISSEUR pose une
// piste, le MOUVEMENT la fait vivre, le DESSINATEUR la lit. Le modele dit ce
// qu'une piste EST et se lit de bout en bout ; ce fichier-ci est un catalogue —
// une entree par sous-type, sans etat partage, ou l'on cherche celle qu'on
// porte sans jamais en lire deux.
//
// ⚠ CHAQUE ENTREE PORTE CE QUI LA DISTINGUE DE SA VOISINE. Deux
// mouvements se ressemblent presque toujours ; c'est leur ecart qui compte,
// et le taire ferait porter le rythme de l'un a l'autre.

// LE MOUVEMENT DU SOUS-TYPE 30 : un anneau dont la jupe ONDULE et retombe.
//
// Le cas 30 du répartiteur de mouvement ne délègue à aucune fonction et fait
// le travail sur place — c'est le cas de la moitié des mouvements, et ils ne
// sont pas immobiles pour autant.
//
// Ce qu'il fait, dans l'ordre :
//
//   - le rayon intérieur croît d'un pas fixe ;
//   - l'inclinaison descend d'un degré par tic, jusqu'à un plancher où l'alpha
//     s'annule : la jupe se couche et la figure s'éteint ;
//   - passé un seuil de rayon, l'alpha décroît ; une fois éteint, et tant qu'il
//     reste du temps à vivre, l'anneau REPART d'un rayon et d'une inclinaison
//     posés en dur. C'est ainsi qu'un portail pulse sans qu'aucune boucle ne
//     soit décrite nulle part ;
//   - enfin chaque segment prend la longueur du rayon, ondulée par le produit
//     de deux sinus : l'un le long du ruban, l'autre dans le temps.
//
// `rank` est le rang de la piste dans le nœud, et il entre dans la phase : les
// quatre pistes d'un même nœud n'ondulent pas ensemble.
//
// ⚠ LES DEUX PREMIÈRES PISTES N'ONT PAS LA MÊME PHASE QUE LES DEUX AUTRES, et
// ce n'est pas une élégance : le client compare le rang à un seuil et prend deux
// formules différentes, dont la seconde DOUBLE la vitesse de l'onde. La
// reproduire est ce qui garde la parité ; la lisser ferait onduler les quatre
// pistes ensemble, ce qui se voit.
void AdvanceUndulatingRingTrack(EffectTrack& track, std::size_t rank, std::int32_t life_ticks);

// LE MOUVEMENT DU SOUS-TYPE 139 : le MÊME anneau qui ondule, à deux différences.
//
// Le cas 139 du répartiteur l'appelle sans rien faire d'autre. Il reprend le cas 30
// constante pour constante — croissance de sept centièmes, seuil de quatre,
// rallumage à 3,37, chute d'alpha de trois, plancher d'inclinaison à dix, le
// style 22 qui monte moins vite, et jusqu'aux deux formules de phase que le rang
// choisit. Deux choses seulement l'en séparent, et les deux se voient :
//
//   - L'ONDE NE BAT PAS AUTOUR DU RAYON MAIS AUTOUR DE LA CLÉ. Le cas 30 lit
//     le rayon comme base de ses segments, celui-ci la clé. Un anneau qui
//     ondulerait autour de son rayon croissant s'élargirait sans cesse ; celui-ci
//     bat autour d'une hauteur fixe pendant que son rayon s'ouvre ;
//   - IL FAIT RESPIRER L'ÉCHELLE DU NŒUD, et c'est la seule chose qu'il écrive
//     hors de la piste : quatre, six, sept puis huit, à chaque quart de cent
//     tics. C'est pourquoi il reçoit l'échelle en référence.
//
// ⚠ IL PREND LES QUATRE PISTES D'UN COUP, là où le cas 30 en prend une : son
// rang entre dans la phase, et le lui passer piste par piste ferait écrire
// l'échelle du nœud quatre fois.
void AdvanceOscillatingRadiusParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                            std::int32_t life_ticks, std::uint8_t& node_scale);

// LE MOUVEMENT DU SOUS-TYPE 108 : un éventail qui s'ouvre UNE FOIS et se fige.
//
// Le cas 108 du répartiteur l'appelle sans rien faire d'autre.
//
// ⚠ CHAQUE SEGMENT SE VERROUILLE À SA PLEINE LONGUEUR, et c'est ce qui distingue
// cette figure d'une ondulation. Le segment croît comme le produit de deux
// sinus — l'un le long du ruban, l'autre dans le temps — jusqu'à DÉPASSER la
// longueur que le premier lui donne seul ; à cet instant le client l'y ramène et
// pose sa MARQUE à un. Le segment ne bouge plus jamais, quoi que fasse le temps.
// Sans ce verrou, l'éventail se refermerait au retour du sinus.
//
// ⚠ ET LE FACTEUR DE TEMPS CESSE AU QUATRE-VINGT-DIXIÈME TIC : passé là, le
// segment prend directement sa pleine longueur. Les deux règles se recouvrent,
// et ce n'est pas une redite — un segment que la marque n'a pas encore figé
// s'ouvre alors d'un coup.
//
// ⚠ SA CLÉ NE VIENT PAS DE SON BÂTISSEUR MAIS DE SA QUEUE. Le client la
// recalcule à chaque tic : un sinus de période quatre-vingt-dix tics, d'amplitude
// deux et demie, ajouté à la deuxième case de QUEUE. C'est l'ADR 0017 encore —
// la queue, que deux familles n'emploient pas, porte ici la base de la figure.
//
// La case de queue qui porte cette base.
inline constexpr std::size_t kRadialSpreadKeyBaseTailSlot = 1;
void AdvanceRadialSpreadParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE MOUVEMENT DU SOUS-TYPE 144 : le cent trente-neuf SANS son echelle.
//
// Le cas 144 l'appelle sans rien faire d'autre. Il reprend le corps commun des cas 30 et 139
// constante pour constante — croissance de sept centiemes, seuil de quatre,
// rallumage a 3,37 avec une inclinaison de soixante-quatorze, chute d'alpha de
// trois, plancher a dix, et les deux formules de phase que le rang choisit.
//
// ⚠ L'ONDE BAT AUTOUR DE LA CLE, comme celle du cent trente-neuf et non comme
// celle du trente. La seule chose qu'il ne fait PAS est de faire respirer
// l'echelle du noeud : la lui preter ferait enfler un anneau que le client laisse
// a sa taille.
void AdvanceGravityWaveTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int32_t life_ticks);

// LE MOUVEMENT DU SOUS-TYPE 118 : une SPIRALE qui s'ouvre et se fige.
//
// Le cas 118 du répartiteur l'appelle sans rien faire d'autre. Il tient de son voisin le 108 —
// le segment croît comme le produit de deux sinus, et se fige — et il en diffère
// par trois choses :
//
//   - SA CLÉ CROÎT GÉOMÉTRIQUEMENT, d'un demi-centième par tic. Une croissance
//     additive ouvrirait la spirale droit ; celle-ci s'accélère ;
//   - SON AZIMUT DÉRIVE, et d'un tirage : quatre degrés plus un tirage sur trois
//     à chaque tic. C'est ce qui en fait une spirale et non un éventail ;
//   - LA QUATRIÈME PISTE N'EST PAS COMME LES TROIS AUTRES. Elle dérive de deux
//     degrés plus un tirage sur DEUX, et sa clé croît cinq fois plus vite. Le
//     client la traite à part par une comparaison sur le rang ; l'aligner sur ses
//     sœurs ferait quatre spirales identiques.
//
// ⚠ LE FIGEAGE N'ARRÊTE PAS LE MOUVEMENT, il MARQUE. À la différence du 108, ce
// mouvement réécrit ses segments à chaque tic tant que le quatre-vingt-dixième
// n'est pas passé, marque ou non : la marque est une sortie que la figure lit,
// pas une mémoire dont le mouvement se sert.
void AdvanceSpiralParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::uint32_t& generator);

// LE MOUVEMENT DU SOUS-TYPE 24 : une ARCHE qui tourne et qui pulse.
//
// Le cas 24 du répartiteur fait le travail SUR PLACE.
//
// ⚠ SON ONDE NE S'AJOUTE PAS À SA BASE, ELLE LA MULTIPLIE, et c'est ce qui le
// sépare du cas 30. Le trente pose un rayon constant sur tous ses segments et y
// ajoute une onde qui court le long du ruban ; celui-ci donne à chaque segment
// une hauteur en demi-arche — la clé fois le sinus du segment — puis fait
// respirer l'arche entière de trois dixièmes. Additionner au lieu de multiplier
// donnerait un ruban plat qui ondule, au lieu d'une arche qui bat.
//
// ⚠ ET SES QUATRE PISTES NE TOURNENT PAS À LA MÊME VITESSE. Les rangs PAIRS
// avancent d'un degré par tic, les impairs de deux : le client les distingue par
// deux comparaisons. Les aligner ferait tourner les quatre arches ensemble.
//
// ⚠ SON ALPHA DÉPEND DE LA VIE DU NŒUD, non d'un tic fixe : il monte d'un par
// tic jusqu'au centième, et ne redescend qu'une fois passé « la vie moins vingt
// tics ». Un nœud court s'éteint donc sans jamais atteindre son plafond.
void AdvanceTurningArchTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int32_t life_ticks);

// LE MOUVEMENT DU SOUS-TYPE 43 : une particule qui MONTE, paraît, puis s'efface.
//
// Le cas 43 du répartiteur de mouvement fait lui aussi le travail sur place.
//
// Ce qu'il fait, dans l'ordre :
//
//   - le compteur avance, et tant qu'il est négatif la piste ne fait rien
//     d'autre : c'est le retard que le bâtisseur a tiré au sort ;
//   - le rayon croît d'un pas que la piste porte elle-même, en première case ;
//   - l'alpha MONTE pendant les dix premiers tics, puis reste, puis descend
//     passé un seuil. Les trois phases ne se recouvrent pas, et c'est ce qui
//     donne son apparition franche et sa disparition lente ;
//   - l'angle rangé en troisième case avance à chaque tic, et l'azimut n'avance
//     que pour une marque précise. Les deux se replient sur un tour.
//
// ⚠ CE MOUVEMENT LIT SES CASES AUTREMENT QUE L'ANNEAU. La première y est un pas
// de croissance et non une longueur, la troisième un angle, et la marque choisit
// à la fois la couleur et la vitesse de rotation. Voir l'avertissement en tête
// de ce fichier : le sens appartient au dessinateur.
//
// ⚠ IL NE BORNE PAS SON ALPHA PAR LE HAUT, et il n'en a pas besoin : la montée
// ne dure que dix tics, ce qui la laisse à deux cent cinquante au plus. Ajouter
// une borne serait inventer une règle ; retirer la durée en ferait une.
void AdvanceRisingParticleTrack(EffectTrack& track);

// LE MOUVEMENT DU SOUS-TYPE 36 : un anneau qui RESPIRE et que parcourt une
// ONDE.
//
// Le cas 36 du répartiteur est écrit EN LIGNE : il n'appelle aucune
// sous-fonction.
//
// Ce qu'il fait, dans l'ordre :
//
//   - le compteur avance, le rayon croît de quatre centièmes, et l'inclinaison
//     descend d'un degré ; sous dix degrés elle s'arrête là ET éteint la piste ;
//   - l'ALPHA EST UN CYCLE dont le RAYON tient l'horloge : il monte de vingt
//     par tic tant que le rayon est sous deux et demi, descend de deux ensuite,
//     et sa chute à zéro fait renaître le rayon à 2,14 et l'inclinaison à 74.
//     Une centaine de tics en tout — neuf de montée, quatre-vingt-dix de
//     descente ;
//   - les VINGT ET UNE longueurs se réécrivent entièrement, en demi-sinusoïde
//     le long du ruban et en sinus dans le temps.
//
// ⚠ IL NE BORNE PAS L'ALPHA PAR LE HAUT, et il n'en a pas besoin : la montée ne
// dure que neuf tics après une renaissance, ce qui la laisse à cent
// quatre-vingts au plus.
//
// ⚠ ET CE MOUVEMENT SUIT SON PORTEUR : le répartiteur recopie la position de
// l'acteur avant toute chose. C'est l'appelant qui la lui donne.
void AdvanceRibbonWaveTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// L'ONDE QUI COURT LE LONG D'UN RUBAN, partagée par les sous-types 36 et 50.
//
// ⚠ ELLE EST ÉCRITE UNE FOIS PARCE QU'ELLE EST IDENTIQUE. Les deux cas du
// répartiteur ont la même boucle, instruction pour instruction : même pas de neuf degrés,
// même amplitude de trois dixièmes, même règle de phase par rang de piste. La
// règle « un fichier par mouvement » vise deux mouvements qui donneraient des
// valeurs DIFFÉRENTES aux mêmes idées ; ici elles sont les mêmes, et la
// dédoubler les ferait diverger à la première correction.
//
//     longueur[i] = clé × (1 + sin(9·i°) × 0,3 × sin(phase°))
//
// Le profil est une DEMI-sinusoïde le long du ruban — nulle aux deux bouts,
// maximale au milieu — et la phase avance avec le compteur de tics.
void ApplyRibbonWaveLengths(EffectTrack& track, std::size_t rank);

// LE MOUVEMENT DU SOUS-TYPE 50 : un anneau qui PULSE et qui TOURNE.
//
// Le cas 50 du répartiteur est écrit EN LIGNE comme celui du
// 36, et qui partage son onde — voir ci-dessus.
//
// Ce qui l'en sépare, et c'est tout le reste :
//
//   - SON RAYON PULSE au lieu de croître. Un compteur à part — le champ de
//     STYLE de la piste — avance d'un degré par tic et reboucle ; le rayon en
//     est le sinus, décalé de trois. L'anneau va de DEUX à QUATRE et revient ;
//   - SON AZIMUT TOURNE de trois degrés par tic, un tour en cent vingt tics ;
//   - IL NE TOUCHE NI À L'ALPHA NI À L'INCLINAISON, là où son frère fait
//     respirer l'un et descendre l'autre.
//
// ⚠ ET CE MOUVEMENT SUIT SON PORTEUR, comme le 36 : c'est l'appelant qui donne
// la position de l'acteur.
void AdvanceTurningRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE MOUVEMENT DU SOUS-TYPE 82 : une figure qui paraît, tourne et s'efface,
// selon un STYLE.
//
// TOUT y dépend d'un seul octet : le style décide de la courbe
// d'alpha, du sens de rotation, et de la façon dont la figure finit.
//
// Ce qu'il fait, dans l'ordre :
//
//   - le compteur avance, et tant qu'il n'a pas passé son seuil, le style
//     choisit ce que devient l'alpha : une montée bornée, une montée puis une
//     descente, un pas d'une unité, ou une valeur POSÉE d'emblée ;
//   - un style à part fait pulser l'alpha par pas de huit et ouvre son rayon
//     tant qu'il se voit ;
//   - un tic sur deux, l'angle de la piste avance ou recule d'un degré selon le
//     style, et chaque sens ne replie que son côté ;
//   - passé le seuil, les styles bas déclinent d'une unité par tic ;
//   - et la figure tourne sur elle-même, d'un autre angle, à chaque tic.
//
// ⚠ LES DEUX ANGLES DE CAMÉRA QUE LE CLIENT ÉCRIT ICI N'Y SONT PAS. Il les
// range dans les champs de style des deux premières pistes du nœud ; nous les
// calculons au moment de composer, comme pour la particule montante — voir
// `effect_particle_figures.h`. Un mouvement qui connaîtrait la caméra ferait
// remonter le rendu dans l'horloge.
//
// ⚠ ET LE RÉ-ANCRAGE NON PLUS. Le client recale la piste sur la position de son
// propriétaire à chaque tic, ce qui fait suivre l'effet quand l'acteur marche.
// Nos nœuds gardent le point où ils ont été semés, pour toute la famille, et
// c'est un manque commun plutôt que celui de ce mouvement.
void AdvanceStyledBillboardTrack(EffectTrack& track);

// LA CASCADE : les quatre pistes se passent leur état, de proche en proche.
//
// C'est le mouvement du sous-type 47 et de ses voisins. C'est le premier de la famille qui ne fait
// pas vivre CHAQUE piste séparément : il travaille sur les QUATRE, et c'est pourquoi il les reçoit
// toutes.
//
// ⚠ LA CASCADE VA DE LA DERNIÈRE À LA PREMIÈRE, et l'ordre décide de tout. La
// quatrième prend la troisième, puis la troisième la deuxième, puis la deuxième
// la première. L'inverser copierait la tête dans les quatre d'un coup, et la
// traînée deviendrait quatre copies superposées.
//
// À chaque saut, le rayon GROSSIT d'un demi et l'alpha PERD cinquante : c'est ce
// qui donne à la traînée sa forme de sillage, une figure qui s'ouvre et s'efface
// en s'éloignant de la tête.
//
// ⚠ UNE PISTE ÉTEINTE N'EST PAS REMPLIE. Le client ne copie que dans une piste
// DÉJÀ allumée, si bien qu'un nœud qui n'en allume qu'une — le sous-type 47 en
// est — n'a aucune cascade et se réduit à sa tête. Copier quand même donnerait
// une traînée là où le client n'en montre pas.
//
// ⚠ ET CE MOUVEMENT PORTE LE RÉ-ANCRAGE, que toute la famille attendait. Il
// recale la première piste sur la position de son propriétaire à chaque tic,
// ce qui fait suivre l'effet quand l'acteur marche. `owner_position` est ce
// point ; passer celui du semis rend l'ancien comportement, immobile.
//
// Les deux angles de billboard se RANGENT dans les pistes, ils ne se
// recalculent pas au dessin : le client les pose en tête de la première et de la
// deuxième, et son dessinateur les y relit. Voir `effect_tinted_quad.h`.
void AdvanceCascadingTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 glm::vec3 owner_position, std::int16_t billboard_turn_degrees,
                                 std::int16_t billboard_tilt_degrees);

// LA TRAÎNÉE GUIDÉE : un projectile qui part droit, VIRE vers celui qui le
// lance, puis retombe en s'effaçant — suivi de trois crans de plus en plus
// pâles.
//
// C'est le mouvement du sous-type 42. Même
// figure que le sous-type 47 — le carré teinté —, et une cascade de la même
// famille, mais son vol est à lui.
//
// ⚠ TROIS PHASES, ET DEUX CHEMINS POUR LES PARCOURIR. Le projectile part en
// ligne droite ; passé une distance, il se met à VIRER vers son porteur d'un
// cinquième de tour par tic, et quand son cap est à moins de quatre degrés du
// but il RETOMBE en perdant douze d'alpha par tic. Mais celui dont la marque dit
// qu'il ÉCLATE saute le virage : il passe tout droit à la chute, sa vitesse
// TRIPLE, et son cap reçoit un écart tiré de part et d'autre — c'est une volée
// qui se disperse au lieu de converger.
//
// ⚠ SA CASCADE N'EST PAS CELLE DU 47, À TROIS ÉGARDS. Elle avance tous les trois
// tics au lieu de six — à CHAQUE tic pour celui qui éclate ; le premier saut
// coûte cent cinquante d'alpha et les suivants vingt-cinq, là où le 47 en prend
// cinquante partout ; et elle ne recopie ni l'azimut ni le rang, si bien que
// chaque cran garde le cap qu'il avait en recevant sa place. C'est cela qui
// courbe la traînée derrière un projectile qui vire.
//
// `aim_point` est ce que le nœud a accumulé de translation : le client y prend
// le cap du PREMIER tic, et le but du virage est le porteur.
void AdvanceHomingTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 owner_position, glm::vec3 aim_point,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees, std::uint32_t& generator);

// L'OBJET LANCÉ : le sous-type 53, et les onze EF_THROWITEM.
//
// C'est le FRÈRE du
// sous-type 42 : même cascade, au champ près et à la perte d'alpha près — cent
// cinquante au premier saut, vingt-cinq ensuite —, mêmes angles de caméra dans
// les deux premières pistes. Les deux partagent donc leur `Shift`.
//
// ⚠ ET TROIS CHOSES L'EN SÉPARENT.
//
// La cascade avance à chaque tic IMPAIR, et non tous les trois : la traînée se
// déplie deux fois plus lentement qu'elle n'avance.
//
// Le vol n'a PAS DE PHASES. Il n'y a ni guidage ni rupture : le cap se prend une
// fois, au premier tic, vers le point que le bâtisseur a rangé dans le nœud, et
// ne bouge plus. C'est un jet, pas une poursuite.
//
// ⚠ ET SA HAUTEUR EST UN DEMI-SINUS, non une pente. Le client indexe sa table
// de sinus par la FRACTION DU VOL ACCOMPLI, portée sur un demi-tour : l'objet
// monte, culmine à mi-chemin et redescend. Une pente linéaire — ce que fait le
// 42 — lui donnerait une trajectoire en toit d'usine. L'amplitude vient de la
// MARQUE DE GENRE : dix pour un genre, trente pour la plupart, et un seul pour
// celui qui tourne vite.
//
// ⚠ LE VOL SE JUGE FINI À 1,05 ET NON À 1, et ce cinquième de tic en plus n'est
// pas un arrondi : il laisse l'objet DÉPASSER son but avant de s'éteindre. Passé
// là, la tête perd vingt d'alpha par tic et rien d'autre ne bouge.
//
// `aim_point` est le point que le bâtisseur a rangé dans le nœud — là où l'objet
// tombe. Le nombre de pas du vol, lui, vit dans la piste : le bâtisseur l'a
// compté et rangé dans sa deuxième marque.
// LE COMPTEUR QUI AVANCE, et rien d'autre : le mouvement du sous-type 162.
//
// Il recopie la position du porteur — ce que la troupe fait
// a cote — puis avance le compteur de tics de CHAQUE piste vive.
//
// ⚠ C'EST LE PLUS COURT DE LA FAMILLE, ET CE N'EST PAS UN MANQUE. Le quartier de
// tarte ne bouge pas : ce qui le disperse est son SEMIS, et le compteur ne sert
// qu'a le faire vieillir. Lui preter un mouvement qu'il n'a pas ferait tourner
// un eclat que le client laisse immobile.
void AdvanceEmitterFrameTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LA COLONNE QUI S OUVRE COUCHE PAR COUCHE : le mouvement du sous-type 131.
//
// Il range les
// deux angles de camera dans les styles des deux premieres pistes, puis pour
// chaque piste vive :
//
//   - AVANT LE VINGTIEME TIC, les DIX alphas de couche montent du meme pas —
//     treize — et s arretent a des plafonds DIFFERENTS, de soixante-dix a deux
//     cent cinquante par pas de vingt. C est ce decalage qui ouvre la colonne en
//     degrade au lieu de l allumer d un bloc ;
//   - APRES, NEUF d entre eux retombent de cinq par tic. La dixieme — la plus
//     large, la plus claire — attend le CENTIEME tic, et c est ce qui laisse un
//     bord a la colonne quand son coeur s est eteint ;
//   - et le rayon enfle d un centieme par tic, une seconde fois tant que ce
//     dernier bord vit : la colonne s ouvre plus vite en se retirant qu en s
//     ouvrant.
void AdvanceRingExpansionTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees);

// LE FAISCEAU QUI GRANDIT EN RECULANT : le mouvement du sous-type 164.
//
// Il suit
// son porteur, puis pour chaque piste vive dont le compteur est remonte :
//
//   - l INDICE D IMAGE avance d une unite et revient a zero des qu il atteint le
//     nombre d images du noeud. C est LUI qui donne son sens a la seconde case de
//     la piste : vue du dessinateur seul, ce n est qu un entier ;
//   - LE RAYON DE DEPART RECULE d une unite, et remonte d UN PAS — la cle de la
//     piste — des qu il passe sous zero. C est ce recul qui fait grandir le
//     faisceau : il part a cent, ou un seul segment tient, et en gagne un chaque
//     fois qu il descend de quinze.
//
// ⚠ IL ECRIT LES QUATRE PISTES A LA SUITE, sans boucle, et le client les traite
// donc toutes les quatre meme si la premiere dort. Chacune garde son propre
// compteur et son propre indice d image.
void AdvanceFourLayerTimerTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::size_t texture_count);

// LE SPRITE QUI ENFLE : le mouvement du sous-type 88.
//
// Le cas 88 du repartiteur fait le travail SUR PLACE. Pour chaque piste vive : le compteur avance,
// le rayon est MULTIPLIE par 1,07 — non augmente —, l'alpha monte pendant dix tics puis redescend
// d'un par tic.
//
// ⚠ UNE PISTE RETARDEE NE FAIT RIEN, pas meme enfler. Son compteur remonte vers
// zero et c'est tout : c'est ce qui fait paraitre les six boules de foudre
// d'EF_STASIS l'une apres l'autre, tous les quatre tics.
//
// ⚠ ET LE PAS DE MONTEE SE LIT DANS LA VARIANTE DU NOEUD, non dans la piste. La
// variante cent treize monte de SIX, toutes les autres de deux : le sprite
// d'EF_GROUNDSHAKE parait trois fois plus vite que celui de son voisin. C'est le
// seul endroit ou ce mouvement regarde hors de sa piste.
void AdvanceGrowingSpriteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::uint8_t node_variant);

// LA NAPPE AU SOL : le mouvement des sous-types 72 ET 172.
//
// Les deux cas l'appellent sans rien faire d'autre — ce n'est pas un
// mouvement qui ressemble a l'autre, c'est le meme code.
//
// Il range les deux angles de camera dans les styles des deux premieres pistes,
// suit son porteur, puis pour chaque piste vive : le compteur avance, l'origine
// de la nappe se reboucle a un carreau, et l'alpha suit l'un de DEUX rythmes.
//
// ⚠ ET C'EST LA MARQUE DE TEINTE QUI CHOISIT LE RYTHME. Un seul octet, deux
// lecteurs, deux sens (ADR 0017) : la figure y lit une couleur, le mouvement y
// lit une cadence. La nappe rouge monte de dix pendant dix tics et RETOMBE passe
// soixante-cinq ; toutes les autres montent de cinq et RESTENT.
//
// ⚠ SON PLAFOND EST UNE BORNE DU TEST, NON DE LA VALEUR : le client monte tant
// que l'alpha ne DEPASSE pas quarante-cinq, si bien qu'il finit a cinquante ou
// cinquante-cinq selon le pas. L'y ramener eteindrait la nappe d'un dixieme.
void AdvanceParticleFrameTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees);

void AdvanceThrownItemTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             glm::vec3 aim_point, std::int16_t billboard_turn_degrees,
                             std::int16_t billboard_tilt_degrees);

// LA TRAÎNÉE QUI RETOMBE : une tête qui tourne, descend et s'efface, suivie de
// trois crans de plus en plus pâles.
//
// C'est le mouvement du sous-type 87 — les potions et les lances qu'un joueur lance.
//
// ⚠ SON SPRITE DESCEND. Il part quatre-vingts unités au-dessus de son porteur
// et s'en rapproche de trois par tic (ADR 0017).
//
// ⚠ ET SA TRAÎNÉE AVANCE D'UN CRAN PAR TIC, non tous les six comme celle du
// sous-type 47. Chaque piste attend son rang, si bien que la traînée se DÉPLIE
// au lieu de paraître entière. Les faire toutes sauter au même tic donnerait
// quatre copies superposées au premier tic, puis plus rien de neuf.
//
// Rend VRAI au tic où le client déclenche son SON. Nous ne le jouons pas ici —
// `render` ne fait pas de bruit —, mais le tic est celui-là et non un autre :
// l'appelant qui voudra le jouer n'aura pas à le deviner.
[[nodiscard]] bool AdvanceFallingTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                             glm::vec3 owner_position,
                                             std::int16_t billboard_turn_degrees,
                                             std::int16_t billboard_tilt_degrees);

// L'ÉVENTAIL : dix lames dont chacune s'allume à SON éclat, puis retombent.
//
// C'est le mouvement du sous-type 147.
//
// ⚠ DIX CASES D'AFFILÉE PORTENT DIX ALPHAS, un par lame, et ce n'est pas une
// valeur répétée : chacune a son propre plafond, de soixante-dix à deux cent
// cinquante par pas de vingt. C'est l'écart entre ces plafonds qui donne à
// l'éventail son dégradé ; les lire comme une seule valeur donnerait dix lames
// de même éclat, c'est-à-dire une tache.
//
// ⚠ ET LE RAYON SE RESSERRE AVANT DE S'OUVRIR. Il perd deux dixièmes par tic
// pendant cinquante tics ; puis, pour deux styles sur trois, il enfle d'un
// centième par tic passé le quatre-vingt-dixième, tant que la dernière lame
// vit. Le bouquet se referme, puis s'ouvre en s'effaçant.
void AdvanceFanBladeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                           std::int16_t billboard_turn_degrees,
                           std::int16_t billboard_tilt_degrees);

// VRAI quand l'éventail doit SUIVRE SON PORTEUR au lieu de rester où il est né.
//
// ⚠ C'EST LA MÊME CASE QUI PORTE LE STYLE, et c'est l'ADR 0017 à la lettre : la
// marque cinq dit « bouquet serré », la marque un dit « suis le personnage ».
// Le sens appartient au lecteur, pas au champ. Y voir un numéro de style
// laisserait l'éventail des sept lunes planté là où le lanceur se tenait au
// premier tic.
// LES GROUPES RADIAUX : quatre couronnes dont CHAQUE CASE suit sa propre règle.
//
// Les cas 60 ET 110 du répartiteur de mouvement y délèguent tous deux : deux
// sous-types pour un mouvement.
//
// Ce qu'il fait, dans l'ordre :
//
//   - une marque fait regarder la caméra : elle range le tour de billboard dans
//     le champ que la TROISIÈME piste emploie comme durée ;
//   - l'azimut avance du RANG de la piste plus un pas que la marque choisit —
//     huit, six, ou quatre puis deux selon que la piste est proche ou loin ; une
//     marque inconnue ne tourne pas du tout ;
//   - l'alpha monte pendant seize tics, de cinq par tic et de trois ou deux
//     selon la marque, jusqu'à cent quatre-vingts ; passé la DURÉE que porte le
//     style, il redescend de deux par tic ;
//   - chaque case prend sa longueur d'une règle À ELLE, que sa propre marque
//     choisit : tenir, clignoter, ou onduler. C'est cela, les GROUPES du nom.
//
// ⚠ LA PREMIÈRE MARQUE FAIT DEUX MÉTIERS — elle choisit la vitesse de rotation
// de la piste ET la règle de sa première case. Le client la lit deux fois ; lui
// donner deux champs séparerait ce qu'il a joint (ADR 0017).
//
// ⚠ ET LE STYLE EST ICI UNE DURÉE, pas un numéro : le tic où la piste commence à
// s'effacer, et NÉGATIF il veut dire « jamais ». Le prendre pour un zéro ferait
// s'effacer d'emblée une piste qui doit tenir.
void AdvanceRadialParticleGroupTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                      std::int16_t billboard_turn_degrees);

// L'ÉTINCELLE QUI MONTE : quatre carrés teintés qui s'élèvent, tournent sur
// eux-mêmes, s'allument puis s'éteignent, et ÉCLATENT chacun à son tour.
//
// Transcrit le cas 59 du répartiteur de mouvement, qui ne délègue à
// aucune fonction : son code est écrit SUR PLACE, et sa cible n'est partagée avec
// aucun autre sous-type.
//
// Ce qu'il fait, dans l'ordre :
//
//   - il range les DEUX angles de caméra dans le style des deux premières
//     pistes, où le carré teinté les relit ;
//   - chaque piste tourne de cinq degrés par tic, dans le sens qui décroît ;
//   - au PREMIER tic elle tire son cap ; du deuxième au dixième son alpha monte
//     de quinze ; au-delà il descend de trois, et la piste s'éteint quand il
//     passe sous zéro ;
//   - passé le dixième tic, la piste ÉCLATE à deux tics qui lui sont propres —
//     trente-cinq et trente-six pour la première, sept de moins par piste
//     ensuite — en posant sa première marque, ce qui fait changer de texture ;
//   - et elle MONTE d'une case de valeur par tic, en dérivant d'une autre selon
//     son cap.
//
// ⚠ LE SEUIL DE L'ÉCLAT DESCEND MÊME POUR UNE PISTE ÉTEINTE. Le client le
// décrémente en tête de boucle, hors de toute garde ; ne le faire avancer que
// pour les pistes vives décalerait l'éclat de toutes les suivantes.
//
// ⚠ LE GÉNÉRATEUR N'EST PAS UN DÉTAIL : le cap se tire AU PREMIER TIC et non au
// semis. Le bâtisseur laisse l'azimut à zéro, et c'est ce mouvement qui le
// choisit.
void AdvanceRisingSparkTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees, std::uint32_t& generator);

// LA GERBE À DEUX MODES : quatre couronnes qui tournent, s'élargissent, se
// couchent et RESPIRENT, chacune un quart de tour après la précédente.
//
// C'est le mouvement du sous-type 31 — le cas 31 du répartiteur y délègue.
//
// Ce qu'il fait, dans l'ordre :
//
//   - l'azimut avance de cinq degrés par tic, de trois dans le mode lent ;
//   - le rayon s'élargit de trois dixièmes par tic, d'un dixième dans le mode
//     lent ;
//   - l'inclinaison descend d'un degré par tic et S'ARRÊTE à un plancher —
//     quarante, ou trente dans le mode lent. Sans lui la gerbe finirait couchée
//     et disparaîtrait dans le sol ;
//   - l'alpha monte pendant dix tics, de vingt par tic et de dix pour un style ;
//     passé dix tics il descend, et sa vitesse DÉPEND DU STYLE — cinq vitesses
//     pour cinq styles ;
//   - chaque segment prend l'arche du cône — neuf degrés par segment — mais
//     RESPIRE de quinze centièmes au lieu de se figer, et chaque piste respire
//     un quart de tour après la précédente.
//
// ⚠ LE MODE SE LIT DANS UN CHAMP DU NŒUD, comparé à quatre-vingt-dix-neuf :
// deux bâtisseurs sur trois l'y posent et prennent le mode LENT ; le troisième
// garde le zéro du constructeur, et le mode RAPIDE. Voir
// `EffectParticleRingStyleSeed::spray_mode`.
void AdvanceDualModeSprayTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                float spray_mode);

// LE CÔNE DE PARTICULES : quatre couronnes qui s'ouvrent en tournant, et un halo
// qui ne tourne pas.
//
// C'est le mouvement du sous-type 23 —
// le cas 23 du répartiteur de mouvement y délègue.
//
// ⚠ CE N'EST PAS LE MOUVEMENT DE L'ANNEAU DU SOUS-TYPE 30, et c'est tout
// l'objet de ce fichier-ci. La table de sauts du répartiteur donne
// à CHAQUE sous-type sa propre cible ; partager le dessinateur ne dit rien du
// mouvement. Les confondre ferait bouger faux trente-sept identifiants.
//
// Ce qu'il fait, dans l'ordre :
//
//   - l'azimut des TROIS PREMIÈRES pistes avance de trois, quatre et cinq degrés
//     par tic ; la quatrième ne tourne pas ;
//   - l'alpha monte pendant vingt tics — de dix jusqu'à cent quatre-vingts, sauf
//     la dernière piste qui monte de huit jusqu'à soixante —, sauf pour un style
//     qui l'en empêche ;
//   - quarante tics avant la fin de la vie du nœud, il redescend : de cinq par
//     tic, de deux pour la dernière piste ;
//   - chaque segment prend pour longueur le produit de DEUX SINUS — une arche de
//     neuf degrés par segment le long du ruban, un quart de tour dans le temps —
//     et SE FIGE une fois son plafond atteint. C'est ce gel qui fait que le cône
//     s'ouvre une fois au lieu de respirer.
//
// ⚠ LA CLÉ DE DÉCALAGE Y EST UNE AMPLITUDE. Le même champ que l'anneau du
// sous-type 30 compare à des flottants exacts sert ici de hauteur d'arche :
// ADR 0017 à la lettre, et une raison de plus de ne pas prêter un mouvement.
//
// `trail_variant` est le numéro de teinte du nœud : la valeur trois n'anime que
// la DERNIÈRE piste, le client sautant les trois autres dès l'entrée de sa
// boucle.
//
// `waves_last_track` est le mouvement jumeau du sous-type 25 : aucun style
// n'empêche la montée, la dernière piste monte jusqu'à cent et s'efface de
// trois, et ses segments figés ONDULENT au lieu de se tenir.
void AdvanceParticleConeSpawnTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                    std::int32_t life_ticks, float trail_variant,
                                    bool waves_last_track = false);

[[nodiscard]] bool FanTracksFollowTheirOwner(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// CE QUE LA DISPERSION A BESOIN DE SAVOIR DU MONDE, et rien de plus.
//
// ⚠ `render` NE CONNAÎT PAS LA CARTE, et ces deux valeurs sont ce qui lui permet
// de s'en passer. Le client, lui, va la chercher par une variable globale ; nous
// la recevons, et c'est ce qui garde le mouvement éprouvable sans carte chargée.
struct EffectScatterWorld {
  // L'altitude du sol PRATICABLE en un point du monde. VIDE quand personne ne
  // sait répondre : la particule garde alors la hauteur de son nœud, ce qui se
  // voit sur un relief — et vaut mieux qu'une altitude inventée.
  //
  // Le client lit ici la hauteur du terrain, que nous portons
  // déjà sous le nom `WalkableHeightAt` — voir `terrain_mesh.h`.
  std::function<float(float world_x, float world_z)> ground_height;
  // ⚠ UNE SEULE CARTE ÉLARGIT LA DISPERSION, et le client la nomme en clair : il
  // compare le nom de la carte courante à « ecl_fild01.rsw » et double alors la
  // boîte où ses particules tombent. Le taire donnerait une nuée deux fois trop
  // serrée sur cette carte-là, et seulement sur elle.
  bool wide_scatter = false;
  // ⚠ ET SUR CETTE MÊME CARTE, LE CARRÉ TEINTÉ PLAFONNE SON ALPHA : son dessin
  // ramène à cent toute piste qui le dépasse, et l'écrit dans la piste — le
  // mouvement suivant repart donc de cent. Voir `CapTintedQuadAlpha`.
  bool caps_tinted_quad_alpha = false;
  // L'ORDONNÉE DU PREMIER CALQUE DU CORPS D'UN ACTEUR, par son identifiant —
  // voir `ActorOutfit::BodyFirstLayerY`. Vide quand personne ne sait répondre :
  // la marque de la brûlure se pose alors à trente unités, comme chez le
  // client sans corps lisible.
  std::function<std::optional<float>(std::uint32_t actor_id)> body_first_layer_y;
};

// LA DISPERSION : quatre particules qui vivent, meurent et RENAISSENT ailleurs.
//
// C'est le mouvement du sous-type 33, dont le bâtisseur ne sème AUCUNE piste :
// tout ce que ces nœuds montrent vient d'ici ; sans lui, ils sont invisibles.
//
// Chaque piste monte son alpha, vit le temps que sa case de vie lui donne, puis
// le redescend ; à zéro elle RENAÎT, à un point tiré au sort autour du nœud.
// C'est cette renaissance qui fait durer une nuée sans que rien ne la ressème.
//
// ⚠ L'AZIMUT N'EST PAS UN ANGLE ICI, C'EST UNE DURÉE : ce mouvement y lit le
// nombre de tics au bout duquel la piste commence à s'éteindre. C'est l'ADR 0017
// une fois de plus, et sur un champ qui porte pourtant un nom — le sens
// appartient au lecteur, y compris quand le nom dit autre chose.
//
// ⚠ ET LA CASE DE STYLE DÉCIDE DE TOUT : la vitesse de montée de l'alpha, la
// forme de la boîte où la particule renaît, sa dérive, et jusqu'au fait de se
// poser sur le relief. Passé quatre-vingt-dix-neuf, elle GÈLE la piste — seule
// la respiration continue.
void AdvanceScatteringParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                     glm::vec3 node_position, std::int16_t billboard_turn_degrees,
                                     std::int16_t billboard_tilt_degrees,
                                     const EffectScatterWorld& world, std::uint32_t& generator);

// LE DÉFILEMENT : un compteur par piste, et rien d'autre.
//
// Transcrit le cas 79 du répartiteur de mouvement, qui ne délègue à
// aucune fonction nommée et tient en six instructions : pour chacune des quatre
// pistes, si elle est vive, son compteur avance d'un tic. Puis il rejoint la
// queue commune, qui vieillit le nœud — ce que la troupe fait déjà.
//
// ⚠ AUCUNE PISTE NE BOUGE ICI, et ce n'est pas un oubli : la bande qui défile
// tire TOUTE sa géométrie de ce compteur, ramené dans le cycle que son semis lui
// a donné. Déplacer les jointures en plus les ferait glisser deux fois.
void AdvanceScrollingBandTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LA NUIT QUI VIENT : un compteur et un alpha, par piste vive.
//
// Transcrit le cas 71 du répartiteur de mouvement. Pour chacune
// des quatre pistes, si elle est vive, son compteur avance d'un tic et son
// alpha d'une unité — jusqu'au plafond, où il s'arrête au lieu de reboucler.
//
// ⚠ ET IL FAIT DEUX CHOSES DE PLUS QUE NOUS NE PORTONS PAS ICI. Le jeu y
// range les deux angles de la caméra dans les champs de style des pistes zéro
// et un, et recale le nœud sur son porteur. Les angles n'ont pas d'emploi chez
// nous — ce client peint le voile à l'écran, déjà face à l'œil, voir
// `ComposeScreenDarkVeilQuads` —, et le ré-ancrage se fait dans la troupe, qui
// sait où est le porteur.
void AdvanceDarkVeilTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// L'ONDE QUI COURT : ce que le mouvement du sous-type 149 met dans ses pistes.
//
// Pour chaque piste vive, et
// seulement une fois son compteur devenu positif :
//
//   - l'azimut avance de TROIS degrés par tic, et se referme sur le tour ;
//   - l'alpha monte de trois par tic tant que le compteur est sous trente, et
//     s'écrête à CENT — pas à deux cent cinquante-cinq, à la différence du
//     voile : ce ruban ne devient jamais opaque. ⚠ Et cet écrêtage NE SERT
//     JAMAIS aux dix rubans connus : semés à zéro, ils culminent à
//     quatre-vingt-sept, qui est vingt-neuf montées de trois ;
//   - quand il reste moins de quarante-six tics à vivre, l'alpha redescend de
//     deux par tic jusqu'à zéro ;
//   - et les VINGT ET UNE cases reçoivent une onde : la case i vaut la clé,
//     plus trois dixièmes de cette clé modulés par le sinus de neuf fois i
//     degrés et par une phase qui court avec le compteur.
//
// ⚠ LA PHASE N'EST PAS LA MÊME POUR LES QUATRE PISTES, et elle ne suit pas une
// règle unique : les deux premières l'avancent de quatre-vingt-dix degrés par
// rang, les deux dernières DOUBLENT leur somme. Les traiter pareil ferait onduler
// les quatre en phase, ce qui rendrait un seul ruban épais au lieu de quatre.
void AdvanceSpinningRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::int32_t node_life_ticks);

// LA TRAÎNÉE QUI RESPIRE : ce que le mouvement du sous-type 66 fait des siennes.
//
// Transcrit le cas 66 du répartiteur, qui fait tout sur place.
// Pour chaque piste vive :
//
//   - son compteur avance, et son azimut est RAMENÉ dans le tour sans jamais
//     être incrémenté — cette figure ne tourne pas, c'est son semis qui
//     échelonne les quatre couches ;
//   - l'alpha monte d'une unité tant que le compteur est sous quarante, et
//     redescend passé cent quarante — mais SEULEMENT pour deux familles de
//     marques. Les autres gardent l'alpha que le semis leur a donné ;
//   - et une piste sur deux — la deuxième et la quatrième — voit son RAYON
//     respirer autour de sa valeur de repos, d'un demi, sur sept cent vingt
//     tics. Les faire respirer toutes les quatre les ferait gonfler ensemble,
//     là où le client en garde deux fixes pour donner l'échelle.
void AdvancePentagonTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LA SPIRALE QUI RENAÎT : le mouvement du sous-type 132.
//
// Transcrit le cas 132 du répartiteur, qui fait tout sur place.
// Pour chaque piste vive dont le compteur est devenu positif :
//
//   - le balayage gagne UN degré par tic et se referme sur le tour ;
//   - la clé croît de deux pour cent par tic — une croissance GÉOMÉTRIQUE, et
//     c'est elle qui fait monter l'éclat en s'élargissant ;
//   - passé vingt, l'alpha perd trois par tic ; à zéro, la piste RENAÎT —
//     clé remise à dix, azimut retiré au sort, alpha rendu — au lieu de
//     mourir. Seules les pistes dont la palette atteint deux s'éteignent pour
//     de bon, et seulement passé trois cents tics ;
//   - et LA TAILLE SE CALCULE : neuf dixièmes de la clé, plus un dixième de
//     la clé modulé par le sinus du balayage. Jamais négative.
//
// ⚠ SANS CE DERNIER CALCUL LA FIGURE NE REND RIEN : son bâtisseur laisse la
// case de taille à zéro. C'est le seul mouvement de la famille dont la figure
// dépende à ce point.
void AdvancePaletteBillboardTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   std::uint32_t& generator);

// LE TOURBILLON : le mouvement du sous-type 103.
//
// Transcrit le cas 103 du répartiteur. Pour chaque piste vive dont
// le compteur est devenu positif :
//
//   - l'azimut avance de TROIS À CINQ degrés, TIRÉS AU SORT à chaque tic. Un
//     pas fixe donnerait une rotation mécanique ; c'est ce tremblement qui
//     fait qu'un tourbillon n'a pas l'air d'un carrousel ;
//   - l'alpha monte d'une unité tant que le compteur est sous quarante ;
//   - et il redescend d'une unité passé un seuil que la SIXIÈME MARQUE porte :
//     soixante d'ordinaire, cent vingt pour une valeur, et JAMAIS pour une
//     autre. Cette dernière ne s'éteint donc pas d'elle-même.
//
// ⚠ LES DEUX BORNES NE SONT PAS EXCLUSIVES, et l'écart entre elles compte : de
// quarante à soixante tics, l'alpha ne bouge plus. C'est le palier du
// tourbillon, et le supprimer le ferait pâlir dès qu'il a fini de paraître.
void AdvanceSpiralStackTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::uint32_t& generator);

// LE SOUFFLE : le mouvement du sous-type 39.
//
// Transcrit le cas 39 du répartiteur. Tout y dépend de la CASE DE
// VARIANTE, celle où le semis a rangé l'octet du site :
//
//   - l'azimut avance de SIX degrés pour une valeur, de TROIS pour trois
//     autres, et ne bouge pas pour le reste ;
//   - l'inclinaison avance d'UN pour une valeur, de TROIS par défaut, et ne
//     bouge pas pour trois valeurs. Comme c'est elle qui mène le souffle du
//     rayon, une inclinaison figée donne un carré de taille constante ;
//   - et pour UNE SEULE valeur, le rayon lui-même respire : il gagne trois
//     dixièmes quand l'azimut a passé le demi-tour, et les perd sinon.
//
// ⚠ IL NE PARCOURT QUE DEUX PISTES SUR QUATRE — son compteur
// de boucle part à deux. Le semis n'en allume qu'une, si bien que cela ne se
// voit pas aujourd'hui ; l'écrire à quatre serait une supposition de plus.
void AdvanceBreathingQuadTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// L'EMPREINTE QUI S'EFFACE : le mouvement du sous-type 67. La première piste,
// seule allumée, compte ses tics et perd un cran d'alpha aux tics impairs, sans
// descendre sous zéro. Son inclinaison ne bouge pas : le carré ne respire pas.
void AdvanceFootprintTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// L'ÉVENTAIL QUI S'OUVRE : le mouvement du sous-type 89.
//
// Transcrit le cas 89 du répartiteur. Pour chaque piste vive dont
// le compteur est devenu positif, tout dépend de la CASE DE VARIANTE :
//
//   - deux variantes montent l'alpha de DIX par tic pendant dix tics, puis le
//     baissent de trois ; une autre monte de six et baisse d'un ; toutes les
//     autres montent de VINGT et retombent de quinze ;
//   - et le rayon croît de deux façons qui ne se ressemblent pas : il GAGNE
//     TROIS unités par tic pour les trois premières variantes, et il est
//     MULTIPLIÉ par un et quinze centièmes pour les autres. L'une ouvre en
//     ligne droite, l'autre en exponentielle.
//
// ⚠ LE RAYON CROÎT MÊME PENDANT LE PALIER, entre le onzième et le treizième
// tic, où l'alpha ne bouge plus : le client le pose hors des branches d'alpha.
void AdvanceConeWedgeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE GLISSEMENT : le mouvement du sous-type 56.
//
// Transcrit le cas 56 du répartiteur. Pour chaque piste vive :
//
//   - l'alpha monte pendant dix tics, puis retombe passé le vingtième ;
//   - l'extrémité proche AVANCE du pas que porte la case trois, et se referme
//     à deux cents — c'est ce qui fait boucler le glyphe au sol ;
//   - et pour les marques de dix et au-delà, CE PAS LUI-MÊME croît de cinq
//     centièmes par tic : le glissement accélère.
//
// ⚠ LES DEUX FAMILLES DE STYLES ONT DES RYTHMES INVERSES. Sous dix, l'alpha
// monte de DOUZE et retombe de QUATRE ; au-delà, il monte de VINGT et ne
// retombe que de DEUX. Celle qui s'allume lentement s'éteint vite, et
// réciproquement — les traiter pareil effacerait cette respiration.
void AdvanceGroundRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE DISQUE AU SOL QUI TOURNE, sous-type 26 : le cercle magique des cartes.
//
// Transcrit le cas 26 du répartiteur. Il
// ne fait que deux choses : le nœud reprend la position de son porteur, et
// l'azimut de la PREMIÈRE piste avance d'UN degré par tic, en rebouclant.
//
// ⚠ ET LE SENS DE ROTATION SE LIT DANS LA CASE DE VARIANTE. Le client compare
// la case quatre — celle où le semis a rangé l'octet du site — à QUATORZE, par
// égalité exacte de flottants : le disque de quatorze tourne à REBOURS, tous
// les autres à l'endroit. C'est ce qui fait qu'un cercle magique à trois
// couches a son anneau du milieu qui contre-tourne ; les faire tourner du même
// côté donnerait trois disques solidaires, ce qui se voit immédiatement.
//
// ⚠ ET LE CLIENT N'INTERROGE PAS L'ALLUMAGE de la piste. Il écrit l'azimut de
// la première quoi qu'il arrive, et le bâtisseur du sous-type 26 l'allume
// toujours ; la transcription reste donc sans garde, plutôt que d'en inventer
// une qui ne change rien.
void AdvanceGroundDiscTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE GRAIN QUI REMONTE : le mouvement du sous-type 157.
//
// Il range
// les deux angles de caméra dans les styles des deux premières pistes, puis pour
// chaque piste vive :
//
//   - l'alpha monte de vingt-cinq pendant les NEUF premiers tics, et se borne à
//     deux cent cinquante-cinq ;
//   - tant que l'ancre est SOUS le nœud, elle remonte de la clé de la piste ;
//     une fois remontée, l'alpha perd quinze par tic ;
//   - l'abscisse et la profondeur se recalculent à chaque tic depuis l'azimut et
//     la case zéro, qui ne bougent NI l'un NI l'autre : le grain monte droit ;
//   - et la piste s'éteint quand l'alpha passe sous zéro.
//
// ⚠ IL NE SUIT PAS SON PORTEUR, seul de sa génération. Le cas du répartiteur
// appelle la fonction sans recopier la position, et c'est voulu :
// l'émetteur sème un nœud tous les dix tics, chacun figé là où son porteur était.
// Les faire suivre collerait toute la pluie au joueur.
void AdvanceCameraFacingRadialTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                     std::int16_t billboard_turn_degrees,
                                     std::int16_t billboard_tilt_degrees);

// L'ANNEAU QUI S'OUVRE ET RECOMMENCE : le mouvement du sous-type 106.
//
// Transcrit le cas 106 du répartiteur, qui fait tout SUR PLACE. Le
// nœud reprend la position de son porteur, puis pour chaque piste vive dont le
// compteur est remonté :
//
//   - L'OUVERTURE DE L'ANNEAU monte d'UN degré par tic et l'alpha de DIX,
//     plafonné à soixante. Au quart de tour, l'ouverture REPART DE ZÉRO et
//     l'alpha de dix : c'est la seule boucle de la famille, et rien d'autre ne
//     la décrit ;
//   - PASSÉ LE TRENTIÈME TIC, un PLAFOND descend sur l'alpha — quatre par tic,
//     nul au quarante-cinquième. Ce n'est pas une soustraction : l'alpha ne
//     bouge que s'il dépasse le plafond, si bien que les derniers battements
//     s'écrêtent au lieu de s'assombrir régulièrement ;
//   - LE TOUR avance d'un degré par tic, en rebouclant ;
//   - L'AZIMUT se recale sur le porteur, DEMI-TOUR compris : l'anneau se dresse
//     derrière lui. Et les deux premières cases de queue reçoivent la
//     PERPENDICULAIRE à cet azimut — c'est l'axe sur lequel le centre de
//     l'anneau glisse pendant qu'il s'ouvre.
//
// `owner_facing_degrees` est l'angle du porteur, en degrés. `render` ne connaît
// pas l'état de jeu (CLAUDE.md) : l'appelant le donne.
void AdvanceRadialFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            float owner_facing_degrees);

// LE GRAIN QUI TOMBE PUIS FILE : le mouvement du sous-type 52, les feux
// d'artifice.
//
// Le nœud suit son
// porteur, les deux angles de caméra vont dans les styles des deux premières
// pistes, puis pour chaque piste vive dont le compteur est remonté :
//
//   - TOUS LES TROIS TICS, la trace se décale d'un cran : l'ancre passe dans la
//     première case de queue, la première dans la deuxième, la deuxième dans la
//     troisième. Seule la première paraît ;
//   - L'ÉTINCELLE s'allume dans une fenêtre de vingt tics propre à chaque piste,
//     et seulement à trois battements sur vingt. C'est elle qui fait briller la
//     gerbe ;
//   - L'AZIMUT tourne de cinq degrés par tic : le carré tourne sur lui-même ;
//   - PUIS DEUX ÉTATS. Tant que la marque d'état est nulle, le grain DESCEND
//     d'un quart par tic, INVISIBLE — son alpha ne naît qu'au cent
//     cinquante-cinquième tic. Ensuite il FILE dans une direction tirée au
//     semis, son rayon fondant de deux pour cent par tic, et son alpha tombe de
//     un ou deux selon un tirage ;
//   - ET AU DEUX MILLIÈME TIC LA PISTE RENAÎT : le compte revient à zéro, le
//     grain reprend sa chute, et sa direction comme son rayon se retirent.
//
// ⚠ AUCUNE PISTE NE S'ÉTEINT JAMAIS ICI. C'est le nœud qui meurt de sa vie ; la
// piste, elle, recommence indéfiniment. Lui donner une extinction couperait la
// gerbe au premier cycle.
//
// `node_position` est la position du nœud dans le monde — celle que la renaissance
// reprend —, et `generator` la suite de tirages du nœud.
void AdvanceSwirlOrbitTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             std::int16_t billboard_turn_degrees,
                             std::int16_t billboard_tilt_degrees, glm::vec3 node_position,
                             std::uint32_t& generator);

// L'ARC QUI S'OUVRE ET SE REFERME : le mouvement du sous-type 93.
//
// Transcrit le cas 93 du répartiteur, qui fait tout sur place et
// tient en onze instructions. Pour chaque piste vive :
//
//   - JUSQU'AU CENT QUATRE-VINGTIÈME TIC, l'alpha monte d'un et l'azimut avance
//     d'un degré ; du cent quatre-vingt-unième au trois cent soixantième,
//     l'alpha descend d'un et l'azimut avance encore. Au-delà, RIEN ne bouge ;
//   - l'alpha se borne entre zéro et deux cent cinquante ;
//   - et l'inclinaison se replie sous le tour sans jamais avancer.
//
// ⚠ CE MOUVEMENT NE SUIT NI SON PORTEUR NI LA CAMÉRA. L'inclinaison est l'angle
// que le BÂTISSEUR a figé à la naissance : l'arc garde son plan quoi que le
// joueur fasse tourner ensuite. Lui donner l'angle de caméra du tic le ferait
// pivoter avec l'écran, ce que le client ne fait pas.
void AdvanceCameraAlignedArcTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE FLOCON QUI TOURNE EN VOLANT : le mouvement du sous-type 134.
//
// Transcrit le cas 134 du répartiteur, qui fait tout sur place. Il
// range les deux angles de caméra dans les styles des deux premières pistes,
// puis pour chaque piste vive :
//
//   - le compteur avance, et l'ANGLE DU QUATRE-COINS avec lui, d'un degré par
//     tic : le flocon tourne sur lui-même ;
//   - l'alpha monte de vingt-cinq pendant les NEUF premiers tics, ne bouge plus
//     du dixième au quarantième, puis tombe de cinq. Il se borne à deux cent
//     cinquante-cinq et à zéro.
//
// ⚠ ET L'AVANCE N'EST PAS ICI. Le flocon ne porte aucune position qui bouge : sa
// figure RECALCULE à chaque image la distance parcourue depuis le compteur de
// tics. Chercher un pas dans ce mouvement serait chercher ce que le client n'y a
// pas mis — voir `ComposeFadingStreakQuads`.
void AdvanceFadingStreakTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t billboard_turn_degrees,
                               std::int16_t billboard_tilt_degrees);

// ⚠ LES MOUVEMENTS DES CINQ DERNIÈRES FAMILLES VIVENT À PART, dans
// `effect_track_burst.h` : cet en-tête avait franchi le plafond de huit cents
// lignes. Un fichier qui les emploie inclut celui-là, pas celui-ci.

}  // namespace ro::render
