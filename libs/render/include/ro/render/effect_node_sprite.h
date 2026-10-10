#pragma once

#include <vector>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/effect_node_fwd.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {

// Le dessinateur d'effets procéduraux LE PLUS EMPLOYÉ du client : celui qui
// pose un SPRITE.
//
// Sur les soixante-quatorze effets relevés dans
// `scripts/client_effect_node_kinds.json`, trente-quatre en dépendent — Sight,
// Heal, Blessing, Aspersio, Detoxication, Soul Strike, Fire Ball, Steal. Il ne
// dessine pas des quads mais une image d'ACT, c'est-à-dire exactement ce que
// nous savons déjà composer.
//
// Ce que le nœud apporte au-dessus du sprite : SON échelle, SA teinte, et le
// choix de l'action et de l'image.

// Les quads d'un nœud dessiné en sprite, en pixels autour de son origine.
//
// ⚠ AUCUNE ROTATION, et ce n'est pas un oubli. Le client additionne bien
// l'angle du calque et celui du nœud — puis LAISSE TOMBER la somme : les quatre
// valeurs qu'il remet à la file de rendu sont un rectangle aligné aux axes,
// haut, gauche, bas, droite. Appliquer la rotation serait fidèle au FICHIER et
// infidèle au JEU, comme pour un acteur.
//
// La teinte du nœud recouvre celle des calques, sauf quand le nœud demande le
// contraire : c'est ainsi qu'un effet peut soit se colorer en bloc, soit
// laisser parler les couleurs de son fichier.
[[nodiscard]] std::vector<SpriteQuad> ComposeEffectNodeSprite(
    const EffectNode& node, const formats::SprSprite& sprite,
    const formats::ActAnimation& animation);

}  // namespace ro::render
