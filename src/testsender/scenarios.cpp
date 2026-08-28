
#include "testsender/scenarios.h"

#include <QVector>

#include <algorithm>
#include <cmath>

namespace TestSenderScenarios {
namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double TwoPi = 2.0 * Pi;
constexpr double DegreesToRadians = Pi / 180.0;
constexpr double EarthRadiusMeters = 6371000.0;
constexpr double BaseLatitude = 0.71558;
constexpr double BaseLongitude = 0.50490;
constexpr double TargetLatitude = 0.71575;
constexpr double TargetLongitude = 0.50530;
constexpr double IzNorthOffsetMeters = -200.0;
constexpr double IzEastOffsetMeters = -570.0;

using InitializeFunction = void (*)(Plane *, Target *);
using UpdateFunction = void (*)(int, double, Plane *, Target *);
using DlzInitializeFunction = void (*)(dlz::TelemetryInputs *);
using DlzUpdateFunction = void (*)(int, double, dlz::TelemetryInputs *);

struct ScenarioDefinition final {
    QString name;
    QString description;
    int defaultIntervalMs = 100;
    InitializeFunction initialize = nullptr;
    UpdateFunction update = nullptr;
};

struct DlzScenarioDefinition final {
    QString name;
    QString description;
    int defaultIntervalMs = 100;
    DlzInitializeFunction initialize = nullptr;
    DlzUpdateFunction update = nullptr;
};

double wrapLongitude(double longitude) {
    double wrapped = std::fmod(longitude + Pi, TwoPi);
    if (wrapped < 0.0)
        wrapped += TwoPi;
    wrapped -= Pi;
    constexpr double BoundaryMargin = 0.000001;
    return std::clamp(wrapped, -Pi + BoundaryMargin, Pi - BoundaryMargin);
}

double wrapPositiveAngle(double angle) {
    double wrapped = std::fmod(angle, TwoPi);
    return wrapped < 0.0 ? wrapped + TwoPi : wrapped;
}

double triangleWave(double phase) {
    return (2.0 / Pi) * std::asin(std::sin(phase));
}

void setPosition(double baseLatitude, double baseLongitude, double northMeters, double eastMeters,
                 double altitude, double position[3]) {
    position[0] = baseLatitude + northMeters / EarthRadiusMeters;
    position[1] =
        wrapLongitude(baseLongitude + eastMeters / (EarthRadiusMeters * std::cos(baseLatitude)));
    position[2] = altitude;
}

void setLarCenterPair(double baseLatitude, double baseLongitude, double northMeters,
                      double eastMeters, Target *target) {
    setPosition(baseLatitude, baseLongitude, northMeters, eastMeters, 1.0, target->ir_pos);
    setPosition(target->ir_pos[0], target->ir_pos[1], IzNorthOffsetMeters, IzEastOffsetMeters, 1.0,
                target->iz_pos);
}

void setTargetPositions(double northMeters, double eastMeters, Target *target) {
    setLarCenterPair(TargetLatitude, TargetLongitude, northMeters, eastMeters, target);
}

void setLarPulse(double elapsedSeconds, Target *target) {
    const double rangePulse = std::sin(0.37 * elapsedSeconds);
    const double anglePulse = std::sin(0.23 * elapsedSeconds + 0.4);
    const double center = 0.25 * std::sin(0.19 * elapsedSeconds);
    const double halfWidth = 0.48 + 0.17 * anglePulse;
    target->iz_theta1 = center - halfWidth;
    target->iz_theta2 = center + halfWidth;
    target->iz_r1 = 1600.0 + 700.0 * rangePulse;
    target->iz_r2 = 14000.0 + 3500.0 * std::sin(0.29 * elapsedSeconds + 0.8);
    target->ir_r = 26000.0 + 5500.0 * std::sin(0.17 * elapsedSeconds + 1.1);
}

void initializeCommon(Plane *plane, Target *target) {
    *plane = {};
    plane->location[0] = BaseLatitude;
    plane->location[1] = BaseLongitude;
    plane->location[2] = 3200.0;
    plane->euler[0] = 0.75;
    plane->velocity[0] = 720.0;

    *target = {};
    setTargetPositions(0.0, 0.0, target);
    target->iz_theta1 = -0.55;
    target->iz_theta2 = 0.70;
    target->iz_r1 = 1800.0;
    target->iz_r2 = 18000.0;
    // Preserve the original default sender contract documented for scenarios
    // that do not explicitly animate the IR range.
    target->ir_r = 26'000.0;
}

void initializeDateline(Plane *plane, Target *target) {
    initializeCommon(plane, target);
    plane->location[1] = Pi - 0.00045;
    target->iz_pos[1] = Pi - 0.00020;
    target->ir_pos[1] = Pi - 0.00015;
}

void initializePolarLarStress(Plane *plane, Target *target) {
    initializeCommon(plane, target);
    plane->location[0] = 76.0 * DegreesToRadians;
    plane->location[1] = -30.0 * DegreesToRadians;
    setLarCenterPair(80.0 * DegreesToRadians, 12.0 * DegreesToRadians, 0.0, 0.0, target);
    target->iz_theta1 = 0.0;
    target->iz_theta2 = 0.0;
    target->iz_r1 = 0.0;
    target->iz_r2 = 2'100'000.0;
    target->ir_r = 2'300'000.0;
}

void initializePoleCrossingRoute(Plane *plane, Target *target) {
    initializeCommon(plane, target);
    plane->location[0] = 70.0 * DegreesToRadians;
    plane->location[1] = BaseLongitude;
    plane->location[2] = 5000.0;
    plane->euler[0] = 0.0;
}

void updateDefault(int packetIndex, double, Plane *plane, Target *) {
    plane->location[1] = wrapLongitude(BaseLongitude + double(packetIndex + 1) * 0.000001);
    plane->euler[0] = wrapPositiveAngle(0.75 + double(packetIndex + 1) * 0.01);
}

void updateSteady(int, double, Plane *, Target *) {}

void updateStraightLine(int, double elapsedSeconds, Plane *plane, Target *) {
    setPosition(BaseLatitude, BaseLongitude, 0.0, 200.0 * elapsedSeconds, 3200.0, plane->location);
    plane->euler[0] = Pi * 0.5;
    plane->velocity[0] = 720.0;
}

void updateCoordinatedTurn(int, double elapsedSeconds, Plane *plane, Target *) {
    constexpr double Radius = 6000.0;
    constexpr double AngularRate = 0.035;
    const double phase = AngularRate * elapsedSeconds;
    setPosition(BaseLatitude, BaseLongitude, Radius * std::cos(phase), Radius * std::sin(phase),
                3300.0, plane->location);
    plane->euler[0] = wrapPositiveAngle(phase + Pi * 0.5);
    plane->euler[1] = 0.01 * std::sin(phase * 2.0);
    plane->euler[2] = std::atan((Radius * AngularRate * AngularRate) / 9.80665);
    plane->velocity[0] = Radius * AngularRate * 3.6;
}

void updateFigureEight(int, double elapsedSeconds, Plane *plane, Target *) {
    constexpr double AngularRate = 0.045;
    const double phase = AngularRate * elapsedSeconds;
    const double north = 7000.0 * std::sin(phase * 2.0);
    const double east = 11000.0 * std::sin(phase);
    const double northRate = 14000.0 * AngularRate * std::cos(phase * 2.0);
    const double eastRate = 11000.0 * AngularRate * std::cos(phase);
    setPosition(BaseLatitude, BaseLongitude, north, east, 3200.0 + 450.0 * std::sin(phase * 3.0),
                plane->location);
    plane->euler[0] = wrapPositiveAngle(std::atan2(eastRate, northRate));
    plane->euler[1] = 0.10 * std::cos(phase * 3.0);
    plane->euler[2] = 0.42 * std::sin(phase * 2.0);
    plane->velocity[0] = std::hypot(northRate, eastRate) * 3.6;
    plane->velocity[2] = 450.0 * 3.0 * AngularRate * std::cos(phase * 3.0) * 3.6;
}

void updateMovingTarget(int, double elapsedSeconds, Plane *, Target *target) {
    const double phase = 0.055 * elapsedSeconds;
    setTargetPositions(6500.0 * std::cos(phase), 9000.0 * std::sin(phase), target);
    target->iz_theta1 = -0.70 + 0.18 * std::sin(phase * 2.0);
    target->iz_theta2 = 0.78 + 0.15 * std::cos(phase * 1.5);
}

void updateCrossingTarget(int, double elapsedSeconds, Plane *plane, Target *target) {
    setPosition(BaseLatitude, BaseLongitude, 0.0, 175.0 * elapsedSeconds, 3200.0, plane->location);
    plane->euler[0] = Pi * 0.5;
    plane->velocity[0] = 630.0;
    const double targetNorth = 14000.0 * triangleWave(0.028 * elapsedSeconds);
    const double targetEast = 175.0 * elapsedSeconds + 2500.0;
    setTargetPositions(targetNorth, targetEast, target);
}

void updatePulsingLar(int, double elapsedSeconds, Plane *, Target *target) {
    setLarPulse(elapsedSeconds, target);
}

void updateDateline(int, double elapsedSeconds, Plane *plane, Target *target) {
    const double planeEast = 260.0 * elapsedSeconds;
    const double targetEast = 110.0 * elapsedSeconds;
    setPosition(BaseLatitude, Pi - 0.00045, 900.0 * std::sin(0.02 * elapsedSeconds), planeEast,
                3200.0, plane->location);
    setLarCenterPair(TargetLatitude, Pi - 0.00015, 0.0, targetEast, target);
    plane->euler[0] = Pi * 0.5;
    plane->velocity[0] = 936.0;
}

void updatePolarLarStress(int, double elapsedSeconds, Plane *plane, Target *target) {
    const double phase = 0.12 * elapsedSeconds;
    const double longitudePhase = 0.045 * elapsedSeconds;
    plane->location[0] = (77.0 + 4.0 * std::sin(phase * 0.7)) * DegreesToRadians;
    plane->location[1] = wrapLongitude(-30.0 * DegreesToRadians + longitudePhase);
    plane->euler[0] = wrapPositiveAngle(0.45 + 0.35 * std::sin(phase));

    setLarCenterPair((80.0 + 3.5 * std::sin(phase * 0.83)) * DegreesToRadians,
                     wrapLongitude(12.0 * DegreesToRadians + longitudePhase * 0.8), 0.0, 0.0,
                     target);

    target->ir_r = 2'050'000.0 + 1'050'000.0 * std::sin(phase * 1.13 + 0.4);

    target->iz_theta1 = 0.0;
    target->iz_theta2 = 0.0;
    target->iz_r1 = 0.0;
    target->iz_r2 = 1'850'000.0 + 1'150'000.0 * std::sin(phase * 0.97 + 1.0);
}

void updatePoleCrossingRoute(int packetIndex, double, Plane *plane, Target *) {
    const double phase = std::remainder(
        70.0 * DegreesToRadians + static_cast<double>(packetIndex) * 5.0 * DegreesToRadians, TwoPi);
    const double direction = std::cos(phase);
    plane->location[0] = std::asin(std::sin(phase));
    plane->location[1] = wrapLongitude(BaseLongitude + (direction < 0.0 ? Pi : 0.0));
    plane->location[2] = 5000.0 + 250.0 * std::sin(phase * 2.0);
    plane->euler[0] = direction < 0.0 ? Pi : 0.0;
    plane->velocity[0] = 900.0;
}

void updateMixedHighDynamics(int, double elapsedSeconds, Plane *plane, Target *target) {
    const double phase = 0.085 * elapsedSeconds;
    const double north = 12500.0 * std::sin(phase * 1.7);
    const double east = 18000.0 * std::sin(phase);
    const double northRate = 12500.0 * 1.7 * 0.085 * std::cos(phase * 1.7);
    const double eastRate = 18000.0 * 0.085 * std::cos(phase);

    setPosition(BaseLatitude, BaseLongitude, north, east, 4200.0 + 1300.0 * std::sin(phase * 2.3),
                plane->location);

    plane->euler[0] = wrapPositiveAngle(std::atan2(eastRate, northRate));
    plane->euler[1] = 0.24 * std::sin(phase * 2.3);
    plane->euler[2] = 0.68 * std::sin(phase * 1.7 + 0.5);
    plane->velocity[0] = std::hypot(northRate, eastRate) * 3.6;
    plane->velocity[1] = 180.0 * std::sin(phase * 1.3);
    plane->velocity[2] = 1300.0 * 2.3 * 0.085 * std::cos(phase * 2.3) * 3.6;
    setTargetPositions(10000.0 * std::sin(phase * 0.73 + 1.0), 15000.0 * std::cos(phase * 0.61),
                       target);
    setLarPulse(elapsedSeconds, target);
}

void initializeDlzHeadOn(dlz::TelemetryInputs *inputs) {
    if (inputs)
        *inputs = {20.0, 0.0, 30000.0};
}

void initializeDlzBeam(dlz::TelemetryInputs *inputs) {
    if (inputs)
        *inputs = {20.0, 90.0, 30000.0};
}

void initializeDlzTail(dlz::TelemetryInputs *inputs) {
    if (inputs)
        *inputs = {13.0, 180.0, 55000.0};
}

void initializeDlzInsideRmin(dlz::TelemetryInputs *inputs) {
    if (inputs)
        *inputs = {1.2, 0.0, 30000.0};
}

void initializeDlzFar(dlz::TelemetryInputs *inputs) {
    if (inputs)
        *inputs = {60.0, 0.0, 30000.0};
}

void initializeDlzAspectSweep(dlz::TelemetryInputs *inputs) {
    if (inputs)
        *inputs = {50.0, 0.0, 55000.0};
}

void updateDlzSteady(int, double, dlz::TelemetryInputs *) {}

void updateDlzAspectSweep(int, double elapsedSeconds, dlz::TelemetryInputs *inputs) {
    if (!inputs)
        return;
    constexpr double SweepDurationSeconds = 8.0;
    inputs->aspectDegrees =
        std::clamp(180.0 * std::max(0.0, elapsedSeconds) / SweepDurationSeconds, 0.0, 180.0);
}

const QVector<DlzScenarioDefinition> &dlzDefinitions() {
    static const QVector<DlzScenarioDefinition> Definitions = {
        {QStringLiteral("dlz-head-on-20"),
         QStringLiteral("DLZ head-on geometry: 20 nm, 0 degrees, 30 kft"), 100, initializeDlzHeadOn,
         updateDlzSteady},
        {QStringLiteral("dlz-beam-20"),
         QStringLiteral("DLZ beam geometry: 20 nm, 90 degrees, 30 kft"), 100, initializeDlzBeam,
         updateDlzSteady},
        {QStringLiteral("dlz-tail-chase-13"),
         QStringLiteral("DLZ tail chase geometry: 13 nm, 180 degrees, 55 kft"), 100,
         initializeDlzTail, updateDlzSteady},
        {QStringLiteral("dlz-inside-rmin"),
         QStringLiteral("DLZ inside-minimum-range geometry: 1.2 nm, 0 degrees, 30 kft"), 100,
         initializeDlzInsideRmin, updateDlzSteady},
        {QStringLiteral("dlz-far-60"),
         QStringLiteral("DLZ far-range geometry: 60 nm, 0 degrees, 30 kft"), 100, initializeDlzFar,
         updateDlzSteady},
        {QStringLiteral("dlz-aspect-sweep"),
         QStringLiteral("DLZ 50 nm aspect sweep from 0 to 180 degrees"), 100,
         initializeDlzAspectSweep, updateDlzAspectSweep},
    };
    return Definitions;
}

const DlzScenarioDefinition *findDlzDefinition(const QString &name) {
    const auto &all = dlzDefinitions();
    const auto found =
        std::find_if(all.cbegin(), all.cend(), [&name](const DlzScenarioDefinition &definition) {
            return definition.name == name;
        });
    return found == all.cend() ? nullptr : &*found;
}

const QVector<ScenarioDefinition> &definitions() {
    static const QVector<ScenarioDefinition> Definitions = {
        {QStringLiteral("default"),
         QStringLiteral("Original slow eastward drift and heading sweep"), 100, initializeCommon,
         updateDefault},
        {QStringLiteral("steady"), QStringLiteral("Fixed aircraft, target, attitude, and LAR"), 100,
         initializeCommon, updateSteady},
        {QStringLiteral("straight-line"), QStringLiteral("Constant-speed eastbound aircraft track"),
         100, initializeCommon, updateStraightLine},
        {QStringLiteral("coordinated-turn"),
         QStringLiteral("Level-speed circular turn with matching roll and heading"), 100,
         initializeCommon, updateCoordinatedTurn},
        {QStringLiteral("figure-eight"),
         QStringLiteral("Aircraft figure-eight with changing heading, pitch, and roll"), 100,
         initializeCommon, updateFigureEight},
        {QStringLiteral("moving-target"),
         QStringLiteral("Orbiting target and LAR centers around a steady aircraft"), 100,
         initializeCommon, updateMovingTarget},
        {QStringLiteral("crossing-target"),
         QStringLiteral("Target repeatedly crosses an eastbound aircraft track"), 100,
         initializeCommon, updateCrossingTarget},
        {QStringLiteral("pulsing-lar"),
         QStringLiteral("Independent deterministic IZ angles and IZ/IR range pulses"), 100,
         initializeCommon, updatePulsingLar},
        {QStringLiteral("dateline-crossing"),
         QStringLiteral("Aircraft and target repeatedly wrap longitude at +/-pi"), 100,
         initializeDateline, updateDateline},
        {QStringLiteral("polar-lar-stress"),
         QStringLiteral("Moving near-pole IR and full-circle IZ zones with independently changing "
                        "multi-thousand-kilometer radii"),
         50, initializePolarLarStress, updatePolarLarStress},
        {QStringLiteral("pole-crossing-route"),
         QStringLiteral("Aircraft great-circle route repeatedly crossing both poles"), 50,
         initializePoleCrossingRoute, updatePoleCrossingRoute},
        {QStringLiteral("mixed-high-dynamics"),
         QStringLiteral("Combined aircraft, target, attitude, velocity, and LAR motion"), 100,
         initializeCommon, updateMixedHighDynamics},
        {QStringLiteral("hundred-hz"),
         QStringLiteral("Mixed high-dynamics traffic at a default rate of 100 packets/second"), 10,
         initializeCommon, updateMixedHighDynamics},
    };
    return Definitions;
}

const ScenarioDefinition *findDefinition(const QString &name) {
    const auto &all = definitions();

    const auto found =
        std::find_if(all.cbegin(), all.cend(), [&name](const ScenarioDefinition &definition) {
            return definition.name == name;
        });

    return found == all.cend() ? nullptr : &*found;
}

} // namespace

QStringList names() {
    QStringList result;
    result.reserve(definitions().size() + dlzDefinitions().size());
    for (const ScenarioDefinition &definition : definitions())
        result.append(definition.name);
    for (const DlzScenarioDefinition &definition : dlzDefinitions())
        result.append(definition.name);
    return result;
}

QString description(const QString &name) {
    const ScenarioDefinition *definition = findDefinition(name);
    if (definition)
        return definition->description;
    const DlzScenarioDefinition *dlzDefinition = findDlzDefinition(name);
    return dlzDefinition ? dlzDefinition->description : QString{};
}

bool contains(const QString &name) {
    return findDefinition(name) != nullptr || findDlzDefinition(name) != nullptr;
}

int defaultIntervalMs(const QString &name) {
    const ScenarioDefinition *definition = findDefinition(name);
    if (definition)
        return definition->defaultIntervalMs;
    const DlzScenarioDefinition *dlzDefinition = findDlzDefinition(name);
    return dlzDefinition ? dlzDefinition->defaultIntervalMs : 100;
}

void initialize(const QString &name, Plane *plane, Target *target,
                dlz::TelemetryInputs *dlzInputs) {
    if (!plane || !target)
        return;
    const ScenarioDefinition *definition = findDefinition(name);
    if (definition && definition->initialize) {
        definition->initialize(plane, target);
        if (dlzInputs)
            *dlzInputs = {20.0, 0.0, 30000.0};
        return;
    }
    const DlzScenarioDefinition *dlzDefinition = findDlzDefinition(name);
    if (dlzDefinition) {
        // DLZ-only mappings do not require LAR values, but a valid common
        // baseline keeps the sender safe when a combined mapping is selected.
        initializeCommon(plane, target);
        if (dlzInputs && dlzDefinition->initialize)
            dlzDefinition->initialize(dlzInputs);
    }
}

void update(const QString &name, int packetIndex, double elapsedSeconds, Plane *plane,
            Target *target, dlz::TelemetryInputs *dlzInputs) {
    if (!plane || !target)
        return;
    const ScenarioDefinition *definition = findDefinition(name);
    if (definition && definition->update) {
        target->time = elapsedSeconds;
        definition->update(packetIndex, elapsedSeconds, plane, target);
        return;
    }
    const DlzScenarioDefinition *dlzDefinition = findDlzDefinition(name);
    if (dlzDefinition) {
        target->time = elapsedSeconds;
        if (dlzInputs && dlzDefinition->update)
            dlzDefinition->update(packetIndex, elapsedSeconds, dlzInputs);
    }
}

} // namespace TestSenderScenarios

//./build-release/lar-test-sender --map maps/full-state-dlz.json --scenario coordinated-turn --host
// 127.0.0.1 --port 45454 --count 100000 --interval 2
//./build-release/lar-test-sender --map maps/full-state.json --scenario polar-lar-stress --host
// 127.0.0.1 --port 45454 --count 100000 --interval 2

//./build-release/lar-test-sender --map maps/full-state-dlz.json --scenario dlz-aspect-sweep --host
// 127.0.0.1 --port 45454 --count 100000 --interval 2
