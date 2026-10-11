#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── L'INSPECTEUR D'ENTITÉ DU STAFF ──────────────────────────────────────────
//
// Ce que le SERVEUR sait d'une entité désignée — fichier de script d'un PNJ,
// spawn d'un monstre, lanceur d'une unité de compétence, réservation d'un objet
// au sol, compte et état d'un joueur —, en tableau clé / valeur.
//
// 🔴 LES LIGNES SONT CELLES DU SERVEUR, montrées telles quelles : elles ne
// passent par aucun catalogue de traduction. Une ligne sans valeur est un
// titre de section.

inline constexpr const char* kEntityInspectorWindowId = "inspecteur_entite";

// Où en est la demande.
enum class EntityInspectorPhase : std::uint8_t {
  // Partie, sans réponse encore.
  kAsking,
  // Partie depuis trop longtemps : le serveur ne connaît peut-être pas le
  // paquet, ou n'a pas reconnu la poignée de main.
  kSilent,
  // Le serveur refuse : ce compte n'est pas du staff.
  kDenied,
  // Aucune entité sous cet identifiant chez le serveur.
  kMissing,
  kReady,
};

struct EntityInspectorRow {
  std::string key;
  // Vide : la clé est un titre de section.
  std::string value;
};

struct EntityInspectorView {
  // Ce qu'on inspecte, DÉJÀ TRADUIT : la famille, le nom, l'identifiant.
  std::string subject;
  EntityInspectorPhase phase = EntityInspectorPhase::kAsking;
  std::vector<EntityInspectorRow> rows;
};

struct EntityInspectorWindowState {
  bool open = false;
  // Mise au premier plan à la prochaine image : une nouvelle cible s'y montre.
  bool raise = false;
};

struct EntityInspectorAnswer {
  // Redemander au serveur.
  bool refresh = false;
};

[[nodiscard]] EntityInspectorAnswer DrawEntityInspectorWindow(EntityInspectorWindowState& state,
                                                              const EntityInspectorView& view,
                                                              const SkinArt& art);

}  // namespace ro::ui
