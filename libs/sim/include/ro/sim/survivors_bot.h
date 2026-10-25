#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_run.h"

namespace ro::sim {

// LE ROBOT DU MODE SURVIVORS : un joueur sans aléa, qui ne lit que ce qu'un
// joueur voit (positions, PV, menaces annoncées, offres) et jamais un
// générateur. Il sert l'outil d'équilibrage et la partie en or longue : même
// champ, même entrée, sous tout compilateur.

// Ses réglages. Les distances sont en sous-unités.
struct SurvivorsBotProfile {
  std::span<const std::uint16_t> skill_order;  // préférence des skills neufs
  std::span<const std::uint16_t> card_order;   // préférence des cartes, neuves ou montées
  std::int32_t threat_radius_units = 0;        // les monstres qu'il fuit
  std::int32_t panic_radius_units = 0;         // un monstre plus près : aucun détour
  std::int32_t gem_reach_units = 0;            // les gemmes et plantes qu'il va chercher
  std::int32_t edge_margin_units = 0;          // en deçà d'un bord, il revient vers le centre
  std::int32_t heal_below_pct = 0;             // sous ces PV en %, Meat et soin
  std::uint32_t steady_steps = 0;              // un cap tenu au moins autant de pas
};

class SurvivorsBot {
 public:
  explicit SurvivorsBot(const SurvivorsBotProfile& profile);
  // LE CAP DU PAS. Seize caps candidats (multiples de 4), notés par le
  // produit scalaire avec la somme de la fuite, du retour au centre, et du
  // désir ou de l'orbite ; un cap dont le point à une ou deux cases est muré
  // est écarté, un cap dont le point à deux cases tombe dans une menace
  // annoncée est pénalisé. Le meilleur l'emporte, le plus petit à égalité ;
  // kNoHeading quand rien ne pousse et que le héros ne se tient dans aucune
  // menace, ou que tous les caps sont murés. Le cap tenu ne change qu'après
  // `steady_steps` pas, ou s'il devient muré ou menacé.
  [[nodiscard]] SurvivorsInput Steer(const SurvivorsField& field);
  // LA RÉPONSE À UN CHOIX : l'offre la mieux classée par la table de priorité,
  // la première à égalité ; aucune relance, aucun saut, aucun bannissement.
  // Hors d'un choix, la première offre.
  [[nodiscard]] SurvivorsAnswer Choose(const SurvivorsField& field) const;

 private:
  SurvivorsBotProfile profile_;
  std::uint8_t held_heading_ = kNoHeading;
  std::uint32_t held_since_step_ = 0;
  // Les index des monstres proches du héros, dans l'ordre de `foes` : ceux
  // qu'un segment vers une gemme peut croiser. Réservé à la construction.
  std::vector<std::uint32_t> near_foes_;
};

// Le profil du Magicien, réglé par l'outil d'équilibrage.
[[nodiscard]] const SurvivorsBotProfile& MageBotProfile();

// Joue jusqu'à la fin de la partie, `most_steps` pas, ou une réponse refusée :
// `Answer(Choose)` à chaque choix, sinon `Advance(20 ms, Steer)`, en rappelant
// `on_step` (s'il en est un) après chaque pas. Rend le nombre de pas joués.
std::uint32_t PlayBot(SurvivorsPlay& play, SurvivorsBot& bot, std::uint32_t most_steps,
                      const std::function<void(const SurvivorsRun&)>& on_step);

}  // namespace ro::sim
