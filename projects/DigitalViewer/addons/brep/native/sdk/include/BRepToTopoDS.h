#pragma once

#include "BRepToTopoDSExport.h"
#include "ReconstructionOptions.h"

#include <BRepParsedData.h>
#include <BRep.h>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BREP_TOPODS_EXPORT BRepToTopoDS
{
public:
    // 默认修复上限根据源单位换算为 1 mm；未知单位保留 1 个坐标单位。
    explicit BRepToTopoDS(const ModelTransfer::ParsedBRepData& parsedData);
    explicit BRepToTopoDS(const ModelTransfer::NDSBRep& brepData);
    // 显式 options 的全部长度已是输入坐标单位，不再按源单位换算。
    BRepToTopoDS(const ModelTransfer::ParsedBRepData& parsedData, const BRepReconstructionOptions& options);
    BRepToTopoDS(const ModelTransfer::NDSBRep& brepData, const BRepReconstructionOptions& options);
    ~BRepToTopoDS();

    BRepToTopoDS(const BRepToTopoDS&) = delete;
    BRepToTopoDS& operator=(const BRepToTopoDS&) = delete;
    BRepToTopoDS(BRepToTopoDS&& other) noexcept;
    BRepToTopoDS& operator=(BRepToTopoDS&& other) noexcept;

    // 输入数据必须至少存活到首次 convert() 返回。转换成功后，结果由本对象独立持有。
    // 成功表示重建结果完整可用，不保证拓扑有效；方向冲突记录警告，开口部分保留为壳。
    bool convert();
    bool isConverted() const noexcept;
    const std::string& errorMessage() const noexcept;

	int numVertices() const noexcept;
    int numEdges() const noexcept;
    int numWires() const noexcept;
    int numFaces() const noexcept;
    int numShells() const noexcept;
    int numSolids() const noexcept;

    const TopoDS_Vertex& vertex(std::size_t index) const;
    const TopoDS_Edge& edge(std::size_t index) const;
    const TopoDS_Wire& wire(std::size_t index) const;
    const TopoDS_Face& face(std::size_t index) const;
    const TopoDS_Shell& shell(std::size_t index) const;
    const TopoDS_Solid& solid(std::size_t index) const;

    // convert() 成功后按源实体索引查询，覆盖两个输出容器。
    // solidIndices 可用于 solid()；shellIndices 可用于 shell()，仅含未组成实体的独立壳。
    // 无效源索引抛出 std::out_of_range，输出保持不变。
    void getShapeBySourceSolidIndex(std::size_t sourceSolidIndex,
        std::vector<std::uint32_t>& solidIndices, std::vector<std::uint32_t>& shellIndices) const;

    // 数量和索引均指向转换后的实际拓扑，可能因修复而改变。
    std::uint32_t facePersistentId(std::size_t faceIndex) const;
    const std::vector<std::uint32_t>& wireEdgeIndices(std::size_t wireIndex) const;
    const std::vector<std::uint32_t>& faceWireIndices(std::size_t faceIndex) const;
    // 输入源壳索引，返回该源壳修复后产生的所有结果壳索引。
    const std::vector<std::uint32_t>& sourceShellResultIndices(std::size_t sourceShellIndex) const;
    const std::vector<std::uint32_t>& shellFaceIndices(std::size_t shellIndex) const;
    const std::vector<std::uint32_t>& solidShellIndices(std::size_t solidIndex) const;
    //
    /**
	* 保存修复后的面 B-rep 到 JSON 字符串。
    */
    bool separateFaceBrepsToJson(std::string& jsonString, const std::vector<int>& faceIndices, std::string& errorMessage);
private:
    class Impl;
#if defined(_MSC_VER)
#pragma warning(suppress : 4251)
#endif
    std::unique_ptr<Impl> m_impl;
};
