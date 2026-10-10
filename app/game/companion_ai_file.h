#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_players.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/companion_tactics.h"

namespace moonclient {

// ── L'IA DES COMPAGNONS SUR LE DISQUE : `savedata/companion_ai.yaml` ───────
//
// Chaque type de compagnon (sa classe exacte) a des PROFILS NOMMÉS. Un profil
// porte tout ce qui se règle : les réglages du niveau 1 (skills compris), les
// tactiques par monstre, les conduites envers les joueurs, les règles si/alors
// et les amis. Une section absente reprend l'IA basique, qui n'est jamais
// écrite : elle existe d'usine.
//
// Chaque personnage y a sa fiche : le profil actif de chaque type pour chacun
// des trois contextes, la dernière forme vue de chaque genre de compagnon, et
// les propositions de recopie refusées.
//
// ⚠ LE FORMAT EST VERSIONNÉ : un fichier d'une version inconnue ne se relit
// pas, et ne s'écrase pas.
//
// 🔴 ON N'ÉCRIT QUE CE QU'ON A CHANGÉ. L'enregistrement relit le fichier, puis
// y renomme, retire ou crée les seuls profils concernés, y remplace les seules
// lignes de tactique modifiées, par leur clé, et les seules sections et fiches
// modifiées : les nœuds inconnus, les autres profils, les autres types, les
// autres personnages, les lignes et les règles écartées à la lecture restent
// tels quels. Deux clients sur la même installation gardent ainsi chacun leurs
// lignes. Seuls les commentaires ne survivent pas, hors de l'en-tête que le
// fichier réécrit.
//
// ⚠ LA FENÊTRE ENTRE LA RELECTURE ET L'ÉCRITURE reste ouverte : de deux
// enregistrements simultanés, le dernier écrit l'emporte.

// LA VERSION DU FICHIER, aux clés et aux mots anglais (ADR 0093). Un fichier
// sans profil nommé ni fiche reste un fichier de cette version.
inline constexpr int kCompanionAiFileVersion = 1;
inline constexpr std::string_view kCompanionAiFileName = "companion_ai.yaml";
// LES NOMS GRAVÉS : des données, pas des libellés ; la fenêtre les traduit.
// L'IA basique n'est jamais écrite comme profil, seulement nommée par une fiche.
inline constexpr std::string_view kBasicProfileName = "Basic AI";
// Le profil qu'un geste crée quand il modifierait l'IA basique, et que prend un
// type sans aucun choix ; un profil nommé comme les autres.
inline constexpr std::string_view kOwnProfileName = "My profile";
// AU PLUS CE NOMBRE DE PROFILS NOMMÉS par type, l'IA basique non comprise.
inline constexpr std::size_t kCompanionProfileMost = 8;
// UN NOM DE PROFIL tient dans ce nombre d'octets, en UTF-8.
inline constexpr std::size_t kProfileNameMostBytes = 32;

// POURQUOI UNE RÈGLE A ÉTÉ ÉCARTÉE : une raison que la fenêtre traduit.
enum class RuleRejection : std::uint8_t {
  // L'entrée n'est pas une règle.
  kNotARule,
  // Un mot hors du vocabulaire, pour le champ nommé.
  kUnknownWord,
  // Un nombre illisible, pour le champ nommé.
  kBadNumber,
  // Aucune liste de conditions.
  kNoCondition,
  // Une condition illisible ou hors du cadre.
  kBadCondition,
  // La règle entière hors du cadre : action, cible d'action, bornes.
  kOutOfFrame,
  // Au-delà de la dernière règle permise.
  kBeyondLimit,
};

// UNE RÈGLE ÉCARTÉE À LA LECTURE : sa place dans la liste du fichier, pourquoi,
// et le champ en cause quand la raison en nomme un. Elle reste dans le fichier.
struct RejectedRule {
  std::size_t position = 0;
  RuleRejection why = RuleRejection::kNotARule;
  std::string field;

  [[nodiscard]] friend bool operator==(const RejectedRule&, const RejectedRule&) = default;
};

// UN PROFIL D'UN TYPE DE COMPAGNON.
struct CompanionProfile {
  // Les réglages du niveau 1, skills compris ; vides quand le fichier n'en
  // porte pas. Leurs tactiques, conduites, règles et amis ne servent pas : ce
  // sont les champs qui suivent.
  std::optional<ro::sim::CompanionAiSettings> settings;
  ro::sim::CompanionTactics tactics;
  ro::sim::CompanionPlayerConducts conducts;
  std::vector<ro::sim::CompanionRule> rules;
  // LES CHAMPS INCONNUS DE CHAQUE RÈGLE, en texte YAML, un par règle de
  // `rules` ; vide pour aucun. Ils suivent leur règle quand elle se déplace.
  std::vector<std::string> rule_extras;
  std::vector<std::string> friends;
  std::vector<RejectedRule> rejected_rules;

  [[nodiscard]] friend bool operator==(const CompanionProfile&, const CompanionProfile&) = default;
};

struct NamedProfile {
  std::string name;
  CompanionProfile profile;

  [[nodiscard]] friend bool operator==(const NamedProfile&, const NamedProfile&) = default;
};

// UN PERSONNAGE : son identifiant (cid) quand le client le connaît, sinon son
// nom. Le nom est gardé à côté du cid, pour qu'un import puisse le montrer.
struct CharacterKey {
  std::uint32_t cid = 0;
  std::string name;

  [[nodiscard]] friend bool operator==(const CharacterKey&, const CharacterKey&) = default;
};

// CETTE FICHE EST CELLE DE CE PERSONNAGE : par le cid quand on le connaît, par
// le nom sinon.
[[nodiscard]] bool SameCharacter(const CharacterKey& stored, const CharacterKey& wanted) noexcept;

// LE PROFIL ACTIF DE CHAQUE CONTEXTE, dans l'ordre de `CompanionAiContext` ;
// vide pour « sans choix ».
using ContextProfiles = std::array<std::optional<std::string>, ro::sim::kCompanionAiContextCount>;

// LES GENRES DE COMPAGNON, dans l'ordre de `AiCompanion`.
inline constexpr std::size_t kAiCompanionCount = 2;

// LA FICHE D'UN PERSONNAGE.
struct CompanionCharacter {
  CharacterKey key;
  // Par classe de compagnon.
  std::map<std::uint16_t, ContextProfiles> active;
  // La dernière classe vue de chaque genre ; zéro pour aucune.
  std::array<std::uint16_t, kAiCompanionCount> previous_form{};
  // Les propositions de recopie refusées : (ancienne classe, nouvelle classe).
  std::vector<std::pair<std::uint16_t, std::uint16_t>> declined;
  // Celles qui attendent leur réponse, même forme.
  std::vector<std::pair<std::uint16_t, std::uint16_t>> proposed;
  // Celles que le chat a déjà signalées.
  std::vector<std::pair<std::uint16_t, std::uint16_t>> told;

  [[nodiscard]] friend bool operator==(const CompanionCharacter&,
                                       const CompanionCharacter&) = default;
};

// LES PROFILS NOMMÉS de chaque classe de compagnon, et les fiches.
struct CompanionProfiles {
  // Dans l'ordre du fichier.
  std::map<std::uint16_t, std::vector<NamedProfile>> by_job;
  std::vector<CompanionCharacter> characters;

  [[nodiscard]] const CompanionProfile* Find(std::uint16_t job, std::string_view name) const;
  [[nodiscard]] CompanionProfile* Find(std::uint16_t job, std::string_view name);
  // Le profil, ajouté vide à la fin s'il n'existe pas. ⚠ SANS BORNE : ce sont
  // les opérations sur les profils qui bornent.
  CompanionProfile& At(std::uint16_t job, std::string_view name);
  [[nodiscard]] const CompanionCharacter* FindCharacter(const CharacterKey& key) const;

  [[nodiscard]] friend bool operator==(const CompanionProfiles&,
                                       const CompanionProfiles&) = default;
};

// UN NOM DE PROFIL QU'UN FICHIER PEUT PORTER : non vide, dans sa longueur, sans
// caractère de contrôle (C0, DEL, C1) ni espace autour, et qui ne se confond
// pas avec un profil réservé (voir `ReservedProfileLabel`).
[[nodiscard]] bool ValidProfileName(std::string_view name) noexcept;
// UN NOM QUI SE LIRAIT COMME UN PROFIL RÉSERVÉ : la clé de l'IA basique, et les
// libellés affichés de l'IA basique et de « Mon profil » dans les trois langues
// du client. La clé « My profile » elle-même reste permise : c'est ce profil.
[[nodiscard]] bool ReservedProfileLabel(std::string_view name) noexcept;
// UN CARACTÈRE DE CONTRÔLE dans ce texte UTF-8 : C0, DEL, ou C1 (U+0080 à
// U+009F).
[[nodiscard]] bool HasControlCharacter(std::string_view text) noexcept;

// LES RÉGLAGES D'UN COMPAGNON DE CETTE CLASSE selon son profil : l'IA basique,
// puis ce que le profil en dit. Sans profil (nul), l'IA basique.
[[nodiscard]] ro::sim::CompanionAiSettings SettingsOfProfile(ro::sim::AiCompanion companion,
                                                             std::uint16_t job,
                                                             const CompanionProfile* profile);

// CE QU'UNE LECTURE A DONNÉ.
struct CompanionAiFileRead {
  CompanionProfiles profiles;
  // Ce qui a été écarté, et pourquoi : une ligne, une règle, un profil ou un
  // réglage hors du vocabulaire ou de ses bornes. Le reste du fichier tient.
  std::vector<std::string> rejected;
  // Une version que ce client ne connaît pas : rien n'est relu, et le fichier
  // ne doit pas être réécrit.
  bool unknown_version = false;
  // Un fichier présent mais illisible : même règle.
  bool unreadable = false;
};

[[nodiscard]] std::filesystem::path CompanionAiFilePath(const std::filesystem::path& client_root);

// LES MODIFICATIONS EN ATTENTE D'ÉCRITURE.
struct CompanionProfileEdits {
  using TacticEdits =
      std::vector<std::pair<ro::sim::TacticKey, std::optional<ro::sim::CompanionTactic>>>;
  // CELLES D'UN PROFIL. Les lignes de tactique, par clé : posée, ou retirée
  // (vide), la dernière l'emporte. Les réglages, les conduites, les règles et
  // les amis, chacun en entier.
  struct ProfileEdits {
    // UN PROFIL CRÉÉ : toutes ses sections s'écrivent en entier, tactiques
    // comprises, même par-dessus un profil de même nom qu'un autre client
    // aurait écrit.
    bool whole = false;
    TacticEdits tactics;
    std::optional<ro::sim::CompanionAiSettings> settings;
    std::optional<std::vector<ro::sim::CompanionPlayerConduct>> conducts;
    std::optional<std::vector<ro::sim::CompanionRule>> rules;
    // Les champs inconnus de chaque règle, voir `CompanionProfile::rule_extras`.
    std::vector<std::string> rule_extras;
    std::optional<std::vector<std::string>> friends;
  };
  // UN RENOMMAGE OU UN RETRAIT, joués dans l'ordre sur le fichier relu, avant
  // les modifications des profils, qui suivent le nom d'arrivée.
  struct ProfileOp {
    enum class Kind : std::uint8_t { kRename, kDelete };
    Kind kind = Kind::kRename;
    std::string name;
    std::string to;
  };
  struct TypeEdits {
    std::vector<ProfileOp> ops;
    std::map<std::string, ProfileEdits> profiles;
    // Les profils créés, dans l'ordre de leur création : c'est celui où ils
    // s'ajoutent au fichier.
    std::vector<std::string> created;
  };
  std::map<std::uint16_t, TypeEdits> by_job;
  // LES FICHES MODIFIÉES, entières : chacune remplace celle du même personnage.
  std::vector<CompanionCharacter> characters;

  void Set(std::uint16_t job, std::string_view profile, const ro::sim::CompanionTactic& tactic);
  void Remove(std::uint16_t job, std::string_view profile, ro::sim::TacticKey key);
  void SetSettings(std::uint16_t job, std::string_view profile,
                   const ro::sim::CompanionAiSettings& settings);
  void SetConducts(std::uint16_t job, std::string_view profile,
                   const ro::sim::CompanionPlayerConducts& conducts);
  // `extras` : les champs inconnus de chaque règle. Sans eux (une autre taille
  // que `rules`), chaque règle reprend ceux de la place qu'elle occupe.
  void SetRules(std::uint16_t job, std::string_view profile,
                const std::vector<ro::sim::CompanionRule>& rules,
                const std::vector<std::string>& extras = {});
  void SetFriends(std::uint16_t job, std::string_view profile,
                  const std::vector<std::string>& friends);
  // UN PROFIL CRÉÉ, ou recopié en entier.
  void SetWhole(std::uint16_t job, std::string_view profile, const CompanionProfile& whole);
  // Les modifications en attente suivent le profil renommé, et disparaissent
  // avec le profil retiré ; les fiches en attente suivent aussi.
  void Rename(std::uint16_t job, std::string_view from, std::string_view to);
  void Delete(std::uint16_t job, std::string_view name);
  void SetCharacter(const CompanionCharacter& character);

  [[nodiscard]] const ProfileEdits* Find(std::uint16_t job, std::string_view profile) const;
  [[nodiscard]] bool empty() const noexcept { return by_job.empty() && characters.empty(); }

 private:
  ProfileEdits& Of(std::uint16_t job, std::string_view profile);
  void Record(std::uint16_t job, std::string_view profile, ro::sim::TacticKey key,
              const std::optional<ro::sim::CompanionTactic>& line);
};

// LE PROFIL ACTIF D'UNE FICHE, renommé (`to`) ou retiré (vide) : ce que les
// opérations sur les profils font aux fiches, en mémoire comme dans le fichier.
// Rend vrai quand la fiche a changé.
bool RenameInCharacter(CompanionCharacter& character, std::uint16_t job, std::string_view from,
                       const std::optional<std::string>& to);

// LE TEXTE, sans I/O : c'est ce que les essais éprouvent.
[[nodiscard]] CompanionAiFileRead ParseCompanionAi(std::string_view text);
// Le texte courant avec ces modifications fusionnées ; rien quand il est
// illisible, d'une version inconnue, ou d'une forme qu'on ne sait pas compléter.
[[nodiscard]] std::optional<std::string> MergeCompanionAi(std::string_view current_text,
                                                          const CompanionProfileEdits& edits);
// Un fichier neuf qui porte ces profils et ces fiches.
[[nodiscard]] std::string FormatCompanionAi(const CompanionProfiles& profiles);

// LE FICHIER. Absent : aucun profil, sans erreur. Ce qui est écarté se dit dans
// le journal.
[[nodiscard]] CompanionAiFileRead LoadCompanionAi(const std::filesystem::path& client_root);
// Relit, fusionne, écrit. Rend la lecture du fichier écrit — les lignes d'un
// autre client y compris —, ou rien quand rien n'a été écrit.
[[nodiscard]] std::optional<CompanionAiFileRead> SaveCompanionAiEdits(
    const std::filesystem::path& client_root, const CompanionProfileEdits& edits);

}  // namespace moonclient
