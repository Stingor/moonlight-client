#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA PETITE FENÊTRE DES PRÉRÉGLAGES DE CARACTÉRISTIQUES, ouverte depuis la
// fenêtre de statut : la liste du compte, l'enregistrement des valeurs
// actuelles, et l'aperçu qui précède toute application.
//
// ⚠ ELLE NE POSSÈDE AUCUN ÉTAT DE JEU : la liste, l'aperçu et le compte rendu
// lui arrivent composés, et elle rend ce que le joueur demande. C'est `app` qui
// range, planifie et envoie.

inline constexpr const char* kStatPresetWindowTitle = core::TrText("Préréglages de stats");
inline constexpr const char* kStatPresetWindowId = "stat_presets";

// Force, agilité, vitalité, intelligence, dextérité, chance.
inline constexpr std::size_t kStatPresetStatCount = 6;
// Le nom qu'on tape, terminateur compris.
inline constexpr std::size_t kStatPresetNameInputBytes = 32;

using StatPresetValues = std::array<std::int64_t, kStatPresetStatCount>;

struct StatPresetRowView {
  std::string_view name;
  StatPresetValues stats = {};
};

enum class StatPresetRowVerdict : std::uint8_t { kReached, kAboveTarget, kShort, kCeiling };

struct StatPresetPreviewRow {
  std::int64_t current = 0;
  std::int64_t after = 0;
  std::int64_t target = 0;
  std::int64_t missing_steps = 0;
  std::int64_t missing_points = 0;
  StatPresetRowVerdict verdict = StatPresetRowVerdict::kReached;
};

struct StatPresetPreviewView {
  // Faux : aucun aperçu à montrer.
  bool shown = false;
  std::string_view name;
  std::array<StatPresetPreviewRow, kStatPresetStatCount> rows = {};
  std::int64_t points_before = 0;
  std::int64_t points_after = 0;
  std::int64_t raises = 0;
};

// CE QUE LA FENÊTRE SAIT, posé par `app` à chaque image. ⚠ LES CHAÎNES VIVENT
// AUSSI LONGTEMPS QUE LA VUE : c'est l'appelant qui les garde.
struct StatPresetWindowView {
  std::span<const StatPresetRowView> presets;
  std::size_t capacity = 0;
  // Les valeurs de base actuelles, et si le serveur les a toutes dites.
  StatPresetValues current = {};
  bool stats_known = false;
  StatPresetPreviewView preview;
  // Une application en cours, et où elle en est.
  bool running = false;
  std::size_t done = 0;
  std::size_t total = 0;
  // Le compte rendu du dernier geste, déjà traduit, ou vide.
  std::string_view report;
};

struct StatPresetWindowState {
  bool open = false;
  std::array<char, kStatPresetNameInputBytes> name = {};
  // La ligne qu'on renomme, et la saisie ; moins de zéro si aucune.
  int renaming = -1;
  std::array<char, kStatPresetNameInputBytes> rename = {};
  // La ligne dont on confirme la suppression ; moins de zéro si aucune.
  int removing = -1;
};

enum class StatPresetVerb : std::uint8_t {
  kNothing,
  // Enregistrer les valeurs actuelles sous `name`.
  kSave,
  // Renommer la ligne `index` en `name`.
  kRename,
  kRemove,
  // Montrer l'aperçu de la ligne `index`.
  kPreview,
  // Lancer l'application de l'aperçu montré.
  kConfirm,
  // Refermer l'aperçu sans rien envoyer.
  kCancelPreview,
  // Arrêter l'application en cours.
  kStop,
};

struct StatPresetAction {
  StatPresetVerb verb = StatPresetVerb::kNothing;
  std::size_t index = 0;
  std::string name;
};

// Le bouton de la barre de titre de la fenêtre de statut qui ouvre celle-ci.
// À poser avant son `BeginSkinnedWindow`.
void PoseStatPresetButton();

[[nodiscard]] StatPresetAction DrawStatPresetWindow(StatPresetWindowState& state,
                                                    const StatPresetWindowView& view,
                                                    const SkinArt& art);

}  // namespace ro::ui
