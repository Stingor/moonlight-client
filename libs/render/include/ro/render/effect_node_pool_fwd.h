#pragma once

namespace ro::render {

// LA TROUPE ET SES NŒUDS, nommés sans être définis.
//
// Un semis ou un mouvement qui ne fait que déclarer une fonction prenant la
// troupe, ou rendant un nœud, s'en contente : la définition complète est dans
// `effect_node_pool.h`, et elle entraîne une douzaine d'en-têtes que seul le
// fichier qui l'implémente a besoin de relire.

struct PooledEffectNode;
class EffectNodePool;

}  // namespace ro::render
