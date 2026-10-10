#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/core/error.h"

namespace ro::proto {

// Longueur exigée, en chiffres. C'est PINCODE_LENGTH côté serveur, et le champ
// du paquet fait exactement cette taille : il n'y a pas de place pour un
// terminateur, et un code plus court est refusé sans un mot.
inline constexpr std::size_t kPincodeLength = 4;

// La permutation des dix chiffres que la graine détermine.
//
// Le serveur la reconstruit à l'identique pour DÉFAIRE ce que le client a fait :
// il lit le chiffre reçu comme un indice dans cette table. La rendre séparément
// n'a d'intérêt que pour le test, qui rejoue le sens serveur.
[[nodiscard]] std::array<std::uint8_t, 10> PincodeTable(std::uint32_t seed);

// Brouille un code PIN pour l'envoi.
//
// Le code ne voyage JAMAIS tel quel : le serveur tire une graine à chaque envoi
// de HC_SECOND_PASSWD_LOGIN, en dérive une permutation des dix chiffres, et
// attend de recevoir les ANTÉCÉDENTS des vrais chiffres par cette permutation.
// Deux envois du même code donnent donc deux trames différentes.
//
// ⚠ La graine à passer est celle de la DERNIÈRE trame reçue : le serveur en
// tire une neuve à chaque état qu'il annonce, y compris pour dire « code
// faux ». Réutiliser la précédente ferait rejeter un code pourtant juste.
//
// Rend une erreur si le code n'est pas exactement kPincodeLength chiffres
// décimaux — le serveur, lui, coupe la liaison dans ce cas.
[[nodiscard]] core::Result<std::string> ObfuscatePincode(std::uint32_t seed,
                                                         std::string_view digits);

}  // namespace ro::proto
