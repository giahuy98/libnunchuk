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

#include <backend/electrum/synchronizer.h>
#include <backend/corerpc/synchronizer.h>
#include <coreutils.h>
#include <tinyformat.h>
#include <utils/connectionlog.hpp>

using namespace boost::asio;

namespace nunchuk {

namespace {

std::atomic<uint64_t> g_synchronizer_instance_counter{0};

const char* TransactionStatusName(TransactionStatus status) {
  switch (status) {
    case TransactionStatus::PENDING_SIGNATURES:
      return "PENDING_SIGNATURES";
    case TransactionStatus::READY_TO_BROADCAST:
      return "READY_TO_BROADCAST";
    case TransactionStatus::NETWORK_REJECTED:
      return "NETWORK_REJECTED";
    case TransactionStatus::PENDING_CONFIRMATION:
      return "PENDING_CONFIRMATION";
    case TransactionStatus::REPLACED:
      return "REPLACED";
    case TransactionStatus::CONFIRMED:
      return "CONFIRMED";
    case TransactionStatus::PENDING_NONCE:
      return "PENDING_NONCE";
    case TransactionStatus::DELETED:
      return "DELETED";
  }
  return "UNKNOWN";
}

const char* BackendTypeName(BackendType backend_type) {
  switch (backend_type) {
    case BackendType::ELECTRUM:
      return "ELECTRUM";
    case BackendType::CORERPC:
      return "CORERPC";
  }
  return "UNKNOWN";
}

}  // namespace

std::unique_ptr<Synchronizer> MakeSynchronizer(const AppSettings& appsettings,
                                               const std::string& account) {
  if (appsettings.get_backend_type() == BackendType::CORERPC) {
    return std::unique_ptr<CoreRpcSynchronizer>(
        new CoreRpcSynchronizer(appsettings, account));
  } else {
    return std::unique_ptr<ElectrumSynchronizer>(
        new ElectrumSynchronizer(appsettings, account));
  }
}

Synchronizer::Synchronizer(const AppSettings& appsettings,
                           const std::string& account)
    : app_settings_(appsettings),
      storage_(NunchukStorage::get(account)),
      sync_thread_(),
      sync_worker_(make_work_guard(io_service_)),
      instance_id_(++g_synchronizer_instance_counter) {
  ConnectionDebugLog(
      "sync",
      strprintf("created sync=%llu ptr=%p backend=%s chain=%d account=%s",
                static_cast<unsigned long long>(instance_id_),
                static_cast<const void*>(this),
                BackendTypeName(app_settings_.get_backend_type()),
                static_cast<int>(app_settings_.get_chain()), account.c_str()));
  sync_thread_ = std::thread([&]() {
    for (;;) {
      try {
        io_service_.run();
        break;  // exited normally
      } catch (...) {
      }
    }
  });
}

Synchronizer::~Synchronizer() {
  ConnectionDebugLog("sync",
                     strprintf("destroyed sync=%llu ptr=%p",
                               static_cast<unsigned long long>(instance_id_),
                               static_cast<const void*>(this)));
}

bool Synchronizer::NeedRecreate(const AppSettings& new_settings) {
  if (app_settings_.get_backend_type() != new_settings.get_backend_type() ||
      app_settings_.get_chain() != new_settings.get_chain())
    throw NunchukException(NunchukException::APP_RESTART_REQUIRED,
                           "App restart required");

  if (app_settings_.use_proxy() != new_settings.use_proxy()) return true;
  if (new_settings.use_proxy() &&
      (app_settings_.get_proxy_host() != new_settings.get_proxy_host() ||
       app_settings_.get_proxy_port() != new_settings.get_proxy_port() ||
       app_settings_.get_proxy_username() !=
           new_settings.get_proxy_username() ||
       app_settings_.get_proxy_password() != new_settings.get_proxy_password()))
    return true;

  if (new_settings.get_backend_type() == BackendType::CORERPC) {
    if (app_settings_.get_corerpc_host() != new_settings.get_corerpc_host() ||
        app_settings_.get_corerpc_port() != new_settings.get_corerpc_port() ||
        app_settings_.get_corerpc_username() !=
            new_settings.get_corerpc_username() ||
        app_settings_.get_corerpc_password() !=
            new_settings.get_corerpc_password())
      return true;
  } else {
    if ((new_settings.get_chain() == Chain::TESTNET &&
         app_settings_.get_testnet_servers() !=
             new_settings.get_testnet_servers()) ||
        (new_settings.get_chain() == Chain::MAIN &&
         app_settings_.get_mainnet_servers() !=
             new_settings.get_mainnet_servers()) ||
        (new_settings.get_chain() == Chain::SIGNET &&
         app_settings_.get_signet_servers() !=
             new_settings.get_signet_servers()))
      return true;
  }
  return false;
}

void Synchronizer::AddBalanceListener(
    std::function<void(std::string, Amount)> listener) {
  balance_listener_.connect(listener);
}

void Synchronizer::AddBalancesListener(
    std::function<void(std::string, Amount, Amount)> listener) {
  balances_listener_.connect(listener);
}

void Synchronizer::AddBlockListener(
    std::function<void(int, std::string)> listener) {
  block_listener_.connect(listener);
}

void Synchronizer::AddTransactionListener(
    std::function<void(std::string, TransactionStatus, std::string)> listener) {
  transaction_listener_.connect(listener);
}

void Synchronizer::AddBlockchainConnectionListener(
    std::function<void(ConnectionStatus, int)> listener) {
  connection_listener_.connect(listener);
}

void Synchronizer::NotifyTransactionUpdate(const std::string& wallet_id,
                                           const std::string& tx_id,
                                           TransactionStatus status) {
  EmitTransactionListener(wallet_id, tx_id, status);
}

void Synchronizer::EmitBalanceListener(const std::string& wallet_id,
                                       Amount balance) {
  ConnectionDebugLog("listener",
                     strprintf("sync=%llu dispatch balance wallet=%s balance=%lld",
                               static_cast<unsigned long long>(instance_id_),
                               wallet_id.c_str(),
                               static_cast<long long>(balance)));
  balance_listener_(wallet_id, balance);
}

void Synchronizer::EmitBalancesListener(const std::string& wallet_id,
                                        Amount balance,
                                        Amount unconfirmed_balance) {
  ConnectionDebugLog(
      "listener",
      strprintf(
          "sync=%llu dispatch balances wallet=%s balance=%lld unconfirmed_balance=%lld",
          static_cast<unsigned long long>(instance_id_),
          wallet_id.c_str(), static_cast<long long>(balance),
          static_cast<long long>(unconfirmed_balance)));
  balances_listener_(wallet_id, balance, unconfirmed_balance);
}

void Synchronizer::EmitBlockListener(int height, const std::string& block_hash) {
  ConnectionDebugLog(
      "listener",
      strprintf("sync=%llu dispatch block height=%d hash=%s",
                static_cast<unsigned long long>(instance_id_), height,
                block_hash.c_str()));
  block_listener_(height, block_hash);
}

void Synchronizer::EmitTransactionListener(const std::string& wallet_id,
                                           const std::string& tx_id,
                                           TransactionStatus status) {
  ConnectionDebugLog(
      "listener",
      strprintf("sync=%llu dispatch transaction wallet=%s txid=%s status=%s",
                static_cast<unsigned long long>(instance_id_),
                wallet_id.c_str(), tx_id.c_str(), TransactionStatusName(status)));
  transaction_listener_(tx_id, status, wallet_id);
}

void Synchronizer::EmitConnectionListener(ConnectionStatus status,
                                          int progress) {
  ConnectionDebugLog(
      "listener",
      strprintf("sync=%llu dispatch blockchain connection status=%s progress=%d",
                static_cast<unsigned long long>(instance_id_),
                ConnectionStatusName(status), progress));
  connection_listener_(status, progress);
}

int Synchronizer::GetChainTip() {
  int rs = chain_tip_;
  if (rs <= 0) rs = storage_->GetChainTip(app_settings_.get_chain());
  return rs;
}

std::string Synchronizer::NewAddress(Chain chain, const std::string& wallet_id,
                                     bool internal) {
  auto wallet = storage_->GetWallet(chain, wallet_id, false, false);
  std::string descriptor = wallet.get_descriptor(
      internal ? DescriptorPath::INTERNAL_ALL : DescriptorPath::EXTERNAL_ALL);
  int index =
      wallet.is_escrow()
          ? -1
          : storage_->GetCurrentAddressIndex(chain, wallet_id, internal) + 1;

  if (SupportBatchLookAhead()) {
    while (true) {
      std::vector<std::string> addresses;
      std::vector<int> indexes;
      for (int i = index; i < index + wallet.get_gap_limit(); i++) {
        addresses.push_back(
            CoreUtils::getInstance().DeriveAddress(descriptor, i));
        indexes.push_back(i);
      }
      int last = BatchLookAhead(chain, wallet_id, addresses, indexes, internal);
      if (last < wallet.get_gap_limit() - 1) {
        index = index + last + 1;
        auto address =
            CoreUtils::getInstance().DeriveAddress(descriptor, index);
        storage_->AddAddress(chain, wallet_id, address, index, internal);
        return address;
      }
      index = index + wallet.get_gap_limit();
    }
  }

  while (true) {
    auto address = CoreUtils::getInstance().DeriveAddress(descriptor, index);
    if (!LookAhead(chain, wallet_id, address, index, internal)) {
      storage_->AddAddress(chain, wallet_id, address, index, internal);
      return address;
    }
    index++;
  }
}

}  // namespace nunchuk
