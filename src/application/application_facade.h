#pragma once

/**
 * @file application_facade.h
 * @brief Main-thread command facade consumed by the Qt presentation layer.
 */

#include "application/application_state.h"
#include "application/application_view_model.h"
#include "application/ip_access_policy_service.h"
#include "application/mode_coordinator.h"
#include "application/ports/application_runtime.h"
#include "application/recording_operation_state.h"
#include "application/request_result_gate.h"

#include <QObject>
#include <QString>

class SourceLifecycleCoordinator;

/**
 * Main-thread presentation facade.
 *
 * It owns no socket, timer, session reader, or session writer. All potentially
 * expensive work crosses IApplicationRuntime and returns as queued state.
 */
class ApplicationFacade final : public QObject {
    Q_OBJECT

  public:
    /**
     * @brief Creates the main-thread application facade.
     *
     * @details Connects the mode coordinator, runtime, source lifecycle coordinator, and
     * presentation view model. The facade owns the presentation-side orchestration while
     * the injected services remain owned by their caller.
     *
     * @param[in] modes Coordinator used to observe and change the application mode.
     * @param[in] runtime Runtime boundary used to dispatch asynchronous commands.
     * @param[in] viewModel Presentation state exposed to the Qt view layer.
     * @param[in] parent Optional Qt parent that owns this facade.
     *
     */
    ApplicationFacade(ModeCoordinator &modes, IApplicationRuntime &runtime,
                      ApplicationViewModel &viewModel, QObject *parent = nullptr);
    /**
     * @brief Creates the application facade with IP access-policy file support.
     *
     * @details This overload additionally injects the service used by
     * loadIpAccessPolicy() to read a policy from disk before submitting it to the runtime.
     * All non-owned collaborators must outlive the facade.
     *
     * @param[in] modes Coordinator used to observe and change the application mode.
     * @param[in] runtime Runtime boundary used to dispatch asynchronous commands.
     * @param[in] viewModel Presentation state exposed to the Qt view layer.
     * @param[in] ipAccessPolicyService Service used to load policies from files.
     * @param[in] parent Optional Qt parent that owns this facade.
     *
     */
    ApplicationFacade(ModeCoordinator &modes, IApplicationRuntime &runtime,
                      ApplicationViewModel &viewModel, IpAccessPolicyService &ipAccessPolicyService,
                      QObject *parent = nullptr);

    /**
     * @brief Returns the read-only presentation view model.
     *
     * @details The returned reference exposes the current presentation state without
     * allowing callers to modify it through this facade.
     *
     * @return Reference to the view model associated with this facade.
     *
     */
    const ApplicationViewModel &viewModel() const noexcept {
        return m_viewModel;
    }

    /**
     * @brief Returns the mutable presentation view model.
     *
     * @details The mutable reference is intended for composition and focused tests that
     * need to configure the facade's presentation state.
     *
     * @return Reference to the view model associated with this facade.
     *
     */
    ApplicationViewModel &viewModel() noexcept {
        return m_viewModel;
    }

    /**
     * @brief Requests loading and validation of a packet mapping file.
     *
     * @details The request is rejected while online capture or recording is active.
     * When accepted, parsing and validation are delegated to the runtime; completion is
     * reported through the mappingLoaded() and errorRaised() signals.
     *
     * @param[in] path Path to the mapping file to load.
     *
     * @return True when the runtime accepts the request; false when validation or dispatch
     * rejects it.
     *
     * @see mappingLoaded()
     *
     */
    bool loadMapping(const QString &path);
    /**
     * @brief Returns the number of fields in the active packet mapping.
     *
     * @details The value is updated after a mapping has been successfully loaded.
     *
     * @return Number of mapped fields, or zero when no mapping is active.
     *
     */
    int mappedFieldCount() const noexcept {
        return m_state.mappedFieldCount;
    }

    /**
     * @brief Returns the minimum packet size required by the active mapping.
     *
     * @details The value is updated after a mapping has been successfully loaded and is
     * used by packet-source validation.
     *
     * @return Minimum packet size in bytes, or zero when no mapping is active.
     *
     */
    int minimumPacketSize() const noexcept {
        return m_state.minimumPacketSize;
    }

    /**
     * @brief Requests the start of online packet capture.
     *
     * @details A valid mapping must be loaded and no recording session may be active.
     * The source lifecycle coordinator owns the source transition and reports the final
     * state asynchronously.
     *
     * @param[in] port UDP port on which the online source should listen.
     *
     * @return True when the start request is accepted; false when preconditions or
     * dispatch reject it.
     *
     * @see stopOnline()
     *
     */
    bool startOnline(quint16 port);
    /**
     * @brief Requests installation of an IP access policy.
     *
     * @details Online capture must be stopped before the policy can change. The policy
     * is sent to the runtime and the resulting active policy is reported asynchronously.
     *
     * @param[in] policy Policy to install for incoming datagrams.
     *
     * @return True when the runtime accepts the request; false when capture is active or
     * dispatch rejects it.
     *
     * @see loadIpAccessPolicy()
     *
     */
    bool setIpAccessPolicy(const IpAccessPolicy &policy);
    /**
     * @brief Loads an IP access policy from a file and submits it for installation.
     *
     * @details The injected policy service parses the file. A successfully parsed policy
     * is then passed to setIpAccessPolicy(); parsing failures are reported through
     * errorRaised().
     *
     * @param[in] path Path to the policy file to load.
     *
     * @return True when the file is loaded and the installation request is accepted;
     * false otherwise.
     *
     * @see setIpAccessPolicy()
     *
     */
    bool loadIpAccessPolicy(const QString &path);
    /**
     * @brief Requests the stop of online packet capture.
     *
     * @details The request is rejected while a recording session is active because the
     * source must remain available until the recording is saved or reset.
     *
     * @return True when the source stop request is accepted; false when it is rejected.
     *
     * @see startOnline()
     *
     */
    bool stopOnline();
    /**
     * @brief Discards the active in-memory recording and then stops online capture.
     *
     * @details This is used only after the user has accepted losing the current recording.
     * The online source is stopped after the asynchronous discard has completed.
     *
     * @return True when the discard request is accepted; false when it is rejected.
     */
    bool stopOnlineAndDiscardRecording();
    /**
     * @brief Reports whether online capture is currently listening.
     *
     * @details This is the last state reported by the source lifecycle coordinator.
     *
     * @return True when the online source is listening; false otherwise.
     *
     */
    bool isListening() const noexcept {
        return m_state.listening;
    }

    /**
     * @brief Returns the port associated with the online source.
     *
     * @details The value reflects the most recently reported online-source state.
     *
     * @return UDP listening port, or zero when no port is active.
     *
     */
    quint16 listeningPort() const noexcept {
        return m_state.listeningPort;
    }

    /**
     * @brief Requests the start of a recording transaction.
     *
     * @details Recording requires a valid mapping and active online capture. The runtime
     * owns packet collection; this facade tracks the transaction state and forwards
     * completion notifications.
     *
     * @return True when the runtime accepts the start request; false when preconditions
     * or dispatch reject it.
     *
     * @see stopRecording()
     * @see pauseRecording()
     *
     */
    bool startRecording();
    /**
     * @brief Pauses the active recording transaction.
     *
     * @details The request is ignored while another recording operation is pending.
     * Packet-source state is updated by the runtime and its recording-state signal.
     *
     */
    void pauseRecording();
    /**
     * @brief Resumes a paused recording transaction.
     *
     * @details The request is ignored while another recording operation is pending.
     *
     */
    void resumeRecording();
    /**
     * @brief Finalizes the active recording and saves it to a file.
     *
     * @details The operation enters the finalizing state and dispatches the save request
     * to the runtime. Completion, failure, and any deferred source shutdown are handled
     * asynchronously.
     *
     * @param[in] targetPath Destination path for the finalized recording.
     *
     * @return True when the save request is accepted; false when validation or dispatch
     * rejects it.
     *
     * @see snapshotRecording()
     * @see resetRecording()
     *
     */
    bool stopRecording(const QString &targetPath);
    /**
     * @brief Requests reset of the active recording transaction.
     *
     * @details Reset clears the runtime recording without saving it. The facade exposes
     * the resetting, idle, or retained-failure state through recordingOperationStateChanged().
     *
     * @return True when the reset request is accepted; false when another operation or
     * dispatch rejects it.
     *
     * @warning Reset discards recording data when the runtime completes the request.
     *
     */
    bool resetRecording();
    /**
     * @brief Saves a snapshot of the active recording without finalizing it.
     *
     * @details The operation enters the snapshot-saving state and delegates persistence
     * to the runtime. The active recording remains available after a successful snapshot.
     *
     * @param[in] targetPath Destination path for the snapshot.
     *
     * @return True when the snapshot request is accepted; false when validation or
     * dispatch rejects it.
     *
     * @see stopRecording()
     *
     */
    bool snapshotRecording(const QString &targetPath);
    /**
     * @brief Reports whether packets are currently being recorded.
     *
     * @details A paused recording session is represented separately and does not report
     * as actively recording.
     *
     * @return True when the recording session is actively accepting packets; false otherwise.
     *
     */
    bool isRecording() const noexcept {
        return m_state.isRecording();
    }

    /**
     * @brief Reports whether a recording session exists.
     *
     * @details The result remains true while a session is paused or awaiting persistence.
     *
     * @return True when a recording session exists; false when no session is active.
     *
     */
    bool hasRecordingSession() const noexcept {
        return m_state.hasRecordingSession;
    }

    RecordingOperationState recordingOperationState() const noexcept {
        return m_recordingOperationState;
    }

    /**
     * @brief Reports whether a recording persistence operation is pending.
     *
     * @details A pending operation prevents a second final save, snapshot, or reset from
     * being dispatched until the first operation completes.
     *
     * @return True when a recording operation is in progress; false otherwise.
     *
     */
    bool recordingOperationPending() const noexcept {
        return isRecordingOperationPending(m_recordingOperationState);
    }

    /**
     * @brief Reports whether the existing recording session is paused.
     *
     * @return True when a recording session exists and is paused; false otherwise.
     *
     */
    bool isRecordingPaused() const noexcept {
        return m_state.isRecordingPaused();
    }

    /**
     * @brief Discards the active recording without persisting it.
     *
     * @details Pending save/reset bookkeeping is cleared and the runtime is instructed to
     * release the recording. This operation is noexcept and does not report a result.
     *
     * @warning Any unsaved recording data is lost.
     *
     */
    void discardRecording() noexcept;

    /**
     * @brief Requests loading and indexing of an offline session file.
     *
     * @details An active recording must be saved or reset before an offline session can
     * be loaded. The source lifecycle coordinator performs validation and reports the
     * result through sessionLoaded() or errorRaised().
     *
     * @param[in] path Path to the session file to load.
     *
     * @return True when the load request is accepted; false when preconditions or
     * dispatch reject it.
     *
     * @see closeSession()
     *
     */
    bool loadSession(const QString &path);
    /**
     * @brief Closes the active offline session.
     *
     * @details The source lifecycle coordinator releases the loaded session and updates
     * the presentation state.
     *
     */
    void closeSession();
    /**
     * @brief Reports whether an offline session is loaded.
     *
     * @return True when a validated playback session is active; false otherwise.
     *
     */
    bool isSessionLoaded() const noexcept {
        return m_state.sessionLoaded;
    }

    /**
     * @brief Returns the number of records in the loaded offline session.
     *
     * @return Record count reported by the active session index, or zero when no session
     * is loaded.
     *
     */
    qint64 loadedRecordCount() const noexcept {
        return m_state.loadedRecordCount;
    }

    /**
     * @brief Starts or resumes offline playback.
     *
     * @details The request is forwarded to the source lifecycle coordinator, which owns
     * playback timing and state transitions.
     *
     * @see pause()
     * @see stop()
     *
     */
    void play();
    /**
     * @brief Pauses offline playback at its current position.
     *
     * @details The request is forwarded to the source lifecycle coordinator.
     *
     * @see play()
     *
     */
    void pause();
    /**
     * @brief Stops offline playback and returns it to its initial position.
     *
     * @details The request is forwarded to the source lifecycle coordinator.
     *
     * @see play()
     *
     */
    void stop();
    /**
     * @brief Seeks playback using a position expressed in seconds.
     *
     * @details The floating-point value is converted to a validated SessionTimestamp.
     * Invalid or non-finite values are reported through errorRaised() and are not sent to
     * the playback source.
     *
     * @param[in] positionSeconds Playback position in seconds.
     *
     * @see seek(SessionTimestamp)
     *
     */
    void seek(double positionSeconds);
    /**
     * @brief Seeks playback to an exact session timestamp.
     *
     * @details The already validated timestamp is forwarded to the source lifecycle
     * coordinator.
     *
     * @param[in] position Exact playback position within the loaded session.
     *
     * @see seek(double)
     *
     */
    void seek(SessionTimestamp position);
    /**
     * @brief Sets the offline playback speed.
     *
     * @details The rate must be positive and finite. When accepted, the runtime is
     * updated and the presentation view model receives the new rate.
     *
     * @param[in] rate Positive playback-speed multiplier.
     *
     * @return True when the runtime accepts the rate; false for invalid input or rejected
     * dispatch.
     *
     */
    bool setPlaybackRate(double rate);
    /**
     * @brief Enables or disables automatic playback repeat.
     *
     * @param[in] enabled Whether playback should restart after reaching the end.
     *
     * @return True when the runtime accepts the setting; false when dispatch rejects it.
     *
     */
    bool setPlaybackRepeat(bool enabled);

    /**
     * @brief Resets the processed-packet metrics.
     *
     * @details The request is forwarded to the source lifecycle coordinator, which resets
     * the count and related presentation metrics.
     *
     */
    void resetProcessedPacketCount();
    /**
     * @brief Shuts down source lifecycles and the runtime.
     *
     * @details Shutdown is idempotent. It clears pending result gates, prevents repeated
     * shutdown work, then asks the source coordinator and runtime to stop.
     *
     */
    void shutdown() noexcept;

  signals:
    /**
     * @brief Notifies observers that the presentation view model changed.
     *
     * @details Connected Qt observers can use this signal to refresh bindings or derived
     * presentation state.
     *
     */
    void viewModelChanged();
    /**
     * @brief Reports an operation or runtime error to observers.
     *
     * @param[in] error Human-readable description of the failure.
     *
     */
    void errorRaised(const QString &error);
    /**
     * @brief Reports a change in the processed-packet count.
     *
     * @param[in] count Number of packets processed by the active source.
     *
     */
    void processedPacketCountChanged(quint64 count);
    /**
     * @brief Reports a change in the processed-packet rate.
     *
     * @param[in] rate Current processed-packet rate.
     *
     */
    void processedPacketRateChanged(quint64 rate);
    /**
     * @brief Reports successful loading of a packet mapping.
     *
     * @param[in] path Path of the mapping that was loaded.
     * @param[in] mappedFieldCount Number of fields described by the mapping.
     * @param[in] minimumPacketSize Minimum packet size required by the mapping, in bytes.
     *
     */
    void mappingLoaded(const QString &path, int mappedFieldCount, int minimumPacketSize);
    /**
     * @brief Reports a change in online capture state.
     *
     * @param[in] listening True when the online source is listening; false when it stopped.
     *
     */
    void onlineStateChanged(bool listening);
    /**
     * @brief Reports completion of an IP access-policy change.
     *
     * @param[in] applied True when the requested policy was installed successfully.
     * @param[in] activePolicy Policy currently active after the operation completes.
     *
     */
    void ipAccessPolicyChangeFinished(bool applied, const IpAccessPolicy &activePolicy);
    /**
     * @brief Reports whether the recording session is actively capturing packets.
     *
     * @param[in] isRecording True while packets are actively being recorded; false when
     * recording is paused or no session exists.
     *
     */
    void recordingChanged(bool isRecording);
    /**
     * @brief Reports a change in the recording-operation state.
     *
     * @param[in] state New state for final save, snapshot, reset, or failure handling.
     *
     */
    void recordingOperationStateChanged(RecordingOperationState state);
    /**
     * @brief Reports successful loading of an offline session.
     *
     * @param[in] path Path of the session file that was loaded.
     * @param[in] recordCount Number of indexed records in the session.
     *
     */
    void sessionLoaded(const QString &path, qint64 recordCount);

  private:
    /**
     * @brief Stores and emits a human-readable operation error.
     *
     * @details The message is written to the view model and emitted through errorRaised().
     * The helper always returns false so it can be used by rejecting boolean operations.
     *
     * @param[in] error Error message to report; an empty message is replaced by a generic
     * failure message.
     *
     * @return Always false.
     *
     */
    bool reportError(const QString &error);
    /**
     * @brief Applies the result of an asynchronous mapping-load request.
     *
     * @details Successful results update mapping state and emit mappingLoaded(); failures
     * are routed through reportError().
     *
     * @param[in] result Runtime result produced by the mapping-load operation.
     *
     */
    void applyResult(const MappingLoadResult &result);
    /**
     * @brief Applies the result of an asynchronous IP-policy change.
     *
     * @details A successful result replaces the active policy. Failures retain the prior
     * policy and are reported before completion is emitted.
     *
     * @param[in] result Runtime result produced by the policy-change operation.
     *
     */
    void applyResult(const IpPolicyChangeResult &result);
    /**
     * @brief Applies the result of an asynchronous recording save.
     *
     * @details The result updates operation state and, after a final save, coordinates the
     * deferred stop of online capture.
     *
     * @param[in] result Runtime result produced by the recording-save operation.
     *
     */
    void applyResult(const RecordingSaveResult &result);
    /**
     * @brief Applies the result of an asynchronous recording reset.
     *
     * @details Successful resets return the operation state to idle. Failures preserve the
     * recording and expose a retained-failure state.
     *
     * @param[in] result Runtime result produced by the recording-reset operation.
     *
     */
    void applyResult(const RecordingResetResult &result);
    /**
     * @brief Updates and publishes the recording-operation state.
     *
     * @details Repeated assignments are ignored. A changed state is emitted through
     * recordingOperationStateChanged().
     *
     * @param[in] state New recording-operation state to publish.
     *
     */
    void setRecordingOperationState(RecordingOperationState state);

    ModeCoordinator &m_modes;
    IApplicationRuntime &m_runtime;
    ApplicationViewModel &m_viewModel;
    IpAccessPolicyService *m_ipAccessPolicyService = nullptr;
    SourceLifecycleCoordinator *m_sources = nullptr;

    ApplicationState m_state;
    IpAccessPolicy m_ipAccessPolicy;
    RecordingOperationState m_recordingOperationState = RecordingOperationState::Idle;
    RequestResultGate<MappingLoadResult> m_mappingLoads;
    RequestResultGate<IpPolicyChangeResult> m_policyChanges;
    RequestResultGate<RecordingSaveResult> m_recordingSaves;
    RequestResultGate<RecordingResetResult> m_recordingResets;
    bool m_stopOnlineAfterFinalSave = false;
    bool m_stopOnlineAfterDiscard = false;
};
