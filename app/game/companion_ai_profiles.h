#pragma once

// LES OPÉRATIONS SUR LES PROFILS NOMMÉS ET LES FICHES DE PERSONNAGE, sans I/O :
// créer, renommer, dupliquer, supprimer, importer, choisir le profil actif d'un
// contexte, et la proposition de recopie après une évolution. Chaque opération
// change les profils en mémoire et note ce qu'il faut écrire. C'est ce que les
// essais éprouvent ; `CompanionAiStage` n'en est que le câblage.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_players.h"
#include "ro/sim/companion_rules.h"

#include "game/companion_ai_file.h"

namespace moonclient {

// POURQUOI UNE OPÉRATION EST REFUSÉE : une raison que la fenêtre traduit.
enum class ProfileRefusal : std::uint8_t {
  kNone,
  kEmptyName,
  kNameTooLong,
  // Un caractère de contrôle.
  kBadCharacter,
  // Le nom de l'IA basique.
  kReservedName,
  // Un autre profil de ce type porte déjà ce nom.
  kNameTaken,
  // Le type a déjà `kCompanionProfileMost` profils.
  kTypeFull,
  kUnknownProfile,
  // L'IA basique ne s'efface, ne se renomme ni ne se modifie.
  kBasicLocked,
};

// LE NOM TEL QU'IL SERA GARDÉ : sans espaces autour.
[[nodiscard]] std::string TrimmedProfileName(std::string_view typed);
// POURQUOI CE NOM N'EST PAS RECEVABLE dans ce type, `kNone` s'il l'est.
// `renamed` : le profil qu'on renomme, dont le nom actuel reste permis.
[[nodiscard]] ProfileRefusal NameRefusal(const CompanionProfiles& profiles, std::uint16_t job,
                                         std::string_view name, std::string_view renamed = {});
// LE CANDIDAT DE RANG `rank` tiré de `wanted` : lui-même au rang zéro, puis
// suivi de « (2) », « (3) »…, raccourci pour tenir dans sa longueur.
[[nodiscard]] std::string SuffixedProfileName(std::string_view wanted, std::size_t rank);
// UN NOM LIBRE tiré de `wanted` : lui-même s'il est libre, sinon suivi de
// « (2) », « (3) »…, raccourci pour tenir dans sa longueur.
[[nodiscard]] std::string FreeProfileName(const CompanionProfiles& profiles, std::uint16_t job,
                                          std::string_view wanted);

// LES RÈGLES QUI LANCENT UN SKILL QUE LE COMPAGNON NE POSSÈDE PAS : elles restent
// dans le profil, infaisables, et la fenêtre le dit. Leur rang dans la liste.
[[nodiscard]] std::vector<std::size_t> RulesMissingSkill(
    std::span<const ro::sim::CompanionRule> rules, std::span<const std::uint16_t> owned_skill_ids);

// LE PROFIL ACTIF de ce personnage pour ce type et ce contexte, sans rien
// écrire : voir `CompanionProfileBook::Active`.
[[nodiscard]] std::string ActiveProfileOf(const CompanionProfiles& profiles,
                                          const CharacterKey& key, std::uint16_t job,
                                          ro::sim::CompanionAiContext context);

// UNE PROPOSITION DE RECOPIE : le compagnon a paru sous `to_job`, une classe
// sans profil, et `from_job`, sa forme précédente, en a.
struct EvolutionProposal {
  std::uint16_t from_job = 0;
  std::uint16_t to_job = 0;

  [[nodiscard]] friend bool operator==(const EvolutionProposal&,
                                       const EvolutionProposal&) = default;
};

class CompanionProfileBook {
 public:
  CompanionProfileBook(CompanionProfiles& profiles, CompanionProfileEdits& edits) noexcept
      : profiles_(profiles), edits_(edits) {}

  // ── LES PROFILS D'UN TYPE ─────────────────────────────────────────────────

  // UN PROFIL NEUF, recopie de `source` : un profil du même type, ou l'IA
  // basique (`kBasicProfileName`). Dupliquer, c'est créer depuis un profil.
  ProfileRefusal Create(std::uint16_t job, std::string_view name, std::string_view source);
  // Les fiches qui le désignaient suivent le nouveau nom. « Mon profil » étant
  // le repli d'un PvE sans choix, chaque fiche sans choix PvE pour ce type
  // reçoit son nouveau nom en choix explicite.
  ProfileRefusal Rename(std::uint16_t job, std::string_view from, std::string_view to);
  // Les fiches qui le désignaient retombent sur leur repli.
  ProfileRefusal Delete(std::uint16_t job, std::string_view name);
  // L'IMPORT : recopie dans `to_job`, sous `name`, le profil `from_name` de
  // `from_job` — un autre type, ou le profil actif d'un autre personnage. Rien
  // n'est retiré : un skill que la destination ne possède pas laisse sa règle
  // infaisable (voir `RulesMissingSkill`), une permission ou une ouverture sans
  // objet reste sans effet. L'IA basique d'un autre type se recopie avec les
  // réglages d'usine de son espèce. Rien d'extérieur au fichier n'est lu.
  ProfileRefusal Import(std::uint16_t to_job, std::string_view name, std::uint16_t from_job,
                        std::string_view from_name);

  // ── LES FICHES ────────────────────────────────────────────────────────────

  // LE PROFIL ACTIF de ce personnage, pour ce type et ce contexte : son choix ;
  // en PvP ou en WoE sans choix (ou un profil disparu), celui du PvE ; en PvE
  // sans choix, « Mon profil » s'il existe, sinon l'IA basique — même quand un
  // autre contexte a un choix.
  [[nodiscard]] std::string Active(const CharacterKey& key, std::uint16_t job,
                                   ro::sim::CompanionAiContext context) const;
  // Un profil du type, ou l'IA basique.
  ProfileRefusal SetActive(const CharacterKey& key, std::uint16_t job,
                           ro::sim::CompanionAiContext context, std::string_view name);
  // Le contexte retombe sur son repli.
  ProfileRefusal ClearActive(const CharacterKey& key, std::uint16_t job,
                             ro::sim::CompanionAiContext context);
  // UN COMPAGNON DE CETTE CLASSE PARAÎT pour ce personnage. Rend la proposition
  // de recopie qui attend sa réponse, s'il y en a une ; sinon la classe devient
  // la dernière forme vue de son genre. Une proposition naît quand la nouvelle
  // classe n'a ni profil ni choix et que la précédente a des profils ; POSÉE,
  // elle est gardée dans la fiche et reste jusqu'à la réponse, même si un
  // profil naît entre-temps pour la nouvelle classe. Tant qu'elle attend, la
  // forme précédente ne change pas.
  std::optional<EvolutionProposal> NoteForm(const CharacterKey& key, std::uint16_t job);
  // LA RÉPONSE : recopier tous les profils de la forme précédente (sous un nom
  // libre quand le leur est pris), et faire prendre à chaque contexte de la
  // nouvelle classe le profil actif de l'ancienne ; ou refuser, ce qui est
  // retenu pour cette paire. Dans les deux cas la nouvelle classe devient la
  // dernière forme vue.
  void Answer(const CharacterKey& key, const EvolutionProposal& proposal, bool accept);
  // LE MESSAGE DE CHAT QUI SIGNALE UNE PROPOSITION n'est dit qu'une fois, d'une
  // connexion à l'autre : rend vrai la première fois, et le retient dans la
  // fiche jusqu'à la réponse.
  bool MarkTold(const CharacterKey& key, const EvolutionProposal& proposal);
  // Ce personnage a sa fiche, créée au besoin.
  void Know(const CharacterKey& key);

 private:
  [[nodiscard]] bool HasProfiles(std::uint16_t job) const;
  // La fiche du personnage, créée s'il n'en a pas ; son nom suit celui du jeu.
  CompanionCharacter& CharacterOf(const CharacterKey& key);
  // Un profil neuf posé à la fin de son type et noté à écrire en entier.
  void Store(std::uint16_t job, std::string_view name, CompanionProfile profile);

  CompanionProfiles& profiles_;
  CompanionProfileEdits& edits_;
};

}  // namespace moonclient
