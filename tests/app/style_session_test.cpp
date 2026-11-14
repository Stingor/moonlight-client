// LA SESSION DE L'ÉDITEUR DE STYLE : la recette, son seul écrivain, et ce que
// le serveur en fait.
//
// ⚠ ÉPROUVÉE SUR LES DÉFAUTS QUI NE SE VOIENT QU'EN JEU, et encore : un
// brouillon qui passe d'un personnage à l'autre, une page blanche refermée par
// le lot du serveur, une monture repeinte des réglages du corps à pied, un
// « Envoyé » qui ment, une coupe qui laisse l'éditeur « modifié » pour de bon.

#include "character/style_session.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/palette_ramps.h"
#include "ro/sim/style.h"

#include "character/style_recipe.h"
#include "character/style_store.h"

using moonclient::FromWire;
using moonclient::kStyleSendCadenceMs;
using moonclient::SameRecipe;
using moonclient::StyleDrafts;
using moonclient::StyleSendKind;
using moonclient::StyleSendState;
using moonclient::StyleSession;
using ro::render::PaletteRecipe;
using ro::render::RampAdjust;
using ro::sim::StyleRecipe;
using ro::sim::StyleVariants;

namespace {

constexpr std::uint32_t kOnFoot = 0x11111111;
constexpr std::uint32_t kMounted = 0x22222222;
constexpr std::uint32_t kStart = 1000;
// Au-delà de toute attente de l'éditeur : l'écho, l'annonce, le brouillon.
constexpr std::uint32_t kLongAfter = 60000;
constexpr std::int16_t kWornHair = 12;

// Une recette reconnaissable à la teinte de sa première pièce.
[[nodiscard]] StyleRecipe Wire(std::int16_t first_hue, std::int16_t hair_style = kWornHair) {
  StyleRecipe recipe;
  recipe.ramps[0].hue = first_hue;
  recipe.hair_style = hair_style;
  return recipe;
}

[[nodiscard]] StyleVariants Variants(std::uint32_t fallback_body) {
  StyleVariants variants;
  variants.fallback_body = fallback_body;
  return variants;
}

[[nodiscard]] StyleSession::Worn On(std::uint32_t body_key, const StyleVariants* mine,
                                    std::uint32_t book_revision) {
  return StyleSession::Worn{.body_key = body_key, .mine = mine, .book_revision = book_revision};
}

[[nodiscard]] RampAdjust Hue(std::int16_t hue) { return RampAdjust{.hue = hue}; }

}  // namespace

TEST_CASE("Session : sans geste, la recette suit ce que le corps porte", "[app][style]") {
  // ⚠ LE LOT PEUT ARRIVER APRÈS L'OUVERTURE : une seule tentative laisserait la
  // fenêtre vide pour toute la session.
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  CHECK(SameRecipe(session.recipe(), PaletteRecipe{}));

  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  const std::uint32_t before = session.revision();
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  CHECK(SameRecipe(session.recipe(), FromWire(Wire(40))));
  CHECK(session.revision() != before);
  CHECK_FALSE(session.has_edits());
}

TEST_CASE("Session : le verrou d'amorçage tient une page blanche", "[app][style]") {
  // 🔴 SANS LUI, LE LOT DU SERVEUR REFERMERAIT LA PAGE BLANCHE à l'image
  // suivante, sous les doigts du joueur.
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.BlankSlate();

  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  CHECK(SameRecipe(session.recipe(), PaletteRecipe{}));
  CHECK(session.has_edits());

  // Un code chargé tient de même.
  PaletteRecipe pasted;
  pasted.ramps[2] = Hue(90);
  session.Load(pasted);
  session.Observe(On(kOnFoot, &mine, 2), kStart);
  CHECK(SameRecipe(session.recipe(), pasted));
}

TEST_CASE("Session : la révision n'avance qu'au changement", "[app][style]") {
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  const std::uint32_t settled = session.revision();
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  CHECK(session.revision() == settled);

  session.SetPiece(1, Hue(30));
  const std::uint32_t edited = session.revision();
  CHECK(edited != settled);
  // Le même réglage reposé ne change rien.
  session.SetPiece(1, Hue(30));
  CHECK(session.revision() == edited);
  // Une pièce hors des huit est ignorée.
  session.SetPiece(ro::render::kMaxRamps, Hue(30));
  CHECK(session.revision() == edited);
}

TEST_CASE("Session : un changement de corps met le travail de côté", "[app][style]") {
  // 🔴 LES RANGS D'UN CORPS NE DÉSIGNENT PAS LES PIÈCES D'UN AUTRE. Garder la
  // recette à pied sur la monture montrait des couleurs que personne ne voit,
  // et « Valider » écrasait la variante de la monture.
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  mine.by_body[kMounted] = Wire(200);
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  session.SetPiece(3, Hue(77));
  const PaletteRecipe on_foot = session.recipe();

  // En selle : la variante propre de la monture.
  session.Observe(On(kMounted, &mine, 1), kStart);
  CHECK(session.body_key() == kMounted);
  CHECK(SameRecipe(session.recipe(), FromWire(Wire(200))));
  CHECK_FALSE(session.has_edits());

  // À pied : le travail en cours revient.
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  CHECK(SameRecipe(session.recipe(), on_foot));
  CHECK(session.has_edits());
}

TEST_CASE("Session : un corps sans variante montre le repli", "[app][style]") {
  // C'est ce que les autres clients montrent sur ce corps.
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  StyleSession session;
  session.Observe(On(kMounted, &mine, 1), kStart);
  CHECK(SameRecipe(session.recipe(), FromWire(Wire(40))));
  CHECK_FALSE(session.own_style());
  CHECK(session.style_count() == 1);
}

TEST_CASE("Session : un personnage neuf ne reprend pas le travail d'un autre", "[app][style]") {
  // 🔴 LA SESSION VIT AVEC LA PARTIE. Le brouillon d'un personnage teint
  // passait au suivant quand celui-ci ne portait encore rien.
  StyleSession first;
  first.Observe(On(kOnFoot, nullptr, 0), kStart);
  first.SetPiece(0, Hue(123));
  REQUIRE(first.has_edits());

  StyleSession next;
  next.Observe(On(kOnFoot, nullptr, 0), kStart);
  CHECK(SameRecipe(next.recipe(), PaletteRecipe{}));
  CHECK_FALSE(next.has_edits());
}

TEST_CASE("Session : « Revenir à mon style » range d'abord le brouillon", "[app][style]") {
  // 🔴 ANNULER LES RETOUCHES NE LES PERD PAS : elles partaient sans laisser de
  // trace, puis le brouillon s'effaçait parce que la recette coïncidait de
  // nouveau avec ce qu'on porte.
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  session.SetPiece(5, Hue(-60));
  const PaletteRecipe composed = session.recipe();

  session.BackToWorn();
  CHECK(SameRecipe(session.recipe(), FromWire(Wire(40))));
  REQUIRE(session.drafts().Find(kOnFoot) != nullptr);
  CHECK(SameRecipe(FromWire(*session.drafts().Find(kOnFoot)), composed));
  CHECK(session.TakeDraftsChanged());
  CHECK(session.OffersDraft());

  // Le temps passe : le brouillon reste.
  session.TickDraft(kStart);
  session.TickDraft(kStart + kLongAfter);
  CHECK(session.drafts().Find(kOnFoot) != nullptr);
  CHECK_FALSE(session.TakeDraftsChanged());

  // Et il se reprend.
  CHECK(session.LoadDraft());
  CHECK(SameRecipe(session.recipe(), composed));
}

TEST_CASE("Session : le brouillon s'écrit une fois la recette immobile", "[app][style]") {
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.SetPiece(0, Hue(10));
  session.TickDraft(kStart);
  // Un glissement continu n'écrit rien tant qu'il dure.
  session.SetPiece(0, Hue(11));
  session.TickDraft(kStart + 1000);
  session.TickDraft(kStart + 2000);
  CHECK_FALSE(session.TakeDraftsChanged());
  session.TickDraft(kStart + 3000);
  CHECK(session.TakeDraftsChanged());
  REQUIRE(session.drafts().Find(kOnFoot) != nullptr);
  CHECK(session.drafts().Find(kOnFoot)->ramps[0].hue == 11);
}

TEST_CASE("Session : le compte des pas croît au pas neuf, et à lui seul", "[app][style]") {
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  CHECK(session.steps_taken() == 0);
  session.SetPiece(0, Hue(10));
  CHECK(session.steps_taken() == 1);
  // Défaire et refaire parcourent l'historique, ils ne rangent rien.
  REQUIRE(session.Undo());
  REQUIRE(session.Redo());
  CHECK(session.steps_taken() == 1);
  // 🔴 AU-DELÀ DU PLAFOND, le compte croît encore.
  for (std::size_t step = 0; step <= moonclient::kStyleHistoryDepth; ++step) {
    session.SetPiece(0, Hue(static_cast<std::int16_t>(step % 2 == 0 ? 20 : 30)));
  }
  CHECK(session.steps_taken() == 2 + moonclient::kStyleHistoryDepth);
}

TEST_CASE("Session : « Partagé » à l'écho seulement", "[app][style]") {
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  session.SetPiece(0, Hue(90));
  const std::optional<PaletteRecipe> sent = session.Validate(kWornHair, kStart);
  REQUIRE(sent.has_value());
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart);
  CHECK(session.send_state() == StyleSendState::kWaiting);

  // ⚠ UN LOT QUI NE PORTE PAS LA DEMANDE N'EST PAS L'ÉCHO.
  session.Observe(On(kOnFoot, &mine, 2), kStart + 100);
  CHECK(session.send_state() == StyleSendState::kWaiting);

  // L'écho : notre recette, et la coupe que le personnage porte.
  mine.by_body[kOnFoot] = Wire(90);
  session.Observe(On(kOnFoot, &mine, 3), kStart + 200);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK_FALSE(session.has_edits());

  // L'annonce s'efface d'elle-même.
  session.Observe(On(kOnFoot, &mine, 3), kStart + kLongAfter);
  CHECK(session.send_state() == StyleSendState::kNone);
}

TEST_CASE("Session : sans écho, « Non retenu »", "[app][style]") {
  // 🔴 LE SERVEUR JETTE SANS UN MOT : un renvoi trop rapproché, une version
  // inconnue, des variables pas encore chargées.
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.SetPiece(0, Hue(90));
  REQUIRE(session.Validate(kWornHair, kStart).has_value());
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart);
  session.Observe(On(kOnFoot, nullptr, 0), kStart + kLongAfter / 2);
  CHECK(session.send_state() == StyleSendState::kRefused);
  // Le travail reste.
  CHECK(session.has_edits());

  // Et rien n'est parti du tout : on le dit sur-le-champ.
  session.Sent(StyleSendKind::kApply, /*left=*/false, kStart);
  CHECK(session.send_state() == StyleSendState::kRefused);
}

TEST_CASE("Session : la coupe portée se fige à la validation", "[app][style]") {
  // 🔴 LE SERVEUR RANGE MOINS UN MAIS RENVOIE LA COUPE PORTÉE : sans la figer,
  // la recette ne coïncidait jamais plus avec ce qu'on porte.
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.SetPiece(0, Hue(90));
  REQUIRE(session.recipe().hair_style < 0);
  const std::optional<PaletteRecipe> sent = session.Validate(kWornHair, kStart);
  REQUIRE(sent.has_value());
  CHECK(sent->hair_style == kWornHair);
  CHECK(session.recipe().hair_style == kWornHair);
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart);

  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(90);
  session.Observe(On(kOnFoot, &mine, 1), kStart + 100);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK_FALSE(session.has_edits());
}

TEST_CASE("Session : un écho qui range la couleur portée vaut accusé", "[app][style]") {
  // ⚠ UNE COULEUR DE CHEVEUX QU'ON N'IMPOSAIT PAS peut revenir remplie de celle
  // du personnage : l'écho n'en est pas moins le nôtre, et la recette la
  // reprend.
  constexpr std::int16_t kWornColor = 5;
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.SetPiece(0, Hue(90));
  REQUIRE(session.recipe().hair_palette < 0);
  REQUIRE(session.Validate(kWornHair, kStart).has_value());
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart);

  StyleVariants mine = Variants(kOnFoot);
  StyleRecipe echoed = Wire(90);
  echoed.hair_palette = kWornColor;
  mine.by_body[kOnFoot] = echoed;
  session.Observe(On(kOnFoot, &mine, 1), kStart + 100);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK(session.recipe().hair_palette == kWornColor);
  CHECK_FALSE(session.has_edits());
}

TEST_CASE("Session : valider ce qu'on porte déjà ne part pas", "[app][style]") {
  // ⚠ LE SERVEUR JETTERAIT LE RENVOI IDENTIQUE sans un mot, et l'éditeur
  // l'annoncerait refusé.
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  CHECK_FALSE(session.Validate(kWornHair, kStart).has_value());
  CHECK(session.send_state() == StyleSendState::kShared);
}

TEST_CASE("Session : la validation emporte le brouillon, à l'écho", "[app][style]") {
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.SetPiece(0, Hue(90));
  session.TickDraft(kStart);
  session.TickDraft(kStart + kLongAfter);
  REQUIRE(session.TakeDraftsChanged());
  REQUIRE(session.drafts().Find(kOnFoot) != nullptr);

  REQUIRE(session.Validate(kWornHair, kStart + kLongAfter).has_value());
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart + kLongAfter);
  // Avant l'écho, rien ne part.
  CHECK(session.drafts().Find(kOnFoot) != nullptr);

  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(90);
  session.Observe(On(kOnFoot, &mine, 1), kStart + kLongAfter + 100);
  CHECK(session.drafts().Find(kOnFoot) == nullptr);
  CHECK(session.TakeDraftsChanged());
}

TEST_CASE("Session : « Oublier » reprend le repli à l'écho", "[app][style]") {
  // 🔴 PAS DE PAGE BLANCHE : ce corps reprend le repli chez nous comme chez les
  // autres, et l'éditeur doit le montrer.
  StyleVariants mine = Variants(kMounted);
  mine.by_body[kMounted] = Wire(40);
  mine.by_body[kOnFoot] = Wire(200);
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  REQUIRE(session.own_style());
  session.Sent(StyleSendKind::kForgetBody, /*left=*/true, kStart);
  // Avant l'écho, la recette ne bouge pas : un refus laisse le travail en
  // place.
  CHECK(SameRecipe(session.recipe(), FromWire(Wire(200))));

  mine.by_body.erase(kOnFoot);
  session.Observe(On(kOnFoot, &mine, 2), kStart + 100);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK(SameRecipe(session.recipe(), FromWire(Wire(40))));
  CHECK_FALSE(session.own_style());
  CHECK_FALSE(session.has_edits());
}

TEST_CASE("Session : un corps retouché après la demande garde son travail", "[app][style]") {
  // ⚠ CE QU'ON COMPOSE ENTRE LA DEMANDE ET L'ÉCHO n'est pas ce que le serveur a
  // retenu : le suivre l'effacerait.
  StyleVariants mine = Variants(kMounted);
  mine.by_body[kMounted] = Wire(40);
  mine.by_body[kOnFoot] = Wire(200);
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  session.Sent(StyleSendKind::kForgetBody, /*left=*/true, kStart);
  session.SetPiece(4, Hue(15));
  const PaletteRecipe composed = session.recipe();

  mine.by_body.erase(kOnFoot);
  session.Observe(On(kOnFoot, &mine, 2), kStart + 100);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK(SameRecipe(session.recipe(), composed));
}

TEST_CASE("Session : « Supprimer » rend la page blanche à l'écho", "[app][style]") {
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  StyleDrafts drafts;
  static_cast<void>(drafts.Put(kOnFoot, Wire(7)));
  StyleSession session(drafts);
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  session.Sent(StyleSendKind::kDeleteAll, /*left=*/true, kStart);
  // Avant l'écho, rien ne s'efface.
  CHECK(session.drafts().Find(kOnFoot) != nullptr);

  // Le lot d'effacement : le livre ne garde plus rien de nous.
  session.Observe(On(kOnFoot, nullptr, 2), kStart + 100);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK(SameRecipe(session.recipe(), PaletteRecipe{}));
  CHECK(session.style_count() == 0);
  CHECK(session.drafts().Find(kOnFoot) == nullptr);
}

TEST_CASE("Session : défaire et refaire, un pas par geste", "[app][style]") {
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  CHECK_FALSE(session.can_undo());
  session.SetPiece(0, Hue(10));
  session.SetPiece(1, Hue(20));
  REQUIRE(session.can_undo());

  REQUIRE(session.Undo());
  CHECK(session.recipe().ramps[1].hue == 0);
  CHECK(session.recipe().ramps[0].hue == 10);
  CHECK(session.can_redo());
  REQUIRE(session.Redo());
  CHECK(session.recipe().ramps[1].hue == 20);

  // 🔴 UN GESTE NEUF EFFACE CE QU'ON AVAIT DÉFAIT.
  REQUIRE(session.Undo());
  session.SetPiece(2, Hue(30));
  CHECK_FALSE(session.can_redo());
}

TEST_CASE("Session : un glissement ne compte que pour un pas", "[app][style]") {
  constexpr std::uint32_t kStroke = 7;
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.SetPiece(0, Hue(10), kStroke);
  session.SetPiece(0, Hue(11), kStroke);
  session.SetPiece(0, Hue(12), kStroke);
  // Un nouvel appui est un autre trait.
  session.SetPiece(0, Hue(40), kStroke + 1);
  REQUIRE(session.Undo());
  CHECK(session.recipe().ramps[0].hue == 12);
  REQUIRE(session.Undo());
  CHECK(session.recipe().ramps[0].hue == 0);
  CHECK_FALSE(session.can_undo());
}

TEST_CASE("Session : un préréglage ou « revenir » se défont", "[app][style]") {
  // 🔴 C'EST CE QUI ÉCRASAIT LE TRAVAIL SANS RETOUR.
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  session.SetPiece(3, Hue(77));
  const PaletteRecipe composed = session.recipe();
  session.Load(FromWire(Wire(120)));
  REQUIRE(session.Undo());
  CHECK(SameRecipe(session.recipe(), composed));

  session.BackToWorn();
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  CHECK_FALSE(session.has_edits());
  REQUIRE(session.Undo());
  // ⚠ ET LE PAS DÉFAIT TIENT : le corps ne reprend pas ce qu'il porte.
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  CHECK(SameRecipe(session.recipe(), composed));
}

TEST_CASE("Session : l'historique est propre à chaque corps", "[app][style]") {
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  session.SetPiece(0, Hue(10));
  session.Observe(On(kMounted, nullptr, 0), kStart);
  CHECK_FALSE(session.can_undo());
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  CHECK(session.can_undo());
}

TEST_CASE("Session : « Valider » attend la cadence et l'écho", "[app][style]") {
  StyleSession session;
  session.Observe(On(kOnFoot, nullptr, 0), kStart);
  CHECK(session.CanSend(kStart));
  // Rien de composé, aucun style : rien à envoyer.
  CHECK_FALSE(session.HasSomethingToSend());
  session.SetPiece(0, Hue(90));
  CHECK(session.HasSomethingToSend());
  REQUIRE(session.Validate(kWornHair, kStart).has_value());
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart);
  // ⚠ PAS DE SECONDE DEMANDE TANT QUE LA PREMIÈRE ATTEND SON ÉCHO.
  CHECK_FALSE(session.CanSend(kStart + kStyleSendCadenceMs + 1));

  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(90);
  session.Observe(On(kOnFoot, &mine, 1), kStart + 100);
  REQUIRE(session.send_state() == StyleSendState::kShared);
  // L'écho est revenu, mais la seconde du serveur court encore.
  CHECK_FALSE(session.CanSend(kStart + 100));
  CHECK(session.CanSend(kStart + kStyleSendCadenceMs));
  CHECK_FALSE(session.HasSomethingToSend());
}

TEST_CASE("Session : prévenir avant d'évincer un corps", "[app][style]") {
  constexpr std::uint32_t kThird = 0x33333333;
  constexpr std::uint32_t kFourth = 0x44444444;
  constexpr std::uint32_t kFifth = 0x55555555;
  StyleVariants mine = Variants(kOnFoot);
  for (const std::uint32_t body : {kOnFoot, kMounted, kThird, kFourth}) {
    mine.by_body[body] = Wire(40);
  }
  StyleSession session;
  session.Observe(On(kOnFoot, &mine, 1), kStart);
  // Un corps déjà habillé se remplace sur place.
  CHECK_FALSE(session.EvictsOldest());
  session.Observe(On(kFifth, &mine, 1), kStart);
  CHECK(session.EvictsOldest());
  // Il reprend le principal : le valider tel quel lui donne le sien.
  CHECK(session.HasSomethingToSend());
}

TEST_CASE("Session : « en faire mon style principal »", "[app][style]") {
  StyleVariants mine = Variants(kOnFoot);
  mine.by_body[kOnFoot] = Wire(40);
  mine.by_body[kMounted] = Wire(60);
  StyleSession session;
  session.Observe(On(kMounted, &mine, 1), kStart);
  CHECK_FALSE(session.is_main());
  // 🔴 CE CORPS PORTE DÉJÀ SA RECETTE : valider ne ferait rien, mais le rang de
  // principal reste à demander.
  CHECK_FALSE(session.Validate(kWornHair, kStart).has_value());
  const std::optional<PaletteRecipe> sent = session.Validate(kWornHair, kStart, /*make_main=*/true);
  REQUIRE(sent.has_value());
  session.Sent(StyleSendKind::kMakeMain, /*left=*/true, kStart);

  // ⚠ UN LOT QUI PORTE LA RECETTE SANS LE RANG N'EST PAS L'ÉCHO.
  session.Observe(On(kMounted, &mine, 2), kStart + 100);
  CHECK(session.send_state() == StyleSendState::kWaiting);
  mine.fallback_body = kMounted;
  session.Observe(On(kMounted, &mine, 3), kStart + 200);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK(session.is_main());
}

namespace {

[[nodiscard]] ro::sim::StyleVerdict Told(ro::sim::StyleSubject subject,
                                         ro::sim::StyleVerdictResult result,
                                         std::uint32_t sequence) {
  return ro::sim::StyleVerdict{.subject = subject, .result = result, .sequence = sequence};
}

}  // namespace

TEST_CASE("Session : un refus du corps se dit sans attendre, avec sa raison", "[app][style]") {
  StyleSession session;
  StyleSession::Worn worn = On(kOnFoot, nullptr, 0);
  session.Observe(worn, kStart);
  session.SetPiece(0, RampAdjust{.hue = 90});
  REQUIRE(session.Validate(kWornHair, kStart).has_value());
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart);

  // ⚠ UN VERDICT D'AVANT LA DEMANDE N'EST PAS LE SIEN : il était déjà là.
  worn.body_verdict = Told(ro::sim::StyleSubject::kBody, ro::sim::StyleVerdictResult::kTooSoon, 0);
  session.Observe(worn, kStart + 50);
  CHECK(session.send_state() == StyleSendState::kWaiting);

  worn.body_verdict =
      Told(ro::sim::StyleSubject::kBody, ro::sim::StyleVerdictResult::kKeyRefused, 1);
  session.Observe(worn, kStart + 100);
  CHECK(session.send_state() == StyleSendState::kRefused);
  CHECK(session.refusal() == ro::sim::StyleVerdictResult::kKeyRefused);
  // Le travail reste en place.
  CHECK(session.has_edits());
}

TEST_CASE("Session : un corps renvoyé à l'identique est partagé", "[app][style]") {
  StyleSession session;
  StyleSession::Worn worn = On(kOnFoot, nullptr, 0);
  session.Observe(worn, kStart);
  session.SetPiece(0, RampAdjust{.hue = 90});
  REQUIRE(session.Validate(kWornHair, kStart).has_value());
  session.Sent(StyleSendKind::kApply, /*left=*/true, kStart);
  worn.body_verdict = Told(ro::sim::StyleSubject::kBody, ro::sim::StyleVerdictResult::kSame, 1);
  session.Observe(worn, kStart + 100);
  CHECK(session.send_state() == StyleSendState::kShared);
  CHECK_FALSE(session.refusal().has_value());
}

TEST_CASE("Session : les calques ont leur cadence, et leur verdict", "[app][style]") {
  constexpr std::uint32_t kCadence = 1000;
  StyleSession session;
  StyleSession::Worn worn = On(kOnFoot, nullptr, 0);
  session.Observe(worn, kStart);
  // 🔴 SANS LA CADENCE DU SERVEUR, rien ne part : il ne sait pas lire la trame.
  CHECK_FALSE(session.CanSendLayers(kStart));

  worn.layers_cadence_ms = kCadence;
  session.Observe(worn, kStart);
  REQUIRE(session.CanSendLayers(kStart));
  // Le corps peut partir dans le même clic : les cadences ne se partagent pas.
  session.SentLayers(/*left=*/true, kStart);
  CHECK(session.CanSend(kStart));
  CHECK_FALSE(session.CanSendLayers(kStart));
  CHECK(session.layers_send_state() == StyleSendState::kWaiting);

  worn.layers_verdict =
      Told(ro::sim::StyleSubject::kLayers, ro::sim::StyleVerdictResult::kNoAtelier, 1);
  session.Observe(worn, kStart + 100);
  CHECK(session.layers_send_state() == StyleSendState::kRefused);
  CHECK(session.layers_refusal() == ro::sim::StyleVerdictResult::kNoAtelier);
  CHECK_FALSE(session.CanSendLayers(kStart + 100));
  CHECK(session.CanSendLayers(kStart + kCadence));

  session.SentLayers(/*left=*/true, kStart + kCadence);
  worn.layers_verdict =
      Told(ro::sim::StyleSubject::kLayers, ro::sim::StyleVerdictResult::kAccepted, 2);
  session.Observe(worn, kStart + kCadence + 50);
  CHECK(session.layers_send_state() == StyleSendState::kShared);
  CHECK_FALSE(session.layers_refusal().has_value());

  // Sans verdict dans le délai, la demande est tenue pour refusée.
  session.SentLayers(/*left=*/true, kStart + 2 * kCadence);
  session.Observe(worn, kStart + 2 * kCadence + kLongAfter);
  CHECK(session.layers_send_state() != StyleSendState::kWaiting);
  CHECK(session.CanSendLayers(kStart + 2 * kCadence + kLongAfter));
}
