#include <pxr/base/gf/vec3f.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <exception>
#include <iostream>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

int main(int argc, char **argv) {
    try {
        PlugRegistry::GetInstance().RegisterPlugins("/usd/lib/usd/plugInfo.json");
        const auto stage = UsdStage::Open(argc > 1 ? argv[1] : "/mesh.usda");
        if (!stage) {
            std::cerr << "FAIL: UsdStage::Open\n";
            return 1;
        }
        const UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/Quad")));
        VtVec3fArray points;
        VtIntArray counts;
        VtIntArray indices;
        if (!mesh || !mesh.GetPointsAttr().Get(&points) ||
                !mesh.GetFaceVertexCountsAttr().Get(&counts) ||
                !mesh.GetFaceVertexIndicesAttr().Get(&indices)) {
            std::cerr << "FAIL: native mesh schema/attribute reads\n";
            return 2;
        }
        const VtVec3fArray expected_points = {
            GfVec3f(0, 0, 0), GfVec3f(1, 0, 0), GfVec3f(1, 1, 0), GfVec3f(0, 1, 0)
        };
        std::string reason;
        if (points != expected_points || counts != VtIntArray({4}) ||
                indices != VtIntArray({0, 1, 2, 3}) ||
                !UsdGeomMesh::ValidateTopology(indices, counts, points.size(), &reason) ||
                UsdGeomGetStageUpAxis(stage) != TfToken("Y") || UsdGeomGetStageMetersPerUnit(stage) != 1.0) {
            std::cerr << "FAIL: mesh values/topology/metadata " << reason << '\n';
            return 3;
        }
        std::cout << "PASS: native OpenUSD USDA points=4 counts=[4] indices=[0,1,2,3]\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 4;
    }
}
