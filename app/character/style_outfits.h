#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/sim/style.h"
#include "ro/sim/style_outfit.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// LES TENUES DU JOUEUR : le code court qui partage tout son style, et la
// garde-robe de son compte.
//
// Une tenue prend ce que le serveur a VALIDÉ, tel qu'il l'a renvoyé : les corps
// de la v7 et les teintures que le personnage affiche, sous leurs clés
// affichées. Rien qu'un brouillon de l'éditeur ne pourrait y entrer.
//
// 🔴 UN CODE COLLÉ OU UNE TENUE RANGÉE NE FAIT QU'UNE PROPOSITION. Ce guichet la
// rend une fois (`TakeProposal`), et l'éditeur la verse dans ses brouillons ;
// rien ne part au serveur sans « Valider », avec toutes les gardes ordinaires.
class StyleOutfits {
 public:
  // La tenue que ce personnage porte, ou rien s'il n'a ni corps ni calque
  // validé.
  [[nodiscard]] static std::optional<ro::sim::StyleOutfit> Capture(std::uint32_t own_gid,
                                                                   const ro::sim::StyleBook& book);

  // Le serveur sait-il recevoir des demandes de tenue ? Sa cadence le prouve.
  [[nodiscard]] static bool Served(const ro::sim::World& world);
  // Une demande peut-elle partir maintenant ? Il faut le service, et la
  // cadence du serveur écoulée depuis la précédente.
  [[nodiscard]] bool CanSend(const ro::sim::World& world, std::uint32_t now_ms) const;

  // ── LES DEMANDES ───────────────────────────────────────────────────────────
  // Chacune rend la raison de ne pas partir : pas de service, trop tôt, une
  // tenue vide, un code ou un nom mal formés.
  ro::core::Result<void> Publish(ro::net::Connection& link, const ro::sim::World& world,
                                 const ro::sim::StyleOutfit& outfit, std::uint32_t now_ms);
  // `typed` : le code tel que le joueur l'a collé, tirets et minuscules compris.
  ro::core::Result<void> Fetch(ro::net::Connection& link, const ro::sim::World& world,
                               std::string_view typed, std::uint32_t now_ms);
  ro::core::Result<void> ListWardrobe(ro::net::Connection& link, const ro::sim::World& world,
                                      std::uint32_t now_ms);
  ro::core::Result<void> Store(ro::net::Connection& link, const ro::sim::World& world,
                               std::uint8_t slot, std::string_view name,
                               const ro::sim::StyleOutfit& outfit, std::uint32_t now_ms);
  ro::core::Result<void> Clear(ro::net::Connection& link, const ro::sim::World& world,
                               std::uint8_t slot, std::uint32_t now_ms);

  // ── CE QUI REVIENT ─────────────────────────────────────────────────────────

  // Lit le guichet du monde : une tenue reçue par son code devient la
  // proposition. À appeler une fois par image, avant `TakeProposal`.
  void Update(const ro::sim::World& world);
  // La tenue d'un rangement devient la proposition ; faux s'il est vide.
  bool ProposeFromWardrobe(const ro::sim::World& world, std::uint8_t slot);
  // 🔴 LA PROPOSITION SE CONSOMME : laissée en place, elle écraserait à chaque
  // image ce que le joueur retouche.
  [[nodiscard]] std::optional<ro::sim::StyleOutfit> TakeProposal();

  // Le dernier code que ce compte a publié, à montrer ; vide sinon.
  [[nodiscard]] const std::string& published_code() const noexcept { return published_code_; }

 private:
  [[nodiscard]] ro::core::Result<void> Ready(const ro::sim::World& world,
                                             std::uint32_t now_ms) const;
  void Sent(const ro::sim::World& world, std::uint32_t now_ms);

  std::uint32_t next_allowed_ms_ = 0;
  bool sent_once_ = false;
  std::uint32_t heard_ = 0;
  std::optional<ro::sim::StyleOutfit> proposal_;
  std::string published_code_;
};

}  // namespace moonclient
