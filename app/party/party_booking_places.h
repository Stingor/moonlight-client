#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace moonclient {

// ── LES LIEUX DE LA RECHERCHE DE GROUPE ─────────────────────────────────────
//
// `data\yaml_files\party_booking.yml` : les régions et, dans chacune, les lieux
// qu'une annonce peut viser. Il remplace `party_booking.lub` du client
// d'origine, dont `scripts/yaml_tables/party_booking.py` le tire.
//
// 🔴 LE RANG EST LE CODE. Une annonce porte sur le fil la région en octet haut
// et le lieu en octet bas, comptés depuis zéro dans CET ordre : une région ou un
// lieu ne s'écarte donc jamais, même mal formé — il resterait un trou qui
// décalerait tous les suivants. Une entrée illisible garde sa place, vide.

inline constexpr std::string_view kPartyBookingPlacesPath = "data\\yaml_files\\party_booking.yml";

// La seule version de schéma que ce lecteur comprend.
inline constexpr int kPartyBookingPlacesVersion = 1;

// UNE RÉGION. ⚠ LES NOMS SONT EN UTF-8.
struct PartyBookingRegion {
  std::string name;
  // Faux pour une région qui ne se vise pas — « tous les lieux » : elle garde
  // son rang, mais ni la recherche ni la publication ne la proposent.
  bool offered = true;
  std::vector<std::string> places;
};

class PartyBookingPlaces {
 public:
  // Lit le texte du fichier. Illisible, d'une autre version ou sans liste de
  // régions : une erreur. ⚠ UNE ENTRÉE FAUTIVE NE CONDAMNE PAS LES AUTRES :
  // elle reste vide à sa place, et `problems` dit pourquoi.
  [[nodiscard]] static ro::core::Result<PartyBookingPlaces> Parse(std::string_view text);

  [[nodiscard]] std::span<const PartyBookingRegion> regions() const noexcept { return regions_; }
  // « Prontera — Fields » ; vide pour un code que la table ne connaît pas.
  [[nodiscard]] std::string NameOf(std::size_t region, std::size_t place) const;
  // Ce que la lecture a écarté, une ligne par fait, pour le journal.
  [[nodiscard]] std::span<const std::string> problems() const noexcept { return problems_; }

 private:
  std::vector<PartyBookingRegion> regions_;
  std::vector<std::string> problems_;
};

}  // namespace moonclient
