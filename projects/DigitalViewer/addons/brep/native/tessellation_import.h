#pragma once
#include <BRepFile.h>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <functional>
#include <cmath>
#include <stdexcept>
#include <map>
#include <set>
#include <tuple>

// BRepCore owns GEOM parsing/validation. This adapter only maps SDK buffers and
// source topology groups to Godot meshes; it never constructs an OCC shape.
namespace dv_brep {
using namespace godot;
using namespace ModelTransfer;
inline Dictionary import_tessellation(const std::filesystem::path &path,
        const ParsedBRepData &data, const ModelTransfer::Object *root, const std::function<void()> &checkpoint) {
    std::list<std::unique_ptr<Tessellation>> tessellations;
    std::string error;
    if (std::filesystem::file_size(path) > 256 * 1024 * 1024)
        throw std::runtime_error("GEOM exceeds the 256 MiB limit.");
    if (!BRepFile::read(path, tessellations, error)) throw std::runtime_error(error);
    std::map<uint32_t, const Tessellation *> by_id;
    for (const auto &t : tessellations) {
        checkpoint();
        if (!t || !by_id.emplace(t->id, t.get()).second) throw std::runtime_error("Duplicate GEOM ID.");
        for (auto f : t->brepEntityIndices)
            if (f >= (t->primitiveType == PrimitiveType::PT_Triangles ? data.faces.size() : data.edges.size()))
                throw std::runtime_error("GEOM topology index is out of range.");
    }
    Array bodies, nodes;
    int triangles = 0, lines = 0, total_vertices = 0;
    std::map<std::tuple<int, uint32_t, uint32_t>, int> cache;
    struct Face { int shell = -1, solid = -1; bool reverse = false; };
    auto append = [&](int type, uint32_t index, const ModelTransfer::Object *object) {
        std::map<uint32_t, Face> faces;
        std::set<uint32_t> edges, covered_faces, covered_edges;
        auto wire = [&](uint32_t w) { for (const auto &ref : data.wires.at(w).edges) edges.insert(ref.index); };
        auto shell = [&](uint32_t s, int solid) {
            for (auto ref : data.shells.at(s).faces) {
                if (!faces.emplace(ref.index, Face{static_cast<int>(s), solid, ref.reverse}).second)
                    throw std::runtime_error("Ambiguous GEOM face occurrence.");
                for (auto w : data.faces.at(ref.index).wires) wire(w.index);
            }
        };
        if (type == static_cast<int>(ModelTransfer::BRT_SOLID)) for (auto s : data.solids.at(index).shells) shell(s, index);
        else if (type == static_cast<int>(ModelTransfer::BRT_SHELL)) shell(index, -1);
        else if (type == static_cast<int>(ModelTransfer::BRT_WIRE)) wire(index);
        else if (type == 4) {
            faces.emplace(index, Face{});
            for (auto w : data.faces.at(index).wires) wire(w.index);
        } else if (type != static_cast<int>(ModelTransfer::BRT_UNKNOWN)) throw std::runtime_error("Unknown GEOM body type.");
        PackedInt32Array references;
        auto add = [&](uint32_t id, bool face_mesh, bool strict) {
            checkpoint();
            auto found = by_id.find(id);
            if (found == by_id.end() || (found->second->primitiveType == PrimitiveType::PT_Triangles) != face_mesh)
                throw std::runtime_error("Missing or wrong-type GEOM ID.");
            const auto &t = *found->second;
            std::vector<size_t> groups;
            for (size_t i = 0; i < t.brepEntityIndices.size(); ++i) {
                auto entity = t.brepEntityIndices[i];
                if (face_mesh ? faces.count(entity) != 0 : edges.count(entity) != 0) {
                    groups.push_back(i);
                    if (face_mesh) {
                        if (!covered_faces.insert(entity).second) throw std::runtime_error("Duplicate GEOM face coverage.");
                    } else covered_edges.insert(entity);
                } else if (strict) throw std::runtime_error("GEOM topology is outside the object body.");
            }
            if (groups.empty()) return;
            auto key = std::make_tuple(type, index, id);
            auto existing = cache.find(key);
            if (existing != cache.end()) { references.append(existing->second); return; }
            PackedVector3Array vertices, normals;
            PackedInt32Array indices;
            std::map<uint32_t, int> remap;
            auto vertex = [&](uint32_t source) {
                auto found = remap.find(source);
                if (found != remap.end()) return found->second;
                if (++total_vertices > 6000000) throw std::runtime_error("GEOM exceeds the 6 million display vertex budget.");
                int index = vertices.size();
                vertices.append(Vector3(t.vertices[source*3], t.vertices[source*3+1], t.vertices[source*3+2]));
                normals.append(t.normals.empty() ? Vector3() : Vector3(t.normals[source*3], t.normals[source*3+1], t.normals[source*3+2]));
                return remap[source] = index;
            };
            Array ranges;
            for (auto group : groups) {
                checkpoint();
                uint32_t first = t.brepPrimitiveGroup[group];
                size_t end = group + 1 < t.brepPrimitiveGroup.size() ? t.brepPrimitiveGroup[group+1] : t.indices.size();
                if (face_mesh) {
                    const auto entity = t.brepEntityIndices[group];
                    const auto &face = faces.at(entity);
                    Dictionary range, definition;
                    PackedInt32Array sources; sources.append(entity);
                    definition["source_face_index"] = entity; definition["source_face_indices"] = sources;
                    definition["source_mapping"] = "source_index";
                    range["definition"] = definition; range["face_index"] = entity;
                    range["persistent_id"] = data.faces.at(entity).persistentId;
                    range["body_type"] = type; range["body_index"] = index;
                    range["shell_index"] = face.shell; range["solid_index"] = face.solid;
                    range["result_face_reversed"] = face.reverse;
                    range["triangle_first"] = indices.size() / 3; range["triangle_count"] = int64_t((end-first)/3);
                    ranges.append(range);
                    for (size_t i = first; i < end; i += 3) {
                        const auto a = vertex(t.indices[i]), b = vertex(t.indices[i+1]), c = vertex(t.indices[i+2]);
                        if (t.normals.empty()) {
                            auto n = (vertices[b]-vertices[a]).cross(vertices[c]-vertices[a]);
                            normals.set(a, normals[a]+n); normals.set(b, normals[b]+n); normals.set(c, normals[c]+n);
                        }
                        // Exported winding is already oriented; only adapt CCW to Godot CW.
                        indices.append(a); indices.append(c); indices.append(b);
                    }
                    triangles += (end-first)/3;
                    if (triangles > 2000000) throw std::runtime_error("GEOM exceeds the 2 million triangle preview budget.");
                } else {
                    for (size_t i = first; i < end; ++i) indices.append(vertex(t.indices[i]));
                    lines += (end-first)/2;
                    if (lines > 4000000) throw std::runtime_error("GEOM exceeds the 4 million line preview budget.");
                }
            }
            if (face_mesh && t.normals.empty())
                for (int i = 0; i < normals.size(); ++i) normals.set(i, normals[i].normalized());
            Dictionary body;
            body["vertices"] = vertices; body["normals"] = normals; body["indices"] = indices;
            body["face_ranges"] = ranges; body["body_type"] = type; body["body_index"] = index;
            body["lines"] = !face_mesh; body["tessellation_id"] = id;
            int result = bodies.size(); bodies.append(body); cache[key] = result; references.append(result);
        };
        if (object) {
            std::set<uint32_t> ids;
            for (auto id : object->faceTessIds) {
                if (!ids.insert(id).second) throw std::runtime_error("Duplicate object GEOM ID.");
                add(id, true, true);
            }
            for (auto id : object->edgeTessIds) {
                if (!ids.insert(id).second) throw std::runtime_error("Duplicate object GEOM ID.");
                add(id, false, true);
            }
        } else for (const auto &t : tessellations) add(t->id, t->primitiveType == PrimitiveType::PT_Triangles, false);
        if (covered_faces.size() != faces.size()) throw std::runtime_error("Incomplete GEOM face coverage.");
        if (faces.empty() || (object && !object->edgeTessIds.empty()))
            for (auto e : edges) if (!covered_edges.count(e) && !data.edges.at(e).degenerated)
                throw std::runtime_error("Incomplete GEOM edge coverage.");
        return references;
    };
    if (root) {
        std::function<void(const ModelTransfer::Object &, int, int)> visit;
        visit = [&](const ModelTransfer::Object &o, int parent, int depth) {
            checkpoint();
            if (nodes.size() >= 100000 || depth > 128) throw std::runtime_error("Assembly node/depth budget exceeded.");
            Dictionary node;
            node["parent"] = parent; node["name"] = String::utf8(o.name.c_str());
            node["persistent_id"] = o.persistentId; node["object_type"] = int(o.type);
            PackedFloat64Array matrix;
            for (auto v : o.matrix) { if (!std::isfinite(v)) throw std::runtime_error("Invalid assembly matrix."); matrix.append(v); }
            node["matrix"] = matrix;
            node["source_body_type"] = int(o.bRepBodyType); node["source_body_index"] = o.repBodyIndex;
            node["bodies"] = append(int(o.bRepBodyType), o.repBodyIndex, &o);
            int current = nodes.size(); nodes.append(node);
            for (const auto *child : o.children) if (child) visit(*child, current, depth+1);
        };
        visit(*root, -1, 0);
    } else {
        auto node = [&](int type, uint32_t index) {
            Dictionary n;
            n["parent"] = -1; n["persistent_id"] = 0;
            n["name"] = String(type == static_cast<int>(ModelTransfer::BRT_SOLID) ? "Solid " : type == static_cast<int>(ModelTransfer::BRT_SHELL) ? "Shell " : type == static_cast<int>(ModelTransfer::BRT_WIRE) ? "Wire " : "Face ") + String::num_int64(index);
            n["bodies"] = append(type, index, nullptr); nodes.append(n);
        };
        std::set<uint32_t> owned_shells, owned_faces, owned_wires;
        for (uint32_t i=0; i<data.solids.size(); ++i) { node(static_cast<int>(ModelTransfer::BRT_SOLID),i); for(auto s:data.solids[i].shells) owned_shells.insert(s); }
        for (uint32_t i=0; i<data.shells.size(); ++i) {
            if (!owned_shells.count(i)) node(static_cast<int>(ModelTransfer::BRT_SHELL),i);
            for (auto f:data.shells[i].faces) owned_faces.insert(f.index);
        }
        for (uint32_t i=0; i<data.faces.size(); ++i) if (!owned_faces.count(i)) node(4,i);
        for (const auto &f : data.faces) for (auto w : f.wires) owned_wires.insert(w.index);
        for (uint32_t i=0; i<data.wires.size(); ++i) if (!owned_wires.count(i)) node(static_cast<int>(ModelTransfer::BRT_WIRE),i);
    }
    if (!triangles && !lines) throw std::runtime_error("GEOM contains no displayable geometry.");
    Dictionary stats, output;
    stats["faces"] = int64_t(data.faces.size()); stats["solids"] = int64_t(data.solids.size()); stats["shells"] = int64_t(data.shells.size());
    stats["source_solids"] = stats["solids"]; stats["source_shells"] = stats["shells"];
    stats["unique_bodies"] = bodies.size(); stats["triangles"] = triangles; stats["lines"] = lines; stats["missing_faces"] = 0;
    stats["geometry_source"] = "geom"; stats["occ_reconstructed"] = false;
    output["bodies"] = bodies; output["nodes"] = nodes; output["stats"] = stats;
    return output;
}
}
