#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

// CE QU'UN ÉTAT D'ACTION FAIT DESSINER À UNE CRÉATURE — monstre, PNJ, pet,
// homoncule, mercenaire, élémentaire : tout ce qui ne se compose pas comme un
// joueur.
//
// 🔴 UNE CRÉATURE N'A PAS LA TABLE DES TREIZE RANGS D'UN JOUEUR. Le client ne
// lui pose un rang que pour une poignée d'états — l'attente, la marche,
// l'attaque, la mort, la blessure, l'emploi, le sommeil — et, pour tout autre,
// il ne touche NI À L'ACTION, NI À L'IMAGE : la créature continue ce qu'elle
// jouait. Une créature qui incante ne prend donc aucune pose d'incantation.
namespace ro::render {

// CE QUI DÉCIDE, pour une créature, du rang qu'un état lui pose.
enum class CreatureMotionFamily : std::uint8_t {
  // Un PNJ : TOUT état le remet à l'attente, depuis sa première image.
  kNpc,
  // Monstre, pet : l'emploi le laisse tel qu'il est.
  kOrdinary,
  // Homoncule, élémentaire, et les deux familles de monstres qui leur
  // ressemblent : l'emploi joue le rang cinq.
  kSkillRank,
  // Mercenaire : l'emploi joue l'attaque.
  kMercenary,
};

// LE RANG qu'un état d'action pose à une créature, ou RIEN quand il la laisse
// telle qu'elle est — son action et son image. Les mercenaires humains ont
// leurs rangs à eux, par leur classe.
[[nodiscard]] std::optional<std::size_t> CreatureActionAnimationFor(std::int8_t action_state,
                                                                    CreatureMotionFamily family,
                                                                    std::uint32_t job_id) noexcept;

// 🔴 VRAI QUAND LE PAS DU MONDE NE FAIT PLUS AVANCER CET ÉTAT chez une
// créature : ni image, ni action, ni direction. Une créature qui y entre garde
// l'image où l'état l'a prise, vue comme à cet instant, jusqu'à la motion
// suivante — Spiral Pierce à l'incantation, entre autres.
[[nodiscard]] bool CreatureStateFreezes(std::int8_t action_state) noexcept;

}  // namespace ro::render
