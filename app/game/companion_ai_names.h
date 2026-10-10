#pragma once

// LES NOMS QUE L'IA DES COMPAGNONS MONTRE : une classe de compagnon, un profil,
// la raison d'un refus. Déjà traduits.

#include <cstdint>
#include <string>
#include <string_view>

#include "game/companion_ai_profiles.h"

namespace moonclient {

// LE NOM D'UNE CLASSE DE COMPAGNON : l'espèce d'un homoncule, évolué ou non ;
// la lignée et le rang d'un mercenaire de guilde ; son numéro sinon.
[[nodiscard]] std::string CompanionClassName(std::uint16_t job);

// LE LIBELLÉ D'UN PROFIL : les noms gravés se traduisent, un nom donné par le
// joueur se dit tel quel.
[[nodiscard]] std::string ProfileLabelOf(std::string_view key);

// LA RAISON D'UN REFUS, traduite ; vide pour `kNone`.
[[nodiscard]] std::string ProfileRefusalText(ProfileRefusal refusal);

}  // namespace moonclient
