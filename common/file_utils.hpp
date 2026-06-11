#pragma once
#include <vector>
#include <fstream>
#include <iostream>

class FileUtils
{
public:
    static bool readBinaryFile(const char *filename, std::vector<char> &data)
    {
        data.clear();

        std::ifstream file(filename, std::ios::binary);
        if (!file)
        {
            std::cout << "open file failed: " << filename << std::endl;
            return false;
        }
        // 移动到文件末尾，获取文件大小
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();

        file.seekg(0, std::ios::beg);

        if (size <= 0)
        {
            std::cout << "file empty: " << filename << std::endl;
            return false;
        }

        data.resize(static_cast<size_t>(size));

        // 读取文件
        if (!file.read(data.data(), size))
        {
            std::cout << "read file failed: " << filename << std::endl;
            return false;
        }
        return true;
    }

    // vector<char>转为二进制文件
    static bool writeBinaryFile(const char *filename, const std::vector<char> &data)
    {
        std::ofstream file(filename, std::ios::binary);

        if (!file)
        {
            std::cout << "creat file failed: " << filename << std::endl;
            return false;
        }

        if (!data.empty())
        {
            file.write(data.data(), static_cast<std::streamsize>(data.size()));
        }
        return true;
    }
};