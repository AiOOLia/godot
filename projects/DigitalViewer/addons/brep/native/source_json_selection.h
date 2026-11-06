#pragma once
#include <BRepFile.h>
#include <JsonWriter.h>
#include <map>

// The binary-only face selector in BRepFile cannot consume JSON. Use its JSON
// parser and JsonWriter, moving the selected topology/dependencies unchanged.
// No curve/surface parsing, conversion or JSON field serialization lives here.
inline bool source_json_faces(const std::string &bytes, const std::vector<int> &indices,
        std::string &json, std::string &error) {
    using namespace ModelTransfer;
    ParsedBRepData data;
    if (!BRepFile::parse(bytes.c_str(), data, error)) return false;
    ParsedBRepData selected;
    selected.versionMajor = data.versionMajor; selected.versionMinor = data.versionMinor;
    selected.versionPatch = data.versionPatch; selected.versionFlags = data.versionFlags;
    selected.unitToMillimeters = data.unitToMillimeters;
    std::map<uint32_t, uint32_t> vertices, edges, wires;
    auto vertex = [&](uint32_t i) -> uint32_t {
        auto found = vertices.find(i);
        if (found != vertices.end()) return found->second;
        auto index = static_cast<uint32_t>(selected.vertices.size());
        selected.vertices.push_back(data.vertices.at(i));
        return vertices[i] = index;
    };
    auto edge = [&](uint32_t i) -> uint32_t {
        auto found = edges.find(i);
        if (found != edges.end()) return found->second;
        auto value = std::move(data.edges.at(i));
        value.startVertexIndex = vertex(value.startVertexIndex);
        value.endVertexIndex = vertex(value.endVertexIndex);
        auto index = static_cast<uint32_t>(selected.edges.size());
        selected.edges.push_back(std::move(value));
        return edges[i] = index;
    };
    auto wire = [&](uint32_t i) -> uint32_t {
        auto found = wires.find(i);
        if (found != wires.end()) return found->second;
        auto value = std::move(data.wires.at(i));
        for (auto &ref : value.edges) ref.index = edge(ref.index);
        auto index = static_cast<uint32_t>(selected.wires.size());
        selected.wires.push_back(std::move(value));
        return wires[i] = index;
    };
    for (int i : indices) {
        bool reverse = false, found = false;
        for (const auto &shell : data.shells) {
            for (const auto &ref : shell.faces) if (ref.index == i) { reverse = ref.reverse; found = true; break; }
            if (found) break;
        }
        auto face = std::move(data.faces.at(i));
        for (auto &ref : face.wires) ref.index = wire(ref.index);
        BRepShellData shell;
        shell.area = face.area;
        shell.faces.push_back({static_cast<uint32_t>(selected.faces.size()), reverse});
        selected.faces.push_back(std::move(face));
        selected.shells.push_back(std::move(shell));
    }
    return JsonWriter().write(selected, json, error);
}
