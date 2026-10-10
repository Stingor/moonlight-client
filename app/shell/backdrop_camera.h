#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/camera.h"

namespace moonclient {

// ── LA MISE EN SCÈNE DE L'ÉCRAN DE CONNEXION ────────────────────────────────
//
// 🔴 EN DUR, ET C'EST UNE DÉCISION, PAS UN OUBLI DE RÉGLAGE. Un écran de
// connexion est une mise en scène : elle est la même pour tout le monde, et
// elle doit l'être avant qu'aucun réglage ne soit lu — le fichier du joueur
// n'est pas encore le sien, il n'a pas ouvert de session.
//
// ⚠ CES QUATRE NOMBRES VIENNENT DES CURSEURS DE L'ÉCRAN DE VEILLE, figés
// ici. C'est le seul lien entre les deux : la veille les relit à
// chaque image et le joueur peut les bouger, le décor non. Les faire DÉRIVER
// des défauts de la veille rebrancherait par la bande ce que cette ligne
// refuse.

// La hauteur du regard au-dessus de l'horizon.
inline constexpr float kBackdropTiltDegrees = 45.0F;

// De combien le décor s'éloigne, en multiples du recul de jeu. ⚠ SOUS UN : le
// décor est PLUS PRÈS que la vue de jeu, il ne recule pas.
inline constexpr float kBackdropDistanceFactor = 0.65F;

// La vitesse de l'orbite, en degrés par seconde. Lente : le décor doit vivre
// sans attirer l'œil — il y a un formulaire par-dessus.
inline constexpr float kBackdropSpinDegreesPerSecond = 6.0F;

// LE DÉCOR SE MONTRE SANS BROUILLARD. La table des cartes le règle pour la vue
// de jeu ; sous le regard incliné du décor, qui balaie la ville entière en
// tournant, certaines villes s'y noieraient au point de ne plus rien montrer.
inline constexpr bool kBackdropFogShown = false;

// De combien les bords s'assombrissent. ⚠ ELLE HABILLE LA VILLE VIVANTE, et
// elle seule : l'illustration de repli se peint AVEC l'interface — voir
// `LoginWindowView::backdrop` —, donc bien après la passe de teinte.
inline constexpr float kBackdropVignette = 0.45F;

// LA POSE DU DÉCOR, à l'instant `elapsed_ms` depuis qu'il tourne.
//
// ⚠ ET CE N'EST PAS `WholeMapCamera`. Celle-là est la vue d'INSPECTION, au
// champ d'éditeur de quarante-cinq degrés : elle cadre la carte entière vue de
// loin, ce qui est exactement ce qu'un fond ne doit pas être. Le champ étroit
// du jeu est ce qui donne à Ragnarok son aspect presque orthographique, et le
// décor le garde.
//
// `elapsed_ms` est l'horloge de l'APPELANT : rien ici n'en lit aucune.
[[nodiscard]] ro::render::Camera BackdropCamera(glm::vec3 look_at, std::uint32_t elapsed_ms);

}  // namespace moonclient
