#pragma once

/**
 * @file scenarios.h
 * @brief Deterministic named state generators used by lar-test-sender.
 */

#include "domain/dlz/dlz_types.h"
#include "domain/state.h"

#include <QString>
#include <QStringList>

namespace TestSenderScenarios {

QStringList names();
QString description(const QString &name);
bool contains(const QString &name);
int defaultIntervalMs(const QString &name);

void initialize(const QString &name, Plane *plane, Target *target,
                dlz::TelemetryInputs *dlzInputs = nullptr);
void update(const QString &name, int packetIndex, double elapsedSeconds, Plane *plane,
            Target *target, dlz::TelemetryInputs *dlzInputs = nullptr);

} // namespace TestSenderScenarios
