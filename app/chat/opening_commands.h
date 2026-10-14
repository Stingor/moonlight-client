#pragma once

// LES COMMANDES DE CE CLIENT QUI OUVRENT UNE FENÊTRE : `/marche` et `/db`. Elles
// ne partent jamais au serveur, et la ligne se prend même quand la fenêtre
// manque.

#include <string_view>

namespace moonclient {

class DatabaseStage;
class MarketStage;

// Rend vrai quand la ligne était l'une d'elles. `/db <texte>` ouvre la base de
// données sur cette recherche.
[[nodiscard]] bool FollowOpeningCommand(std::string_view line, MarketStage* market,
                                        DatabaseStage* database);

}  // namespace moonclient
