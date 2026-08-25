
#include "infrastructure/runtime/threaded_application_runtime.h"

#include "infrastructure/runtime/network_runtime_worker.h"
#include "infrastructure/runtime/persistence_runtime_worker.h"
#include "infrastructure/runtime/playback_runtime_worker.h"
#include "infrastructure/runtime/recording_runtime_worker.h"

#include <QMetaObject>
#include <QStringList>
#include <QtGlobal>

#include <atomic>
#include <cmath>
#include <utility>

namespace {

QString shutdownRejection() {
    return QStringLiteral("The application runtime is shutting down");
}

template <typename Worker> bool shutdownAndRehome(Worker *worker, QThread *destination) {
    if (!worker)
        return true;
    std::atomic_bool moved = false;
    const bool invoked = QMetaObject::invokeMethod(
        worker,
        [worker, destination, &moved] {
            worker->shutdown();
            const bool rehomed =
                worker->moveToThread(destination) && worker->thread() == destination;
            moved.store(rehomed, std::memory_order_release);
        },
        Qt::BlockingQueuedConnection);
    return invoked && moved.load(std::memory_order_acquire);
}

} // namespace

ThreadedApplicationRuntime::ThreadedApplicationRuntime(QObject *parent)
    : ThreadedApplicationRuntime(RuntimeWorkerFactories{}, parent) {}

ThreadedApplicationRuntime::ThreadedApplicationRuntime(RuntimeWorkerFactories factories,
                                                       QObject *parent)
    : IApplicationRuntime(parent) {
    registerRuntimeMessageTypes();
    qRegisterMetaType<Plane>();
    qRegisterMetaType<Target>();
    qRegisterMetaType<DecodedState>();
    qRegisterMetaType<QBitArray>();
    qRegisterMetaType<QVector<CapturedPacket>>();
    qRegisterMetaType<SessionSnapshot>();
    qRegisterMetaType<IpAccessPolicy>();

    m_networkThread.setObjectName(QStringLiteral("lar-network-thread"));
    m_sessionThread.setObjectName(QStringLiteral("lar-session-thread"));
    m_persistenceThread.setObjectName(QStringLiteral("lar-persistence-thread"));

    QStringList constructionFailures;
    m_networkWorker =
        factories.createNetworkWorker ? factories.createNetworkWorker() : new NetworkRuntimeWorker;
    if (!m_networkWorker) {
        constructionFailures << QStringLiteral(
            "Network worker factory returned null; using the default worker");
        m_networkWorker = new NetworkRuntimeWorker;
    }
    m_recordingWorker = factories.createRecordingWorker ? factories.createRecordingWorker()
                                                        : new RecordingRuntimeWorker;
    if (!m_recordingWorker) {
        constructionFailures << QStringLiteral(
            "Recording worker factory returned null; using the default worker");
        m_recordingWorker = new RecordingRuntimeWorker;
    }
    m_playbackWorker = factories.createPlaybackWorker ? factories.createPlaybackWorker()
                                                      : new PlaybackRuntimeWorker;
    if (!m_playbackWorker) {
        constructionFailures << QStringLiteral(
            "Playback worker factory returned null; using the default worker");
        m_playbackWorker = new PlaybackRuntimeWorker;
    }
    m_persistenceWorker = factories.createPersistenceWorker ? factories.createPersistenceWorker()
                                                            : new PersistenceRuntimeWorker;
    if (!m_persistenceWorker) {
        constructionFailures << QStringLiteral(
            "Persistence worker factory returned null; using the default worker");
        m_persistenceWorker = new PersistenceRuntimeWorker;
    }

    const auto detachFactoryParent = [&constructionFailures](QObject *worker, const QString &name) {
        if (!worker->parent())
            return;
        worker->setParent(nullptr);
        constructionFailures << QStringLiteral("%1 worker factory returned a parented object; "
                                               "runtime ownership was made explicit")
                                    .arg(name);
    };
    detachFactoryParent(m_networkWorker, QStringLiteral("Network"));
    detachFactoryParent(m_recordingWorker, QStringLiteral("Recording"));
    detachFactoryParent(m_playbackWorker, QStringLiteral("Playback"));
    detachFactoryParent(m_persistenceWorker, QStringLiteral("Persistence"));

    m_networkWorker->moveToThread(&m_networkThread);
    m_recordingWorker->moveToThread(&m_sessionThread);
    m_playbackWorker->moveToThread(&m_sessionThread);
    m_persistenceWorker->moveToThread(&m_persistenceThread);
    connect(&m_networkThread, &QThread::started, m_networkWorker,
            &NetworkRuntimeWorker::initialize);
    connect(&m_sessionThread, &QThread::started, m_recordingWorker,
            &RecordingRuntimeWorker::initialize);
    connect(&m_sessionThread, &QThread::started, m_playbackWorker,
            &PlaybackRuntimeWorker::initialize);
    connect(this, &ThreadedApplicationRuntime::mappingLoadRequested, m_networkWorker,
            &NetworkRuntimeWorker::loadMapping, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::onlineStartRequested, m_networkWorker,
            &NetworkRuntimeWorker::startOnline, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::ipAccessPolicyChangeRequested, m_networkWorker,
            &NetworkRuntimeWorker::setIpAccessPolicy, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::onlineStopRequested, m_networkWorker,
            &NetworkRuntimeWorker::stopOnline, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::networkMetricsResetRequested, m_networkWorker,
            &NetworkRuntimeWorker::resetMetrics, Qt::QueuedConnection);

    connect(this, &ThreadedApplicationRuntime::recordingStartRequested, m_recordingWorker,
            &RecordingRuntimeWorker::startRecording, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::recordingPauseRequested, m_recordingWorker,
            &RecordingRuntimeWorker::pauseRecording, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::recordingResumeRequested, m_recordingWorker,
            &RecordingRuntimeWorker::resumeRecording, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::recordingStopRequested, m_recordingWorker,
            &RecordingRuntimeWorker::stopRecording, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::recordingResetRequested, m_recordingWorker,
            &RecordingRuntimeWorker::resetRecording, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::recordingSnapshotRequested, m_recordingWorker,
            &RecordingRuntimeWorker::snapshotRecording, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::recordingDiscardRequested, m_recordingWorker,
            &RecordingRuntimeWorker::discardRecording, Qt::QueuedConnection);

    connect(this, &ThreadedApplicationRuntime::sessionLoadRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::loadSession, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::sessionCloseRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::closeSession, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::playbackStartRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::play, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::playbackPauseRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::pause, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::playbackStopRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::stop, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::playbackSeekRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::seek, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::playbackRateChangeRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::setPlaybackRate, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::playbackRepeatChangeRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::setPlaybackRepeat, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::playbackMetricsResetRequested, m_playbackWorker,
            &PlaybackRuntimeWorker::resetMetrics, Qt::QueuedConnection);
    connect(this, &ThreadedApplicationRuntime::constructionFailureQueued, this,
            &ThreadedApplicationRuntime::publishConstructionFailure, Qt::QueuedConnection);

    connect(m_networkWorker, &NetworkRuntimeWorker::mappingLoadFinished, this,
            &IApplicationRuntime::mappingLoadFinished);
    connect(m_networkWorker, &NetworkRuntimeWorker::onlineStartFinished, this,
            [this](const OnlineStartResult &result) {
                emit onlineStartFinished(result);
                if (m_activeStateSource == RuntimeStateSource::Online &&
                    result.epoch.generation == m_activeStateGeneration && !result.started) {
                    m_activeStateSource = RuntimeStateSource::None;
                }
            });
    connect(m_networkWorker, &NetworkRuntimeWorker::onlineStopFinished, this,
            &IApplicationRuntime::onlineStopFinished);
    connect(m_networkWorker, &NetworkRuntimeWorker::onlineStateChanged, this,
            [this](const OnlineStateEvent &event) {
                emit onlineStateChanged(event);
                if (!event.listening && m_activeStateSource == RuntimeStateSource::Online &&
                    event.epoch.generation == m_activeStateGeneration) {
                    m_activeStateSource = RuntimeStateSource::None;
                }
            });
    connect(m_networkWorker, &NetworkRuntimeWorker::ipAccessPolicyChangeFinished, this,
            &IApplicationRuntime::ipAccessPolicyChangeFinished);
    connect(m_networkWorker, &NetworkRuntimeWorker::stateReady, this,
            &IApplicationRuntime::stateReady);
    connect(m_networkWorker, &NetworkRuntimeWorker::metricsChanged, this,
            &IApplicationRuntime::metricsChanged);
    connect(m_networkWorker, &NetworkRuntimeWorker::commandFinished, this,
            &IApplicationRuntime::commandFinished);

    connect(m_playbackWorker, &PlaybackRuntimeWorker::stateReady, this,
            &IApplicationRuntime::stateReady);
    connect(m_playbackWorker, &PlaybackRuntimeWorker::metricsChanged, this,
            &IApplicationRuntime::metricsChanged);
    connect(m_playbackWorker, &PlaybackRuntimeWorker::sessionLoadFinished, this,
            [this](const SessionLoadResult &result) {
                emit sessionLoadFinished(result);
                if (!result.loaded && m_activeStateSource == RuntimeStateSource::Playback &&
                    result.epoch.generation == m_activeStateGeneration) {
                    m_activeStateSource = RuntimeStateSource::None;
                }
            });
    connect(m_playbackWorker, &PlaybackRuntimeWorker::sessionClosed, this,
            [this](const SessionCloseResult &result) {
                emit sessionClosed(result);
                if (m_activeStateSource == RuntimeStateSource::Playback &&
                    result.epoch.generation == m_activeStateGeneration) {
                    m_activeStateSource = RuntimeStateSource::None;
                }
            });
    connect(m_playbackWorker, &PlaybackRuntimeWorker::playbackPositionChanged, this,
            &IApplicationRuntime::playbackPositionChanged);
    connect(m_playbackWorker, &PlaybackRuntimeWorker::playbackPlayingChanged, this,
            &IApplicationRuntime::playbackPlayingChanged);
    connect(m_playbackWorker, &PlaybackRuntimeWorker::playbackFinished, this,
            &IApplicationRuntime::playbackFinished);
    connect(m_playbackWorker, &PlaybackRuntimeWorker::commandFinished, this,
            &IApplicationRuntime::commandFinished);

    connect(m_recordingWorker, &RecordingRuntimeWorker::recordingStateChanged, this,
            &IApplicationRuntime::recordingStateChanged);
    connect(m_recordingWorker, &RecordingRuntimeWorker::recordingSaveFinished, this,
            &IApplicationRuntime::recordingSaveFinished);
    connect(m_recordingWorker, &RecordingRuntimeWorker::recordingResetFinished, this,
            &IApplicationRuntime::recordingResetFinished);
    connect(m_recordingWorker, &RecordingRuntimeWorker::commandFinished, this,
            &IApplicationRuntime::commandFinished);

    const auto forwardFailure = [this](const RuntimeFailure &failure) {
        emit runtimeError(failure);
    };
    connect(m_networkWorker, &NetworkRuntimeWorker::runtimeError, this, forwardFailure);
    connect(m_recordingWorker, &RecordingRuntimeWorker::runtimeError, this, forwardFailure);
    connect(m_playbackWorker, &PlaybackRuntimeWorker::runtimeError, this, forwardFailure);

    connect(m_networkWorker, &NetworkRuntimeWorker::recordingBatchReady, m_recordingWorker,
            &RecordingRuntimeWorker::appendRecordingBatch, Qt::QueuedConnection);
    connect(m_recordingWorker, &RecordingRuntimeWorker::recordingBatchHandled, m_networkWorker,
            &NetworkRuntimeWorker::acknowledgeRecordingBatch, Qt::QueuedConnection);
    connect(m_recordingWorker, &RecordingRuntimeWorker::recordingInputEnabled, m_networkWorker,
            &NetworkRuntimeWorker::setRecordingInputEnabled, Qt::QueuedConnection);
    connect(m_recordingWorker, &RecordingRuntimeWorker::recordingDrainRequested, m_networkWorker,
            &NetworkRuntimeWorker::drainRecordingInput, Qt::QueuedConnection);
    connect(m_networkWorker, &NetworkRuntimeWorker::recordingInputDrained, m_recordingWorker,
            &RecordingRuntimeWorker::recordingInputDrained, Qt::QueuedConnection);
    connect(m_networkWorker, &NetworkRuntimeWorker::recordingInputFailed, m_recordingWorker,
            &RecordingRuntimeWorker::recordingInputFailed, Qt::QueuedConnection);
    connect(m_recordingWorker, &RecordingRuntimeWorker::persistenceRequested, m_persistenceWorker,
            &PersistenceRuntimeWorker::save, Qt::QueuedConnection);
    connect(m_persistenceWorker, &PersistenceRuntimeWorker::saveFinished, m_recordingWorker,
            &RecordingRuntimeWorker::persistenceFinished, Qt::QueuedConnection);

    m_networkThread.start();
    m_sessionThread.start();
    m_persistenceThread.start();
    for (const QString &message : constructionFailures) {
        emit constructionFailureQueued(message);
    }
}

ThreadedApplicationRuntime::~ThreadedApplicationRuntime() {
    shutdown();
}

int ThreadedApplicationRuntime::runningWorkerThreadCount() const noexcept {
    return int(m_networkThread.isRunning()) + int(m_sessionThread.isRunning()) +
           int(m_persistenceThread.isRunning());
}

CommandDispatch ThreadedApplicationRuntime::loadMapping(const QString &path) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (path.isEmpty())
        return rejectCommand(QStringLiteral("Mapping path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    emit mappingLoadRequested(path, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::startOnline(quint16 port) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (port == 0)
        return rejectCommand(QStringLiteral("UDP port must be non-zero"));
    const CommandDispatch dispatch = acceptCommand();
    const quint64 generation = ++m_nextStateGeneration;
    emit onlineStartRequested(port, generation, dispatch.request);
    m_activeStateSource = RuntimeStateSource::Online;
    m_activeStateGeneration = generation;
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::setIpAccessPolicy(const IpAccessPolicy &policy) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit ipAccessPolicyChangeRequested(policy, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::stopOnline() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const quint64 generation = m_activeStateGeneration;
    emit onlineStopRequested(generation, dispatch.request);
    if (m_activeStateSource == RuntimeStateSource::Online) {
        m_activeStateSource = RuntimeStateSource::None;
    }
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::startRecording(const QByteArray &mappingJson) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (mappingJson.isEmpty()) {
        return rejectCommand(QStringLiteral("Recording mapping is empty"));
    }
    const CommandDispatch dispatch = acceptCommand();
    emit recordingStartRequested(mappingJson, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::pauseRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit recordingPauseRequested(dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::resumeRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit recordingResumeRequested(dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::stopRecording(const QString &targetPath) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (targetPath.isEmpty())
        return rejectCommand(QStringLiteral("Save path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    emit recordingStopRequested(targetPath, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::resetRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit recordingResetRequested(dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::snapshotRecording(const QString &targetPath) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (targetPath.isEmpty())
        return rejectCommand(QStringLiteral("Save path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    emit recordingSnapshotRequested(targetPath, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::discardRecording() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit recordingDiscardRequested(dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::loadSession(const QString &path) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (path.isEmpty())
        return rejectCommand(QStringLiteral("Session path is empty"));
    const CommandDispatch dispatch = acceptCommand();
    const quint64 generation = ++m_nextStateGeneration;
    emit sessionLoadRequested(path, generation, dispatch.request);
    m_activeStateSource = RuntimeStateSource::Playback;
    m_activeStateGeneration = generation;
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::closeSession() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    const quint64 generation = m_activeStateGeneration;
    emit sessionCloseRequested(generation, dispatch.request);
    if (m_activeStateSource == RuntimeStateSource::Playback) {
        m_activeStateSource = RuntimeStateSource::None;
    }
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::play() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit playbackStartRequested(dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::pause() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit playbackPauseRequested(dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::stop() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit playbackStopRequested(dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::seek(SessionTimestamp position) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit playbackSeekRequested(position, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::setPlaybackRate(double rate) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    if (rate <= 0.0 || !std::isfinite(rate)) {
        return rejectCommand(QStringLiteral("Playback rate must be a positive finite number"));
    }
    const CommandDispatch dispatch = acceptCommand();
    emit playbackRateChangeRequested(rate, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::setPlaybackRepeat(bool enabled) {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    emit playbackRepeatChangeRequested(enabled, dispatch.request);
    return dispatch;
}

CommandDispatch ThreadedApplicationRuntime::resetMetrics() {
    if (m_shuttingDown)
        return rejectCommand(shutdownRejection());
    const CommandDispatch dispatch = acceptCommand();
    if (m_activeStateSource == RuntimeStateSource::Online) {
        emit networkMetricsResetRequested(dispatch.request);
    } else if (m_activeStateSource == RuntimeStateSource::Playback) {
        emit playbackMetricsResetRequested(dispatch.request);
    } else {
        emit commandFinished({dispatch.request, RuntimeCommandKind::MetricsReset, true, {}});
    }
    return dispatch;
}

void ThreadedApplicationRuntime::publishConstructionFailure(const QString &message) {
    if (!m_shuttingDown) {
        emit runtimeError({{}, std::nullopt, RuntimeFailureCode::Construction, message});
    }
}

void ThreadedApplicationRuntime::shutdown() {
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    m_activeStateSource = RuntimeStateSource::None;
    ++m_nextStateGeneration;
    QThread *const ownerThread = thread();
    bool workersRehomed = true;
    if (m_networkThread.isRunning())
        workersRehomed &= shutdownAndRehome(m_networkWorker, ownerThread);
    if (m_sessionThread.isRunning()) {
        workersRehomed &= shutdownAndRehome(m_recordingWorker, ownerThread);
        workersRehomed &= shutdownAndRehome(m_playbackWorker, ownerThread);
    }
    if (m_persistenceThread.isRunning())
        workersRehomed &= shutdownAndRehome(m_persistenceWorker, ownerThread);

    if (!workersRehomed) {
        qFatal("A runtime worker could not be rehomed before thread shutdown");
    }
    m_networkThread.quit();
    m_sessionThread.quit();
    m_persistenceThread.quit();
    m_networkThread.wait();
    m_sessionThread.wait();
    m_persistenceThread.wait();
    delete std::exchange(m_networkWorker, nullptr);
    delete std::exchange(m_recordingWorker, nullptr);
    delete std::exchange(m_playbackWorker, nullptr);
    delete std::exchange(m_persistenceWorker, nullptr);
}
