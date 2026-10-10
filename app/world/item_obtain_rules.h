#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/ui/item_obtain_toast.h"

namespace moonclient {

// LA PILE DU BANDEAU D'OBTENTION : quand deux ramassages n'en font qu'un, quand
// une ligne meurt, et laquelle tombe quand la pile déborde.
//
// 🔴 DES FONCTIONS PURES, SANS MONDE NI TEXTURE, et c'est ce qui les rend
// essayables : les trois règles qui décident de ce qu'on lit sont muettes en
// jeu. Une ligne qui ne fusionne pas ressemble à un second ramassage, une ligne
// qui expire trop tôt ressemble à une ligne qu'on n'a pas vue passer, et
// l'écrêtage par le mauvais bout jette la plus RÉCENTE sans que rien ne le dise.

// UNE LIGNE DE LA PILE.
//
// ⚠ ELLE PORTE SES TEXTES, elle ne les désigne pas : la vue passée au dessin
// est prise APRÈS coup, sur une pile qui ne bouge plus. Une vue prise pendant
// le remplissage meurt à la première réallocation du vecteur.
struct ObtainedLine {
  std::uint32_t item_id = 0;
  // La quantité CUMULÉE de la ligne, regroupements compris.
  std::int32_t amount = 0;
  std::int32_t refine_level = 0;
  bool identified = true;
  bool damaged = false;
  // L'instant du DERNIER apport : un regroupement fait repartir le minuteur.
  std::uint32_t born_ms = 0;
  // Le nom composé de l'objet, en UTF-8.
  std::string name;
  // « - 5 obtained. », refait à chaque apport puisque la quantité change.
  std::string suffix;
};

// DEUX APPORTS VONT-ILS SUR LA MÊME LIGNE ?
//
// Même objet, et même ÉTAT : sinon un « +7 » fraîchement forgé se ferait avaler
// par la pile des non-affinés, et un équipement cassé par celle des intacts.
[[nodiscard]] bool SameObtainedLine(const ObtainedLine& first, const ObtainedLine& second);

// Verse un apport dans la pile : il s'additionne à sa ligne et en relance le
// minuteur, ou il en ouvre une.
//
// Rend l'INDICE de la ligne touchée — c'est elle dont le suffixe est à refaire,
// sa quantité ayant changé. La règle ne sait pas le formater : le libellé vient
// de l'installation du joueur.
[[nodiscard]] std::size_t MergeOrPushObtained(std::vector<ObtainedLine>& stack,
                                              ObtainedLine arriving, bool merge_same);

// Retire les lignes dont le temps est passé.
//
// ⚠ SOUSTRACTION NON SIGNÉE : `now - born >= life`, et jamais
// `now > born + life`, qui se trompe au débordement de l'horloge — la somme y
// déborde à son tour et retombe SOUS `now`, si bien qu'une ligne née juste
// avant le débordement s'efface à la milliseconde suivante. C'est le défaut du
// client, qu'on ne reprend pas.
void ExpireObtainedLines(std::vector<ObtainedLine>& stack, std::uint32_t now_ms,
                         std::uint32_t life_ms);

// Ramène la pile au nombre voulu.
//
// ⚠ CE SONT LES PLUS ANCIENNES QUI TOMBENT, donc on jette par le DÉBUT : la
// pile est tenue dans l'ordre d'arrivée, et c'est le dessin qui la renverse
// quand le réglage veut la plus récente en haut.
void CapObtainedLines(std::vector<ObtainedLine>& stack, int most_lines);

// Combien de temps une ligne tient, réglage borné.
//
// Le plancher n'est pas un garde-fou de façade : au-dessous, une ligne
// disparaît avant d'avoir été lue, et le bandeau ne fait plus que clignoter.
[[nodiscard]] std::uint32_t ObtainedLifeMs(int duration_ms);

// Combien de lignes la pile garde, réglage borné par le plafond dur.
[[nodiscard]] int ObtainedLineCap(int most_lines);

}  // namespace moonclient
