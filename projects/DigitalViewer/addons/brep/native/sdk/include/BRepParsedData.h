#pragma once

#include "BRep.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace ModelTransfer
{
    inline constexpr std::uint32_t BREP_INVALID_INDEX =
        (std::numeric_limits<std::uint32_t>::max)();

    struct BRepDirectedRef
    {
        std::uint32_t index = BREP_INVALID_INDEX;
        bool reverse = false;
    };

    struct BRepEdgeRef : BRepDirectedRef
    {
        bool seamWithFace = false;
    };

    struct BRepVertexData
    {
        std::array<double, 3> point = {0.0, 0.0, 0.0};
        double tolerance = 0.0;
    };

    struct BRepEdgeData
    {
        std::uint32_t startVertexIndex = BREP_INVALID_INDEX;
        std::uint32_t endVertexIndex = BREP_INVALID_INDEX;
        double length = 0.0;
        std::uint32_t persistentId = 0;
        bool sameSense = true;
        double tolerance = 0.0;
        bool degenerated = false;
        std::unique_ptr<NDSCurve> curve;
    };

    struct BRepWireData
    {
        std::vector<BRepEdgeRef> edges;
    };

    struct BRepFaceData
    {
        std::uint32_t persistentId = 0;
        // In source coordinate units squared. Negative means unavailable.
        double area = -1.0;
        double perimeter = 0.0;
        std::unique_ptr<NDSSurface> surface;
        std::vector<BRepDirectedRef> wires;
    };

    struct BRepShellData
    {
        double area = 0.0;
        std::vector<BRepDirectedRef> faces;
    };

    struct BRepSolidData
    {
        double volume = 0.0;
        std::vector<std::uint32_t> shells;
    };

    struct ParsedBRepData
    {
        std::uint8_t versionMajor = 0;
        std::uint8_t versionMinor = 0;
        std::uint8_t versionPatch = 0;
        std::uint8_t versionFlags = 0;
        // Millimeters per geometry-coordinate unit; 0 for unknown/legacy data.
        double unitToMillimeters = 0.0;
        std::vector<BRepVertexData> vertices;
        std::vector<BRepEdgeData> edges;
        std::vector<BRepWireData> wires;
        std::vector<BRepFaceData> faces;
        std::vector<BRepShellData> shells;
        std::vector<BRepSolidData> solids;
    };
}
