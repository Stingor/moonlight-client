#pragma once

#include <string>

#include "ro/sim/entry_queue.h"

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
  // La carte est un champ de bataille.
  bool on_battlefield = false;
  bool in_party = false;
  bool party_leader = false;
  bool in_guild = false;
  bool guild_master = false;
  // La liaison accepte un envoi.
  bool link_ready = false;
};

// LA RAISON, déjà traduite, qui refuse cette inscription sans rien envoyer ;
// nulle si elle part. ⚠ LE SERVEUR JUGE L'EFFECTIF : aucune garde ici sur la
// taille du groupe ou de la guilde.
[[nodiscard]] const char* ApplyRefusal(ro::sim::EntryQueueApplyKind kind, const ApplyFacts& facts);

// Ce qu'un avis de la file dit au joueur, déjà traduit.
[[nodiscard]] const char* NoticeText(ro::sim::EntryQueueNoticeKind kind);

// Le niveau d'entrée d'un terrain, déjà traduit ; vide : aucune borne.
[[nodiscard]] std::string ConditionText(const BattlegroundField& field);

// L'effectif d'un terrain par camp, déjà traduit ; vide : non dit.
[[nodiscard]] std::string TeamText(const BattlegroundField& field);

// Le fichier d'un son de la file, dans les archives du jeu.
[[nodiscard]] InterfaceSoundFile SoundFileOf(ro::sim::EntryQueueSound sound);

}  // namespace moonclient
