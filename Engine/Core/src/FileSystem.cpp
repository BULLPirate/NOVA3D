#include <Nova/Core/FileSystem.h>

#include <fstream>
#include <sstream>

namespace Nova {

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool CreateDirectories(const std::filesystem::path& path, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    return true;
}

FileIOResult ReadTextFile(const std::filesystem::path& path) {
    FileIOResult result;
    std::ifstream in(path);
    if (!in) {
        result.Error = "cannot open " + path.generic_string();
        return result;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in && !in.eof()) {
        result.Error = "failed reading " + path.generic_string();
        return result;
    }
    result.Text = buffer.str();
    result.Ok = true;
    return result;
}

FileIOResult WriteTextFile(const std::filesystem::path& path, std::string_view text) {
    FileIOResult result;
    std::string dirError;
    if (!path.parent_path().empty() && !CreateDirectories(path.parent_path(), dirError)) {
        result.Error = dirError;
        return result;
    }
    std::ofstream out(path);
    if (!out) {
        result.Error = "cannot write " + path.generic_string();
        return result;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) {
        result.Error = "failed writing " + path.generic_string();
        return result;
    }
    result.Ok = true;
    return result;
}

} // namespace Nova
