#pragma once

#include <cstdint>
#include <string>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── AVANT D'OUVRIR UNE ADRESSE, ET AVANT D'ADMETTRE UN SITE ─────────────────
//
// 🔴 UN LIEN DU CHAT VIENT D'UN AUTRE JOUEUR : il ne doit pas lancer le
// navigateur d'un simple clic. La confirmation montre l'adresse entière, l'hôte
// en évidence, et ce que l'adresse a de suspect.
//
// `ui` n'ouvre rien : elle rend la réponse, et `app` lance le navigateur.

enum class ChatUrlAnswer : std::uint8_t { kNone, kOpen, kCancel, kCopy };

struct ChatUrlPrompt {
  // L'adresse telle qu'elle était écrite, puis telle qu'elle s'ouvrira.
  std::string written;
  std::string openable;
  std::string host;
  // Les drapeaux de `ro::core::WebAddressWarning`.
  std::uint8_t warnings = 0;
  // L'adresse ne se lit pas : on ne sait pas quel site elle contactera. Elle
  // se dit sur une ligne d'avertissement, et n'offre jamais « Toujours ».
  bool unreadable = false;
  // « Toujours ouvrir les liens de ce site » n'est offert qu'en https, sans
  // aucun avertissement.
  bool offer_always = false;
  // La confirmation est demandée ; elle s'ouvre à l'image suivante.
  bool asked = false;
  bool always = false;
  ChatUrlAnswer answer = ChatUrlAnswer::kNone;
};

// « Autoriser ce site ? » : ses images se chargeront au simple survol.
struct ChatHostPrompt {
  std::string host;
  bool asked = false;
  bool granted = false;
};

void DrawChatUrlPrompt(ChatUrlPrompt& prompt, const SkinArt& art);
void DrawChatHostPrompt(ChatHostPrompt& prompt, const SkinArt& art);

}  // namespace ro::ui
