#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::test {

// Épingle le tampon d'émission d'un socket, et rend faux si le système refuse.
//
// Windows agrandit ce tampon tout seul quand une écriture le dépasse : il a
// avalé deux mégaoctets d'un coup vers un pair qui n'annonçait que quatre
// kilooctets de fenêtre. Sans épinglage, il n'existe donc AUCUN volume qui
// rende l'envoi partiel certain, et le chemin de reprise ne serait éprouvé que
// par chance. Une valeur non nulle désactive cet agrandissement.
bool LimitSendBuffer(std::uintptr_t socket, int bytes);

// Un serveur TCP minimal sur la boucle locale, pour éprouver le transport.
//
// POURQUOI UN VRAI SOCKET PLUTÔT QU'UNE SIMULATION
// Ce qu'on veut vérifier ici, c'est justement ce qu'une simulation efface : une
// lecture qui rend moins d'octets qu'il n'en circule, une écriture partielle,
// une fermeture par le pair. Un faux socket qui rendrait toujours tout ne
// prouverait que la cohérence du faux.
//
// Le port est choisi par le système (bind sur 0) puis relu : deux exécutions
// simultanées ne peuvent pas se disputer un numéro fixé d'avance.
class LoopbackServer {
 public:
  // `receive_buffer_bytes` rétrécit le tampon de réception annoncé au client ;
  // zéro laisse celui du système.
  //
  // C'est le seul moyen SÛR de provoquer un envoi partiel côté client : sur une
  // boucle locale, le système absorbe volontiers plusieurs mégaoctets d'un
  // coup, et une taille assez grande pour le saturer sur une machine ne l'est
  // pas forcément sur une autre. Contraindre le tuyau depuis le banc d'essai
  // rend le cas certain au lieu de le rendre probable.
  explicit LoopbackServer(int receive_buffer_bytes = 0);
  ~LoopbackServer();

  LoopbackServer(const LoopbackServer&) = delete;
  LoopbackServer& operator=(const LoopbackServer&) = delete;

  [[nodiscard]] bool listening() const noexcept;
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

  // Accepte le client en attente. À appeler après la connexion du client.
  [[nodiscard]] bool Accept();

  // Envoie des octets au client, en insistant jusqu'à ce que tout soit parti.
  [[nodiscard]] bool Send(std::span<const std::uint8_t> bytes);

  // Lit jusqu'à `count` octets, en attendant au plus le délai donné.
  [[nodiscard]] std::vector<std::uint8_t> Receive(std::size_t count, int timeout_ms = 2000);

  // LE CLIENT A-T-IL RACCROCHÉ ?
  //
  // 🔴 C'EST LA SEULE FAÇON D'ÉPROUVER QU'UNE SOCKET A ÉTÉ FERMÉE, et son
  // absence a laissé passer un défaut : `Receive` rend un tampon vide aussi
  // bien pour « le pair a raccroché » que pour « le pair n'a rien dit », donc
  // il ne sait pas distinguer une liaison rendue d'une liaison oubliée
  // grande ouverte.
  //
  // Rend vrai sur une fermeture ordonnée ou une coupure, faux si le délai
  // passe sans rien — ou si le client a envoyé des octets, ce qui prouve
  // justement qu'il est encore là.
  //
  // ⚠ ELLE CONSOMME CE QUI RESTAIT À LIRE, et elle le doit : des octets en
  // attente ne disent pas que le pair est vivant, seulement qu'il a parlé
  // avant de partir. La fin du flux est derrière eux.
  [[nodiscard]] bool ClientHungUp(int timeout_ms = 2000);

  // Ferme le lien avec le client, en laissant le serveur à l'écoute.
  void CloseClient() noexcept;

 private:
  // Descripteurs natifs, gardés en entier non signé pour porter aussi bien le
  // SOCKET de Windows que le int de POSIX sans exposer <winsock2.h> ici.
  std::uintptr_t listener_ = static_cast<std::uintptr_t>(-1);
  std::uintptr_t client_ = static_cast<std::uintptr_t>(-1);
  std::uint16_t port_ = 0;
  int receive_buffer_bytes_ = 0;
};

}  // namespace ro::test
