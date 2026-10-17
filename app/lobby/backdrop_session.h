#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/net/session.h"
#include "ro/sim/world.h"

#include "lobby/session_drive.h"

namespace moonclient::lobby {

// L'identifiant que toute session de décor présente.
//
// ⚠ CE N'EST PAS UN COMPTE : rien n'existe en base, et le serveur ne fait que
// prêter à la session un identifiant pris dans une plage réservée (voir
// `SPECTATOR_USERID`, src/common/mmo.hpp). Il n'y a donc ni inscription, ni
// ligne à effacer, ni orphelin après un plantage.
//
// ⚠ ET LE MOT DE PASSE EST IGNORÉ PAR LE SERVEUR, délibérément : il ne
// garderait rien — un spectateur ne parle pas, n'agit pas, ne se voit pas — et
// chaque client devrait le porter en clair. On n'envoie donc rien.
inline constexpr std::string_view kBackdropAccount = "moonlight_spectator";

// COMBIEN DE TEMPS LE DÉCOR RESTE VIVANT, compté depuis son entrée dans le
// monde.
//
// 🔴 LA MOITIÉ DE CE QUE LE SERVEUR TOLÈRE, ET L'ÉCART EST LE CONTRAT.
// `battle_config.spectator_session_ttl` vaut 600 s et compte du même instant :
// s'il frappait le premier, la coupure serait SUBIE, elle passerait par le
// chemin de détection de perte de lien, et elle ouvrirait un message par-dessus
// l'écran de connexion — au milieu de la saisie du joueur. En rendant la
// session avant, la scène se contente de garder son dernier instant, et le
// minuteur du serveur ne trouve plus personne à couper.
inline constexpr std::uint32_t kBackdropLifetimeMs = 300'000;

// AU-DELÀ, LE DÉCOR NE VIENDRA PAS, et l'on s'en tient au repli local.
//
// Trois actes, et un serveur qui répond le fait en quelques millisecondes : le
// budget d'un acte est de cinq secondes dans ce dépôt (voir `kPhaseTimeoutMs`,
// app/game/connected_session.cpp), donc quinze pour les trois. Ce qui dépasse
// n'est pas lent, c'est muet — et une session muette tient tout de même la
// seule place que `spectator_max_per_ip` accorde à cette adresse.
inline constexpr std::uint32_t kBackdropOpeningLimitMs = 15'000;

// LE TEMPS QU'ON LAISSE AU SERVEUR POUR REPRENDRE LA PLACE, avant de rouvrir.
//
// 🔴 FERMER NOTRE SOCKET NE SUFFIT PAS À LIBÉRER LA PLACE. Le serveur de CARTE
// doit s'apercevoir de la coupure, le dire au serveur de personnages, qui le
// dit au serveur de connexion ; tant que ce relais n'a pas couru, le login
// compte toujours le spectateur en ligne (`login_spectator_id_alive`,
// src/login/login.cpp) et `spectator_max_per_ip: 1` refuse la suivante par le
// code 3.
//
// ⚠ CE N'EST PAS CETTE VALEUR QUI RÈGLE LE REFUS, c'est le renvoi
// ci-dessous : le relais dépend de la charge du serveur, pas de nous. La pause
// n'a qu'à être du bon ordre de grandeur, et le journal dit combien d'essais
// il a fallu.
inline constexpr std::uint32_t kBackdropRelaunchPauseMs = 700;

// COMBIEN DE FOIS ON REMET LA VILLE EN JEU AVANT DE S'EN TENIR AU REPLI.
//
// ⚠ BORNÉ, PARCE QU'UN REFUS N'EST PAS TOUJOURS UNE COURSE : le code 3 dit
// aussi « décor éteint dans la configuration » et « tous les identifiants
// réservés sont pris », et insister n'y changerait rien. Trois essais espacés
// bornent l'attente à deux secondes, ce qui reste un changement de ville.
inline constexpr int kBackdropRelaunchTries = 3;

// L'ÉTAT DU DÉCOR, DU POINT DE VUE DE QUI LE DESSINE.
enum class BackdropState : std::uint8_t {
  // Rien n'a été tenté.
  kAsleep,
  // La session se monte. Le repli local tient l'écran en attendant : il n'y a
  // jamais d'instant où rien n'est montré.
  kOpening,
  // La ville est peuplée.
  kLive,
  // Le joueur a demandé une autre ville : l'ancienne session est rendue, et
  // l'on laisse au serveur le temps d'en reprendre la place avant de rouvrir.
  kRelaunching,
  // Plus de session, et la scène garde son dernier instant. Rendue par nous, ou
  // perdue — l'écran ne fait pas la différence, seul le journal la fait.
  kReleased,
  // Le décor vivant est hors de portée pour ce lancement : repli local,
  // définitivement.
  kUnavailable,
};

// LE DÉCOR VIVANT : une vraie session sur le serveur, derrière l'écran de
// connexion.
//
// Elle joue les mêmes trois actes qu'un joueur, avec l'identifiant du décor et
// l'emplacement zéro. Ce qu'elle rapporte est un MONDE — des passants, des
// monstres, des joueurs — que la coquille dessine à la place d'une illustration
// fixe.
//
// 🔴 ELLE NE PORTE AUCUNE POIGNÉE DE MAIN D'INTÉGRITÉ, et ce n'est pas un oubli.
// Le serveur EXEMPTE la session spectateur de ce contrôle ; lui en envoyer une
// serait se déclarer client de jeu sur une session qu'il traite autrement.
// C'est aussi pourquoi le décor ne peut pas être « une partie avec un
// drapeau » : `Game` émet la sienne dès que la liaison s'ouvre.
//
// ⚠ ET SES ÉCHECS SONT MUETS. Un décor est une décoration : qu'il manque ne
// regarde pas le joueur, qui a son formulaire et sa ville en repli. Le journal,
// lui, dit tout — c'est par là qu'on apprend que le serveur a refusé.
class BackdropSession {
 public:
  // LE SERVEUR EST CONNU DÈS LA NAISSANCE, et non à la première ouverture.
  //
  // 🔴 RETENU PAR `Open`, il manquerait quand le client démarre avec « Ville en
  // fond » décochée, puisque rien n'appelle alors `Open` : rallumer la case
  // partirait vers une adresse VIDE — `WSAEADDRNOTAVAIL` (10049), trois fois,
  // puis le repli local. Le recevoir UNE fois, là où l'on ne peut pas
  // l'oublier, ferme ce chemin.
  BackdropSession(std::string host, std::uint16_t port) : host_(std::move(host)), port_(port) {}

  // Demande la session. `now_ms` arme la borne d'ouverture.
  void Open(std::uint32_t now_ms);

  // Fait avancer la session, puis verse au monde ce qui est arrivé.
  void Advance(std::uint32_t now_ms);

  // Rend la session. VOLONTAIRE, donc silencieux.
  //
  // 🔴 C'EST LE GESTE CENTRAL DU MODULE, et il se joue à l'instant où le joueur
  // se connecte — jamais quand sa connexion réussit. Aucune session de décor ne
  // doit survivre au départ de la vraie : elle ne serait plus un décor mais une
  // session gratuite que personne ne regarde, et elle tiendrait la seule place
  // que le serveur accorde à cette adresse.
  //
  // Idempotent.
  void Release() noexcept;

  // RELANCE UNE SESSION, DONC UNE AUTRE VILLE, À LA DEMANDE DU JOUEUR.
  //
  // 🔴 C'EST L'EXCEPTION À « UNE SEULE FOIS PAR LANCEMENT », et elle est
  // délibérée. Cette garde existe pour qu'un décor ne reprenne pas
  // AUTOMATIQUEMENT la place que le serveur n'accorde qu'une fois par adresse
  // — à chaque échec de connexion —, ni ne change de ville sous le message
  // d'erreur qu'on est en train de lire. Ici c'est le joueur qui demande, et
  // il sait ce qu'il demande : le lieu est tiré par le serveur à l'ouverture
  // d'une session, en changer coûte d'en rouvrir une.
  //
  // ⚠ D'OÙ QU'ON PARTE, sauf d'une ouverture déjà en route. Deux gestes du
  // joueur y mènent, et ce sont les deux seuls : « Changer de vue » pendant
  // qu'une ville est vivante, et « Ville en fond » qu'on rallume après l'avoir
  // éteinte. Les distinguer aurait donné deux chemins pour une même demande.
  //
  // ⚠ ELLE NE REDEMANDE PAS L'ADRESSE : c'est le même serveur, celui que le
  // constructeur a reçu. La lui repasser serait une façon de plus de se tromper.
  //
  // ⚠ ET DEPUIS UNE VILLE VIVANTE, ELLE ATTEND avant de rouvrir — voir
  // `kBackdropRelaunchPauseMs`. Depuis un décor éteint il n'y a RIEN à rendre,
  // donc aucune place à laisser reprendre : on part tout de suite.
  void Relaunch(std::uint32_t now_ms);

  // CHANGE DE SERVEUR, quand le joueur en choisit un autre : la session en
  // cours se rend, le monde se vide, et toute ouverture suivante vise la
  // nouvelle adresse. Revient à `kAsleep`.
  //
  // ⚠ ELLE N'OUVRE RIEN : savoir si le joueur veut une ville regarde l'écran.
  void Retarget(std::string host, std::uint16_t port);

  [[nodiscard]] BackdropState state() const noexcept { return state_; }
  [[nodiscard]] const ro::sim::World& world() const noexcept { return world_; }

  // LE MONDE, MODIFIABLE, POUR QUI REND AU MONDE CE QUE L'IMAGE A CONSTATÉ.
  //
  // 🔴 SEULE LA COMPOSITION SAIT QUAND UNE AGONIE S'ACHÈVE — le serveur ne le
  // redit pas. Un décor qui n'aurait qu'une vue en lecture verrait ses morts
  // s'accumuler jusqu'à la fin de la session. Voir `SettleWorld`.
  [[nodiscard]] ro::sim::World& world() noexcept { return world_; }
  // La carte où le serveur a posé le décor. ⚠ Elle n'est connue qu'à l'entrée :
  // celle que la LISTE annonçait est une autre — le serveur tire un lieu au
  // sort à chacun des deux moments.
  [[nodiscard]] const std::string& map_name() const noexcept { return map_name_; }
  [[nodiscard]] const ro::net::MapPhase::Spawn& spawn() const noexcept { return spawn_; }

  // Vrai quand la liaison est tombée au lieu d'être rendue. N'a d'effet sur
  // aucun écran : c'est le témoin qui permet de l'éprouver, et de le journaliser.
  [[nodiscard]] bool lost() const noexcept { return lost_; }

 private:
  // Abandonne, en disant pourquoi au journal et rien au joueur.
  void GiveUp(std::string_view why);

  // Ouvre la session sur `host_` / `port_`. ⚠ DEUX APPELANTS : la première
  // ouverture, et chaque essai de relance.
  void Launch(std::uint32_t now_ms);

  SessionDrive drive_;
  BackdropState state_ = BackdropState::kAsleep;
  // Le serveur, reçu à la construction : toute ouverture s'y refait. Seul
  // `Retarget` le change.
  std::string host_;
  std::uint16_t port_;
  // Quand la relance a commencé à attendre, et combien d'essais il lui reste.
  // ⚠ ZÉRO HORS RELANCE : un refus ordinaire ne se retente pas.
  std::uint32_t relaunch_since_ms_ = 0;
  int relaunch_left_ = 0;
  bool lost_ = false;
  // La première figure a-t-elle été annoncée au journal ? Une seule fois par
  // session : ce qui compte est de savoir si la ville s'est peuplée du tout.
  bool peopled_told_ = false;
  // Quand l'ouverture a été demandée, et quand le monde est arrivé. Deux bornes
  // distinctes, parce qu'elles ne mesurent pas la même chose.
  std::uint32_t opening_since_ms_ = 0;
  std::uint32_t live_since_ms_ = 0;
  std::string map_name_;
  ro::net::MapPhase::Spawn spawn_;
  ro::net::Connection link_;
  ro::sim::World world_;
};

}  // namespace moonclient::lobby
