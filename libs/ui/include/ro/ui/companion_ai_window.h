#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/companion_tactic_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA FENÊTRE « IA DU COMPAGNON » ──────────────────────────────────────────
//
// L'éditeur du profil d'un compagnon : ses réglages de base, ses tactiques par
// monstre, ses règles si/alors et ses amis. Une seule fenêtre, flottante, avec
// un onglet par compagnon quand les deux sont là ; elle se ferme quand aucun ne
// l'est plus.
//
// Cette couche ne connaît pas l'IA. Tout arrive en nombres, en rangs de liste
// et en libellés DÉJÀ TRADUITS, et chaque geste repart en intention : `app` la
// valide, la donne à l'IA et l'enregistre.
//
// 🔴 LE VOCABULAIRE DES RÈGLES VIENT D'`app`, avec ses bornes. La fenêtre ne
// propose que ce qu'il permet : chaque condition, chaque action et chaque
// cible d'action se choisit dans une liste, et seuls les nombres se tapent.

// LES BORNES QUE LA FENÊTRE IMPOSE, celles de l'IA et du fichier : `app` les
// vérifie à la compilation.
inline constexpr std::size_t kCompanionAiRuleMost = 16;
inline constexpr std::size_t kCompanionAiConditionMost = 3;
inline constexpr std::size_t kCompanionAiFriendMost = 32;
// ⚠ UN PLAFOND ABSOLU : un curseur de distance s'arrête plus tôt, au rayon de
// vue que le serveur annonce — voir `CompanionAiCellsShown`.
inline constexpr int kCompanionAiCellsMost = 50;
inline constexpr int kCompanionAiPercentMost = 100;
inline constexpr int kCompanionAiDelayMostMs = 3600000;
inline constexpr int kCompanionAiSpReserveMost = 99999;
inline constexpr int kCompanionAiRuleDelayMostMs = 3600000;
inline constexpr int kCompanionAiPerTargetMost = 99;
// Un nom de personnage du serveur et son terminateur.
inline constexpr std::size_t kCompanionAiFriendNameBytes = 24;
// Ce qu'on tape pour chercher dans une longue liste d'une règle, terminateur
// compris.
inline constexpr std::size_t kCompanionAiListFilterBytes = 64;

// LES RANGS DES LISTES DE LA FENÊTRE, dans l'ordre où `app` les numérote.
inline constexpr int kTacticRankChoices = 7;
inline constexpr int kTacticTriggerChoices = 3;
// Le déclencheur « Toujours », premier de la liste.
inline constexpr int kTacticTriggerAlways = 0;
// Les lignes génériques, dans cet ordre : `app` vérifie qu'il est le sien.
inline constexpr int kTacticGenericAnyMonster = 0;
inline constexpr int kTacticGenericChests = 1;
inline constexpr int kTacticGenericStill = 2;
inline constexpr int kTacticGenericMinions = 3;
inline constexpr int kTacticGenericChoices = 4;
inline constexpr int kOffensiveSkillChoices = 4;
inline constexpr int kOwnerSeatedChoices = 2;

// LE LIBELLÉ D'UNE LIGNE GÉNÉRIQUE, déjà traduit.
[[nodiscard]] const char* CompanionTacticGenericLabel(int generic);

// ── RÉGLAGES ────────────────────────────────────────────────────────────────

// Un skill offensif permis ou non, et à quel niveau. Zéro : le niveau appris.
struct SkillPermissionField {
  std::uint16_t skill_id = 0;
  bool allowed = true;
  int level = 0;

  [[nodiscard]] friend bool operator==(const SkillPermissionField&,
                                       const SkillPermissionField&) = default;
};

// LES RÉGLAGES DU NIVEAU 1, champ pour champ. Les délais sont en
// millisecondes, et « zéro » veut dire « aucun » partout où il borne.
struct CompanionBaseFields {
  int follow_cells = 0;
  int leash_still_cells = 0;
  int leash_walking_cells = 0;
  int engage_still_cells = 0;
  int engage_walking_cells = 0;
  bool attack_on_own = false;
  int own_attack_min_hp_percent = 0;
  int own_attack_min_sp_percent = 0;
  bool defend_owner = false;
  bool assist_owner = false;
  bool defend_partner = false;
  bool defend_self = false;
  bool spare_others_monsters = false;
  bool chase = false;
  bool retarget_while_chasing = false;
  int drop_untouched_after_ms = 0;
  int arrival_attack_hold_ms = 0;
  int offensive_skills = 0;
  // Une ligne par skill offensif possédé, dans l'ordre de `offensive_skills` de
  // la vue.
  std::vector<SkillPermissionField> permissions;
  int skill_sp_reserve = 0;
  int skill_delay_ms = 0;
  bool standby_defends = false;
  bool standby_after_fight = false;
  bool standby_kept_across_maps = false;
  bool move_order_holds_cell = false;
  bool move_order_hold_fights = false;
  int owner_seated = 0;
  int return_below_hp_percent = 0;
  int flee_below_hp_percent = 0;

  [[nodiscard]] friend bool operator==(const CompanionBaseFields&,
                                       const CompanionBaseFields&) = default;
};

// UN SKILL OFFENSIF QUE LE COMPAGNON POSSÈDE, pour ses permissions.
struct OffensiveSkillRow {
  std::uint16_t skill_id = 0;
  std::string label;
  // Le nom INTERNE que le serveur donne, que seul le staff voit au survol.
  std::string idname;
  int learned_level = 0;
};

// ── TACTIQUES ───────────────────────────────────────────────────────────────

struct TacticRowView {
  // Le monstre, ou la ligne générique, déjà traduit.
  std::string label;
  int rank = 0;
  int trigger = 0;
  TacticAdvancedFields advanced;
};

// ── RÈGLES ──────────────────────────────────────────────────────────────────

// UN CHOIX D'UNE LISTE : sa valeur pour `app`, son libellé déjà traduit.
struct RuleChoice {
  int value = 0;
  std::string label;
  // Ce qui aide à choisir, déjà traduit, montré en retrait après le libellé :
  // le niveau, la race, l'élément d'un monstre… Vide sans rien.
  std::string detail;
};

// LE CHAMP NUMÉRIQUE QU'UNE CONDITION REMPLIT.
enum class RuleNumberField : std::uint8_t { kNone, kNumber, kSeconds, kCells };

struct RuleNumberSlot {
  RuleNumberField field = RuleNumberField::kNone;
  int least = 0;
  int most = 0;
  // Ce qui précède le nombre, puis son unité : « à moins de », « cases ».
  std::string label;
};

// LA LISTE QU'UNE CONDITION DÉROULE : un identifiant (statut, skill, monstre)
// ou un rang d'énumération (race, élément, activité…).
enum class RuleListField : std::uint8_t { kNone, kId, kChoice };

inline constexpr std::size_t kRuleNumberSlots = 2;

// UNE SORTE DE CONDITION : un sujet et un prédicat, et ce qu'ils demandent.
struct RuleConditionKind {
  int subject = 0;
  int predicate = 0;
  // « PV du maître », déjà traduit.
  std::string label;
  // Les comparaisons permises, chacune avec son libellé propre (« porte »,
  // « sous », « au moins »…). Jamais vide.
  std::vector<RuleChoice> comparisons;
  std::array<RuleNumberSlot, kRuleNumberSlots> numbers;
  RuleListField list = RuleListField::kNone;
  std::span<const RuleChoice> choices;
  // Ce qu'il faut savoir de cette condition, déjà traduit ; vide sans rien.
  std::string tip;
};

// UNE ACTION, et les cibles d'action qu'elle permet hors skill.
struct RuleActionKind {
  int action = 0;
  std::string label;
  bool needs_skill = false;
  bool needs_step = false;
  // Hors skill : les cibles d'action permises ; vide quand elle n'en a pas.
  std::vector<RuleChoice> aims;
  // Ce que fait l'action, et ce qui continue pendant ce temps, déjà traduit.
  std::string tip;
};

// UN SKILL QU'UNE RÈGLE PEUT LANCER, et les cibles d'action que son genre
// permet.
struct RuleSkillChoice {
  int skill_id = 0;
  std::string label;
  int learned_level = 0;
  std::vector<RuleChoice> aims;
};

struct RuleVocabulary {
  std::span<const RuleConditionKind> conditions;
  std::span<const RuleActionKind> actions;
  std::span<const RuleSkillChoice> skills;
  // Le pas d'écart permis, en cases.
  int step_most = 0;
  // Le rayon de vue de la session, qui borne les distances en cases : celui que
  // le serveur annonce, et le défaut d'un rAthena (quatorze) tant qu'il n'a
  // rien dit. `app` le pose toujours ; zéro, la valeur d'une vue non remplie,
  // laisse le plafond absolu seul.
  int view_cells = 0;
};

struct RuleConditionFields {
  int subject = 0;
  int predicate = 0;
  int comparison = 0;
  int number = 0;
  int seconds = 0;
  int cells = 0;
  int id = 0;
  int choice = 0;

  [[nodiscard]] friend bool operator==(const RuleConditionFields&,
                                       const RuleConditionFields&) = default;
};

struct RuleFields {
  int action = 0;
  int aim = 0;
  int skill_id = 0;
  int skill_level = 0;
  int step_cells = 0;
  std::vector<RuleConditionFields> conditions;
  int own_delay_ms = 0;
  int per_target_most = 0;
  bool ignores_sp_reserve = false;

  [[nodiscard]] friend bool operator==(const RuleFields&, const RuleFields&) = default;
};

struct RuleRowView {
  // La règle en une phrase, déjà traduite.
  std::string sentence;
  RuleFields fields;
  // Elle vient de donner un ordre : la ligne se surligne.
  bool deciding = false;
  // POURQUOI ELLE NE PEUT PAS AGIR pour ce compagnon (un skill qu'il ne possède
  // pas), déjà traduit : la ligne se grise et le dit. Vide quand elle le peut.
  std::string unusable;
};

// ── CONDUITES ENVERS LES JOUEURS ────────────────────────────────────────────

// Les lignes génériques, dans cet ordre : `app` vérifie qu'il est le sien.
inline constexpr int kConductGenericAnyPlayer = 0;
inline constexpr int kConductGenericOwnerGuild = 1;
inline constexpr int kConductGenericChoices = 2;

struct ConductRowView {
  // Le personnage, la classe ou la ligne générique, déjà traduit.
  std::string label;
  int rank = 0;
  int trigger = 0;
  // Ceux des champs avancés qui valent pour un joueur.
  TacticAdvancedFields advanced;
};

// ── PROFILS ─────────────────────────────────────────────────────────────────

// LES TROIS CONTEXTES, dans l'ordre où `app` les numérote : PvE, PvP, WoE.
inline constexpr std::size_t kCompanionAiContexts = 3;
// Un nom de profil (32 octets) et son terminateur.
inline constexpr std::size_t kCompanionAiProfileNameBytes = 33;

// UN PROFIL DU TYPE : sa clé, que `app` reçoit en retour, et son libellé déjà
// traduit.
struct ProfileChoiceView {
  std::string key;
  std::string label;
};

struct CompanionProfilesView {
  // Les profils du type, l'IA basique en tête.
  std::span<const ProfileChoiceView> profiles;
  // Le rang, dans `profiles`, du profil que la fenêtre édite.
  int edited = 0;
  // LE CHOIX DE CE PERSONNAGE pour chaque contexte : un rang dans `profiles`,
  // moins de zéro sans choix ; et ce que le contexte prend alors, déjà traduit.
  std::array<int, kCompanionAiContexts> chosen{-1, -1, -1};
  std::array<std::string, kCompanionAiContexts> fallback;
  // Le contexte de la carte, que la fenêtre marque.
  int current_context = 0;
  // LE PROFIL ÉDITÉ N'AGIT DANS AUCUN CONTEXTE de ce personnage : la fenêtre le
  // dit, et propose de l'y employer.
  bool unused = false;
  // Le type a son plafond de profils.
  bool full = false;
  // Ce que « Importer… » propose, chacun avec son origine, déjà traduit.
  std::span<const std::string> imports;
  // « Recopier le profil de Lif ? », déjà traduit ; vide sans proposition.
  std::string proposal;
};

// ── LA FENÊTRE ──────────────────────────────────────────────────────────────

// L'IDENTITÉ DE LA FENÊTRE, sous laquelle `interface.ini` range sa place et sa
// taille et la pile d'Échap la trouve : le titre change avec la langue, elle
// jamais.
inline constexpr const char* kCompanionAiWindowId = "###companion_ai";

inline constexpr int kCompanionAiHomunculus = 0;
inline constexpr int kCompanionAiMercenary = 1;

// LES SECTIONS. L'onglet « Joueurs » se montre après « Tactiques ».
enum class CompanionAiSection : std::uint8_t { kSettings, kTactics, kRules, kFriends, kPlayers };

// LE CURSEUR DE DISTANCE que la souris survole ou tient, dans « Réglages » :
// `app` ne montre alors au sol que la zone qu'il règle, plus épaisse.
enum class CompanionAiZoneSlider : std::uint8_t {
  kNone,
  kFollow,
  kLeashStill,
  kLeashWalking,
  kEngageStill,
  kEngageWalking,
};

struct CompanionAiView {
  bool homunculus_present = false;
  bool mercenary_present = false;
  // Le compagnon que cette vue décrit : `kCompanionAiHomunculus` ou
  // `kCompanionAiMercenary`.
  int companion = kCompanionAiHomunculus;
  // Sa classe : la confirmation d'une remise à zéro la retient. Zéro tant
  // qu'elle n'est pas connue.
  std::uint16_t companion_class = 0;
  // Son nom, et le profil en vigueur, déjà traduits.
  std::string companion_name;
  std::string profile;
  // FAUX TANT QUE LE PROFIL NE SE DÉSIGNE PAS (classe inconnue) : la fenêtre le
  // dit, et rien ne s'y règle.
  bool editable = true;
  // LE DROIT DU STAFF, relu par `app` à chaque image : l'identifiant paraît au
  // survol d'un skill offensif.
  bool staff_allowed = false;
  // LE DERNIER GESTE REFUSÉ, et sa raison, déjà traduite ; vide sans refus.
  // `refused_rule` : la règle en cause, moins de zéro pour aucune.
  std::string refusal;
  int refused_rule = -1;

  // LES PROFILS : celui qu'on édite, les choix des contextes, la proposition.
  CompanionProfilesView profiles;
  // LE PROFIL ÉDITÉ EST L'IA BASIQUE : elle se montre, ne se modifie pas.
  bool basic = false;

  CompanionBaseFields base;
  std::span<const OffensiveSkillRow> offensive_skills;

  std::span<const TacticRowView> tactics;
  std::span<const TacticSkillChoice> openings;
  // Les lignes génériques déjà posées.
  std::array<bool, kTacticGenericChoices> generic_present{};
  bool tactics_full = false;

  // Le rayon de vue de la session, en cases : celui que le serveur annonce, et
  // le défaut d'un rAthena (quatorze) tant qu'il n'a rien dit. `app` le pose
  // toujours ; zéro, la valeur d'une vue non remplie, laisse le plafond seul.
  int view_cells = 0;
  // LA CASE « MONTRER LES ZONES AU SOL », lue des réglages par `app` : cochée,
  // les zones restent au sol fenêtre fermée.
  bool ground_zones_shown = false;

  std::span<const RuleRowView> rules;
  // Les règles que la lecture du fichier a écartées, chacune en une phrase qui
  // dit sa place et sa raison.
  std::span<const std::string> rejected_rules;
  RuleVocabulary vocabulary;

  std::span<const std::string> friends;

  std::span<const ConductRowView> conducts;
  std::array<bool, kConductGenericChoices> conduct_generic_present{};
  bool conducts_full = false;
  // Les classes de joueur qu'une ligne peut désigner.
  std::span<const RuleChoice> player_jobs;
};

// CE QUE L'EN-TÊTE DES PROFILS DEMANDE : un nom, une suppression à confirmer.
enum class ProfileDialog : std::uint8_t { kNone, kNew, kRename, kDuplicate, kImport, kDelete };

struct CompanionAiWindowState {
  bool open = false;
  // L'épingle de sa barre de titre, retenue par `app` d'une session à l'autre.
  bool pinned = false;
  // LE BOUTON « SIGNALER UN BUG » de sa barre de titre, posé par `app` depuis
  // les réglages.
  bool bug_report = false;
  // LA LARGEUR DE SA LIGNE LA PLUS LARGE, bords compris : la fenêtre ne
  // s'élargit pas au-delà. ⚠ ELLE NE FAIT QUE MONTER — la plus large vue
  // depuis le lancement, tous onglets confondus —, sans quoi la fenêtre
  // changerait de largeur à chaque onglet. Zéro tant qu'elle n'est pas
  // mesurée.
  float widest_pixels = 0.0F;
  // Posé par l'appelant : passer devant, sur l'onglet de ce compagnon.
  bool focus_asked = false;
  int companion = kCompanionAiHomunculus;
  // Posé par l'appelant : la section à montrer à la prochaine image (voir
  // `CompanionAiSection`), moins de zéro pour aucune demande.
  int section_asked = -1;
  // La règle et la ligne de tactique dépliées, moins de zéro pour aucune.
  int expanded_rule = -1;
  int expanded_tactic = -1;
  // « Repartir de l'IA basique » attend sa confirmation, pour CE compagnon :
  // son genre et sa classe. Elle tombe s'il n'est plus celui de l'onglet.
  bool reset_asked = false;
  int reset_companion = kCompanionAiHomunculus;
  std::uint16_t reset_class = 0;
  std::array<char, kCompanionAiFriendNameBytes> friend_name{};
  // LE PROFIL QU'ON ÉDITE, par sa clé ; vide : celui qui est appliqué.
  std::string edited_profile;
  ProfileDialog profile_dialog = ProfileDialog::kNone;
  std::array<char, kCompanionAiProfileNameBytes> profile_name{};
  int import_choice = 0;
  int expanded_conduct = -1;
  int conduct_job_choice = 0;
  // CE QU'ON CHERCHE dans la liste d'une règle ouverte : une seule s'ouvre à la
  // fois, et chacune repart d'un champ vide.
  std::array<char, kCompanionAiListFilterBytes> list_filter{};
};

// LE COMPAGNON DE L'ONGLET CHANGE, ou la fenêtre se ferme : tout ce qui est
// propre au compagnon — confirmation, dépliés, nom tapé — repart de zéro. Le
// seul chemin pour changer de compagnon.
void ChangeAiWindowCompanion(CompanionAiWindowState& state, int companion);

enum class CompanionAiVerb : std::uint8_t {
  kNothing,
  kSetBase,
  kResetToBasic,
  // La ligne `index` : son rang, son déclencheur et ses champs avancés.
  kSetTactic,
  kRemoveTactic,
  kAddGenericTactic,
  kEditRule,
  kAddRule,
  kRemoveRule,
  kDuplicateRule,
  // La règle `index` va au rang `to`.
  kMoveRule,
  kAddFriend,
  kRemoveFriend,
  // LES PROFILS : un nom tapé (`name`), une source d'import (`choice`), le choix
  // d'un contexte (`context`, `choice` : un rang de profil, moins de zéro pour
  // le repli), la réponse à la proposition (`accept`).
  kNewProfile,
  kRenameProfile,
  kDuplicateProfile,
  kDeleteProfile,
  kImportProfile,
  kSetContextProfile,
  kAnswerProposal,
  // LES CONDUITES : la ligne `index`, une ligne générique (`generic`), une
  // classe (`choice`, la valeur de `player_jobs`).
  kSetConduct,
  kRemoveConduct,
  kAddGenericConduct,
  kAddJobConduct,
};

struct CompanionAiWindowAction {
  CompanionAiVerb verb = CompanionAiVerb::kNothing;
  // Le compagnon de la vue sur laquelle le geste a été fait.
  int companion = kCompanionAiHomunculus;
  // Sa classe, pour `kResetToBasic` : `app` refuse une remise à zéro qui ne
  // vise plus le même compagnon.
  std::uint16_t companion_class = 0;
  std::size_t index = 0;
  std::size_t to = 0;
  CompanionBaseFields base;
  int rank = 0;
  int trigger = 0;
  TacticAdvancedFields advanced;
  int generic = 0;
  RuleFields rule;
  std::string name;
  // LE PROFIL ÉDITÉ au moment du geste, par sa clé : c'est lui que le geste
  // modifie.
  std::string profile;
  int context = 0;
  int choice = -1;
  bool accept = false;
  bool closed = false;
  // Le bouton « signaler un bug » de la barre de titre a été cliqué.
  bool report_bug = false;
  // À CHAQUE IMAGE, et non au seul geste : le curseur de distance survolé ou
  // tenu, et la case « Montrer les zones au sol » cliquée.
  CompanionAiZoneSlider hovered_zone = CompanionAiZoneSlider::kNone;
  bool ground_zones_toggled = false;
};

[[nodiscard]] CompanionAiWindowAction DrawCompanionAiWindow(CompanionAiWindowState& state,
                                                            const CompanionAiView& view,
                                                            const SkinArt& art);

// UN GESTE ACCEPTÉ PAR `app` : les règle et ligne dépliées suivent leur
// place — déplacement, retrait, copie, ajout.
void FollowAcceptedAiGesture(CompanionAiWindowState& state, const CompanionAiWindowAction& action,
                             std::size_t rule_count);

// LA CONDITION RAMENÉE DANS SA SORTE : une comparaison permise, des nombres
// dans leurs bornes, un choix de sa liste.
void FitRuleCondition(RuleConditionFields& condition, const RuleConditionKind& kind);
// L'ACTION RAMENÉE DANS SA SORTE : un skill possédé et un niveau appris, une
// cible d'action permise (la première, sans cible), un pas dans ses bornes.
void FitRuleAction(RuleFields& rule, const RuleActionKind& kind, const RuleVocabulary& vocabulary);

// UN CHOIX QUE LA RECHERCHE GARDE : son libellé contient le texte tapé, à la
// casse près, ou son identifiant l'écrit — « 103 » trouve 1039 comme 21039.
// Rien de tapé garde tout.
[[nodiscard]] bool RuleChoiceMatches(const RuleChoice& choice, std::string_view typed);

// UN NOM DE PERSONNAGE TAPÉ, sans les espaces qui l'entourent ; vide s'il n'en
// reste rien.
[[nodiscard]] std::string TrimmedCharacterName(std::string_view typed);

// CE QUE LA LIGNE « IA » D'UN ONGLET DEMANDE.
enum class CompanionAiLineVerb : std::uint8_t {
  kNothing,
  kOpen,
  kAcceptProposal,
  kDeclineProposal
};

// LA LIGNE « IA : <profil> » ET SON BOUTON « Régler… », dans l'onglet d'un
// compagnon, et sous elle la proposition de recopie (`proposal`, déjà traduite,
// vide sans elle) avec « Recopier » et « Non merci ». `profile` vide : rien ne
// se dessine.
[[nodiscard]] CompanionAiLineVerb DrawCompanionAiLine(std::string_view profile,
                                                      std::string_view proposal,
                                                      const SkinArt& art);

}  // namespace ro::ui
