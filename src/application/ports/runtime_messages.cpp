
#include "application/ports/runtime_messages.h"

#include <mutex>

void registerRuntimeMessageTypes() {
    static std::once_flag once;
    std::call_once(once, [] {
        qRegisterMetaType<RuntimeStateSource>();
        qRegisterMetaType<SessionTimestamp>();
        qRegisterMetaType<RuntimeRequestId>();
        qRegisterMetaType<RuntimeSourceEpoch>();
        qRegisterMetaType<CommandDispatch>();
        qRegisterMetaType<RuntimeCommandKind>();
        qRegisterMetaType<RuntimeCommandResult>();
        qRegisterMetaType<MappingLoadResult>();
        qRegisterMetaType<OnlineStartResult>();
        qRegisterMetaType<OnlineStopResult>();
        qRegisterMetaType<OnlineStateEvent>();
        qRegisterMetaType<IpPolicyChangeResult>();
        qRegisterMetaType<StateEvent>();
        qRegisterMetaType<MetricsEvent>();
        qRegisterMetaType<RecordingStateEvent>();
        qRegisterMetaType<RecordingSaveResult>();
        qRegisterMetaType<RecordingResetResult>();
        qRegisterMetaType<SessionLoadResult>();
        qRegisterMetaType<SessionCloseResult>();
        qRegisterMetaType<PlaybackPositionEvent>();
        qRegisterMetaType<PlaybackStateEvent>();
        qRegisterMetaType<PlaybackFinishedEvent>();
        qRegisterMetaType<RuntimeFailureCode>();
        qRegisterMetaType<RuntimeFailure>();
    });
}
