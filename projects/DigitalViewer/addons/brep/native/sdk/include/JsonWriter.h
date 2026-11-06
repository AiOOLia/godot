#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>

#include "BRepModel.h"
#include "BRepParsedData.h"

namespace ModelTransfer
{
    class BREP_EXPORT JsonWriter
    {
    public:
        bool write(const CADModel& model,
                   const std::filesystem::path& brepFileName,
                   const std::filesystem::path& outputFile,
                   std::string& errorMessage) const;

        bool write(const ParsedBRepData& data,
            const std::filesystem::path& outputFile,
            std::string& errorMessage) const;
        bool write(const ParsedBRepData& data,
            std::string& jsonString,
            std::string& errorMessage) const;
    private:
        bool write(const ParsedBRepData& data,
            std::ostream& stream,
            std::string& errorMessage) const;
    };
}
