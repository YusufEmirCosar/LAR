#pragma once

/**
 * @file earth_map_load_controller.h
 * @brief Revision-safe asynchronous lifecycle for packaged map loading.
 */

#include "viewer/map/map_asset_source.h"

#include <QFutureWatcher>
#include <QObject>

#include <memory>

/** Single-flight, revision-safe asynchronous map asset loader. */
class EarthMapLoadController final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Idle,
        Loading,
        AwaitingInstallation,
        Loaded,
        Failed,
    };

    explicit EarthMapLoadController(std::shared_ptr<const lar::map::IMapAssetSource> source,
                                    QObject *parent = nullptr);
    ~EarthMapLoadController() override;

    bool ensureLoaded();
    void completeInstallation(quint64 revision, bool installed, const QString &error = {});
    void markFailed(const QString &error);
    void invalidate() noexcept;

    State state() const noexcept {
        return m_state;
    }
    bool isLoaded() const noexcept {
        return m_state == State::Loaded;
    }
    QString failureMessage() const {
        return m_failureMessage;
    }

  signals:
    /**
     * @brief Processes load started.
     */
    void loadStarted();
    void assetReady(quint64 revision, const lar::map::MapAssetReadResult &result);

  private:
    void finish(quint64 revision, lar::map::MapAssetReadResult result);

    std::shared_ptr<const lar::map::IMapAssetSource> m_source;
    QFutureWatcher<lar::map::MapAssetReadResult> *m_watcher = nullptr;
    quint64 m_revision = 0;
    State m_state = State::Idle;
    QString m_failureMessage;
};
