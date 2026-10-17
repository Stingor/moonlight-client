#include "ro/ui/bank_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include "ro/ui/inventory_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

using render::SkinPiece;

// ── LA GÉOMÉTRIE DU FOND, MESURÉE SUR LE BITMAP ─────────────────────────────
//
// `bg_bank_moon.bmp` fait 345 sur 187, et il porte deux repères peints :
//   · le sac de zeny, de x 7 à x 35 et de y 93 à y 123 ;
//   · la bande grise du montant, de y 86 à y 133, bordée de gris clair.
//
// 🔴 LE SAC TIENT LIEU DE LIBELLÉ. Rien n'écrit « Montant » quand le fond est
// là : le champ démarre juste après le sac, et la bande accueille le message
// d'état à sa droite. Écrire le mot par-dessus l'image le doublerait.
constexpr float kBagRightPixels = 35.0F;
constexpr float kAmountInputPixels = 120.0F;

// ── LES TROIS TRANCHES DU FOND, relevées ligne par ligne sur le bitmap ──────
//
//   y   0..85  rayures claires — 247 et 255 en alternance, une ligne sur deux
//   y  86      la bordure grise 198 qui ouvre la bande
//   y  87..132 la bande gris-bleu 229,231,234, où le SAC est peint (y 93..123)
//   y 133      la bordure grise qui la referme
//   y 134..186 rayures à nouveau
//
// La bande occupe donc `[86, 134[`, soit QUARANTE-HUIT lignes.
constexpr float kBandTopPixels = 86.0F;
constexpr float kBandBottomPixels = 134.0F;
static_assert(kBandBottomPixels - kBandTopPixels == kBankBandPixels,
              "la bande relevée sur le bitmap et celle que l'en-tête publie doivent tenir "
              "la même mesure");

// La largeur de repli, SANS le bitmap. ⚠ ELLE DÉRIVE DE LA POLICE et non d'un
// nombre rond : le facteur est le rapport du bitmap à son corps de police —
// 345 / 15 —, et c'est ce qui garde la fenêtre habitable quand le texte grossit.
constexpr float kFallbackWidthPixels = 300.0F;
constexpr float kFallbackWidthInFonts = 23.0F;

// Ce que vaut un pour-cent, pour la jauge de remplissage.
constexpr float kPercent = 100.0F;

// LES DEUX TEINTES DU MESSAGE D'ÉTAT. ⚠ ELLES NE SONT PAS DANS LA PALETTE : la
// palette dit ce qu'est une fenêtre du jeu, et ces deux-là ne servent qu'ici —
// le client d'origine n'a pas de message vert du tout.
constexpr ImVec4 kStatusErrorColor(0.85F, 0.15F, 0.15F, 1.0F);
constexpr ImVec4 kStatusDoneColor(0.15F, 0.55F, 0.20F, 1.0F);

// LES PALIERS, dans l'ordre de la rangée. Voir `kBankStepCount`.
struct BankStep {
  const char* label;
  std::int64_t value;
};
constexpr std::array<BankStep, kBankStepCount> kSteps = {
    BankStep{"+10k", 10000},    BankStep{"+100k", 100000},    BankStep{"+1M", 1000000},
    BankStep{"+10M", 10000000}, BankStep{"+100M", 100000000},
};

// Un montant groupé par milliers, suivi de son « z ».
//
// ⚠ AVEC UNE ESPACE, à rebours du pied du sac qui colle son « z » au nombre.
// Chacun garde l'écriture propre à sa fenêtre : les unifier changerait l'une
// des deux à l'écran.
[[nodiscard]] std::string Zeny(std::int64_t amount) {
  return GroupThousands(amount) + " " + std::string(kZenySuffix);
}

// LE FOND, EN TROIS TRANCHES, LA BANDE POSÉE SUR LA LIGNE DU MONTANT.
//
// 🔴 UN BLIT D'UN SEUL TENANT NE TOMBE JUSTE QUE POUR UNE POLICE. Le bitmap a
// été peint pour une hauteur de ligne donnée : dès que la police, l'échelle ou
// la langue changent d'un pixel, la bande grise ne recouvre plus le champ
// qu'elle encadre — elle descend sous les boutons, ou remonte au-dessus du sac.
// C'est le même raisonnement que pour une barre de titre, qu'on ne blitte pas
// non plus : on la DÉCOUPE.
//
// ⚠ LA BANDE NE S'ÉTIRE QU'EN DERNIER RECOURS, parce que le SAC DE ZENY y est
// peint : l'appelant lui réserve sa hauteur d'origine et centre le bloc dedans,
// si bien qu'elle n'est étirée que lorsque le bloc, lui, ne tient plus dans
// quarante-huit lignes. Les deux tranches rayées, elles, s'étirent sans qu'on
// le voie : huit valeurs sur deux cent cinquante-cinq séparent leurs rayures.
void DrawBankBackground(const SkinImage& background, ImDrawList* into, float left, float right,
                        float body_top, float body_bottom, float band_top, float band_bottom) {
  if (!background.usable() || background.height <= 0.0F) {
    return;
  }
  const float texture = background.height;
  // ⚠ CHAQUE TRANCHE NE SE DESSINE QUE SI ELLE A UNE HAUTEUR : un rectangle
  // retourné ne disparaît pas, il se peint à l'envers.
  const auto slice = [&](float from, float to, float top, float bottom) {
    if (bottom <= top) {
      return;
    }
    into->AddImage(background.id, ImVec2(left, top), ImVec2(right, bottom),
                   ImVec2(0.0F, from / texture), ImVec2(1.0F, to / texture));
  };
  slice(0.0F, kBandTopPixels, body_top, band_top);
  slice(kBandTopPixels, kBandBottomPixels, band_top, band_bottom);
  slice(kBandBottomPixels, texture, band_bottom, body_bottom);
}

// LA RANGÉE DES PALIERS, alignée sur la colonne du champ.
//
// ⚠ L'ESPACEMENT SE RESSERRE PLUTÔT QUE LES BOUTONS NE DÉBORDENT : la rangée a
// perdu la largeur du sac, et le plus large des cinq libellés doit tenir dans sa
// case. On garde l'écart habituel tant qu'il rentre, et on le rogne seulement
// s'il le faut — la rangée reste donc correcte quand la police grossit, alors
// que le fond, lui, impose sa largeur.
//
// ⚠ EN PIXELS ENTIERS : une largeur ou un écart fractionnaire décale les
// boutons d'un demi-pixel, et le découpage en trois tranches bave au raccord.
void DrawStepRow(BankWindowState& state, const SkinArt& art, bool has_background,
                 float amount_column) {
  if (has_background) {
    ImGui::SetCursorPosX(amount_column);
  }
  float widest = 0.0F;
  for (const BankStep& step : kSteps) {
    widest = std::max(widest, ImGui::CalcTextSize(step.label).x);
  }
  // `GetFontSize()` approche ce que l'art prend à gauche et à droite du libellé :
  // c'est la part de la case que le texte n'a pas.
  const float needed = (widest + ImGui::GetFontSize()) * kBankStepCount;
  const float available = ImGui::GetContentRegionAvail().x;
  float gap = ImGui::GetStyle().ItemSpacing.x;
  if (needed + gap * (kBankStepCount - 1) > available) {
    gap = std::max(0.0F, (available - needed) / (kBankStepCount - 1));
  }
  gap = std::floor(gap);
  const float step_width = std::floor((available - gap * (kBankStepCount - 1)) / kBankStepCount);
  for (int rank = 0; rank < kBankStepCount; ++rank) {
    if (rank > 0) {
      ImGui::SameLine(0.0F, gap);
    }
    if (SkinSmallButton(kSteps[static_cast<std::size_t>(rank)].label, art,
                        ImVec2(step_width, 0.0F))) {
      state.amount = BankClampAmount(state.amount + kSteps[static_cast<std::size_t>(rank)].value);
    }
  }
}

}  // namespace

void SetBankStatus(BankWindowState& state, const char* utf8, bool is_error) {
  std::snprintf(state.status, sizeof(state.status), "%s", utf8 == nullptr ? "" : utf8);
  state.status_error = is_error;
}

std::int64_t BankMaxDeposit(std::int64_t vault, std::int64_t zeny) {
  const std::int64_t room = kBankZenyMax - vault;
  return std::max<std::int64_t>(0, std::min(zeny, room));
}

std::int64_t BankMaxWithdraw(std::int64_t vault, std::int64_t zeny) {
  const std::int64_t room = kBankZenyMax - zeny;
  return std::max<std::int64_t>(0, std::min(vault, room));
}

std::int64_t BankClampAmount(std::int64_t amount) {
  return std::max<std::int64_t>(0, std::min(amount, kBankZenyMax));
}

BankWindowAction DrawBankWindow(BankWindowState& state, const BankWindowView& view,
                                const SkinArt& art) {
  BankWindowAction action;
  if (!state.open) {
    state.was_open = false;
    return action;
  }

  // LE FRONT MONTANT : la session vient de s'ouvrir, le champ et le message
  // repartent à zéro. ⚠ SINON LE MONTANT DE LA DERNIÈRE VISITE RESTERAIT LÀ,
  // armé, sur une fenêtre qui déplace de l'argent.
  if (!state.was_open) {
    state.was_open = true;
    state.amount = 0;
    SetBankStatus(state, "", false);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  }

  // 🔴 LA LARGEUR EST CELLE DU FOND, ET IMPOSÉE À CHAQUE IMAGE. Le bitmap est
  // peint pour elle, et le sac de zeny y est à une abscisse fixe : c'est ce qui
  // fait démarrer le champ juste après lui. La HAUTEUR, elle, s'ajuste au
  // contenu — un `y` nul demande à ImGui de la calculer —, et c'est le
  // découpage du fond en trois tranches qui absorbe la différence.
  const SkinImage& background = art.Get(SkinPiece::kBankBackground);
  const float scale = Scale();
  const float width = background.usable() ? background.width * scale
                                          : std::max(kFallbackWidthPixels * scale,
                                                     ImGui::GetFontSize() * kFallbackWidthInFonts);
  ImGui::SetNextWindowSize(ImVec2(width, 0.0F), ImGuiCond_Always);

  static WindowName name(kBankWindowTitle, kBankWindowId);
  bool open = true;
  const bool begun = BeginSkinnedWindow(name.Get(), &open, art, SkinnedTitle::kBar,
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  if (!open) {
    action.verb = BankVerb::kClose;
  }
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }

  // 🔴 DEUX CANAUX, ET C'EST LE SEUL MOYEN. Le fond doit être DERRIÈRE le
  // contenu, et l'on ne sait où poser sa bande qu'une fois les soldes écrits :
  // c'est leur hauteur qui dit où commence la ligne du montant. Le contenu part
  // donc sur le canal 1, le fond sur le 0, et la fusion les remet dans l'ordre.
  // ⚠ AUCUN RETOUR ANTICIPÉ ENTRE LE SPLIT ET LA FUSION : le dessin de la
  // fenêtre resterait coupé en deux.
  ImDrawList* const into = ImGui::GetWindowDrawList();
  into->ChannelsSplit(2);
  into->ChannelsSetCurrent(1);

  const ImVec2 corner = ImGui::GetWindowPos();
  const float body_left = corner.x;
  const float body_right = corner.x + ImGui::GetWindowWidth();
  const float body_top = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().WindowPadding.y;

  std::array<char, 64> text = {};

  // ── LES TROIS SOLDES ───────────────────────────────────────────────────────
  //
  // ⚠ ALIGNÉS SUR UNE COLONNE, ce que le client d'origine ne fait pas — il colle
  // le nombre juste après son libellé. La colonne se cale sur le PLUS LONG des
  // libellés, pour qu'aucun ne morde sur les chiffres.
  const float value_column =
      ImGui::CalcTextSize(core::Tr("Montant")).x + ImGui::GetFontSize() * 1.5F;

  ImGui::TextUnformatted(core::Tr("Banque"));
  ImGui::SameLine(value_column);
  ImGui::TextUnformatted(Zeny(view.vault).c_str());

  ImGui::TextUnformatted(core::Tr("Poche"));
  ImGui::SameLine(value_column);
  // ⚠ RIEN TANT QUE LA BOURSE N'EST PAS CONNUE : un zéro se lirait « vous êtes
  // ruiné » au lieu de « je ne sais pas encore ».
  ImGui::TextUnformatted(view.zeny_known ? Zeny(view.zeny).c_str() : "-");

  // 🔴 LE TOTAL ET LA JAUGE SONT INCONDITIONNELS, et ce n'est pas un oubli de
  // réglage : le fond est un bitmap à hauteur FIXE, et un bloc qu'on masquerait
  // ferait glisser tout ce qui suit hors du fond peint.
  ImGui::TextUnformatted(core::Tr("Total"));
  ImGui::SameLine(value_column);
  ImGui::TextUnformatted(Zeny(view.vault + (view.zeny_known ? view.zeny : 0)).c_str());
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("La banque est plafonnée à 2 147 483 647 z (INT32), côté client comme côté "
               "serveur (MAX_BANK_ZENY). Un dépôt qui ferait dépasser ce plafond est refusé "
               "en bloc, pas tronqué."));

  // La jauge de remplissage vis-à-vis du plafond : le client d'origine ne dit
  // rien tant qu'on ne bute pas dedans, et l'erreur tombe alors sans prévenir.
  const float filled =
      static_cast<float>(static_cast<double>(view.vault) / static_cast<double>(kBankZenyMax));
  std::snprintf(text.data(), text.size(), core::Tr("%.1f %% du plafond"),
                static_cast<double>(filled * kPercent));
  // Une hauteur nulle prend celle d'un cadre standard : c'est la seule qui
  // laisse ImGui centrer le texte du calque. Plus fine, la barre le rogne.
  ImGui::ProgressBar(filled, ImVec2(-1.0F, 0.0F), text.data());

  // 🔴 LA BANDE S'OUVRE SUR LE SÉPARATEUR, et c'est le bitmap qui le dit : sa
  // bordure haute — une ligne de gris 198 — tombe exactement là où le trait de
  // séparation se dessine. Les deux se confondent, et c'est pour cela que le
  // repère se prend AVANT lui.
  const float band_top = ImGui::GetCursorScreenPos().y;
  ImGui::Separator();

  // ── LE MONTANT, DANS LA BANDE GRISE DU FOND ────────────────────────────────
  //
  // ⚠ UNE SEULE COLONNE POUR LE CHAMP ET POUR LA RANGÉE DES PALIERS : le sac
  // occupe le début de la bande, et tout ce qui porte un montant démarre après
  // lui. Deux variables dériveraient l'une de l'autre.
  const bool has_background = background.usable();
  const float amount_column = kBagRightPixels * scale + ImGui::GetStyle().ItemSpacing.x;

  // ⚠ AUCUN CALAGE VERTICAL ICI, et c'est délibéré : le champ tombe où le flux
  // le pose. Le centrer dans la bande le descendrait de quelques pixels, et le
  // sac de zeny — peint sept lignes sous la bordure — ne lui ferait plus face.
  if (has_background) {
    ImGui::SetCursorPosX(amount_column);
    ImGui::SetNextItemWidth(kAmountInputPixels * scale);
  } else {
    ImGui::TextUnformatted(core::Tr("Montant"));
    ImGui::SameLine(value_column);
    ImGui::SetNextItemWidth(-1.0F);
  }
  if (ImGui::InputScalar("##montant", ImGuiDataType_S64, &state.amount, nullptr, nullptr, "%lld",
                         ImGuiInputTextFlags_CharsDecimal)) {
    state.amount = BankClampAmount(state.amount);
  }

  // LE MESSAGE D'ÉTAT, À DROITE DU CHAMP, dans la partie libre de la bande.
  // C'est là que le client d'origine l'affiche aussi, et surtout cela garde la
  // hauteur de la fenêtre constante — donc le fond aligné. Pas de retour à la
  // ligne : un texte trop long est rogné au bord.
  if (state.status[0] != '\0') {
    ImGui::SameLine();
    ImGui::TextColored(state.status_error ? kStatusErrorColor : kStatusDoneColor, "%s",
                       state.status);
  }

  DrawStepRow(state, art, has_background, amount_column);
  // LE BAS DE LA BANDE : sa hauteur d'origine, ou le bloc lui-même s'il la
  // dépasse — à grande police, le champ et les boutons passent les
  // quarante-huit lignes, et une bande qui garderait sa taille les laisserait
  // hors de leur cadre. Le sac qu'elle porte s'étire alors un peu, ce qui se
  // voit moins.
  //
  // ⚠ LE FLUX N'EST PAS REPOUSSÉ AU-DELÀ : « Tout déposer » commence quelques
  // pixels AVANT que la bande ne se referme, et c'est ce chevauchement qui
  // fait paraître la bande arrêtée sur le bouton.
  const float band_bottom =
      std::max(band_top + kBankBandPixels * scale, ImGui::GetCursorScreenPos().y);

  // ── LES DEUX COLONNES DU BAS ───────────────────────────────────────────────
  //
  // ⚠ LA LARGEUR DE COLONNE EST PLANCHÉRISÉE : une largeur disponible impaire
  // tombe sur un demi-pixel, et le découpage en trois tranches d'un bouton s'y
  // échantillonne à cheval sur deux texels — bords baveux, couture visible.
  //
  // ⚠ ET LA COLONNE DE DROITE SE POSE EN COORDONNÉES D'ÉCRAN, pas par un
  // `SameLine` : le petit bouton se resserre quand il suit un `SameLine`, le
  // grand non, et les deux rangées se décaleraient l'une par rapport à l'autre.
  const std::int64_t zeny = view.zeny_known ? view.zeny : 0;
  const std::int64_t most_deposited = view.zeny_known ? BankMaxDeposit(view.vault, zeny) : 0;
  const std::int64_t most_withdrawn = view.zeny_known ? BankMaxWithdraw(view.vault, zeny) : 0;

  const float column_gap = ImGui::GetStyle().ItemSpacing.x;
  const float column_width = std::floor((ImGui::GetContentRegionAvail().x - column_gap) * 0.5F);

  ImVec2 row = ImGui::GetCursorScreenPos();
  ImGui::BeginDisabled(most_deposited <= 0);
  if (SkinSmallButton(core::Tr("Tout déposer"), art, ImVec2(column_width, 0.0F))) {
    state.amount = most_deposited;
  }
  ImGui::EndDisabled();
  if (most_deposited > 0 && ImGui::IsItemHovered()) {
    SkinTooltip(Zeny(most_deposited).c_str());
  }
  ImGui::SetCursorScreenPos(ImVec2(row.x + column_width + column_gap, row.y));
  ImGui::BeginDisabled(most_withdrawn <= 0);
  if (SkinSmallButton(core::Tr("Tout retirer"), art, ImVec2(column_width, 0.0F))) {
    state.amount = most_withdrawn;
  }
  ImGui::EndDisabled();
  if (most_withdrawn > 0 && ImGui::IsItemHovered()) {
    SkinTooltip(Zeny(most_withdrawn).c_str());
  }

  ImGui::Separator();

  // ── LES DEUX ACTIONS ───────────────────────────────────────────────────────
  //
  // ⚠ ON GRISE CE QUI NE PEUT PAS MARCHER, là où le client d'origine laisse
  // cliquer et dit « non » après coup. La raison, elle, s'affiche dans la bande.
  const bool can_deposit = state.amount > 0 && state.amount <= most_deposited;
  const bool can_withdraw = state.amount > 0 && state.amount <= most_withdrawn;

  row = ImGui::GetCursorScreenPos();
  ImGui::BeginDisabled(!can_deposit);
  if (SkinButton(core::Tr("Déposer"), art, ImVec2(column_width, 0.0F))) {
    action.verb = BankVerb::kDeposit;
    action.amount = state.amount;
  }
  ImGui::EndDisabled();
  ImGui::SetCursorScreenPos(ImVec2(row.x + column_width + column_gap, row.y));
  ImGui::BeginDisabled(!can_withdraw);
  if (SkinButton(core::Tr("Retirer"), art, ImVec2(column_width, 0.0F))) {
    action.verb = BankVerb::kWithdraw;
    action.amount = state.amount;
  }
  ImGui::EndDisabled();

  // 🔴 LE CHAMP SE VIDE À L'ENVOI, comme dans le client d'origine — et c'est
  // aussi ce qui tient lieu de garde au double-clic : les deux boutons se
  // grisent aussitôt, et le second clic ne peut plus rien envoyer. Le solde,
  // lui, ne bouge qu'à la réponse du serveur.
  if (action.verb == BankVerb::kDeposit || action.verb == BankVerb::kWithdraw) {
    state.amount = 0;
    SetBankStatus(state,
                  action.verb == BankVerb::kDeposit ? core::Tr("Dépôt envoyé...")
                                                    : core::Tr("Retrait envoyé..."),
                  false);
  }

  // ── LE FOND, MAINTENANT QU'ON SAIT OÙ TOUT EST TOMBÉ ──────────────────────
  //
  // ⚠ LE BAS DU CORPS SE LIT AU CURSEUR, pas à la taille de la fenêtre : celle
  // d'une fenêtre qui s'auto-ajuste est encore celle de l'image PRÉCÉDENTE, et
  // la première image la donnerait fausse.
  const float body_bottom = ImGui::GetCursorScreenPos().y + ImGui::GetStyle().WindowPadding.y;
  into->ChannelsSetCurrent(0);
  DrawBankBackground(background, into, body_left, body_right, body_top, body_bottom, band_top,
                     band_bottom);
  into->ChannelsMerge();

  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
