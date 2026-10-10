#pragma once

#include <imgui.h>

namespace ro::ui {

// DU TEXTE QUI RESTE LISIBLE SUR N'IMPORTE QUOI.
//
// Le jeu écrit par-dessus la scène — un rang sous une icône, un compteur, un
// temps restant — et le décor dessous peut être de n'importe quelle couleur. Il
// pose donc le texte DEUX FOIS : une ombre ou un cerne, puis le texte.

// LE CERNE : les huit voisins, puis le texte.
//
// ⚠ RAYON D'UN PIXEL D'ÉCRAN, JAMAIS MIS À L'ÉCHELLE. C'est un cerne, pas une
// bordure : à l'échelle, il deviendrait un liseré épais qui mangerait le glyphe.
//
// `bold` repasse le texte décalé d'un pixel, dans sa propre couleur : le faux
// gras du client, celui qui épaissit sans changer de police — la nôtre n'a
// qu'une graisse chargée, et en charger une seconde coûterait un atlas entier.
void AddTextHalo(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, ImU32 halo,
                 bool bold = false);

// Le même cerne, sur une TRANCHE de chaîne. Une ligne mesurée par pointeurs
// n'est jamais terminée par un zéro à l'endroit voulu.
void AddTextHalo(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, const char* text_end,
                 ImU32 halo, bool bold = false);

// LE MÊME CERNE, À UNE TAILLE IMPOSÉE.
//
// ⚠ POUR CE QUI DOIT TENIR DANS UNE PLACE DONNÉE, et non pour faire joli : une
// étiquette de touche dans une case de raccourci, où « Ctrl+Maj+F1 » déborde à
// la taille ordinaire. L'appelant mesure, rétrécit, et passe la taille qu'il a
// trouvée.
//
// `size` à zéro ou moins reprend la taille courante.
void AddTextHalo(ImDrawList* into, float size, ImVec2 at, ImU32 color, const char* text, ImU32 halo,
                 bool bold = false);

// LE CERNE EN CROIX : quatre passes au lieu de huit.
//
// ⚠ VOLONTAIREMENT PLUS LÉGER, et ce n'est pas une version incomplète du
// précédent : sur un petit texte serré — le rang sous une icône de compétence —
// les huit passes épaississent au point de fermer les contre-formes.
void AddTextCrossHalo(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, ImU32 halo);

// L'OMBRE PORTÉE : le texte une fois décalé, puis à sa place.
//
// ⚠ LE DÉCALAGE EST EN PIXELS D'ÉCRAN, pas à l'échelle : qui veut qu'il suive
// l'échelle le passe déjà converti. Le convertir ici doublerait la mise à
// l'échelle de ceux qui le font.
void AddTextRelief(ImDrawList* into, ImVec2 at, ImU32 color, const char* text, ImU32 relief,
                   ImVec2 offset, bool bold = false);

}  // namespace ro::ui
