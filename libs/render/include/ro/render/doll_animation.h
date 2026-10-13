#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/formats/image.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"

namespace ro::render {

// ── LE PANTIN EN IMAGES : DE QUOI FAIRE UN GIF ANIMÉ ────────────────────────
//
// La fiche montre un personnage qui bouge ; on veut l'emporter tel quel, fond
// transparent, au pixel près. Tout se calcule ici SANS CARTE GRAPHIQUE : quels
// instants échantillonner, combien de temps chacun dure, et les images RGBA
// elles-mêmes, que `formats` encode ensuite.
//
// 🔴 UN SEUL CADRAGE POUR TOUTES LES IMAGES. Cadrer chacune sur ce qu'elle
// peint ferait trembler le personnage d'une image à l'autre : l'enveloppe est
// l'UNION de toutes, et chaque image s'y dessine au même endroit.

// Le plus d'images qu'un GIF de pantin porte.
inline constexpr std::size_t kMostDollGifFrames = 40;
// LA PLUS LONGUE BOUCLE qu'on cherche à faire tomber juste, en millisecondes.
// Au-delà, deux horloges qui ne s'accordent qu'au bout d'une minute donneraient
// un fichier énorme pour une différence que personne ne verra.
inline constexpr float kLongestDollLoopMs = 10000.0F;
// LES PAS D'HORLOGE QU'ON CROIT, en millisecondes. En deçà d'une milliseconde ou
// au-delà d'une boucle entière, la cadence d'un .act est aberrante — nulle,
// infinie, ou d'un fichier abîmé — et l'horloge ne compte pas : la pièce reste
// sur son image, comme une pose tenue.
inline constexpr float kShortestDollStepMs = 1.0F;
inline constexpr float kLongestDollStepMs = kLongestDollLoopMs;
// LE JOUR AUTOUR DU PERSONNAGE, en pixels du GIF, de chaque côté.
inline constexpr int kDollGifMarginPx = 14;
// Les échelles entières permises : chaque pixel du sprite devient un carré.
inline constexpr int kSmallestDollGifScale = 1;
inline constexpr int kLargestDollGifScale = 3;
// Le plus grand côté de l'enveloppe, en pixels de sprite. Un personnage tient
// dans deux cents ; au-delà de celle-ci, c'est une pièce aberrante, et l'image
// ne se fait pas.
inline constexpr int kLargestDollSpritePx = 512;
// 🔴 LE PLUS DE PIXELS QUE LE GIF ENTIER PEUT PORTER, toutes images comprises.
// Les images se font AU CLIC, sur le fil de la boucle, puis l'encodeur en
// reçoit le tout : sans ce plafond, une grande enveloppe à l'échelle trois sur
// quarante images demanderait plus d'un gigaoctet d'un coup. Seize millions de
// pixels font 64 Mo de RGBA, et laissent passer tout personnage ordinaire.
inline constexpr std::size_t kMostDollGifPixels = std::size_t{16} * 1024 * 1024;

// LES HORLOGES D'UNE POSE.
struct DollClocks {
  // LE CORPS NE COMPTE QUE S'IL JOUE : une pose tenue garde son image zéro, et
  // seules les pièces qui ont leur propre horloge bougent — comme sur la fiche.
  bool body_plays = false;
  // Le nombre d'images de l'action du corps dans LA direction montrée, et la
  // durée de chacune.
  std::size_t body_frames = 1;
  float body_frame_ms = 0.0F;
  // Les coiffes et la cape animées.
  std::vector<AlternateClock> attachments;
};

// Les horloges d'une pose, lues sur la tenue. ⚠ LE CORPS EST LU DANS LA
// DIRECTION DEMANDÉE, et non dans la zéro : deux directions d'une même action
// peuvent ne pas compter autant d'images.
[[nodiscard]] DollClocks DollClocksOf(const ActorOutfit& outfit, std::size_t animation,
                                      std::size_t direction, bool body_plays);

// LES INSTANTS À ÉCHANTILLONNER, et le délai de chacun.
struct DollTimeline {
  // Depuis le début de l'animation, en millisecondes, croissants.
  std::vector<float> sample_ms;
  // Un par instant, en centièmes de seconde : l'unité du GIF.
  std::vector<int> delays_cs;
};

// Le plan d'une boucle qui retombe juste.
//
// 🔴 LA BOUCLE EST LE PLUS PETIT COMMUN MULTIPLE des tours du corps et des
// pièces animées, et chaque changement d'image de l'une d'elles est un instant.
// Un GIF qui bouclerait sur le seul tour du corps couperait un chapeau au
// milieu du sien : on le verrait sauter à chaque reprise.
//
// ⚠ SAUF QUAND CE N'EST PAS RAISONNABLE : plus de `kMostDollGifFrames`
// instants, une boucle de plus de `kLongestDollLoopMs`, ou un tour qui ne tombe
// pas sur une milliseconde entière. On se contente alors du tour du CORPS —
// ou, corps immobile, du plus long des tours des pièces —, borné au même
// nombre d'images.
//
// 🔴 LES DÉLAIS SE CUMULENT SANS DÉRIVE : le délai i vaut l'arrondi de
// l'instant i+1 moins celui de l'instant i, en centièmes. Arrondir chaque délai
// pour lui-même ferait glisser l'animation d'un centième par image. Deux
// instants plus proches que `kFastestGifDelayCs` n'en font qu'un : les
// navigateurs n'obéissent plus en deçà.
//
// Rien d'animé — ou seulement des horloges hors de [kShortestDollStepMs,
// kLongestDollStepMs] — : une seule image, de délai nul.
[[nodiscard]] DollTimeline PlanDollTimeline(const DollClocks& clocks);

// UN ÉCHANTILLON POUR MESURER UNE ENVELOPPE : l'instant qui place les pièces à
// horloge propre, en millisecondes, et l'image que le corps tient.
struct DollProbe {
  float at_ms = 0.0F;
  std::size_t body_frame = 0;
};

// DE QUOI VOIR CHAQUE IMAGE DE CHAQUE PIÈCE, sur chaque image du corps.
//
// 🔴 CE N'EST PAS LE PLAN D'UN GIF. Une boucle qui retombe juste peut compter
// des centaines d'instants, et son repli n'échantillonne une pièce qu'aux pas
// d'un autre tour : il en manquerait les images extrêmes. Ici, chaque tour se
// parcourt sur SA propre longueur, et le corps se tient sur chacune de ses
// images — celle de `rest_frame` seule quand il ne joue pas. Une pièce
// s'accroche au corps ou à la tête, jamais à une autre pièce : ce produit voit
// donc chaque position qu'elle prend. Coût : images du corps × somme des pas.
[[nodiscard]] std::vector<DollProbe> DollEnvelopeProbes(const DollClocks& clocks,
                                                        std::size_t rest_frame);

// La pose d'un échantillon. ⚠ LA RÉFÉRENCE APPARTIENT À LA TENUE.
[[nodiscard]] const ComposedActor& ComposeDollProbe(ActorOutfit& outfit, std::size_t animation,
                                                    std::size_t direction, const DollProbe& probe);

// CE QU'ON PHOTOGRAPHIE.
struct DollShot {
  // Le RANG d'animation, tel que le .act les range — voir `sprite_frame.h`.
  std::size_t animation = kAnimationIdle;
  std::size_t direction = 0;
  // Voir `DollClocks::body_plays`.
  bool body_plays = false;
  // L'échelle ENTIÈRE, au plus proche voisin. Bornée à
  // [kSmallestDollGifScale, kLargestDollGifScale].
  int scale = kSmallestDollGifScale;
  int margin_px = kDollGifMarginPx;
};

// LES POSES COMPOSÉES, une par instant du plan, et leur enveloppe commune.
struct DollPoses {
  std::vector<ComposedActor> poses;
  // Le coin haut-gauche de l'enveloppe, en pixels de sprite autour des pieds,
  // et sa taille, arrondie au pixel entier qui la contient.
  glm::vec2 low{0.0F, 0.0F};
  int width = 0;
  int height = 0;
};

// Compose chaque instant du plan, et mesure l'union de ce qu'ils peignent.
//
// Échoue si la tenue ne peint rien, ou si l'enveloppe dépasse
// `kLargestDollSpritePx` de côté.
[[nodiscard]] core::Result<DollPoses> ComposeDollPoses(ActorOutfit& outfit, const DollShot& shot,
                                                       const DollTimeline& timeline);

// Le nombre de pixels du GIF entier, toutes images comprises, à cette échelle
// et cette marge. À comparer à `kMostDollGifPixels` AVANT de dessiner.
[[nodiscard]] std::size_t DollGifPixels(const DollPoses& poses, const DollShot& shot);

// Les images du GIF, toutes de la même taille.
//
// La toile vaut l'enveloppe commune, agrandie de l'échelle, plus la marge de
// chaque côté : le CENTRE DE L'ENVELOPPE tombe donc au milieu de la toile en
// abscisse, et son bas — les pieds, ou ce qui descend plus bas qu'eux — à une
// marge du bord inférieur.
//
// ⚠ LES COULEURS SONT DÉ-PRÉMULTIPLIÉES : la toile de sprite fond chaque calque
// sur un fond noir transparent, ce qui assombrit un bord à demi opaque. Le GIF,
// qui n'a pas de demi-teinte, doit en recevoir la VRAIE couleur.
//
// Échoue au-delà de `kMostDollGifPixels`, sans rien allouer.
[[nodiscard]] core::Result<std::vector<formats::Image>> DrawDollFrames(const DollPoses& poses,
                                                                       const DollShot& shot);

}  // namespace ro::render
