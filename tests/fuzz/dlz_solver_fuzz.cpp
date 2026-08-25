#include "domain/dlz/dlz_solver.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    std::array<double, 32> values{};
    std::memcpy(values.data(), data, std::min(size, sizeof(values)));

    dlz::ShooterState shooter;
    shooter.pos = {values[0], values[1], values[2]};
    shooter.vel = {values[3], values[4], values[5]};
    shooter.mach = values[6];
    shooter.psi = values[7];
    shooter.theta = values[8];
    shooter.phi = values[9];

    dlz::TargetState target;
    target.pos = {values[10], values[11], values[12]};
    target.vel = {values[13], values[14], values[15]};
    target.mach = values[16];
    target.psi = values[17];
    target.nzDefensive = values[18];

    dlz::Atmosphere atmosphere{values[19], values[20], values[21], values[22]};
    dlz::Geometry geometry;
    geometry.rangeNm = values[23];
    geometry.rangeRateKnots = values[24];
    geometry.aspectRadians = values[25];
    geometry.altitudeDifferenceFeet = values[26];
    dlz::WeaponModel weapon{values[27], values[28], values[29], values[30], values[31]};
    (void)dlz::solve(shooter, target, atmosphere, geometry, weapon);
    return 0;
}
