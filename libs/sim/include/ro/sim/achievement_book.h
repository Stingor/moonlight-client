#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// LES SUCCÈS DU PERSONNAGE, tels que le serveur les dit : où en est chacun, le
// score et le rang, plus les deux choses que le client tient seul — les succès
// qu'on SUIT à l'écran, et les réclamations parties sans réponse.
//
// ⚠ UN SUCCÈS ABSENT N'EST PAS UN SUCCÈS INCONNU : le serveur ne liste que ceux
// qui ont commencé. Absent veut dire « rien de fait », et c'est ce que rend
// `Find` — un pointeur nul, que l'appelant lit comme une progression vierge.
//
// ⚠ ELLE NAÎT AVEC LA PARTIE ET MEURT AVEC ELLE, comme le livre des titres.

// Le serveur tient dix compteurs par succès.
inline constexpr std::size_t kAchievementCounterCount = 10;

// LE NUMÉRO D'AUCUN SUCCÈS. Le serveur pousse à la connexion une mise à jour
// qui ne porte que le score : son enregistrement est nul.
inline constexpr std::uint32_t kNoAchievement = 0;

// Le plus de succès qu'on suit à la fois.
inline constexpr std::size_t kMostTrackedAchievements = 3;

struct AchievementProgress {
  bool completed = false;
  // Le compteur de chaque objectif, dans l'ordre du catalogue.
  std::array<std::uint32_t, kAchievementCounterCount> counts = {};
  // L'instant de l'achèvement, en secondes Unix ; zéro tant qu'il n'est pas obtenu.
  std::uint32_t completed_at = 0;
  // La récompense est réclamée.
  bool rewarded = false;
};

// LE SCORE ET LE RANG, que chaque paquet de succès redit en entier.
//
// 🔴 LES POINTS DU RANG SONT RELATIFS AU PALIER, pas au score : `rank_points`
// est ce qu'on a déjà gagné DANS le palier courant, `rank_span` ce que le palier
// demande en tout. Ce sont les deux bornes de la jauge.
struct AchievementStanding {
  std::uint32_t total_points = 0;
  std::uint16_t rank = 0;
  std::uint32_t rank_points = 0;
  std::uint32_t rank_span = 0;
};

// Un succès tel que le fil le décrit.
struct AchievementRecord {
  std::uint32_t achievement_id = 0;
  AchievementProgress progress;
};

// Ce qu'un suivi demandé a donné.
enum class TrackOutcome : std::uint8_t {
  kTracked,
  kUntracked,
  // Déjà trois succès suivis : le client le dit, et n'ajoute rien.
  kFull,
};

class AchievementBook {
 public:
  // LA LISTE COMPLÈTE : le score, et chaque succès listé remplacé tel quel.
  //
  // ⚠ RIEN NE S'EN RETIRE : un succès absent de la liste garde ce qu'on en
  // savait, comme chez le client d'origine, qui ne fait qu'insérer.
  void ApplyList(const AchievementStanding& standing, std::span<const AchievementRecord> records);

  // LA MISE À JOUR D'UN SUCCÈS. Vrai quand elle l'ACHÈVE — inachevé ou inconnu
  // jusque-là, achevé maintenant : c'est le moment de la bannière. Un
  // enregistrement nul ne pose que le score.
  bool ApplyUpdate(const AchievementStanding& standing, const AchievementRecord& record);

  // L'accusé d'une réclamation, réussie ou non : elle n'est plus en vol.
  void SettleClaim(std::uint32_t achievement_id);

  [[nodiscard]] const AchievementStanding& standing() const noexcept { return standing_; }

  // La progression d'un succès, ou nullptr s'il n'a pas commencé.
  [[nodiscard]] const AchievementProgress* Find(std::uint32_t achievement_id) const;

  // Tous les succès connus, par numéro croissant.
  [[nodiscard]] std::span<const AchievementRecord> records() const noexcept { return records_; }

  // LA RÉCLAMATION EST-ELLE PERMISE ? Achevé, pas encore réclamé, pas déjà en
  // vol — et le succès doit rapporter quelque chose, ce que seul le catalogue
  // sait : l'appelant le dit par `rewards`. Ce sont les gardes du serveur ;
  // émettre hors d'elles ne vaudrait qu'un refus.
  [[nodiscard]] bool CanClaim(std::uint32_t achievement_id, bool rewards) const;

  // Note une réclamation partie. Faux si elle n'était pas permise.
  bool NoteClaim(std::uint32_t achievement_id, bool rewards);

  [[nodiscard]] bool ClaimPending(std::uint32_t achievement_id) const;
  // Une réclamation attend-elle sa réponse, quelle qu'elle soit ? « Tout
  // réclamer » n'en lance une qu'une fois la précédente close.
  [[nodiscard]] bool ClaimInFlight() const noexcept { return !pending_claims_.empty(); }

  // LE SUIVI, que le client tient seul : rien n'en part vers le serveur.
  TrackOutcome ToggleTracking(std::uint32_t achievement_id);
  [[nodiscard]] bool IsTracked(std::uint32_t achievement_id) const;
  // Les succès suivis, dans l'ordre où on les a ajoutés.
  [[nodiscard]] std::span<const std::uint32_t> tracked() const noexcept { return tracked_; }

  // Combien de fois l'état a changé : une vue composée se refait quand il bouge.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  AchievementRecord& Upsert(std::uint32_t achievement_id);

  AchievementStanding standing_;
  std::vector<AchievementRecord> records_;
  std::vector<std::uint32_t> pending_claims_;
  std::vector<std::uint32_t> tracked_;
  std::uint32_t revision_ = 0;
};

}  // namespace ro::sim
