#pragma once

#include "application/application_facade.h"
#include "application/direct_application_runtime.h"
#include "application/metrics_service.h"
#include "application/mode_coordinator.h"
#include "application/online_capture_service.h"
#include "application/playback_service.h"
#include "application/recording_pipeline_coordinator.h"
#include "application/recording_service.h"
#include "application/session_limits.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"
#include "infrastructure/network/qt_ip_access_policy_repository.h"
#include "infrastructure/network/qt_udp_datagram_source.h"
#include "infrastructure/runtime/network_runtime_worker.h"
#include "infrastructure/runtime/threaded_application_runtime.h"
#include "infrastructure/session/file_session_snapshot.h"
#include "infrastructure/session/lar_session_reader.h"
#include "infrastructure/session/lar_session_writer.h"
#include "infrastructure/session/qt_session_persistence.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QtTest>

#include <limits>
#include <utility>

namespace {

[[maybe_unused]] PacketMapping timeOnlyMapping() {
    return PacketMapping({{StateField::Time, 0, 8}},
                         QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])"));
}

class FakeDatagramSource final : public IDatagramSource {
  public:
    bool start(quint16 port, QString *error) override {
        Q_UNUSED(error);
        listening = true;
        boundPort = port;
        return true;
    }

    void stop() override {
        listening = false;
    }
    bool isListening() const override {
        return listening;
    }
    quint16 localPort() const override {
        return boundPort;
    }

    bool setIpAccessPolicy(const IpAccessPolicy &policy) override {
        if (!acceptPolicyChanges)
            return false;
        ipPolicy = policy;
        return true;
    }

    void deliver(const QByteArray &data, qint64 receivedAtNanoseconds = 0) {
        emit datagramReceived(data, receivedAtNanoseconds);
    }

    bool listening = false;
    quint16 boundPort = 0;
    IpAccessPolicy ipPolicy;
    bool acceptPolicyChanges = true;
};

class FakePacketDecoder final : public IPacketDecoder {
  public:
    void setMapping(PacketMapping mapping) override {
        currentMapping = std::move(mapping);
    }

    bool decode(const QByteArray &packet, DecodedState *state, QString *error) const override {
        if (packet != QByteArrayLiteral("accepted")) {
            if (error)
                *error = QStringLiteral("rejected test packet");
            return false;
        }
        *state = {};
        state->target.time = 42.0;
        state->availableFields = QBitArray(StateField::Count);
        state->availableFields.setBit(StateField::Time);
        return true;
    }

    PacketMapping currentMapping;
};

class FakeSessionSnapshot final : public ISessionSnapshot {
  public:
    explicit FakeSessionSnapshot(QByteArray bytes = {}) : payload(std::move(bytes)) {}

    bool writeTo(QIODevice &destination, QString *error) const override {
        if (destination.write(payload) == payload.size())
            return true;
        if (error)
            *error = destination.errorString();
        return false;
    }

    QByteArray payload;
};

class FakeRecordingTransaction final : public IRecordingTransaction {
  public:
    bool begin(const QByteArray &mappingJson, QString *error) override {
        Q_UNUSED(error);
        active = true;
        mapping = mappingJson;
        timestamps.clear();
        packets.clear();
        return true;
    }

    bool append(SessionTimestamp timestamp, const QByteArray &packet, QString *error) override {
        Q_UNUSED(error);
        if (!active)
            return false;
        timestamps.append(timestamp);
        packets.append(packet);
        return true;
    }

    bool reset(QString *error) override {
        Q_UNUSED(error);
        if (!active)
            return false;
        timestamps.clear();
        packets.clear();
        return true;
    }

    bool createSnapshot(SessionSnapshot *snapshot, QString *error) override {
        if (!active || !snapshot) {
            if (error)
                *error = QStringLiteral("no active transaction");
            return false;
        }
        *snapshot = std::make_shared<FakeSessionSnapshot>();
        return true;
    }

    void cancel() noexcept override {
        active = false;
        timestamps.clear();
        packets.clear();
    }

    bool isActive() const noexcept override {
        return active;
    }
    quint64 recordCount() const noexcept override {
        return static_cast<quint64>(packets.size());
    }

    bool active = false;
    QByteArray mapping;
    QVector<SessionTimestamp> timestamps;
    QVector<QByteArray> packets;
};

class FakeSessionPersistence final : public ISessionPersistence {
  public:
    bool save(const SessionSnapshot &snapshot, const QString &targetPath, QString *error) override {
        Q_UNUSED(targetPath);
        if (!snapshot) {
            if (error)
                *error = QStringLiteral("snapshot is empty");
            return false;
        }
        if (fail) {
            if (error)
                *error = QStringLiteral("simulated final-save failure");
            return false;
        }
        return true;
    }

    bool fail = false;
};

class FakeRecordingClock final : public IRecordingClock {
  public:
    qint64 nowNanoseconds() const override {
        return now;
    }

    qint64 now = 0;
};

class FakeSessionReader final : public ISessionReader {
  public:
    bool loadFile(const QString &path, QString *error) override {
        Q_UNUSED(path);
        Q_UNUSED(error);
        valid = true;
        return true;
    }

    bool loadData(const QByteArray &data, QString *error) override {
        Q_UNUSED(data);
        Q_UNUSED(error);
        valid = true;
        return true;
    }

    void close() noexcept override {
        valid = false;
    }
    bool isValid() const noexcept override {
        return valid;
    }
    qint64 recordCount() const noexcept override {
        return items.size();
    }
    SessionTimestamp duration() const noexcept override {
        return items.isEmpty() ? SessionTimestamp{} : items.constLast().timestamp;
    }
    bool timestampAt(qint64 index, SessionTimestamp *timestamp, QString *error) const override {
        if (!valid || !timestamp || index < 0 || index >= items.size()) {
            if (error)
                *error = QStringLiteral("invalid timestamp request");
            return false;
        }
        ++timestampAtCalls;
        *timestamp = items.at(static_cast<qsizetype>(index)).timestamp;
        return true;
    }
    bool recordAt(qint64 index, SessionStateItem *item, QString *error) const override {
        if (!valid || !item || index < 0 || index >= items.size()) {
            if (error)
                *error = QStringLiteral("invalid record request");
            return false;
        }
        if (index == failingRecordIndex) {
            if (error)
                *error = QStringLiteral("simulated record read failure");
            return false;
        }
        ++recordAtCalls;
        *item = items.at(static_cast<qsizetype>(index));
        return true;
    }

    bool valid = false;
    QVector<SessionStateItem> items;
    int failingRecordIndex = -1;
    mutable quint64 timestampAtCalls = 0;
    mutable quint64 recordAtCalls = 0;
};

class FakePlaybackClock final : public IPlaybackClock {
  public:
    void start(int requestedFramesPerSecond) override {
        framesPerSecond = requestedFramesPerSecond;
        active = true;
    }

    void stop() override {
        active = false;
    }
    bool isActive() const override {
        return active;
    }

    void advanceFrames(int frameCount = 1) {
        for (int frame = 0; frame < frameCount; ++frame) {
            emit tick();
        }
    }

    bool active = false;
    int framesPerSecond = 0;
};

class FakeMappingRepository final : public IMappingRepository {
  public:
    bool loadFile(const QString &path, PacketMapping *mapping, QString *error) override {
        Q_UNUSED(path);
        if (fail) {
            if (error)
                *error = QStringLiteral("simulated mapping failure");
            return false;
        }
        *mapping = configuredMapping;
        return true;
    }

    bool loadJson(const QByteArray &json, PacketMapping *mapping, QString *error) override {
        Q_UNUSED(json);
        if (fail) {
            if (error)
                *error = QStringLiteral("simulated mapping failure");
            return false;
        }
        *mapping = configuredMapping;
        return true;
    }

    PacketMapping configuredMapping = timeOnlyMapping();
    bool fail = false;
};

struct FacadeTestContext {
    FakeDatagramSource datagramSource;
    FakePacketDecoder decoder;
    MetricsService metrics;
    FakeMappingRepository mappingRepository;
    FakeRecordingTransaction transaction;
    FakeSessionPersistence persistence;
    FakeRecordingClock recordingClock;
    FakeSessionReader reader;
    FakePlaybackClock playbackClock;

    ModeCoordinator modes;
    OnlineCaptureService capture{datagramSource, decoder, metrics};
    RecordingService recording{transaction, recordingClock};
    PlaybackService playback{reader, playbackClock};
    DirectApplicationRuntime runtime{capture, recording,         playback,
                                     metrics, mappingRepository, persistence};
    ApplicationViewModel viewModel;
    ApplicationFacade facade{modes, runtime, viewModel};
};

[[maybe_unused]] SessionTimestamp sessionTimestamp(double seconds) {
    const auto timestamp = SessionTimestamp::fromSeconds(seconds);
    Q_ASSERT(timestamp.has_value());
    return timestamp.value_or(SessionTimestamp{});
}

[[maybe_unused]] SessionStateItem stateItem(double timestamp, double targetTime) {
    SessionStateItem item;
    item.timestamp = sessionTimestamp(timestamp);
    item.state.target.time = targetTime;
    item.state.availableFields = QBitArray(StateField::Count);
    item.state.availableFields.setBit(StateField::Time);
    return item;
}

} // namespace
