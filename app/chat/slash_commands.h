#pragma once

#include <span>
#include <string_view>

namespace moonclient {

// LES COMMANDES « / » QUE MOONCLIENT PREND VRAIMENT EN CHARGE, telles que la
// liste au-dessus de la barre de chat les propose.
//
// 🔴 UNE SEULE TABLE, ET UN ESSAI QUI LA TIENT D'ACCORD AVEC LES LECTEURS. Les
// lecteurs de commandes sont épars — `player_command`, les bascules, les
// annonces, `SubmitChat` lui-même — et chacun garde ses mots. L'essai
// `slash_commands_test.cpp` vérifie les deux sens : chaque nom proposé est
// reconnu par un lecteur, et chaque nom reconnu est proposé. Une commande
// ajoutée à un lecteur s'ajoute donc ici, ou l'essai rougit.
//
// ⚠ LES ÉMOTICÔNES N'Y SONT PAS : elles viennent de leur propre table
// (`EmotionCommandRows`), seule source du lien entre un nom et un numéro.
// ⚠ NI LES COMMANDES DE DÉVELOPPEMENT, qui n'existent qu'en Debug.
struct SlashCommand {
  // Le nom, sans la barre oblique.
  std::string_view name;
  // Les autres graphies, sans la barre oblique, séparées par une espace.
  std::string_view aliases;
  // Ce qu'elle attend après le nom ; vide sans argument.
  std::string_view params;
  // Ce qu'elle fait, en français : un littéral marqué par `TrText`, traduit à
  // la lecture.
  const char* help = "";
  // La complétion pose une espace après le nom.
  bool takes_argument = false;
  // RÉSERVÉE AU STAFF : le client la tait pour un joueur, il ne la propose
  // donc qu'à un compte dont le groupe a un niveau.
  bool staff = false;
};

[[nodiscard]] std::span<const SlashCommand> SlashCommands();

// La commande numérotée des émoticônes, « /e <numéro> », à part de leur table.
[[nodiscard]] const SlashCommand& NumberedEmotionCommand();

// L'aide commune à toutes les émoticônes, marquée pour le catalogue.
[[nodiscard]] const char* EmotionCommandHelp();

// Le titre du sous-menu des macros qui porte ces commandes, marqué pour le
// catalogue.
[[nodiscard]] const char* ClientCommandsMenuLabel();

}  // namespace moonclient
