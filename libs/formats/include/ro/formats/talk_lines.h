#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ro::formats {

// LES FICHIERS DE RÉPLIQUES du barde et de la danseuse.
//
// Deux compétences font DIRE quelque chose au personnage plutôt que de dessiner
// quoi que ce soit : la plaisanterie glaciale et le cri. Le client ouvre l'un de
// ces deux fichiers, en tire une ligne au sort, et l'envoie au serveur comme une
// parole ordinaire.
inline constexpr std::string_view kFrostJokeLinesPath = "data\\BA_frostjoke.txt";
inline constexpr std::string_view kScreamLinesPath = "data\\DC_scream.txt";

// LA RÉPLIQUE NUMÉRO `index`, ou vide s'il n'y en a pas.
//
// ⚠ LES LIGNES SONT SÉPARÉES PAR UN RETOUR CHARIOT, pas par un saut de ligne :
// c'est le retour chariot que le client compte, et lui seul.
//
// ⚠ ET LA PREMIÈRE LIGNE EST UN TITRE, que le client saute sans le dire. Son
// compteur ne s'incrémente qu'au premier retour chariot, si bien que la réplique
// numéro UN est la DEUXIÈME ligne du fichier — « FROST JOKE » et « SCREAM », les
// deux titres, ne sortent donc jamais. Compter depuis zéro les ferait dire.
//
// Les retours, les sauts de ligne et les tabulations sont retirés de la
// réplique : chacune commence par une tabulation dans le fichier.
[[nodiscard]] std::string TalkLine(std::string_view contents, std::size_t index);

}  // namespace ro::formats
