#pragma once

#include <cstdint>
#include <span>

namespace ro::render {

// Une sorte de coup qui pose un effet de FRAPPE, et lequel.
struct PlainHitByDamageType {
  std::uint8_t damage_type = 0;
  std::int32_t effect_id = -1;
};

// Une classe d'attaquant qui écarte l'effet du MESSAGE de son défaut.
struct PlainHitByJob {
  std::uint16_t job = 0;
  std::int32_t effect_id = -1;
};

// Ce que le client pose quand aucune des deux tables ne répond.
struct PlainHitEffectDefaults {
  // L'effet de frappe d'un coup qui n'est pas critique : AUCUN, et c'est
  // différent de zéro — zéro est EF_HIT1.
  std::int32_t strike = -1;
  // Celui du message : la gerbe ordinaire.
  std::int32_t message = 0;
  // Et ce que le client met quand on se frappe SOI-MÊME : ni effet ni son.
  std::int32_t self_inflicted = -2;
};

[[nodiscard]] std::span<const PlainHitByDamageType> PlainHitStrikeEffectTable();
[[nodiscard]] std::span<const PlainHitByJob> PlainHitMsgEffectTable();
[[nodiscard]] PlainHitEffectDefaults PlainHitDefaults();

}  // namespace ro::render
