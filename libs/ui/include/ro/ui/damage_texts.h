#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include <imgui.h>

namespace ro::ui {

// LES NOMBRES DE DÉGÂTS ÉCRITS EN POLICE, quand le joueur les préfère au sprite
// des chiffres du client.
//
// 🔴 LA POLICE SE GRAVE À LA TAILLE DE CHAQUE INSTANT : un nombre qui naît cinq
// fois plus gros qu'il ne finit ne se lit net qu'à ce prix, et c'est ce que
// l'atlas d'ImGui sait faire — il grave à la demande et oublie ce qui ne sert
// plus. Un atlas gravé une fois et étiré rendrait un chiffre flou à sa
// naissance, justement quand on le regarde.
//
// ⚠ SUR LA LISTE DE FOND, et en premier : au-dessus du monde, SOUS les jauges et
// les bulles comme le sont les chiffres du sprite, et sous toutes les fenêtres.
//
// ⚠ `ui` NE SAIT RIEN DU MONDE : elle reçoit un point d'écran, une taille et des
// glyphes. C'est `app` qui pose le nombre et le projette.

// Assez pour « 999999 », « 1000K » ou « 4294M », et le zéro final.
inline constexpr std::size_t kDamageTextCapacity = 8;

// Le cerne des glyphes, noir comme le liseré des chiffres du client.
inline constexpr ImU32 kDamageTextOutlineColor = IM_COL32(0, 0, 0, 255);

// UN NOMBRE À ÉCRIRE, déjà à l'écran.
struct DamageTextView {
  // Le milieu du nombre : le centre de sa rangée de glyphes, à mi-hauteur d'un
  // chiffre.
  ImVec2 middle{0.0F, 0.0F};
  // La hauteur d'un chiffre, en pixels d'écran : la police se grave à la taille
  // qui la lui donne, quelle que soit la famille.
  float digit_height = 0.0F;
  // L'écart d'un centre de glyphe au suivant, en pixels d'écran.
  float pitch = 0.0F;
  // L'épaisseur du cerne, en pixels d'écran. Zéro : pas de cerne.
  float outline = 0.0F;
  // La couleur, transparence comprise : le cerne prend la même transparence.
  ImU32 color = IM_COL32_WHITE;
  // Les glyphes de gauche à droite, terminés par un zéro.
  std::array<char, kDamageTextCapacity> glyphs{};
};

// CE QUE L'ÉTAGE D'INTERFACE PEINT, ET AVEC QUELLE POLICE.
struct DamageTexts {
  std::span<const DamageTextView> texts;
  // LE NOM de la police : une famille de l'interface ou une police du dossier
  // du jeu — voir `UiContext::FontNamed`. Vide : celle de l'interface.
  std::string_view font_name;
  // Le gras, pour une famille qui en livre un. Une police du dossier du jeu n'a
  // qu'un dessin.
  bool bold = true;
};

// Peint tous les nombres, dans l'ordre donné — le dernier par-dessus. `font`
// nul, ou sans chiffres, reprend la police courante : une police fantaisie qui
// ne dessine que des lettres ne doit pas faire taire les dégâts. ⚠ À APPELER
// DANS L'IMAGE D'IMGUI, hors de toute fenêtre : elle écrit sur la liste de
// fond.
void DrawDamageTexts(std::span<const DamageTextView> texts, ImFont* font);

// Les mêmes, sur une autre liste de dessin : celle d'un cadre d'essai.
void PaintDamageTexts(ImDrawList& into, std::span<const DamageTextView> texts, ImFont* font);

}  // namespace ro::ui
