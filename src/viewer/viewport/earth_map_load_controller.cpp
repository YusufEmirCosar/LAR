
#include "viewer/viewport/earth_map_load_controller.h"

#include <QtConcurrentRun>

#include <utility>

EarthMapLoadController::EarthMapLoadController(
    std::shared_ptr<const lar::map::IMapAssetSource> source, QObject *parent)
    : QObject(parent), m_source(std::move(source)) {}

EarthMapLoadController::~EarthMapLoadController() {
    invalidate();
}

bool EarthMapLoadController::ensureLoaded() {
    if (m_state == State::Loaded || m_state == State::Loading ||
        m_state == State::AwaitingInstallation) {
        return true;
    }
    if (m_state == State::Failed)
        return false;
    if (!m_source) {
        m_state = State::Failed;
        m_failureMessage = QStringLiteral("The packaged map source is unavailable.");
        return false;
    }

    m_state = State::Loading;
    m_failureMessage.clear();
    const quint64 revision = ++m_revision;
    const auto source = m_source;
    auto *watcher = new QFutureWatcher<lar::map::MapAssetReadResult>(this);
    m_watcher = watcher;
    connect(watcher, &QFutureWatcher<lar::map::MapAssetReadResult>::finished, this,
            [this, watcher, revision] {
                const lar::map::MapAssetReadResult result = watcher->result();
                if (m_watcher == watcher)
                    m_watcher = nullptr;
                watcher->deleteLater();
                finish(revision, result);
            });
    emit loadStarted();
    watcher->setFuture(QtConcurrent::run([source] {
        lar::map::MapAssetReadResult result;
        try {
            result = source->load();
        } catch (...) {
            result = {nullptr, lar::map::MapAssetError::Io,
                      QStringLiteral("The packaged map source failed unexpectedly.")};
        }
        return result;
    }));
    return true;
}

void EarthMapLoadController::finish(quint64 revision, lar::map::MapAssetReadResult result) {
    if (revision != m_revision || m_state != State::Loading)
        return;
    if (!result.succeeded()) {
        m_state = State::Failed;
        m_failureMessage = result.message;
    } else {
        m_state = State::AwaitingInstallation;
    }
    emit assetReady(revision, result);
}

void EarthMapLoadController::completeInstallation(quint64 revision, bool installed,
                                                  const QString &error) {
    if (revision != m_revision || m_state != State::AwaitingInstallation) {
        return;
    }
    m_state = installed ? State::Loaded : State::Failed;
    if (!installed)
        m_failureMessage = error;
}

void EarthMapLoadController::markFailed(const QString &error) {
    ++m_revision;
    m_state = State::Failed;
    m_failureMessage = error;
}

void EarthMapLoadController::invalidate() noexcept {
    ++m_revision;
    if (m_watcher) {
        disconnect(m_watcher, nullptr, this, nullptr);
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    if (m_state == State::Loading || m_state == State::AwaitingInstallation) {
        m_state = State::Idle;
    }
}
