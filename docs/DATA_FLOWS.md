# Runtime data flows

These UML sequence diagrams show the main success paths and the guards that
change them. Error paths return through the same typed completion or
`RuntimeFailure` channel; output state is retained unless a complete operation
succeeds.

## Mapping installation and online activation

```mermaid
sequenceDiagram
    actor User
    participant UI as OnlinePanel / workflow
    participant F as ApplicationFacade
    participant L as SourceLifecycleCoordinator
    participant R as IApplicationRuntime
    participant N as NetworkRuntimeWorker
    participant M as JsonMappingRepository
    participant C as OnlineCaptureService

    User->>UI: choose mapping file
    UI->>F: loadMapping(path)
    F->>R: loadMapping(path)
    R-->>F: CommandDispatch(requestId)
    R->>N: mappingLoadRequested(path, requestId)
    N->>M: loadFile(path)
    M-->>N: validated PacketMapping or error
    N->>C: setMapping(mapping)
    N-->>R: MappingLoadResult(requestId)
    R-->>F: mappingLoadFinished(result)
    F->>F: RequestResultGate accepts current ID
    F-->>UI: mappingLoaded / errorRaised

    User->>UI: start listener(port)
    UI->>F: startOnline(port)
    F->>L: startOnline(port)
    L->>R: startOnline(port)
    R-->>L: accepted requestId + new online epoch
    R->>N: onlineStartRequested(port, generation, requestId)
    N->>C: start(port)
    C-->>N: bound or operational error
    N-->>R: OnlineStartResult(requestId, epoch)
    R-->>L: onlineStartFinished(result)
    L->>L: accept only pending ID and desired source
    L-->>F: onlineStateChanged
```

Mapping and policy replacement are rejected while capture is active. A failed
load leaves the previous mapping installed. A failed policy replacement leaves
the previous applied policy visible and active.

## Datagram acceptance and presentation

```mermaid
sequenceDiagram
    participant OS as UDP stack
    participant U as QtUdpDatagramSource
    participant C as OnlineCaptureService
    participant D as MappedPacketDecoder
    participant V as StateValidator
    participant N as NetworkRuntimeWorker
    participant R as ThreadedApplicationRuntime
    participant L as SourceLifecycleCoordinator
    participant VM as ApplicationViewModel
    participant UI as MainWindow / viewports

    OS->>U: one datagram + sender address
    U->>U: canonical sender-policy check
    alt sender allowed
        U-->>C: datagramReceived(bytes, monotonicNs)
        C->>D: decode(bytes, temporaryState)
        D->>V: validate(temporaryState)
        alt complete frame valid
            D-->>C: DecodedState
            C-->>N: rawPacketReceived(CapturedPacket)
            C->>C: retain latest visual state
            Note over C: 16 ms publication timer coalesces visuals only
            C-->>N: stateReceived(latest)
            N-->>R: StateEvent(onlineEpoch, state)
            R-->>L: stateReady(event)
            L->>L: epoch/source acceptance check
            L->>VM: setDecodedState(state)
            VM-->>UI: stateChanged
        else invalid packet
            D-->>C: categorized text error
            C-->>R: RuntimeFailure
        end
    else sender denied
        U->>U: discard before decoding
    end
```

Every accepted raw packet remains eligible for recording even when visual
publication is coalesced. A rejected packet affects neither the view model nor
the recording transaction.

## Recording append and backpressure

```mermaid
sequenceDiagram
    participant N as NetworkRuntimeWorker
    participant RW as RecordingRuntimeWorker
    participant P as RecordingPipelineCoordinator
    participant S as RecordingService
    participant T as LarSessionWriter

    N->>N: collect accepted CapturedPacket values
    Note over N: Flush every 2 ms, 2048 packets, or 2 MiB
    N-->>RW: recordingBatchReady(batchId, packets)
    RW->>P: appendRecordingBatch(batchId, packets)
    P->>S: recordPackets(packets)
    loop packet in receipt order
        S->>S: derive active SessionTimestamp
        S->>T: append(timestamp, raw bytes)
    end
    P-->>RW: recordingBatchHandled(batchId)
    RW-->>N: acknowledgeRecordingBatch(batchId)
```

At most eight batches are outstanding. The network side caps a batch at 2 MiB
and buffered recording input at 32 MiB. If the session side cannot keep up,
recording input pauses with a diagnostic rather than growing without bound.
UDP display processing can continue.

## Snapshot barrier

Snapshot, pause, reset, discard, and final save all use the same ordered drain
barrier. Snapshot differs because post-boundary accepted packets are retained
and recording resumes after the immutable prefix is captured.

```mermaid
sequenceDiagram
    actor User 
    participant F as ApplicationFacade 
    participant R as ThreadedApplicationRuntime 
    participant P as RecordingPipelineCoordinator
    participant N as NetworkRuntimeWorker 
    participant S as RecordingService 
    participant T as LarSessionWriter 
    participant W as PersistenceRuntimeWorker 
    participant Q as QtSessionPersistence

    User->>F: snapshotRecording(path)
    F->>R: snapshotRecording(path)
    R-->>F: CommandDispatch(requestId)
    R->>P: snapshotRecording(path)
    P-->>N: recordingDrainRequested(token, preserveIncoming=true)
    
    N->>N: mark monotonic boundary, flush pre-boundary batch
    
    N-->>P: recordingInputDrained(token, boundaryNs)
    P->>S: pauseRecordingAt(boundaryNs)
    P->>S: createSnapshot()
    S->>T: createSnapshot()
    T-->>S: immutable FileSessionSnapshot
    P-->>W: persistenceRequested(snapshot, path)
    W->>Q: save(snapshot, path)
    Q->>Q: QSaveFile write + commit
    Q-->>W: saved or failure
    W-->>P: persistenceFinished
    
    alt snapshot saved
        P->>S: resumeRecording()
        P-->>N: recordingInputEnabled(true)
        N->>N: merge bounded post-boundary buffer
    else save failed
        P->>S: preserve transaction for retry
        P-->>F: RecordingSaveResult(saved=false)
    end
```

For final save, `preserveIncoming` is false. Successful persistence completes
the transaction; the facade then completes the requested online stop. Failure
retains the transaction and existing destination for retry.

## Offline source transition and playback

```mermaid
sequenceDiagram
    actor User
    participant F as ApplicationFacade
    participant L as SourceLifecycleCoordinator
    participant R as ThreadedApplicationRuntime
    participant P as PlaybackRuntimeWorker
    participant RD as LarSessionReader
    participant S as PlaybackService
    participant T as PlaybackPublicationThrottle
    participant VM as ApplicationViewModel

    User->>F: loadSession(path)
    F->>L: loadSession(path)
    opt online source active
        L->>R: stopOnline()
        R-->>L: matching OnlineStopResult
    end
    L->>R: loadSession(path)
    R-->>L: requestId + new playback epoch
    R->>P: sessionLoadRequested(path, epoch, requestId)
    P->>S: loadSession(path)
    S->>RD: loadFile(path)
    
    RD->>RD: validate header, mapping, records, build index
    
    RD-->>S: valid reader or error
    S->>RD: recordAt(0)
    P-->>L: SessionLoadResult(requestId, epoch)
    L->>L: request/desired-source acceptance
    L->>VM: loaded metadata + first frame

    User->>F: play()
    F->>L: play()
    L->>R: play()
    R->>P: playbackStartRequested
    P->>S: play()
    loop 60 Hz replay ticks
        S->>S: next = current + (1000 / 60) * rate
        S->>RD: binary-search strict predecessor timestamp
        RD-->>S: decode at most one selected record
        S-->>T: selected frame + exact position
        T-->>VM: latest epoch-tagged publication every 16 ms
    end
```

Seek uses a binary search for the last timestamp at or before the requested
position. Normal replay does not decode intermediate records between sampled
positions. With Repeat enabled, the newly calculated cursor is reduced modulo
the final timestamp before every binary search, preserving any overshoot even
across multiple loops. Without Repeat, passing the final timestamp publishes
completion. The separate Burst widget is an inert placeholder and dispatches
no runtime command.

## Source epoch rejection

Queued Qt delivery means an old worker event can physically arrive after a new
source becomes active. Epoch checking makes this harmless.

```mermaid
sequenceDiagram
    participant Old as Online generation 7
    participant L as SourceLifecycleCoordinator
    participant New as Playback generation 8
    participant VM as ApplicationViewModel

    New-->>L: SessionLoadResult(playback, 8)
    L->>L: activeEpoch = playback/8
    Old-->>L: delayed StateEvent(online, 7)
    L->>L: rejects source/generation mismatch
    New-->>L: StateEvent(playback, 8)
    L->>VM: accepts atomic state
```

Request IDs protect command completions; source epochs protect continuing
publications. They solve different races and both are required.

## Build-time and runtime map flow

```mermaid
sequenceDiagram
    participant CMake
    participant G as GeoJsonSourceReader
    participant M as MapMeshCompiler
    participant W as MapAssetWriter
    participant P as PackagedMapAssetSource
    participant L as EarthMapLoadController
    participant V as EarthLarView
    participant GPU as EarthMapGpuRenderer

    CMake->>G: read world_boundaries.geojson
    G-->>M: bounded SourceMap
    M->>M: project, triangulate, deduplicate, build borders
    M-->>W: MapMesh
    W-->>CMake: .larmap + manifest
    CMake->>CMake: package beside executable

    V->>L: ensureLoaded()
    L->>P: load() on concurrent task
    P->>P: manifest + package validation
    P-->>L: MapAssetReadResult(revision)
    L-->>V: assetReady(revision, result)
    V->>GPU: installMesh(mesh) with current context
    V-->>L: completeInstallation(revision, installed)
```

The runtime never parses GeoJSON. Revisions prevent a late asynchronous result
from installing after invalidation or a newer request.

## LAR zone preparation and drawing

```mermaid
flowchart LR
    STATE["Target + availability"] --> VALID["LarZoneInputValidator"]
    VALID --> DEF["IR / IZ definitions"]
    DEF --> PARAM["LarParametricZoneGpuLayer"]
    DEF --> SAMPLE["GeodesicZoneSampler"]
    CAMERA["MapCamera + viewport size"] --> SAMPLE
    SAMPLE --> ASSEMBLE["LarZoneMeshAssembler"]
    ASSEMBLE --> MESH["LarZoneMesh"]
    MESH --> GPU["LarZoneGpuLayer fallback"]
```

The parametric shader path is primary. The CPU worker path produces the same
bounded geodesic definitions for fallback and tests. Inputs are rejected before
allocation if fields, coordinates, radii, or counts violate central limits.

## DLZ input arbitration

```mermaid
sequenceDiagram
    participant VM as ApplicationViewModel
    participant V as ValuesPanel
    participant W as HudWorkspace
    participant A as ScenarioAdapter
    participant P as PresentationController
    participant H as HudView

    VM-->>V: atomic dlzInputs + availability
    V->>W: setExternalInputs(inputs, complete, source)
    alt UDP / Offline Replay mode
        W->>A: build(external inputs)
        A-->>W: geometry + guarded solution or error
    else Calculation Test mode
        W->>W: cache external inputs only
        W->>A: build(slider inputs)
        A-->>W: geometry + guarded solution or error
    end
    alt valid supported frame
        W->>P: update(solution, range, rate, mode, dt)
        P-->>H: HudState + displayed solution
    else invalid or unsupported
        W->>P: clear()
        
        W-->>H: persistent diagnostic, no launch-zone drawing
        
    end
```

Changing input mode never blends sources. The external triple is atomic: a
mapping with only one or two DLZ fields is invalid.
