#include "map_edit_header.h"

namespace ro::mapedit::detail {

// L'EN-TÊTE QUE L'ÉDITEUR ÉCRIT : la mention d'origine, le format, et les
// commandes qui servent à écrire et à vérifier un document.
const std::string_view kMapEditHeader =
    R"(# ─────────────────────────────────────────────────────────────────────────────
#  L'édition d'une carte
#
#  Donnée de moonclient, tenue par le staff (docs/adr/0118). La carte reste
#  celle de l'installation : le client applique ces opérations au chargement,
#  avant de bâtir ses maillages. Le visuel seulement : la marchabilité (GAT)
#  n'est jamais touchée.
#
#  Écrit par l'éditeur de carte du jeu ; il se relit et se corrige aussi à la
#  main.
#
#  map    : la carte, son nom sans extension — celui du fichier ;
#  revision : facultatif, le numéro que le serveur a donné au document
#             (docs/adr/0124) — absent, jamais servi ;
#  models : les retouches de modèles du RSW, chacune :
#    index    : le rang du modèle dans le RSW, compté à partir de zéro ;
#    file     : son fichier, relatif à data\model\ ;
#    origin   : sa position dans le RSW d'origine, [x, y, z] ;
#    position, rotation, scale : ce qui change, [x, y, z], dans le repère du
#               fichier — un champ absent garde la valeur du RSW ;
#    hidden   : vrai pour ne plus le dessiner ;
#    opacity  : facultatif, de 0 (invisible) à 1 (plein) — absent, plein ;
#    why      : pourquoi, obligatoire.
#  added  : les modèles ajoutés, qui prennent rang après ceux du RSW, chacun :
#    file     : son fichier, relatif à data\model\ ;
#    position, rotation, scale : son placement, [x, y, z], dans le repère du
#               fichier — position est obligatoire ;
#    hidden   : vrai pour ne pas le dessiner ;
#    opacity  : facultatif, de 0 (invisible) à 1 (plein) — absent, plein ;
#    animation : facultatif, comment il joue les images clés de son fichier —
#               loop en boucle, once une fois, still figé ; absent, loop ;
#    animation_speed : facultatif, sa vitesse de jeu, de 0 exclu à 100 —
#               absent, 1 ;
#    why      : pourquoi, obligatoire.
#  lights :les lumières ponctuelles ajoutées, chacune :
#    position : [x, y, z], dans le repère du RSW ;
#    color    : [r, g, b], 1 pour la pleine intensité — au-delà, plus fort ;
#    range    : le rayon où elle s'éteint, en unités du RSW ;
#    night_only : vrai pour ne l'allumer qu'à la nuit ;
#    hours    : facultatif, [de, à] — sa plage d'heures du serveur, de 0 à 24 ;
#    flicker  : vrai pour qu'elle vacille, comme une torche ;
#    shadow   : faux pour qu'elle ne projette pas d'ombre — une lumière dans
#               une lanterne, que la lanterne boucherait ;
#    why      : pourquoi, obligatoire.
#  effects : les effets de carte ajoutés, chacun :
#    number   : le numéro de l'effet, comme un objet d'effet du RSW ;
#    position : [x, y, z], dans le repère du RSW ;
#    emit_speed, param : sa cadence et ses quatre paramètres, comme le RSW ;
#    night_only : vrai pour ne le lever qu'à la nuit ;
#    hours    : facultatif, [de, à] — sa plage d'heures du serveur, de 0 à 24 ;
#    glow     : facultatif, la lumière qu'il porte — color, range, flicker,
#               shadow ;
#    why      : pourquoi, obligatoire.
#  meshes : les retouches de maillages, chacune :
#    index, file, origin : la pose qu'elle désigne, comme une retouche de
#               modèle ;
#    all_poses : vrai pour toutes les poses du fichier, sinon cette pose seule ;
#    vertices : les sommets déplacés — node (le nom du nœud), vertex (son rang
#               dans le nœud), origin et position, dans le repère du nœud ;
#    why      : pourquoi, obligatoire.
#  ground : les retouches du relief, à la brosse :
#    why      : pourquoi, obligatoire — une raison pour tout le relief ;
#    points   : [colonne, rangée, levée] — un point de la grille des coins du
#               sol, (largeur + 1) × (hauteur + 1) points, le coin bas-gauche
#               du cube (x, y) étant le point (x, y) ; la levée en unités du
#               monde, positive vers le haut. Elle lève tous les coins de cube
#               qui touchent le point : une falaise garde sa marche.
#  sprites : les sprites de décor — une créature seulement peinte, que le
#            serveur ne connaît pas et qu'on ne clique pas —, chacun :
#    sprite   : son nom dans la table des classes (« SAVAGE »), ou le numéro de
#               sa classe ;
#    position : ses pieds, [x, y, z], dans le repère du RSW ;
#    action   : le rang d'animation du .act — 0 attente, 1 marche, 2 attaque,
#               3 blessure, 4 mort ;
#    frame    : facultatif, l'image tenue — absent, l'animation boucle ;
#    speed    : facultatif, la cadence de la boucle, en fois celle du .act ;
#    direction : de 0 (de face) à 7, dans le sens des directions du .act ;
#    turn     : facultatif, la rotation, en degrés par seconde ;
#    roll, roll_speed : facultatifs, l'image pivotée dans son plan, en degrés
#               et en degrés par seconde — une broche ;
#    scale    : facultatif, l'échelle ; opacity : facultatif, de 0 à 1 ;
#    hidden   : vrai pour ne pas le dessiner ;
#    why      : pourquoi, obligatoire.
#  map_sounds : les retouches des sources de son du RSW, chacune :
#    index    : le rang de la source dans le RSW, compté à partir de zéro ;
#    file     : son fichier, relatif à data\wav\ ;
#    origin   : sa position dans le RSW d'origine, [x, y, z] ;
#    position : facultatif, sa nouvelle place, [x, y, z] ;
#    hidden   : vrai pour la faire taire ;
#    volume, range, cycle : facultatifs — le volume de 0 à 1, la portée en
#               unités du RSW, le cycle en secondes ; absents, ceux du RSW ;
#    night_only : vrai pour qu'elle ne sonne qu'à la nuit ;
#    hours    : facultatif, [de, à] — sa plage d'heures du serveur, de 0 à 24 ;
#    why      : pourquoi, obligatoire.
#  sounds : les sons d'ambiance ajoutés, chacun :
#    file     : son fichier, relatif à data\wav\ ;
#    position : [x, y, z], dans le repère du RSW ;
#    volume   : de 0 à 1 ;
#    size     : [largeur, hauteur], l'emprise en unités entières — une largeur
#               nulle ne sonne jamais ;
#    range    : jusqu'où il s'entend, en unités du RSW, strictement positive ;
#    cycle    : tous les combien de secondes il se rejoue, strictement positif ;
#    night_only, hours : comme une retouche ;
#    why      : pourquoi, obligatoire.
#  Une opération dont la cible ne correspond plus n'est pas posée, et le
#  journal le dit.
#
#  Les modèles d'une carte, leur rang et leur placement :
#      roc-cli map models <dossier client> <carte> [<case x> <case y> <rayon>]
#  Vérifier un document sans l'installer (le journal dit ce qui est posé et ce
#  qui est refusé) :
#      roc-cli map view <dossier client> <carte> <sortie.png> <x> <y> \
#          --edition dist/data/map_edits/<carte>.yml
# ─────────────────────────────────────────────────────────────────────────────
)";

}  // namespace ro::mapedit::detail
