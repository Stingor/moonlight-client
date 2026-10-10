#pragma once

// L'IA DES COMPAGNONS DANS LA BOUCLE DE LA PARTIE : un cerveau par compagnon,
// cadencé à chaque image, et les gestes du joueur qui lui donnent des ordres.
// Ce que décide l'IA part sur le fil ici.

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_statuses.h"
#include "ro/sim/monster_sheet.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/companion_tactic_window.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/job_names.h"
#include "ro/vfs/status_table.h"

#include "game/companion_ai_editor.h"
#include "game/companion_ai_fields.h"
#include "game/companion_ai_file.h"
#include "game/companion_ai_profile_gestures.h"
#include "game/companion_ai_profiles.h"
#include "game/companion_ai_session.h"
#include "game/companion_ai_trace.h"
#include "game/companion_ai_window_compose.h"
#include "game/companion_aim.h"
#include "world/armed_cursor.h"

namespace moonclient {

class HomunculusStage;
class MercenaryStage;

// CE QU'UN GESTE D'ORDRE DIRECT A FAIT.
enum class CompanionGesture : std::uint8_t {
  // Rien : pas de compagnon, ou le second appui d'un double-clic.
  kIgnored,
  // Premier appui sur un acteur : il est désigné, le second appui l'attaque.
  kDesignated,
  kCommanded,
  kReserved,
};

// LE SILENCE APRÈS LEQUEL UNE RETOUCHE DE « AVANCÉ… » S'ENREGISTRE : assez long
// pour couvrir un glisser ou une saisie, assez court pour qu'un plantage ne
// perde presque rien.
inline constexpr std::uint32_t kTacticSaveQuietMs = 1500;
// LA RÈGLE QUI VIENT DE DÉCIDER reste surlignée ce temps dans la fenêtre « IA du
// compagnon » : assez pour que l'œil la trouve, assez court pour suivre la
// suivante.
inline constexpr std::uint32_t kDecidingRuleShownMs = 1200;

// CE QUE LA FENÊTRE « IA DU COMPAGNON » LIT DU RESTE DE LA PARTIE, pour nommer
// les skills, les statuts, les monstres et les mots de leur fiche. Tout est
// facultatif : sans table, un nom cède la place à son numéro.
struct CompanionAiWindowBooks {
  const ro::sim::World& world;
  const ro::sim::SkillTree* skill_tree = nullptr;
  const ro::vfs::StatusTable* statuses = nullptr;
  const ro::vfs::ItemWords* words = nullptr;
  const ro::sim::MonsterSheetBook* monster_sheets = nullptr;
  // Les noms des classes de joueur, que les conduites désignent.
  const ro::vfs::JobNames* job_names = nullptr;
};

// CE QUE L'IA LIT DU RESTE DE LA PARTIE, à chaque image.
struct CompanionAiBooks {
  // Le groupe du maître : les invocations de ses membres sont du camp.
  const ro::sim::PartyRoster& party;
  // Les tables de skills du client, qui disent la zone d'un skill ; nulles
  // avant leur chargement.
  const ro::sim::SkillTree* skill_tree = nullptr;
  // Les fiches de monstre reçues, que lisent les conditions des règles.
  const ro::sim::MonsterSheetBook* monster_sheets = nullptr;
  // La carte de marche de la carte courante.
  const ro::sim::WalkMap* walk_map = nullptr;
  // La guilde du maître, zéro sans guilde.
  std::uint32_t owner_guild_id = 0;
};

class CompanionAiStage {
 public:
  // À CHAQUE IMAGE : suit les compagnons, oublie tout au changement de carte,
  // et met sur le fil l'ordre que chaque IA décide.
  //
  // UN SEUL ARMEMENT À LA FOIS, avec le grimoire du joueur : `grimoire_armed`
  // qui vient de passer à vrai désarme le skill de compagnon. Rend vrai quand un
  // skill de compagnon vient de s'armer : l'appelant désarme alors le grimoire.
  [[nodiscard]] bool Advance(ro::net::Connection& link, ro::sim::World& world,
                             HomunculusStage& homunculus, MercenaryStage& mercenary,
                             const CompanionAiBooks& books, bool grimoire_armed,
                             std::uint32_t now_ms);
  // LA FICHE DE MONSTRE qu'une règle a voulu lire sans l'avoir, à demander au
  // serveur ; zéro sans demande. Elle se consomme.
  [[nodiscard]] std::uint32_t TakeWantedMonsterSheet() noexcept {
    return std::exchange(wanted_monster_sheet_, 0);
  }

  // UNE TRAME DU SERVEUR, LUE SANS ÊTRE PRISE : la liste des statuts d'un de
  // nos compagnons (`ZC 0x0F2D`), et le refus d'un skill
  // (`ZC_ACK_TOUSESKILL` en échec) que l'on reconnaît pour celui d'un de nos
  // compagnons, qui suspend ce skill dans son IA. Les autres refus que le serveur
  // tait se rattrapent par l'absence de trace, voir `CompanionSkillCaster`.
  void ObserveFrame(const ro::net::Frame& frame, const HomunculusStage& homunculus,
                    const MercenaryStage& mercenary, std::uint32_t now_ms);

  // L'AVIS DE CHARGEMENT DE LA CARTE VIENT DE PARTIR, à `loaded_ms` relu à cet
  // instant : le serveur compte de là son délai avant d'accepter une attaque de
  // compagnon, et le chargement peut avoir duré des secondes.
  void NoteArrival(std::uint32_t loaded_ms) noexcept;

  // Le compagnon est là et actif : ses gestes ont un sens.
  [[nodiscard]] bool present(ro::sim::AiCompanion companion) const noexcept;
  // POUR LA CONSOLE DE MISE AU POINT, en lecture : l'acteur suivi (zéro sans
  // lui) et le cerveau.
  [[nodiscard]] std::uint32_t actor_of(ro::sim::AiCompanion companion) const noexcept {
    return SlotOf(companion).actor_id;
  }
  [[nodiscard]] const ro::sim::CompanionBrain& brain_of(
      ro::sim::AiCompanion companion) const noexcept {
    return SlotOf(companion).brain;
  }

  // ── LES GESTES DE LA SOURIS ───────────────────────────────────────────────
  //
  // Sur un acteur attaquable : deux appuis séparés sur le même acteur
  // l'attaquent, le premier ne fait que le désigner, et la désignation ne
  // s'éteint pas d'elle-même. Au sol : « aller ici ». Avec `reserve` (Maj),
  // l'ordre part tout de suite dans la file réservée. Le second appui d'un
  // double-clic ne compte pas.
  CompanionGesture PressOnActor(ro::sim::AiCompanion companion, std::uint32_t actor_id,
                                bool reserve, bool second_of_double);
  CompanionGesture PressOnGround(ro::sim::AiCompanion companion, ro::sim::CellPosition cell,
                                 bool reserve, bool second_of_double);

  // « EN ATTENTE », en bascule, et ce qu'elle dit dans la conversation. Rend
  // faux sans compagnon.
  bool ToggleStandby(ro::sim::AiCompanion companion, ro::sim::World& world, std::uint32_t now_ms);
  [[nodiscard]] bool standby(ro::sim::AiCompanion companion) const noexcept;

  // ── LE SKILL DE COMPAGNON QUI ATTEND SA CIBLE ─────────────────────────────
  [[nodiscard]] const std::optional<CompanionAim>& aim() const noexcept { return aim_; }
  // Ce que le curseur doit montrer pour lui.
  [[nodiscard]] SkillAim cursor_aim() const noexcept;
  // Le clic sur un acteur, ou sur une case. Rend vrai quand le skill est parti
  // à l'IA ; faux quand il attend autre chose (une case, ou un acteur).
  bool ReleaseOnActor(std::uint32_t actor_id);
  bool ReleaseOnGround(ro::sim::CellPosition cell);
  void Disarm() noexcept { aim_.reset(); }

  // ── LES PROFILS, LEURS CONTEXTES ET CE QUI SURVIT ─────────────────────────
  //
  // Chaque type de compagnon a des profils nommés ; ce personnage en choisit un
  // par contexte (PvE, PvP, WoE), et l'IA suit celui du contexte de la carte.
  // Voir `game/companion_ai_session.h`, dont ceci n'est que le câblage.

  // UNE PARTIE COMMENCE : les profils de ce personnage relus, et chaque
  // compagnon remis à neuf. `character_id` est son cid, zéro quand le client ne
  // le connaît pas (son nom sert alors de clé).
  void See(const std::filesystem::path& client_root, std::uint32_t character_id);
  // LA PARTIE S'ACHÈVE : ce qui attend s'écrit, profils et délais.
  void Leave(std::uint32_t now_ms);
  // Le contexte où l'IA choisit ses profils.
  [[nodiscard]] ro::sim::CompanionAiContext context() const noexcept { return session_.context(); }
  // LA PROPOSITION DE RECOPIE qui attend sa réponse pour ce compagnon, vide sans
  // elle ; et la réponse.
  [[nodiscard]] const std::optional<EvolutionProposal>& proposal(
      ro::sim::AiCompanion companion) const noexcept;
  void AnswerProposal(ro::sim::AiCompanion companion, bool accept);
  // Sa phrase, traduite, pour l'onglet du compagnon ; vide sans proposition.
  [[nodiscard]] std::string_view ProposalText(ro::sim::AiCompanion companion) const;

  // ── LES CONDUITES, DEPUIS LE MENU D'ENTITÉ D'UN JOUEUR ────────────────────
  //
  // Reconnues au nom du joueur, dans le profil appliqué ; un geste sur l'IA
  // basique crée « Mon profil », comme une tactique.
  [[nodiscard]] std::optional<ro::sim::CompanionPlayerConduct> ConductFor(
      ro::sim::AiCompanion companion, const std::string& name) const;
  // UN CHOIX DU SOUS-MENU : un rang, un déclencheur (`TacticMenuChoice`), ou
  // retirer la ligne. Une ligne neuve naît au rang « moyen » et au déclencheur
  // « toujours ».
  void ChooseConduct(ro::sim::AiCompanion companion, const std::string& name, int choice);
  // « Avancé… » : la fenêtre « IA du compagnon » s'ouvre sur l'onglet
  // « Joueurs ».
  void OpenConductWindow(ro::sim::AiCompanion companion, ro::ui::CompanionAiWindowState& window);

  // ── LES TACTIQUES PAR MONSTRE ─────────────────────────────────────────────
  //
  // Un choix du sous-menu s'enregistre aussitôt ; ce que règle « Avancé… » va
  // tout de suite à l'IA, et au fichier après `kTacticSaveQuietMs` sans
  // retouche, ou dès la fenêtre fermée. Voir `companion_ai_file.h`.
  // La ligne de ce compagnon pour cette classe de monstre ; vide sans ligne, ou
  // sans compagnon.
  [[nodiscard]] std::optional<ro::sim::CompanionTactic> TacticFor(ro::sim::AiCompanion companion,
                                                                  std::uint16_t monster_job) const;
  // UN CHOIX DU SOUS-MENU : un rang ou un déclencheur (`TacticMenuChoice`), ou
  // retirer la ligne. Une ligne qui n'existait pas naît au rang « moyen » et
  // au déclencheur « toujours ».
  void ChooseTactic(ro::sim::AiCompanion companion, std::uint16_t monster_job, int choice);
  // « Avancé… » : la fenêtre s'ouvre sur cette ligne.
  void OpenTacticWindow(ro::sim::AiCompanion companion, std::uint16_t monster_job,
                        std::string monster_name, ro::ui::CompanionTacticWindowState& window);
  // À chaque image : la vue de la fenêtre, et ce qu'elle a rendu.
  void StageTacticWindow(const HomunculusStage& homunculus, const MercenaryStage& mercenary,
                         const ro::sim::SkillTree* skill_tree,
                         ro::ui::CompanionTacticWindowState& window,
                         ro::ui::CompanionTacticView& view);
  void ApplyTacticWindow(const ro::ui::CompanionTacticWindowAction& action, std::uint32_t now_ms);

  // ── LA FENÊTRE « IA DU COMPAGNON » ────────────────────────────────────────
  //
  // Chaque modification va tout de suite à l'IA, et au fichier comme celles de
  // « Avancé… » : après `kTacticSaveQuietMs` de calme, à la fermeture, ou au
  // départ du compagnon. Voir `game/companion_ai_window_stage.cpp`.

  // Le profil en vigueur, traduit, pour la ligne « IA : » de son onglet ; vide
  // sans compagnon.
  [[nodiscard]] const char* ProfileLabel(ro::sim::AiCompanion companion) const;
  // « Régler… » : la fenêtre s'ouvre, ou passe devant, sur ce compagnon.
  void OpenAiWindow(ro::sim::AiCompanion companion, ro::ui::CompanionAiWindowState& window);
  // À chaque image : la vue de la fenêtre. Elle se ferme quand aucun compagnon
  // n'est plus là.
  void StageAiWindow(const HomunculusStage& homunculus, const MercenaryStage& mercenary,
                     const CompanionAiWindowBooks& books, ro::ui::CompanionAiWindowState& window,
                     ro::ui::CompanionAiView& view);
  // Ce que la fenêtre a demandé ; un geste accepté fait suivre ses dépliés.
  void ApplyAiWindow(const ro::ui::CompanionAiWindowAction& action,
                     ro::ui::CompanionAiWindowState& window, std::uint32_t now_ms);

  // LES AMIS DU COMPAGNON, depuis le menu d'entité d'un joueur : reconnus à
  // leur nom.
  [[nodiscard]] bool IsCompanionFriend(ro::sim::AiCompanion companion,
                                       const std::string& name) const;
  [[nodiscard]] bool CompanionFriendsFull(ro::sim::AiCompanion companion) const;
  void SetCompanionFriend(ro::sim::AiCompanion companion, const std::string& name, bool befriend,
                          std::uint32_t now_ms);

 private:
  struct Slot {
    ro::sim::CompanionBrain brain;
    ro::sim::CompanionAiSettings settings;
    // L'acteur suivi, zéro quand le compagnon n'est pas actif.
    std::uint32_t actor_id = 0;
    // Le dernier compagnon suivi, acteur et classe : un autre fait repartir
    // l'IA de zéro.
    FollowedCompanion followed;
    // L'acteur désigné par un premier appui.
    ro::sim::AttackDesignation designation;
    // La liste de ses statuts, demandée au serveur quand une règle la lit.
    ro::sim::CompanionStatusWatch statuses;
    // Son profil appliqué et sa proposition de recopie.
    CompanionSeat seat;
    // Ses décisions, au journal.
    CompanionAiTrace trace;
  };

  void Follow(Slot& slot, const ro::sim::CompanionFacts& facts, std::uint32_t now_ms);
  // CE COMPAGNON tel que la session le mène.
  [[nodiscard]] SeatedCompanion Seated(ro::sim::AiCompanion companion) noexcept;
  [[nodiscard]] std::array<SeatedCompanion, 2> SeatedAll() noexcept;
  // Rend vrai quand ce skill est armé, qu'il vienne de l'être ou le soit déjà.
  bool Arm(const CompanionAim& aim, ro::sim::World& world, std::uint32_t now_ms);
  [[nodiscard]] Slot& SlotOf(ro::sim::AiCompanion companion) noexcept;
  [[nodiscard]] const Slot& SlotOf(ro::sim::AiCompanion companion) const noexcept;
  // LE PROFIL QUE LA FENÊTRE ÉDITE : celui qu'elle a choisi s'il existe encore,
  // sinon celui qui est appliqué.
  [[nodiscard]] std::string EditedProfile(const Slot& slot,
                                          const ro::ui::CompanionAiWindowState& window) const;
  // L'en-tête des profils et l'onglet « Joueurs » de la vue, voir
  // `game/companion_ai_window_profiles.cpp`.
  void ComposeProfiles(ro::sim::AiCompanion companion, std::uint16_t job, const std::string& edited,
                       ro::ui::CompanionProfilesView& into);
  void ComposePlayers(const ro::sim::CompanionAiSettings& settings,
                      const CompanionAiWindowBooks& books);
  // LA PROPOSITION DE CHAQUE COMPAGNON : sa phrase, et le message de chat qui
  // la signale, une fois par proposition et par partie.
  void NoteProposals(ro::sim::World& world, std::uint32_t now_ms);
  // Le nom du profil appliqué à ce compagnon ; l'IA basique avant le premier.
  [[nodiscard]] std::string AppliedProfile(const Slot& slot) const;
  // Les réglages des compagnons suivis, refaits depuis les profils.
  void RefreshSettings();
  // Pose la ligne dans le profil du compagnon, la note à enregistrer, et la
  // donne à son IA. Rend faux sans compagnon, table pleine ou ligne hors du
  // cadre.
  bool StoreTactic(ro::sim::AiCompanion companion, const ro::sim::CompanionTactic& tactic);
  // Retire la ligne, et l'enregistre aussitôt.
  bool DropTactic(ro::sim::AiCompanion companion, ro::sim::TacticKey key);
  // Fusionne les modifications en attente dans le fichier relu ; elles restent
  // en attente si rien n'a été écrit.
  void SaveProfiles();
  // L'enregistrement après le silence de la fenêtre « Avancé… ».
  void SaveQuietEdits(std::uint32_t now_ms);
  // Les skills que l'ouverture d'une tactique de ce compagnon peut nommer.
  void CollectOpenings(ro::sim::AiCompanion companion, const HomunculusStage& homunculus,
                       const MercenaryStage& mercenary, const ro::sim::SkillTree* skill_tree,
                       std::vector<ro::ui::TacticSkillChoice>& into) const;
  // LE PROFIL QU'UN GESTE DE CE COMPAGNON MODIFIE, voir
  // `CompanionAiSession::EditTarget` ; sans profil sans compagnon.
  [[nodiscard]] AiEditTarget EditTargetOf(ro::sim::AiCompanion companion);
  // Le geste fait : son refus retenu pour la fenêtre, ou l'IA mise à jour et
  // l'enregistrement armé.
  void Absorb(ro::sim::AiCompanion companion, const AiEditResult& result, std::uint32_t now_ms);

  // LA LIGNE QUE LA FENÊTRE « AVANCÉ… » MONTRE.
  struct EditedTactic {
    ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
    std::uint16_t monster_job = 0;
    std::string monster_name;
  };

  std::array<Slot, 2> slots_;
  std::uint32_t wanted_monster_sheet_ = 0;
  // Les entrées sur une carte déjà vues ; vide au début d'une partie, dont la
  // première carte est une entrée.
  std::optional<std::uint32_t> seen_map_entries_;
  // L'instant de la dernière entrée sur une carte : le délai d'arrivée court
  // depuis là, même pour une IA qui repart de zéro après.
  std::optional<std::uint32_t> arrived_ms_;
  // L'instant noté par `NoteArrival`, en attente du changement de carte.
  std::optional<std::uint32_t> noted_arrival_ms_;
  std::optional<CompanionAim> aim_;
  // Le grimoire était armé à l'image précédente.
  bool grimoire_was_armed_ = false;

  CompanionAiSession session_;
  // L'instant de la dernière retouche de la fenêtre « Avancé… » qui attend
  // d'être écrite.
  std::optional<std::uint32_t> edited_since_ms_;
  std::optional<EditedTactic> edited_;
  // Les skills que l'ouverture peut nommer, pour la vue de la fenêtre.
  std::vector<ro::ui::TacticSkillChoice> openings_;
  std::string window_title_;
  std::string window_companion_;

  // CE QUE LA VUE DE LA FENÊTRE « IA DU COMPAGNON » DÉSIGNE, refait à chaque
  // image où elle est ouverte.
  std::uint32_t last_now_ms_ = 0;
  // La fenêtre à l'image précédente : un départ ou une fermeture enregistre.
  AiWindowWatch window_watch_;
  // LE DERNIER GESTE REFUSÉ : pour quel compagnon (genre et classe), quelle
  // règle, et pourquoi.
  struct Refusal {
    int companion = -1;
    std::uint16_t job = 0;
    int rule = -1;
    std::string text;
  };
  Refusal refusal_;
  // Les monstres en vue, par classe, et leur nom.
  std::map<std::uint16_t, std::string> visible_monsters_;
  RuleVocabularyStore vocabulary_;
  std::vector<ro::ui::OffensiveSkillRow> offensive_rows_;
  std::vector<ro::ui::TacticRowView> tactic_rows_;
  std::vector<ro::sim::TacticKey> tactic_row_keys_;
  std::vector<ro::ui::TacticSkillChoice> ai_openings_;
  std::vector<ro::ui::RuleRowView> rule_rows_;
  std::vector<std::string> rejected_lines_;
  std::vector<std::string> friend_rows_;
  // Les statuts nommés par l'installation, triés par nom, et la taille de la
  // table qui les a donnés.
  std::vector<std::uint16_t> status_ids_;
  std::size_t status_table_size_ = 0;
  std::string window_companion_name_;
  // L'EN-TÊTE DES PROFILS ET L'ONGLET « JOUEURS », refaits à chaque image ; les
  // sources d'import, quand elles sont périmées.
  ProfilesViewStore profiles_view_;
  // Les réglages du profil édité, quand ce n'est pas l'appliqué.
  ro::sim::CompanionAiSettings edited_settings_;
  std::vector<ro::ui::ConductRowView> conduct_rows_;
  std::vector<ro::sim::PlayerConductKey> conduct_row_keys_;
  std::array<bool, ro::ui::kConductGenericChoices> conduct_generic_present_{};
  std::vector<ro::ui::RuleChoice> player_jobs_;
  std::size_t job_names_size_ = 0;
  // La proposition de chaque compagnon telle que sa phrase la dit.
  std::array<std::optional<EvolutionProposal>, 2> proposal_shown_;
  std::array<std::string, 2> proposal_texts_;
  // Le libellé du profil appliqué de chaque compagnon, pour son onglet.
  mutable std::array<std::string, 2> profile_labels_;
};

}  // namespace moonclient
