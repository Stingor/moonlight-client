#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::sim {

// ── QUI NE PEUT PLUS NOUS CHUCHOTER ─────────────────────────────────────────
//
// Le serveur tient, pour le personnage joué, une liste de noms dont il jette
// les chuchotements, et un interrupteur qui les jette tous. Cette classe en
// tient le reflet, pour qu'un geste sache dans quel sens aller : « Bloquer »
// ou « Autoriser ».
//
// 🔴 LA LISTE VIT LE TEMPS DE LA CONNEXION. Le serveur ne l'enregistre nulle
// part : elle est vide à l'entrée en jeu, survit aux changements de carte sur
// le même serveur de zone, et se perd avec la déconnexion ou le passage à un
// autre serveur de zone. Il ne l'envoie jamais de lui-même : on ne la reçoit
// que sur demande.
//
// 🔴 ELLE NE FILTRE QUE LES CHUCHOTEMENTS. Le chat public, le groupe et la
// guilde passent.
//
// ⚠ LES NOMS SE COMPARENT OCTET POUR OCTET, comme chez le serveur : la casse
// compte, « bob » n'est pas « Bob ».

// Le sens d'une demande, et de la réponse qui la suit.
enum class WhisperBlockWay : std::uint8_t {
  kBlock,
  kAllow,
};

// CE QUE LE SERVEUR RÉPOND À UNE DEMANDE PAR NOM, tel qu'il le code.
enum class WhisperBlockResult : std::uint8_t {
  kDone = 0,
  // Bloquer : le nom est celui que le serveur réserve à ses propres messages.
  // Autoriser : le nom n'était pas dans la liste.
  kFailed = 1,
  // Bloquer : la liste est pleine.
  kListFull = 2,
};

// LA RÉPONSE À UNE DEMANDE PAR NOM, avec le nom qu'elle concernait.
//
// 🔴 LE SERVEUR NE LE RÉPÈTE PAS : sa réponse ne porte que le sens et le code.
// Le nom vient de la demande gardée à l'envoi, appariée dans l'ordre — le
// serveur répond à chacune sur-le-champ, dans l'ordre d'arrivée. Vide quand
// aucune demande n'attendait, ou qu'elle attendait l'autre sens.
struct WhisperBlockAnswer {
  std::string name;
  WhisperBlockWay way = WhisperBlockWay::kBlock;
  // Le code tel que la trame le porte : un serveur plus récent peut en
  // envoyer d'autres, qui ne changent rien à la liste.
  std::uint8_t result = 0;
};

// CE QU'UNE RÉPONSE DU SERVEUR A CHANGÉ À LA LISTE, pour qui la garde ailleurs.
//
// ⚠ UN BLOCAGE RÉUSSI SE NOTE MÊME SI LE NOM Y ÉTAIT DÉJÀ : le serveur dit
// « réussi » dans les deux cas, et la liste gardée ailleurs peut ne pas l'avoir.
enum class WhisperBlockChangeKind : std::uint8_t {
  kAdded,
  kRemoved,
  // « Tout autoriser » a vidé la liste.
  kEmptied,
};

struct WhisperBlockChange {
  WhisperBlockChangeKind kind = WhisperBlockChangeKind::kAdded;
  // Vide pour `kEmptied`.
  std::string name;
};

// Combien de demandes peuvent attendre leur réponse à la fois. Bien au-delà
// de ce qu'une main clique : la borne n'empêche qu'un serveur muet de faire
// grandir la file sans fin.
inline constexpr std::size_t kMostPendingWhisperBlocks = 32;

class WhisperBlockList {
 public:
  // Ce nom est-il dans la liste ?
  [[nodiscard]] bool Blocks(std::string_view name) const;
  // Tout le monde est-il bloqué, liste ou non ?
  [[nodiscard]] bool blocks_everyone() const noexcept { return everyone_; }
  // Les noms, dans l'ordre où le serveur les tient.
  [[nodiscard]] std::span<const std::string> names() const noexcept { return names_; }

  // UNE DEMANDE PAR NOM EST PARTIE : sa réponse la retrouvera.
  void Expect(std::string name, WhisperBlockWay way);
  [[nodiscard]] std::size_t pending() const noexcept { return pending_.size(); }

  // LA RÉPONSE À LA PLUS ANCIENNE DEMANDE, et ce qu'elle change.
  //
  // Bloquer réussi : le nom entre — le serveur dit aussi « réussi » pour un
  // nom déjà présent. Autoriser réussi : il sort. Autoriser refusé : le
  // serveur ne l'avait pas, il sort aussi. Un refus de blocage ne change rien.
  //
  // ⚠ UNE RÉPONSE QUI NE S'APPARIE PAS ne change rien non plus, et oublie les
  // demandes en attente : l'ordre est perdu, la suivante se tromperait de nom.
  WhisperBlockAnswer Settle(WhisperBlockWay way, std::uint8_t result);

  // LA RÉPONSE À « TOUT BLOQUER » OU « TOUT AUTORISER ».
  //
  // Tout bloquer allume l'interrupteur ; un refus dit qu'il l'était déjà.
  // Tout autoriser éteint l'interrupteur s'il était allumé, et ne touche pas
  // la liste ; sinon il VIDE la liste. Un refus dit qu'il n'y avait ni l'un ni
  // l'autre.
  void SettleEveryone(WhisperBlockWay way, bool failed);

  // La liste que le serveur vient de redire. Elle ne dit rien de
  // l'interrupteur.
  void Replace(std::vector<std::string> names);

  // Une partie commence, ou un autre serveur de zone prend la main : il ne
  // tient rien pour nous. ⚠ CE N'EST PAS UN CHANGEMENT : rien ne se note.
  void Clear() noexcept;

  // CE QUE LES RÉPONSES ONT CHANGÉ depuis la dernière fois, dans l'ordre.
  // ⚠ SE PREND ET S'EFFACE. La liste que le serveur redit (`Replace`) ne s'y
  // note pas : elle constate, elle ne change rien.
  [[nodiscard]] std::vector<WhisperBlockChange> TakeChanges();

 private:
  struct Pending {
    std::string name;
    WhisperBlockWay way = WhisperBlockWay::kBlock;
  };

  void Add(const std::string& name);
  void Remove(std::string_view name);

  std::vector<std::string> names_;
  std::deque<Pending> pending_;
  std::vector<WhisperBlockChange> changes_;
  bool everyone_ = false;
};

}  // namespace ro::sim
