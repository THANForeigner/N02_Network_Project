#include "copyfile.h"

std::string CreateFile(std::string name, std::string path)
{
    std::string newFile=path+"/"+name;
    return newFile;
}
FileName GetFileName(std::string file)
{
    unsigned int i;
    for(i=0;i<file.size();i++)
    {
        if(file[i]=='.')
        {
            break;
        }
    }
    FileName newFile;
    newFile.tail=file.substr(i);
    newFile.name=file.substr(0,i);
    return newFile;
}
void CopyToPath(std::string baseFile, std::string DesPath)
{
    int fileInPath;
    for(int i=(int)baseFile.size()-1;i>0;i--)
    {
        fileInPath=i;
        if(baseFile[i]=='/')break;
    }
    std::string file=baseFile.substr(fileInPath+1);
    FileName Name=GetFileName(file);
    std::string newFile=CreateFile(Name.name+Name.tail,DesPath);
    if(newFile==baseFile)
    {
        Name.name=Name.name+"_copy";
        newFile=CreateFile(Name.name+Name.tail,DesPath);
    }
    std::ifstream in(baseFile, std::ios::binary);
    std::ofstream out(newFile, std::ios::binary);
    out<<in.rdbuf();
}
void CopyToPathRename(std::string baseFile, std::string DesPath, std::string newName)
{
    std::string newFile=CreateFile(newName,DesPath);
    if(newFile==baseFile)
    {
        newName=newName+"_copy";
        newFile=CreateFile(newName,DesPath);
    }
    std::ifstream in(baseFile, std::ios::binary);
    std::ofstream out(newFile, std::ios::binary);
    out<<in.rdbuf();
}

