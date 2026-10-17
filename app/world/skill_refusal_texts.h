#pragma once

#include <cstdint>

#include "ro/sim/skill_book.h"

namespace moonclient {

// CE QU'UNE PHRASE DE REFUS ATTEND en plus de son texte, à la place de ses
// trous `%s` et `%d`.
enum class RefusalHole : std::uint8_t {
  kNone,
  // `%s` : le nom affiché de la compétence refusée.
  kSkillName,
  // `%s` : l'objet requis, en lien d'item ; `%d` : la quantité.
  kItemAndCount,
  // `%s` : l'objet requis, en lien d'item.
  kItem,
  // `%d` : le nombre que le paquet porte.
  kCount,
  // `%s` : le nom de la compétence dont le paquet porte le NUMÉRO à la place du
  // nombre — celle qu'un enchaînement exige.
  kSkillOfCount,
  // `%d` : le motif lui-même, pour celui que la table ignore.
  kCause,
  // `%s` : l'état qui interdit le lancer — voir `BlockingStatusName`.
  kBlockingStatus,
};

// LA PHRASE D'UN REFUS, avant traduction : `french` est la clé du catalogue,
// marquée par `TrText`, ou NUL quand le client se tait.
struct RefusalPhrase {
  const char* french = nullptr;
  RefusalHole hole = RefusalHole::kNone;
  // UNE SECONDE LIGNE, sans trou, ou nul : un seul cas du client en écrit deux.
  const char* then_french = nullptr;
};

// La phrase que le client écrit pour ce refus — choisie par le motif, et pour
// quelques motifs par la compétence ou le nombre que le paquet porte aussi.
//
// 🔴 UN REFUS RETENU PAR LE CLIENT (`SkillRefusal::local`) se dit par sa
// raison, jamais par son motif, qui n'en est pas un.
//
// ⚠ CERTAINS MOTIFS SE TAISENT, comme chez le client : ceux que le serveur ne
// dit plus, et celui qui accompagne un message que le serveur a déjà envoyé
// lui-même. Un motif au-delà de la table, lui, se dit par son numéro : il vient
// d'un serveur plus récent que la table, et un refus muet ressemble à un client
// qui n'a rien envoyé.
//
// ⚠ LE MOTIF QUI PORTE UN NUMÉRO DE MESSAGE rend une phrase NULLE : c'est
// `RefusalMessageId` qui le dit.
[[nodiscard]] RefusalPhrase RefusalPhraseOf(const ro::sim::SkillRefusal& refusal);

// Le numéro du message que ce refus désigne dans la table des messages du
// serveur, ou ZÉRO quand son motif n'en porte pas.
[[nodiscard]] std::uint32_t RefusalMessageId(const ro::sim::SkillRefusal& refusal);

// VRAI QUAND `/skillfail` ÉTEINT TAIT CE REFUS : l'échec sans motif précis —
// le motif zéro hors des skills qu'il nomme, le motif dix — et le délai de
// réutilisation. Les autres motifs disent ce qui manque, et le jeu les écrit
// toujours.
[[nodiscard]] bool RefusalFollowsSkillFailSwitch(const ro::sim::SkillRefusal& refusal);

// LE NOM DE L'ÉTAT QUI INTERDIT un refus que le client a retenu lui-même, en
// jargon du jeu, donc sans traduction — ou nul quand le refus n'en nomme pas.
[[nodiscard]] const char* BlockingStatusName(const ro::sim::SkillRefusal& refusal);

// La ligne entière, « compétence : phrase », avant traduction : la ponctuation
// change d'une langue à l'autre.
[[nodiscard]] const char* RefusalLineFrench();

}  // namespace moonclient
