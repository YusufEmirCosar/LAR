#pragma once

/**
 * @file runtime_event_context.h
 * @brief Backward-compatible name for the runtime source-generation token.
 */

#include "application/ports/runtime_messages.h"

// Source-compatible name retained for internal producer adapters while the
// public runtime protocol uses the more precise RuntimeSourceEpoch spelling.
using RuntimeEventContext = RuntimeSourceEpoch;
