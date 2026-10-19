#pragma once

#include <cstdint>

namespace ro::sim {

// LA BANQUE DE ZENY : ce que le serveur RÉPOND, et rien de ce qu'on en montre.
//
// Elle vit à part du monde pour la même raison que le dialogue d'un PNJ : c'est
// une famille à elle, et `world.h` touchait son plafond de lignes.
//
// 🔴 TROIS PLAFONDS, ET AUCUN NE VAUT POUR UN AUTRE :
//   · LA BANQUE tient au plus `SessionSettings::bank_zeny_max` — ce que le
//     serveur annonce dans ses règles, sur soixante-quatre bits ;
//   · LA POCHE tient au plus `kPocketZenyMax` (`MAX_ZENY` du serveur) ;
//   · UNE DEMANDE de dépôt ou de retrait porte son montant dans un `int32`.
// Le solde se lit sur soixante-quatre bits, comme le paquet. Un transfert qui
// ferait passer la destination au-delà de son plafond est refusé EN BLOC,
// jamais tronqué.
inline constexpr std::int64_t kPocketZenyMax = 2147483647;
inline constexpr std::int64_t kBankRequestMostZeny = 2147483647;

// CE QUE LE SERVEUR A RÉPONDU À LA DERNIÈRE DEMANDE DE BANQUE.
//
// ⚠ CE SONT LES VERDICTS DU CLIENT D'ORIGINE, pas les codes du fil. Les trois
// paquets numérotent leurs raisons CHACUN À SA FAÇON — la raison 2 veut dire
// « le système travaille » sur la demande d'ouverture, « pas assez de zeny » sur
// l'accusé de dépôt, et « erreur inconnue » sur celui de retrait. Les traduire
// ici, au décodage, est la seule façon de ne pas laisser un nombre nu traverser
// trois couches en changeant de sens en route.
//
// ⚠ ET DEUX CODES DU FIL TOMBENT DANS LE MÊME VERDICT : le client d'origine
// n'a pas de cas pour `BDA_ERROR` ni pour `BWA_UNKNOWN_ERROR`, qui échouent
// tous deux dans son défaut — l'erreur de système. Ce n'est pas un raccourci de
// notre part, c'est ce qu'il affiche.
enum class BankVerdict : std::uint8_t {
  // Rien à dire : la demande est passée. ⚠ C'EST AUSSI L'ÉTAT DE DÉPART, et les
  // deux se distinguent par le NUMÉRO du verdict, jamais par sa valeur.
  kDone,
  // La demande d'ouverture a échoué : le serveur travaille, ou il refuse.
  kSystemBusy,
  kSystemError,
  // Le dépôt : pas assez en poche, ou la banque déborderait.
  kDepositNoMoney,
  kDepositOverflow,
  // Le retrait : pas assez en banque.
  kWithdrawNoMoney,
  // Une fenêtre du client interdit le transfert. ⚠ AUCUN SERVEUR NE L'ENVOIE
  // AUJOURD'HUI — le code existe dans les deux accusés et rAthena ne l'emploie
  // pas —, et c'est précisément pour cela qu'on le porte : un verdict muet se
  // lirait « erreur inconnue » le jour où un fork s'en sert.
  kProhibited,
};

// LA BANQUE : UN COFFRE À ZENY, ET IL EST AU COMPTE.
//
// ⚠ AU COMPTE, PAS AU PERSONNAGE. Le serveur la range dans `#BANKVAULT`, une
// variable de compte : tous les personnages y puisent le même solde. Le
// remettre à zéro en changeant de personnage serait donc faux — c'est le
// serveur qui le redit, à chaque demande d'ouverture.
class BankSession {
 public:
  // ⚠ SUR SOIXANTE-QUATRE BITS, comme le paquet : voir `kPocketZenyMax`.
  [[nodiscard]] std::int64_t vault() const noexcept { return vault_; }
  // Le serveur vient de dire le solde. ⚠ LES TROIS PAQUETS DE BANQUE LE DISENT,
  // et ils sont seuls à le dire : celui de la demande d'ouverture et les deux
  // accusés de transfert. Un solde qu'on ne reprendrait pas sur un accusé
  // resterait celui d'avant le dépôt jusqu'à la prochaine ouverture.
  void TellVault(std::int64_t money) noexcept { vault_ = money; }

  // LA SESSION BANQUE EST-ELLE OUVERTE ?
  //
  // 🔴 ELLE NE VIT QUE CHEZ NOUS, et c'est le serveur qui la BASCULE. Il ne
  // tient aucun état de banque ouverte — le client d'origine ne lui envoie
  // jamais `CZ_REQ_CLOSE_BANKING` —, mais son `ZC_BANKING_CHECK` ouvre la
  // fenêtre si elle était fermée et la ferme si elle était ouverte. D'où
  // l'absence de « rafraîchir » : redemander le solde refermerait la banque.
  [[nodiscard]] bool open() const noexcept { return open_; }
  void Open() noexcept { open_ = true; }
  // ⚠ AUCUN PAQUET NE PART, et le contenu ne s'en va pas non plus : il n'y a
  // pas de contenu, seulement un solde que le serveur redira. C'est ce que fait
  // la croix de la fenêtre d'origine.
  void Close() noexcept { open_ = false; }

  // CE QUE LE SERVEUR A RÉPONDU À LA DERNIÈRE DEMANDE, et le NUMÉRO de cette
  // réponse.
  //
  // 🔴 LE NUMÉRO EST CE QUI REND LA RÉPONSE LISIBLE UNE FOIS. Deux refus
  // identiques de suite portent le même verdict, et sans compteur le second
  // passerait pour l'écho du premier — le joueur ne saurait pas que son
  // deuxième clic a été refusé lui aussi. Même mécanique que `last_chat_seq()`
  // du monde, et pour la même raison.
  //
  // ⚠ ZÉRO VEUT DIRE « LA BANQUE N'A ENCORE RIEN DIT ». Le verdict vaut alors
  // `kDone`, qui est aussi celui d'une demande passée : c'est le numéro qui les
  // sépare, jamais la valeur.
  [[nodiscard]] BankVerdict verdict() const noexcept { return verdict_; }
  [[nodiscard]] std::uint64_t verdict_seq() const noexcept { return verdict_seq_; }
  void TellVerdict(BankVerdict verdict) noexcept {
    verdict_ = verdict;
    ++verdict_seq_;
  }

 private:
  std::int64_t vault_ = 0;
  bool open_ = false;
  BankVerdict verdict_ = BankVerdict::kDone;
  std::uint64_t verdict_seq_ = 0;
};

}  // namespace ro::sim
