#pragma once

#include <deque>
#include <string>
#include <vector>

#include "ro/net/chat_target.h"
#include "ro/ui/cast_bars.h"
#include "ro/ui/chat_bubble.h"
#include "ro/ui/chat_input.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/item_obtain_toast.h"

#include "chat/chat_item_link.h"

namespace moonclient {

// LES TAMPONS QUE L'INTERFACE DÉSIGNE : ce que la partie compose pour ses vues,
// qui n'en gardent qu'une adresse — l'armement de la saisie, les émoticônes, les
// bulles et les barres projetées, les liens en attente, l'horloge de la veille.
//
// 🔴 C'EST POURQUOI ILS VIVENT DANS UNE BASE DE `Game` : construite avant tout
// membre de la partie et détruite après, elle survit à toute vue qui pointe
// dedans.
//
// ⚠ AUCUNE MÉTHODE : la partie les lit comme les siens, sous le même nom.
class GameUiBuffers {
 protected:
  // ── LA FENÊTRE ET L'INTERFACE ──────────────────────────────────────────────
  // ⚠ AVANT L'INTERFACE : voir l'ordre de destruction, en tête de `Game`.
  // CE QUE LA BARRE DE SAISIE A ARMÉ : le mode du sélecteur, et le destinataire
  // de la box « Pseudo ».
  //
  // ⚠ REFAIT À CHAQUE IMAGE, et sa vue regarde le tampon de la barre — dont
  // l'adresse ne bouge pas de la partie, mais dont le CONTENU change à chaque
  // frappe. Le composer une fois pour toutes figerait le destinataire au nom
  // qu'il portait au démarrage.
  //
  // ⚠ IL NE SURVIT PAS À LA PARTIE, comme dans le client : c'est un état de
  // session, pas un réglage. Retrouver sa guilde armée après une reconnexion
  // ferait partir la première phrase ailleurs qu'on ne l'attend.
  ro::net::ChatArming armed_chat_;
  // LE NOM DU CANAL ARMÉ, et il vit ICI parce que `ChatArming` n'en porte
  // qu'une VUE. La liste que le sélecteur propose est refaite à chaque image ;
  // y laisser pointer l'armement le ferait pendre dès l'image suivante.
  std::string armed_channel_;
  // LES ÉMOTICÔNES PROPOSÉES, composées UNE FOIS à l'entrée en jeu, en trois
  // tableaux qui se pointent l'un l'autre : les couches bout à bout, les images
  // qui les découpent, les émoticones qui découpent les images. ⚠ PAS À CHAQUE
  // IMAGE : la grille les montre TOUTES dès qu'elle s'ouvre, et les redemander
  // soixante fois par seconde coûterait une centaine de recherches.
  //
  // 🔴 AUCUN NE DOIT PLUS BOUGER UNE FOIS REMPLI : les vues pointent dedans, et
  // une réallocation les laisserait toutes pendantes. C'est pourquoi la place se
  // réserve avant de remplir.
  std::vector<ro::ui::ChatEmotionLayer> emotion_layers_;
  std::vector<ro::ui::ChatEmotionFrame> emotion_frames_;
  std::vector<ro::ui::ChatEmotion> emotions_;
  // Les groupes d'emoji, intertitres TRADUITS. ⚠ VIDE SANS POLICE D'EMOJI :
  // l'onglet ne s'ouvre alors pas du tout, plutôt que sur des losanges.
  std::vector<ro::ui::ChatEmojiGroup> emojis_;
  // Les bulles de parole projetées à l'écran, récoltées comme les barres.
  std::vector<ro::ui::ChatBubbleView> chat_bubbles_;
  // LA POIGNÉE DU BANDEAU D'OBTENTION : sa largeur, relue d'une image sur la
  // suivante pour la centrer, et l'ordre de la replacer quand un réglage
  // vient de la déplacer sous elle.
  ro::ui::ItemObtainAnchorGrip toast_grip_;
  // LES PHRASES DE BULLE DONT UNE BALISE A ETE TRADUITE. Voir chat_view.h : les
  // vues ci-dessus pointent dedans, et une file ne les deplace jamais.
  std::deque<DecoratedChatLine> bubble_texts_;
  // LES BARRES DES ENTITÉS — vie, magie et incantation —, projetées à l'écran :
  // récoltées par les calques, peintes par l'étage d'interface, comme les bulles.
  std::vector<ro::ui::EntityBarView> entity_bars_;
  std::vector<ro::ui::CastBarView> cast_bars_;

  // LES LIENS POSÉS DANS LA SAISIE ET PAS ENCORE PARTIS. ⚠ LE JOUEUR VOIT LE
  // NOM, LA LIGNE PORTE LA BALISE : voir `chat_item_link.h`.
  std::vector<PendingChatLink> pending_links_;
  // CE QUE LA RANGÉE DE SAISIE DOIT PEINDRE, refait à chaque image en regard de
  // `pending_links_` : les libellés pointent dedans, et le jeton est le rang.
  std::vector<ro::ui::ChatInputLink> input_links_;

  // Les deux lignes de l'horloge. ⚠ DANS DES TAMPONS QUI SURVIVENT AU DESSIN :
  // la vue n'en garde qu'un pointeur.
  std::string afk_clock_text_;
  std::string afk_away_text_;
};

}  // namespace moonclient
