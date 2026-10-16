#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/proto/common_packets.h"

namespace ro::net {

// À quel rythme battre la liaison du serveur de PERSONNAGES.
//
// Le serveur ferme une session dont il n'a rien reçu depuis `stall_time`
// secondes — soixante par défaut (`src/common/socket.cpp`) —, et l'écran de
// sélection peut rester ouvert bien plus longtemps que cela. Douze secondes est
// le rythme du client d'origine, que le serveur nomme en toutes lettres
// au-dessus de sa table de lecture : « Client keep-alive packet (every 12
// seconds) ».
inline constexpr std::uint32_t kCharacterPingIntervalMs = 12'000;

// L'adresse d'un serveur, telle qu'elle circule.
struct ServerAddress {
  // Le champ tel qu'il arrive sur le fil.
  //
  // ⚠ Le serveur y écrit l'adresse en ordre RÉSEAU. Après le décodage
  // petit-boutiste, les quatre octets pointés se lisent donc du plus FAIBLE au
  // plus fort : 127.0.0.1 arrive comme 0x0100007f. Garder la valeur brute plutôt
  // qu'un entier « corrigé » évite d'avoir à se rappeler laquelle des deux
  // conventions un champ nommé `ip` porte.
  std::uint32_t wire_ip = 0;
  std::uint16_t port = 0;

  // L'adresse en notation pointée, prête pour Connection::Open.
  [[nodiscard]] std::string host() const;
};

// L'INDICE DANS LA LISTE du personnage assis à cet EMPLACEMENT, ou moins un.
//
// 🔴 LA LISTE N'EST PAS INDEXÉE PAR EMPLACEMENT, et rien à sa lecture ne le
// dit. Le serveur l'envoie DENSE, dans l'ordre où sa base la rend, et c'est
// `CharNum` qui porte l'emplacement de chacun (`info.CharNum = p.slot`,
// `char_mmo_char_tobuf`). Les deux coïncident tant que le compte n'a pas de
// TROU — donc tant que personne n'a supprimé ni déplacé de personnage —, et
// c'est exactement ce qui rend la confusion invisible à l'essai.
//
// ⚠ ET C'EST L'EMPLACEMENT QUE LE SERVEUR ATTEND : `CH_SELECT_CHAR` se résout
// par `char_num = slot` en base (`char_clif.cpp:1105`), `CH_MAKE_CHAR` de même.
// Lui envoyer un rang de liste à la place fait entrer en jeu avec un AUTRE
// personnage que celui qu'on montre — ou refuser, quand le rang ne désigne
// personne.
[[nodiscard]] int IndexOfSlot(std::span<const proto::CharacterInfo> characters,
                              std::uint8_t slot) noexcept;

// CE QU'UNE CRÉATION DEMANDE.
//
// ⚠ NI CARACTÉRISTIQUES NI CLASSE : depuis PACKETVER 20120307 le serveur pose
// lui-même les six caractéristiques à un, et depuis 20151001 il lit la classe du
// paquet — mais n'en accepte que deux (`allowed_job_flag`), et le client
// d'origine n'envoie jamais que le novice. Un champ qu'on ne
// saurait pas remplir de deux façons différentes n'est pas un champ.
struct CharacterDraft {
  std::string name;
  std::uint8_t slot = 0;
  std::uint16_t hair_style = 0;
  std::uint16_t hair_palette = 0;
  // 0 = femme, 1 = homme, comme partout ailleurs.
  std::uint8_t sex = 0;
};

// OÙ EN EST LA DERNIÈRE CRÉATION DEMANDÉE.
//
// 🔴 UN REFUS DE CRÉATION N'EST PAS UN REFUS DE PHASE, et c'est la raison
// d'être de cette énumération. `PhaseState::kRefused` MET FIN à la phase : la
// liaison ne sert plus, et l'écran revient au formulaire. Un nom déjà pris n'a
// rien à voir avec cela — le joueur reste devant sa tablée et en essaie un
// autre. Les confondre déconnecterait quelqu'un pour une faute de frappe.
enum class CreationState : std::uint8_t {
  // Rien de demandé, ou la réponse précédente a été lue et oubliée.
  kIdle,
  // La demande est partie.
  kPending,
  // Accepté : la liste porte le nouveau personnage.
  kCreated,
  // Refusé : `creation_refusal_text()` dit pourquoi.
  kRefused,
};

// QUELLE DEMANDE DE SUPPRESSION EST EN COURS.
//
// 🔴 LA SUPPRESSION SE FAIT EN DEUX TEMPS, séparés d'une heure en production
// (`char_del_delay: 3600`). On RÉSERVE, on attend l'échéance que le serveur
// pose, puis on CONFIRME avec le secret du compte — ou l'on ANNULE. Trois
// paquets, trois réponses, et trois tables de codes qui se ressemblent sans
// vouloir dire la même chose : voir `refusal_text.h`.
enum class DeletionStep : std::uint8_t {
  kNone,
  // `CH_DELETE_CHAR3_RESERVED` (0x0827) — aucun secret demandé.
  kReserve,
  // `CH_DELETE_CHAR3_CANCEL` (0x082b).
  kCancel,
  // `CH_DELETE_CHAR` (0x01fb) — avec le secret de `char_del_option`, donc
  // l'email en production.
  kConfirm,
};

// OÙ EN EST CETTE DEMANDE.
//
// 🔴 COMME POUR LA CRÉATION, UN REFUS N'EST PAS UN REFUS DE PHASE : un
// personnage en guilde reste à sa place, et le joueur va quitter sa guilde.
// Couper la liaison pour cela le renverrait au formulaire de connexion.
enum class DeletionState : std::uint8_t { kIdle, kPending, kDone, kRefused };

// QUELLE RETOUCHE D'UN PERSONNAGE EST EN COURS : changer d'emplacement, ou de
// nom.
//
// ⚠ L'UNE ET L'AUTRE SE PAIENT. Le serveur compte par PERSONNAGE ce qu'il lui
// reste — `chr_slot_changeCnt` et `chr_name_changeCnt` dans la liste —, et un
// coupon utilisé en jeu ajoute au premier. Rien ne se retouche à zéro.
enum class AmendStep : std::uint8_t {
  kNone,
  // `CH_REQ_CHANGE_CHARACTER_SLOT` (0x08d4).
  kMove,
  // `CH_REQ_CHANGE_CHARNAME` (0x08fc), qui porte le nouveau nom : la
  // vérification préalable `CH_REQ_IS_VALID_CHARNAME` est facultative à cette
  // version, et le serveur refait les mêmes contrôles au renommage.
  kRename,
};

// 🔴 COMME POUR LA SUPPRESSION, UN REFUS N'EST PAS UN REFUS DE PHASE : un nom
// déjà pris se corrige devant la tablée.
enum class AmendState : std::uint8_t { kIdle, kPending, kDone, kRefused };

// Où en est une phase de la session.
enum class PhaseState : std::uint8_t {
  // Rien de neuf : la boucle doit rappeler. Ce n'est pas une erreur.
  kWaiting,
  // La phase a obtenu ce qu'elle attendait.
  kDone,
  // Le serveur a dit non ; refusal_code() dit pourquoi.
  kRefused,
  // Le lien est tombé, ou une trame était inintelligible ; error() le dit.
  kFailed,
};

// Premier acte : la demande de connexion.
//
// Elle se termine sur la liste des serveurs de personnages et les jetons de
// session qu'il faudra présenter au suivant.
class LoginPhase {
 public:
  struct CharacterServer {
    std::string name;
    ServerAddress address;
    std::uint16_t users = 0;
  };

  // Ce que le serveur de connexion accorde, et que le serveur de personnages
  // exigera tel quel.
  struct Account {
    std::uint32_t account_id = 0;
    std::uint32_t login_id1 = 0;
    std::uint32_t login_id2 = 0;
    std::uint8_t sex = 0;
    // LE JETON DU SERVICE WEB du serveur, que le client présente avec son
    // compte pour y déposer ou y lire un emblème de guilde. ⚠ IL VAUT UNE
    // SESSION : il ne se journalise pas.
    std::string web_token;
    // LE NOM DU MONDE, celui du serveur de personnages que la session
    // rejoint — le premier de la liste, que les deux pilotes choisissent. Le
    // service web range les emblèmes sous ce nom.
    std::string world_name;
  };

  // Ouvre le lien et envoie la demande.
  //
  // Le mot de passe traverse le fil EN CLAIR : c'est ce que le protocole
  // prévoit à cette version, et le dire ici vaut mieux que de le laisser
  // découvrir. Il n'est ni conservé ni journalisé par cette classe.
  [[nodiscard]] static core::Result<LoginPhase> Start(std::string_view host, std::uint16_t port,
                                                      std::string_view account,
                                                      std::string_view password);

  // À rappeler à chaque tour de boucle tant qu'elle rend kWaiting.
  [[nodiscard]] PhaseState Advance();

  [[nodiscard]] const Account& account() const noexcept { return account_; }
  [[nodiscard]] const std::vector<CharacterServer>& servers() const noexcept { return servers_; }
  [[nodiscard]] std::uint32_t refusal_code() const noexcept { return refusal_code_; }

  // Le refus EN CLAIR, quand on sait le dire.
  //
  // 🔴 LE CODE SEUL NE SUFFIT PLUS : deux paquets différents refusent une même
  // phase — le refus propre à l'acte et l'avis d'éviction `SC_NOTIFY_BAN` —, et
  // leurs codes se lisent dans des tables DIFFÉRENTES. Rendre le seul nombre
  // laisserait l'appelant choisir la mauvaise, et le motif affiché serait faux
  // sans que rien ne le dise.
  //
  // Vide quand le refus vient d'une table qu'on ne traduit pas encore :
  // l'appelant montre alors le code, comme avant.
  [[nodiscard]] const std::string& refusal_text() const noexcept { return refusal_text_; }
  [[nodiscard]] const core::Error& error() const noexcept { return error_; }

 private:
  explicit LoginPhase(Connection link) noexcept : link_(std::move(link)) {}

  Connection link_;
  Account account_;
  std::vector<CharacterServer> servers_;
  std::uint32_t refusal_code_ = 0;
  std::string refusal_text_;
  core::Error error_ = core::Error{core::ErrorCode::kIoFailure, {}};
  PhaseState state_ = PhaseState::kWaiting;
};

// Second acte : le choix du personnage.
//
// Il se termine sur l'adresse du serveur de zone et la carte où l'on entre —
// tout ce qu'il faut pour l'acte suivant, qui n'existe pas encore.
class CharacterPhase {
 public:
  // Où en est l'échange. L'appelant en a besoin : c'est LUI qui choisit le
  // personnage, et il ne peut le faire qu'une fois la liste reçue.
  enum class Step : std::uint8_t {
    kAwaitingAccountId,
    kAwaitingCharacterList,
    // Le serveur réclame le code PIN : à l'appelant d'appeler SubmitPincode.
    // La liste, elle, est DÉJÀ arrivée — le serveur l'envoie avant de demander.
    kAwaitingPincode,
    kCheckingPincode,
    // La liste est là : à l'appelant d'appeler Select.
    kAwaitingSelection,
    kAwaitingZoneAddress,
    kDone,
  };

  struct Handoff {
    std::uint32_t character_id = 0;
    // Le nom de la carte SANS extension. Le serveur la nomme gonryun.gat, du
    // nom de son fichier de marchabilité ; une carte en compte trois, et le
    // reste du client les désigne par leur tronc commun.
    std::string map_name;
    ServerAddress address;
  };

  [[nodiscard]] static core::Result<CharacterPhase> Start(const ServerAddress& server,
                                                          const LoginPhase::Account& account);

  [[nodiscard]] PhaseState Advance();

  // Demande l'entrée en jeu avec le personnage de cet emplacement.
  //
  // Sans effet tant que la liste n'est pas là : l'emplacement d'un personnage
  // qu'on n'a pas encore vu ne veut rien dire.
  //
  // ⚠ Choisir avant d'avoir répondu au code PIN ferait COUPER la liaison : le
  // serveur n'accepte qu'une poignée de paquets tant que le code n'est pas
  // donné, et ferme sans un mot sur tous les autres.
  void Select(std::uint8_t slot);

  // Demande la création d'un personnage à cet emplacement.
  //
  // ⚠ SANS EFFET HORS DU CHOIX, pour la même raison que `Select` : le serveur
  // n'accepte qu'une poignée de paquets tant que le code PIN n'est pas donné,
  // et ferme sans un mot sur tous les autres.
  //
  // ⚠ ET RIEN N'EST VALIDÉ ICI. Cette couche n'a pas les règles du serveur —
  // longueur minimale, lettres autorisées, plafond de personnages — et les
  // recopier au jugé donnerait une SECONDE vérité, celle qui refuserait un nom
  // que le serveur aurait pris. L'écran vérifie ce qu'il sait (le nom vide,
  // l'emplacement occupé), le serveur tranche le reste.
  void Create(const CharacterDraft& draft);

  // Où en est la dernière création demandée, et pourquoi elle a échoué.
  //
  // ⚠ L'APPELANT OUBLIE LA RÉPONSE quand il l'a montrée — `ForgetCreation` —,
  // sans quoi un refus lu une fois resterait affiché sous la tentative
  // suivante.
  [[nodiscard]] CreationState creation() const noexcept { return creation_; }
  [[nodiscard]] std::uint8_t creation_refusal_code() const noexcept {
    return creation_refusal_code_;
  }
  [[nodiscard]] const std::string& creation_refusal_text() const noexcept {
    return creation_refusal_text_;
  }
  // L'emplacement du dernier personnage créé, ou moins un.
  [[nodiscard]] int created_slot() const noexcept { return created_slot_; }
  void ForgetCreation() noexcept;

  // ── LA SUPPRESSION, EN DEUX TEMPS ─────────────────────────────────────────
  //
  // ⚠ LES TROIS DEMANDES DÉSIGNENT LE PERSONNAGE PAR SON IDENTIFIANT, jamais
  // par son emplacement : c'est ce que portent les trois paquets, et c'est ce
  // qui les rend insensibles à un déplacement d'emplacement entre deux gestes.
  //
  // ⚠ SANS EFFET HORS DU CHOIX, comme `Select` et `Create`.

  // Premier temps : réserve la suppression. Aucun secret n'est demandé ; le
  // serveur pose l'échéance et la rend dans sa réponse.
  void ReserveDeletion(std::uint32_t character_id);

  // Renonce à une suppression réservée.
  void CancelDeletion(std::uint32_t character_id);

  // Second temps : supprime pour de bon, l'échéance passée.
  //
  // 🔴 LE SECRET EST CELUI DE `char_del_option`, donc l'EMAIL du compte en
  // production. ⚠ Et l'envoyer AVANT l'échéance échoue sous le code que le
  // serveur emploie pour un mauvais email — voir `DeleteRefusalText`.
  void ConfirmDeletion(std::uint32_t character_id, std::string_view secret);

  [[nodiscard]] DeletionStep deletion_step() const noexcept { return deletion_step_; }
  [[nodiscard]] DeletionState deletion() const noexcept { return deletion_; }
  // Le personnage visé par la demande en cours, ou zéro.
  [[nodiscard]] std::uint32_t deletion_character_id() const noexcept {
    return deletion_character_id_;
  }
  [[nodiscard]] const std::string& deletion_refusal_text() const noexcept {
    return deletion_refusal_text_;
  }
  void ForgetDeletion() noexcept;

  // ── LES RETOUCHES : DÉPLACER, RENOMMER ────────────────────────────────────
  //
  // ⚠ SANS EFFET HORS DU CHOIX, comme le reste.
  //
  // 🔴 LE SERVEUR ACCUSE, PUIS RENVOIE LA LISTE ENTIÈRE, dans cet ordre. La
  // réussite se reporte donc aussitôt dans notre copie, et la liste qui suit la
  // remplace par la vérité de la base — le compteur restant compris.

  // Déplace le personnage de `from_slot` vers `to_slot`. Une place occupée
  // ÉCHANGE les deux personnages, quand le serveur le permet
  // (`char_movetoused`).
  void MoveCharacter(std::uint8_t from_slot, std::uint8_t to_slot);

  // Renomme ce personnage. `new_name` est dans la page de code du serveur.
  //
  // ⚠ RIEN N'EST VALIDÉ ICI, pour la raison dite à `Create`.
  void RenameCharacter(std::uint32_t character_id, std::string_view new_name);

  [[nodiscard]] AmendStep amend_step() const noexcept { return amend_step_; }
  [[nodiscard]] AmendState amend() const noexcept { return amend_; }
  // L'emplacement d'arrivée du dernier déplacement demandé, ou moins un.
  [[nodiscard]] int amend_target_slot() const noexcept { return amend_to_slot_; }
  [[nodiscard]] const std::string& amend_refusal_text() const noexcept {
    return amend_refusal_text_;
  }
  void ForgetAmend() noexcept;

  // Répond à la demande de code PIN.
  //
  // Le code est brouillé avec la graine de la dernière demande reçue ; il ne
  // part jamais tel quel, et cette classe ne le conserve pas.
  //
  // Rend une erreur, SANS rien émettre, si le code n'a pas la forme exigée —
  // quatre chiffres. Le serveur, lui, coupe la liaison dans ce cas. Sans effet
  // hors de kAwaitingPincode.
  [[nodiscard]] core::Result<void> SubmitPincode(std::string_view digits);

  // Tient la liaison ouverte pendant que l'on choisit.
  //
  // À appeler à CHAQUE tour de boucle : elle décide elle-même s'il est temps.
  // Le paquet est l'un des trois seuls que le serveur de personnages accepte
  // d'une session spectateur, donc le décor vivant peut battre comme le reste.
  //
  // ⚠ LE PREMIER APPEL ARME L'HORLOGE SANS RIEN ENVOYER. Cette couche ne lit
  // aucune horloge — elle ignore donc QUAND la demande d'entrée est partie —, et
  // battre aussitôt poserait un paquet sur une liaison qui vient d'en porter un.
  // L'appelant bat dès qu'il tient la phase, si bien que ce premier appel marque
  // à peu de chose près l'ouverture.
  //
  // 🔴 L'IDENTIFIANT DOIT ÊTRE CELUI DE LA SESSION : `chclif_parse_keepalive`
  // REFUSE la trame dont l'identifiant ne correspond pas, et un refus de lecture
  // ferme la liaison. Rien ne part donc tant que le serveur n'a pas donné son
  // identifiant nu.
  //
  // `now_ms` est l'horloge de l'APPELANT.
  void Beat(std::uint32_t now_ms);

  // Vrai si le serveur a rejeté le dernier code proposé.
  //
  // L'appelant en a besoin pour distinguer « donne ton code » de « recommence,
  // celui-là est faux » : le serveur emploie le même paquet pour les deux.
  [[nodiscard]] bool pincode_refused() const noexcept { return pincode_refused_; }

  [[nodiscard]] Step step() const noexcept { return step_; }
  // LA LISTE, telle que la tablée la montre.
  //
  // ⚠ `DelRevDate` Y EST UNE DATE UNIX, zéro quand rien n'est programmé. Le
  // fil, lui, porte un délai restant : il est ancré à la réception.
  [[nodiscard]] const std::vector<proto::CharacterInfo>& characters() const noexcept {
    return characters_;
  }

  // COMBIEN D'EMPLACEMENTS CE COMPTE PEUT REMPLIR, et combien la liste en
  // compte au total. Zéro tant que le serveur ne les a pas annoncés.
  //
  // 🔴 CE N'EST PAS LE NOMBRE DE PERSONNAGES, et les deux ne se déduisent pas
  // l'un de l'autre : un compte neuf a zéro personnage et neuf emplacements. Un
  // écran qui compterait les personnages pour dessiner des places libres n'en
  // montrerait aucune, donc n'offrirait jamais de créer.
  //
  // ⚠ ILS VIENNENT D'UN AUTRE PAQUET QUE LA LISTE — `HC_ACCEPT_ENTER2`
  // (0x082d), envoyé juste AVANT elle —, et ce sont deux nombres différents :
  // `producible` est ce que le compte peut remplir (`sd.char_slots`), `total`
  // est ce que le protocole permet (`MAX_CHARS`). Le second sert à lire la
  // liste, le premier à décider si une place se propose.
  [[nodiscard]] std::uint8_t producible_slots() const noexcept { return producible_slots_; }
  [[nodiscard]] std::uint8_t total_slots() const noexcept { return total_slots_; }
  [[nodiscard]] const Handoff& handoff() const noexcept { return handoff_; }
  [[nodiscard]] std::uint32_t refusal_code() const noexcept { return refusal_code_; }
  // Voir `LoginPhase::refusal_text`.
  [[nodiscard]] const std::string& refusal_text() const noexcept { return refusal_text_; }
  [[nodiscard]] const core::Error& error() const noexcept { return error_; }

 private:
  explicit CharacterPhase(Connection link) noexcept : link_(std::move(link)) {}

  // Traite une annonce d'état du code PIN. Rend faux si elle est fatale ; les
  // champs d'erreur sont alors renseignés.
  [[nodiscard]] bool HandlePincodeState(std::uint16_t state);

  // Le personnage de cet identifiant dans notre copie de la liste, ou nul.
  //
  // ⚠ PAR IDENTIFIANT ET NON PAR EMPLACEMENT : c'est ce que portent les trois
  // paquets de suppression, et cela reste juste même si l'emplacement a bougé
  // entre la demande et la réponse.
  [[nodiscard]] proto::CharacterInfo* Seated(std::uint32_t character_id) noexcept;

  // Lit l'accusé d'une retouche. Rend faux si la trame n'en est pas un ; une
  // trame illisible pose `kFailed`, que l'appelant relit.
  [[nodiscard]] bool TookAmendAnswer(const Frame& frame);

  Connection link_;
  Step step_ = Step::kAwaitingAccountId;
  std::uint32_t account_id_ = 0;
  // La graine du dernier brouillage annoncé. Le serveur en tire une NEUVE à
  // chaque annonce, y compris pour dire « code faux » : garder la première
  // ferait refuser un code pourtant juste.
  std::uint32_t pincode_seed_ = 0;
  bool pincode_refused_ = false;
  // 🔴 LA LISTE EST-ELLE ARRIVÉE ? Un TÉMOIN, et non le nombre de personnages :
  // un compte qui vient d'être créé en a ZÉRO, et compter les lire rendait sa
  // liste indiscernable d'une liste jamais reçue. La phase repartait alors
  // attendre ce qu'elle tenait déjà — c'est-à-dire pour toujours.
  bool list_received_ = false;
  std::uint8_t producible_slots_ = 0;
  std::uint8_t total_slots_ = 0;
  CreationState creation_ = CreationState::kIdle;
  std::uint8_t creation_refusal_code_ = 0;
  std::string creation_refusal_text_;
  int created_slot_ = -1;
  DeletionStep deletion_step_ = DeletionStep::kNone;
  DeletionState deletion_ = DeletionState::kIdle;
  std::uint32_t deletion_character_id_ = 0;
  std::string deletion_refusal_text_;
  AmendStep amend_step_ = AmendStep::kNone;
  AmendState amend_ = AmendState::kIdle;
  // ⚠ LES DEUX ACCUSÉS NE DISENT NI QUI NI OÙ : on retient la demande.
  int amend_from_slot_ = -1;
  int amend_to_slot_ = -1;
  std::uint32_t amend_character_id_ = 0;
  std::string amend_name_;
  std::string amend_refusal_text_;
  // Quand le dernier battement est parti, ou rien tant que l'horloge n'est pas
  // armée. Voir `Beat`.
  std::optional<std::uint32_t> last_beat_ms_;
  std::vector<proto::CharacterInfo> characters_;
  Handoff handoff_;
  std::uint32_t refusal_code_ = 0;
  std::string refusal_text_;
  core::Error error_ = core::Error{core::ErrorCode::kIoFailure, {}};
  PhaseState state_ = PhaseState::kWaiting;
};

// CE QUI PRÉCÈDE LA PREMIÈRE TRAME D'UNE LIAISON DE ZONE : l'identifiant de
// compte NU, sans opcode, que le serveur envoie juste avant d'accepter.
//
// Rend le nombre d'octets à sauter — 4 devant une acceptation, 0 sinon — ou
// rien tant que `head`, le début du flux, est trop court pour trancher.
//
// ⚠ RIEN DANS CES QUATRE OCTETS NE DIT CE QU'ILS SONT : c'est ce qui les SUIT.
// Le refus arrive sans identifiant, et un compte dont les 16 bits bas valent
// l'opcode du refus existe : l'acceptation se cherche donc la première. Le
// refus fait assez d'octets pour que l'attente se termine toujours.
//
// La même règle sert la phase d'entrée et la relecture d'un flux enregistré.
[[nodiscard]] std::optional<std::size_t> ZoneStreamPreamble(std::span<const std::uint8_t> head);

// Troisième acte : l'entrée sur la carte.
//
// Il se termine dès que le serveur a accepté et que l'avis de carte chargée est
// parti. Ce qui suit n'est plus une poignée de main mais le monde lui-même, et
// c'est à l'appelant de le lire — la liaison lui est remise.
class MapPhase {
 public:
  enum class Step : std::uint8_t {
    kAwaitingAccountId,
    kAwaitingAccept,
    // Le serveur a accepté, l'avis de carte chargée est parti, le monde arrive.
    kInGame,
  };

  // Où le personnage apparaît, tel que le serveur le donne.
  struct Spawn {
    // L'horloge du serveur au moment de l'acceptation, en millisecondes.
    std::uint32_t start_tick = 0;
    // Trois octets où deux coordonnées et une direction sont empaquetées.
    // Rendus tels quels : les découper est l'affaire de la couche qui place le
    // personnage, pas de celle qui transporte.
    std::array<std::uint8_t, 3> packed_position = {};
  };

  // `client_tick` est l'horloge du client. Le serveur s'en sert pour caler la
  // sienne ; la couche réseau ne lit aucune horloge elle-même.
  [[nodiscard]] static core::Result<MapPhase> Start(const CharacterPhase::Handoff& handoff,
                                                    const LoginPhase::Account& account,
                                                    std::uint32_t client_tick);

  [[nodiscard]] PhaseState Advance();

  [[nodiscard]] Step step() const noexcept { return step_; }
  [[nodiscard]] const Spawn& spawn() const noexcept { return spawn_; }
  [[nodiscard]] std::uint32_t refusal_code() const noexcept { return refusal_code_; }
  // Voir `LoginPhase::refusal_text`.
  [[nodiscard]] const std::string& refusal_text() const noexcept { return refusal_text_; }
  [[nodiscard]] const core::Error& error() const noexcept { return error_; }

  // Donne à voir les octets reçus sur la liaison de zone.
  //
  // Se pose AVANT l'acceptation, parce que c'est là que le flux commence : une
  // capture qui débuterait après manquerait l'accord d'entrée et tout ce que le
  // serveur envoie dans la foulée.
  void SetReceiveObserver(ReceiveObserver observer) {
    link_.set_receive_observer(std::move(observer));
  }

  // La liaison, une fois l'entrée acceptée.
  //
  // ⚠ La phase s'en dessaisit : elle a fini son travail, et laisser deux objets
  // croire qu'ils lisent le même flux ferait perdre des trames à l'un des deux.
  [[nodiscard]] Connection TakeLink() noexcept { return std::move(link_); }

 private:
  explicit MapPhase(Connection link) noexcept : link_(std::move(link)) {}

  // Décide si les premiers octets reçus portent une trame ou un identifiant nu.
  // Rend faux tant qu'il n'y a pas de quoi trancher.
  [[nodiscard]] bool DecideBareAccountId();

  Connection link_;
  Step step_ = Step::kAwaitingAccountId;
  std::uint32_t account_id_ = 0;
  Spawn spawn_;
  std::uint32_t refusal_code_ = 0;
  std::string refusal_text_;
  core::Error error_ = core::Error{core::ErrorCode::kIoFailure, {}};
  PhaseState state_ = PhaseState::kWaiting;
  // Les trames reçues avant l'accord, que la phase ne lit pas : elles reviennent
  // au jeu avec celles qui le suivent, voir `Connection::Requeue`.
  std::vector<Frame> held_;
};

}  // namespace ro::net
