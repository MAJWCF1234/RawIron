#include "RawIron/Runtime/EosRendezvousProvider.h"

#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Core/Log.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#if defined(RAWIRON_HAS_EOS)
#include "eos_sdk.h"
#include "eos_auth.h"
#include "eos_connect.h"
#include "eos_lobby.h"
#include "eos_logging.h"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#endif

namespace ri::runtime {
namespace {

struct EosCredentials {
    std::string productName = "RawIron";
    std::string productVersion = "1.0.0";
    std::string productId;
    std::string sandboxId;
    std::string deploymentId;
    std::string clientId;
    std::string clientSecret;

    /// An unedited copy of eos_config.example.json fills every field with a `YOUR_EOS_*` marker.
    /// Treating those as real credentials pushes the failure all the way down to an opaque EOS
    /// init error, so they are rejected here where the fix can be described.
    [[nodiscard]] static bool IsPlaceholder(const std::string& value) {
        return value.empty() || value.starts_with("YOUR_EOS_") || value.starts_with("YOUR_");
    }

    [[nodiscard]] bool Ready() const {
        return !IsPlaceholder(productId) && !IsPlaceholder(sandboxId) && !IsPlaceholder(deploymentId) &&
               !IsPlaceholder(clientId) && !IsPlaceholder(clientSecret);
    }
};

[[nodiscard]] std::optional<std::filesystem::path> FindEosConfigPath() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {
        fs::current_path() / "eos_config.json",
        fs::current_path() / "config" / "eos_config.json",
        fs::current_path() / "ThirdParty" / "EOS" / "eos_config.json",
    };
    // Walk up from cwd looking for a RawIron-style root with config.
    fs::path walk = fs::current_path();
    for (int i = 0; i < 6; ++i) {
        candidates.push_back(walk / "eos_config.json");
        candidates.push_back(walk / "config" / "eos_config.json");
        candidates.push_back(walk / "ThirdParty" / "EOS" / "eos_config.json");
        if (!walk.has_parent_path() || walk == walk.root_path()) {
            break;
        }
        walk = walk.parent_path();
    }
    for (const fs::path& p : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(p, ec)) {
            return p;
        }
    }
    return std::nullopt;
}

/// An environment variable that is set but empty is treated as absent, so a blank
/// EOS_CLIENT_SECRET cannot shadow a valid one in eos_config.json.
[[nodiscard]] std::optional<std::string> ReadEnvironmentValue(const char* name) {
#if defined(_MSC_VER)
    // MSVC deprecates std::getenv; _dupenv_s returns an owned copy that must be freed.
    char* raw = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&raw, &length, name) != 0 || raw == nullptr) {
        return std::nullopt;
    }
    std::string value(raw);
    std::free(raw);
#else
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
        return std::nullopt;
    }
    std::string value(raw);
#endif
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] EosCredentials LoadEosCredentials() {
    EosCredentials creds{};

    // File first (defaults / local developer config), then non-empty environment overrides.
    // Launchers and CI inject EOS_* secrets that must win over a checked-in or stale
    // eos_config.json — including placeholder YOUR_EOS_* values left from the example file.
    const auto path = FindEosConfigPath();
    if (path.has_value()) {
        const std::string text = ri::core::detail::ReadTextFile(*path);
        if (!text.empty()) {
            if (const auto v = ri::core::detail::ExtractJsonString(text, "product_name")) {
                creds.productName = *v;
            }
            if (const auto v = ri::core::detail::ExtractJsonString(text, "product_version")) {
                creds.productVersion = *v;
            }
            if (const auto v = ri::core::detail::ExtractJsonString(text, "product_id")) {
                creds.productId = *v;
            }
            if (const auto v = ri::core::detail::ExtractJsonString(text, "sandbox_id")) {
                creds.sandboxId = *v;
            }
            if (const auto v = ri::core::detail::ExtractJsonString(text, "deployment_id")) {
                creds.deploymentId = *v;
            }
            if (const auto v = ri::core::detail::ExtractJsonString(text, "client_id")) {
                creds.clientId = *v;
            }
            if (const auto v = ri::core::detail::ExtractJsonString(text, "client_secret")) {
                creds.clientSecret = *v;
            }
            ri::core::LogInfo("EOS: loaded credentials from " + path->string());
        }
    }

    if (const auto v = ReadEnvironmentValue("EOS_PRODUCT_ID")) {
        creds.productId = *v;
    }
    if (const auto v = ReadEnvironmentValue("EOS_SANDBOX_ID")) {
        creds.sandboxId = *v;
    }
    if (const auto v = ReadEnvironmentValue("EOS_DEPLOYMENT_ID")) {
        creds.deploymentId = *v;
    }
    if (const auto v = ReadEnvironmentValue("EOS_CLIENT_ID")) {
        creds.clientId = *v;
    }
    if (const auto v = ReadEnvironmentValue("EOS_CLIENT_SECRET")) {
        creds.clientSecret = *v;
    }
    return creds;
}

#if defined(RAWIRON_HAS_EOS)

class EosRendezvousProvider final : public IRendezvousProvider {
public:
    ~EosRendezvousProvider() override { Shutdown(); }

    bool Startup() override {
        credentials_ = LoadEosCredentials();
        if (!credentials_.Ready()) {
            ri::core::LogInfo(
                "EOS: credentials missing or still placeholders. Copy ThirdParty/EOS/eos_config.example.json "
                "to eos_config.json (repo root or config/) and replace every YOUR_EOS_* value with your Epic "
                "Dev Portal IDs, or set EOS_* env vars.");
            return false;
        }
        ri::core::LogInfo("EOS: credentials loaded for product '" + credentials_.productName + "'.");

        EOS_InitializeOptions initOpts{};
        initOpts.ApiVersion = EOS_INITIALIZE_API_LATEST;
        initOpts.ProductName = credentials_.productName.c_str();
        initOpts.ProductVersion = credentials_.productVersion.c_str();
        ri::core::LogInfo("EOS: calling EOS_Initialize...");
        const EOS_EResult initResult = EOS_Initialize(&initOpts);
        ri::core::LogInfo(std::string("EOS_Initialize -> ") + EOS_EResult_ToString(initResult));
        if (initResult != EOS_EResult::EOS_Success && initResult != EOS_EResult::EOS_AlreadyConfigured) {
            return false;
        }
        eosInitialized_ = true;

        cacheDir_ = (std::filesystem::temp_directory_path() / "RawIronEOSCache").string();
        std::error_code ec;
        std::filesystem::create_directories(cacheDir_, ec);

        EOS_Platform_Options platformOpts{};
        platformOpts.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
        platformOpts.ProductId = credentials_.productId.c_str();
        platformOpts.SandboxId = credentials_.sandboxId.c_str();
        platformOpts.ClientCredentials.ClientId = credentials_.clientId.c_str();
        platformOpts.ClientCredentials.ClientSecret = credentials_.clientSecret.c_str();
        platformOpts.bIsServer = EOS_FALSE;
        platformOpts.DeploymentId = credentials_.deploymentId.c_str();
        platformOpts.Flags = EOS_PF_DISABLE_OVERLAY;
        platformOpts.CacheDirectory = cacheDir_.c_str();
        platformOpts.TickBudgetInMilliseconds = 0;

        ri::core::LogInfo("EOS: calling EOS_Platform_Create...");
        platform_ = EOS_Platform_Create(&platformOpts);
        if (platform_ == nullptr) {
            ri::core::LogInfo("EOS_Platform_Create returned null — check product/sandbox/deployment/client credentials.");
            Shutdown();
            return false;
        }
        connect_ = EOS_Platform_GetConnectInterface(platform_);
        lobby_ = EOS_Platform_GetLobbyInterface(platform_);
        if (connect_ == nullptr || lobby_ == nullptr) {
            ri::core::LogInfo("EOS: failed to acquire Connect/Lobby interfaces.");
            Shutdown();
            return false;
        }

        ri::core::LogInfo("EOS: DeviceId Connect login...");
        if (!LoginDeviceId()) {
            Shutdown();
            return false;
        }

        // Intentionally no background tick thread: EOS_Platform_Tick must stay on one
        // thread. AuthoritativeNetModule::OnRuntimeFrame and Wait() drive ticks.
        ri::core::LogInfo("EOS rendezvous online (DeviceId Connect login).");
        return true;
    }

    void Shutdown() override {
        ReleaseActiveLobby();
        if (platform_ != nullptr) {
            EOS_Platform_Release(platform_);
            platform_ = nullptr;
        }
        connect_ = nullptr;
        lobby_ = nullptr;
        localUserId_ = nullptr;
        loggedIn_ = false;
        retainedSlots_.clear();
        if (eosInitialized_) {
            EOS_Shutdown();
            eosInitialized_ = false;
        }
    }

    void Tick() override {
        TickPlatform();
        PruneRetainedSlots();
    }

    [[nodiscard]] std::optional<std::string> IssueJoinCode(const JoinCodeIssueRequest& request) override {
        if (!loggedIn_ || lobby_ == nullptr) {
            return std::nullopt;
        }
        if (request.hostEndpoint.host.empty() || request.hostEndpoint.port == 0) {
            return std::nullopt;
        }

        const std::string token = EncodeDirectJoinToken(request);
        std::string lobbyId;
        {
            auto slot = MakeSlot();
            EOS_Lobby_CreateLobbyOptions opts{};
            opts.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
            opts.LocalUserId = localUserId_;
            opts.MaxLobbyMembers = 8;
            opts.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
            opts.bPresenceEnabled = EOS_FALSE;
            opts.bAllowInvites = EOS_TRUE;
            opts.BucketId = "rawiron";
            opts.bDisableHostMigration = EOS_FALSE;
            opts.bEnableRTCRoom = EOS_FALSE;
            opts.LocalRTCOptions = nullptr;
            opts.LobbyId = nullptr;
            opts.bEnableJoinById = EOS_TRUE;
            opts.bRejoinAfterKickRequiresInvite = EOS_FALSE;
            opts.AllowedPlatformIds = nullptr;
            opts.AllowedPlatformIdsCount = 0;
            opts.bCrossplayOptOut = EOS_FALSE;
            opts.RTCRoomJoinActionType = EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin;

            EOS_Lobby_CreateLobby(
                lobby_, &opts, slot.get(),
                [](const EOS_Lobby_CreateLobbyCallbackInfo* data) {
                    auto* s = static_cast<AsyncSlot*>(data->ClientData);
                    s->result = data->ResultCode;
                    if (data->LobbyId != nullptr) {
                        s->lobbyId = data->LobbyId;
                    }
                    s->done = true;
                    s->cv.notify_all();
                });
            if (!Wait(slot, std::chrono::seconds(15)) || slot->result != EOS_EResult::EOS_Success) {
                ri::core::LogInfo(std::string("EOS CreateLobby failed: ") + EOS_EResult_ToString(slot->result));
                return std::nullopt;
            }
            lobbyId = slot->lobbyId;
        }

        // Claim ownership the moment the lobby exists on EOS. Anything that fails from here on has
        // to tear it down, or the host leaves an advertised lobby behind that resolves to nothing.
        activeLobbyId_ = lobbyId;
        ownsActiveLobby_ = true;

        if (!SetLobbyTokenAttribute(lobbyId, token)) {
            ri::core::LogInfo("EOS: failed to publish RI_TOKEN lobby attribute; destroying the lobby.");
            ReleaseActiveLobby();
            return std::nullopt;
        }

        return "EOS:" + lobbyId;
    }

    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string& code) override {
        if (!loggedIn_ || lobby_ == nullptr) {
            return std::nullopt;
        }
        // Allow embedding DirectToken directly for mixed LAN/EOS workflows.
        if (auto direct = DecodeDirectJoinToken(code)) {
            return direct;
        }

        std::string lobbyId = code;
        constexpr std::string_view prefix{"EOS:"};
        if (lobbyId.starts_with(prefix)) {
            lobbyId = lobbyId.substr(prefix.size());
        }
        if (lobbyId.empty()) {
            return std::nullopt;
        }

        {
            auto slot = MakeSlot();
            EOS_Lobby_JoinLobbyByIdOptions opts{};
            opts.ApiVersion = EOS_LOBBY_JOINLOBBYBYID_API_LATEST;
            opts.LobbyId = lobbyId.c_str();
            opts.LocalUserId = localUserId_;
            opts.bPresenceEnabled = EOS_FALSE;
            opts.LocalRTCOptions = nullptr;
            opts.bCrossplayOptOut = EOS_FALSE;
            opts.RTCRoomJoinActionType = EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin;
            EOS_Lobby_JoinLobbyById(
                lobby_, &opts, slot.get(),
                [](const EOS_Lobby_JoinLobbyByIdCallbackInfo* data) {
                    auto* s = static_cast<AsyncSlot*>(data->ClientData);
                    s->result = data->ResultCode;
                    s->done = true;
                    s->cv.notify_all();
                });
            if (!Wait(slot, std::chrono::seconds(15)) || slot->result != EOS_EResult::EOS_Success) {
                ri::core::LogInfo(std::string("EOS JoinLobbyById failed: ") + EOS_EResult_ToString(slot->result));
                return std::nullopt;
            }
        }

        // Membership starts at the successful join, so every failure below has to leave again
        // rather than sitting in a lobby the caller believes it never entered.
        activeLobbyId_ = lobbyId;
        ownsActiveLobby_ = false;

        const auto token = ReadLobbyTokenAttribute(lobbyId);
        if (!token.has_value()) {
            ri::core::LogInfo("EOS: lobby joined but RI_TOKEN attribute missing; leaving the lobby.");
            ReleaseActiveLobby();
            return std::nullopt;
        }
        auto resolved = DecodeDirectJoinToken(*token);
        if (!resolved.has_value()) {
            ri::core::LogInfo("EOS: lobby RI_TOKEN attribute is malformed; leaving the lobby.");
            ReleaseActiveLobby();
            return std::nullopt;
        }
        return resolved;
    }

private:
    struct AsyncSlot {
        std::mutex mutex;
        std::condition_variable cv;
        std::atomic<bool> done{false};
        EOS_EResult result = EOS_EResult::EOS_UnexpectedError;
        std::string lobbyId;
        EOS_ProductUserId userId = nullptr;
        EOS_ContinuanceToken continuance = nullptr;
    };

    [[nodiscard]] static std::shared_ptr<AsyncSlot> MakeSlot() { return std::make_shared<AsyncSlot>(); }

    void PruneRetainedSlots() {
        retainedSlots_.erase(std::remove_if(retainedSlots_.begin(), retainedSlots_.end(),
                                            [](const std::shared_ptr<AsyncSlot>& slot) {
                                                return slot == nullptr || slot->done.load();
                                            }),
                             retainedSlots_.end());
    }

    void TickPlatform() {
        if (platform_ != nullptr) {
            EOS_Platform_Tick(platform_);
        }
    }

    /// Best-effort teardown of the lobby this provider created or joined. Without it a
    /// host that exits leaves an advertised lobby behind until EOS times it out, and
    /// stale codes keep resolving to a dead endpoint.
    void ReleaseActiveLobby() {
        if (lobby_ == nullptr || localUserId_ == nullptr || activeLobbyId_.empty()) {
            activeLobbyId_.clear();
            ownsActiveLobby_ = false;
            return;
        }

        auto slot = MakeSlot();
        if (ownsActiveLobby_) {
            EOS_Lobby_DestroyLobbyOptions opts{};
            opts.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
            opts.LocalUserId = localUserId_;
            opts.LobbyId = activeLobbyId_.c_str();
            EOS_Lobby_DestroyLobby(lobby_, &opts, slot.get(),
                                   [](const EOS_Lobby_DestroyLobbyCallbackInfo* data) {
                                       auto* s = static_cast<AsyncSlot*>(data->ClientData);
                                       s->result = data->ResultCode;
                                       s->done = true;
                                       s->cv.notify_all();
                                   });
        } else {
            EOS_Lobby_LeaveLobbyOptions opts{};
            opts.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
            opts.LocalUserId = localUserId_;
            opts.LobbyId = activeLobbyId_.c_str();
            EOS_Lobby_LeaveLobby(lobby_, &opts, slot.get(),
                                 [](const EOS_Lobby_LeaveLobbyCallbackInfo* data) {
                                     auto* s = static_cast<AsyncSlot*>(data->ClientData);
                                     s->result = data->ResultCode;
                                     s->done = true;
                                     s->cv.notify_all();
                                 });
        }
        // Shutdown must not hang on a dead service; the platform release that follows
        // cancels anything still outstanding.
        (void)Wait(slot, std::chrono::seconds(3));
        activeLobbyId_.clear();
        ownsActiveLobby_ = false;
    }

    void PumpOsMessages() {
#if defined(_WIN32)
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
#endif
    }

    bool Wait(const std::shared_ptr<AsyncSlot>& slot, std::chrono::milliseconds timeout) {
        if (slot == nullptr) {
            return false;
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        auto lastLog = std::chrono::steady_clock::now();
        while (!slot->done.load()) {
            TickPlatform();
            PumpOsMessages();
            const auto now = std::chrono::steady_clock::now();
            if (now > deadline) {
                // Keep the slot alive until the in-flight EOS callback completes; Tick()
                // continues to pump the platform and will prune once done.
                retainedSlots_.push_back(slot);
                ri::core::LogInfo("EOS: async wait timed out.");
                return false;
            }
            if (now - lastLog > std::chrono::seconds(2)) {
                ri::core::LogInfo("EOS: waiting for online services…");
                lastLog = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    bool LoginDeviceId() {
        {
            auto slot = MakeSlot();
            EOS_Connect_CreateDeviceIdOptions opts{};
            opts.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
            opts.DeviceModel = "PC Windows RawIron";
            EOS_Connect_CreateDeviceId(
                connect_, &opts, slot.get(),
                [](const EOS_Connect_CreateDeviceIdCallbackInfo* data) {
                    auto* s = static_cast<AsyncSlot*>(data->ClientData);
                    s->result = data->ResultCode;
                    s->done = true;
                    s->cv.notify_all();
                });
            if (!Wait(slot, std::chrono::seconds(12))) {
                return false;
            }
            if (slot->result != EOS_EResult::EOS_Success &&
                slot->result != EOS_EResult::EOS_DuplicateNotAllowed) {
                ri::core::LogInfo(std::string("EOS CreateDeviceId failed: ") + EOS_EResult_ToString(slot->result));
                return false;
            }
        }

        auto loginSlot = MakeSlot();
        EOS_Connect_Credentials creds{};
        creds.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
        creds.Token = nullptr;
        creds.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;

        EOS_Connect_UserLoginInfo userInfo{};
        userInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
        userInfo.DisplayName = "RawIron";
        userInfo.NsaIdToken = nullptr;

        EOS_Connect_LoginOptions loginOpts{};
        loginOpts.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
        loginOpts.Credentials = &creds;
        loginOpts.UserLoginInfo = &userInfo;

        EOS_Connect_Login(
            connect_, &loginOpts, loginSlot.get(),
            [](const EOS_Connect_LoginCallbackInfo* data) {
                auto* s = static_cast<AsyncSlot*>(data->ClientData);
                s->result = data->ResultCode;
                s->userId = data->LocalUserId;
                s->continuance = data->ContinuanceToken;
                s->done = true;
                s->cv.notify_all();
            });
        if (!Wait(loginSlot, std::chrono::seconds(15))) {
            return false;
        }

        if (loginSlot->result == EOS_EResult::EOS_InvalidUser) {
            auto createSlot = MakeSlot();
            EOS_Connect_CreateUserOptions createOpts{};
            createOpts.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
            createOpts.ContinuanceToken = loginSlot->continuance;
            EOS_Connect_CreateUser(
                connect_, &createOpts, createSlot.get(),
                [](const EOS_Connect_CreateUserCallbackInfo* data) {
                    auto* s = static_cast<AsyncSlot*>(data->ClientData);
                    s->result = data->ResultCode;
                    s->userId = data->LocalUserId;
                    s->done = true;
                    s->cv.notify_all();
                });
            if (!Wait(createSlot, std::chrono::seconds(15)) || createSlot->result != EOS_EResult::EOS_Success) {
                ri::core::LogInfo(std::string("EOS CreateUser failed: ") + EOS_EResult_ToString(createSlot->result));
                return false;
            }
            localUserId_ = createSlot->userId;
        } else if (loginSlot->result == EOS_EResult::EOS_Success) {
            localUserId_ = loginSlot->userId;
        } else {
            ri::core::LogInfo(std::string("EOS Connect_Login failed: ") + EOS_EResult_ToString(loginSlot->result));
            return false;
        }

        loggedIn_ = localUserId_ != nullptr;
        return loggedIn_;
    }

    bool SetLobbyTokenAttribute(const std::string& lobbyId, const std::string& token) {
        EOS_HLobbyModification mod = nullptr;
        EOS_Lobby_UpdateLobbyModificationOptions modOpts{};
        modOpts.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
        modOpts.LocalUserId = localUserId_;
        modOpts.LobbyId = lobbyId.c_str();
        if (EOS_Lobby_UpdateLobbyModification(lobby_, &modOpts, &mod) != EOS_EResult::EOS_Success || mod == nullptr) {
            return false;
        }

        EOS_Lobby_AttributeData attr{};
        attr.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        attr.Key = "RI_TOKEN";
        attr.Value.AsUtf8 = token.c_str();
        attr.ValueType = EOS_EAttributeType::EOS_AT_STRING;

        EOS_LobbyModification_AddAttributeOptions addOpts{};
        addOpts.ApiVersion = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
        addOpts.Attribute = &attr;
        addOpts.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
        if (EOS_LobbyModification_AddAttribute(mod, &addOpts) != EOS_EResult::EOS_Success) {
            EOS_LobbyModification_Release(mod);
            return false;
        }

        auto slot = MakeSlot();
        EOS_Lobby_UpdateLobbyOptions updateOpts{};
        updateOpts.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
        updateOpts.LobbyModificationHandle = mod;
        EOS_Lobby_UpdateLobby(
            lobby_, &updateOpts, slot.get(),
            [](const EOS_Lobby_UpdateLobbyCallbackInfo* data) {
                auto* s = static_cast<AsyncSlot*>(data->ClientData);
                s->result = data->ResultCode;
                s->done = true;
                s->cv.notify_all();
            });
        const bool ok = Wait(slot, std::chrono::seconds(20)) && slot->result == EOS_EResult::EOS_Success;
        EOS_LobbyModification_Release(mod);
        return ok;
    }

    [[nodiscard]] std::optional<std::string> ReadLobbyTokenAttribute(const std::string& lobbyId) {
        EOS_HLobbyDetails details = nullptr;
        EOS_Lobby_CopyLobbyDetailsHandleOptions copyOpts{};
        copyOpts.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
        copyOpts.LobbyId = lobbyId.c_str();
        copyOpts.LocalUserId = localUserId_;
        if (EOS_Lobby_CopyLobbyDetailsHandle(lobby_, &copyOpts, &details) != EOS_EResult::EOS_Success ||
            details == nullptr) {
            return std::nullopt;
        }

        EOS_Lobby_Attribute* attribute = nullptr;
        EOS_LobbyDetails_CopyAttributeByKeyOptions keyOpts{};
        keyOpts.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;
        keyOpts.AttrKey = "RI_TOKEN";
        const EOS_EResult rc = EOS_LobbyDetails_CopyAttributeByKey(details, &keyOpts, &attribute);
        std::optional<std::string> out;
        if (rc == EOS_EResult::EOS_Success && attribute != nullptr && attribute->Data != nullptr &&
            attribute->Data->ValueType == EOS_EAttributeType::EOS_AT_STRING &&
            attribute->Data->Value.AsUtf8 != nullptr) {
            out = attribute->Data->Value.AsUtf8;
        }
        if (attribute != nullptr) {
            EOS_Lobby_Attribute_Release(attribute);
        }
        EOS_LobbyDetails_Release(details);
        return out;
    }

    EosCredentials credentials_{};
    std::string cacheDir_{};
    EOS_HPlatform platform_ = nullptr;
    EOS_HConnect connect_ = nullptr;
    EOS_HLobby lobby_ = nullptr;
    EOS_ProductUserId localUserId_ = nullptr;
    bool loggedIn_ = false;
    bool eosInitialized_ = false;
    std::string activeLobbyId_{};
    bool ownsActiveLobby_ = false;
    /// Slots abandoned by Wait() timeout; kept until late EOS callbacks finish writing.
    std::vector<std::shared_ptr<AsyncSlot>> retainedSlots_{};
};

#else

class EosRendezvousProvider final : public IRendezvousProvider {
public:
    bool Startup() override {
        ri::core::LogInfo("EOS rendezvous unavailable: rebuild with -DRAWIRON_USE_EOS=ON and install ThirdParty/EOS/SDK.");
        return false;
    }
    void Shutdown() override {}
    [[nodiscard]] std::optional<std::string> IssueJoinCode(const JoinCodeIssueRequest&) override { return std::nullopt; }
    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string&) override { return std::nullopt; }
};

#endif

} // namespace

std::unique_ptr<IRendezvousProvider> CreateEosRendezvousProvider() {
    return std::make_unique<EosRendezvousProvider>();
}

} // namespace ri::runtime
