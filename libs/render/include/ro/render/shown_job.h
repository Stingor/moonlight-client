#pragma once

#include <cstdint>

// LE MÉTIER QU'UN ACTEUR MONTRE : celui dont il prend le corps, et non celui
// qu'il exerce. Un chevalier sur son Peco montre le chevalier monté, un Rune
// Knight sur son dragon le corps du dragon de sa couleur, un marié le corps des
// noces. Tout ce qui se lit sur le corps se lit sur lui : le sprite, ses poses,
// la hauteur des effets, l'ancre des effets de chapeau, l'échelle.
//
// ⚠ LE SERVEUR N'EN DIT QUE LA MOITIÉ. Il annonce le chevalier et le croisé
// montés sous leur classe montée quand il a calculé leur apparence en selle ;
// pour tous les autres il annonce le métier, et lève un bit d'option. C'est au
// client de choisir le corps, et il le choisit à chaque annonce et à chaque
// changement d'option.
namespace ro::render {

// Les bits d'option du serveur qui changent le corps, dans `e_option`.
inline constexpr std::uint32_t kOptionPeco = 0x20;
inline constexpr std::uint32_t kOptionWedding = 0x1000;
inline constexpr std::uint32_t kOptionFlying = 0x8000;
inline constexpr std::uint32_t kOptionXmas = 0x10000;
inline constexpr std::uint32_t kOptionSummer = 0x40000;
inline constexpr std::uint32_t kOptionGreenDragon = 0x80000;
inline constexpr std::uint32_t kOptionWolfRider = 0x200000;
inline constexpr std::uint32_t kOptionMadogear = 0x400000;
inline constexpr std::uint32_t kOptionBlackDragon = 0x800000;
inline constexpr std::uint32_t kOptionWhiteDragon = 0x1000000;
inline constexpr std::uint32_t kOptionBlueDragon = 0x2000000;
inline constexpr std::uint32_t kOptionRedDragon = 0x4000000;
inline constexpr std::uint32_t kOptionHanbok = 0x8000000;
inline constexpr std::uint32_t kOptionOktoberfest = 0x10000000;

// Les corps de costume : ils remplacent celui du métier, quel qu'il soit.
inline constexpr std::uint32_t kWeddingJob = 22;
inline constexpr std::uint32_t kXmasJob = 26;
inline constexpr std::uint32_t kSummerJob = 27;
inline constexpr std::uint32_t kHanbokJob = 28;
inline constexpr std::uint32_t kOktoberfestJob = 29;
inline constexpr std::uint32_t kDressUpJob = 30;

// Ce que le serveur annonce par des ÉTATS, et non par des options, et qui
// change aussi le corps.
struct ShownBodyStates {
  // EFST_DRESS_UP : le second costume d'été.
  bool dressed_up = false;
  // EFST_MADOGEAR_TYPE. Le bit d'option du Madogear suffit aussi : c'est lui
  // que porte l'annonce, et l'état n'arrive qu'après.
  bool madogear = false;
};

// LE MÉTIER MONTRÉ, dans l'ordre où le client tranche :
//
//   1. le mariage, qui l'emporte sur tout ;
//   2. hors du vol de l'union des Star Gladiators, les costumes : Noël, été,
//      hanbok, Oktoberfest, puis le second costume d'été ;
//   3. le Peco — et le griffon de la Royal Guard, qui porte le même bit ;
//   4. le dragon, dont le premier bit levé choisit seul la couleur ;
//   5. le loup du ranger ;
//   6. le Madogear ;
//   7. le vol de l'union.
//
// ⚠ UN BIT SANS CORPS POUR CE MÉTIER NE CLÔT RIEN : l'examen continue avec la
// règle suivante. Un métier qu'aucune règle ne connaît se montre tel quel.
[[nodiscard]] std::uint32_t ShownJobFor(std::uint32_t job_id, std::uint32_t option,
                                        ShownBodyStates states = {}) noexcept;

// LE MÉTIER À PIED D'UN CORPS MONTÉ : le plus petit métier dont ce corps est la
// monture — Peco ou griffon, dragon, loup, Madogear, monture du shop. Un corps
// qui n'est la monture de rien se rend tel quel, et le vol de l'union n'en est
// pas une : ce corps-là reste le sien. C'est sur ce corps que le client mesure la
// place de repos de la tête.
[[nodiscard]] std::uint32_t FootJobOf(std::uint32_t shown_job) noexcept;

// VRAI POUR UN CORPS DE COSTUME : il ne dit plus rien du métier.
[[nodiscard]] bool IsCostumeJob(std::uint32_t shown_job) noexcept;

// VRAI POUR UN CORPS DE MADOGEAR, bébé et Meister compris. Ce corps ne tient
// ni arme ni bouclier, et porte à la place la lueur de sa machine.
[[nodiscard]] bool IsMadogearJob(std::uint32_t shown_job) noexcept;

// VRAI POUR UN CORPS DU VOL DE L'UNION : Star Gladiator, son bébé, Star Emperor,
// son bébé et Sky Emperor, en lévitation. Assis, ce corps flotte encore au-dessus
// du sol.
[[nodiscard]] bool IsUnionFlightJob(std::uint32_t shown_job) noexcept;

}  // namespace ro::render
