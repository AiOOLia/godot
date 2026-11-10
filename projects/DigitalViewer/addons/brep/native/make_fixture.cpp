// Reproducible face with a square hole, serialized by the existing BRepCore DLL.
#include <BRepFile.h>
#include <cmath>
#include <filesystem>
#include <iostream>
using namespace ModelTransfer;
int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) return 2;
    const std::string mode = argc == 3 ? argv[2] : "planar-hole";
    const bool split = mode == "split-shell" || mode == "split-solid";
    auto face = std::make_shared<NDSFace>();
    face->persistentId = 42; face->area = split ? 8 : 84; face->perimeter = split ? 16 : 56;
    auto plane = std::make_unique<NDSPlane>();
    for (int i = 0; i < 3; ++i) {
        plane->origin[i] = 0; plane->normal[i] = i == 2 ? 1 : 0; plane->axisX[i] = i == 0 ? 1 : 0;
    }
    const double z = mode == "nurbs" ? 0.12345678901234567 : 0.0;
    if (mode == "nurbs") {
        auto surface = std::make_unique<NDSBSplineSurface>();
        surface->udegree = surface->vdegree = 1; surface->uPoleCount = surface->vPoleCount = 2;
        surface->poles = {0,0,z, 0,10,z, 10,0,z, 10,10,z};
        surface->weights = {1,1,1,1}; surface->uknots = surface->vknots = {0,1};
        surface->umults = surface->vmults = {2,2}; face->pSurf = std::move(surface);
    } else face->pSurf = std::move(plane);
    if (mode == "zero-id") face->persistentId = 0;
    auto ring = [&](std::vector<std::array<double, 3>> points) {
        std::vector<std::shared_ptr<NDSVertex>> vertices;
        for (auto p : points) {
            auto v = std::make_shared<NDSVertex>(); v->point = p; v->tolerance = 1e-7; vertices.push_back(v);
        }
        auto wire = std::make_unique<NDSWire>();
        for (size_t i = 0; i < points.size(); ++i) {
            auto edge = std::make_shared<NDSEdge>();
            edge->startVertex = vertices[i]; edge->endVertex = vertices[(i+1)%points.size()]; edge->tolerance = 1e-7;
            auto line = std::make_unique<NDSLine>();
            double length = 0;
            for (int j = 0; j < 3; ++j) { double d = edge->endVertex->point[j]-edge->startVertex->point[j]; length += d*d; }
            edge->length = std::sqrt(length);
            for (int j = 0; j < 3; ++j) {
                line->origin[j] = points[i][j]; line->direction[j] = (edge->endVertex->point[j]-points[i][j])/edge->length;
            }
            edge->pCurve = std::move(line);
            auto use = std::make_shared<NDSDirectedEdge>(); use->edge = edge;
            wire->edges.push_back(std::move(use));
        }
        face->wires.push_back(std::move(wire));
    };
    if (mode == "curved-edges") {
        face->area = 21 * std::acos(-1.0);
        face->perimeter = 14 * std::acos(-1.0);
        for (double radius : {5.0, 2.0}) {
            auto vertex = std::make_shared<NDSVertex>(); vertex->point = {radius, 0, 0}; vertex->tolerance = 1e-7;
            auto edge = std::make_shared<NDSEdge>(); edge->startVertex = edge->endVertex = vertex;
            edge->length = 2 * std::acos(-1.0) * radius; edge->tolerance = 1e-7;
            edge->hasTrimDomain = true; edge->trimMin = 0; edge->trimMax = 2 * std::acos(-1.0);
            auto circle = std::make_unique<NDSCircle>(); circle->radius = radius;
            for (int i = 0; i < 3; ++i) {
                circle->center[i] = 0; circle->normal[i] = i == 2 ? 1 : 0; circle->axisX[i] = i == 0 ? 1 : 0;
            }
            edge->pCurve = std::move(circle);
            auto use = std::make_shared<NDSDirectedEdge>(); use->edge = edge; use->reverse = radius == 2.0;
            auto wire = std::make_unique<NDSWire>(); wire->edges.push_back(use); face->wires.push_back(std::move(wire));
        }
    } else if (mode == "shared-edge") {
        face->area = 1; face->perimeter = 4;
        ring({{0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}});
    } else if (split) {
        // Two disconnected regions in one source face, matching the NDS repair regression.
        ring({{-4,-1,0}, {-2,-1,0}, {-2,1,0}, {-4,1,0}});
        ring({{2,-1,0}, {4,-1,0}, {4,1,0}, {2,1,0}});
    } else {
        ring({{0,0,z}, {10,0,z}, {10,10,z}, {0,10,z}});
        ring({{3,3,z}, {3,7,z}, {7,7,z}, {7,3,z}});
    }
    auto use = std::make_unique<NDSDirectedFace>(); use->face = face; use->reverse = false;
    auto shell = std::make_unique<NDSShell>(); shell->area = split ? 8 : 84; shell->faces.push_back(std::move(use));
    std::unique_ptr<NDSShell> second_shell;
    if (mode == "shared-edge") {
        auto shared = (*std::next(face->wires.front()->edges.begin()))->edge;
        face = std::make_shared<NDSFace>(); face->persistentId = 43; face->area = 1; face->perimeter = 4;
        auto surface = std::make_unique<NDSPlane>();
        for (int i = 0; i < 3; ++i) {
            surface->origin[i] = 0; surface->normal[i] = i == 2 ? 1 : 0; surface->axisX[i] = i == 0 ? 1 : 0;
        }
        face->pSurf = std::move(surface);
        ring({{1,0,0}, {2,0,0}, {2,1,0}, {1,1,0}});
        face->wires.front()->edges.back()->edge = shared;
        face->wires.front()->edges.back()->reverse = true;
        auto use = std::make_unique<NDSDirectedFace>(); use->face = face; shell->faces.push_back(std::move(use));
        shell->area = 2;
    }
    if (mode == "duplicate-id" || mode == "shell-duplicate-id") {
        face = std::make_shared<NDSFace>(); face->persistentId = 42;
        auto plane2 = std::make_unique<NDSPlane>();
        for (int i = 0; i < 3; ++i) {
            plane2->origin[i] = i == 2 ? 5 : 0; plane2->normal[i] = i == 2 ? 1 : 0; plane2->axisX[i] = i == 0 ? 1 : 0;
        }
        face->pSurf = std::move(plane2); face->area = 100; face->perimeter = 40;
        ring({{0,0,5}, {10,0,5}, {10,10,5}, {0,10,5}});
        auto second = std::make_unique<NDSDirectedFace>(); second->face = face; second->reverse = false;
        if (mode == "shell-duplicate-id") {
            second_shell = std::make_unique<NDSShell>(); second_shell->area = 100; second_shell->faces.push_back(std::move(second));
        } else shell->faces.push_back(std::move(second));
    }
    NDSBRep brep;
    brep.unitToMillimeters = mode == "units" ? 25.4 : 1.0;
    if (mode == "split-solid") {
        auto solid = std::make_unique<NDSSolid>(); solid->volume = 0;
        solid->shells.push_back(std::move(shell)); brep.solids.push_back(std::move(solid));
    } else brep.shells.push_back(std::move(shell));
    if (second_shell) brep.shells.push_back(std::move(second_shell));
    std::string error;
    if (mode == "tessellation") {
        CADModel model;
        model.bRep = std::move(brep);
        model.root.name = "Tessellated hole";
        model.root.type = OT_BODY; model.root.bRepBodyType = BRT_SHELL; model.root.repBodyIndex = 0;
        auto triangles = std::make_unique<Tessellation>();
        triangles->id = 101; triangles->primitiveType = PrimitiveType::PT_Triangles;
        triangles->vertices = {0,0,0, 10,0,0, 10,10,0, 0,10,0, 3,3,0, 3,7,0, 7,7,0, 7,3,0};
        triangles->indices = {0,1,7, 0,7,4, 1,2,6, 1,6,7, 2,3,5, 2,5,6, 3,0,4, 3,4,5};
        // Omit normals to exercise buffer-only normal generation in the viewer.
        auto source_face = model.bRep.shells.front()->faces.front()->face;
        triangles->sourceFaces = {source_face}; triangles->brepPrimitiveGroup = {0};
        model.root.faceTessIds = {101};
        auto lines = std::make_unique<Tessellation>();
        lines->id = 102; lines->primitiveType = PrimitiveType::PT_Lines;
        lines->vertices = triangles->vertices;
        for (const auto &wire : source_face->wires) for (const auto &ref : wire->edges) {
            lines->sourceEdges.push_back(ref->edge);
            lines->brepPrimitiveGroup.push_back(static_cast<uint32_t>(lines->indices.size()));
            auto a = lines->sourceEdges.size()-1;
            lines->indices.push_back(static_cast<uint32_t>(a));
            lines->indices.push_back(static_cast<uint32_t>(a/4*4+(a+1)%4));
        }
        model.root.edgeTessIds = {102};
        model.tessellations.push_back(std::move(triangles)); model.tessellations.push_back(std::move(lines));
        if (!BRepFile::write(model, std::filesystem::path(argv[1]), mode.c_str(), error)) { std::cerr << error << '\n'; return 1; }
    } else if (!BRepFile::write(brep, argv[1], mode.c_str(), error)) { std::cerr << error << '\n'; return 1; }
    ParsedBRepData parsed;
    if (!BRepFile::parse(std::filesystem::path(argv[1]) / (mode + ".brep"), parsed, error)) { std::cerr << error << '\n'; return 1; }
    if (parsed.unitToMillimeters != (mode == "tessellation" ? 1.0 : brep.unitToMillimeters) || parsed.versionMinor != 3) return 3;
    std::cout << "BREP_FIXTURE_OK mode=" << mode << " faces=" << parsed.faces.size() << " unit=" << parsed.unitToMillimeters << '\n';
}
