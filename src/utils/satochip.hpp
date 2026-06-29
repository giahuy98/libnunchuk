/*
 * This file is part of libnunchuk (https://github.com/nunchuk-io/libnunchuk).
 * Copyright (c) 2020 Enigmo.
 *
 * libnunchuk is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * libnunchuk is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libnunchuk. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NUNCHUK_SATOCHIP_H
#define NUNCHUK_SATOCHIP_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace nunchuk {

using CardBip32GetExtendedKeyFn =
    std::function<std::vector<std::vector<unsigned char>>(
        const std::string &path)>;

using CardSignTransactionHashFn = std::function<std::vector<unsigned char>(
    unsigned char keynbr, const std::vector<unsigned char> &txhash,
    const std::optional<std::vector<unsigned char>> &chalresponse)>;

using CardTaprootTweakPrivateKeyFn = std::function<std::vector<unsigned char>(
    int keynbr, const std::vector<unsigned char> &tweak, bool bypass_flag)>;

using CardSignSchnorrHashFn = std::function<std::vector<unsigned char>(
    const std::vector<unsigned char> &txhash,
    const std::optional<std::vector<unsigned char>> &chalresponse)>;

using CardMusig2GenerateNonceFn =
    std::function<std::vector<std::vector<unsigned char>>(
        int keynbr, const std::vector<unsigned char> &aggpk,
        const std::vector<unsigned char> &msg,
        const std::vector<unsigned char> &extra)>;

using CardMusig2SignFn = std::function<std::vector<unsigned char>(
    int keynbr, const std::vector<unsigned char> &secnonce,
    const std::vector<unsigned char> &b, const std::vector<unsigned char> &ea,
    bool r_has_even_y, bool ggacc_is_1)>;

// Save after MuSig2 nonce generation; consume should return and delete it.
using CardMusig2SaveSecNonceFn = std::function<void(
    const std::string &session_id, const std::vector<unsigned char> &secnonce)>;

using CardMusig2ConsumeSecNonceFn =
    std::function<std::optional<std::vector<unsigned char>>(
        const std::string &session_id)>;

std::string SatochipGetDescriptor(
    const CardBip32GetExtendedKeyFn &cardBip32GetExtendedKeyFn,
    std::string path, bool is_testnet = false);

struct SatochipSignPsbtParams {
  CardBip32GetExtendedKeyFn cardBip32GetExtendedKeyFn;
  CardSignTransactionHashFn cardSignTransactionHashFn;
  CardTaprootTweakPrivateKeyFn cardTaprootTweakPrivateKeyFn;
  CardSignSchnorrHashFn cardSignSchnorrHashFn;
  CardMusig2GenerateNonceFn cardMusig2GenerateNonceFn;
  CardMusig2SignFn cardMusig2SignFn;
  std::optional<std::vector<unsigned char>> chalresponse;
};

std::string SatochipGetMasterFingerprint(
    const CardBip32GetExtendedKeyFn &cardBip32GetExtendedKeyFn);

std::string SatochipSignPsbt(const SatochipSignPsbtParams &params,
                             const std::string &xfp, const std::string &psbt,
                             const CardMusig2SaveSecNonceFn &saveSecNonceFn,
                             const CardMusig2ConsumeSecNonceFn
                                 &consumeSecNonceFn);

}  // namespace nunchuk

#endif
