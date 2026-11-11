#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/core/byte_reader.h"
#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/style.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES COULEURS QUE LES JOUEURS ONT COMPOSÉES, diffusées par le serveur.
//
// `ZC_MOONLIGHT_STYLES` (0x0F27) arrive par LOTS, et ce n'est pas un luxe :
// entrer sur une carte peuplée fait paraître des dizaines de joueurs d'un
// coup, et un paquet par joueur multiplierait les trames pour rien.
//
// 🔴 AUCUNE COULEUR NE PASSE SUR LE FIL. Une entrée ne porte que des décalages
// par rampe, et chaque client recalcule la palette depuis le `.spr` qu'il
// possède déjà. C'est ce qui la fait tenir en cinquante-six octets au lieu du
// kilo-octet d'une palette — et c'est aussi ce qui exige que tous les clients
// détectent EXACTEMENT les mêmes rampes. Voir l'ADR 0050.
//
// ⚠ MUET SANS LA POIGNÉE DE MAIN : comme tous les `ZC` personnalisés de ce
// serveur, le lot ne part qu'aux clients qui se sont annoncés. Voir l'ADR 0040.
//
// ⚠ UNE VERSION DE FIL INCONNUE FAIT SAUTER L'ENTRÉE, jamais le lot : la
// disposition des réglages peut changer — un plafond de rampes différent, par
// exemple — et la lire quand même poserait des couleurs tirées d'octets mal
// découpés.
[[nodiscard]] FrameOutcome ApplyPlayerStylesFrame(const Frame& frame, sim::World& world);

// LE STYLISTE OUVRE LA FENÊTRE — `ZC_MOONLIGHT_STYLE_OPEN` (0x0F28).
//
// ⚠ TROIS MODES, ET LE TROISIÈME N'EST PAS LE DEUXIÈME : fermer, ouvrir,
// basculer. Un PNJ veut pouvoir dire « ouvre » sans risquer de refermer ce que
// le joueur venait d'ouvrir lui-même.
[[nodiscard]] FrameOutcome ApplyStyleOpenFrame(const Frame& frame, sim::World& world);

// CE QUE LE JOUEUR VIENT DE COMPOSER, envoyé au serveur qui le rediffusera.
//
// `body_key` désigne le corps auquel cette recette se rattache — voir
// `render::BodySpriteKey`. ⚠ ELLE NE VAUT QUE POUR LUI : les index d'une
// palette n'ont aucune signification commune d'un corps à l'autre.
//
// `make_main` DEMANDE d'en faire le style PRINCIPAL — le repli, celui que
// reprend un corps qui n'a pas le sien. ⚠ UNE DEMANDE, PAS UN ORDRE : c'est le
// serveur qui tient le repli, et il le marque dans ses lots. Sans elle, ou sur
// un serveur qui ne l'honore pas, le repli reste la première variante rangée.
//
// ⚠ PARTIE NE VEUT PAS DIRE RETENUE : le serveur jette sans un mot ce qu'il
// refuse, et seul l'écho de la zone dit ce qu'il a gardé. L'erreur ne dit que
// ce qui n'a pas pu partir — une liaison fermée.
[[nodiscard]] core::Result<void> SendPlayerStyle(Connection& link, const sim::StyleRecipe& recipe,
                                                 std::uint32_t body_key, bool make_main);

// EFFACER : ce corps-ci, ou TOUS.
//
// ⚠ DEUX DEMANDES ET NON UNE, parce que ce ne sont pas les mêmes gestes :
// « rendre ce corps à son apparence d'origine » et « je ne veux plus de
// couleurs du tout ». Les confondre ferait perdre toutes ses variantes — le
// serveur en range quatre — à qui n'en retirait qu'une.
[[nodiscard]] core::Result<void> SendPlayerStyleClear(Connection& link, std::uint32_t body_key);
[[nodiscard]] core::Result<void> SendPlayerStyleClearAll(Connection& link);

// UNE VARIANTE DU LOT 0x0F27 SANS SON GID : version, drapeaux, clé du corps,
// trois numéros, huit réglages. C'est ainsi qu'un style hors de vue porte ses
// corps (`ro/net/absent_styles.h`).
inline constexpr std::size_t kStyleBodyEntryBytes = 52;

struct StyleBodyEntry {
  std::uint32_t body_key = 0;
  sim::StyleRecipe recipe;
  // La variante de repli, que reprend un corps qui n'a pas la sienne.
  bool fallback = false;
};

// Lit une variante, et avance toujours de `kStyleBodyEntryBytes`. Rien si elle
// est d'une autre version, si elle efface, ou si sa clé est nulle.
[[nodiscard]] std::optional<StyleBodyEntry> ReadStyleBodyEntry(core::ByteReader& reader);

}  // namespace ro::net
