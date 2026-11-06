#pragma once

#include <algorithm>
#include <iterator>
#include <utility>

#include "./BRep.h"

namespace ModelTransfer
{
    enum class BRepBodyType : uint8_t
    {
        BRT_UNKNOWN = 0,
        BRT_SOLID = 1,
        BRT_SHELL,
        BRT_WIRE,
    };

    enum class ObjectType : uint8_t
    {
        OT_ASSEMBLY = 0,
        OT_PART = 1,
        OT_BODY = 2,
    };

    using enum BRepBodyType;
    using enum ObjectType;

    enum class PrimitiveType : uint8_t
    {
        PT_Triangles,
        PT_Lines
    };

	struct Tessellation
	{
        uint32_t id;
		PrimitiveType primitiveType;
		std::vector<float> vertices;
		std::vector<float> normals;
		std::vector<uint32_t> indices;
        //对应的 B-rep 实体索引，边/面
		std::vector<uint32_t> brepEntityIndices;
        //当前离散几何数据对象所含图元按拓扑对象的分组，
        //记录了每个拓扑对象所对应的图元里第一个图元在顶点索引数组中的起始索引号
        std::vector<uint32_t> brepPrimitiveGroup;
        // Import-only associations, one per group; resolved to file indices by BRepWriter.
        // Shared ownership keeps removed topology identifiable until write validation.
        std::vector<std::shared_ptr<const NDSFace>> sourceFaces;
        std::vector<std::shared_ptr<const NDSEdge>> sourceEdges;
	};

    struct Object
    {
        std::string name;
        std::uint32_t persistentId = 0;
        ObjectType type = OT_ASSEMBLY;
        BRepBodyType bRepBodyType = BRT_UNKNOWN;
        std::uint32_t repBodyIndex = std::numeric_limits<std::uint32_t>::max();
        void* pBody = nullptr;
        double matrix[16] = {
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0,
        };
        std::list<Object*> children;
        // 对应的 tessellation ID 列表
		std::list<uint32_t> faceTessIds;
		std::list<uint32_t> edgeTessIds;

        Object() = default;
        Object(const Object&) = delete;
        Object& operator=(const Object&) = delete;

        Object(Object&& other) noexcept
            : name(std::move(other.name)),
              persistentId(other.persistentId),
              type(other.type),
              bRepBodyType(other.bRepBodyType),
              repBodyIndex(other.repBodyIndex),
              pBody(other.pBody),
              children(std::move(other.children)),
              faceTessIds(std::move(other.faceTessIds)),
              edgeTessIds(std::move(other.edgeTessIds))
        {
            std::copy(std::begin(other.matrix), std::end(other.matrix), std::begin(matrix));
            other.children.clear();
        }

        Object& operator=(Object&& other) noexcept
        {
            if (this == &other) return *this;
            for (Object* child : children) delete child;
            name = std::move(other.name);
            persistentId = other.persistentId;
            type = other.type;
            bRepBodyType = other.bRepBodyType;
            repBodyIndex = other.repBodyIndex;
            pBody = other.pBody;
            std::copy(std::begin(other.matrix), std::end(other.matrix), std::begin(matrix));
            children = std::move(other.children);
            faceTessIds = std::move(other.faceTessIds);
            edgeTessIds = std::move(other.edgeTessIds);
            other.children.clear();
            return *this;
        }

        ~Object()
        {
            for (Object* child : children) delete child;
        }
    };

    struct CADModel
    {
        Object root;
		std::list<std::unique_ptr<Tessellation>> tessellations;
        NDSBRep bRep;
    };
}
