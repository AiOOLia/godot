#pragma once

#include <Precision.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>

// All distances are expressed in the coordinates of the input BRep, not mm.
struct BRepReconstructionOptions
{
    // Lower bound for topology tolerances; preserve larger source/measured errors.
    double minTopologyTolerance = Precision::Confusion();
    // Algorithm target for projection, parameter/line correction and face repair.
    double numericalPrecision = Precision::Confusion();
    // Independent permission for healing, NOT a topology tolerance to assign.
    // Like OCC read.maxprecision.val, expressed in the working coordinate unit.
    // Explicit options use this value as-is; default builders use defaultsForUnit.
    double maxRepairTolerance = 1.0;

    static BRepReconstructionOptions defaultsForUnit(double unitToMillimeters)
    {
        if (!std::isfinite(unitToMillimeters) || unitToMillimeters < 0.0)
            throw std::invalid_argument("unitToMillimeters must be finite and non-negative");
        BRepReconstructionOptions result;
        if (unitToMillimeters > 0.0)
        {
            // OCC's default allowance is 1 mm. Convert only the allowance,
            // keeping geometry and the numerical/topology precision unchanged.
            const double limit = 1.0 / unitToMillimeters;
            if (!std::isfinite(limit))
                throw std::invalid_argument("1 mm repair allowance overflows the BRep coordinate unit");
            result.maxRepairTolerance = std::max({limit,
                result.minTopologyTolerance, result.numericalPrecision});
        }
        // Unknown/legacy units: retain the old 1-coordinate-unit allowance.
        // This fallback does not assert that the coordinates are millimeters.
        return result;
    }

    void validate() const
    {
        if (!std::isfinite(minTopologyTolerance) || minTopologyTolerance < Precision::Confusion())
            throw std::invalid_argument("minTopologyTolerance must be finite and >= OCC Confusion (1e-7)");
        if (!std::isfinite(numericalPrecision) || numericalPrecision < Precision::Confusion())
            throw std::invalid_argument("numericalPrecision must be finite and >= OCC Confusion (1e-7)");
        if (!std::isfinite(maxRepairTolerance) ||
            maxRepairTolerance < std::max(minTopologyTolerance, numericalPrecision))
            throw std::invalid_argument("maxRepairTolerance must be finite and >= topology/numerical precision");
    }

    double initialTolerance(double sourceTolerance = 0.0) const
    {
        return std::max(minTopologyTolerance, sourceTolerance);
    }

    double repairLimit(double existingTolerance) const
    {
        // Never truncate evidence already recorded on a local edge or vertex.
        return std::max(maxRepairTolerance, initialTolerance(existingTolerance));
    }
};
