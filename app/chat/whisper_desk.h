#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/chat_line.h"
#include "ro/sim/whisper.h"

namespace moonclient {

// LES CONVERSATIONS 1:1 : lesquelles sont ouvertes, ce qu'elles ont pris au
// fil, et ce qu'elles n'ont pas encore montré.
//
// 🔴 UNE CONVERSATION OUVERTE PREND LA LIGNE, et le fil ne l'écrit pas : c'est
// le contrat du client d'origine, dont la fenêtre 1:1 « consomme » ce qu'elle
// montre. Elle la prend AU MOMENT OÙ LA LIGNE ARRIVE — refermer la fenêtre ne
// rend pas au fil ce qu'elle avait pris, et l'ouvrir après coup n'en retire
// rien. Le journal sur le disque, lui, garde tout.
//
// ⚠ UNE CONVERSATION N'A PAS DE LIGNES À ELLE : elle montre, dans le tampon
// partagé du monde, celles qui nomment son correspondant. C'est ce qui lui fait
// reprendre d'emblée ce qui s'est déjà dit avec lui pendant la partie.

// LE RÉGLAGE DES AMIS DU CLIENT : ses trois cases. Toutes décochées au départ,
// comme chez lui — par défaut, aucune conversation ne s'ouvre seule.
struct FriendSetup {
  bool open_from_friend = false;
  bool open_from_stranger = false;
  bool alarm = false;
};

// UNE LIGNE QUI ARRIVE OUVRE-T-ELLE UNE CONVERSATION ? La case des amis pour un
// ami, celle des inconnus pour tous les autres — un membre de la guilde
// compris : le client ne regarde que sa liste d'amis.
[[nodiscard]] bool OpensConversation(const FriendSetup& setup, ro::sim::WhisperRelation relation);

// Combien de conversations à la fois. ⚠ CE N'EST PAS UNE BORNE DU CLIENT
// D'ORIGINE, qui n'en a pas : au-delà, les fenêtres se recouvrent sans que le
// joueur puisse rien y faire.
//
// 🔴 AU PLAFOND, LA MOINS ACTIVE CÈDE — SI ELLE PEUT. Une conversation non lue,
// ou dont la saisie porte une phrase, ne cède jamais : elle emporterait ce que
// le joueur n'a pas vu, ou ce qu'il écrivait. Céder vaut fermer — ce qu'elle
// avait pris au fil n'y revient pas, comme à la croix —, et c'est juste parce
// qu'elle a été LUE. Si toutes sont protégées, la nouvelle ne s'ouvre pas, et
// la ligne qui l'aurait ouverte reste au fil.
inline constexpr std::size_t kMostConversations = 8;

struct Conversation {
  std::string peer;
  // LE RANG DE SA DERNIÈRE ACTIVITÉ — ouverte, ou une ligne échangée. C'est lui
  // qui désigne celle qui cède quand le plafond est atteint. ⚠ UN COMPTE ET NON
  // UNE HEURE : seul l'ordre compte, et un geste n'a pas toujours l'horloge
  // sous la main.
  std::uint64_t active_rank = 0;
  // Elle a reçu sans être regardée.
  bool unread = false;
  // Sa saisie porte une phrase, que la fenêtre dit à chaque image.
  bool drafting = false;
  // Demandé par un geste : passer devant, et donner le clavier à sa saisie.
  // ⚠ JAMAIS POSÉ PAR UNE LIGNE QUI ARRIVE : on écrit peut-être déjà ailleurs,
  // et une fenêtre qui vole le clavier au milieu d'une phrase la coupe en deux.
  bool raise = false;
};

class WhisperDesk {
 public:
  // LE GESTE « CHUCHOTER DANS UNE FENÊTRE » : ouvre la conversation, ou ramène
  // celle qui l'est, et lui donne le clavier. Nul quand le plafond est atteint
  // et qu'aucune ne peut céder.
  Conversation* Open(std::string_view peer);
  void Close(std::string_view peer);

  [[nodiscard]] Conversation* Find(std::string_view peer);
  [[nodiscard]] const Conversation* Find(std::string_view peer) const;
  [[nodiscard]] std::span<const Conversation> conversations() const noexcept {
    return conversations_;
  }

  // CE QU'UNE IMAGE A FAIT DES LIGNES NEUVES.
  struct Absorbed {
    // Combien de conversations une ligne a fait NAÎTRE : c'est à leur
    // naissance, et seulement là, que le client fait sonner son alarme.
    int opened = 0;
  };

  // Lit les lignes arrivées depuis l'appel précédent : ouvre ce que le réglage
  // demande, prend ce qu'une conversation montre, et marque le non-lu.
  // `own_name` écarte ce qu'on se chuchote à soi-même.
  Absorbed Absorb(std::span<const ro::sim::ChatLine> chat, const FriendSetup& setup,
                  std::string_view own_name);

  // Une conversation l'a-t-elle prise ? Le fil ne la montre pas.
  [[nodiscard]] bool ShownApart(std::uint64_t seq) const;

  // Le joueur la regarde : son non-lu s'efface.
  void Seen(std::string_view peer);
  // Ce que porte sa saisie, dit par la fenêtre.
  void NoteDraft(std::string_view peer, bool drafting);

  // Une partie commence : rien n'est ouvert, rien n'a été pris. `last_seq` est
  // le rang de la dernière ligne déjà dite : ce qui précède n'ouvre plus rien.
  void Clear(std::uint64_t last_seq);

 private:
  // Ouvre une conversation neuve, en faisant céder au plafond la moins active
  // de celles qui le peuvent. Nul si aucune ne le peut.
  Conversation* Admit(std::string_view peer);
  // Le rang d'une activité neuve.
  std::uint64_t Stir() noexcept { return ++activity_; }

  std::vector<Conversation> conversations_;
  // LES RANGS DES LIGNES PRISES, croissants : elles arrivent dans l'ordre.
  std::vector<std::uint64_t> apart_;
  std::uint64_t absorbed_through_ = 0;
  std::uint64_t activity_ = 0;
};

}  // namespace moonclient
