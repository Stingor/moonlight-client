#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// LA LISTE DES CHARRETTES : les apparences qu'on peut donner à la sienne.
//
// 🔴 DEUX CHEMINS L'OUVRENT, ET ILS NE RÉPONDENT PAS PAR LE MÊME PAQUET.
//
//   · `MC_CHANGECART` : le CLIENT dresse la liste lui-même, d'après le niveau de
//     base, et la réponse part sans identité.
//   · `MC_CARTDECORATE` : le SERVEUR envoie la liste, avec une identité que la
//     réponse doit rappeler.
//
// L'identité nulle distingue les deux, et c'est elle que la fenêtre renvoie.

// La compétence qui ouvre la liste dressée par le client.
inline constexpr std::uint16_t kChangeCartSkill = 154;
// Ce qu'elle coûte, en points de magie : sous ce seuil, la liste ne s'ouvre pas.
inline constexpr std::int64_t kChangeCartLeastSp = 40;

// LES TYPES QUE `MC_CHANGECART` PROPOSE à ce niveau de base, dans l'ordre de la
// liste : le type N dès que le niveau DÉPASSE son palier, en s'arrêtant au
// premier palier non franchi ; puis le type treize au-delà du niveau cent.
[[nodiscard]] std::vector<std::uint8_t> ChangeCartTypesFor(std::int64_t base_level);

// CE QUI EMPÊCHE D'OUVRIR LA LISTE, dans l'ordre où le client le vérifie : les
// points de magie d'abord, la charrette ensuite.
enum class ChangeCartRefusal : std::uint8_t { kNone, kNotEnoughSp, kNoCart };
[[nodiscard]] ChangeCartRefusal ChangeCartRefusalFor(std::int64_t sp,
                                                     std::int32_t cart_type) noexcept;

// Une liste proposée.
struct CartChoiceOffer {
  // ZÉRO pour la liste du client, l'identité du serveur sinon.
  std::uint32_t identity = 0;
  std::vector<std::uint8_t> cart_types;
};

class CartChoice {
 public:
  // La liste de `MC_CHANGECART`, dressée pour ce niveau.
  //
  // ⚠ UNE LISTE DÉJÀ OUVERTE NE SE REFAIT PAS : le client montre la fenêtre
  // telle qu'elle est.
  void OfferChange(std::int64_t base_level);

  // La liste du serveur. ⚠ ELLE REMPLACE CELLE QUI ÉTAIT OUVERTE, et une liste
  // vide n'ouvre rien.
  void OfferFromServer(std::uint32_t identity, std::span<const std::uint8_t> cart_types);

  // Un choix fait, ou la fenêtre fermée : la liste s'en va.
  void Close() noexcept;

  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] const CartChoiceOffer& offer() const noexcept { return offer_; }
  // Le rang de la liste montrée : il change à chaque liste nouvelle.
  [[nodiscard]] std::uint64_t serial() const noexcept { return serial_; }

 private:
  bool open_ = false;
  CartChoiceOffer offer_;
  std::uint64_t serial_ = 0;
};

}  // namespace ro::sim
