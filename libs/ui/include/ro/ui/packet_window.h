#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/i18n.h"

namespace ro::ui {

// L'INSPECTEUR DE TRAMES : ce que chaque opcode reçu est devenu.
//
// C'est la première fenêtre du client, et ce n'est pas un hasard. Un opcode que
// le traducteur ne traite pas ne va nulle part : le jeu ne montre rien et ne se
// plaint de rien, et le joueur n'a qu'une impression — « ça ne marche pas ».
// Le relevé existait déjà, mais il se lisait par la commande `/trames`, dans un
// journal de chat qui n'en tient que huit lignes.
//
// ⚠ LA FENÊTRE NE CONNAÎT PAS `net`. Le DAG l'interdit — `ui ← core, render` —
// et c'est la même discipline que la `FrameScene` du renderer : `app` recopie
// le relevé en données pures, la fenêtre les dessine. Ce qui s'y gagne est
// concret : ces lignes se fabriquent à la main dans un test, sans serveur.

// Le nom de la fenêtre : titre montré, puis identité après `###`. Voir
// `kLogWindowName` — l'identité ne se traduit pas.
inline constexpr const char* kPacketWindowTitle = core::TrText("Trames reçues");
inline constexpr const char* kPacketWindowId = "packets";

// Une ligne du relevé. Les quatre compteurs PARTITIONNENT les trames d'un
// opcode : leur somme est le nombre de fois où il est arrivé.
struct PacketRow {
  std::uint16_t opcode = 0;
  std::uint32_t applied = 0;
  std::uint32_t ignored = 0;
  std::uint32_t unhandled = 0;
  std::uint32_t undecodable = 0;

  // Les trames qui n'ont RIEN donné : ni traduites, ni décodables. Ce sont les
  // seules qui décrivent du travail restant.
  [[nodiscard]] std::uint32_t missing() const noexcept { return unhandled + undecodable; }
  [[nodiscard]] std::uint32_t total() const noexcept { return applied + ignored + missing(); }
};

// Ce que le joueur a réglé, et qui doit survivre d'une image à la suivante.
//
// ⚠ EN MODE IMMÉDIAT, UNE FENÊTRE NE GARDE RIEN. Elle se redessine entière à
// chaque image, donc tout ce qui doit durer vit DEHORS, chez qui l'appelle.
// C'est ce qui rend une fenêtre vérifiable : on lui donne un état, on la
// dessine, on regarde l'état qu'elle rend.
struct PacketWindowState {
  bool open = false;
  // N'afficher que les opcodes dont au moins une trame s'est perdue. Vrai par
  // défaut : c'est la seule liste qui décrive du travail.
  bool missing_only = true;
};

// Les lignes à montrer, dans l'ordre où elles se lisent.
//
// L'ORDRE EST CELUI DU TRAVAIL : ce qui manque le plus souvent d'abord, puis à
// égalité par opcode croissant — donc stable, ce qu'un tri par fréquence seule
// ne serait pas, et une liste qui se réordonne sous le curseur est illisible.
//
// Fonction pure, et séparée du dessin pour cette raison : c'est elle qui porte
// la seule décision de la fenêtre, et elle se vérifie sans ImGui.
[[nodiscard]] std::vector<PacketRow> SelectPacketRows(std::span<const PacketRow> rows,
                                                      const PacketWindowState& state);

// Dessine la fenêtre si elle est ouverte, et rend le nombre de lignes montrées.
//
// Le compte rendu n'est pas décoratif : sans lui, un test ne pourrait dire que
// « ça n'a pas planté ». Il vaut zéro quand la fenêtre est fermée.
std::size_t DrawPacketWindow(PacketWindowState& state, std::span<const PacketRow> rows,
                             std::uint32_t frames);

}  // namespace ro::ui
