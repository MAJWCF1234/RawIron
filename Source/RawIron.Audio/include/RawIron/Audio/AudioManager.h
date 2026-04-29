#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::audio {

struct AudioEnvironmentProfileInput {
    std::string label;
    std::vector<std::string> activeVolumes;
    std::optional<double> reverbMix;
    std::optional<double> echoDelayMs;
    std::optional<double> echoFeedback;
    std::optional<double> dampening;
    std::optional<double> volumeScale;
    std::optional<double> playbackRate;
};

struct AudioEnvironmentProfile {
    std::string label;
    double reverbMix = 0.0;
    double echoDelayMs = 0.0;
    double echoFeedback = 0.0;
    double dampening = 0.0;
    double volumeScale = 1.0;
    double playbackRate = 1.0;
    std::vector<std::string> activeVolumes;
};

struct AudioPlaybackRequest {
    double volume = 1.0;
    double playbackRate = 1.0;
    /// 0 = center, -1 = full left, +1 = full right.
    double pan = 0.0;
    /// Dry-path occlusion scalar from ray/portal tests (0 clear, 1 fully occluded).
    double occlusion = 0.0;
    /// Distance from listener in meters for simple distance attenuation.
    double distanceMeters = 0.0;
};

struct AudioResolvedPlayback {
    double volume = 1.0;
    double playbackRate = 1.0;
    double pan = 0.0;
    double occlusion = 0.0;
    double distanceMeters = 0.0;
    std::optional<AudioEnvironmentProfile> profile;
};

struct AudioClipRequest {
    std::string path;
    bool loop = false;
    double volume = 1.0;
    double playbackRate = 1.0;
    double pan = 0.0;
    bool spatialized = false;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    double minDistance = 1.0;
    double maxDistance = 64.0;
};

struct AudioManagerOptions {
    bool muted = false;
};

struct AudioManagerMetrics {
    std::size_t managedSounds = 0;
    std::size_t loopsCreated = 0;
    std::size_t oneShotsPlayed = 0;
    std::size_t voicesPlayed = 0;
    bool voiceActive = false;
    std::size_t environmentChanges = 0;
    std::string activeEnvironment = "none";
    double activeEnvironmentMix = 0.0;
    std::size_t pendingEchoes = 0;
    std::size_t droppedEchoes = 0;
};

class AudioPlaybackHandle {
public:
    using FinishedCallback = std::function<void()>;

    virtual ~AudioPlaybackHandle() = default;

    virtual void Play() = 0;
    virtual void Pause() = 0;
    virtual void Stop() = 0;
    virtual void Unload() = 0;
    [[nodiscard]] virtual double GetCurrentTime() const = 0;
    virtual void SetCurrentTime(double value) = 0;
    [[nodiscard]] virtual double GetVolume() const = 0;
    virtual void SetVolume(double value) = 0;
    [[nodiscard]] virtual double GetDuration() const = 0;
    [[nodiscard]] virtual bool IsPlaying() const = 0;
    [[nodiscard]] virtual double GetPlaybackRate() const = 0;
    virtual void SetPlaybackRate(double value) = 0;
    virtual void SetFinishedCallback(FinishedCallback callback) = 0;
};

class AudioBackend {
public:
    virtual ~AudioBackend() = default;
    [[nodiscard]] virtual std::shared_ptr<AudioPlaybackHandle> CreatePlayback(const AudioClipRequest& request) = 0;

    /// Drain work posted from the audio device thread (e.g. playback-finished notifications).
    /// Must be invoked from the main / simulation thread; safe to call every frame.
    virtual void PumpHostThreadAudioWork() {}
};

class AudioManager;

class ManagedSound : public std::enable_shared_from_this<ManagedSound> {
public:
    using SoundId = std::uint64_t;

    void Play();
    void Pause();
    void Stop();
    void Unload();

    [[nodiscard]] double GetCurrentTime() const;
    void SetCurrentTime(double value);

    [[nodiscard]] double GetVolume() const;
    void SetVolume(double value);

    [[nodiscard]] double GetPlaybackRate() const;
    void SetPlaybackRate(double value);

    [[nodiscard]] const std::string& GetPath() const;
    [[nodiscard]] double GetDuration() const;
    [[nodiscard]] bool IsPlaying() const;
    [[nodiscard]] bool IsLooping() const;
    [[nodiscard]] SoundId GetId() const;

private:
    friend class AudioManager;

    ManagedSound(SoundId soundId,
                 std::shared_ptr<AudioPlaybackHandle> playback,
                 std::string path,
                 bool loop,
                 AudioManager* manager);

    void ReleaseOnPlaybackFinished();

    SoundId soundId_ = 0;
    std::shared_ptr<AudioPlaybackHandle> playback_;
    std::string path_;
    bool loop_ = false;
    bool unloaded_ = false;
    AudioManager* manager_ = nullptr;
};

class AudioManager {
public:
    explicit AudioManager(std::shared_ptr<AudioBackend> backend, AudioManagerOptions options = {});

    void SetMuted(bool muted);
    [[nodiscard]] bool IsMuted() const;

    /// Global linear gain (0–1) applied to resolved clip volumes for newly created playback.
    /// Echo send levels follow the dry clip volume captured at schedule time.
    void SetMasterLinearGain(double gain);
    [[nodiscard]] double GetMasterLinearGain() const noexcept;

    [[nodiscard]] std::optional<AudioEnvironmentProfile> NormalizeEnvironmentProfile(
        const std::optional<AudioEnvironmentProfileInput>& profile) const;
    bool SetEnvironmentProfile(const std::optional<AudioEnvironmentProfileInput>& profile);
    [[nodiscard]] std::optional<AudioEnvironmentProfile> GetEnvironmentProfile() const;

    [[nodiscard]] AudioResolvedPlayback ApplyEnvironmentToPlayback(const AudioPlaybackRequest& request = {}) const;

    [[nodiscard]] std::shared_ptr<ManagedSound> CreateManagedSound(std::string_view filePath,
                                                                   double volume = 0.5,
                                                                   bool loop = false,
                                                                   double playbackRate = 1.0);
    [[nodiscard]] std::shared_ptr<ManagedSound> CreateLoopingSound(std::string_view filePath,
                                                                   double volume = 0.5);
    [[nodiscard]] std::shared_ptr<ManagedSound> PlayOneShot(std::string_view filePath,
                                                            double volume = 0.7);
    [[nodiscard]] std::shared_ptr<ManagedSound> PlayVoice(std::string_view filePath,
                                                          double volume = 1.0);

    void StopManagedSound(const std::shared_ptr<ManagedSound>& sound, bool unload = false);
    void Tick(double deltaMs);

    [[nodiscard]] AudioManagerMetrics GetMetrics() const;
    [[nodiscard]] std::size_t PendingEchoCount() const;

private:
    friend class ManagedSound;

    struct PendingEcho {
        std::string filePath;
        double volume = 0.0;
        double playbackRate = 1.0;
        double fireAtMs = 0.0;
    };

    [[nodiscard]] std::shared_ptr<ManagedSound> CreateManagedSoundInternal(std::string_view filePath,
                                                                           double volume,
                                                                           bool loop,
                                                                           double playbackRate,
                                                                           bool autoPlay,
                                                                           bool autoReleaseOnFinish,
                                                                           bool trackAsVoice,
                                                                           bool countLoopMetric,
                                                                           bool countOneShotMetric,
                                                                           bool countVoiceMetric);
    void ScheduleEcho(std::string_view filePath,
                      double volume,
                      double playbackRate,
                      const std::optional<AudioEnvironmentProfile>& profile);
    void UnregisterSound(ManagedSound::SoundId soundId);
    void HandlePlaybackFinished(ManagedSound::SoundId soundId, bool trackAsVoice);

    std::shared_ptr<AudioBackend> backend_;
    bool muted_ = false;
    double masterLinearGain_ = 1.0;
    double currentTimeMs_ = 0.0;
    ManagedSound::SoundId nextSoundId_ = 1;
    std::optional<ManagedSound::SoundId> activeVoiceId_;
    std::optional<AudioEnvironmentProfile> environmentProfile_;
    std::string environmentSignature_ = "none";
    std::unordered_map<ManagedSound::SoundId, std::shared_ptr<ManagedSound>> managedSounds_;
    std::vector<PendingEcho> pendingEchoes_;
    std::size_t loopsCreated_ = 0;
    std::size_t oneShotsPlayed_ = 0;
    std::size_t voicesPlayed_ = 0;
    std::size_t environmentChanges_ = 0;
    std::size_t droppedEchoes_ = 0;
};

} // namespace ri::audio
