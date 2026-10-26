#pragma once

// LES TEXTES DU CODEX QUE LES RÈGLES NE PORTENT PAS, désignés par les
// énumérations de `ui` que la vue passe : libellés des chiffres, unités,
// comportements et traits, coups de boss, sortes d'événements, sources
// d'arrivée. Les éléments et les tailles sont ceux de la base de données.
//
// ⚠ UN FORMAT LITTÉRAL PAR SORTE, et non une table de formats : un format lu
// dans une table n'est plus vérifié par le compilateur contre ses arguments.
//
// En-tête PRIVÉ de `libs/ui/src`.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/ui/survivors_codex_window.h"

namespace ro::ui {

inline constexpr std::size_t kCodexTextSize = 192;
using CodexText = std::array<char, kCodexTextSize>;

// Le nom d'un onglet, traduit.
[[nodiscard]] const char* CodexTabLabel(SurvivorsCodexTab tab);

// Le libellé d'une ligne de chiffres, traduit.
[[nodiscard]] const char* CodexFigureLabelText(SurvivorsCodexFigureLabel label);

// UN NOMBRE DIT DANS SON UNITÉ : « 1,5 s », « 4,5 cases », « 175 % ». Les
// décimales nulles se taisent, et le séparateur décimal est celui de la langue.
[[nodiscard]] CodexText CodexValueText(SurvivorsCodexUnit unit, std::int64_t value);

// Le nom d'un élément ou d'une taille, traduit ; "" hors des tables.
[[nodiscard]] const char* CodexElementName(std::uint8_t element);
[[nodiscard]] const char* CodexSizeName(std::uint8_t size);

// La façon dont un monstre approche, puis ses traits et sa meute.
[[nodiscard]] CodexText CodexBehaviourText(const SurvivorsCodexBehaviour& behaviour);

// D'où vient un monstre, et à quelle heure.
[[nodiscard]] CodexText CodexSourceText(const SurvivorsCodexSource& source);

// Des affinités, « Vent 175 %, Feu 50 % », dans l'ordre de la vue.
[[nodiscard]] CodexText CodexAffinitiesText(std::span<const SurvivorsCodexAffinity> affinities);

// Un coup de boss : ce qu'il fait, et la plage de PV où il joue quand elle
// n'est pas la vie entière.
[[nodiscard]] CodexText CodexMoveText(const SurvivorsCodexMove& move);

// La sorte d'un événement de la chronologie, traduite.
[[nodiscard]] const char* CodexEventKindName(SurvivorsCodexEventKind kind);

// Ce qu'un événement amène et quand : l'espèce et son compte, ou les heures
// des élites et de la nuit.
[[nodiscard]] CodexText CodexEventText(const SurvivorsCodexEntry& entry);

}  // namespace ro::ui
