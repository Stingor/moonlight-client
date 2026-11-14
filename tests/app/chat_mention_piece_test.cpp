// LA MENTION POSÉE DANS LA SAISIE : jamais une ligne qui commence par « @ »,
// que le serveur lirait comme une commande.

#include "chat/chat_mention_piece.h"

#include <catch2/catch_test_macros.hpp>

#include "ro/net/chat_target.h"

using moonclient::MentionPiece;

TEST_CASE("Mention : en tête de ligne, le nom seul — le serveur n'y lit pas de commande",
          "[app][chat][mention]") {
  CHECK(MentionPiece("", "Who") == "Who ");
  CHECK(MentionPiece("   ", "Who") == "Who ");
  CHECK_FALSE(ro::net::IsServerCommandText(MentionPiece("", "Who")));
}

TEST_CASE("Mention : dans la phrase, « @nom » comme un mot", "[app][chat][mention]") {
  CHECK(MentionPiece("salut ", "Who") == "@Who ");
  CHECK(MentionPiece("salut", "Who") == " @Who ");
}
