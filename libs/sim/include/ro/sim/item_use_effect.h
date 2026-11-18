#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "ro/sim/chat_line.h"

namespace ro::sim {

// CE QUE L'USAGE D'UN OBJET MONTRE ET FAIT ENTENDRE, sur celui qui l'a utilisé.
//
// 🔴 L'ACCUSÉ D'USAGE PART À TOUTE LA ZONE. Quand l'usage réussit, le serveur
// envoie ZC_USE_ITEM_ACK2 (0x01C8) avec l'objet et l'AID de l'utilisateur à
// tous ceux qui le voient. Le client joue alors, sur cet acteur — soi, ou un
// voisin en vue —, ce que cette table dit de l'objet : une potion rouge montre
// l'effet 204, un pétard fait entendre `effect\itempokjuk.wav` puis montre le
// sien. Un usage refusé ne joue rien, et un acteur qui n'est pas en vue non
// plus : ni effet, ni son.
//
// ⚠ LA TABLE EST EN DUR DANS LE CLIENT : aucun fichier de l'installation ne la
// porte. Elle est engendrée, voir `generated/item_use_effect_table.cpp`.
//
// ⚠ LE SON SE JOUE À L'OREILLE, pas à la place de l'acteur : le client le pose
// à l'auditeur, à plein volume, que l'objet soit le sien ou celui d'un voisin
// au bord de l'écran.

// Au plus deux effets par objet : le Durian et le parchemin d'Assumptio.
inline constexpr std::size_t kItemUseEffectsMax = 2;

struct ItemUseEffect {
  std::uint32_t item_id = 0;
  // Les effets semés sur l'utilisateur, dans l'ordre où le client les lance.
  std::array<std::uint16_t, kItemUseEffectsMax> effect_ids{};
  std::uint8_t effect_count = 0;
  // Le son, relatif au dossier des sons du jeu ; vide : aucun.
  std::string_view sound;
};

// Toute la table, triée par objet. Engendrée.
[[nodiscard]] std::span<const ItemUseEffect> ItemUseEffects();

// Ce que l'usage de cet objet joue, ou rien : la plupart des objets ne jouent
// rien du tout.
[[nodiscard]] const ItemUseEffect* FindItemUseEffect(std::uint32_t item_id);

// LA LIGNE QU'UN OBJET ÉCRIT DANS LA DISCUSSION, pour SOI SEUL : le manuel
// d'expérience annonce son bonus, qui dépend du métier MONTRÉ — celui du corps,
// monture et costume compris.
enum class ItemUseLine : std::uint8_t {
  kNone,
  // « jusqu'à 50 % d'expérience pendant 30 minutes » : les troisièmes classes.
  kExpBonus50,
  // « jusqu'à 75 % » : tous les autres corps.
  kExpBonus75,
};

// La ligne de cet objet pour ce métier montré. Engendrée.
[[nodiscard]] ItemUseLine ItemUseLineFor(std::uint32_t item_id, std::uint32_t shown_job);

// Sa case de filtre et sa couleur (0xRRGGBB) : celles d'un changement d'état.
inline constexpr ChatType kItemUseLineType = ChatType::kAbnormalStatus;
inline constexpr std::uint32_t kItemUseLineColor = 0x00FFFF;

}  // namespace ro::sim
