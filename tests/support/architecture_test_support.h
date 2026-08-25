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

/**
 * @brief Performs the time Only Mapping operation.
 *
 * @details The operation exposes the stable behavior of the owning type to its callers.
 *
 * @return The value produced by the operation.
 */
[[maybe_unused]] PacketMapping timeOnlyMapping() {
    return PacketMapping({{StateField::Time, 0, 8}},
                         QByteArrayLiteral(R"([{"name":"time","index":0,"offset":0,"size":8}])"));
}

class FakeDatagramSource final : public IDatagramSource {
  public:
    /**
     * @brief Starts start.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] port UDP port used by the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool start(quint16 port, QString *error) override {
        Q_UNUSED(error);
        listening = true;
        boundPort = port;
        return true;
    }

    /**
     * @brief Ends or resets stop.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     */
    void stop() override {
        listening = false;
    }
    /**
     * @brief Reports whether is Listening is true.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    bool isListening() const override {
        return listening;
    }
    /**
     * @brief Performs the local Port operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    quint16 localPort() const override {
        return boundPort;
    }

    /**
     * @brief Updates set Ip Access Policy.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] policy Policy supplied to the operation.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool setIpAccessPolicy(const IpAccessPolicy &policy) override {
        if (!acceptPolicyChanges)
            return false;
        ipPolicy = policy;
        return true;
    }

    /**
     * @brief Performs the deliver operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] data Data supplied to the operation.
     * @param[in] receivedAtNanoseconds Time value expressed in seconds.
     */
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
    /**
     * @brief Updates set Mapping.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] mapping Mapping supplied to the operation.
     */
    void setMapping(PacketMapping mapping) override {
        currentMapping = std::move(mapping);
    }

    /**
     * @brief Obtains decode.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] packet Packet supplied to the operation.
     * @param[in] state State value supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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
    /**
     * @brief Constructs a FakeSessionSnapshot.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] bytes Bytes supplied to the operation.
     */
    explicit FakeSessionSnapshot(QByteArray bytes = {}) : payload(std::move(bytes)) {}

    /**
     * @brief Performs the write To operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] destination Destination supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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
    /**
     * @brief Starts begin.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] mappingJson Mapping Json supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool begin(const QByteArray &mappingJson, QString *error) override {
        Q_UNUSED(error);
        active = true;
        mapping = mappingJson;
        timestamps.clear();
        packets.clear();
        return true;
    }

    /**
     * @brief Performs the append operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] timestamp Timestamp supplied to the operation.
     * @param[in] packet Packet supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool append(SessionTimestamp timestamp, const QByteArray &packet, QString *error) override {
        Q_UNUSED(error);
        if (!active)
            return false;
        timestamps.append(timestamp);
        packets.append(packet);
        return true;
    }

    /**
     * @brief Ends or resets reset.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool reset(QString *error) override {
        Q_UNUSED(error);
        if (!active)
            return false;
        timestamps.clear();
        packets.clear();
        return true;
    }

    /**
     * @brief Performs the create Snapshot operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] snapshot Snapshot supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool createSnapshot(SessionSnapshot *snapshot, QString *error) override {
        if (!active || !snapshot) {
            if (error)
                *error = QStringLiteral("no active transaction");
            return false;
        }
        *snapshot = std::make_shared<FakeSessionSnapshot>();
        return true;
    }

    /**
     * @brief Reports whether cancel is true.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     */
    void cancel() noexcept override {
        active = false;
        timestamps.clear();
        packets.clear();
    }

    /**
     * @brief Reports whether is Active is true.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    bool isActive() const noexcept override {
        return active;
    }
    /**
     * @brief Performs the record Count operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
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
    /**
     * @brief Performs the save operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] snapshot Snapshot supplied to the operation.
     * @param[in] targetPath Path of the input or output file.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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
    /**
     * @brief Performs the now Nanoseconds operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    qint64 nowNanoseconds() const override {
        return now;
    }

    qint64 now = 0;
};

class FakeSessionReader final : public ISessionReader {
  public:
    /**
     * @brief Obtains load File.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] path Path of the input or output file.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool loadFile(const QString &path, QString *error) override {
        Q_UNUSED(path);
        Q_UNUSED(error);
        valid = true;
        return true;
    }

    /**
     * @brief Obtains load Data.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] data Data supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
    bool loadData(const QByteArray &data, QString *error) override {
        Q_UNUSED(data);
        Q_UNUSED(error);
        valid = true;
        return true;
    }

    /**
     * @brief Ends or resets close.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     */
    void close() noexcept override {
        valid = false;
    }
    /**
     * @brief Reports whether is Valid is true.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    bool isValid() const noexcept override {
        return valid;
    }
    /**
     * @brief Performs the record Count operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    qint64 recordCount() const noexcept override {
        return items.size();
    }
    /**
     * @brief Performs the duration operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return The value produced by the operation.
     */
    SessionTimestamp duration() const noexcept override {
        return items.isEmpty() ? SessionTimestamp{} : items.constLast().timestamp;
    }
    /**
     * @brief Performs the timestamp At operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] index Index supplied to the operation.
     * @param[in] timestamp Timestamp supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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
    /**
     * @brief Performs the record At operation.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] index Index supplied to the operation.
     * @param[in] item Item supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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
    /**
     * @brief Starts start.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] requestedFramesPerSecond Requested playback frame rate.
     */
    void start(int requestedFramesPerSecond) override {
        framesPerSecond = requestedFramesPerSecond;
        active = true;
    }

    /**
     * @brief Ends or resets stop.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     */
    void stop() override {
        active = false;
    }
    /**
     * @brief Reports whether is Active is true.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    bool isActive() const override {
        return active;
    }

    /**
     * @brief Performs the advance Frames operation.
     *
     * @details The operation follows the contract of the owning type and preserves its documented
     * invariants.
     *
     * @param[in] frameCount Frame Count supplied to the operation.
     */
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
    /**
     * @brief Obtains load File.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] path Path of the input or output file.
     * @param[out] mapping Mapping supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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

    /**
     * @brief Obtains load Json.
     *
     * @details Implementations provide the concrete behavior required by the owning interface.
     *
     * @param[in] json Json supplied to the operation.
     * @param[out] mapping Mapping supplied to the operation.
     * @param[out] error Optional destination for a human-readable diagnostic.
     *
     * @return True when the operation succeeds; false otherwise.
     */
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

/**
 * @brief Performs the session Timestamp operation.
 *
 * @details The operation follows the contract of the owning type and preserves its documented
 * invariants.
 *
 * @param[in] seconds Time value expressed in seconds.
 *
 * @return The value produced by the operation.
 */
[[maybe_unused]] SessionTimestamp sessionTimestamp(double seconds) {
    const auto timestamp = SessionTimestamp::fromSeconds(seconds);
    Q_ASSERT(timestamp.has_value());
    return timestamp.value_or(SessionTimestamp{});
}

/**
 * @brief Performs the state Item operation.
 *
 * @details The operation follows the contract of the owning type and preserves its documented
 * invariants.
 *
 * @param[in] timestamp Timestamp supplied to the operation.
 * @param[in] targetTime Target Time supplied to the operation.
 *
 * @return The value produced by the operation.
 */
[[maybe_unused]] SessionStateItem stateItem(double timestamp, double targetTime) {
    SessionStateItem item;
    item.timestamp = sessionTimestamp(timestamp);
    item.state.target.time = targetTime;
    item.state.availableFields = QBitArray(StateField::Count);
    item.state.availableFields.setBit(StateField::Time);
    return item;
}

} // namespace
