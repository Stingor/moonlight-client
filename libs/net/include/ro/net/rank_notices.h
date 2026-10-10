#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "ro/net/connection.h"

namespace ro::net {

// LES CLASSEMENTS ET LES AVIS « D'ÉTOILE » : ce que le serveur dit d'un rang et
// de ce qu'un personnage a désigné.
//
//   CZ 0x097C [op][type:2]                       « montre ce classement »
//   ZC 0x0AF6 [op][type:2][cid:4 ×10][points:4 ×10][mes points:4]
//   ZC 0x097E [op][type:2][gagnés:4][total:4]    des points de rang gagnés
//   ZC 0x020E [op][nom:24][id:4][étoile:1][code:1]
//   ZC 0x0AF7 [op][drapeau:2][cid:4][nom:24]     le nom d'un personnage
//
// 🔴 LE CLASSEMENT NE PORTE QUE DES IDENTIFIANTS DE PERSONNAGE : les noms se
// demandent à part, et arrivent quand ils arrivent. C'est à `app` de les
// attendre.

// Les classements que le serveur tient, dans la numérotation du fil.
enum class RankKind : std::uint8_t {
  kBlacksmith = 0,
  kAlchemist = 1,
  kTaekwon = 2,
  kKiller = 3,
};

// Combien de rangs un classement porte.
inline constexpr std::size_t kRankingSize = 10;

struct RankingList {
  RankKind kind = RankKind::kBlacksmith;
  // Zéro pour une place vide.
  std::array<std::uint32_t, kRankingSize> character_ids = {};
  std::array<std::uint32_t, kRankingSize> points = {};
  std::uint32_t own_points = 0;
};

struct RankingPoints {
  RankKind kind = RankKind::kBlacksmith;
  std::uint32_t gained = 0;
  std::uint32_t total = 0;
};

// Ce que dit un avis d'étoile, selon son code.
enum class StarNoticeCode : std::uint8_t {
  // Un lieu désigné par Feeling, puis le rappel de ce lieu.
  kPlaceDesignated = 0,
  kPlaceRecalled = 1,
  // Un monstre ou un métier désigné par Hatred, puis son rappel.
  kTargetDesignated = 10,
  kTargetRecalled = 11,
  // La cible d'une mission Taekwon, avec l'avancement en pour cent.
  kTaekwonMission = 20,
  kTamingMission = 21,
  // L'objet que le rang de collectionneur demande ; l'identifiant est l'objet.
  kCollectorItem = 22,
  // Lieux et cibles de l'ange remis à zéro.
  kStarsReset = 30,
  // Les points de vie d'une cible ; l'identifiant porte le nombre.
  kTargetHp = 40,
};

struct StarNotice {
  // Le lieu ou la cible, en UTF-8 : une carte, le nom d'un monstre ou d'un
  // métier selon le code.
  std::string name;
  // Un monstre, un métier ou un objet selon le code — ou un nombre.
  std::uint32_t subject_id = 0;
  // Le soleil, la lune ou l'étoile (0, 1, 2) ; l'avancement d'une mission.
  std::uint8_t star = 0;
  StarNoticeCode code = StarNoticeCode::kPlaceDesignated;
};

struct CharacterNameAnswer {
  std::uint32_t character_id = 0;
  // En UTF-8. Vide quand le serveur ne connaît pas ce personnage.
  std::string name;
};

// « Montre ce classement. » ⚠ ÉMET, ET RIEN DE PLUS : le serveur répond par
// `ReadRankingList`.
void RequestRanking(Connection& link, RankKind kind);

[[nodiscard]] std::optional<RankingList> ReadRankingList(const Frame& frame);
[[nodiscard]] std::optional<RankingPoints> ReadRankingPoints(const Frame& frame);
[[nodiscard]] std::optional<StarNotice> ReadStarNotice(const Frame& frame);
[[nodiscard]] std::optional<CharacterNameAnswer> ReadCharacterName(const Frame& frame);

}  // namespace ro::net
