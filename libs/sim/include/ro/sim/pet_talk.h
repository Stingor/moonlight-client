#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ro/sim/pet.h"

namespace ro::sim {

// ── LA PAROLE DES FAMILIERS ────────────────────────────────────────────────
//
// 🔴 C'EST LE CLIENT DU MAÎTRE QUI FAIT PARLER SON FAMILIER. Le serveur ne
// porte aucun texte : le client du maître choisit l'occasion, l'écrit dans UN
// entier qu'il envoie, et le serveur le renvoie tel quel à tous ceux qui voient
// le familier. Chacun retrouve alors la réplique dans SA table, et la tire au
// sort lui-même.
//
// L'entier se lit en chiffres décimaux, du plus faible au plus fort :
//
//   * le RESTE (chiffre des unités) : deux veut dire « une émoticône », dont le
//     numéro est l'entier divisé par dix ; toute autre valeur veut dire « une
//     réplique » ;
//   * pour une réplique, les dizaines disent le rang de faim, les centaines
//     l'occasion, et ce qui reste au-dessus du millier la classe du familier.
//
// Une réplique part avec un reste nul.
//
// 🔴 LE RESTE TROIS EST LE SALUT À LA CONNEXION, une extension du serveur
// Moonlight : l'occasion « connect » porte le numéro dix, qui ne tient pas dans
// le chiffre des centaines. Le serveur l'envoie au nom du maître sous la forme
// d'une réplique « repos » (occasion quatre) au reste trois : un client qui
// ignore l'extension montre une réplique de repos de la bonne espèce, celui-ci
// une réplique de connexion.
inline constexpr std::int32_t kPetConnectRest = 3;
// Le chiffre d'occasion que la table donne au salut.
inline constexpr int kPetConnectOccasionDigit = 10;

// LES OCCASIONS, numérotées comme le chiffre des centaines.
enum class PetTalkOccasion : std::uint8_t {
  kFeeding = 0,
  kHunting = 1,
  kDanger = 2,
  kDead = 3,
  kStand = 4,
  kSpecialPerformance = 5,
  kLevelUp = 6,
  kPerformance1 = 7,
  kPerformance2 = 8,
  kPerformance3 = 9,
};

// L'entier d'une réplique.
[[nodiscard]] std::int32_t PetLineCode(std::uint32_t pet_class, PetTalkOccasion occasion,
                                       int hunger_rank) noexcept;
// L'entier de l'émoticône qui accompagne une occasion, selon les rangs de faim
// et d'intimité ; rien quand le client n'en montre pas.
[[nodiscard]] std::optional<std::int32_t> PetEmotionCode(int hunger_rank, int intimacy_rank,
                                                         PetTalkOccasion occasion) noexcept;

// 🔴 UNE RÉPLIQUE NE PART QUE D'UN FAMILIER FIDÈLE : en deçà de ce seuil
// d'intimité, seule l'émoticône part. Le client se le dit à l'envoi.
inline constexpr int kPetTalkIntimacyAtLeast = 900;
[[nodiscard]] bool PetActLeaves(std::int32_t code, int intimacy) noexcept;

// CE QU'UN ENTIER REÇU DIT.
struct PetLineHeard {
  std::uint32_t pet_class = 0;
  // Les deux chiffres TELS QUE LUS : la table décide de ce qu'ils nomment.
  int hunger_digit = 0;
  int occasion_digit = 0;

  friend bool operator==(const PetLineHeard&, const PetLineHeard&) = default;
};
struct PetEmotionHeard {
  std::int32_t emotion = 0;

  friend bool operator==(const PetEmotionHeard&, const PetEmotionHeard&) = default;
};
using PetActHeard = std::variant<PetLineHeard, PetEmotionHeard>;

[[nodiscard]] PetActHeard ReadPetActCode(std::int32_t code) noexcept;

// LA LIGNE QUE L'ON VOIT, dans la bulle comme dans la conversation :
// « [Pet] <nom de l'acteur> : <réplique> ». La marque distingue la réplique
// d'un familier de ce qu'un joueur du même nom aurait tapé.
[[nodiscard]] std::string PetLineShown(const std::string& speaker_name, std::string_view line);

// L'OCCASION D'UN SPECTACLE, selon le numéro que le serveur tire : de un à
// trois les trois numéros ordinaires, quatre le spectacle spécial. Zéro n'en
// est pas un.
[[nodiscard]] std::optional<PetTalkOccasion> PerformanceOccasion(std::int32_t performance) noexcept;

// Les deux tirages du client, en pour cent, et l'écart qu'il exige entre deux
// répliques de chasse ou de danger — le même témoin pour les deux.
inline constexpr int kPetHuntingChancePercent = 25;
inline constexpr int kPetDangerChancePercent = 50;
inline constexpr int kPetStandChancePercent = 25;
inline constexpr std::uint32_t kPetHuntingQuietMs = 60000;
inline constexpr std::uint32_t kPetDangerQuietMs = 5000;
// Le maître est en danger sous ce pourcentage de sa vie.
inline constexpr std::int64_t kPetDangerHealthBelowPercent = 25;
// Le familier arrêté parle au plus une fois, passé ce délai de repos.
inline constexpr std::uint32_t kPetStandAfterMs = 3000;

// LES ACTIONS DU PAQUET DES COUPS qui n'en sont pas : ramasser, s'asseoir, se
// relever. Elles n'ouvrent pas de chasse.
inline constexpr std::uint8_t kOwnerPickUpAction = 1;
inline constexpr std::uint8_t kOwnerSitDownAction = 2;
inline constexpr std::uint8_t kOwnerStandUpAction = 3;

// LES DÉGÂTS QUI ANNULENT UN COUP : le client ne lit pas le paquet qui les
// porte — ni parole, ni cible mémorisée.
inline constexpr std::int32_t kVoidStrikeDamage = -30000;
// UN COUP DE COMPÉTENCE dont le client ne fait rien quand il vise son propre
// lanceur : la ruée du loup (RA_WUGDASH). Aucune cible ne se mémorise.
inline constexpr std::uint16_t kSelfIgnoredStrikeSkill = 2242;

// 🔴 LA CHASSE SE JUGE SUR CE QU'ÉTAIT LE MAÎTRE AVANT SON COUP, et non à chaque
// coup : il faut qu'il se tienne AU REPOS — debout, ni en marche, ni en plein
// geste — et que la cible de ce coup ne soit pas celle qu'il a MÉMORISÉE.
//
// Son acteur mémorise la cible de chacun de ses coups, quelle qu'en soit
// l'action — ramasser et s'asseoir compris —, celle de chacun de ses coups de
// compétence et celle de chacune de ses incantations qui vise quelqu'un. Frapper
// encore la même cible ne fait donc rien dire ; en changer, revenu au repos, si.
// L'entrée sur une carte oublie la cible.
class PetHuntWatch {
 public:
  // Un coup du maître : vrai quand il ouvre une chasse. La cible est mémorisée
  // dans tous les cas.
  [[nodiscard]] bool Strike(bool at_rest, std::uint32_t target_id, std::uint8_t action) noexcept;
  // Un coup de compétence, ou une incantation qui vise quelqu'un.
  void Remember(std::uint32_t target_id) noexcept { remembered_target_id_ = target_id; }
  void Forget() noexcept { remembered_target_id_ = 0; }

 private:
  std::uint32_t remembered_target_id_ = 0;
};

// CE QUE LE CLIENT DU MAÎTRE FAIT DIRE À SON FAMILIER, et quand.
//
// Chaque méthode rend les entiers à envoyer, dans l'ordre — la réplique, puis
// l'émoticône —, déjà passés par `PetActLeaves`. Un tirage se donne en pour
// cent, de zéro à quatre-vingt-dix-neuf.
//
// ⚠ L'APPELANT S'ASSURE QUE L'ACTEUR DU FAMILIER EST EN VUE : sans lui, le
// client ne dit rien.
class PetChatter {
 public:
  // Le repas pris : la faim d'AVANT le repas, que le changement d'état qui le
  // précède a gardée.
  [[nodiscard]] std::vector<std::int32_t> Fed(const Pet& pet) const;
  // Le maître est mort.
  [[nodiscard]] std::vector<std::int32_t> OwnerDied(const Pet& pet) const;
  // Le maître a pris un niveau de base.
  [[nodiscard]] std::vector<std::int32_t> OwnerLevelledUp(const Pet& pet) const;
  // Le familier donne le spectacle que le serveur a tiré : une réplique, sans
  // émoticône. ⚠ UN NUMÉRO INCONNU fait partir l'entier nul, comme chez le
  // client — que tous liront comme le repas d'un Poring affamé.
  [[nodiscard]] std::vector<std::int32_t> Performed(const Pet& pet, std::int32_t performance) const;

  // Le maître ouvre une chasse — voir `PetHuntWatch` : un tirage, et un silence
  // depuis la dernière réplique de chasse ou de danger.
  [[nodiscard]] std::vector<std::int32_t> OwnerEngaged(const Pet& pet, std::uint32_t now_ms,
                                                       int roll_percent);
  // 🔴 LA VIE DU MAÎTRE VIENT D'ÊTRE REDITE par le paquet de paramètre qui la
  // porte — à CHAQUE paquet, que la valeur ait changé ou non : sous le seuil, un
  // tirage et un silence plus court.
  [[nodiscard]] std::vector<std::int32_t> OwnerHurt(const Pet& pet, std::int64_t health,
                                                    std::int64_t max_health, std::uint32_t now_ms,
                                                    int roll_percent);
  // À chaque image : le familier qui s'arrête de marcher parle peut-être, une
  // fois, après un moment de repos.
  [[nodiscard]] std::vector<std::int32_t> ObserveRest(const Pet& pet, bool walking,
                                                      std::uint32_t now_ms, int roll_percent);

  void Reset() { *this = PetChatter{}; }

 private:
  // Le témoin partagé de la chasse et du danger : vrai, et noté, quand le
  // silence exigé est écoulé.
  [[nodiscard]] bool QuietFor(std::uint32_t quiet_ms, std::uint32_t now_ms);

  std::optional<std::uint32_t> last_spoken_ms_;
  bool was_walking_ = false;
  std::optional<std::uint32_t> resting_since_ms_;
};

}  // namespace ro::sim
