// BREP and assembly parsing and reconstruction stay in the supplied NDS DLLs.
#include <BRepFile.h>
#include <BRepToTopoDS.h>
#include "source_json_selection.h"
#include "tessellation_import.h"
#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>

using namespace godot;
namespace {
std::mutex kernel_mutex;
constexpr size_t max_bytes = 256 * 1024 * 1024;
constexpr size_t max_json_bytes = 16 * 1024 * 1024;
constexpr int max_triangles = 2000000;
String utf8(const std::string &s) { return String::utf8(s.c_str()); }
std::string read_file(const String &path, size_t limit) {
    std::ifstream stream(std::filesystem::u8path(path.utf8().get_data()), std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Cannot open BREP dependency: " + std::string(path.utf8().get_data()));
    auto length = stream.tellg();
    if (length <= 0 || static_cast<size_t>(length) > limit) throw std::runtime_error("Empty file or BREP input size limit exceeded.");
    std::string bytes(static_cast<size_t>(length), '\0');
    stream.seekg(0);
    if (!stream.read(bytes.data(), length)) throw std::runtime_error("Truncated BREP input.");
    return bytes;
}
// Only a depth guard; the existing BRepCore JSON reader remains the parser.
void guard_json(const std::string &text) {
    bool quoted = false, escaped = false;
    int depth = 0;
    for (char c : text) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '[' || c == '{') {
            if (++depth > 256) throw std::runtime_error("BREP JSON exceeds depth limit (256).");
        } else if (c == ']' || c == '}') --depth;
    }
}
}

class BRepDocument : public RefCounted {
    GDCLASS(BRepDocument, RefCounted)
    std::atomic<bool> cancelled{false};
    std::atomic<int> stage{0};
    std::string source_bytes;
    bool source_is_json = false;
    bool direct_geometry = false;
    std::unique_ptr<BRepToTopoDS> reconstructed;
    std::map<int, Dictionary> face_definitions;
    std::map<uint32_t, int> source_export_indices;
    std::map<int, Dictionary> json_cache;
    void checkpoint() const {
        if (cancelled) throw std::runtime_error("BREP import cancelled.");
    }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("import_model", "brep_path", "json_path", "options"), &BRepDocument::import_model);
        ClassDB::bind_method(D_METHOD("cancel"), &BRepDocument::cancel);
        ClassDB::bind_method(D_METHOD("get_stage"), &BRepDocument::get_stage);
        ClassDB::bind_method(D_METHOD("get_face_brep_json", "face_index"), &BRepDocument::get_face_brep_json);
    }
public:
    Dictionary get_face_brep_json(int face_index) {
        std::unique_lock<std::mutex> lock(kernel_mutex);
        Dictionary out;
        out["source_json"] = ""; out["reconstructed_json"] = "";
        out["source_error"] = ""; out["reconstructed_error"] = "";
        if ((!reconstructed && !direct_geometry) || !face_definitions.count(face_index)) {
            out["source_error"] = "BREP face index is out of range or document is unavailable.";
            out["reconstructed_error"] = out["source_error"];
            return out;
        }
        auto cached = json_cache.find(face_index);
        if (cached != json_cache.end()) return cached->second;
        Dictionary definition = face_definitions.at(face_index);
        out["source_mapping"] = definition["source_mapping"];
        out["source_face_indices"] = definition["source_face_indices"];
        std::string json, error;
        try {
            PackedInt32Array indices = definition["source_face_indices"];
            std::vector<int> exported;
            for (int i = 0; i < indices.size(); ++i)
                exported.push_back(source_is_json ? indices[i] : source_export_indices.at(indices[i]));
            if (exported.empty()) out["source_error"] = "No source face mapping is available.";
            else if (source_is_json) {
                if (source_json_faces(source_bytes, exported, json, error)) out["source_json"] = utf8(json);
                else out["source_error"] = utf8(error);
            }
            else {
                ModelTransfer::BRepFile file(source_bytes.data(), static_cast<int>(source_bytes.size()));
                if (file.separateFaceBrepsToJson(json, exported, error)) out["source_json"] = utf8(json);
                else out["source_error"] = utf8(error);
            }
        } catch (const std::exception &e) { out["source_error"] = utf8(e.what()); }
        catch (...) { out["source_error"] = "Source face JSON export failed."; }
        // Always export the selected repaired result, independently of source
        // mapping/export failures. Both strings are the SDK's exact output.
        try {
            json.clear(); error.clear();
            if (direct_geometry) {
                // Display uses only GEOM. Reconstruct the selected source face
                // on demand for its OCC definition, without tessellating it.
                String source = out["source_json"];
                ModelTransfer::ParsedBRepData face_data;
                if (source.is_empty() || !ModelTransfer::BRepFile::parse(source.utf8().get_data(), face_data, error))
                    throw std::runtime_error("Cannot reconstruct selected source face: " + error);
                BRepToTopoDS selected(face_data);
                if (!selected.convert()) throw std::runtime_error(selected.errorMessage());
                std::vector<int> faces;
                for (int i=0; i<selected.numFaces(); ++i) faces.push_back(i);
                if (!selected.separateFaceBrepsToJson(json, faces, error)) throw std::runtime_error(error);
                out["reconstructed_json"] = utf8(json);
            } else if (reconstructed->separateFaceBrepsToJson(json, {face_index}, error)) out["reconstructed_json"] = utf8(json);
            else out["reconstructed_error"] = utf8(error);
        } catch (const Standard_Failure &e) { out["reconstructed_error"] = utf8(e.what()); }
        catch (const std::exception &e) { out["reconstructed_error"] = utf8(e.what()); }
        catch (...) { out["reconstructed_error"] = "Reconstructed face JSON export failed."; }
        if (json_cache.size() >= 16) json_cache.erase(json_cache.begin());
        json_cache[face_index] = out;
        return out;
    }
    void cancel() { cancelled = true; }
    String get_stage() const {
        const char *names[] = {"Queued", "Parsing", "Reconstructing", "Tessellating", "Uploading", "Preparing geometry"};
        return names[stage.load()];
    }
    Dictionary import_model(String path, String json_path, Dictionary options) {
        Dictionary output;
        output["error"] = "";
        try {
            std::unique_lock<std::mutex> lock(kernel_mutex);
            reconstructed.reset(); source_bytes.clear(); face_definitions.clear(); source_export_indices.clear(); json_cache.clear();
            direct_geometry = false;
            checkpoint();
            stage = 1;
            source_is_json = options.get("input_json", false);
            auto bytes = read_file(path, source_is_json ? max_json_bytes : max_bytes);
            if (!source_is_json && (bytes.size() < 8 || bytes.compare(0, 8, std::string("NDSBREP\0", 8)) != 0))
                throw std::runtime_error("Unsupported BREP dialect. Expected NDSBREP binary data (not OCCT text BREP).");
            ModelTransfer::ParsedBRepData parsed;
            ModelTransfer::Object root;
            std::string error;
            if (source_is_json) {
                if (bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) bytes.erase(0, 3);
                if (bytes.find('\0') != std::string::npos) throw std::runtime_error("BREP JSON contains an embedded NUL.");
                guard_json(bytes);
                if (!ModelTransfer::BRepFile::parse(bytes.c_str(), parsed, error)) throw std::runtime_error(error);
            } else if (!ModelTransfer::BRepFile::parse(bytes.data(), bytes.size(), parsed, error)) throw std::runtime_error(error);
            if (!json_path.is_empty()) {
                auto json = read_file(json_path, 16 * 1024 * 1024);
                guard_json(json);
                if (!ModelTransfer::BRepFile::parse(json.data(), json.size(), root, error)) throw std::runtime_error(error);
            }
            // Resolve BRepFile's face export ordinals before either display path.
            std::set<uint32_t> source_owned_shells;
            for (const auto &solid : parsed.solids) for (auto s : solid.shells) source_owned_shells.insert(s);
            auto index_source_shell = [&](uint32_t s) {
                for (auto ref : parsed.shells.at(s).faces)
                    if (!source_export_indices.count(ref.index)) source_export_indices[ref.index] = static_cast<int>(source_export_indices.size());
            };
            for (uint32_t s=0; s<parsed.shells.size(); ++s) if (!source_owned_shells.count(s)) index_source_shell(s);
            for (const auto &solid : parsed.solids) for (auto s : solid.shells) index_source_shell(s);
            Array warnings;
            auto geom_path = std::filesystem::u8path(path.utf8().get_data());
            geom_path.replace_extension(".geom");
            if (std::filesystem::exists(geom_path)) {
                checkpoint();
                stage = 5;
                try {
                    auto direct = dv_brep::import_tessellation(geom_path, parsed, json_path.is_empty() ? nullptr : &root, [&]() { checkpoint(); });
                    checkpoint();
                    Array direct_bodies = direct["bodies"];
                    for (int b=0; b<direct_bodies.size(); ++b) {
                        Dictionary body = direct_bodies[b];
                        Array ranges = body["face_ranges"];
                        for (int r=0; r<ranges.size(); ++r) {
                            Dictionary range = ranges[r];
                            face_definitions[int(range["face_index"])] = range["definition"];
                        }
                    }
                    Dictionary stats = direct["stats"];
                    stats["unit_to_millimeters"] = parsed.unitToMillimeters;
                    stats["units"] = parsed.unitToMillimeters > 0 ? String::num(parsed.unitToMillimeters) + " mm per source unit" : String("unknown (source coordinates)");
                    stats["format_version"] = String::num_int64(parsed.versionMajor) + "." + String::num_int64(parsed.versionMinor);
                    stats["input_format"] = source_is_json ? "json" : "binary";
                    output["bodies"] = direct_bodies; output["nodes"] = direct["nodes"]; output["stats"] = stats; output["warnings"] = warnings;
                    source_bytes = std::move(bytes); direct_geometry = true; stage = 4;
                    return output;
                } catch (const std::exception &e) {
                    checkpoint();
                    face_definitions.clear();
                    warnings.append("GEOM unavailable; falling back to OCC: " + utf8(e.what()));
                }
            }
            checkpoint();
            stage = 2;
            auto converted = std::make_unique<BRepToTopoDS>(parsed);
            auto &converter = *converted;
            if (!converter.convert()) throw std::runtime_error(converter.errorMessage());
            std::map<uint32_t, std::vector<uint32_t>> source_by_id;
            for (uint32_t i = 0; i < parsed.faces.size(); ++i) source_by_id[parsed.faces[i].persistentId].push_back(i);
            // A source face can split during repair. Result indices must never
            // index parsed.faces. Persistent IDs survive repair; shell ancestry
            // can disambiguate IDs reused in different source shells.
            std::map<int, std::set<uint32_t>> face_source_shells;
            for (uint32_t s = 0; s < parsed.shells.size(); ++s)
                for (auto result_shell : converter.sourceShellResultIndices(s))
                    for (auto f : converter.shellFaceIndices(result_shell)) face_source_shells[f].insert(s);
            auto define_face = [&](int result_face) -> Dictionary {
                auto found = face_definitions.find(result_face);
                if (found != face_definitions.end()) return found->second;
                Dictionary out;
                auto id = converter.facePersistentId(result_face);
                auto candidates = source_by_id[id];
                String mapping = "persistent_id";
                if (candidates.size() > 1 && face_source_shells.count(result_face)) {
                    std::set<uint32_t> shell_faces;
                    for (auto s : face_source_shells[result_face])
                        for (auto ref : parsed.shells[s].faces) shell_faces.insert(ref.index);
                    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                        [&](uint32_t f) { return !shell_faces.count(f); }), candidates.end());
                    mapping = "shell_persistent_id";
                }
                PackedInt32Array source_candidates;
                for (auto f : candidates) source_candidates.append(f);
                out["source_face_indices"] = source_candidates;
                if (candidates.size() == 1) {
                    out["source_mapping"] = mapping;
                    out["source_face_index"] = int64_t(candidates.front());
                } else {
                    // Export all explicitly labelled candidates if the public
                    // SDK cannot identify one source face uniquely.
                    out["source_mapping"] = candidates.empty() ? "unavailable" : "ambiguous";
                }
                return face_definitions[result_face] = out;
            };
            checkpoint();
            stage = 3;
            Array bodies, nodes;
            std::map<std::pair<int, uint32_t>, int> cache;
            int total_triangles = 0, missing_faces = 0, node_count = 0;
            double quality = options.get("deflection_scale", 1.0);
            if (!std::isfinite(quality) || quality < 0.1 || quality > 10.0) throw std::runtime_error("Invalid BREP deflection scale (0.1..10).");
            auto mesh_body = [&](int type, uint32_t index) -> int {
                auto key = std::make_pair(type, index);
                auto found = cache.find(key);
                if (found != cache.end()) return found->second;
                TopoDS_Shape shape;
                if (type == static_cast<int>(ModelTransfer::BRT_SOLID)) {
                    if (index >= static_cast<uint32_t>(converter.numSolids())) throw std::runtime_error("Assembly solid index is out of range.");
                    shape = converter.solid(index);
                } else if (type == static_cast<int>(ModelTransfer::BRT_SHELL)) {
                    if (index >= static_cast<uint32_t>(converter.numShells())) throw std::runtime_error("Assembly shell index is out of range.");
                    shape = converter.shell(index);
                } else if (type == 4) shape = converter.face(index);
                else {
                    warnings.append("Wire-only body is not displayed by this preview.");
                    return -1;
                }
                checkpoint();
                Bnd_Box box;
                BRepBndLib::Add(shape, box);
                double diagonal = 1.0;
                if (!box.IsVoid()) {
                    double a,b,c,x,y,z;
                    box.Get(a,b,c,x,y,z);
                    diagonal = std::sqrt((x-a)*(x-a)+(y-b)*(y-b)+(z-c)*(z-c));
                }
                double deflection = std::clamp(diagonal * 1e-3, 1e-4, 1.0) * quality;
                BRepMesh_IncrementalMesh mesher(shape, deflection, false, 0.4, false);
                if (mesher.GetStatusFlags()) warnings.append("OCCT meshing reported status " + String::num_int64(mesher.GetStatusFlags()));
                PackedVector3Array vertices, normals;
                PackedInt32Array indices;
                Array ranges;
                for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
                    checkpoint();
                    auto face = TopoDS::Face(it.Current());
                    TopLoc_Location location;
                    auto tri = BRep_Tool::Triangulation(face, location);
                    if (tri.IsNull() || !tri->NbTriangles()) { ++missing_faces; continue; }
                    if (total_triangles + tri->NbTriangles() > max_triangles) throw std::runtime_error("BREP mesh exceeds the 2 million triangle preview budget. Use a coarser deflection_scale.");
                    int base = vertices.size(), start = indices.size() / 3;
                    int count = tri->NbNodes();
                    vertices.resize(base + count);
                    normals.resize(base + count);
                    for (int v = 1; v <= count; ++v) {
                        auto p = tri->Node(v).Transformed(location.Transformation());
                        if (!std::isfinite(p.X()) || !std::isfinite(p.Y()) || !std::isfinite(p.Z())) throw std::runtime_error("Non-finite BREP vertex.");
                        vertices.set(base + v - 1, Vector3(p.X(), p.Y(), p.Z()));
                        normals.set(base + v - 1, Vector3());
                    }
                    bool reverse = (face.Orientation() == TopAbs_REVERSED) != location.Transformation().IsNegative();
                    for (int t = 1; t <= tri->NbTriangles(); ++t) {
                        int a,b,c;
                        tri->Triangle(t).Get(a,b,c);
                        a += base - 1; b += base - 1; c += base - 1;
                        if (reverse) std::swap(b,c);
                        auto n = (vertices[b]-vertices[a]).cross(vertices[c]-vertices[a]);
                        normals.set(a, normals[a]+n); normals.set(b, normals[b]+n); normals.set(c, normals[c]+n);
                        // OCCT uses CCW fronts; Godot uses clockwise fronts.
                        indices.append(a); indices.append(c); indices.append(b);
                    }
                    for (int v = base; v < base + count; ++v) normals.set(v, normals[v].normalized());
                    int result_face = -1;
                    for (int f = 0; f < converter.numFaces(); ++f)
                        if (converter.face(f).IsSame(face)) { result_face = f; break; }
                    Dictionary range;
                    range["face_index"] = result_face;
                    range["persistent_id"] = result_face < 0 ? int64_t(0) : int64_t(converter.facePersistentId(result_face));
                    if (result_face >= 0) range["definition"] = define_face(result_face);
                    range["result_face_reversed"] = face.Orientation() == TopAbs_REVERSED;
                    range["body_type"] = type; range["body_index"] = index;
                    int shell_index = type == static_cast<int>(ModelTransfer::BRT_SHELL) ? static_cast<int>(index) : -1;
                    if (type == static_cast<int>(ModelTransfer::BRT_SOLID) && result_face >= 0) {
                        for (auto s : converter.solidShellIndices(index)) {
                            const auto &faces = converter.shellFaceIndices(s);
                            if (std::find(faces.begin(), faces.end(), result_face) != faces.end()) { shell_index = s; break; }
                        }
                    }
                    range["shell_index"] = shell_index;
                    range["solid_index"] = type == static_cast<int>(ModelTransfer::BRT_SOLID) ? static_cast<int>(index) : -1;
                    range["triangle_first"] = start; range["triangle_count"] = tri->NbTriangles();
                    ranges.append(range);
                    total_triangles += tri->NbTriangles();
                }
                Dictionary body;
                body["vertices"] = vertices; body["normals"] = normals; body["indices"] = indices;
                body["face_ranges"] = ranges; body["body_type"] = type; body["body_index"] = index;
                int result = bodies.size();
                bodies.append(body); cache[key] = result;
                return result;
            };
            std::function<void(const ModelTransfer::Object &, int, int)> visit;
            visit = [&](const ModelTransfer::Object &object, int parent, int depth) {
                checkpoint();
                if (++node_count > 100000 || depth > 128) throw std::runtime_error("Assembly node/depth budget exceeded.");
                Dictionary node;
                node["parent"] = parent; node["name"] = utf8(object.name);
                node["persistent_id"] = int64_t(object.persistentId); node["object_type"] = int(object.type);
                PackedFloat64Array matrix;
                for (double v : object.matrix) {
                    if (!std::isfinite(v)) throw std::runtime_error("Invalid assembly matrix.");
                    matrix.append(v);
                }
                node["matrix"] = matrix;
                // Source body indices no longer equal repaired result indices.
                PackedInt32Array references;
                if (object.bRepBodyType == ModelTransfer::BRT_SOLID) {
                    if (object.repBodyIndex >= parsed.solids.size()) throw std::runtime_error("Assembly source solid index is out of range.");
                    std::vector<uint32_t> solids, shells;
                    converter.getShapeBySourceSolidIndex(object.repBodyIndex, solids, shells);
                    for (auto index : solids) references.append(mesh_body(static_cast<int>(ModelTransfer::BRT_SOLID), index));
                    for (auto index : shells) references.append(mesh_body(static_cast<int>(ModelTransfer::BRT_SHELL), index));
                    if (solids.empty() && shells.empty()) warnings.append("Source solid has no repaired display body.");
                } else if (object.bRepBodyType == ModelTransfer::BRT_SHELL) {
                    if (object.repBodyIndex >= parsed.shells.size()) throw std::runtime_error("Assembly source shell index is out of range.");
                    const auto &shells = converter.sourceShellResultIndices(object.repBodyIndex);
                    for (auto index : shells) references.append(mesh_body(static_cast<int>(ModelTransfer::BRT_SHELL), index));
                    if (shells.empty()) warnings.append("Source shell has no repaired display body.");
                } else if (object.bRepBodyType == ModelTransfer::BRT_WIRE) {
                    warnings.append("Wire-only body is not displayed by this preview.");
                } else if (object.bRepBodyType != ModelTransfer::BRT_UNKNOWN) {
                    throw std::runtime_error("Unknown assembly body type.");
                }
                node["bodies"] = references;
                node["source_body_type"] = int(object.bRepBodyType);
                node["source_body_index"] = int64_t(object.repBodyIndex);
                int current = nodes.size(); nodes.append(node);
                for (const auto *child : object.children) if (child) visit(*child, current, depth + 1);
            };
            if (!json_path.is_empty()) visit(root, -1, 0);
            else {
                std::set<uint32_t> owned_shells;
                auto append = [&](int type, int index) {
                    Dictionary node;
                    node["parent"] = -1; node["name"] = String(type == static_cast<int>(ModelTransfer::BRT_SOLID) ? "Solid " : "Shell ") + String::num_int64(index);
                    PackedInt32Array references;
                    references.append(mesh_body(type, index));
                    node["persistent_id"] = 0; node["bodies"] = references; nodes.append(node);
                };
                for (int i = 0; i < converter.numSolids(); ++i) {
                    append(static_cast<int>(ModelTransfer::BRT_SOLID), i);
                    for (auto s : converter.solidShellIndices(i)) owned_shells.insert(s);
                }
                for (int i = 0; i < converter.numShells(); ++i) if (!owned_shells.count(i)) append(static_cast<int>(ModelTransfer::BRT_SHELL), i);
                if (!bodies.size()) for (int i = 0; i < converter.numFaces(); ++i) append(4, i);
            }
            checkpoint();
            if (!total_triangles) throw std::runtime_error("No displayable faces in this BREP model (wire-only geometry is not supported yet).");
            if (missing_faces) warnings.append(String::num_int64(missing_faces) + " faces have no triangulation.");
            Dictionary stats;
            stats["faces"] = converter.numFaces(); stats["solids"] = converter.numSolids(); stats["shells"] = converter.numShells();
            stats["unique_bodies"] = bodies.size(); stats["triangles"] = total_triangles; stats["missing_faces"] = missing_faces;
            stats["unit_to_millimeters"] = parsed.unitToMillimeters;
            stats["units"] = parsed.unitToMillimeters > 0.0
                ? String::num(parsed.unitToMillimeters) + " mm per source unit" : String("unknown (source coordinates)");
            stats["source_solids"] = int64_t(parsed.solids.size()); stats["source_shells"] = int64_t(parsed.shells.size());
            stats["format_version"] = String::num_int64(parsed.versionMajor) + "." + String::num_int64(parsed.versionMinor);
            stats["input_format"] = source_is_json ? "json" : "binary";
            stats["geometry_source"] = "occ"; stats["occ_reconstructed"] = true;
            output["bodies"] = bodies; output["nodes"] = nodes; output["stats"] = stats; output["warnings"] = warnings;
            source_bytes = std::move(bytes);
            reconstructed = std::move(converted);
            stage = 4;
        } catch (const Standard_Failure &e) { output["error"] = utf8(e.what()); }
        catch (const std::exception &e) { output["error"] = utf8(e.what()); }
        catch (...) { output["error"] = "Unknown BREP SDK failure."; }
        return output;
    }
};

void initialize_brep(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) ClassDB::register_class<BRepDocument>();
}
void terminate_brep(ModuleInitializationLevel) {}
extern "C" GDExtensionBool GDE_EXPORT digital_viewer_brep_init(GDExtensionInterfaceGetProcAddress address, GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    GDExtensionBinding::InitObject init(address, library, initialization);
    init.register_initializer(initialize_brep); init.register_terminator(terminate_brep);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
