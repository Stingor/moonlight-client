#pragma once

#include <cstdint>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA BANQUE DE ZENY : un coffre sans case, et une fenêtre qui ne se redimensionne
// pas.
//
// 🔴 SON FOND EST UN BITMAP À HAUTEUR FIXE, ET C'EST LUI QUI COMMANDE. Le sac de
// zeny et la bande grise du montant sont CUITS dans l'image, aux pixels près :
// la fenêtre prend donc la largeur du bitmap, le blit est 1:1, et rien de son
// contenu ne se masque — un bloc qu'on retirerait ferait glisser tout ce qui
// suit hors du fond peint. C'est la seule fenêtre du client dans ce cas, et
// c'est pourquoi elle n'a aucun réglage.
//
// ⚠ ELLE NE S'OUVRE PAS TOUTE SEULE, et le raccourci ne l'ouvre pas non plus :
// il DEMANDE au serveur, qui répond, et c'est la réponse qui ouvre — ou ferme.
// L'état d'ouverture recopie donc `world.bank().open()` ; voir `ro/net/bank.h`.
//
// ⚠ ET IL N'Y A PAS DE BOUTON « RAFRAÎCHIR » : redemander le solde REFERMERAIT
// la banque. Ce n'est pas un oubli, c'est le protocole.

// LES PLAFONDS, en zeny. Trois notions, trois noms :
//   · la POCHE tient au plus `kPocketZenyMax` (`MAX_ZENY` du serveur) ;
//   · une DEMANDE porte son montant dans un `int32` : `kBankRequestMostZeny` ;
//   · la BANQUE tient ce que le serveur annonce (`BankWindowView::vault_max`),
//     ou `kBankVaultMaxUnannounced` tant qu'il n'a rien dit — le plafond d'un
//     rAthena d'usine.
//
// 🔴 ILS SONT DÉCLARÉS DEUX FOIS, ICI ET DANS `sim`, ET CE N'EST PAS UN
// OUBLI : `ui` ne connaît pas `sim` — le graphe des bibliothèques l'interdit,
// et c'est une propriété qu'on tient. Ils sont recoupés par des `static_assert`
// dans `app/items/bank_view.cpp`, le seul endroit qui voit les deux : leur
// divergence est une erreur de compilation, pas une remarque en revue.
inline constexpr std::int64_t kPocketZenyMax = 2147483647;
inline constexpr std::int64_t kBankRequestMostZeny = 2147483647;
inline constexpr std::int64_t kBankVaultMaxUnannounced = 2147483647;

// LE TITRE ET L'IDENTITÉ. ⚠ L'IDENTITÉ NE SE TRADUIT PAS : c'est elle qui porte
// la place que le joueur a donnée à la fenêtre.
inline constexpr const char* kBankWindowTitle = core::TrText("Banque");
inline constexpr const char* kBankWindowId = "bank";

// CE QUE LA FENÊTRE SAIT DU MONDE, recopié par l'appelant. ⚠ `ui` NE CONNAÎT NI
// `sim` NI LE PROTOCOLE : elle reçoit trois nombres et un drapeau.
struct BankWindowView {
  // Le solde en banque, et la poche. ⚠ LES DEUX SUR SOIXANTE-QUATRE BITS : le
  // solde dépasse un `int32` sur un serveur qui le permet, et un total qui
  // additionne les deux déborderait d'un `int32`.
  std::int64_t vault = 0;
  std::int64_t zeny = 0;
  // ⚠ LA BOURSE PEUT ÊTRE INCONNUE. Le serveur de zone n'annonce `SP_ZENY` que
  // lorsqu'elle CHANGE : elle vient de la liste des personnages, et tant que
  // rien ne l'a posée, un zéro se lirait « vous n'avez rien » au lieu de « je
  // ne sais pas ». La fenêtre grise alors ses deux boutons.
  bool zeny_known = false;
  // Le plafond de la banque que le serveur annonce. Voir `kPocketZenyMax`.
  std::int64_t vault_max = kBankVaultMaxUnannounced;
};

// CE QUE LA FENÊTRE A DEMANDÉ.
//
// ⚠ ELLE N'ENVOIE RIEN ELLE-MÊME et ne bouge aucun solde : elle rend un verbe,
// et c'est `app` qui parle au serveur. Même règle que ses voisines.
enum class BankVerb : std::uint8_t {
  kNothing,
  kDeposit,
  kWithdraw,
  // 🔴 FERMER N'EST PAS UN VERBE DU SERVEUR, ici : il ne tient aucun état de
  // banque ouverte, et le client d'origine ne lui envoie jamais sa fermeture.
  // La croix ferme donc la session CHEZ NOUS, sans un octet sur le fil.
  kClose,
};

struct BankWindowAction {
  BankVerb verb = BankVerb::kNothing;
  // Le montant du transfert, déjà validé par la fenêtre. ⚠ IL NE VAUT QUE POUR
  // UN VERBE DE TRANSFERT, et il est alors dans `]0, INT32_MAX]`.
  std::int64_t amount = 0;
};

// CE QUE LA FENÊTRE GARDE D'UNE IMAGE À L'AUTRE.
struct BankWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie `world.bank().open()`. Une
  // fenêtre ouverte sans le serveur montrerait un solde périmé.
  bool open = false;
  // Le front montant, pour vider le champ et le message à l'ouverture.
  bool was_open = false;

  // LE MONTANT SAISI. ⚠ SUR SOIXANTE-QUATRE BITS POUR ABSORBER UNE SAISIE HORS
  // BORNES sans déborder : il est borné à `[0, INT32_MAX]` à chaque frappe,
  // comme le fait le client d'origine.
  std::int64_t amount = 0;

  // LE MESSAGE DE LA BANDE, sous le champ de saisie — l'équivalent de la chaîne
  // que la fenêtre d'origine garde pour son bas de cadre.
  //
  // ⚠ UN TAMPON ET NON UN POINTEUR : le texte vient d'une traduction, dont le
  // pointeur meurt au prochain changement de langue.
  char status[192] = {};
  bool status_error = false;
};

// LES PALIERS D'INCRÉMENT, dans l'ordre de la rangée.
//
// ⚠ PAS DE « +1k » : Moonlight est un serveur à taux élevé, et mille zeny n'y
// représentent rien. Les trois boutons du client d'origine valent en réalité
// 1e5, 1e6 et 1e7 — leurs bitmaps se nomment `10mil`, `100mil` et `1000mil`,
// qui mentent —, et les deux paliers ajoutés encadrent les siens.
inline constexpr int kBankStepCount = 5;

// Pose le message de la bande. ⚠ DÉJÀ TRADUIT : cette couche ne traduit pas ce
// que l'appelant lui donne.
void SetBankStatus(BankWindowState& state, const char* utf8, bool is_error);

// Le plus grand dépôt et le plus grand retrait possibles, ceux du mode « Max ».
//
// 🔴 LE PLAFOND S'APPLIQUE À LA DESTINATION, jamais à la source : déposer est
// borné par ce que la BANQUE peut encore recevoir (`vault_max`), retirer par ce
// que la POCHE peut encore contenir (`kPocketZenyMax`). Les intervertir laisse
// proposer un transfert que le serveur refuse en bloc. Les deux sont bornés en
// outre par ce qu'une demande porte, `kBankRequestMostZeny`.
[[nodiscard]] std::int64_t BankMaxDeposit(std::int64_t vault, std::int64_t zeny,
                                          std::int64_t vault_max);
[[nodiscard]] std::int64_t BankMaxWithdraw(std::int64_t vault, std::int64_t zeny);

// LA HAUTEUR DE LA BANDE GRISE DU FOND, en pixels d'art — quarante-huit
// lignes, relevées sur le bitmap (`[86, 134[`).
//
// 🔴 ELLE EST UN PLANCHER, PAS UNE MESURE : la bande s'ouvre sur le trait de
// séparation et se referme quarante-huit lignes plus bas — ou au bas du champ
// et de sa rangée de paliers, si une grande police les fait déborder. La
// rétrécir déformerait le sac de zeny qu'elle porte.
inline constexpr float kBankBandPixels = 48.0F;

// Borne une saisie à `[0, kBankRequestMostZeny]`, à chaque frappe et à chaque
// palier : c'est ce qu'une demande peut porter.
[[nodiscard]] std::int64_t BankClampAmount(std::int64_t amount);

[[nodiscard]] BankWindowAction DrawBankWindow(BankWindowState& state, const BankWindowView& view,
                                              const SkinArt& art);

}  // namespace ro::ui
