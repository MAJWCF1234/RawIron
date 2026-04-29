#include "RawIron/Audio/AudioBackendMiniaudio.h"
#include "RawIron/Audio/AudioEnvironment.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

extern "C" {
#include "miniaudio.h"
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace ri::audio {
namespace {

#if defined(_WIN32)
[[nodiscard]] std::wstring Utf8ClipPathToWide(const std::string& utf8) {
#if defined(__cpp_lib_char8_t) && __cpp_lib_char8_t >= 201811L
    const std::filesystem::path path(std::u8string_view(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
#else
    const std::filesystem::path path = std::filesystem::u8path(utf8);
#endif
    return path.wstring();
}
#endif

void AppendResult(std::string* out, ma_result result) {
    if (out == nullptr) {
        return;
    }
    char buffer[128]{};
    std::snprintf(buffer, sizeof(buffer), "miniaudio error %d", static_cast<int>(result));
    *out += buffer;
}

class MiniaudioAudioBackend;

class MiniaudioPlaybackHandle final : public AudioPlaybackHandle,
                                      public std::enable_shared_from_this<MiniaudioPlaybackHandle> {
public:
    MiniaudioPlaybackHandle(std::shared_ptr<MiniaudioAudioBackend> backend,
                            AudioClipRequest request);
    ~MiniaudioPlaybackHandle() override;

    [[nodiscard]] bool Initialize(std::string* errorOut);

    void Play() final;
    void Pause() final;
    void Stop() final;
    void Unload() final;

    [[nodiscard]] double GetCurrentTime() const final;
    void SetCurrentTime(double value) final;
    [[nodiscard]] double GetVolume() const final;
    void SetVolume(double value) final;
    [[nodiscard]] double GetDuration() const final;
    [[nodiscard]] bool IsPlaying() const final;
    [[nodiscard]] double GetPlaybackRate() const final;
    void SetPlaybackRate(double value) final;
    void SetFinishedCallback(FinishedCallback callback) final;

private:
    [[nodiscard]] MiniaudioAudioBackend* Backend() const;

    void UnloadInternal();
    static void EndProc(void* userData, ma_sound* sound);
    void DispatchFinished();

    std::shared_ptr<MiniaudioAudioBackend> backend_;
    AudioClipRequest request_;
    ma_sound sound_{};
    bool initialized_ = false;
    std::atomic<bool> unloaded_{false};
    mutable std::mutex callbackMutex_;
    FinishedCallback finishedCallback_{};
};

class MiniaudioAudioBackend final : public AudioBackend, public std::enable_shared_from_this<MiniaudioAudioBackend> {
public:
    MiniaudioAudioBackend(std::string* errorOut, const MiniaudioBackendSettings* userSettings);
    ~MiniaudioAudioBackend() override;

    MiniaudioAudioBackend(const MiniaudioAudioBackend&) = delete;
    MiniaudioAudioBackend& operator=(const MiniaudioAudioBackend&) = delete;

    [[nodiscard]] bool IsReady() const noexcept {
        return ready_;
    }

    [[nodiscard]] ma_engine* Engine() noexcept {
        return &engine_;
    }
    [[nodiscard]] bool NoSpatialization() const noexcept {
        return noSpatialization_;
    }

    [[nodiscard]] std::shared_ptr<AudioPlaybackHandle> CreatePlayback(const AudioClipRequest& request) override;
    void PumpHostThreadAudioWork() override;

    void EnqueueHostWork(std::function<void()> job) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        hostWorkQueue_.push_back(std::move(job));
    }

private:
    ma_engine engine_{};
    bool ready_ = false;
    bool noSpatialization_ = true;
    std::mutex queueMutex_;
    std::vector<std::function<void()>> hostWorkQueue_;
};

MiniaudioPlaybackHandle::MiniaudioPlaybackHandle(std::shared_ptr<MiniaudioAudioBackend> backend,
                                                 AudioClipRequest request)
    : backend_(std::move(backend)),
      request_(std::move(request)) {}

MiniaudioPlaybackHandle::~MiniaudioPlaybackHandle() {
    UnloadInternal();
}

[[nodiscard]] bool MiniaudioPlaybackHandle::Initialize(std::string* errorOut) {
    MiniaudioAudioBackend* engine = Backend();
    if (engine == nullptr) {
        return false;
    }

#if defined(_WIN32)
    const std::wstring widePath = Utf8ClipPathToWide(request_.path);
    ma_uint32 flags = MA_SOUND_FLAG_DECODE;
    if (engine->NoSpatialization() && !request_.spatialized) {
        flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    }
    const ma_result result =
        ma_sound_init_from_file_w(engine->Engine(), widePath.c_str(), flags, nullptr, nullptr, &sound_);
#else
    ma_uint32 flags = MA_SOUND_FLAG_DECODE;
    if (engine->NoSpatialization() && !request_.spatialized) {
        flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    }
    const ma_result result = ma_sound_init_from_file(
        engine->Engine(), request_.path.c_str(), flags, nullptr, nullptr, &sound_);
#endif
    if (result != MA_SUCCESS) {
        if (errorOut != nullptr) {
            errorOut->assign("open failed: ");
            errorOut->append(request_.path);
            errorOut->append(" ");
            AppendResult(errorOut, result);
        }
        return false;
    }

    initialized_ = true;
    ma_sound_set_looping(&sound_, request_.loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&sound_, static_cast<float>(ClampAudioVolume(request_.volume, 1.0)));
    ma_sound_set_pitch(&sound_, static_cast<float>(ClampAudioPlaybackRate(request_.playbackRate, 1.0)));
    ma_sound_set_pan(&sound_, static_cast<float>(ClampAudioPan(request_.pan, 0.0)));
    ma_sound_set_min_distance(&sound_, static_cast<float>(std::max(0.05, request_.minDistance)));
    ma_sound_set_max_distance(&sound_, static_cast<float>(std::max(request_.minDistance + 0.05, request_.maxDistance)));
    if (request_.spatialized) {
        ma_sound_set_spatialization_enabled(&sound_, MA_TRUE);
        ma_sound_set_position(&sound_, request_.positionX, request_.positionY, request_.positionZ);
    }
    ma_sound_set_end_callback(&sound_, &MiniaudioPlaybackHandle::EndProc, this);
    return true;
}

void MiniaudioPlaybackHandle::Play() {
    if (unloaded_ || !initialized_) {
        return;
    }
    (void)ma_sound_start(&sound_);
}

void MiniaudioPlaybackHandle::Pause() {
    if (unloaded_ || !initialized_) {
        return;
    }
    (void)ma_sound_stop(&sound_);
}

void MiniaudioPlaybackHandle::Stop() {
    if (unloaded_ || !initialized_) {
        return;
    }
    (void)ma_sound_stop(&sound_);
    (void)ma_sound_seek_to_pcm_frame(&sound_, 0);
}

void MiniaudioPlaybackHandle::Unload() {
    UnloadInternal();
}

double MiniaudioPlaybackHandle::GetCurrentTime() const {
    if (unloaded_ || !initialized_) {
        return 0.0;
    }
    float seconds = 0.0F;
    // miniaudio queries are not const-qualified; logical const only.
    if (ma_sound_get_cursor_in_seconds(const_cast<ma_sound*>(&sound_), &seconds) != MA_SUCCESS) {
        return 0.0;
    }
    return static_cast<double>(seconds);
}

void MiniaudioPlaybackHandle::SetCurrentTime(double value) {
    if (unloaded_ || !initialized_) {
        return;
    }
    MiniaudioAudioBackend* engine = Backend();
    if (engine == nullptr) {
        return;
    }
    const double seekSeconds = std::max(0.0, std::isfinite(value) ? value : 0.0);
    float durationSeconds = 0.0F;
    if (ma_sound_get_length_in_seconds(const_cast<ma_sound*>(&sound_), &durationSeconds) == MA_SUCCESS
        && std::isfinite(durationSeconds) && durationSeconds > 0.0f) {
        if (seekSeconds > static_cast<double>(durationSeconds)) {
            durationSeconds = std::max(0.0f, durationSeconds);
        } else {
            durationSeconds = static_cast<float>(seekSeconds);
        }
    } else {
        durationSeconds = static_cast<float>(seekSeconds);
    }
    const ma_uint32 sampleRate = ma_engine_get_sample_rate(engine->Engine());
    const auto frame = static_cast<ma_uint64>(static_cast<double>(durationSeconds) * static_cast<double>(sampleRate));
    (void)ma_sound_seek_to_pcm_frame(&sound_, frame);
}

double MiniaudioPlaybackHandle::GetVolume() const {
    if (unloaded_ || !initialized_) {
        return 0.0;
    }
    return static_cast<double>(ma_sound_get_volume(&sound_));
}

void MiniaudioPlaybackHandle::SetVolume(double value) {
    if (unloaded_ || !initialized_) {
        return;
    }
    ma_sound_set_volume(&sound_, static_cast<float>(ClampAudioVolume(value, 1.0)));
}

double MiniaudioPlaybackHandle::GetDuration() const {
    if (unloaded_ || !initialized_) {
        return 0.0;
    }
    float length = 0.0F;
    if (ma_sound_get_length_in_seconds(const_cast<ma_sound*>(&sound_), &length) != MA_SUCCESS) {
        return 0.0;
    }
    return static_cast<double>(length);
}

bool MiniaudioPlaybackHandle::IsPlaying() const {
    if (unloaded_ || !initialized_) {
        return false;
    }
    return ma_sound_is_playing(&sound_) != MA_FALSE;
}

double MiniaudioPlaybackHandle::GetPlaybackRate() const {
    if (unloaded_ || !initialized_) {
        return 1.0;
    }
    return static_cast<double>(ma_sound_get_pitch(&sound_));
}

void MiniaudioPlaybackHandle::SetPlaybackRate(double value) {
    if (unloaded_ || !initialized_) {
        return;
    }
    ma_sound_set_pitch(&sound_, static_cast<float>(ClampAudioPlaybackRate(value, 1.0)));
}

void MiniaudioPlaybackHandle::SetFinishedCallback(FinishedCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    finishedCallback_ = std::move(callback);
}

MiniaudioAudioBackend* MiniaudioPlaybackHandle::Backend() const {
    return backend_.get();
}

void MiniaudioPlaybackHandle::UnloadInternal() {
    if (unloaded_) {
        return;
    }
    unloaded_ = true;
    if (initialized_) {
        ma_sound_set_end_callback(&sound_, nullptr, nullptr);
        ma_sound_stop(&sound_);
        ma_sound_uninit(&sound_);
        initialized_ = false;
    }
}

void MiniaudioPlaybackHandle::EndProc(void* userData, ma_sound* /*sound*/) {
    auto* self = static_cast<MiniaudioPlaybackHandle*>(userData);
    if (self == nullptr || self->unloaded_) {
        return;
    }
    if (ma_sound_is_looping(&self->sound_) != MA_FALSE) {
        return;
    }
    std::shared_ptr<MiniaudioAudioBackend> backend = self->backend_;
    if (backend == nullptr) {
        return;
    }
    std::shared_ptr<MiniaudioPlaybackHandle> keepAlive;
    try {
        keepAlive = self->shared_from_this();
    } catch (...) {
        return;
    }
    backend->EnqueueHostWork([keepAlive]() { keepAlive->DispatchFinished(); });
}

void MiniaudioPlaybackHandle::DispatchFinished() {
    if (unloaded_) {
        return;
    }
    FinishedCallback callbackCopy;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callbackCopy = finishedCallback_;
    }
    if (callbackCopy) {
        callbackCopy();
    }
}

MiniaudioAudioBackend::MiniaudioAudioBackend(std::string* errorOut, const MiniaudioBackendSettings* userSettings) {
    MiniaudioBackendSettings resolved{};
    if (userSettings != nullptr) {
        resolved = *userSettings;
    }

    noSpatialization_ = resolved.noSpatialization;

    ma_engine_config engineConfig = ma_engine_config_init();
    if (resolved.periodSizeInMilliseconds > 0) {
        engineConfig.periodSizeInMilliseconds = resolved.periodSizeInMilliseconds;
    }

    const ma_result result = ma_engine_init(&engineConfig, &engine_);
    if (result != MA_SUCCESS) {
        if (errorOut != nullptr) {
            errorOut->assign("ma_engine_init failed: ");
            AppendResult(errorOut, result);
        }
        return;
    }
    ma_engine_listener_set_position(&engine_, 0,
                                    resolved.listenerPosition[0],
                                    resolved.listenerPosition[1],
                                    resolved.listenerPosition[2]);
    ma_engine_listener_set_direction(&engine_, 0,
                                     resolved.listenerDirection[0],
                                     resolved.listenerDirection[1],
                                     resolved.listenerDirection[2]);
    ma_engine_listener_set_world_up(&engine_, 0,
                                    resolved.listenerWorldUp[0],
                                    resolved.listenerWorldUp[1],
                                    resolved.listenerWorldUp[2]);
    ready_ = true;
}

MiniaudioAudioBackend::~MiniaudioAudioBackend() {
    if (ready_) {
        ma_engine_uninit(&engine_);
    }
}

std::shared_ptr<AudioPlaybackHandle> MiniaudioAudioBackend::CreatePlayback(const AudioClipRequest& request) {
    if (!ready_ || request.path.empty()) {
        return nullptr;
    }

    std::shared_ptr<MiniaudioAudioBackend> alive = shared_from_this();
    auto handle = std::make_shared<MiniaudioPlaybackHandle>(std::move(alive), request);
    std::string localError;
    if (!handle->Initialize(&localError)) {
        return nullptr;
    }
    return handle;
}

void MiniaudioAudioBackend::PumpHostThreadAudioWork() {
    std::vector<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        batch.swap(hostWorkQueue_);
    }
    for (std::function<void()>& job : batch) {
        if (job) {
            job();
        }
    }
}

} // namespace

std::shared_ptr<AudioBackend> CreateMiniaudioAudioBackend(std::string* errorMessage,
                                                          const MiniaudioBackendSettings* settings) {
    try {
        auto impl = std::make_shared<MiniaudioAudioBackend>(errorMessage, settings);
        if (!impl->IsReady()) {
            return nullptr;
        }
        return impl;
    } catch (...) {
        if (errorMessage != nullptr) {
            errorMessage->assign("miniaudio backend constructor threw");
        }
        return nullptr;
    }
}

} // namespace ri::audio
