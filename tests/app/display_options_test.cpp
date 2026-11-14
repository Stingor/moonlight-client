// Comment l'affichage s'ecrit dans le fichier, et comment il se relit.

#include "options/display_options.h"

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using moonclient::DisplayModeOf;
using moonclient::InterfacePercentOf;
using moonclient::InterfaceScaleLabel;
using moonclient::InterfaceScaleOf;
using moonclient::kInterfaceScalePercents;
using moonclient::PlaceOf;
using moonclient::PlaceText;
using moonclient::ResolutionOf;
using moonclient::ResolutionText;
using ro::render::DisplayMode;
using ro::render::Resolution;
using ro::render::WindowPlace;

TEST_CASE("Affichage : une resolution se relit telle qu'on l'a ecrite", "[app][affichage]") {
  // 🔴 C'EST CET ALLER-RETOUR QUI JUSTIFIE D'ECRIRE LA TAILLE PLUTOT QUE SON
  // RANG. La liste vient de l'ecran : brancher un autre moniteur la reordonne,
  // et un rang relu designerait alors autre chose. Le texte, lui, dit la meme
  // chose sur toutes les machines.
  for (const Resolution size :
       {Resolution{.width = 640, .height = 480}, Resolution{.width = 1280, .height = 720},
        Resolution{.width = 3840, .height = 2160}}) {
    CHECK(ResolutionOf(ResolutionText(size)) == size);
  }
}

TEST_CASE("Affichage : ce qui ne s'ecrit pas ainsi rend une taille NULLE", "[app][affichage]") {
  // ⚠ UN CHAMP VIDE N'EST PAS UNE ERREUR : c'est « aucun choix fait », et la
  // fenetre garde alors la taille qu'elle a. Un fichier trafique doit atterrir
  // au meme endroit -- jamais sur une fenetre de zero pixel.
  for (const char* ecrit : {"", "x", "1280", "1280x", "x720", "abc", "0x0", "1280x0", "-8x600"}) {
    INFO(ecrit);
    const Resolution lu = ResolutionOf(ecrit);
    CHECK(lu.width == 0);
    CHECK(lu.height == 0);
  }
}

TEST_CASE("Affichage : une place se relit, et elle peut etre NEGATIVE", "[app][affichage]") {
  // 🔴 UN SECOND ECRAN POSE A GAUCHE DU PRINCIPAL A DES ABSCISSES NEGATIVES.
  // Une lecture qui refuserait le signe ramenerait la fenetre sur l'ecran
  // principal a chaque demarrage, et le joueur qui travaille a gauche la
  // retrouverait deplacee sans rien avoir touche.
  for (const WindowPlace where :
       {WindowPlace{.x = 0, .y = 0}, WindowPlace{.x = 100, .y = 50},
        WindowPlace{.x = -1920, .y = 0}, WindowPlace{.x = -80, .y = -40}}) {
    const auto lu = PlaceOf(PlaceText(where));
    REQUIRE(lu.has_value());
    CHECK(*lu == where);
  }
}

TEST_CASE("Affichage : « jamais placee » n'est pas « placee au coin »", "[app][affichage]") {
  // 🔴 LA DISTINCTION DECIDE DE LA PREMIERE PARTIE. Rendre une place nulle
  // collerait la fenetre en haut a gauche au tout premier demarrage, au lieu de
  // laisser le systeme la centrer -- et personne ne comprendrait pourquoi.
  CHECK_FALSE(PlaceOf("").has_value());
  CHECK_FALSE(PlaceOf("100").has_value());
  CHECK_FALSE(PlaceOf("cent,cinquante").has_value());
  // Tandis qu'une place ECRITE au coin se relit bien comme telle.
  const auto corner = PlaceOf("0,0");
  REQUIRE(corner.has_value());
  CHECK(corner->x == 0);
  CHECK(corner->y == 0);
}

TEST_CASE("Affichage : les deux separateurs ne se confondent pas", "[app][affichage]") {
  // ⚠ UNE TAILLE PREND UN « x », UNE PLACE UNE VIRGULE. Ce ne sont pas deux
  // longueurs mais deux coordonnees, et l'un des deux accepte le signe.
  CHECK_FALSE(PlaceOf(ResolutionText(Resolution{.width = 800, .height = 600})).has_value());
  const Resolution lu = ResolutionOf(PlaceText(WindowPlace{.x = 800, .y = 600}));
  CHECK(lu.width == 0);
}

TEST_CASE("Affichage : le rang du mode est FIGE, c'est un nombre qu'on ecrit", "[app][affichage]") {
  // ⚠ REORDONNER CETTE LISTE CHANGERAIT LE REGLAGE DE TOUS CEUX QUI L'ONT DEJA
  // POSE : le fichier porte un rang, pas un nom.
  CHECK(DisplayModeOf(0) == DisplayMode::kWindowed);
  CHECK(DisplayModeOf(1) == DisplayMode::kBorderless);
  CHECK(DisplayModeOf(2) == DisplayMode::kFullscreen);
  // Et tout le reste retombe sur le mode fenetre, le seul dont on soit sur
  // qu'il laisse le joueur atteindre ses reglages.
  CHECK(DisplayModeOf(-1) == DisplayMode::kWindowed);
  CHECK(DisplayModeOf(9) == DisplayMode::kWindowed);
}

// ── L'ECHELLE DE L'INTERFACE, PAR PALIERS ───────────────────────────────────

TEST_CASE("Echelle : les paliers vont d'une borne du moteur a l'autre", "[app][affichage]") {
  // 🔴 CE QUE CET ESSAI GARDE : la liste ne doit rien offrir que le moteur
  // refuserait, ni rien laisser hors d'atteinte. `SetInterfaceScale` BORNE ce
  // qu'on lui donne en silence — un palier au-dela se choisirait sans effet, et
  // le joueur verrait sa liste mentir.
  CHECK(InterfaceScaleOf(kInterfaceScalePercents.front()) == ro::ui::kSmallestInterfaceScale);
  CHECK(InterfaceScaleOf(kInterfaceScalePercents.back()) == ro::ui::kLargestInterfaceScale);
  // ⚠ ET ILS MONTENT : la liste se lit de haut en bas, et un rang sert de cle
  // de recherche.
  for (std::size_t rank = 1; rank < kInterfaceScalePercents.size(); ++rank) {
    CHECK(kInterfaceScalePercents[rank - 1] < kInterfaceScalePercents[rank]);
  }
}

TEST_CASE("Echelle : un palier se retrouve par son facteur", "[app][affichage]") {
  // C'est cet aller-retour qui permet d'ecrire le FACTEUR dans le fichier et de
  // montrer un POUR CENT dans la liste : le rang se recalcule a chaque
  // ouverture, et il doit retomber sur le palier qu'on avait choisi.
  for (const int percent : kInterfaceScalePercents) {
    CHECK(InterfacePercentOf(InterfaceScaleOf(percent)) == percent);
  }
}

TEST_CASE("Echelle : le pour cent s'ARRONDIT, il ne se tronque pas", "[app][affichage]") {
  // 🔴 UNE TRONCATURE FERAIT MANQUER SON PALIER A QUI VIENT DE LE CHOISIR. Un
  // facteur ne survit pas toujours intact a un aller-retour par le fichier ou
  // par le moteur : 1,4999 doit rester « 150 % », pas devenir « 149 % » et
  // ajouter une entree parasite a la liste.
  CHECK(InterfacePercentOf(1.4999F) == 150);
  CHECK(InterfacePercentOf(1.5001F) == 150);
  CHECK(InterfacePercentOf(0.999F) == 100);
  // Et ce qui ne tombe sur aucun palier garde sa valeur : c'est ce que
  // `/echelle` peut poser, et la liste lui fait une entree.
  CHECK(InterfacePercentOf(1.137F) == 114);
}

TEST_CASE("Echelle : le libelle porte l'insecable avant le signe", "[app][affichage]") {
  // ⚠ SANS LUI, LE « % » PASSE SEUL A LA LIGNE. La liste est etroite, et c'est
  // la premiere chose qui se coupe.
  CHECK(InterfaceScaleLabel(125) == "125\u00A0%");
  CHECK(InterfaceScaleLabel(50) == "50\u00A0%");
}
