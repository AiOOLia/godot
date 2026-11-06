#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#if defined(BREP_IMPLEMENTATION)
#define BREP_EXPORT __declspec(dllexport)
#else
#define BREP_EXPORT __declspec(dllimport)
#endif
#elif defined(__linux__)
#if defined(BREP_IMPLEMENTATION)
#define BREP_EXPORT __attribute__((visibility("default")))
#else
#define BREP_EXPORT
#endif
#else
#error "Unsupported platform"
#endif

namespace ModelTransfer
{
    enum eCurveType
    {
        CT_LINE = 0,
        CT_LINE_SEG,
        CT_CIRCLE,
        CT_ELLIPSE,
        CT_NURBS,
        CT_OTHER
    };

    struct NDSCurve
    {
        virtual ~NDSCurve() = default;
        eCurveType type;
        //
        virtual void applyScale(double scale) {};
    };

    struct NDSLine : NDSCurve
    {
        double origin[3];
        double direction[3];
        NDSLine()
        {
            type = eCurveType::CT_LINE;
        }

        void applyScale(double scale) override
        {
			origin[0] *= scale;
            origin[1] *= scale;
			origin[2] *= scale;
        }
    };

    struct NDSLineSegment : NDSCurve
    {
        NDSLineSegment()
        {
            type = eCurveType::CT_LINE_SEG;
        }
    };

    struct NDSCircle : NDSCurve
    {
        double radius;
        double center[3];
        double normal[3];
        double axisX[3];
        NDSCircle()
        {
            type = eCurveType::CT_CIRCLE;
        }

        void applyScale(double scale) override
        {
			radius *= scale;
            center[0] *= scale;
            center[1] *= scale;
            center[2] *= scale;
        }
    };

    struct NDSEllipse : NDSCurve
    {
        double radiusX;
        double radiusY;
        double center[3];
        double axisX[3];
        double normal[3];
        NDSEllipse()
        {
            type = eCurveType::CT_ELLIPSE;
        }
        void applyScale(double scale) override
        {
            radiusX *= scale;
            radiusY *= scale;
            center[0] *= scale;
            center[1] *= scale;
            center[2] *= scale;
        }
    };

    struct NDSBSplineCurve : NDSCurve
    {
        // 控制点， x y z....
        std::vector<double> poles;
        // 权重，默认为1
        std::vector<double> weights;
        std::vector<double> knots;
        std::vector<double> multiplicities;
        int degree;
        //
        NDSBSplineCurve()
        {
            type = eCurveType::CT_NURBS;
        }

        void applyScale(double scale) override
        {
			for (auto i = 0; i < poles.size(); i++)
            {
                poles[i] *= scale;
            }
        }
    };

    enum eSurfaceType
    {
        ST_PLANE = 0,
        ST_CYLINDER,
        ST_CONE,
        ST_SPHERE,
        ST_TORUS,
        ST_NURBS,
        ST_OTHER,
        ST_EXTRUSION,
        ST_REVOLUTION
    };

    struct NDSSurface
    {
        virtual ~NDSSurface() = default;
        eSurfaceType type;
        bool UReversed = false;
        bool VReversed = false;
        //
        virtual void applyScale(double scale) {};
    };

    struct NDSExtrusionSurface : NDSSurface
    {
        std::unique_ptr<NDSCurve> basisCurve;
        double direction[3];
        double origin[3];
        double axisX[3];
        double axisY[3];
        double uMin = 0.0;
        double uMax = 0.0;
        double vMin = 0.0;
        double vMax = 0.0;
        NDSExtrusionSurface()
        {
            type = eSurfaceType::ST_EXTRUSION;
        }
        //
        void applyScale(double scale) override
        {
			basisCurve->applyScale(scale);
            //
			origin[0] *= scale;
			origin[1] *= scale;
			origin[2] *= scale;
        }
    };

    struct NDSRevolutionSurface : NDSSurface
    {
        std::unique_ptr<NDSCurve> basisCurve;
        double origin[3];
        double direction[3];
        double trsfOrigin[3];
        double axisX[3];
        double axisY[3];
        NDSRevolutionSurface()
        {
            type = eSurfaceType::ST_REVOLUTION;
        }
        void applyScale(double scale) override
        {
            basisCurve->applyScale(scale);
            //
            origin[0] *= scale;
            origin[1] *= scale;
            origin[2] *= scale;
            //
			trsfOrigin[0] *= scale;
            trsfOrigin[1] *= scale;
			trsfOrigin[2] *= scale;
        }
    };

    struct NDSPlane : NDSSurface
    {
        double origin[3];
        double axisX[3];
        double normal[3];
        NDSPlane()
        {
            type = eSurfaceType::ST_PLANE;
        }
        //
        void applyScale(double scale) override
        {
            origin[0] *= scale;
            origin[1] *= scale;
            origin[2] *= scale;
        }
    };

    struct NDSSphere : NDSSurface
    {
        double radius;
        double origin[3];
        double axisX[3];
        double axisZ[3];
        NDSSphere()
        {
            radius = 0;
            type = eSurfaceType::ST_SPHERE;
        }

        void applyScale(double scale) override
        {
			radius *= scale;
            origin[0] *= scale;
            origin[1] *= scale;
            origin[2] *= scale;
        }
    };

    struct NDSCylinder : NDSSurface
    {
        double height;
        double radius;
        double origin[3];
        double normal[3];
        double axisX[3];
        NDSCylinder()
        {
            type = eSurfaceType::ST_CYLINDER;
        }
        void applyScale(double scale) override
        {
			radius *= scale;
			height *= scale;
            //
            origin[0] *= scale;
            origin[1] *= scale;
            origin[2] *= scale;
        }
    };

    struct NDSCone : NDSSurface
    {
        double halfangle;
        double radius;
        double origin[3];
        double normal[3];
        double axisX[3];
        NDSCone()
        {
            type = eSurfaceType::ST_CONE;
        }
        void applyScale(double scale) override
        {
			radius *= scale;
            //
            origin[0] *= scale;
            origin[1] *= scale;
            origin[2] *= scale;
        }
    };

    struct NDSTorus : NDSSurface
    {
        double origin[3];
        double centerLineRadius;
        double sectionRadius;
        double normal[3];
        double axisX[3];
        NDSTorus()
        {
            centerLineRadius = sectionRadius = 0;
            type = eSurfaceType::ST_TORUS;
        }
        void applyScale(double scale) override
        {
			centerLineRadius *= scale;
			sectionRadius *= scale;
            //
            origin[0] *= scale;
            origin[1] *= scale;
            origin[2] *= scale;
        }
    };

    struct NDSBSplineSurface : NDSSurface
    {
        // 控制点按 Pu0v0, Pu0v1, ..., Pu1v0, ... 展开，每个点为 x y z
        std::vector<double> poles;
        // 有理曲面权重，顺序与 poles 的控制点顺序一致；为空表示非有理曲面
        std::vector<double> weights;
        std::vector<double> uknots;
        std::vector<double> vknots;
        std::vector<double> umults;
        std::vector<double> vmults;
        int udegree = 0;
        int vdegree = 0;
        int uPoleCount = 0;
        int vPoleCount = 0;
        //
        NDSBSplineSurface()
        {
            type = eSurfaceType::ST_NURBS;
        }

        void applyScale(double scale) override
        {
			for (auto i = 0; i < poles.size(); i++)
            {
                poles[i] *= scale;
            }
        }
    };

    struct NDSVertex
    {
        std::array<double, 3> point = {0.0, 0.0, 0.0};
        double tolerance = 0.0;
		bool scaleApplied = false;

        double& operator[](std::size_t index)
        {
            return point[index];
        }

        const double& operator[](std::size_t index) const
        {
            return point[index];
        }

        void applyScale(double scale)
        {
			if (scaleApplied) return;
            //
			point[0] *= scale;
			point[1] *= scale;
			point[2] *= scale;
			tolerance *= scale;
            //
			scaleApplied = true;
        }
    };
    //
    struct NDSEdge
    {
        std::shared_ptr<NDSVertex> startVertex;
        std::shared_ptr<NDSVertex> endVertex;
        std::unique_ptr<NDSCurve> pCurve;
        double length; // edge length
        std::uint32_t persistentId = 0;
        bool hasTrimDomain = false;
        double trimMin = 0.0;
        double trimMax = 0.0;
        bool sameSense = true;
        double tolerance = 0.0;
        bool degenerated = false;
		bool scaleApplied = false;

        void applyScale(double scale)
        {
			if (scaleApplied) return;
            //
			tolerance *= scale;
			startVertex->applyScale(scale);
			endVertex->applyScale(scale);
			length *= scale;
            if (pCurve) {
				pCurve->applyScale(scale);
            }
            //
			scaleApplied = true;
        }
    };
    // 有向边，由Edge对象以及是否需要反向标识组成
    // 当有向边的起点和终点与原边向反时需要反向
    struct NDSDirectedEdge
    {
        std::shared_ptr<NDSEdge> edge;
        std::unique_ptr<NDSCurve> puvCurve;
        bool reverse = false;
        bool seamWithFace = false;
		bool scaleApplied = false;
        //
        void applyScale(double scale)
        {
		   if (scaleApplied) return;
           //
           edge->applyScale(scale);
		   scaleApplied = true;
		}
    };

    struct NDSWire
    {
        std::list<std::shared_ptr<NDSDirectedEdge>> edges;
        bool reverseWithSurface = false;
		bool scaleApplied = false;
        //
        void applyScale(double scale)
        {
			if (scaleApplied) return;
            //
            for (auto& directedEdge : edges)
            {
                directedEdge->applyScale(scale);
            }
			scaleApplied = true;
		}
    };

    struct NDSFace
    {
        std::uint32_t persistentId = 0;
        std::unique_ptr<NDSSurface> pSurf;
        bool hasSurfaceDomain = false;
        double uMin = 0.0;
        double uMax = 0.0;
        double vMin = 0.0;
        double vMax = 0.0;
        // Negative means unavailable; zero is a measured zero area (format 1.2+).
        double area = -1.0;
        double perimeter = 0.0;
		bool scaleApplied = false;
        //
        std::list<std::unique_ptr<NDSWire>> wires;
        //
        void applyScale(double scale)
        {
			if (scaleApplied) return;
            //
            if (pSurf)
            {
				pSurf->applyScale(scale);
            }
            //
            for (auto& wire : wires)
            {
				wire->applyScale(scale);
            }
            //
			if (area >= 0.0) area *= scale * scale;
			perimeter *= scale;
            //
			scaleApplied = true;
        }
    };

    // 有向面，由Face对象以及是否需要反向标识组成
    // 当面的法线与其对应的Surface的法线相反时需要反向
    struct NDSDirectedFace
    {
        std::shared_ptr<NDSFace> face;
        bool reverse;
		bool scaleApplied = false;
        //
        void applyScale(double scale)
        {
			if (scaleApplied) return;
			face->applyScale(scale);
			scaleApplied = true;
        }
    };

    struct NDSShell
    {
        std::list<std::unique_ptr<NDSDirectedFace>> faces;
        double area = -1.0;
		bool scaleApplied = false;
        void applyScale(double scale)
        {
            if (scaleApplied) return;
            for (auto& directedFace : faces)
            {
				directedFace->applyScale(scale);
            }
            if (area >= 0.0) area *= scale * scale;
			scaleApplied = true;
		}
    };

    struct NDSSolid
    {
        std::list<std::unique_ptr<NDSShell>> shells;
        double volume;
        void applyScale(double scale)
        {
            for (auto& shell : shells)
            {
				shell->applyScale(scale);
            }
            volume *= scale * scale * scale;
		}
    };

    struct NDSBRep
    {
        // 一个几何坐标单位对应的毫米数；0 表示未知或未统一的单位。
        // 只描述坐标，不缩放几何，也不包含对象实例变换。
        double unitToMillimeters = 0.0;
        std::list<std::unique_ptr<NDSSolid>> solids;
        // 不属于任何solid对象的shell;
        std::list<std::unique_ptr<NDSShell>> shells;
        // 不属于任何面的边
        std::list<std::unique_ptr<NDSWire>> wires;
        //
        bool isEmpty()
        {
            if (solids.empty() && shells.empty() && wires.empty())
            {
                return true;
            }
            //
            return false;
        }
    };
}
