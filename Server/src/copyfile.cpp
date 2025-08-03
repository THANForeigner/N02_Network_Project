#include "copyfile.h"
#include <filesystem>
#include <iostream>
const std::filesystem::path DESTINATION_DIR = "../data/copyfile";

void CopyToPath(std::string baseFile)
{
    std::filesystem::path sourcePath(baseFile);
    std::filesystem::path filename = sourcePath.filename();
    if (filename.empty())
    {
        std::cout<<"Invalid source file path provided.";
        exit(1);
    }
    std::filesystem::path destinationPath = DESTINATION_DIR / filename;
    if (!std::filesystem::exists(sourcePath))
    {
        std::cout<<"Source file does not exist: " << sourcePath.string();
        exit(1);
    }
    if (!std::filesystem::is_regular_file(sourcePath))
    {
        std::cout<<"Source is not a regular file: " << sourcePath.string();
        exit(1);
    }
    std::filesystem::create_directories(DESTINATION_DIR);
    if (std::filesystem::exists(destinationPath))
    {
        std::string new_name = destinationPath.stem().string() + "_copy" + destinationPath.extension().string();
        destinationPath.replace_filename(new_name);
    }
    std::filesystem::copy_file(sourcePath, destinationPath);
}