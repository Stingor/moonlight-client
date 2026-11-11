#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/sim/style.h"

namespace ro::net {

// LES RECETTES DE LA TABLÉE : le serveur de PERSONNAGES rend, pour chaque
// personnage du compte, ce qu'il montrait à sa dernière déconnexion — v7 et
// calques sous les clés affichées. Voir l'ADR 0147 et
// docs/couleurs-du-personnage/conception.md, lot L14.
//
// LA DEMANDE (`CH 0x0F65`, fixe) : [op:2][capacités:4].
// LA RÉPONSE (`HC 0x0F66`) : [op:2][taille:2][drapeaux:1][taille_brute:2] puis
// des blocs [char_id:4] et ce que lit `ReadShownStyle`. Le drapeau 0x02 dit
// qu'une autre trame de la même réponse suit. Un personnage sans bloc ne montre
// rien.
//
// 🔴 UN SERVEUR DE PERSONNAGES QUI NE CONNAÎT PAS 0x0F65 COUPE LA LIAISON : la
// demande ne part qu'après la règle 28, vue en jeu et gardée dans le cache de la
// tablée, et seulement si la table des longueurs connaît 0x0F66.

// La capacité qu'annonce la demande : ce client lit le HC 0x0F66.
inline constexpr std::uint32_t kCharSelectStylesReads = 0x00000001;

[[nodiscard]] std::vector<std::uint8_t> EncodeCharSelectStylesRequest();

// Vrai si la table des longueurs connaît la réponse.
[[nodiscard]] bool CharSelectStylesKnown();

struct CharSelectStyle {
  std::uint32_t char_id = 0;
  sim::StyleVariants variants;
  std::vector<sim::StyleLayerRecipe> layers;
};

// UNE TRAME DE LA RÉPONSE.
struct CharSelectStylesAnswer {
  std::vector<CharSelectStyle> blocks;
  // Une autre trame de la même réponse suit.
  bool more = false;
};

// 🔴 TOUT OU RIEN : un bloc qui déborde rend une erreur, et rien n'est rangé.
[[nodiscard]] core::Result<std::vector<CharSelectStyle>> ReadCharSelectStylePayload(
    std::span<const std::uint8_t> raw);
// La trame `HC 0x0F66`, décompressée à la taille exacte annoncée, plafonnée.
[[nodiscard]] core::Result<CharSelectStylesAnswer> ReadCharSelectStylesFrame(const Frame& frame);

}  // namespace ro::net
