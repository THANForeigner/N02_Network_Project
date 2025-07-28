#pragma once
#include<fstream>
#include<string>
struct FileName{
    std::string name;
    std::string tail;
};
void CopyToPath(std::string baseFile, std::string DesPath);
void CopyToPathRename(std::string baseFile, std::string DesPath, std::string newName);
