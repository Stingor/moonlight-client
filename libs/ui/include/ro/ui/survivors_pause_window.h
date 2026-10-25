#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA PAUSE D'UNE PARTIE : Reprendre, Abandonner (confirmé sur place), la rangée
// des fenêtres qu'on ouvre par-dessus — « Options », celles du jeu —, et les
// réglages du mode — « Effets réduits » (U20), « Afficher les mesures » (U24,
// offert au seul client de développement), « Ombres du soleil de la horde »
// (U27). En tête, ce qui a mis en pause quand ce n'est pas le joueur ; en
// pied, les mesures quand elles sont demandées.
//
// Les cases montrent les réglages de la vue : un clic les rend dans
// l'intention, `app` les applique et les range dans `savedata\survivors.yml`.

enum class SurvivorsPauseReason : std::uint8_t { kPlayer, kFocusLost, kWorldAlert };

// Une mesure d'une part de l'image, sur la fenêtre glissante.
struct SurvivorsMeasureRow {
  const char* label = "";  // ⚠ EN FRANÇAIS, marqué par `TrText` : traduit ici
  double p50_ms = 0.0;
  double p95_ms = 0.0;
  double max_ms = 0.0;
};
// Un compte de l'image : monstres, quads, lots, effets, voix.
struct SurvivorsCountRow {
  const char* label = "";  // ⚠ EN FRANÇAIS, marqué par `TrText` : traduit ici
  long long value = 0;
};

struct SurvivorsPauseView {
  SurvivorsPauseReason reason = SurvivorsPauseReason::kPlayer;
  // Le bandeau d'une alerte du monde réel. ⚠ EN FRANÇAIS, marqué par `TrText`.
  const char* alert_banner = nullptr;
  bool accepts_input = false;
  bool reduced_effects = false;
  // La case « Afficher les mesures » paraît : un outil de développement.
  bool measures_offered = false;
  bool show_measures = false;
  bool horde_sun_shadows = false;
  std::span<const SurvivorsMeasureRow> measures;
  std::span<const SurvivorsCountRow> counts;
};

// La confirmation de l'abandon tient d'une image à l'autre.
struct SurvivorsPauseState {
  bool confirming_abandon = false;
};

struct SurvivorsPauseAsk {
  bool resume = false;
  bool abandon = false;  // confirmé
  // « Options » : les options du jeu s'ouvrent par-dessus la pause.
  bool open_options = false;
  std::optional<bool> reduced_effects;
  std::optional<bool> show_measures;
  std::optional<bool> horde_sun_shadows;
};

[[nodiscard]] SurvivorsPauseAsk DrawSurvivorsPauseWindow(SurvivorsPauseState& state,
                                                         const SurvivorsPauseView& view,
                                                         const SkinArt& art);

}  // namespace ro::ui
