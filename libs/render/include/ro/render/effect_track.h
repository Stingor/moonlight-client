#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// Une PISTE de ruban : l'état qu'un effet procédural entretient, et que sa
// figure lit pour se dessiner.
//
// C'est le socle de la famille procédurale, et l'ADR 0014 ne l'avait pas vu :
// cent dessinateurs sur cent onze lisent là-dedans, et ils couvrent 784 des 801
// nœuds qui n'ont pas d'image. Une figure de cette famille ne se porte pas sans
// sa piste — elle n'aurait rien à dessiner.
//
// ⚠ CE N'EST PAS UN CHAMP DU NŒUD, c'est un ÉTAT. Le bâtisseur la pose, le
// mouvement la fait vivre à chaque tic, et le dessinateur la lit. Les trois sont
// distincts, et porter l'un sans les autres donne soit une figure figée, soit un
// état que personne ne montre.
//
// Dans le client, un nœud en porte quatre à la suite, de +1000 à +1719, chacune
// sur cent quatre-vingts octets. Deux mesures indépendantes tombent sur cette
// borne : quatre fois cent quatre-vingts font exactement sept cent vingt, et le
// relevé du lot 7 nomme +1720 autre chose.
//
// ⚠ LE SENS D'UN CHAMP APPARTIENT AU DESSINATEUR, PAS AU CHAMP. C'est l'ADR 0017
// descendu d'un étage : après le nom d'une fonction, c'est maintenant le nom
// d'un champ qui ne veut rien dire hors du dessinateur qui le lit. Deux mesures
// le montrent sur la même case : ce qu'un anneau prend pour l'arc qu'il balaie,
// un billboard le prend pour un cap, et ce qu'un anneau prend pour la longueur
// de ses segments, un billboard le prend pour une échelle, une demi-taille et un
// angle. Les champs dont les deux lectures s'accordent portent ici un nom ; les
// autres portent leur TYPE et leur unité, et c'est la figure qui les nomme.

// Les dimensions d'une piste et le compte de pistes par nœud : voir
// `effect_track_fwd.h`.

struct EffectTrack {
  // Une piste éteinte ne se dessine pas et n'avance pas. Le bâtisseur allume
  // celles qu'il emploie ; un effet n'en veut pas toujours quatre.
  bool active = false;

  // De zéro à 255, en ENTIER — le mouvement le fait monter et descendre par
  // pas entiers, là où l'alpha du nœud est fractionnaire. Les deux coexistent
  // et ne se remplacent pas.
  std::int16_t alpha = 0;

  // Le nombre que le bâtisseur passe à sa figure. Le mouvement s'en sert pour
  // choisir une branche, et un dessinateur au moins le lit comme un ANGLE :
  // lui donner un sens unique serait le deviner.
  std::int16_t style = 0;

  // UN ANGLE EN DEGRÉS, dont le dessinateur décide du sens. L'anneau y lit
  // quelle part du cercle il balaie — trois cent soixante le referme, en deçà
  // c'est un arc ; le billboard y lit le cap sous lequel il se pose.
  std::int16_t degrees = 0;

  // Où le ruban commence sur le cercle. Quatre pistes à quatre-vingt-dix degrés
  // d'écart font une figure qui tourne sans qu'aucune ne tourne.
  std::int16_t azimuth_degrees = 0;

  // De combien le bord extérieur s'écarte du plan de l'anneau. Le mouvement le
  // couche peu à peu, ce qui fait retomber la jupe.
  std::int16_t tilt_degrees = 0;

  // Depuis combien de tics la piste vit. NÉGATIF au départ pour les pistes que
  // le bâtisseur veut retarder : tant qu'il l'est, la piste ne bouge pas.
  std::int32_t ticks = 0;

  // UNE CLÉ, PAS UNE MESURE. La figure la compare à deux valeurs exactes et en
  // tire un décalage vertical du ruban ; toute autre valeur ne décale rien. Le
  // bâtisseur du sous-type 30 n'y pose que des entiers de douze à vingt-cinq
  // (voir `effect_track_seed.h`), donc aucun décalage. Le client range d'ailleurs
  // vingt-quatre juste à côté de la clé 24.000999 : la lire comme une hauteur
  // soulèverait l'anneau de vingt-quatre unités.
  float lift_key = 0.0F;

  // Le rayon du cercle intérieur, en unités du monde. C'est lui que le
  // mouvement fait croître, et dont les segments tirent leur ondulation.
  float radius_at_anchor = 0.0F;

  // VINGT ET UNE VALEURS DONT LE SENS APPARTIENT AU DESSINATEUR.
  //
  // L'anneau y lit la longueur de son ruban à chaque segment, du bord intérieur
  // vers l'extérieur, et son bâtisseur les pose à zéro — un ruban sans longueur
  // n'a pas d'aire, et c'est le mouvement qui lui en donne une. Le billboard n'y
  // lit que les trois premières : une échelle, une demi-taille, et un angle rangé
  // en flottant. Chaque figure nomme les cases qu'elle emploie ; les nommer ici
  // en donnerait vingt et un noms faux à toutes les autres.
  std::array<float, kEffectTrackSlots> value = {};

  // VINGT ET UN OCTETS, de même. Le bâtisseur de l'anneau les met à zéro ; celui
  // du billboard écrit dans le premier le cas de couleur que sa figure choisit.
  std::array<std::uint8_t, kEffectTrackSlots> mark = {};

  // OÙ LA PISTE S'ANCRE dans le monde, et non où le nœud s'ancre : un bâtisseur
  // qui sème quatre pistes leur donne le même point, un autre les disperse.
  // Zéro veut dire « au nœud », ce qui est le cas de l'anneau.
  glm::vec3 anchor = {0.0F, 0.0F, 0.0F};

  // NEUF CASES DE PLUS, et leur sens appartient encore au dessinateur.
  //
  // Elles closent les cent quatre-vingts octets d'une piste. Les deux premières
  // familles portées n'en lisent aucune, et c'est pourquoi elles manquaient ;
  // la troisième y range la hauteur d'où sa figure part. Les compter absentes
  // parce que deux familles les ignorent aurait fait croire la piste finie.
  std::array<float, kEffectTrackTailSlots> tail = {};
};

}  // namespace ro::render
