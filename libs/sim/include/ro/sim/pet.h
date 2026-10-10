#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// ── LE FAMILIER DU JOUEUR ──────────────────────────────────────────────────
//
// Le serveur le décrit par deux trames : sa fiche entière (nom, niveau, faim,
// intimité, accessoire, classe) et ses changements d'état un par un. Son
// identifiant d'acteur arrive par le changement d'état « naissance », jamais
// par la fiche.
//
// 🔴 « LE FAMILIER EST PARTI » NE SE LIT SUR AUCUNE TRAME. Un retour à l'œuf
// ordinaire n'efface rien de ce que le serveur a dit, et le changement d'état
// « œuf » est aussi envoyé à l'éclosion. Le seul observable est la disparition
// de son ACTEUR — et un changement de carte le fait disparaître lui aussi, le
// temps que les apparitions de la carte suivante arrivent. Voir `ObservePresence`.

// La faim, de zéro à cent ; l'intimité, de zéro à mille.
inline constexpr int kPetHungerMax = 100;
inline constexpr int kPetIntimacyMax = 1000;

// Le client ne propose l'évolution qu'AU-DESSUS de ce seuil d'intimité.
inline constexpr int kPetEvolutionOfferedAbove = 900;
// Le serveur, lui, exige ce minimum : entre les deux, l'évolution proposée
// échoue.
inline constexpr int kPetEvolutionServerMinimum = 910;
// Sous ce seuil, la fiche s'ouvre d'elle-même, une fois par familier.
inline constexpr int kPetLowIntimacyBelow = 100;
// Le plus long nom que le serveur garde, en octets sur le fil.
inline constexpr std::size_t kPetNameMaxBytes = 23;
// L'absence CONTINUE de l'acteur au-delà de laquelle le familier est parti.
inline constexpr std::uint32_t kPetGoneAfterMs = 1000;

// Les cinq paliers du client, de 0 (affamé, distant) à 4 (rassasié, fidèle).
[[nodiscard]] int PetHungerRank(int hunger) noexcept;
[[nodiscard]] int PetIntimacyRank(int intimacy) noexcept;

// LE MOT QUI DIT L'INTIMITÉ, sur la fiche comme sur l'œuf.
enum class PetIntimacyWord : std::uint8_t {
  kHateWithPassion,
  kHate,
  kAwkward,
  kShy,
  kNeutral,
  kCordial,
  kLoyal,
  kUnknown,
};

// Le mot de la fiche. ⚠ LES MONTURES DORAM ONT LEUR BARÈME sous cent : la
// haine y remplace la distance.
[[nodiscard]] PetIntimacyWord PetIntimacyWordOf(int intimacy, std::uint32_t pet_class) noexcept;
// Le mot du rang d'un œuf, de 1 à 5 ; rien pour un rang inconnu.
[[nodiscard]] std::optional<PetIntimacyWord> PetEggRankWord(int rank) noexcept;

// POURQUOI UN NOM EST REFUSÉ AVANT D'ÊTRE ENVOYÉ.
enum class PetNameRefusal : std::uint8_t {
  kNone,
  kEmpty,
  kTooLong,
};

[[nodiscard]] PetNameRefusal CheckPetName(std::string_view wire_name) noexcept;

// POURQUOI L'ÉVOLUTION SERAIT REFUSÉE, dite avant le clic, dans l'ordre où le
// serveur teste.
enum class PetEvolutionRefusal : std::uint8_t {
  kNone,
  kIntimacy,
  kAccessory,
  kMaterials,
};

[[nodiscard]] PetEvolutionRefusal PetEvolutionRefusalOf(int intimacy, bool wears_accessory,
                                                        bool has_materials) noexcept;

// LE NOMBRE D'EXEMPLAIRES D'UN ITEM DANS LE SAC, toutes piles additionnées —
// comme le serveur compte les matériaux.
[[nodiscard]] std::int64_t CountInBag(const Inventory& bag, std::uint32_t item_id) noexcept;

// L'ŒUF DU FAMILIER SORTI : celui que le serveur marque « éclos » dans le sac.
// Zéro quand le sac n'en porte aucun.
[[nodiscard]] std::uint32_t HatchedEggOf(const Inventory& bag) noexcept;

// La famille que le serveur donne à un accessoire de pet.
inline constexpr std::uint8_t kPetAccessoryItemType = 8;

// UN ACCESSOIRE DE PET, tel que le sac le reconnaît : sa famille, et aucun
// emplacement d'équipement — il ne se porte pas sur le personnage. Le serveur
// juge seul s'il va au pet sorti.
[[nodiscard]] bool IsPetAccessoryItem(const InventoryItem& item) noexcept;

// Les changements d'état, numérotés comme le serveur.
enum class PetStateChange : std::uint8_t {
  kBirth = 0,
  kIntimacy = 1,
  kHunger = 2,
  kAccessory = 3,
  kPerformance = 4,
  kHairstyle = 5,
  kEgg = 6,
};

// LA FICHE QUE LE SERVEUR ENVOIE.
struct PetProperty {
  std::string name;
  bool renamed = false;
  int level = 0;
  int hunger = 0;
  int intimacy = 0;
  std::uint32_t accessory_item_id = 0;
  std::uint32_t pet_class = 0;
};

class Pet {
 public:
  // ── CE QUE LE SERVEUR DIT ─────────────────────────────────────────────────

  void ApplyProperty(const PetProperty& property);
  // Faux quand le changement ne regarde pas l'état du familier : apparence,
  // spectacle, ou familier d'un autre.
  bool ApplyStateChange(PetStateChange change, std::uint32_t actor_id, std::int32_t value);
  // L'écho de l'auto-alimentation.
  void AnnounceAutoFeed(bool enabled);
  // Le résultat d'une évolution, tel que le serveur le numérote.
  void AnnounceEvolution(std::uint32_t result) { evolution_result_ = result; }
  // La liste d'éclosion : les cases du sac qui portent un œuf.
  void OfferEggs(std::vector<std::uint16_t> client_slots);

  // ── CE QUE LE CLIENT FAIT DE SON CÔTÉ ─────────────────────────────────────

  // Le nom part sur le fil : la fiche le montre aussitôt, sans attendre la
  // prochaine du serveur.
  void Rename(std::string name) { name_ = std::move(name); }
  void CloseEggOffer() noexcept { eggs_offered_ = false; }

  // L'ACTEUR EST-IL LÀ, à cette image ? `map_entries` est le compteur
  // d'entrées sur une carte : après une entrée, l'absence ne compte qu'une fois
  // le familier reparu — un chargement peut durer plus que le délai.
  void ObservePresence(bool actor_seen, std::uint32_t map_entries, std::uint32_t now_ms);

  // ── CE QUI EST À DIRE, une fois ───────────────────────────────────────────

  // L'intimité vient de passer sous le seuil pour un familier pas encore
  // averti : la fiche doit s'ouvrir.
  [[nodiscard]] bool TakeLowIntimacyWarning() noexcept;
  // L'accessoire a changé d'apparence : la fiche est à redemander pour nommer
  // l'item porté.
  [[nodiscard]] bool TakeSheetWanted() noexcept;
  [[nodiscard]] std::optional<std::uint32_t> TakeEvolutionResult() noexcept;
  [[nodiscard]] std::optional<bool> TakeAutoFeedEcho() noexcept;
  // La liste d'éclosion vient d'arriver.
  [[nodiscard]] bool TakeEggOfferOpened() noexcept;

  // ── CE QU'ON EN LIT ───────────────────────────────────────────────────────

  // Le serveur a décrit un familier, et son acteur n'a pas disparu durablement.
  [[nodiscard]] bool known() const noexcept { return pet_class_ != 0 && !gone_; }
  // Son acteur est là à cette image : la fiche ne se dessine pas sur un absent.
  [[nodiscard]] bool seen() const noexcept { return known() && seen_; }
  [[nodiscard]] bool gone() const noexcept { return gone_; }
  // L'identifiant de son acteur ; zéro quand il n'y en a pas.
  [[nodiscard]] std::uint32_t actor_id() const noexcept { return known() ? actor_id_ : 0; }
  // Le dernier acteur que le serveur a annoncé, même tenu pour parti : c'est
  // lui qu'on guette pour savoir s'il revient.
  [[nodiscard]] std::uint32_t announced_actor_id() const noexcept { return actor_id_; }
  [[nodiscard]] std::uint32_t pet_class() const noexcept { return pet_class_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] bool renameable() const noexcept { return renameable_; }
  [[nodiscard]] int level() const noexcept { return level_; }
  [[nodiscard]] int hunger() const noexcept { return hunger_; }
  // La faim avant le dernier changement : elle dit le SENS de la variation.
  [[nodiscard]] int previous_hunger() const noexcept { return previous_hunger_; }
  [[nodiscard]] int intimacy() const noexcept { return intimacy_; }
  // L'item de l'accessoire ; zéro quand il n'y en a pas, ou que la fiche qui le
  // nomme n'est pas encore revenue.
  [[nodiscard]] std::uint32_t accessory_item_id() const noexcept { return accessory_item_id_; }
  [[nodiscard]] bool wears_accessory() const noexcept { return wears_accessory_; }
  [[nodiscard]] bool auto_feed() const noexcept { return auto_feed_; }
  [[nodiscard]] bool eggs_offered() const noexcept { return eggs_offered_; }
  [[nodiscard]] const std::vector<std::uint16_t>& offered_egg_slots() const noexcept {
    return offered_egg_slots_;
  }

 private:
  void NoteIntimacy(int intimacy);

  std::uint32_t actor_id_ = 0;
  std::uint32_t pet_class_ = 0;
  std::string name_;
  bool renameable_ = false;
  int level_ = 0;
  int hunger_ = 0;
  int previous_hunger_ = 0;
  int intimacy_ = 0;
  std::uint32_t accessory_item_id_ = 0;
  bool auto_feed_ = false;

  // La présence.
  bool seen_ = false;
  bool gone_ = false;
  std::uint32_t map_entries_ = 0;
  // Depuis quand l'acteur manque, ou rien s'il est là — ou si une entrée sur
  // une carte vient de remettre le compte à zéro.
  std::optional<std::uint32_t> absent_since_ms_;
  // Une entrée sur une carte a eu lieu et le familier n'a pas encore reparu :
  // son absence ne compte pas.
  bool awaiting_return_ = false;

  // L'accessoire : porté ou non, et l'apparence que le serveur en a dite.
  bool wears_accessory_ = false;
  std::int32_t accessory_view_ = 0;
  bool sheet_wanted_ = false;

  // Le familier déjà averti de sa basse intimité ; zéro quand l'intimité est
  // remontée.
  std::uint32_t low_intimacy_warned_id_ = 0;
  bool low_intimacy_warning_ = false;

  std::optional<std::uint32_t> evolution_result_;
  std::optional<bool> auto_feed_echo_;

  bool eggs_offered_ = false;
  bool egg_offer_opened_ = false;
  std::vector<std::uint16_t> offered_egg_slots_;
};

}  // namespace ro::sim
