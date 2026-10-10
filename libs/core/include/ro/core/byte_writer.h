#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ro::core {

// Écrivain d'octets, miroir du ByteReader.
//
// PETIT-BOUTISTE, pour la même raison que lui : les formats et les paquets RO
// le sont, et l'assemblage octet par octet donne le même résultat sur toutes
// les machines — la couche `proto` doit compiler à l'identique sur le runner
// Linux, qui nous donne les sanitizers.
//
// Pas de drapeau d'échec, contrairement au lecteur. Une écriture ne peut pas
// manquer d'octets : le tampon grandit. Ce qui peut manquer, c'est la PLACE
// dans un champ de largeur fixe, et FixedString tranche cette question à sa
// façon plutôt que de la remonter.
//
// L'écrivain ne POSSÈDE pas son tampon : il écrit dans celui qu'on lui donne.
// C'est ce qui permet de le poser sur un vecteur qu'on rendra ensuite, sans
// recopie.
class ByteWriter {
 public:
  explicit ByteWriter(std::vector<std::uint8_t>& into) noexcept : bytes_(into) {}

  void U8(std::uint8_t value) { bytes_.push_back(value); }
  void U16(std::uint16_t value) { Little(value, 2); }
  void U32(std::uint32_t value) { Little(value, 4); }
  void U64(std::uint64_t value) { Little(value, 8); }

  void I8(std::int8_t value) { U8(static_cast<std::uint8_t>(value)); }
  void I16(std::int16_t value) { U16(static_cast<std::uint16_t>(value)); }
  void I32(std::int32_t value) { U32(static_cast<std::uint32_t>(value)); }
  void I64(std::int64_t value) { U64(static_cast<std::uint64_t>(value)); }

  void F32(float value) { U32(std::bit_cast<std::uint32_t>(value)); }

  void Bytes(std::span<const std::uint8_t> raw) {
    bytes_.insert(bytes_.end(), raw.begin(), raw.end());
  }

  // Écrit le texte tel quel, sans complément ni terminaison.
  //
  // C'est la forme d'une queue de paquet à longueur variable : sa longueur est
  // celle de la trame, et rien n'a besoin de la marquer.
  void Text(std::string_view text) { bytes_.insert(bytes_.end(), text.begin(), text.end()); }

  // Champ de largeur fixe, complété de zéros.
  //
  // ⚠ Le champ est rempli JUSQU'AU BORD. Réserver le dernier octet pour un
  // terminateur paraissait prudent ; c'est faux, et cela casse.
  //
  // Faux, parce que le pair tronque déjà : le serveur lit un nom de vingt-quatre
  // octets avec une fonction qui en copie vingt-trois et termine elle-même. Un
  // nom de vingt-quatre caractères est donc inexprimable pour QUICONQUE, client
  // officiel compris, et se l'interdire ici n'ajoute aucune sûreté.
  //
  // Cassant, parce que tous les champs de largeur fixe ne sont pas des chaînes.
  // Le code PIN fait exactement quatre chiffres, que le serveur exige tous les
  // quatre : en réserver un pour un terminateur en envoyait trois, et la
  // connexion était fermée sans un mot.
  void FixedString(std::string_view text, std::size_t width) {
    const std::size_t kept = std::min(text.size(), width);
    bytes_.insert(bytes_.end(), text.begin(), text.begin() + static_cast<std::ptrdiff_t>(kept));
    bytes_.insert(bytes_.end(), width - kept, std::uint8_t{0});
  }

  // Remplace deux octets DÉJÀ écrits.
  //
  // Sert à la longueur d'une trame variable, qui n'est connue qu'une fois la
  // queue écrite : on la réserve, puis on la remplit. La compter d'avance
  // reviendrait à la compter deux fois, donc à risquer de la compter
  // différemment. Rend faux si le décalage sort du tampon.
  [[nodiscard]] bool PatchU16(std::size_t offset, std::uint16_t value) {
    if (offset + 2 > bytes_.size()) {
      return false;
    }
    bytes_[offset] = static_cast<std::uint8_t>(value & 0xFF);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    return true;
  }

  [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

 private:
  void Little(std::uint64_t value, std::size_t width) {
    for (std::size_t index = 0; index < width; ++index) {
      bytes_.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xFF));
    }
  }

  std::vector<std::uint8_t>& bytes_;
};

}  // namespace ro::core
