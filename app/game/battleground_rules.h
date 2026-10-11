#pragma once

#include <span>
#include <string>

#include "ro/sim/entry_queue.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/status_effects.h"

#include "game/battleground_file.h"
#include "game/interface_sound.h"

namespace moonclient {

// ── LES RÈGLES PURES DU CHAMP DE BATAILLE ───────────────────────────────────
//
// Ce que l'étage décide sans rien envoyer ni rien dessiner : s'il refuse une
// inscription, ce qu'il dit d'un avis, comment il écrit une condition d'entrée,
// quel son il joue.

// CE QUE L'ÉTAGE SAIT DU JOUEUR au moment de s'inscrire.
struct ApplyFacts {
  // La carte est un champ de bataille : c'est le drapeau de carte qui interdit
  // l'inscription.
  bool on_battlefield = false;
  // Le délai qui suit une file quittée tient encore. Voir `HasApplyDelay`.
  bool apply_delayed = false;
  bool in_party = false;
  bool party_leader = false;
  // Les membres de la party connectés, le joueur compris.
  int party_online = 0;
  bool in_guild = false;
  bool guild_master = false;
  // Les membres de la guilde connectés, tels que l'en-tête de guilde les dit ;
  // zéro tant qu'il n'est pas venu.
  int guild_online = 0;
  // Les joueurs par camp du terrain choisi, au plus ; zéro : non dit.
  int team_most = 0;
  // La liaison accepte un envoi.
  bool link_ready = false;
};

// LA RAISON, déjà traduite, qui refuse cette inscription sans rien envoyer ;
// nulle si elle part. Dans l'ordre : la liaison, la carte, le délai, puis la
// party ou la guilde — appartenance, chef ou maître, connectés plus nombreux
// qu'un camp n'a de places. ⚠ LE SERVEUR JUGE LE RESTE : niveau, job, doublon.
[[nodiscard]] const char* ApplyRefusal(ro::sim::EntryQueueApplyKind kind, const ApplyFacts& facts);

// Vrai tant que le délai qui suit une file quittée tient sur le joueur.
[[nodiscard]] bool HasApplyDelay(std::span<const ro::sim::ActiveStatus> player_statuses);

// Les membres connectés de la party, le joueur compris.
[[nodiscard]] int OnlinePartyMembers(std::span<const ro::sim::PartyMember> members);

// Ce qu'un avis de la file dit au joueur, déjà traduit.
[[nodiscard]] const char* NoticeText(ro::sim::EntryQueueNoticeKind kind);

// Le niveau d'entrée d'un terrain, déjà traduit ; vide : aucune borne.
[[nodiscard]] std::string ConditionText(const BattlegroundField& field);

// L'effectif d'un terrain par camp, déjà traduit ; vide : non dit.
[[nodiscard]] std::string TeamText(const BattlegroundField& field);

// Le fichier d'un son de la file, dans les archives du jeu.
[[nodiscard]] InterfaceSoundFile SoundFileOf(ro::sim::EntryQueueSound sound);

}  // namespace moonclient
