#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/socket.h"
#include "ro/proto/frame_reader.h"

namespace ro::net {

// Une trame reçue, opcode compris.
struct Frame {
  std::uint16_t opcode = 0;
  // La trame ENTIÈRE, opcode compris. Le décodeur lit donc des décalages
  // identiques à ceux des sources du serveur, sans en retrancher deux.
  std::vector<std::uint8_t> bytes;
  // LA TRAME QUI LA DÉCRIT, reçue juste avant elle, entière, ou vide : l'auteur
  // d'une ligne de parole (`ZC_MOONLIGHT_CHAT_AUTHOR`). Voir `Connection`.
  std::vector<std::uint8_t> preamble;
};

// L'état d'une liaison après un tour de boucle.
enum class LinkStatus : std::uint8_t {
  // La session continue. C'est le seul état où l'on peut encore émettre.
  kOpen,
  // Le serveur a fermé proprement. Arrive normalement à la fin des phases de
  // connexion et de personnage : chacune se termine par la fermeture du lien.
  kClosedByPeer,
  // Opcode inconnu ou trame impossible. Le flux n'est plus interprétable et il
  // n'existe aucun moyen de le rattraper : voir ro::proto::NextFrame.
  kProtocolError,
  // Le socket a échoué.
  kIoError,
};

// Un lien vers un serveur : le socket, le découpage, et rien d'autre.
//
// Ce qui traverse cette classe, ce sont des OCTETS. Elle ne connaît qu'un
// paquet — l'auteur de parole, qu'elle attache à la trame suivante —, ne
// décode rien d'autre, et ne touche à aucun état de jeu — c'est ce qui
// permettra de la déplacer sur un fil dédié sans avoir à se demander ce qui est
// sûr d'être touché depuis où.
// Reçoit les octets bruts au fil de leur arrivée, avant tout découpage.
//
// Sert à ENREGISTRER une session pour la rejouer. Vide, rien n'est observé.
using ReceiveObserver = std::function<void(std::span<const std::uint8_t>)>;

// Reçoit chaque trame CONFIÉE à l'émission, telle qu'on l'a confiée.
//
// Sert à qui doit savoir ce que le joueur a DEMANDÉ quand la réponse ne le
// rappelle pas : une liste de fabrication ne dit ni le skill ni l'objet qui
// l'ont ouverte. Vide, rien n'est observé.
using SendObserver = std::function<void(std::span<const std::uint8_t>)>;

// JUGE CHAQUE TRAME CONFIÉE À L'ÉMISSION, avant qu'elle parte : faux, elle ne
// part pas.
//
// C'est le DERNIER REMPART d'une règle qui ne doit souffrir aucun chemin
// oublié : pendant un échange, ce que le serveur tiendrait pour de la triche
// ne quitte pas le client, quel que soit le geste qui l'a demandé. Vide, tout
// part.
using SendGate = std::function<bool(std::span<const std::uint8_t>)>;

class Connection {
 public:
  Connection() = default;

  // Ce qu'un tour de boucle a produit.
  struct Polled {
    LinkStatus status = LinkStatus::kOpen;
    std::vector<Frame> frames;
    // Les octets non encadrés qui avaient été annoncés par ExpectRaw. Vides en
    // régime normal.
    std::vector<std::uint8_t> raw;
  };

  [[nodiscard]] static core::Result<Connection> Open(std::string_view host, std::uint16_t port,
                                                     int timeout_ms = kConnectTimeoutMs);

  // Vide le tampon d'émission, lit ce qui est arrivé, et rend les trames
  // entières.
  //
  // 🔴 UN AUTEUR DE PAROLE (`ZC_MOONLIGHT_CHAT_AUTHOR`) N'EST PAS RENDU SEUL :
  // il s'attache, en `preamble`, à la trame qui le suit — si c'est bien
  // l'opcode qu'il annonce ; sinon il se perd. Il attend cette trame d'un tour
  // de boucle à l'autre, puisqu'une lecture peut couper entre les deux.
  //
  // Les trames sont rendues MÊME quand le statut annonce la fin : ce que le
  // serveur a envoyé avant de fermer est souvent la raison de la fermeture, et
  // la jeter serait perdre le seul message qui l'explique.
  [[nodiscard]] Polled Poll() {
    Receive();
    return Extract();
  }

  // Les deux moitiés de Poll, séparées.
  //
  // Elles existent pour un seul cas, mais il est réel : le serveur de zone fait
  // précéder son ACCEPTATION d'un identifiant nu, et son REFUS de rien du tout.
  // Décider lequel arrive demande de REGARDER les octets reçus avant de les
  // découper — ce que Poll, qui fait les deux d'un trait, ne permet pas.
  //
  // Recevoir sans découper ne perd rien : les octets restent dans le tampon.
  void Receive();
  [[nodiscard]] Polled Extract();

  // Met des octets en attente d'émission, et tente de les envoyer aussitôt.
  //
  // ⚠ L'envoi peut n'être que partiel ; le reste part au prochain Poll. Un
  // client qui ignore ce cas coupe une trame en deux dès que le tampon du
  // système se remplit, et la session se désynchronise sans erreur visible.
  void Send(std::span<const std::uint8_t> bytes);

  // Annonce que les `count` prochains octets reçus ne portent pas de trame.
  //
  // Tout le flux n'est pas encadré : le serveur de personnages répond à la
  // demande d'entrée par un identifiant de compte nu, sans opcode. Seul
  // l'appelant sait quand cela arrive, parce que seul lui sait ce qu'il vient
  // de demander.
  void ExpectRaw(std::size_t count) { reader_.ExpectRaw(count); }

  // Rend des trames déjà découpées, que le prochain `Extract` restituera EN TÊTE,
  // dans leur ordre.
  //
  // 🔴 UNE PHASE QUI S'ARRÊTE AU MILIEU D'UNE LECTURE NE JETTE PAS LA SUITE. Le
  // serveur de zone envoie la liste d'amis et d'autres états dans la même
  // foulée que son accord d'entrée : ils arrivent souvent dans la même lecture
  // que lui, et la phase qui ne lit que l'accord doit les laisser au jeu.
  void Requeue(std::vector<Frame> frames);

  using Observer = ReceiveObserver;

  // Donne à voir les octets reçus, AVANT tout découpage.
  //
  // C'est ce qui permet d'enregistrer une session pour la rejouer. Le découpage
  // ne peut pas rendre ce service : quand il bute, c'est justement que le flux
  // n'est plus des trames, et ce qu'il faut relire est ce qu'il a refusé.
  //
  // La couche réseau n'écrit AUCUN fichier : elle tend les octets, et
  // l'appelant en fait ce qu'il veut. Une bibliothèque qui ouvre un fichier
  // décide à la place de son programme.
  void set_receive_observer(ReceiveObserver observer) { observer_ = std::move(observer); }

  // Donne à voir chaque trame confiée à `Send`. ⚠ UN SEUL OBSERVATEUR : en
  // poser un second remplace le premier.
  void set_send_observer(SendObserver observer) { send_observer_ = std::move(observer); }

  // Pose le juge des trames émises. ⚠ UN SEUL JUGE : en poser un second
  // remplace le premier. Une trame qu'il refuse n'est pas non plus observée.
  void set_send_gate(SendGate gate) { send_gate_ = std::move(gate); }

  // Ce que la dernière réception a conclu, avant tout découpage.
  [[nodiscard]] LinkStatus received_status() const noexcept { return received_; }

  // Les octets reçus et pas encore découpés. Voir FrameReader::pending_bytes.
  [[nodiscard]] std::span<const std::uint8_t> pending_bytes() const noexcept {
    return reader_.pending_bytes();
  }

  void Close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return socket_.is_open(); }

  // Octets en attente d'émission. Non nul quand le tampon du système a refusé.
  [[nodiscard]] std::size_t pending_output() const noexcept {
    return outgoing_.size() - sent_offset_;
  }

  // La raison du dernier statut non ouvert. Vide tant que rien n'a échoué.
  [[nodiscard]] const core::Error& error() const noexcept { return error_; }

  // Le socket sous-jacent, pour les tests qui doivent contraindre le tuyau.
  [[nodiscard]] const TcpSocket& socket() const noexcept { return socket_; }

 private:
  explicit Connection(TcpSocket socket) noexcept : socket_(std::move(socket)) {}

  // Rend faux si l'émission a échoué.
  [[nodiscard]] bool Flush();

  TcpSocket socket_;
  proto::FrameReader reader_;
  // Les trames rendues par `Requeue`, restituées avant celles du tampon.
  std::vector<Frame> requeued_;
  // Le dernier auteur de parole reçu, en attente de la trame qu'il décrit.
  std::vector<std::uint8_t> pending_author_;
  ReceiveObserver observer_;
  SendObserver send_observer_;
  SendGate send_gate_;
  std::vector<std::uint8_t> outgoing_;
  std::size_t sent_offset_ = 0;
  // Ce que la dernière réception a conclu, retenu jusqu'au découpage.
  LinkStatus received_ = LinkStatus::kOpen;
  core::Error error_ = core::Error{core::ErrorCode::kIoFailure, {}};
};

}  // namespace ro::net
