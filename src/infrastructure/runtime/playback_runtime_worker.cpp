
#include "infrastructure/runtime/playback_runtime_worker.h"

#include "application/playback_service.h"
#include "infrastructure/runtime/playback_metrics.h"
#include "infrastructure/session/lar_session_reader.h"
#include "infrastructure/timing/qt_playback_clock.h"

#include <utility>

namespace {

class InertPlaybackClock final : public IPlaybackClock {
  public:
    explicit InertPlaybackClock(QObject *parent) : IPlaybackClock(parent) {}
    void start(int) override {
        m_active = true;
    }
    void stop() override {
        m_active = false;
    }
    bool isActive() const override {
        return m_active;
    }

  private:
    bool m_active = false;
};

} // namespace

PlaybackRuntimeWorker::PlaybackRuntimeWorker(std::unique_ptr<ISessionReader> reader,
                                             PlaybackClockFactory clockFactory, QObject *parent)
    : QObject(parent), m_reader(reader ? std::move(reader) : std::make_unique<LarSessionReader>()),
      m_clockFactory(clockFactory ? std::move(clockFactory)
                                  : [](QObject *clockParent) -> IPlaybackClock * {
          return new QtPlaybackClock(clockParent);
      }) {}

PlaybackRuntimeWorker::~PlaybackRuntimeWorker() = default;

void PlaybackRuntimeWorker::initialize() {
    if (m_playback)
        return;
    m_clock = m_clockFactory(this);
    if (!m_clock) {
        m_clock = new InertPlaybackClock(this);
        emit runtimeError({{},
                           std::nullopt,
                           RuntimeFailureCode::Construction,
                           QStringLiteral("Playback clock factory returned null")});
    }
    m_playback = new PlaybackService(*m_reader, *m_clock, this);
    m_metrics = new PlaybackMetrics(this);

    // PlaybackService already samples at the presentation rate. Forwarding the
    // sampled frame on that same tick avoids beating a separate 16 ms timer
    // against the 60 Hz clock and silently coalescing otherwise valid frames.
    connect(m_playback, &PlaybackService::frameReady, this, [this](const DecodedState &state) {
        if (m_publicationDeferred) {
            m_deferredState = state;
            return;
        }
        emit stateReady({{RuntimeStateSource::Playback, m_stateGeneration}, state});
    });
    connect(m_playback, &PlaybackService::positionChanged, this, [this](SessionTimestamp position) {
        if (m_publicationDeferred) {
            m_deferredPosition = position;
            return;
        }
        emit playbackPositionChanged({{RuntimeStateSource::Playback, m_stateGeneration}, position});
    });
    connect(m_playback, &PlaybackService::recordsProcessed, m_metrics, &PlaybackMetrics::record);
    connect(m_playback, &PlaybackService::playingChanged, this, [this](bool playing) {
        m_metrics->setActive(playing);
        emit playbackPlayingChanged({{RuntimeStateSource::Playback, m_stateGeneration}, playing});
    });
    connect(m_playback, &PlaybackService::playbackFinished, this, [this] {
        m_metrics->finish();
        emit playbackFinished({{RuntimeStateSource::Playback, m_stateGeneration}});
    });
    connect(m_playback, &PlaybackService::playbackError, this, [this](const QString &message) {
        emit runtimeError({{},
                           RuntimeSourceEpoch{RuntimeStateSource::Playback, m_stateGeneration},
                           RuntimeFailureCode::Operational,
                           message});
    });
    connect(m_metrics, &PlaybackMetrics::metricsChanged, this, [this](quint64 count, quint64 rate) {
        emit metricsChanged({{RuntimeStateSource::Playback, m_stateGeneration}, count, rate});
    });
}

void PlaybackRuntimeWorker::loadSession(const QString &path, quint64 generation,
                                        RuntimeRequestId request) {
    initialize();
    m_stateGeneration = generation != 0 ? generation : m_stateGeneration + 1;
    m_publicationDeferred = true;
    clearDeferredPublication();
    m_playback->closeSession();
    clearDeferredPublication();
    m_metrics->reset();

    QString error;
    const bool loaded = m_playback->loadSession(path, &error);
    emit sessionLoadFinished({request,
                              {RuntimeStateSource::Playback, m_stateGeneration},
                              loaded,
                              path,
                              loaded ? m_playback->recordCount() : 0,
                              loaded ? m_playback->duration() : SessionTimestamp{},
                              error});

    m_publicationDeferred = false;
    if (loaded)
        flushDeferredPublication();
    else
        clearDeferredPublication();
}

void PlaybackRuntimeWorker::closeSession(quint64 generation, RuntimeRequestId request) {
    initialize();
    const RuntimeSourceEpoch epoch{RuntimeStateSource::Playback,
                                   generation != 0 ? generation : m_stateGeneration};
    m_publicationDeferred = true;
    m_playback->closeSession();
    m_metrics->setActive(false);
    clearDeferredPublication();
    m_publicationDeferred = false;
    emit sessionClosed({request, epoch, true, {}});
}

void PlaybackRuntimeWorker::play(RuntimeRequestId request) {
    initialize();
    const bool eligible = m_playback->isLoaded() && m_playback->recordCount() > 0;
    if (eligible)
        m_playback->play();
    const bool succeeded = eligible && m_playback->isPlaying();
    emit commandFinished(
        {request, RuntimeCommandKind::PlaybackPlay, succeeded,
         succeeded ? QString{} : QStringLiteral("No loaded playback session can be played")});
}

void PlaybackRuntimeWorker::pause(RuntimeRequestId request) {
    initialize();
    const bool succeeded = m_playback->isLoaded();
    if (succeeded)
        m_playback->pause();
    emit commandFinished({request, RuntimeCommandKind::PlaybackPause, succeeded,
                          succeeded ? QString{} : QStringLiteral("No playback session is loaded")});
}

void PlaybackRuntimeWorker::stop(RuntimeRequestId request) {
    initialize();
    const bool succeeded = m_playback->isLoaded();
    if (succeeded)
        m_playback->stop();
    emit commandFinished({request, RuntimeCommandKind::PlaybackStop, succeeded,
                          succeeded ? QString{} : QStringLiteral("No playback session is loaded")});
}

void PlaybackRuntimeWorker::seek(SessionTimestamp position, RuntimeRequestId request) {
    initialize();
    const bool eligible = m_playback->isLoaded() && m_playback->recordCount() > 0;
    if (eligible)
        m_playback->seek(position);
    const bool succeeded = eligible && m_playback->isLoaded();
    emit commandFinished(
        {request, RuntimeCommandKind::PlaybackSeek, succeeded,
         succeeded ? QString{} : QStringLiteral("No loaded playback record can be sought")});
}

void PlaybackRuntimeWorker::setPlaybackRate(double rate, RuntimeRequestId request) {
    initialize();
    const bool succeeded = m_playback->setRate(rate);
    emit commandFinished(
        {request, RuntimeCommandKind::PlaybackRateChange, succeeded,
         succeeded ? QString{} : QStringLiteral("Playback rate must be a positive finite number")});
}

void PlaybackRuntimeWorker::setPlaybackRepeat(bool enabled, RuntimeRequestId request) {
    initialize();
    m_playback->setRepeat(enabled);
    emit commandFinished({request, RuntimeCommandKind::PlaybackRepeatChange, true, {}});
}

void PlaybackRuntimeWorker::resetMetrics(RuntimeRequestId request) {
    initialize();
    m_metrics->reset();
    emit commandFinished({request, RuntimeCommandKind::MetricsReset, true, {}});
}

void PlaybackRuntimeWorker::shutdown() {
    if (!m_playback)
        return;
    m_metrics->shutdown();
    m_publicationDeferred = true;
    m_playback->closeSession();
    clearDeferredPublication();
    m_publicationDeferred = false;
}

void PlaybackRuntimeWorker::clearDeferredPublication() noexcept {
    m_deferredState.reset();
    m_deferredPosition.reset();
}

void PlaybackRuntimeWorker::flushDeferredPublication() {
    const auto state = std::exchange(m_deferredState, std::nullopt);
    const auto position = std::exchange(m_deferredPosition, std::nullopt);
    if (state)
        emit stateReady({{RuntimeStateSource::Playback, m_stateGeneration}, *state});
    if (position) {
        emit playbackPositionChanged(
            {{RuntimeStateSource::Playback, m_stateGeneration}, *position});
    }
}
