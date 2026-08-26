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

    CommandDispatch loadMapping(const QString &path) override {
        Q_UNUSED(path);
        return dispatch();
    }

    CommandDispatch startOnline(quint16 port) override {
        const CommandDispatch accepted = dispatch();
        if (!accepted)
            return accepted;
        m_onlineEpoch = {RuntimeStateSource::Online, ++m_generation};
        starts.append({accepted.request, m_onlineEpoch, port});
        return accepted;
    }

    CommandDispatch setIpAccessPolicy(const IpAccessPolicy &policy) override {
        Q_UNUSED(policy);
        return dispatch();
    }

    CommandDispatch stopOnline() override {
        const CommandDispatch accepted = dispatch();
        if (accepted)
            stops.append({accepted.request, m_onlineEpoch});
        return accepted;
    }

    CommandDispatch startRecording(const QByteArray &mappingJson) override {
        Q_UNUSED(mappingJson);
        return dispatch();
    }
    CommandDispatch pauseRecording() override {
        return dispatch();
    }
    CommandDispatch resumeRecording() override {
        return dispatch();
    }
    CommandDispatch stopRecording(const QString &targetPath) override {
        Q_UNUSED(targetPath);
        return dispatch();
    }
    CommandDispatch resetRecording() override {
        return dispatch();
    }
    CommandDispatch snapshotRecording(const QString &targetPath) override {
        Q_UNUSED(targetPath);
        return dispatch();
    }
    CommandDispatch discardRecording() override {
        return dispatch();
    }

    CommandDispatch loadSession(const QString &path) override {
        const CommandDispatch accepted = dispatch();
        if (!accepted)
            return accepted;
        m_playbackEpoch = {RuntimeStateSource::Playback, ++m_generation};
        loads.append({accepted.request, m_playbackEpoch, path});
        return accepted;
    }

    CommandDispatch closeSession() override {
        const CommandDispatch accepted = dispatch();
        if (accepted)
            closes.append({accepted.request, m_playbackEpoch});
        return accepted;
    }

    CommandDispatch play() override {
        return dispatch();
    }
    CommandDispatch pause() override {
        return dispatch();
    }
    CommandDispatch stop() override {
        return dispatch();
    }
    CommandDispatch seek(SessionTimestamp position) override {
        Q_UNUSED(position);
        return dispatch();
    }
    CommandDispatch setPlaybackRate(double rate) override {
        Q_UNUSED(rate);
        return dispatch();
    }
    CommandDispatch setPlaybackRepeat(bool enabled) override {
        Q_UNUSED(enabled);
        return dispatch();
    }
    CommandDispatch resetMetrics() override {
        return dispatch();
    }

    void shutdown() override {
        m_shuttingDown = true;
    }

    void completeStart(int index, bool started, const QString &error = {}) {
        const StartCall call = starts.at(index);
        emit onlineStartFinished({call.request, call.epoch, started, error});
        emit onlineStateChanged({call.epoch, started, started ? call.port : quint16{0}});
    }

    void completeStop(int index, bool stopped, const QString &error = {}) {
        const StopCall call = stops.at(index);
        emit onlineStateChanged({call.epoch, false, 0});
        emit onlineStopFinished({call.request, call.epoch, stopped, error});
    }

    void completeLoad(int index, bool loaded, qint64 recordCount = 0,
                      SessionTimestamp duration = {}, const QString &error = {}) {
        const LoadCall call = loads.at(index);
        emit sessionLoadFinished({call.request, call.epoch, loaded, call.path,
                                  loaded ? recordCount : 0, loaded ? duration : SessionTimestamp{},
                                  error});
    }

    void completeClose(int index, bool closed, const QString &error = {}) {
        const CloseCall call = closes.at(index);
        emit sessionClosed({call.request, call.epoch, closed, error});
    }

    void publishMetrics(RuntimeSourceEpoch epoch, quint64 count, quint64 rate = 0) {
        emit metricsChanged({epoch, count, rate});
    }

    void publishPosition(RuntimeSourceEpoch epoch, SessionTimestamp position) {
        emit playbackPositionChanged({epoch, position});
    }

    QVector<StartCall> starts;
    QVector<StopCall> stops;
    QVector<LoadCall> loads;
    QVector<CloseCall> closes;

  private:
    CommandDispatch dispatch() {
        return m_shuttingDown ? rejectCommand(QStringLiteral("runtime is shut down"))
                              : acceptCommand();
    }

    RuntimeSourceEpoch m_onlineEpoch;
    RuntimeSourceEpoch m_playbackEpoch;
    quint64 m_generation = 0;
    bool m_shuttingDown = false;
};
