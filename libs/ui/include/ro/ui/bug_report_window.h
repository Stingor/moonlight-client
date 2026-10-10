#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LE RAPPORT DE BUG : la boîte de saisie, et l'accusé du serveur ─────────
//
// Voir l'ADR 0052. Une fenêtre qui veut son bouton le pose dans sa barre de
// titre (`PoseBugReportButton`) et, cliqué, le dit dans sa réponse — c'est
// `app` qui compose le contexte : `ui` ne saurait nommer ni un item, ni un PNJ.

// Le message, en octets UTF-8. ⚠ CINQ CENTS : la colonne du serveur, et le
// message part en CP1252, où un caractère ne pèse jamais plus d'un octet.
inline constexpr std::size_t kBugReportMessageBytes = 500;

// L'accusé : enregistré, trop rapide, refusé. Moins un : rien à montrer.
inline constexpr int kBugReportSaved = 0;
inline constexpr int kBugReportTooSoon = 1;

struct BugReportWindowState {
  // UNE DEMANDE D'OUVERTURE, posée par `app` avec le contexte. ⚠ UNE BOÎTE
  // OUVERTE NE SE REMPLACE PAS : elle jetterait le message en cours de frappe.
  bool asked = false;
  bool open = false;
  // Ce que le joueur lit du contexte, déjà traduit ; et ce qui part, que `ui`
  // ne lit pas.
  std::string context_label;
  std::string context_json;
  std::uint8_t category = 0;
  std::array<char, kBugReportMessageBytes + 1> message{};

  // L'instant du dernier envoi, sur l'horloge d'ImGui, pour griser « Envoyer »
  // trois secondes ; négatif avant le premier.
  double sent_at_s = -1.0;

  // L'ACCUSÉ, posé par `app` à la réception : `ack_fresh` dit qu'il vient
  // d'arriver, et la fenêtre en date alors l'affichage.
  int ack_status = -1;
  bool ack_fresh = false;
  double ack_at_s = 0.0;

  // Posé par `app` depuis les réglages : faux, aucune fenêtre ne pose le bouton.
  bool buttons = true;
};

// Ce que la boîte rend : un message à envoyer, avec le contexte de l'état.
struct BugReportAction {
  bool send = false;
  std::string message;
};

// La boîte, si une demande ou une saisie est en cours, et l'accusé.
[[nodiscard]] BugReportAction DrawBugReportWindow(BugReportWindowState& state, const SkinArt& art);

// LE BOUTON DE LA BARRE DE TITRE, pour la prochaine fenêtre habillée : à poser
// AVANT son `BeginSkinnedWindow`, et à relire par `TitleActionClicked(rang)`,
// le rang étant l'ordre de pose. Rien n'est posé quand `shown` est faux.
void PoseBugReportButton(bool shown);

}  // namespace ro::ui
