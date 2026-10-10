#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES DIX MACROS DE CHAT : une ligne toute prête sous une touche.
//
// C'est la fenêtre « Shortcut List » du client — celle d'Alt+M —, dont le nom
// de classe ment sur toute la ligne : elle n'affiche aucune émote malgré son
// `UIEmotionWnd`, et le fichier qu'elle écrit s'appelle `EmotionHotkey`. Cinq
// vocabulaires pour un seul objet ; on l'appelle ici ce qu'elle est.
//
// ⚠ DIX, ET CE N'EST PAS UN CHIFFRE ROND CHOISI AU HASARD : c'est la taille du
// vecteur du client, celle que ses dix comportements de raccourci indexent, et
// celle que le joueur retrouvera s'il repasse au client d'origine.

inline constexpr std::size_t kChatMacroCount = 10;

// Ce qu'une macro peut porter, en octets.
//
// 🔴 CINQUANTE, COMME LE CHAMP DE SAISIE DU CLIENT. Le client sait
// techniquement en envoyer plus — son tampon d'envoi fait 256 octets —, mais
// son champ natif tronque à cinquante SANS RIEN DIRE. Accepter davantage ici
// serait un cadeau empoisonné : la macro reviendrait amputée au premier
// aller-retour, et rien n'expliquerait où sa fin est passée.
//
// ⚠ ET LA LIMITE SE VOIT ARRIVER : c'est tout l'objet de la colonne de compte.
inline constexpr std::size_t kChatMacroBytes = 50;
inline constexpr std::size_t kChatMacroSize = kChatMacroBytes + 1;

// Ce que la fenêtre garde d'une image à l'autre.
//
// ⚠ LES TAMPONS SONT LA SOURCE PENDANT LA PARTIE, et le fichier ne les relit
// qu'au démarrage : `app` les descend une fois, puis les remonte quand ils
// changent. Les redescendre à chaque image écraserait la frappe en cours.
struct ChatMacroState {
  bool open = false;
  std::array<std::array<char, kChatMacroSize>, kChatMacroCount> rows = {};
};

// UNE COMMANDE PROPOSÉE PAR LE MENU CONTEXTUEL, et son effet en clair.
//
// 🔴 `ui` NE CONNAÎT AUCUNE COMMANDE DE SERVEUR, et ne doit pas en apprendre :
// elles dépendent du serveur, de ses surcharges et des permissions du joueur.
// `app` les recopie de la configuration du serveur et les passe DÉJÀ TRADUITES
// — les traduire ici demanderait un `Tr` sur autre chose qu'un littéral.
struct ChatMacroCommand {
  std::string_view text;
  std::string_view help;
};

// Un sous-menu du clic droit : son titre, et ce qu'il propose.
struct ChatMacroCommandGroup {
  std::string_view label;
  std::span<const ChatMacroCommand> items;
};

// CE QUE LA FENÊTRE DEMANDE. ⚠ DES INTENTIONS, RIEN D'APPLIQUÉ : `ui` ne sait
// pas envoyer une ligne, ne connaît aucune table de touches et n'écrit aucun
// fichier.
struct ChatMacroAsk {
  // Le rang de la macro à envoyer TOUT DE SUITE, ou moins de zéro.
  //
  // ⚠ UNE SEULE PAR IMAGE, et c'est suffisant : le joueur n'a qu'une souris.
  int send = -1;
  // Il a demandé l'écran des raccourcis, pour réaffecter ces dix touches-là.
  bool open_hotkeys = false;
  // Il a confirmé le retour aux valeurs d'usine. ⚠ APRÈS LA MODALE, jamais au
  // clic : le bouton ouvre la question, c'est la réponse qui arrive ici.
  bool reset = false;
  // Une ligne a été écrite, échangée, préremplie ou vidée : le fichier doit
  // suivre.
  //
  // 🔴 UN DRAPEAU ET NON UN RELEVÉ, ici seulement, parce que la comparaison
  // coûterait dix chaînes par image alors que le widget rend déjà « j'ai
  // changé ». C'est l'inverse du choix fait pour le sol uni, et la raison est
  // la taille de ce qu'il faudrait comparer.
  bool changed = false;
};

// ⚠ LE TITRE PORTE TROIS DIÈSES : ce qui suit est l'IDENTITÉ, que la langue ne
// change pas. Sans cela, traduire l'interface rouvrirait la fenêtre ailleurs.
//
// 🔴 « MACROS DE CHAT », ET NON LE TITRE DU CLIENT. Celui-là, « Shortcut
// List », ne dit ni ce que c'est ni à quoi ça sert, et traduit il entrerait en collision
// avec notre propre « Configuration des raccourcis », qui est la ligne d'à côté
// dans le menu d'Échap.
//
// ⚠ ET « MACROS » SEUL NE SUFFIT PAS : le mot désigne aussi l'anti-bot du client
// (`UIMacroRegisterWnd`), qui n'a aucun rapport. C'est le choix de
// l'implémentation de référence, pour cette raison-là.
inline constexpr const char* kChatMacroTitle = core::TrText("Macros de chat");
inline constexpr const char* kChatMacroId = "chat_macros";

// L'identité de la question des valeurs d'usine. ⚠ UNE SEULE CHAÎNE POUR
// L'OUVERTURE ET POUR LE DESSIN : ImGui apparie ses boîtes par identifiant, et
// deux libellés différents ne se retrouvent pas — le bouton ne ferait alors
// simplement rien, sans un mot.
inline constexpr const char* kChatMacroResetId = "macros_valeurs_usine";

// Dessine la fenêtre.
//
// `keys` sont les dix libellés de touche — « Alt+1 » —, composés par `app` :
// cette couche ne sait pas nommer un code de touche, et la table des raccourcis
// est la seule source de ce qu'ils valent.
//
// `target` est l'endroit où ces macros partiront, DÉJÀ TRADUIT. C'est le
// renseignement que la fenêtre du client ne donne nulle part : la même macro
// part alentour, au groupe ou à la guilde selon ce que la barre a armé.
//
// `commands` et `emotions` remplissent le menu du clic droit. ⚠ LES ÉMOTES
// PROPOSÉES SONT CELLES QUI ONT UNE COMMANDE, et `app` l'a déjà tranché : celles
// qui n'en ont pas n'ont rien à faire dans une macro.
void DrawChatMacros(ChatMacroState& state, std::span<const std::string> keys,
                    std::string_view target, std::span<const ChatMacroCommandGroup> commands,
                    std::span<const ChatEmotion> emotions, const SkinArt& art, ChatMacroAsk* asked);

}  // namespace ro::ui
