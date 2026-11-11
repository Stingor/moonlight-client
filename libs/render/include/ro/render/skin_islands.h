#pragma once

#include <cstdint>
#include <span>

#include "ro/formats/spr.h"

namespace ro::render {

// LES REFLETS ÉGARÉS D'UN VISAGE REJOIGNENT LA PEAU.
//
// Sur bien des têtes, un reflet de joue, de nez ou de menton a été peint d'une
// entrée de palette hors de l'ensemble de peau — presque toujours une couleur
// de cheveux. Le réglage de peau ne l'atteint pas, et la palette de cheveux le
// repeint : quelques points clairs restent sur un visage foncé, ou prennent la
// couleur des cheveux. `roc-cli palette peau-egaree` en compte sur 144 têtes
// humaines sur 180.
//
// UN ÎLOT est un groupe de pixels hors de la peau, d'un seul tenant (quatre
// voisins), que la peau entoure : il ne touche ni la transparence ni le bord
// de l'image, tient en `kSkinIslandMostPixels` pixels au plus, et ne porte
// aucun pixel plus sombre que le ton le plus sombre de la peau.
//
// 🔴 LE PIXEL SOMBRE EST CE QUI ÉPARGNE LES YEUX ET LA BOUCHE : une pupille, un
// trait en portent toujours un, un reflet égaré n'en porte pas. Un filtre de
// teinte les épargnait aussi, mais perdait l'entrée la plus fréquente.
//
// Chaque pixel d'un îlot prend l'entrée de peau de clarté la plus proche, sur
// la palette du sprite — celle du graphiste. Il suit alors le réglage de peau
// comme le reste du visage. La clarté se juge à la plus forte composante.
//
// ⚠ AU CHARGEMENT, UNE FOIS : la pipette et le rendu lisent les mêmes index.
// Rien ne voyage : chaque client fait le même calcul sur le même fichier.

// Le plus gros îlot repris : au-delà, c'est un œil entier ou une bouche.
inline constexpr int kSkinIslandMostPixels = 12;

// Reprend les îlots des images palettisées de `sprite` ; rend le nombre de
// pixels repris. Sans palette ou sans ensemble de peau, ne fait rien.
int FoldSkinIslands(formats::SprSprite& sprite, std::span<const std::uint8_t> skin);

}  // namespace ro::render
