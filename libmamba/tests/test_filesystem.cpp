#include <gtest/gtest.h>

#include "mamba/core/mamba_fs.hpp"

namespace mamba
{
    TEST(u8path, consistent_encoding)
    {
        const auto utf8_string = u8"joël";
        const fs::u8path filename(utf8_string);
        EXPECT_EQ(filename.string(), utf8_string);

        const fs::u8path file_path = fs::temp_directory_path() / filename;
        EXPECT_EQ(file_path.filename().string(), utf8_string);
    }

    TEST(u8path, string_stream_encoding)
    {
        const auto utf8_string = u8"joël";
        const fs::u8path filename(utf8_string);
        std::stringstream stream;
        stream << filename;
        EXPECT_EQ(stream.str(), utf8_string);

        fs::u8path path_read;
        stream.seekg(0);
        stream >> path_read;
        EXPECT_EQ(path_read.string(), utf8_string);
    }
}
