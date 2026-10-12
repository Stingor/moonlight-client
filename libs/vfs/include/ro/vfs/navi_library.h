#pragma once

#include <cstdint>
#include <string>

#include "ro/core/error.h"
#include "ro/formats/navi_npc_distances.h"
#include "ro/formats/navi_tables.h"

namespace ro::vfs {

class Vfs;

// Ce que le chargement a coûté, table par table, pour le journal.
struct NaviLoadReport {
  std::uint32_t read_ms = 0;
  std::uint32_t parse_ms = 0;
  std::uint64_t bytes = 0;
  // La première table optionnelle qui a manqué ou s'est refusée, vide sinon.
  std::string degraded;
};

// Charge les tables de navigation depuis l'archive, SAUF celle des distances aux
// PNJ, qui se lit à la demande (`LoadNaviNpcDistanceFile`).
//
// La table des cartes est OBLIGATOIRE : sans elle, rien ne se résout. Les autres
// ne le sont pas — sans les PNJ la recherche est plus pauvre, sans les distances
// l'itinéraire est estimé moins finement —, et leur absence se dit dans
// `report.degraded` plutôt qu'elle ne refuse tout.
//
// ⚠ LONG : quelques mégaoctets de texte. À appeler hors du fil de rendu.
[[nodiscard]] core::Result<formats::NaviData> LoadNaviData(Vfs& vfs, NaviLoadReport& report);

// Lit la table des distances aux PNJ et repère le bloc de chaque carte, sans
// les analyser. Pour le seul repli du guidage vers un PNJ dont la carte n'a
// pas de grille lisible : une lecture de 5 Mo, qu'on ne paie qu'à ce moment.
[[nodiscard]] core::Result<formats::NaviNpcDistanceFile> LoadNaviNpcDistanceFile(Vfs& vfs);

}  // namespace ro::vfs
