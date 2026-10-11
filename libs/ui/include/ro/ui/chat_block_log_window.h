#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LE JOURNAL DES BLOCAGES DE CHAT D'UN JOUEUR, POUR LE STAFF ──────────────
//
// Les `@mute` et `@unmute` qui l'ont visé, du plus récent au plus ancien, tels
// que le serveur les a relus dans son journal des commandes : date, auteur,
// commande, durée. Les cellules arrivent DÉJÀ ÉCRITES et traduites : la fenêtre
// ne fait que les ranger.

inline constexpr const char* kChatBlockLogWindowId = "journal_blocages_chat";

// Où en est la demande.
enum class ChatBlockLogPhase : std::uint8_t {
  // Partie, sans réponse encore.
  kAsking,
  // Partie depuis trop longtemps : le serveur ne connaît peut-être pas le
  // paquet, ou n'a pas reconnu la poignée de main.
  kSilent,
  // Le serveur refuse : ce compte n'est pas du staff.
  kDenied,
  // Aucun joueur en ligne sous cet AID.
  kMissing,
  // Le serveur ne journalise pas les commandes en SQL : il n'a rien à relire.
  kLogOff,
  // La lecture du journal a échoué chez le serveur.
  kReadFailed,
  kReady,
};

struct ChatBlockLogRow {
  // La date, à l'heure de ce poste.
  std::string when;
  std::string author;
  // `@mute` ou `@unmute`.
  std::string command;
  // Vide pour une commande qui ne porte pas de durée.
  std::string duration;
};

struct ChatBlockLogView {
  // Le joueur visé, DÉJÀ TRADUIT : le nom et l'AID.
  std::string subject;
  ChatBlockLogPhase phase = ChatBlockLogPhase::kAsking;
  std::vector<ChatBlockLogRow> rows;
};

struct ChatBlockLogWindowState {
  bool open = false;
  // Mise au premier plan à la prochaine image : un nouveau joueur s'y montre.
  bool raise = false;
};

struct ChatBlockLogAnswer {
  // Redemander au serveur.
  bool refresh = false;
};

[[nodiscard]] ChatBlockLogAnswer DrawChatBlockLogWindow(ChatBlockLogWindowState& state,
                                                        const ChatBlockLogView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
