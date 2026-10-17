#include "lobby/backdrop_session.h"

#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/net/world_update.h"
#include "ro/sim/world.h"

namespace moonclient::lobby {
namespace {

// L'emplacement du personnage du décor. Le serveur n'en donne qu'un, et il le
// nomme « Spectator ».
constexpr std::uint8_t kBackdropSlot = 0;

// QUI PEUPLE LA VILLE, PAR ESPÈCE.
//
// 🔴 UN TOTAL NE SUFFIT PAS, et l'avoir cru a produit une affirmation fausse :
// « six figures sur niflheim » ne dit pas si ce sont des passants ou des
// monstres, et niflheim en a des deux. Le relevé qui accompagnait cette ligne
// portait sur quatre villes SANS monstres — izlude, geffen, veins, eclage —,
// et l'on en avait conclu que le décor n'en montrait jamais. Le joueur, qui
// regardait l'écran, en voyait.
std::string DescribePopulation(const ro::sim::World& world) {
  std::size_t players = 0;
  std::size_t monsters = 0;
  std::size_t others = 0;
  for (const auto& entry : world.actors()) {
    switch (entry.second.kind) {
      case ro::sim::ActorKind::kPlayer:
        ++players;
        break;
      case ro::sim::ActorKind::kMonster:
        ++monsters;
        break;
      default:
        ++others;
        break;
    }
  }
  return std::to_string(players) + " joueur(s), " + std::to_string(monsters) + " monstre(s), " +
         std::to_string(others) + " autre(s)";
}

}  // namespace

void BackdropSession::Launch(std::uint32_t now_ms) {
  // 🔴 SANS SERVEUR, ON NE PART PAS. Une adresse vide n'échoue pas en silence :
  // elle traverse trois essais de relance et trois lignes de journal avant de
  // rendre le repli local, en nommant une panne réseau qui n'existe pas.
  if (host_.empty() || port_ == 0) {
    GiveUp("aucun serveur n'a été donné au décor");
    return;
  }
  opening_since_ms_ = now_ms;
  state_ = BackdropState::kOpening;
  drive_.Begin(Credentials{
      .host = host_, .port = port_, .account = std::string(kBackdropAccount), .password = {}});
}

void BackdropSession::Open(std::uint32_t now_ms) {
  if (state_ != BackdropState::kAsleep) {
    return;
  }
  Launch(now_ms);
}

void BackdropSession::Relaunch(std::uint32_t now_ms) {
  // ⚠ DÉJÀ EN ROUTE : deux clics de suite ne doivent pas ouvrir deux sessions,
  // dont la seconde se ferait refuser par la première.
  if (state_ == BackdropState::kOpening || state_ == BackdropState::kRelaunching) {
    return;
  }
  const bool was_live = state_ == BackdropState::kLive;
  // ⚠ DEUX APPELS ET NON UN TERNAIRE : le motif d'un message de journal est
  // vérifié à la COMPILATION, et une expression n'en est pas un.
  if (was_live) {
    ro::core::LogInfo("décor : une autre ville est demandée — la session se rouvre");
  } else {
    ro::core::LogInfo("décor : rallumé par le joueur — une ville est demandée");
  }
  // 🔴 `Release` ET NON `drive_.Release()` SEUL, ET L'ÉCART VALAIT LE BOUTON.
  // Une fois la ville vivante, la liaison de zone N'EST PLUS DANS LE PILOTE :
  // elle lui a été PRISE (`TakeLink`) à l'entrée dans le monde. Rendre le
  // pilote laissait donc la socket de la carte grande ouverte — le serveur
  // comptait toujours le spectateur en ligne, et le login refusait la session
  // suivante en dix-huit millisecondes, une fois pour toutes.
  Release();
  lost_ = false;
  peopled_told_ = false;
  map_name_.clear();
  world_ = ro::sim::World();
  relaunch_left_ = kBackdropRelaunchTries;
  if (!was_live) {
    // 🔴 RIEN À RENDRE, DONC RIEN À ATTENDRE. La pause existe pour laisser le
    // serveur reprendre la place d'une session qu'on vient de fermer ; sans
    // session fermée, elle ne serait qu'une seconde d'attente offerte.
    state_ = BackdropState::kAsleep;
    Launch(now_ms);
    return;
  }
  relaunch_since_ms_ = now_ms;
  state_ = BackdropState::kRelaunching;
}

void BackdropSession::Retarget(std::string host, std::uint16_t port) {
  Release();
  host_ = std::move(host);
  port_ = port;
  lost_ = false;
  peopled_told_ = false;
  relaunch_left_ = 0;
  map_name_.clear();
  world_ = ro::sim::World();
  state_ = BackdropState::kAsleep;
}

void BackdropSession::GiveUp(std::string_view why) {
  // ⚠ AU JOURNAL ET NULLE PART AILLEURS. Le refus du serveur est une
  // information d'exploitation — décor éteint, ou place déjà prise par un autre
  // client de cette adresse —, jamais un message pour qui remplit un
  // formulaire.
  ro::core::LogInfo("décor : {} — l'écran de connexion garde son fond local", why);
  drive_.Release();
  link_.Close();
  state_ = BackdropState::kUnavailable;
}

void BackdropSession::Advance(std::uint32_t now_ms) {
  switch (state_) {
    case BackdropState::kAsleep:
    case BackdropState::kReleased:
    case BackdropState::kUnavailable:
      return;

    case BackdropState::kRelaunching:
      if (now_ms - relaunch_since_ms_ < kBackdropRelaunchPauseMs) {
        return;
      }
      // ⚠ REMIS À L'ENDORMISSEMENT D'ABORD : c'est le verrou qui interdit la
      // relance AUTOMATIQUE, et `Launch` ne le lit pas.
      state_ = BackdropState::kAsleep;
      Launch(now_ms);
      return;

    case BackdropState::kOpening: {
      drive_.Advance(now_ms);
      switch (drive_.stage()) {
        case SessionDrive::Stage::kChoosing:
          // L'emplacement zéro, sans rien demander à personne : le décor n'a
          // qu'un personnage, et c'est le seul choix qui existe.
          drive_.Choose(kBackdropSlot);
          return;
        case SessionDrive::Stage::kInWorld:
          map_name_ = drive_.map_name();
          spawn_ = drive_.spawn();
          link_ = drive_.TakeLink();
          live_since_ms_ = now_ms;
          state_ = BackdropState::kLive;
          // ⚠ LA RELANCE EST FINIE : ce qui arrivera à CETTE session — une
          // perte, un refus plus tard — n'a pas à se retenter tout seul.
          relaunch_left_ = 0;
          ro::core::LogInfo("décor : la ville de {} est vivante", map_name_);
          return;
        case SessionDrive::Stage::kRefused:
        case SessionDrive::Stage::kFailed:
          // 🔴 UN REFUS PENDANT UNE RELANCE SE RETENTE, et c'est le cas
          // ordinaire : le serveur n'a pas encore repris la place de la
          // session qu'on vient de rendre. Voir `kBackdropRelaunchPauseMs`.
          if (relaunch_left_ > 0) {
            --relaunch_left_;
            ro::core::LogInfo("décor : {} — on réessaie dans {} ms ({} essai(s) après)",
                              drive_.message(), kBackdropRelaunchPauseMs, relaunch_left_);
            drive_.Release();
            link_.Close();
            relaunch_since_ms_ = now_ms;
            state_ = BackdropState::kRelaunching;
            return;
          }
          GiveUp(drive_.message());
          return;
        default:
          break;
      }
      // 🔴 LE MUET EST LE CAS QUI COÛTE. Un serveur qui accepte la liaison puis
      // ne répond plus laisse les phases attendre sans fin, et cette session-là
      // tient tout de même la seule place que l'adresse obtient.
      if (now_ms - opening_since_ms_ >= kBackdropOpeningLimitMs) {
        GiveUp("le serveur n'a pas monté la session à temps");
      }
      return;
    }

    case BackdropState::kLive: {
      // LA BORNE D'ABORD, LE MONDE ENSUITE. Voir `kBackdropLifetimeMs` : ce qui
      // compte est de rendre la session AVANT que le serveur ne la coupe, et
      // une image de plus ne vaut pas le message d'erreur qu'elle risquerait.
      if (now_ms - live_since_ms_ >= kBackdropLifetimeMs) {
        // ⚠ LE COMPTE QUI VAUT SE DIT ICI : c'est le seul instant où l'on sait
        // ce que la ville aura montré en entier, monstres et joueurs entrés
        // dans le champ compris. Celui de la première salve ne porte que les
        // PNJ, qui sont les seuls à ne jamais en sortir.
        ro::core::LogInfo(
            "décor : session rendue après {} s — {}, la scène garde son dernier instant",
            kBackdropLifetimeMs / 1000, DescribePopulation(world_));
        Release();
        return;
      }
      ro::net::Connection::Polled polled = link_.Poll();
      for (const ro::net::Frame& frame : polled.frames) {
        static_cast<void>(ro::net::ApplyFrame(frame, world_, now_ms));
      }
      // 🔴 LA PREMIÈRE ARRIVÉE SE DIT, UNE FOIS. Une ville vide ressemble trait
      // pour trait à un décor cassé : sans cette ligne, « je ne vois personne »
      // ne distingue pas « le serveur ne nous envoie rien » de « il n'y avait
      // personne à cet endroit-là ».
      //
      // ⚠ ET ELLE NE DIT QUE LA PREMIÈRE SALVE. Ce que le serveur envoie à
      // l'entrée est ce qui se trouve dans le rayon de vue À CET INSTANT : les
      // PNJ, qui ne bougent pas, y sont tous ; ce qui marche n'y est que s'il
      // s'y trouvait. Le vrai compte se dit à la fin — voir la ligne de
      // remise, et `DescribePopulation`.
      if (!peopled_told_ && !world_.actors().empty()) {
        peopled_told_ = true;
        ro::core::LogInfo("décor : première salve sur {} — {}", map_name_,
                          DescribePopulation(world_));
      }
      if (polled.status != ro::net::LinkStatus::kOpen) {
        // ⚠ SUBIE, ET C'EST TOUT CE QUE CELA CHANGE : l'écran fait la même
        // chose — il garde son dernier instant. Mais que le serveur ait coupé
        // le premier veut dire que notre borne n'a pas tenu, et cela se dit.
        lost_ = true;
        link_.Close();
        state_ = BackdropState::kReleased;
        ro::core::LogWarning("décor : le serveur a coupé avant nous — la scène se fige");
      }
      return;
    }
  }
}

void BackdropSession::Release() noexcept {
  // 🔴 UNE RELANCE EN ATTENTE S'ANNULE, et l'oublier serait pire que le défaut
  // qu'elle corrige : le minuteur rouvrirait un décor PENDANT que le joueur se
  // connecte, sur la seule place que le serveur accorde à cette adresse — et
  // c'est sa connexion à lui qui serait refusée.
  if (state_ == BackdropState::kRelaunching) {
    relaunch_left_ = 0;
    state_ = BackdropState::kReleased;
    return;
  }
  if (state_ != BackdropState::kOpening && state_ != BackdropState::kLive) {
    return;
  }
  drive_.Release();
  link_.Close();
  state_ = BackdropState::kReleased;
}

}  // namespace moonclient::lobby
