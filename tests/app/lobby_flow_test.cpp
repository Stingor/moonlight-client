#include "lobby/lobby_flow.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/ui_caps.h"
#include "ro/proto/common_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

#include "lobby/backdrop_session.h"
#include "support/lobby_realm.h"
#include "support/loopback_server.h"

using moonclient::lobby::BackdropState;
using moonclient::lobby::LobbyFlow;
using moonclient::lobby::LobbyScreenKind;
using ro::test::LoopbackServer;

using namespace lobby_test;

TEST_CASE("Lobby : la séquence complète mène à la remise", "[app][lobby]") {
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  CHECK(flow.screen() == LobbyScreenKind::kForm);

  flow.Submit("compte", "motdepasse");
  CHECK(flow.screen() == LobbyScreenKind::kConnecting);
  Pump(flow, clock_ms);

  REQUIRE(PlayLogin(realm, flow, clock_ms));
  REQUIRE(PlayCharacters(realm, flow, clock_ms, CharacterList()));
  REQUIRE(flow.screen() == LobbyScreenKind::kCharacters);
  REQUIRE(flow.characters().size() == 1);
  CHECK(flow.characters()[0].name == "Alice");

  flow.ChooseCharacter(0);
  CHECK(flow.screen() == LobbyScreenKind::kEntering);
  REQUIRE(PlayZone(realm, flow, clock_ms, "gonryun.gat"));
  Pump(flow, clock_ms);

  CHECK(flow.screen() == LobbyScreenKind::kHandoff);
  // Le serveur nomme la carte du nom de son fichier de marchabilité ; le reste
  // du client la désigne par le tronc commun de ses trois fichiers.
  CHECK(flow.map_name() == "gonryun");
  CHECK(flow.account().account_id == kAccountId);
  CHECK(flow.chosen_slot() == 0);
  CHECK(flow.spawn().start_tick == 555'000);

  // 🔴 ET LE PERSONNAGE CHOISI RÉPOND ENCORE, alors que la liste qui le portait
  // n'existe plus. `Advance` DÉTRUIT la phase des personnages en passant au
  // serveur de zone, et c'est exactement ici que l'appelant compose le monde de
  // départ. Tant que la réponse pointait dans cette liste, elle rendait nul —
  // l'écran rendait la main sans être « entré », `main` y lisait « le joueur a
  // fermé la fenêtre », et le client disparaissait EN SILENCE au clic sur un
  // personnage. C'est l'assertion qui manquait à cet essai-là.
  const ro::proto::CharacterInfo* chosen = flow.chosen_character();
  REQUIRE(chosen != nullptr);
  CHECK(chosen->name == "Alice");
  CHECK(chosen->CharNum == 0);

  // La liaison est remise ouverte : c'est dessus que le monde arrive.
  CHECK(flow.TakeLink().is_open());
}

TEST_CASE("Lobby : sur un compte à trou, c'est l'EMPLACEMENT qui désigne", "[app][lobby]") {
  // ⚠ L'ESSAI D'AU-DESSUS NE PEUT PAS VOIR CE DÉFAUT : avec un seul personnage
  // à l'emplacement zéro, rang et emplacement coïncident. Il faut un trou —
  // celui qu'une suppression laisse — pour que les deux divergent, et le
  // serveur résout toujours par l'emplacement (`char_num = slot`).
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.Submit("compte", "motdepasse");
  Pump(flow, clock_ms);
  REQUIRE(PlayLogin(realm, flow, clock_ms));
  REQUIRE(PlayCharacters(realm, flow, clock_ms, GappedCharacterList()));
  REQUIRE(flow.screen() == LobbyScreenKind::kCharacters);
  REQUIRE(flow.characters().size() == 2);

  // L'emplacement DEUX, qui est le rang UN. Se tromper ferait entrer en jeu
  // sous le nom, les niveaux et l'apparence de quelqu'un d'autre.
  flow.ChooseCharacter(2);
  REQUIRE(PlayZone(realm, flow, clock_ms, "gonryun.gat"));
  Pump(flow, clock_ms);

  REQUIRE(flow.screen() == LobbyScreenKind::kHandoff);
  const ro::proto::CharacterInfo* chosen = flow.chosen_character();
  REQUIRE(chosen != nullptr);
  CHECK(chosen->name == "Carole");
  CHECK(chosen->CharNum == 2);
}

TEST_CASE("Lobby : un tour entier sépare l'intention de l'appel qui bloque", "[app][lobby]") {
  // 🔴 L'OUVERTURE D'UNE SOCKET BLOQUE, et le cas qui coûte est celui du
  // serveur éteint : 5,2 s mesurées sur un port fermé, c'est-à-dire exactement
  // quand le joueur a besoin d'un message. Le formulaire a déjà été dessiné au
  // moment où le bouton est pressé ; sans ce tour d'attente, l'appel partirait
  // avant qu'aucune image ne porte « connexion… », et l'écran resterait figé
  // sur un formulaire qui a l'air de ne rien faire.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.Submit("compte", "motdepasse");
  CHECK(flow.screen() == LobbyScreenKind::kConnecting);

  // LE PREMIER TOUR N'OUVRE RIEN. Le serveur attend, et n'attrape personne.
  flow.Advance(clock_ms);
  CHECK_FALSE(realm.login.Accept());

  // LE SECOND OUVRE.
  clock_ms += kTurnMs;
  flow.Advance(clock_ms);
  CHECK(realm.login.Accept());
}

TEST_CASE("Lobby : le décor rend sa session AVANT que la vraie parte", "[app][lobby]") {
  // 🔴 À L'INSTANT OÙ LE JOUEUR SE CONNECTE, ET NON QUAND SA CONNEXION RÉUSSIT.
  // La preuve tient à ce que le banc ne fait rien : le serveur de connexion
  // n'accepte personne, donc la session du joueur n'a certainement pas abouti
  // — et le décor est pourtant déjà rendu. Rendre plus tard laisserait une
  // session de décor vivante derrière chaque tentative manquée, sur la seule
  // place que le serveur accorde à cette adresse.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);
  REQUIRE(flow.backdrop().state() == BackdropState::kLive);
  CHECK(flow.backdrop().map_name() == "prontera");

  flow.Submit("compte", "motdepasse");
  flow.Advance(clock_ms);
  clock_ms += kTurnMs;
  flow.Advance(clock_ms);

  CHECK(flow.backdrop().state() == BackdropState::kReleased);
  CHECK_FALSE(flow.backdrop().lost());
}

TEST_CASE("Lobby : changer de vue REND la liaison de carte", "[app][lobby]") {
  // 🔴 C'EST TOUT LE DÉFAUT, ET IL S'EST VU EN PRODUCTION. Une fois la ville
  // vivante, la liaison de zone n'est plus dans le pilote — elle lui a été
  // PRISE à l'entrée dans le monde. Rendre le pilote seul laissait donc la
  // socket de la carte ouverte : le serveur comptait toujours le spectateur en
  // ligne, `spectator_max_per_ip` vaut UN en production, et la session
  // suivante était refusée dix-huit millisecondes plus tard — pour de bon.
  //
  // ⚠ ET L'ESSAI NE PEUT PAS SE CONTENTER DE REGARDER L'ÉTAT : celui-ci passe
  // à la relance dans les deux cas. Ce qui distingue le correctif de son
  // absence est une SOCKET, et rien d'autre.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);
  REQUIRE(flow.backdrop().state() == BackdropState::kLive);

  flow.RelaunchBackdrop(clock_ms);
  CHECK(realm.zone.ClientHungUp());
}

TEST_CASE("Lobby : changer de vue attend avant de rouvrir", "[app][lobby]") {
  // ⚠ FERMER NOTRE SOCKET NE LIBÈRE PAS LA PLACE : le serveur de carte doit
  // s'en apercevoir et le relayer jusqu'au login. Rouvrir à l'image suivante
  // se ferait refuser par la place qu'on vient tout juste de rendre.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);

  flow.RelaunchBackdrop(clock_ms);
  const std::uint32_t asked_ms = clock_ms;
  flow.Advance(asked_ms + moonclient::lobby::kBackdropRelaunchPauseMs - 1);
  CHECK(flow.backdrop().state() == BackdropState::kRelaunching);

  flow.Advance(asked_ms + moonclient::lobby::kBackdropRelaunchPauseMs);
  CHECK(flow.backdrop().state() == BackdropState::kOpening);
}

TEST_CASE("Lobby : un décor sans serveur renonce au lieu d'accuser le réseau", "[app][lobby]") {
  // 🔴 C'EST LA PANNE TELLE QU'ELLE S'EST VUE À L'ÉCRAN. Une adresse vide ne se
  // refuse pas toute seule : elle traverse les trois essais de relance et trois
  // lignes de journal avant de rendre le repli local — en nommant
  // `WSAEADDRNOTAVAIL` (10049), c'est-à-dire une panne réseau qui n'existe pas.
  // Deux minutes de recherche du mauvais côté, pour un champ jamais rempli.
  //
  // ⚠ ÉPROUVÉE ICI ET NON PAR L'ARBITRE : celui-ci donne toujours son serveur
  // au décor, donc la garde y est injoignable. Elle protège d'une régression,
  // pas d'un cas courant — et une garde qu'aucun essai n'atteint est une garde
  // qu'on retire ou qu'on éprouve là où elle vit.
  moonclient::lobby::BackdropSession orphan(std::string(), 0);
  orphan.Open(1000);
  CHECK(orphan.state() == BackdropState::kUnavailable);
}

TEST_CASE("Lobby : rallumer un décor JAMAIS ouvert trouve quand même le serveur", "[app][lobby]") {
  // 🔴 PAYÉ À L'ÉCRAN, ET LE JOURNAL NOMMAIT UNE PANNE QUI N'EXISTAIT PAS. Le
  // serveur était retenu par la PREMIÈRE ouverture — que rien n'appelle quand
  // le client démarre avec « Ville en fond » décochée. Rallumer la case partait
  // alors vers une adresse VIDE : `WSAEADDRNOTAVAIL` (10049), trois essais de
  // relance, puis le repli local, en accusant le réseau.
  //
  // ⚠ L'ESSAI SUIVANT NE POUVAIT PAS LE VOIR : il monte le décor d'abord, donc
  // le serveur y est déjà retenu. C'est le décor JAMAIS ouvert qui sépare les
  // deux, et c'est exactement le client démarré sans sa ville.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(flow.backdrop().state() == BackdropState::kAsleep);

  flow.RelaunchBackdrop(clock_ms);
  Pump(flow, clock_ms);
  CHECK(realm.login.Accept());
}

TEST_CASE("Lobby : rallumer la ville repart sans attendre", "[app][lobby]") {
  // 🔴 LA PAUSE NE SERT QU'À LAISSER LE SERVEUR REPRENDRE UNE PLACE QU'ON VIENT
  // DE RENDRE. Le joueur qui rallume « Ville en fond » n'a rien rendu à cet
  // instant — sa session a été fermée quand il l'a éteinte —, et lui faire
  // attendre serait une seconde offerte à personne.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);

  // Le joueur décoche : la session est rendue.
  flow.backdrop().Release();
  REQUIRE(flow.backdrop().state() == BackdropState::kReleased);

  // Il recoche : l'ouverture part dans la foulée, sans passer par l'attente.
  flow.RelaunchBackdrop(clock_ms);
  CHECK(flow.backdrop().state() == BackdropState::kOpening);
}

TEST_CASE("Lobby : deux demandes de suite n'ouvrent pas deux sessions", "[app][lobby]") {
  // 🔴 DEUX CLICS VALENT UN, et l'oublier coûterait la session qu'on vient
  // d'ouvrir : relancer une seconde fois rendrait la première — donc fermerait
  // sa liaison — pour en rouvrir une autre, qui se ferait refuser par la place
  // que la première n'a pas encore rendue.
  //
  // ⚠ CE QUI LE PROUVE EST UNE SOCKET, pas un état : les deux chemins
  // laisseraient l'écran sur « en cours d'ouverture ».
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);
  flow.backdrop().Release();

  flow.RelaunchBackdrop(clock_ms);
  Pump(flow, clock_ms);
  REQUIRE(realm.login.Accept());

  flow.RelaunchBackdrop(clock_ms);
  Pump(flow, clock_ms);
  CHECK_FALSE(realm.login.ClientHungUp(200));
}

TEST_CASE("Lobby : se connecter annule une relance en attente", "[app][lobby]") {
  // 🔴 LE PIÈGE QUE LA RELANCE INTRODUIT, et il est pire que le défaut qu'elle
  // corrige : un minuteur laissé courir rouvrirait un décor PENDANT que le
  // joueur se connecte, sur la seule place que le serveur accorde à cette
  // adresse — et c'est la connexion du JOUEUR qui serait refusée.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);

  flow.RelaunchBackdrop(clock_ms);
  REQUIRE(flow.backdrop().state() == BackdropState::kRelaunching);

  // ⚠ DEUX TOURS : la demande attend une image avant de partir, pour que
  // l'écran ait le temps d'annoncer « connexion… » avant l'appel qui bloque.
  // C'est au DÉPART de la vraie session que le décor se rend.
  flow.Submit("compte", "motdepasse");
  flow.Advance(clock_ms);
  clock_ms += kTurnMs;
  flow.Advance(clock_ms);
  CHECK(flow.backdrop().state() == BackdropState::kReleased);

  // Bien après l'heure : la relance ne doit plus jamais partir.
  flow.Advance(clock_ms + moonclient::lobby::kBackdropRelaunchPauseMs * 4);
  CHECK(flow.backdrop().state() == BackdropState::kReleased);
}

TEST_CASE("Lobby : un refus revient au formulaire, avec son motif", "[app][lobby]") {
  constexpr std::uint32_t kWrongPassword = 1;
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.Submit("compte", "faux");
  Pump(flow, clock_ms);

  REQUIRE(realm.login.Accept());
  REQUIRE(realm.login.Receive(ro::proto::CaLogin::kWireSize).size() ==
          ro::proto::CaLogin::kWireSize);
  ro::proto::AcRefuseLogin refusal;
  refusal.error = kWrongPassword;
  REQUIRE(realm.login.Send(Encode(refusal)));
  Pump(flow, clock_ms);

  CHECK(flow.screen() == LobbyScreenKind::kForm);
  // ⚠ LE MOTIF EN CLAIR ET NON UN NUMÉRO : le code nu reste lisible pour qui a
  // les sources du serveur, ce qui n'est pas le joueur.
  CHECK(flow.message().find("mot de passe") != std::string::npos);
}

TEST_CASE("Lobby : un serveur qui se tait à l'entrée en jeu ramène au formulaire", "[app][lobby]") {
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.Submit("compte", "motdepasse");
  Pump(flow, clock_ms);
  REQUIRE(PlayLogin(realm, flow, clock_ms));
  REQUIRE(PlayCharacters(realm, flow, clock_ms, CharacterList()));
  REQUIRE(flow.screen() == LobbyScreenKind::kCharacters);

  // Le serveur de personnages reçoit la demande et ne répond jamais : c'est un
  // serveur de zone qui n'a pas fini de démarrer.
  flow.ChooseCharacter(0);
  Pump(flow, clock_ms);
  REQUIRE(flow.screen() == LobbyScreenKind::kEntering);

  // À mi-délai, on attend encore ; au-delà, c'est une panne.
  clock_ms += moonclient::lobby::kStageTimeoutMs / 2;
  Pump(flow, clock_ms);
  CHECK(flow.screen() == LobbyScreenKind::kEntering);

  clock_ms += moonclient::lobby::kStageTimeoutMs;
  Pump(flow, clock_ms);
  CHECK(flow.screen() == LobbyScreenKind::kForm);
  CHECK(flow.message().find("ne répond pas") != std::string::npos);
}

TEST_CASE("Lobby : un compte SANS personnage atteint quand même sa sélection", "[app][lobby]") {
  // C'est le joueur qui vient de s'inscrire, et l'écran doit lui proposer de
  // créer. Une liste vide est une RÉPONSE, pas une absence de réponse.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.Submit("neuf", "motdepasse");
  Pump(flow, clock_ms);
  REQUIRE(PlayLogin(realm, flow, clock_ms));
  REQUIRE(PlayCharacters(realm, flow, clock_ms, EmptyCharacterList()));

  CHECK(flow.screen() == LobbyScreenKind::kCharacters);
  CHECK(flow.characters().empty());
  CHECK(flow.message().empty());
}

TEST_CASE("Lobby : le décor se nomme sans MachineGuid, puis n'annonce que les calques",
          "[app][lobby][styles]") {
  // 🔴 LE DÉCOR N'EST PAS LE JOUEUR : sa poignée de main ne porte pas le
  // MachineGuid de la machine, et son annonce ne dit qu'une chose, qu'il sait
  // lire les teintures de calques. C'est tout ce qu'un serveur donne à une
  // session spectateur : le droit de voir les couleurs des joueurs.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;
  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);
  Pump(flow, clock_ms);
  constexpr std::size_t kListenBytes = 4096;
  constexpr int kListenMs = 200;
  const std::vector<std::uint8_t> sent = realm.zone.Receive(kListenBytes, kListenMs);

  const auto word_at = [&sent](std::size_t at) {
    return static_cast<std::uint16_t>(sent[at] | (sent[at + 1] << 8U));
  };
  // La première trame d'un opcode et d'une longueur donnés, à partir de `from`.
  const auto find = [&](std::uint16_t opcode, std::size_t size, std::size_t from) {
    for (std::size_t at = from; at + size <= sent.size(); ++at) {
      if (word_at(at) == opcode && word_at(at + 2) == size) {
        return at;
      }
    }
    return sent.size();
  };
  constexpr std::size_t kGuidOffset = 4;
  constexpr std::size_t kGuidChars = 36;
  constexpr std::size_t kCapsOffset = 4;
  const std::size_t handshake = find(ro::proto::CzMoonclientIntegrity::kOpcode,
                                     ro::proto::CzMoonclientIntegrity::kWireSize, 0);
  REQUIRE(handshake < sent.size());
  for (std::size_t at = 0; at < kGuidChars; ++at) {
    CHECK(sent[handshake + kGuidOffset + at] == 0);
  }
  const std::size_t announce =
      find(ro::proto::CzMoonlightUiCaps::kOpcode, ro::proto::CzMoonlightUiCaps::kWireSize,
           handshake + ro::proto::CzMoonclientIntegrity::kWireSize);
  REQUIRE(announce < sent.size());
  const std::uint32_t caps =
      static_cast<std::uint32_t>(word_at(announce + kCapsOffset)) |
      (static_cast<std::uint32_t>(word_at(announce + kCapsOffset + 2)) << 16U);
  CHECK(caps == ro::net::kUiCapStyleLayers);
}

TEST_CASE("Lobby : le décor se rend de lui-même à sa borne", "[app][lobby]") {
  // 🔴 LA MOITIÉ DE CE QUE LE SERVEUR TOLÈRE, ET L'ÉCART EST LE CONTRAT.
  // `spectator_session_ttl` vaut 600 s et compte du même instant. S'il frappait
  // le premier, la coupure serait SUBIE et ouvrirait un message par-dessus le
  // formulaire, au milieu de la saisie.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  const std::uint32_t live_since_ms = RaiseBackdrop(realm, flow, clock_ms);
  REQUIRE(live_since_ms != 0);
  REQUIRE(flow.backdrop().state() == BackdropState::kLive);

  flow.Advance(live_since_ms + moonclient::lobby::kBackdropLifetimeMs - 1);
  CHECK(flow.backdrop().state() == BackdropState::kLive);

  flow.Advance(live_since_ms + moonclient::lobby::kBackdropLifetimeMs);
  CHECK(flow.backdrop().state() == BackdropState::kReleased);
  // Rendue, pas perdue : c'est toute la différence, et elle ne se voit que là.
  CHECK_FALSE(flow.backdrop().lost());
}

TEST_CASE("Lobby : la perte du décor ne dit rien au joueur", "[app][lobby]") {
  // Un décor est une décoration. Qu'il meure ne regarde pas qui remplit un
  // formulaire : l'écran garde son dernier instant, et le journal seul en parle.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  REQUIRE(RaiseBackdrop(realm, flow, clock_ms) != 0);
  REQUIRE(flow.backdrop().state() == BackdropState::kLive);

  realm.zone.CloseClient();
  Pump(flow, clock_ms);

  CHECK(flow.backdrop().state() == BackdropState::kReleased);
  CHECK(flow.backdrop().lost());
  // Rien n'a bougé du côté du joueur : ni l'écran, ni le message.
  CHECK(flow.screen() == LobbyScreenKind::kForm);
  CHECK(flow.message().empty());
}

TEST_CASE("Lobby : un décor refusé laisse le repli local, en silence", "[app][lobby]") {
  // En production `spectator_max_per_ip` vaut UN : le second client de la même
  // adresse se fait refuser, par le code 3 du serveur de connexion. Ce refus
  // n'est pas celui du joueur, et il ne doit pas atteindre son écran.
  constexpr std::uint32_t kRejectedFromServer = 3;
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.OpenBackdrop(clock_ms);
  Pump(flow, clock_ms);

  REQUIRE(realm.login.Accept());
  REQUIRE(realm.login.Receive(ro::proto::CaLogin::kWireSize).size() ==
          ro::proto::CaLogin::kWireSize);
  ro::proto::AcRefuseLogin refusal;
  refusal.error = kRejectedFromServer;
  REQUIRE(realm.login.Send(Encode(refusal)));
  Pump(flow, clock_ms);

  CHECK(flow.backdrop().state() == BackdropState::kUnavailable);
  CHECK(flow.screen() == LobbyScreenKind::kForm);
  CHECK(flow.message().empty());
}

TEST_CASE("Lobby : un décor muet est abandonné à sa borne d'ouverture", "[app][lobby]") {
  // Le cas qui coûte : un serveur qui accepte la liaison puis ne répond plus.
  // Les phases attendraient sans fin, et cette session-là tiendrait tout de
  // même la seule place que l'adresse obtient.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  // ⚠ L'INSTANT DE LA DEMANDE, RELEVÉ ICI : la borne se compte depuis lui, et
  // le `Pump` qui suit fait avancer l'horloge de plusieurs tours.
  const std::uint32_t opened_at_ms = clock_ms;
  flow.OpenBackdrop(clock_ms);
  Pump(flow, clock_ms);
  REQUIRE(realm.login.Accept());
  REQUIRE(flow.backdrop().state() == BackdropState::kOpening);

  flow.Advance(opened_at_ms + moonclient::lobby::kBackdropOpeningLimitMs - 1);
  CHECK(flow.backdrop().state() == BackdropState::kOpening);

  flow.Advance(opened_at_ms + moonclient::lobby::kBackdropOpeningLimitMs);
  CHECK(flow.backdrop().state() == BackdropState::kUnavailable);
  CHECK(flow.message().empty());
}

TEST_CASE("Lobby : le retour au formulaire rend la session", "[app][lobby]") {
  // ⚠ LA LISTE NE SURVIT PAS AU RETOUR, et c'est la vérité du protocole : elle
  // venait d'une liaison qu'on referme. Y revenir demande une connexion neuve.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow flow("127.0.0.1", realm.login.port());
  flow.Submit("compte", "motdepasse");
  Pump(flow, clock_ms);
  REQUIRE(PlayLogin(realm, flow, clock_ms));
  REQUIRE(PlayCharacters(realm, flow, clock_ms, CharacterList()));
  REQUIRE(flow.screen() == LobbyScreenKind::kCharacters);

  flow.Back();
  CHECK(flow.screen() == LobbyScreenKind::kForm);
  CHECK(flow.characters().empty());
  CHECK(flow.message().empty());
}

TEST_CASE("Lobby : le retour depuis la partie reprend au serveur de personnages", "[app][lobby]") {
  // 🔴 NI FORMULAIRE, NI MOT DE PASSE, NI SERVEUR DE CONNEXION : le serveur de
  // carte a rendu les jetons au serveur de personnages, et c'est avec eux qu'on
  // s'y représente.
  Realm realm;
  REQUIRE(realm.listening());
  std::uint32_t clock_ms = 1000;

  LobbyFlow first("127.0.0.1", realm.login.port());
  first.Submit("compte", "motdepasse");
  Pump(first, clock_ms);
  REQUIRE(PlayLogin(realm, first, clock_ms));
  REQUIRE(PlayCharacters(realm, first, clock_ms, CharacterList()));
  first.ChooseCharacter(0);
  REQUIRE(PlayZone(realm, first, clock_ms, "gonryun.gat"));
  Pump(first, clock_ms);
  REQUIRE(first.screen() == LobbyScreenKind::kHandoff);

  // Pris à la remise, comme le fait l'écran.
  const moonclient::lobby::Comeback comeback = first.comeback();
  CHECK(comeback.usable());
  CHECK(comeback.character_server.port == realm.characters.port());
  CHECK(comeback.account.login_id1 == 111);
  CHECK(comeback.account.login_id2 == 222);
  first.Release();

  LobbyFlow back("127.0.0.1", realm.login.port());
  back.Resume(comeback);
  CHECK(back.screen() == LobbyScreenKind::kConnecting);
  Pump(back, clock_ms);

  REQUIRE(realm.characters.Accept());
  const std::vector<std::uint8_t> enter = realm.characters.Receive(kEnterRequestSize);
  REQUIRE(enter.size() == kEnterRequestSize);
  // L'opcode, puis le compte et les DEUX JETONS de la connexion d'origine.
  CHECK(enter[0] == kEnterOpcodeLow);
  CHECK(std::vector<std::uint8_t>(enter.begin() + kEnterAccountOffset,
                                  enter.begin() + kEnterFirstTokenOffset) ==
        BareAccountId(kAccountId));
  CHECK(enter[kEnterFirstTokenOffset] == 111);
  CHECK(enter[kEnterSecondTokenOffset] == 222);
  REQUIRE(realm.characters.Send(BareAccountId(kAccountId)));
  REQUIRE(realm.characters.Send(CharacterList()));
  Pump(back, clock_ms);

  CHECK(back.screen() == LobbyScreenKind::kCharacters);
  REQUIRE(back.characters().size() == 1);
  CHECK(back.characters()[0].name == "Alice");
  CHECK(back.account().account_id == kAccountId);
}

TEST_CASE("Lobby : un retour sans jetons ne part pas", "[app][lobby]") {
  // Rien à présenter au serveur de personnages : le formulaire reste.
  LobbyFlow flow("127.0.0.1", 1);
  flow.Resume(moonclient::lobby::Comeback{});
  CHECK(flow.screen() == LobbyScreenKind::kForm);
}

TEST_CASE("Lobby : un retour refusé ramène au formulaire, motif en clair", "[app][lobby]") {
  // Le serveur de personnages a disparu entre-temps : le joueur n'est pas
  // bloqué, il retombe sur le formulaire.
  std::uint32_t clock_ms = 1000;
  moonclient::lobby::Comeback comeback;
  comeback.account.account_id = kAccountId;
  comeback.character_server = ro::net::ServerAddress{kLoopbackWireIp, 1};

  LobbyFlow flow("127.0.0.1", 1);
  flow.Resume(comeback);
  Pump(flow, clock_ms);
  CHECK(flow.screen() == LobbyScreenKind::kForm);
  CHECK_FALSE(flow.message().empty());
}

TEST_CASE("Lobby : le motif d'une liaison perdue se lit sous le formulaire, puis s'efface",
          "[app][lobby]") {
  // LE RETOUR FORCÉ DEPUIS LA PARTIE arrive sur un formulaire neuf : le motif
  // doit y être lu, par le même canal qu'un refus.
  LobbyFlow flow("127.0.0.1", 1);
  const std::string why = "Déconnecté du serveur.\nLe serveur ne répond plus.";
  flow.Notify(why);
  CHECK(flow.screen() == LobbyScreenKind::kForm);
  CHECK(flow.message() == why);

  // ⚠ ET IL S'EFFACE À LA TENTATIVE SUIVANTE : un motif qui survit à son écran
  // devient un mensonge.
  flow.Submit("compte", "motdepasse");
  CHECK(flow.message().empty());

  // HORS DU FORMULAIRE, RIEN : l'attente n'a pas de motif à montrer.
  flow.Notify(why);
  CHECK(flow.message().empty());
}
