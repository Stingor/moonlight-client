// SOUS QUELLE ESPÈCE SE RÈGLE LA BARRE D'UN ACTEUR.
//
// 🔴 « DANS MON GROUPE » SE LIT DANS LA LISTE DU GROUPE. La vie connue ne le
// dit pas : `ZC_NOTIFY_HP_TO_GROUPM 0x080E` part aussi vers le staff qui voit
// la vie de toute la zone (`clif_hpmeter`).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/sim/actor.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world.h"

#include "world/entity_bar_view.h"

using moonclient::EntityBarKindOf;
using moonclient::EntityBarsAbove;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;
using ro::ui::EntityBarKind;

namespace {

// Hors de tout groupe.
const ro::sim::PartyRoster kNoParty;

[[nodiscard]] Actor Someone(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  return actor;
}

}  // namespace

TEST_CASE("Espèce de barre : la mienne se règle à part", "[app][barres]") {
  World world;
  world.set_player_id(1'000);
  const Actor me = Someone(1'000, ActorKind::kPlayer);
  CHECK(EntityBarKindOf(me, world, kNoParty) == EntityBarKind::kOwn);
}

TEST_CASE("Espèce de barre : un boss n'est pas un monstre ordinaire", "[app][barres]") {
  World world;
  Actor beast = Someone(2'000, ActorKind::kMonster);
  CHECK(EntityBarKindOf(beast, world, kNoParty) == EntityBarKind::kMonster);
  beast.boss = ro::sim::BossRank::kMiniBoss;
  CHECK(EntityBarKindOf(beast, world, kNoParty) == EntityBarKind::kBoss);
  // ⚠ LES DEUX RANGS PARTAGENT LA BARRE : ce réglage-là ne sépare que « on le
  // regarde longtemps » du reste, et un MVP n'a pas de troisième onglet.
  beast.boss = ro::sim::BossRank::kMvp;
  CHECK(EntityBarKindOf(beast, world, kNoParty) == EntityBarKind::kBoss);
}

TEST_CASE("Espèce de barre : le groupe se lit dans sa liste, pas dans la vie connue",
          "[app][barres]") {
  World world;
  world.set_player_id(1'000);
  Actor other = Someone(3'000, ActorKind::kPlayer);
  CHECK(EntityBarKindOf(other, world, kNoParty) == EntityBarKind::kPlayer);

  // ⚠ LE STAFF REÇOIT LA VIE DE TOUTE LA ZONE par le paquet du groupe : une vie
  // connue ne fait pas un coéquipier.
  other.health = 400;
  other.max_health = 900;
  CHECK(EntityBarKindOf(other, world, kNoParty) == EntityBarKind::kPlayer);

  ro::sim::PartyRoster party;
  party.Replace("Groupe", {ro::sim::PartyMember{.account_id = 1'000},
                           ro::sim::PartyMember{.account_id = 3'000}});
  CHECK(EntityBarKindOf(other, world, party) == EntityBarKind::kParty);
  // Sa vie pas encore arrivée, il est déjà du groupe.
  other.health = -1;
  other.max_health = -1;
  CHECK(EntityBarKindOf(other, world, party) == EntityBarKind::kParty);
  CHECK(EntityBarKindOf(Someone(3'001, ActorKind::kPlayer), world, party) ==
        EntityBarKind::kPlayer);
}

TEST_CASE("Place des barres : ce qu'elles prennent, et de quel côté", "[app][barres]") {
  // 🔴 CE QUI SE POSE AILLEURS DOIT SAVOIR CE QU'ELLES OCCUPENT. Le nom s'écrit
  // sous les pieds, l'enseigne au-dessus de la tête : les deux visent le même
  // bord que la jauge, et sans cette mesure ils se posent dessus. Le client
  // écarte les siens de huit pixels (jauge à `y + 12`, nom à `y + 20`).
  World world;
  const Actor beast = Someone(5'000, ActorKind::kMonster);

  ro::ui::EntityBarsLook look;
  ro::ui::EntityBarLook& monsters =
      look.kinds[static_cast<std::size_t>(ro::ui::EntityBarKind::kMonster)];
  monsters.under_feet = true;
  monsters.offset_pixels = 12.0F;
  monsters.height_pixels = 5.0F;
  monsters.magic = false;

  constexpr float kNoZoom = 1.0F;
  // ⚠ RIEN DE L'AUTRE CÔTÉ : une enseigne qui monterait quand même flotterait
  // au-dessus de rien.
  CHECK(EntityBarsAbove(beast, world, kNoParty, look, kNoZoom) == Catch::Approx(0.0F));

  monsters.under_feet = false;
  CHECK(EntityBarsAbove(beast, world, kNoParty, look, kNoZoom) == Catch::Approx(17.0F));

  // Éteintes, elles ne prennent aucune place — de part ni d'autre.
  monsters.shown = false;
  CHECK(EntityBarsAbove(beast, world, kNoParty, look, kNoZoom) == Catch::Approx(0.0F));
}

TEST_CASE("Barres des marchands : une échoppe les fait taire, un salon non", "[app][barres]") {
  World world;
  world.set_player_id(1'000);
  Actor seller = Someone(3'000, ActorKind::kPlayer);
  ro::ui::EntityBarsLook look;
  REQUIRE(look.hide_vendors);

  // Sans enseigne, c'est un joueur comme un autre.
  CHECK_FALSE(moonclient::HidesEntityBarsOf(seller, world, look));

  ro::sim::Signboard board;
  board.kind = ro::sim::SignboardKind::kVendingShop;
  seller.signboard = board;
  CHECK(moonclient::HidesEntityBarsOf(seller, world, look));
  board.kind = ro::sim::SignboardKind::kBuyingStore;
  seller.signboard = board;
  CHECK(moonclient::HidesEntityBarsOf(seller, world, look));
  board.kind = ro::sim::SignboardKind::kChatRoom;
  seller.signboard = board;
  CHECK_FALSE(moonclient::HidesEntityBarsOf(seller, world, look));

  // Le réglage éteint, l'échoppe ne change plus rien.
  board.kind = ro::sim::SignboardKind::kVendingShop;
  seller.signboard = board;
  look.hide_vendors = false;
  CHECK_FALSE(moonclient::HidesEntityBarsOf(seller, world, look));

  // ⚠ JAMAIS MOI : ma barre suit son onglet, boutique ouverte ou non.
  look.hide_vendors = true;
  Actor me = Someone(1'000, ActorKind::kPlayer);
  me.signboard = board;
  CHECK_FALSE(moonclient::HidesEntityBarsOf(me, world, look));
}

TEST_CASE("Barres des marchands : l'enseigne ne monte plus au-dessus d'une barre tue",
          "[app][barres]") {
  World world;
  world.set_player_id(1'000);
  Actor seller = Someone(3'000, ActorKind::kPlayer);
  ro::sim::Signboard board;
  board.kind = ro::sim::SignboardKind::kVendingShop;
  seller.signboard = board;

  ro::ui::EntityBarsLook look;
  for (ro::ui::EntityBarLook& kind : look.kinds) {
    kind.shown = true;
    kind.under_feet = false;
  }
  constexpr float kNoZoom = 1.0F;
  CHECK(EntityBarsAbove(seller, world, kNoParty, look, kNoZoom) == Catch::Approx(0.0F));
  look.hide_vendors = false;
  CHECK(EntityBarsAbove(seller, world, kNoParty, look, kNoZoom) > 0.0F);
}

TEST_CASE("Place des barres : ce que l'échelle d'ancrage fait à la bande", "[app][barres]") {
  // 🔴 LA BARRE NE SUIT PAS LE ZOOM. La jauge du client est un widget de
  // soixante sur cinq PIXELS que rien n'agrandit ; seule son ORDONNÉE passe par
  // une échelle, qui vaut `hauteur de fenêtre / 480` fois l'échelle de dessin
  // du sprite.
  //
  // ⚠ LA CONSÉQUENCE EST CE QUE CE BANC GARDE : l'ÉCART s'échelonne, la HAUTEUR
  // de la pile non. Une barre de cinq pixels en fait cinq à tous les reculs.
  ro::ui::EntityBarLook monsters;
  monsters.under_feet = true;
  monsters.offset_pixels = 12.0F;
  monsters.height_pixels = 5.0F;
  monsters.magic = false;
  ro::ui::EntityBarView view;
  view.feet = ImVec2(0.0F, 0.0F);
  view.health.known = true;

  const auto bottom_at = [&](float scale) {
    view.scale = scale;
    const std::optional<ro::ui::ScreenBand> band = ro::ui::EntityBarsBandUnderFeet(monsters, view);
    REQUIRE(band.has_value());
    return band->bottom;
  };
  // Douze d'écart mis à l'échelle, plus cinq de barre qui ne bougent pas.
  CHECK(bottom_at(2.0F) == Catch::Approx(29.0F));
  CHECK(bottom_at(0.5F) == Catch::Approx(11.0F));
  CHECK(bottom_at(1.0F) == Catch::Approx(17.0F));
  // ⚠ ET PLUS AUCUNE BORNE : le client n'en a pas, parce que son échelle n'est
  // pas le zoom. Elle vient de la fenêtre et du corps, et ni l'une ni l'autre ne
  // part à cent.
  CHECK(bottom_at(10.0F) == Catch::Approx(125.0F));
  // Une échelle nulle colle l'écart aux pieds, et la barre garde sa hauteur.
  CHECK(bottom_at(0.0F) == Catch::Approx(5.0F));

  // Au-dessus de la tête, elle ne prend rien sous les pieds.
  monsters.under_feet = false;
  CHECK_FALSE(ro::ui::EntityBarsBandUnderFeet(monsters, view).has_value());
}

TEST_CASE("Place du nom : il ne s'écarte que d'une barre qu'il chevauche", "[app][barres]") {
  // 🔴 DEUX PLACES INDÉPENDANTES, COMME CHEZ LE CLIENT : un nom posé sous sa
  // barre ne bouge pas, et l'absence de barre ne laisse pas de trou.
  const ro::ui::ScreenBand bar{.top = 12.0F, .bottom = 17.0F};
  CHECK(ro::ui::NameTopClearOfBars(20.0F, 14.0F, bar) == Catch::Approx(20.0F));
  CHECK(ro::ui::NameTopClearOfBars(6.0F, 14.0F, std::nullopt) == Catch::Approx(6.0F));
  // Chevauchée, la barre repousse le nom juste sous elle.
  CHECK(ro::ui::NameTopClearOfBars(6.0F, 14.0F, bar) ==
        Catch::Approx(17.0F + ro::ui::kNameBarGapPixels));
  CHECK(ro::ui::NameTopClearOfBars(15.0F, 14.0F, bar) ==
        Catch::Approx(17.0F + ro::ui::kNameBarGapPixels));
  // Un nom entièrement au-dessus de la barre reste où il est.
  CHECK(ro::ui::NameTopClearOfBars(-10.0F, 14.0F, bar) == Catch::Approx(-10.0F));
}

// 🔴 L'ÉCHELLE D'ANCRAGE EST CELLE DU CLIENT, ET ELLE A TROIS PARTS : la
// fenêtre, le corps, et le RECUL. L'échelle qui multiplie l'écart du client
// vaut `échelle_de_profondeur(profondeur) × échelle_du_corps / 1,34`, où
// l'échelle de profondeur est `facteur_d_écran × profondeur / 7`. Elle porte
// donc la PERSPECTIVE, et c'est elle qui garde l'étiquette collée au sprite
// quand on zoome ; sans elle, l'étiquette décroche du sprite.
TEST_CASE("Place des barres : l'échelle d'ancrage a trois parts", "[app][barres]") {
  using ro::render::ScreenAnchorScale;

  // À la hauteur de référence, sans zoom et sur un corps ordinaire : un.
  CHECK(ScreenAnchorScale(480.0F, 1.0F, 1.0F) == Catch::Approx(1.0F));
  // La FENÊTRE : deux fois plus haute, deux fois plus bas sous les pieds.
  CHECK(ScreenAnchorScale(960.0F, 1.0F, 1.0F) == Catch::Approx(2.0F));
  // Le RECUL : c'est la part qui manquait, et celle qui se voit le plus.
  CHECK(ScreenAnchorScale(480.0F, 2.0F, 1.0F) == Catch::Approx(2.0F));
  CHECK(ScreenAnchorScale(480.0F, 0.25F, 1.0F) == Catch::Approx(0.25F));
  // Le CORPS : un monstre rapetissé rapproche sa barre de ses pieds.
  CHECK(ScreenAnchorScale(480.0F, 1.0F, 0.5F) == Catch::Approx(0.5F));
  // Les trois se multiplient.
  CHECK(ScreenAnchorScale(960.0F, 2.0F, 1.5F) == Catch::Approx(6.0F));

  // ⚠ AUCUNE BORNE : une borne casse la proportionnalité aux extrêmes, donc
  // exactement là où l'ancrage se voit décrocher. Le client n'en a pas.
  CHECK(ScreenAnchorScale(480.0F, 8.0F, 1.0F) == Catch::Approx(8.0F));
  CHECK(ScreenAnchorScale(480.0F, 0.05F, 1.0F) == Catch::Approx(0.05F));

  // ⚠ UNE FENÊTRE DÉGÉNÉRÉE NE COLLE RIEN AUX PIEDS : une hauteur nulle rendrait
  // zéro, et tout ce qui s'ancre se poserait sur le sprite.
  CHECK(ScreenAnchorScale(0.0F, 1.0F, 1.0F) == Catch::Approx(1.0F));
}

TEST_CASE("Espèce de barre : un compagnon se sépare du PNJ", "[app][barres]") {
  World world;
  const Actor npc = Someone(4'000, ActorKind::kNonPlayer);
  CHECK(EntityBarKindOf(npc, world, kNoParty) == EntityBarKind::kOther);
  const Actor unknown = Someone(4'001, ActorKind::kUnknown);
  CHECK(EntityBarKindOf(unknown, world, kNoParty) == EntityBarKind::kOther);

  for (const ro::sim::Familiar familiar :
       {ro::sim::Familiar::kPet, ro::sim::Familiar::kHomunculus, ro::sim::Familiar::kMercenary,
        ro::sim::Familiar::kElemental}) {
    Actor companion = Someone(4'002, ActorKind::kNonPlayer);
    companion.familiar = familiar;
    CHECK(EntityBarKindOf(companion, world, kNoParty) == EntityBarKind::kCompanion);
  }
}

TEST_CASE("Barre d'un compagnon : sa fiche porte la vie que l'acteur n'a pas", "[app][barres]") {
  constexpr std::uint32_t kMine = 4'100;
  const std::array<moonclient::OwnCompanionGauges, moonclient::kOwnCompanionSheets> own = {{
      {.actor_id = kMine, .health = 300, .max_health = 900, .magic = 20, .max_magic = 80},
      {.actor_id = 0, .health = 77, .max_health = 88},
  }};
  const ro::ui::EntityBarLook look =
      ro::ui::DefaultEntityBars()[static_cast<std::size_t>(EntityBarKind::kCompanion)];

  ro::ui::EntityBarView mine;
  mine.actor_id = kMine;
  CHECK_FALSE(ro::ui::ShowsEntityBar(look, mine));
  moonclient::TakeOwnCompanionGauges(mine, own);
  CHECK(mine.health.known);
  CHECK(mine.health.current == 300);
  CHECK(mine.health.most == 900);
  CHECK(mine.magic.current == 20);
  // Montrée intacte comme entamée : c'est ce que veut cette espèce.
  CHECK(ro::ui::ShowsEntityBar(look, mine));

  // ⚠ LE COMPAGNON D'UN AUTRE N'A RIEN : aucune barre, et jamais vide.
  ro::ui::EntityBarView stranger;
  stranger.actor_id = 4'101;
  moonclient::TakeOwnCompanionGauges(stranger, own);
  CHECK_FALSE(stranger.health.known);
  CHECK_FALSE(ro::ui::ShowsEntityBar(look, stranger));

  // Le rang vide d'une fiche absente ne répond à personne, pas même à zéro.
  ro::ui::EntityBarView nobody;
  moonclient::TakeOwnCompanionGauges(nobody, own);
  CHECK_FALSE(nobody.health.known);

  // Une fiche pas encore reçue ne fait pas une barre vide.
  const std::array<moonclient::OwnCompanionGauges, 1> unread = {{{.actor_id = kMine}}};
  ro::ui::EntityBarView waiting;
  waiting.actor_id = kMine;
  moonclient::TakeOwnCompanionGauges(waiting, unread);
  CHECK_FALSE(ro::ui::ShowsEntityBar(look, waiting));
}

TEST_CASE("Espèce de barre : un coéquipier du champ de bataille est du groupe, avant sa vie",
          "[app][barres][battleground]") {
  // Le camp suffit : la vie arrive à part, et la jauge ne doit pas changer de
  // règle entre les deux paquets.
  World world;
  world.set_player_id(1'000);
  constexpr std::uint16_t kOurCamp = 5;
  constexpr std::uint16_t kTheirCamp = 6;
  world.battlefield().TellCamp(1'000, kOurCamp, 1'000);
  world.battlefield().TellCamp(5'000, kOurCamp, 1'000);
  world.battlefield().TellCamp(5'001, kTheirCamp, 1'000);
  const Actor mate = Someone(5'000, ActorKind::kPlayer);
  const Actor foe = Someone(5'001, ActorKind::kPlayer);
  const Actor stranger = Someone(5'002, ActorKind::kPlayer);
  CHECK(EntityBarKindOf(mate, world, kNoParty) == EntityBarKind::kParty);
  CHECK(EntityBarKindOf(foe, world, kNoParty) == EntityBarKind::kPlayer);
  CHECK(EntityBarKindOf(stranger, world, kNoParty) == EntityBarKind::kPlayer);
  // Le joueur garde sa propre barre, camp ou non.
  CHECK(EntityBarKindOf(Someone(1'000, ActorKind::kPlayer), world, kNoParty) ==
        EntityBarKind::kOwn);
  // Une nouvelle carte oublie les camps.
  world.EnterMap("prontera", 150, 150);
  CHECK(EntityBarKindOf(mate, world, kNoParty) == EntityBarKind::kPlayer);
}
