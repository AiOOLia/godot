#include "./test.h"
#include "../include/osg/PagedLOD"
#include "../include/osgDB/ReadFile.h"
#include "../include/osgDB/ConvertUTF.h"
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#include <Windows.h>
#endif

std::string get_current_encoding() {
#ifdef _WIN32
    UINT code_page = GetACP();
    switch (code_page) {
    case 936:  return "GBK";
    case 932:  return "Shift-JIS";
    case 949:  return "EUC-KR";
    case 65001:return "UTF-8";
    default:   return "Windows-" + std::to_string(code_page);
    }
#else
    const char* lang = std::getenv("LC_ALL");
    if (!lang) lang = std::getenv("LC_CTYPE");
    if (!lang) lang = std::getenv("LANG");
    if (lang) {
        std::string s = lang;
        size_t dot = s.find('.');
        if (dot != std::string::npos) {
            size_t end = s.find('@', dot + 1);
            if (end == std::string::npos) end = s.size();
            return s.substr(dot + 1, end - dot - 1);
        }
    }
    return "UTF-8";  // 默认 UTF-8
#endif
}

int main()
{
    std::cout << std::locale().name();
    std::cout << get_current_encoding();
	std::u8string u8_path = u8"E:/WorkSpace/TJH/Mesh调度测试/OSGB-LOD-1/1-WuDangShan/Production_2/Production_2.osgb";

    if (std::filesystem::exists(u8_path)) {
        std::cout << "aaaa";
    }

	auto pnode = osgDB::readNodeFile(osgDB::convertStringFromUTF8toCurrentCodePage(u8_path));
	std::cout << pnode->className();
	//
	auto pg = (osg::PagedLOD*)pnode;
	auto childpath = pg->getDatabasePath() + pg->getFileName(1);
	//
	auto pnode1 = osgDB::readNodeFile(childpath);
	//
	return 0;
}