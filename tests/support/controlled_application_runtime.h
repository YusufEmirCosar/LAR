#pragma once

#include "application/ports/application_runtime.h"

#include <QVector>

/** Deterministic no-thread runtime whose completions are released by tests. */
class ControlledApplicationRuntime final : public IApplicationRuntime {
  public:
    struct StartCall final {
        RuntimeRequestId request;
        RuntimeSourceEpoch epoch;
        quint16 port = 0;
    };
    struct StopCall final {
        RuntimeRequestId request;
        RuntimeSourceEpoch epoch;
    };
    struct LoadCall final {
        RuntimeRequestId request;
        RuntimeSourceEpoch epoch;
        QString path;
    };
    struct CloseCall final {
        RuntimeRequestId request;
        RuntimeSourceEpoch epoch;
    };

    /**
     * @brief Obtains load Mapping.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] path Path of the input or output file.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch loadMapping(const QString &path) override {
        Q_UNUSED(path);
        return dispatch();
    }

    /**
     * @brief Starts start Online.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] port UDP port used by the operation.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch startOnline(quint16 port) override {
        const CommandDispatch accepted = dispatch();
        if (!accepted)
            return accepted;
        m_onlineEpoch = {RuntimeStateSource::Online, ++m_generation};
        starts.append({accepted.request, m_onlineEpoch, port});
        return accepted;
    }

    /**
     * @brief Updates set Ip Access Policy.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] policy Policy supplied to the operation.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch setIpAccessPolicy(const IpAccessPolicy &policy) override {
        Q_UNUSED(policy);
        return dispatch();
    }

    /**
     * @brief Ends or resets stop Online.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch stopOnline() override {
        const CommandDispatch accepted = dispatch();
        if (accepted)
            stops.append({accepted.request, m_onlineEpoch});
        return accepted;
    }

    /**
     * @brief Starts start Recording.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] mappingJson Mapping Json supplied to the operation.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch startRecording(const QByteArray &mappingJson) override {
        Q_UNUSED(mappingJson);
        return dispatch();
    }
    /**
     * @brief Performs the pause Recording operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch pauseRecording() override {
        return dispatch();
    }
    /**
     * @brief Performs the resume Recording operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch resumeRecording() override {
        return dispatch();
    }
    /**
     * @brief Ends or resets stop Recording.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] targetPath Path of the input or output file.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch stopRecording(const QString &targetPath) override {
        Q_UNUSED(targetPath);
        return dispatch();
    }
    /**
     * @brief Ends or resets reset Recording.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch resetRecording() override {
        return dispatch();
    }
    /**
     * @brief Performs the snapshot Recording operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] targetPath Path of the input or output file.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch snapshotRecording(const QString &targetPath) override {
        Q_UNUSED(targetPath);
        return dispatch();
    }
    /**
     * @brief Performs the discard Recording operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch discardRecording() override {
        return dispatch();
    }

    /**
     * @brief Obtains load Session.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] path Path of the input or output file.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch loadSession(const QString &path) override {
        const CommandDispatch accepted = dispatch();
        if (!accepted)
            return accepted;
        m_playbackEpoch = {RuntimeStateSource::Playback, ++m_generation};
        loads.append({accepted.request, m_playbackEpoch, path});
        return accepted;
    }

    /**
     * @brief Ends or resets close Session.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch closeSession() override {
        const CommandDispatch accepted = dispatch();
        if (accepted)
            closes.append({accepted.request, m_playbackEpoch});
        return accepted;
    }

    /**
     * @brief Performs the play operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch play() override {
        return dispatch();
    }
    /**
     * @brief Performs the pause operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch pause() override {
        return dispatch();
    }
    /**
     * @brief Ends or resets stop.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch stop() override {
        return dispatch();
    }
    /**
     * @brief Performs the seek operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] position Position supplied to the operation.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch seek(SessionTimestamp position) override {
        Q_UNUSED(position);
        return dispatch();
    }
    /**
     * @brief Updates set Playback Rate.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] rate Finite numeric value used by the operation.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch setPlaybackRate(double rate) override {
        Q_UNUSED(rate);
        return dispatch();
    }
    /**
     * @brief Updates set Playback Repeat.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] enabled Whether the corresponding feature is enabled.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch setPlaybackRepeat(bool enabled) override {
        Q_UNUSED(enabled);
        return dispatch();
    }
    /**
     * @brief Ends or resets reset Metrics.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch resetMetrics() override {
        return dispatch();
    }

    /**
     * @brief Ends or resets shutdown.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     */
    void shutdown() override {
        m_shuttingDown = true;
    }

    /**
     * @brief Performs the complete Start operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] index Index supplied to the operation.
     * @param[in] started Started supplied to the operation.
     * @param[in] error Optional destination for a human-readable diagnostic.
     */
    void completeStart(int index, bool started, const QString &error = {}) {
        const StartCall call = starts.at(index);
        emit onlineStartFinished({call.request, call.epoch, started, error});
        emit onlineStateChanged({call.epoch, started, started ? call.port : quint16{0}});
    }

    /**
     * @brief Performs the complete Stop operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] index Index supplied to the operation.
     * @param[in] stopped Stopped supplied to the operation.
     * @param[in] error Optional destination for a human-readable diagnostic.
     */
    void completeStop(int index, bool stopped, const QString &error = {}) {
        const StopCall call = stops.at(index);
        emit onlineStateChanged({call.epoch, false, 0});
        emit onlineStopFinished({call.request, call.epoch, stopped, error});
    }

    /**
     * @brief Performs the complete Load operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] index Index supplied to the operation.
     * @param[in] loaded Loaded supplied to the operation.
     * @param[in] recordCount Record Count supplied to the operation.
     * @param[in] duration Duration supplied to the operation.
     * @param[in] error Optional destination for a human-readable diagnostic.
     */
    void completeLoad(int index, bool loaded, qint64 recordCount = 0,
                      SessionTimestamp duration = {}, const QString &error = {}) {
        const LoadCall call = loads.at(index);
        emit sessionLoadFinished({call.request, call.epoch, loaded, call.path,
                                  loaded ? recordCount : 0, loaded ? duration : SessionTimestamp{},
                                  error});
    }

    /**
     * @brief Performs the complete Close operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] index Index supplied to the operation.
     * @param[in] closed Closed supplied to the operation.
     * @param[in] error Optional destination for a human-readable diagnostic.
     */
    void completeClose(int index, bool closed, const QString &error = {}) {
        const CloseCall call = closes.at(index);
        emit sessionClosed({call.request, call.epoch, closed, error});
    }

    /**
     * @brief Performs the publish Metrics operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] epoch Epoch supplied to the operation.
     * @param[in] count Count supplied to the operation.
     * @param[in] rate Finite numeric value used by the operation.
     */
    void publishMetrics(RuntimeSourceEpoch epoch, quint64 count, quint64 rate = 0) {
        emit metricsChanged({epoch, count, rate});
    }

    /**
     * @brief Performs the publish Position operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] epoch Epoch supplied to the operation.
     * @param[in] position Position supplied to the operation.
     */
    void publishPosition(RuntimeSourceEpoch epoch, SessionTimestamp position) {
        emit playbackPositionChanged({epoch, position});
    }

    QVector<StartCall> starts;
    QVector<StopCall> stops;
    QVector<LoadCall> loads;
    QVector<CloseCall> closes;

  private:
    /**
     * @brief Performs the dispatch operation.
     *
     * @details The operation exposes the stable behavior of the owning type to its callers.
     *
     * @return The value produced by the operation.
     */
    CommandDispatch dispatch() {
        return m_shuttingDown ? rejectCommand(QStringLiteral("runtime is shut down"))
                              : acceptCommand();
    }

    RuntimeSourceEpoch m_onlineEpoch;
    RuntimeSourceEpoch m_playbackEpoch;
    quint64 m_generation = 0;
    bool m_shuttingDown = false;
};
