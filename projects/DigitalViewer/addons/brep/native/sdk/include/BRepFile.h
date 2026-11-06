#pragma once

#include "./BRepModel.h"
#include "./BRepParsedData.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace ModelTransfer
{
	class BREP_EXPORT BRepFile
	{
	public:
		static bool write(const CADModel& model, const std::filesystem::path& outputFolder, const char* modelName, std::string& errorMessage);
		static bool write(const CADModel& model, const char* outputFolder, const char* modelName, std::string& errorMessage);
		static bool write(const NDSBRep& bRep, const char* outputFolder, const char* brepName, std::string& errorMessage);
		static bool writeSolid(const NDSBRep& bRep, std::size_t solidIndex,
			const std::filesystem::path& outputFile, std::string& errorMessage);
		static bool read(const char* modelPath, CADModel& model, std::string& errorMessage);
		static bool read(const char* brepPath, NDSBRep& bRep, std::string& errorMessage);
		static bool read(const std::filesystem::path& path,
			std::list<std::unique_ptr<Tessellation>>& tessellations, std::string& errorMessage);
		static bool parse(const std::filesystem::path& path, ParsedBRepData& data, std::string& errorMessage);
		static bool parse(const char* bytes, std::size_t size, ParsedBRepData& data, std::string& errorMessage);
		/**
		* Parses a JSON string containing B-rep data.
		* @param jsonBrep The JSON string containing the B-rep data.
		* @param data The parsed B-rep data.
		* @param errorMessage The error message if the operation fails.
		* @return True if the operation is successful, false otherwise.
		*/
		static bool parse(const char* jsonBrep, ParsedBRepData& data, std::string& errorMessage);
		static bool parse(const std::filesystem::path& path, Object& root, std::string& errorMessage);
		static bool parse(const char* bytes, std::size_t size, Object& root, std::string& errorMessage);
	public:
		BRepFile(const char* brepPath);
		BRepFile(const char* brepData, int size);
		~BRepFile();

		BRepFile(const BRepFile&) = delete;
		BRepFile& operator=(const BRepFile&) = delete;
		BRepFile(BRepFile&& other) noexcept;
		BRepFile& operator=(BRepFile&& other) noexcept;

		bool separateShellBreps(const char* outputFolder, const char* brepName, const std::vector<int>& shellIndices, std::string& errorMessage);
		bool separateSolidBreps(const char* outputFolder, const char* brepName, const std::vector<int>& solidIndices, std::string& errorMessage);
		bool separateFaceBreps(const char* outputFolder, const char* brepName, const std::vector<int>& faceIndices, std::string& errorMessage);
		/**
		* Saves the face B-reps to a JSON string.
		* @param jsonString The JSON string to populate.
		* @param faceIndices The indices of the faces to save.
		* @param errorMessage The error message if the operation fails.
		* @return True if the operation is successful, false otherwise.
		*/
		bool separateFaceBrepsToJson(std::string& jsonString, const std::vector<int>& faceIndices, std::string& errorMessage);
		bool saveToJson(const char* outputPath, std::string& errorMessage);
		/**
		* Saves the B-rep data to a JSON string.
		* @param jsonString The JSON string to populate.
		* @param errorMessage The error message if the operation fails.
		* @return True if the operation is successful, false otherwise.
		*/
		bool saveToJson(std::string& jsonString, std::string& errorMessage);
	private:
		char* brepData = nullptr;
		int brepDataSize = 0;
		bool ownsData = false;
		char* initializationError = nullptr;
	};
}
