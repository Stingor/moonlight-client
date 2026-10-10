#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ro::render {

// L'effet qu'un acteur porte TANT QU'UN ÉTAT DURE.
//
// Ceux-là n'arrivent par aucun paquet de compétence : le client les RESÈME de
// lui-même, à partir du seul bit d'option de l'acteur. Sight et Ruwach en sont
// le modèle : un compteur par acteur avance à chaque tic ; quand il atteint la
// durée que la table du client déclare pour l'effet (la table de l'ADR 0042),
// l'effet ENTIER naît sur l'acteur et le compteur repart de zéro.
//
// 🔴 C'EST L'EFFET ENTIER QUI SE REJOUE, PAS UN SPRITE : l'ombre, la ronde et
// le pas d'échelle compris. MG_SIGHT et `@effect 22` passent par le même
// dessin. Le natif rejoue le vrai effet 22, attaché à l'acteur, tous les deux
// cent soixante-dix tics.
//
// ⚠ ET IL NE S'ÉTEINT PAS QUAND LE BIT TOMBE : la dernière instance semée vit
// sa vie, et c'est le RESEMIS qui cesse. Voir `OptionEffectDue` pour le
// compteur.
struct OptionEffect {
  // Le bit de `e_option` qui l'allume, avec les valeurs de src/map/status.hpp.
  std::uint32_t option_mask = 0;
  std::uint32_t effect_id = 0;
  // Tous les combien de tics l'effet renaît : la durée déclarée de l'effet.
  std::int32_t respawn_ticks = 0;
  // 🔴 LE BRUIT DE L'ALLUMAGE, ET DE LUI SEUL. Il ne vient pas du resemis mais
  // du FRONT MONTANT du bit, à la réception du changement d'option : l'allumage
  // de Sight joue le fichier ci-dessous en 3D, et Ruwach a sa branche jumelle
  // avec LE MÊME fichier. Aucune table de sons d'effet ni de compétence ne le
  // porte : sans ce champ, MG_SIGHT serait muet.
  std::string_view rise_sound;
};

[[nodiscard]] std::span<const OptionEffect> OptionEffectTable();

// CE QUE L'ENTRÉE ET LA SORTIE DE HIDING MONTRENT, au changement d'état reçu
// en direct — jamais à l'apparition d'un acteur déjà caché.
//
// 🔴 LA FUMÉE DES DEUX CÔTÉS, L'EFFET À L'ENTRÉE SEULE. Se cacher comme
// reparaître fait une bouffée de fumée aux pieds (`kSmokePuffSprite`) ; seul le
// fait de se cacher pose en plus l'effet 16, et avec lui son bruit. Le paquet
// de la compétence, lui, ne montre rien : c'est le changement d'option qui
// parle, et Hiding se relance pour sortir.
//
// ⚠ LE NINJA ET LE BÉBÉ NINJA N'ONT QUE LA FUMÉE : leur métier les dispense de
// l'effet d'entrée.
inline constexpr std::uint32_t kHideEntryEffect = 16;
inline constexpr std::uint16_t kJobNinja = 25;
inline constexpr std::uint16_t kJobBabyNinja = 4222;

struct HideTransitionLook {
  bool smoke_puff = false;
  // Zéro quand rien ne naît.
  std::uint32_t effect_id = 0;
};

[[nodiscard]] constexpr HideTransitionLook HideTransitionLookFor(bool now_hidden,
                                                                 std::uint16_t job) {
  HideTransitionLook look;
  look.smoke_puff = true;
  if (now_hidden && job != kJobNinja && job != kJobBabyNinja) {
    look.effect_id = kHideEntryEffect;
  }
  return look;
}

// L'EFFET D'ÉTAT QUI RENAÎT À CET INSTANT sur un acteur, ou nul.
//
// 🔴 UN SEUL COMPTEUR PAR ACTEUR, que tous les effets de la table se
// partagent : ils se testent DANS L'ORDRE de la table, et le premier dont la
// durée est atteinte naît et remet le compteur à zéro — ce qui fait échouer le
// test des suivants à ce tic-là. Un acteur sous Sight et Ruwach à la fois ne
// resème donc que celui des deux qui arrive le premier à sa borne.
//
// ⚠ UN ACTEUR SANS AUCUNE OPTION porte son compteur AU PLUS HAUT, et un état
// qui s'allume ensuite naît aussitôt, même s'il vient de tomber : c'est à
// l'appelant de l'oublier quand le mot d'options est nul.
//
// `since_sown_ms` : le temps écoulé depuis le dernier semis, ou rien si le
// compteur est au plus haut.
[[nodiscard]] const OptionEffect* OptionEffectDue(std::uint32_t option,
                                                  std::optional<std::uint32_t> since_sown_ms);

}  // namespace ro::render
