#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace ro::sim {

// CE QUE LE SERVEUR SEUL SAIT D'UNE COMPÉTENCE.
//
// 🔴 NI L'INSTALLATION NI LE FIL ORDINAIRE NE LE PORTENT. Sa description
// écrit un paragraphe pour le joueur ; le grimoire envoie un rang et un coût. Le
// TEMPS d'incantation, le temps d'attente, le délai d'après-coup et les DÉGÂTS
// qu'une compétence ferait vivent dans `skill_db` et dans les formules de
// combat, côté serveur — et ils dépendent de l'équipement et des états du
// personnage, donc d'un calcul qu'aucune table ne peut contenir.
//
// Le serveur les envoie sur demande : `CZ 0x0F0B -> ZC 0x0F0C` pour le cast,
// `CZ 0x0F0D -> ZC 0x0F0E` pour les dégâts.
//
// ⚠ ILS SE DEMANDENT, ILS N'ARRIVENT PAS SEULS. Une fenêtre qui les montre doit
// donc savoir se dessiner sans eux, et dire qu'elle attend.

// LE CAST D'UNE COMPÉTENCE À UN RANG, tout en millisecondes.
struct SkillCastFacts {
  // 🔴 DEUX TEMPS D'INCANTATION, ET ILS NE SE RÉDUISENT PAS PAREIL : le VARIABLE
  // tombe avec la DEX et l'INT et se coupe à l'interruption, le FIXE ne bouge
  // que sous certains états. Les additionner perdrait la moitié de ce qu'on
  // vient demander.
  std::int32_t cast_variable_ms = 0;
  std::int32_t cast_fixed_ms = 0;
  // Avant de pouvoir la relancer.
  std::int32_t cooldown_ms = 0;
  // Avant de pouvoir lancer QUOI QUE CE SOIT d'autre.
  std::int32_t after_delay_ms = 0;
};

// L'ESTIMATION DE DÉGÂTS d'une compétence contre une cible.
struct SkillDamageFacts {
  std::uint32_t skill_id = 0;
  std::uint16_t level = 0;
  // ⚠ TROIS CIBLES POSSIBLES, ET LE ZÉRO N'EST PAS « AUCUNE » : zéro est le
  // mannequin NEUTRE sans défense — les dégâts bruts —, `0xFFFFFFFF` est
  // soi-même (le miroir du duel), et tout le reste est un vrai monstre avec sa
  // défense, son élément, sa race et sa taille.
  std::uint32_t target_mob_id = 0;
  // 0 : l'estimation vaut. 1 : la compétence ne fait pas de dégâts. 2 : cible
  // introuvable.
  std::uint8_t status = 0;
  // Ce que le coup EST, aux yeux du serveur : arme, magie, ou divers.
  std::uint8_t attack_kind = 0;
  // Combien de coups la compétence porte d'un lancement.
  std::uint16_t hits = 0;
  std::int64_t least = 0;
  std::int64_t most = 0;
  std::int64_t average = 0;
  // Le nom du monstre visé, vide pour le mannequin et pour soi-même.
  std::string target_name;
};

// CE QUE LE SERVEUR A DIT, RANGÉ PAR COMPÉTENCE.
//
// ⚠ UNE TABLE QUI GRANDIT À LA DEMANDE, et non une liste refaite d'un coup comme
// celle du coffre : chaque réponse ne parle que d'UNE compétence, et ce qu'on a
// déjà demandé reste bon tant que rien n'a changé d'équipement.
class SkillFacts {
 public:
  // Pose la table de cast d'une compétence, un rang par entrée — l'entrée `n`
  // est le rang `n + 1`.
  void TellCast(std::uint32_t skill_id, std::vector<SkillCastFacts> levels);

  // Vide quand le serveur n'a rien dit de cette compétence, ce qui est le cas
  // NORMAL tant qu'on ne l'a pas demandé.
  [[nodiscard]] std::span<const SkillCastFacts> CastOf(std::uint32_t skill_id) const;

  // ⚠ UNE SEULE ESTIMATION PAR COMPÉTENCE : la suivante remplace la précédente.
  // Changer de cible, c'est redemander, et garder les deux ferait lire une
  // estimation faite contre autre chose.
  void TellDamage(SkillDamageFacts told);

  [[nodiscard]] const SkillDamageFacts* DamageOf(std::uint32_t skill_id) const;

  void Clear() noexcept {
    cast_.clear();
    damage_.clear();
  }

 private:
  // ⚠ RANGÉES PAR NUMÉRO : rien n'en dépend, mais un parcours reproductible vaut
  // mieux qu'un ordre de table de hachage quand on vient lire au débogueur.
  std::map<std::uint32_t, std::vector<SkillCastFacts>> cast_;
  std::map<std::uint32_t, SkillDamageFacts> damage_;
};

}  // namespace ro::sim
