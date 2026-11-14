// SOUS QUELLE ESPÈCE SE RÈGLE L'ÉTIQUETTE D'UN ACTEUR.
//
// 🔴 CE DÉCOUPAGE N'EST PAS CELUI DES BARRES DE VIE, et c'est le piège que ce
// banc garde. Les barres ont un onglet « groupe » et un fourre-tout « autres » ;
// les étiquettes ont un PNJ et un OBJET AU SOL à la place, parce que ce qui
// distingue une étiquette d'une autre, c'est ce que le serveur met dedans — un
// titre en toutes lettres pour l'un, rien du tout pour l'autre.
//
// ⚠ ET IL N'Y A PAS D'ESPÈCE « GROUPE » : le nom d'un compagnon arrive comme
// celui de n'importe quel joueur. Un onglet de plus promettrait une distinction
// que le protocole ne fait pas.

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/entity_names.h"
#include "ro/render/name_plate.h"
#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "world/overlay_view.h"

using moonclient::HasNamePlate;
using moonclient::NameCutAtHash;
using moonclient::NamePlateKindOf;
using ro::render::NamePlateKind;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::BossRank;
using ro::sim::World;

namespace {

[[nodiscard]] Actor Someone(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  return actor;
}

}  // namespace

TEST_CASE("Espèce d'étiquette : la mienne se règle à part", "[app][noms]") {
  World world;
  world.set_player_id(1'000);
  CHECK(NamePlateKindOf(Someone(1'000, ActorKind::kPlayer), world) == NamePlateKind::kOwn);
  // ⚠ MOI PASSE AVANT TOUT : sans cet ordre, ma propre étiquette hériterait des
  // réglages faits pour les autres joueurs.
  CHECK(NamePlateKindOf(Someone(2'000, ActorKind::kPlayer), world) == NamePlateKind::kPlayer);
}

TEST_CASE("Espèce d'étiquette : un boss n'est pas un monstre ordinaire", "[app][noms]") {
  World world;
  Actor beast = Someone(2'000, ActorKind::kMonster);
  CHECK(NamePlateKindOf(beast, world) == NamePlateKind::kMonster);
  beast.boss = BossRank::kMiniBoss;
  CHECK(NamePlateKindOf(beast, world) == NamePlateKind::kBoss);
  beast.boss = BossRank::kMvp;
  CHECK(NamePlateKindOf(beast, world) == NamePlateKind::kBoss);
}

TEST_CASE("Espèce d'étiquette : le PNJ et le butin ont la leur", "[app][noms]") {
  World world;
  CHECK(NamePlateKindOf(Someone(3'000, ActorKind::kNonPlayer), world) == NamePlateKind::kNonPlayer);
  Actor companion = Someone(3'001, ActorKind::kNonPlayer);
  companion.familiar = ro::sim::Familiar::kHomunculus;
  CHECK(NamePlateKindOf(companion, world) == NamePlateKind::kCompanion);
  CHECK(NamePlateKindOf(Someone(4'000, ActorKind::kItem), world) == NamePlateKind::kGroundItem);
  // Ce qui n'a pas d'espèce à lui se règle comme un joueur, faute de mieux.
  CHECK(NamePlateKindOf(Someone(5'000, ActorKind::kUnknown), world) == NamePlateKind::kPlayer);
}

// 🔴 UN PORTAIL N'A JAMAIS D'ÉTIQUETTE, ET AUCUN RÉGLAGE NE PEUT L'EN DONNER.
// C'est une garde du client — l'étiquette de survol compare le métier
// d'affichage à 45 et renonce — et elle compte parce que le serveur NOMME ses
// warps : « ggprowarp » a paru sous un portail dès que les PNJ ont été réglés
// sur « toujours ».
TEST_CASE("Étiquette : un portail n'en porte pas", "[app][noms]") {
  Actor portal = Someone(6'000, ActorKind::kNonPlayer);
  portal.look.job = ro::sim::kPortalJob;
  CHECK_FALSE(HasNamePlate(portal));

  // Tout le reste en porte une, y compris un PNJ ordinaire.
  Actor pnj = Someone(6'001, ActorKind::kNonPlayer);
  pnj.look.job = 100;
  CHECK(HasNamePlate(pnj));
  CHECK(HasNamePlate(Someone(6'002, ActorKind::kMonster)));
  CHECK(HasNamePlate(Someone(6'003, ActorKind::kItem)));

  // ⚠ ET LE MÉTIER SEUL NE SUFFIT PAS : quarante-cinq est un métier de PNJ, et
  // rien n'interdit à un monstre ou à un joueur de porter ce numéro-là.
  Actor monstre = Someone(6'004, ActorKind::kMonster);
  monstre.look.job = ro::sim::kPortalJob;
  CHECK(HasNamePlate(monstre));
}

// 🔴 LA COUPE AU CROISILLON EST POUR LES NON-JOUEURS, et ce dépôt a cru
// l'inverse : sept mille cent quarante et un PNJ de ce serveur portent un nom
// de script croisillonné, et le joueur lisait l'identifiant caché.
TEST_CASE("Étiquette : qui se coupe au croisillon", "[app][noms]") {
  CHECK(NameCutAtHash(Someone(7'000, ActorKind::kNonPlayer)));
  CHECK(NameCutAtHash(Someone(7'001, ActorKind::kMonster)));
  CHECK(NameCutAtHash(Someone(7'002, ActorKind::kItem)));
  CHECK_FALSE(NameCutAtHash(Someone(7'003, ActorKind::kPlayer)));
}

// 🔴 L'ÉCART DE NIVEAU NE PEINT QUE DES MONSTRES, ET LA GARDE EST ICI, pas dans
// le panneau. Le panneau ne propose la case que sous les deux onglets qui la
// méritent — c'est ce que le joueur a demandé en trouvant la page incohérente —
// mais le FICHIER de réglages s'ouvre dans un éditeur de texte, et un drapeau
// posé à la main sur les joueurs peindrait en rouge tous ceux qui nous
// dépassent.
//
// ⚠ ET LE NIVEAU D'UN JOUEUR EST BIEN LÀ : le paquet d'apparition le porte pour
// tout le monde, donc rien dans les nombres ne dirait « celui-ci ne compte
// pas ».
TEST_CASE("Étiquette : l'écart de niveau ne déborde pas sur les joueurs", "[app][noms]") {
  World world;
  world.set_player_id(1'000);
  REQUIRE(world.SetPlayerParameter(static_cast<std::uint16_t>(ro::sim::ServerParameter::kBaseLevel),
                                   50) == ro::sim::ParameterOutcome::kStored);

  ro::render::EntityNameLook how;
  how.level_gap = true;
  how.color = ro::render::PackedColor({0x10, 0x20, 0x30, 255});
  const std::array<std::uint8_t, 4> chosen = ro::render::UnpackedColor(how.color);

  // Vingt niveaux au-dessus des miens : le rouge du client.
  Actor beast = Someone(8'000, ActorKind::kMonster);
  beast.level = 70;
  CHECK(moonclient::NameColorOf(beast, how, world) == ro::render::kOverLevelNameColor);

  // Le même écart sur un joueur, un PNJ, un butin : la couleur de l'espèce
  // reste, et le réglage ne peut rien y faire.
  for (const ActorKind kind : {ActorKind::kPlayer, ActorKind::kNonPlayer, ActorKind::kItem}) {
    Actor other = Someone(8'001, kind);
    other.level = 70;
    CHECK(moonclient::NameColorOf(other, how, world) == chosen);
  }

  // ⚠ ET LA CASE ÉTEINTE NE PEINT RIEN NON PLUS, monstre compris : sans quoi ce
  // banc serait vert avec une garde qui refuserait tout.
  how.level_gap = false;
  CHECK(moonclient::NameColorOf(beast, how, world) == chosen);
}
