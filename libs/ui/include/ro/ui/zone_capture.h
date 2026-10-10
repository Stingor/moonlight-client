#pragma once

#include <algorithm>
#include <cstdint>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── L'ENREGISTREUR DE ZONE ──────────────────────────────────────────────────
//
// L'outil de capture du système, en jeu : on trace un rectangle à la souris une
// fois, il est mémorisé, et une touche enregistre ce qui s'y passe pendant
// quelques secondes. Ce qui en sort part dans le PRESSE-PAPIER — voir
// `capture_clipboard.h`, qui dit pourquoi la livraison n'est pas un fichier.
//
// 🔴 LA CAPTURE CONTIENT NOTRE INTERFACE, et c'est tout l'intérêt : montrer une
// manipulation, c'est montrer les fenêtres dans lesquelles elle se fait. La
// conséquence est immédiate et commande tout le dessin de ce module : ce qu'il
// trace pendant un enregistrement ATTERRIRAIT DANS LES IMAGES. Rien n'est donc
// dessiné à l'intérieur de la zone — le cadre la borde par l'extérieur, et le
// témoin cherche une marge libre autour. Sans marge, on n'affiche RIEN plutôt
// que de salir la prise.

// Un rectangle en POINTS D'INTERFACE.
//
// ⚠ CE N'EST PAS L'UNITÉ DU TAMPON DE DESSIN. Sur un écran à facteur d'échelle,
// le système donne davantage de pixels que la fenêtre n'a de points ; la souris
// et ImGui parlent en points, la relecture d'image parle en pixels. La
// conversion a lieu à UN SEUL endroit, au moment de la prise.
struct ZoneRect {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;

  [[nodiscard]] friend bool operator==(const ZoneRect&, const ZoneRect&) = default;
  [[nodiscard]] bool empty() const noexcept { return width <= 0 || height <= 0; }
};

// ── LES BORNES, ET LEUR RAISON ──────────────────────────────────────────────

// En deçà, un tracé est un clic qui a glissé.
inline constexpr int kSmallestZone = 32;

inline constexpr int kSlowestCapture = 5;
inline constexpr int kFastestCapture = 20;
inline constexpr int kShortestCapture = 1;
inline constexpr int kLongestCapture = 15;
// La largeur de sortie. ⚠ C'EST LE RÉGLAGE QUI PÈSE LE PLUS LOURD : la mémoire
// et la taille du fichier montent avec le CARRÉ de la largeur.
inline constexpr int kNarrowestOutput = 160;
inline constexpr int kWidestOutput = 1280;
inline constexpr int kLongestDelay = 5;

// Le plafond de ce qu'on garde en mémoire le temps d'encoder.
//
// 🔴 CE N'EST PAS UNE PRÉCAUTION DE CONFORT. Les images sont réservées AVANT le
// départ : une allocation qui échouerait au milieu d'un enregistrement laisserait
// un clip tronqué, au mieux. Au-delà de ce budget, on refuse et on le dit tout
// de suite.
inline constexpr std::size_t kLargestCaptureBytes = 512U * 1024U * 1024U;

// Les réglages qui survivent au lancement.
struct ZoneCaptureSettings {
  ZoneRect zone;
  int frames_per_second = 10;
  int seconds = 6;
  int widest_output = 640;
  int start_delay_seconds = 3;

  [[nodiscard]] friend bool operator==(const ZoneCaptureSettings&,
                                       const ZoneCaptureSettings&) = default;
};

// Ramène dans ses bornes ce qu'un fichier retouché à la main a pu donner.
[[nodiscard]] constexpr ZoneCaptureSettings SaneCapture(ZoneCaptureSettings asked) noexcept {
  asked.frames_per_second = std::clamp(asked.frames_per_second, kSlowestCapture, kFastestCapture);
  asked.seconds = std::clamp(asked.seconds, kShortestCapture, kLongestCapture);
  asked.widest_output = std::clamp(asked.widest_output, kNarrowestOutput, kWidestOutput);
  asked.start_delay_seconds = std::clamp(asked.start_delay_seconds, 0, kLongestDelay);
  if (asked.zone.width < kSmallestZone || asked.zone.height < kSmallestZone) {
    asked.zone = ZoneRect{};
  }
  return asked;
}

// La taille de SORTIE : la zone, réduite à la largeur voulue en gardant ses
// proportions. Jamais agrandie — un clip flou plus lourd que l'original
// n'apporte rien.
[[nodiscard]] constexpr ZoneRect OutputSize(const ZoneRect& zone, int widest) noexcept {
  if (zone.empty() || widest <= 0 || zone.width <= widest) {
    return ZoneRect{.left = 0, .top = 0, .width = zone.width, .height = zone.height};
  }
  const int height = std::max(1, zone.height * widest / zone.width);
  return ZoneRect{.left = 0, .top = 0, .width = widest, .height = height};
}

// Ce que l'enregistrement occupera en mémoire, images brutes comprises.
[[nodiscard]] constexpr std::size_t CaptureBytes(const ZoneCaptureSettings& how) noexcept {
  const ZoneRect out = OutputSize(how.zone, how.widest_output);
  const auto frames = static_cast<std::size_t>(std::max(1, how.frames_per_second * how.seconds));
  return frames * static_cast<std::size_t>(std::max(0, out.width)) *
         static_cast<std::size_t>(std::max(0, out.height)) * 4U;
}

// ── CE QUE L'INTERFACE MONTRE ───────────────────────────────────────────────

enum class ZoneCapturePhase : std::uint8_t {
  kIdle,
  kSelecting,  // tracé à la souris en cours
  kCountdown,  // zone armée, le temps de se placer
  kRecording,
  kEncoding,
};

struct ZoneCaptureView {
  ZoneCapturePhase phase = ZoneCapturePhase::kIdle;
  ZoneRect zone;
  int seconds_left = 0;
  int frames_taken = 0;
  int frames_wanted = 0;
  // Le cadre de repérage. ⚠ À L'ARRÊT SEULEMENT : la prise a lieu après notre
  // dessin, ce cadre se retrouverait dans les images.
  bool show_zone = false;
  // Un mot flottant — « copié », « zone trop petite ». Déjà traduit, ou nul.
  const char* message = nullptr;
};

// Ce que le joueur a fait pendant le tracé.
struct ZoneCaptureAsk {
  bool drawn = false;
  ZoneRect zone;
  bool cancelled = false;
};

// Le rectangle en cours de glissement : d'où il part, et s'il est commencé.
//
// ⚠ À L'APPELANT, et non en statique : un statique survivait à une annulation
// décidée ailleurs, si bien que la sélection suivante repartait de l'ANCIENNE
// ancre, et son premier relâchement validait une zone jamais tracée.
struct ZoneTraceState {
  bool dragging = false;
  ImVec2 anchor;
};

// LE TRACÉ À LA SOURIS, par-dessus tout le reste.
//
// ⚠ IL PREND LA SOURIS ET LE CLAVIER tant qu'il dure : un clic destiné au
// rectangle ne doit pas partir aussi dans le monde. Hors de la phase de
// tracé, il oublie ce qui était commencé.
void DrawZoneSelection(const ZoneCaptureView& view, const SkinArt& art, ZoneTraceState& trace,
                       ZoneCaptureAsk* asked);

// LE TÉMOIN, et le cadre de repérage. Tous deux HORS de la zone.
void DrawCaptureBadge(const ZoneCaptureView& view, const SkinArt& art);

}  // namespace ro::ui
