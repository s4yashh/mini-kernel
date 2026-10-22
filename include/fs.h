#ifndef MINI_KERNEL_FS_H
#define MINI_KERNEL_FS_H

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace filesystem {

enum class Allocation { CONTIGUOUS, LINKED, INDEXED };
enum class Access { SEQUENTIAL, DIRECT };

inline const char* allocationName(Allocation allocation) {
    switch (allocation) {
        case Allocation::CONTIGUOUS: return "contiguous";
        case Allocation::LINKED: return "linked";
        case Allocation::INDEXED: return "indexed";
    }
    return "unknown";
}

struct FileEntry {
    int id = 0;
    std::string path, owner, contents;
    Allocation allocation = Allocation::CONTIGUOUS;
    std::vector<int> blocks;
    std::vector<int> dataBlocks;
    int indexBlock = -1;
    int writes = 0;                  // number of times the contents were written
};

// A small, in-memory file system with directory, protection, allocation, and journal demos.
class FileSystem {
    int blockSize_;
    std::vector<int> blockOwner_;
    std::map<std::string, FileEntry> files_;
    std::set<std::string> directories_{"/"};
    std::vector<std::string> journal_;
    int nextFileId_ = 1;

    static std::string parentOf(const std::string& path) {
        const std::size_t slash = path.find_last_of('/');
        if (slash == 0) return "/";
        return path.substr(0, slash);
    }

    static bool validPath(const std::string& path) {
        if (path.size() <= 1 || path.front() != '/' || path.back() == '/' ||
            path.find("//") != std::string::npos || path.find('\n') != std::string::npos)
            return false;
        std::istringstream parts(path.substr(1));
        std::string part;
        while (std::getline(parts, part, '/'))
            if (part.empty() || part == "." || part == "..") return false;
        return true;
    }

    static int blocksRequired(std::size_t bytes, int blockSize) {
        if (bytes == 0) return 1;
        return static_cast<int>((bytes + static_cast<std::size_t>(blockSize) - 1) /
                                static_cast<std::size_t>(blockSize));
    }

    std::string normalise(const std::string& path) const {
        if (path.empty()) return std::string();
        if (path[0] != '/') return std::string();
        std::vector<std::string> parts;
        std::istringstream input(path);
        std::string part;
        while (std::getline(input, part, '/')) {
            if (part.empty() || part == ".") continue;
            if (part == "..") {
                if (parts.empty()) return std::string();
                parts.pop_back();
                continue;
            }
            parts.push_back(part);
        }
        if (parts.empty()) return "/";
        std::string result;
        for (const std::string& item : parts) result += "/" + item;
        return result;
    }

    bool allocate(std::vector<int>& owners, int fileId, int dataCount,
                  Allocation method, std::vector<int>& allBlocks,
                  std::vector<int>& dataBlocks, int& indexBlock) const {
        const int indexCount = method == Allocation::INDEXED ? 1 : 0;
        const int required = dataCount + indexCount;
        if (required > static_cast<int>(owners.size())) return false;
        std::vector<int> selected;
        if (method == Allocation::CONTIGUOUS) {
            int run = 0, start = -1;
            for (std::size_t i = 0; i < owners.size(); ++i) {
                if (owners[i] == 0) {
                    if (run == 0) start = static_cast<int>(i);
                    ++run;
                    if (run == required) break;
                } else run = 0;
            }
            if (run < required) return false;
            for (int i = 0; i < required; ++i) selected.push_back(start + i);
        } else {
            for (std::size_t i = 0; i < owners.size() &&
                 selected.size() < static_cast<std::size_t>(required); ++i)
                if (owners[i] == 0) selected.push_back(static_cast<int>(i));
            if (selected.size() != static_cast<std::size_t>(required)) return false;
        }
        for (int block : selected) owners[static_cast<std::size_t>(block)] = fileId;
        allBlocks = selected;
        dataBlocks.assign(selected.begin(), selected.begin() + dataCount);
        indexBlock = method == Allocation::INDEXED ? selected.back() : -1;
        return true;
    }

    void requireAccess(const FileEntry& file, const std::string& user) const {
        if (user != "admin" && user != file.owner)
            throw std::runtime_error("Access denied for user " + user + " to " + file.path);
    }

public:
    explicit FileSystem(int totalBlocks = 64, int blockSize = 16)
        : blockSize_(blockSize), blockOwner_(static_cast<std::size_t>(totalBlocks), 0) {
        if (totalBlocks <= 0 || blockSize <= 0)
            throw std::invalid_argument("Disk block count and block size must be positive");
    }

    bool exists(const std::string& path) const {
        return files_.count(path) != 0 || directories_.count(path) != 0;
    }
    bool isDirectory(const std::string& path) const { return directories_.count(path) != 0; }
    bool isFile(const std::string& path) const { return files_.count(path) != 0; }
    int fileCount() const { return static_cast<int>(files_.size()); }
    int directoryCount() const { return static_cast<int>(directories_.size()); }
    int totalBlocks() const { return static_cast<int>(blockOwner_.size()); }
    int usedBlocks() const { return totalBlocks() - freeBlocks(); }
    int blockSize() const { return blockSize_; }
    int blocksOf(const std::string& path) const {
        const auto found = files_.find(path);
        return found == files_.end() ? 0 : static_cast<int>(found->second.blocks.size());
    }
    double utilisation() const {
        return totalBlocks() == 0 ? 0.0 : 100.0 * usedBlocks() / totalBlocks();
    }
    std::string currentOwner(const std::string& path) const {
        const auto found = files_.find(path);
        return found == files_.end() ? std::string() : found->second.owner;
    }
    std::string resolve(const std::string& currentDirectory, const std::string& path) const {
        if (path.empty()) return std::string();
        if (path[0] == '/') return normalise(path);
        return normalise(currentDirectory + "/" + path);
    }

    bool createDirectory(const std::string& path) {
        if (!validPath(path) || directories_.count(path) || files_.count(path) ||
            !directories_.count(parentOf(path))) return false;
        journal_.push_back("BEGIN mkdir " + path);
        directories_.insert(path);
        journal_.push_back("COMMIT mkdir " + path);
        return true;
    }

    // Creates every missing directory on the way to `path` ("mkdir -p").
    bool createDirectories(const std::string& path) {
        const std::string clean = normalise(path);
        if (clean.empty() || clean == "/") return clean == "/";
        std::size_t start = 0;
        while (true) {
            const std::size_t slash = clean.find('/', start + 1);
            const std::string part = slash == std::string::npos ? clean : clean.substr(0, slash);
            if (!directories_.count(part) && !createDirectory(part)) return false;
            if (slash == std::string::npos) break;
            start = slash;
        }
        return true;
    }

    bool writeFile(const std::string& path, const std::string& contents,
                   Allocation method, const std::string& owner = "user") {
        if (!validPath(path) || owner.empty() || !directories_.count(parentOf(path))) return false;
        if (directories_.count(path)) return false;   // a directory already owns this name
        const auto existing = files_.find(path);
        if (existing != files_.end() && owner != "admin" && owner != existing->second.owner)
            return false;                              // only the owner (or admin) may overwrite
        const int fileId = existing == files_.end() ? nextFileId_ : existing->second.id;
        std::vector<int> temporaryOwners = blockOwner_;
        if (existing != files_.end())
            for (int block : existing->second.blocks) temporaryOwners[static_cast<std::size_t>(block)] = 0;
        const int dataCount = blocksRequired(contents.size(), blockSize_);
        std::vector<int> blocks, dataBlocks;
        int indexBlock = -1;
        if (!allocate(temporaryOwners, fileId, dataCount, method, blocks, dataBlocks, indexBlock))
            return false;

        journal_.push_back("BEGIN write " + path);
        blockOwner_.swap(temporaryOwners);
        FileEntry entry{fileId, path, existing == files_.end() ? owner : existing->second.owner,
                        contents, method, blocks, dataBlocks, indexBlock,
                        existing == files_.end() ? 1 : existing->second.writes + 1};
        files_[path] = entry;
        if (existing == files_.end()) ++nextFileId_;
        journal_.push_back("COMMIT write " + path);
        return true;
    }

    // Appends text (allocating new blocks when the file grows).
    bool appendFile(const std::string& path, const std::string& extra,
                    Allocation method, const std::string& owner = "user") {
        const std::string contents = readFile(path, owner) + extra;
        return writeFile(path, contents, method, owner);
    }

    // Copies a file through the simulated block allocator. The destination must
    // be a new path (or an existing file owned by the same user).
    bool copyFile(const std::string& source, const std::string& destination,
                  Allocation method, const std::string& user = "user") {
        if (source == destination) return false;
        if (!files_.count(source) || !validPath(destination)) return false;
        if (directories_.count(destination)) return false;
        if (!directories_.count(parentOf(destination))) return false;
        std::string contents;
        try { contents = readFile(source, user); }
        catch (const std::exception&) { return false; }
        const std::string owner = files_[source].owner;
        if (!writeFile(destination, contents, method, owner)) return false;
        journal_.push_back("COPY " + source + " -> " + destination);
        return true;
    }

    // Renames a file in place (same directory) or moves it to another directory.
    bool renameFile(const std::string& from, const std::string& to, const std::string& user = "user") {
        if (from == to) return false;
        const auto found = files_.find(from);
        if (found == files_.end()) return false;
        if (!validPath(to) || directories_.count(to) || files_.count(to)) return false;
        if (!directories_.count(parentOf(to))) return false;
        requireAccess(found->second, user);
        journal_.push_back("BEGIN rename " + from + " -> " + to);
        FileEntry entry = found->second;
        entry.path = to;
        files_.erase(found);
        files_[to] = entry;
        journal_.push_back("COMMIT rename " + from + " -> " + to);
        return true;
    }

    bool moveFile(const std::string& from, const std::string& to, const std::string& user = "user") {
        return renameFile(from, to, user);
    }

    void metadata(const std::string& path, std::ostream& out = std::cout) const {
        if (directories_.count(path)) {
            out << "Directory " << path << "\n"
                << "  Entries:";
            int entries = 0;
            for (const std::string& directory : directories_)
                if (directory != path && parentOf(directory) == path) { out << ' ' << directory; ++entries; }
            for (const auto& item : files_) if (parentOf(item.first) == path) { out << ' ' << item.first; ++entries; }
            out << "\n  Entry count: " << entries << '\n';
            return;
        }
        const auto found = files_.find(path);
        if (found == files_.end()) throw std::out_of_range("File does not exist: " + path);
        const FileEntry& file = found->second;
        out << "File " << file.path << "\n"
            << "  Type        : regular file\n"
            << "  Owner       : " << file.owner << "\n"
            << "  Size        : " << file.contents.size() << " bytes\n"
            << "  Allocation  : " << allocationName(file.allocation) << "\n"
            << "  Blocks      : " << file.blocks.size() << " ("
            << file.dataBlocks.size() << " data, "
            << (file.indexBlock >= 0 ? 1 : 0) << " index)\n"
            << "  Block list  : ";
        for (std::size_t i = 0; i < file.blocks.size(); ++i)
            out << (i ? "," : "") << file.blocks[i];
        out << "\n  Index block : " << (file.indexBlock >= 0 ? std::to_string(file.indexBlock) : "none")
            << "\n  Writes      : " << file.writes << "\n";
    }

    // Directory-table row used by list()/application reports.
    void describe(const std::string& path, std::ostream& out = std::cout) const {
        metadata(path, out);
    }

    bool deleteFile(const std::string& path, const std::string& user = "user") {
        const auto found = files_.find(path);
        if (found == files_.end()) return false;
        requireAccess(found->second, user);
        journal_.push_back("BEGIN delete " + path);
        for (int block : found->second.blocks) blockOwner_[static_cast<std::size_t>(block)] = 0;
        files_.erase(found);
        journal_.push_back("COMMIT delete " + path);
        return true;
    }

    bool deleteDirectory(const std::string& path) {
        if (path == "/" || directories_.count(path) == 0) return false;
        for (const auto& item : files_) if (parentOf(item.first) == path) return false;
        for (const std::string& directory : directories_)
            if (directory != path && parentOf(directory) == path) return false;
        journal_.push_back("BEGIN rmdir " + path);
        directories_.erase(path);
        journal_.push_back("COMMIT rmdir " + path);
        return true;
    }

    std::string readFile(const std::string& path, const std::string& user = "user",
                         Access access = Access::SEQUENTIAL, std::size_t offset = 0,
                         std::size_t length = std::string::npos) const {
        const auto found = files_.find(path);
        if (found == files_.end()) throw std::out_of_range("File does not exist: " + path);
        requireAccess(found->second, user);
        if (access == Access::SEQUENTIAL) return found->second.contents;
        if (offset > found->second.contents.size()) throw std::out_of_range("Direct-read offset is past EOF");
        return found->second.contents.substr(offset, length);
    }

    void list(const std::string& path = "/", std::ostream& out = std::cout) const {
        if (!directories_.count(path)) throw std::out_of_range("Directory does not exist: " + path);
        out << "Directory " << path << "\nType  Name                           Size  Blocks  Allocation  Owner\n";
        for (const std::string& directory : directories_)
            if (directory != path && parentOf(directory) == path)
                out << "DIR   " << std::left << std::setw(29) << directory.substr(path == "/" ? 1 : path.size() + 1)
                    << std::right << std::setw(6) << "-" << std::setw(8) << "-"
                    << std::setw(12) << "-" << "  -\n";
        for (const auto& pair : files_) if (parentOf(pair.first) == path) {
            const FileEntry& file = pair.second;
            const std::size_t slash = file.path.find_last_of('/');
            const std::string name = file.path.substr(slash + 1);
            out << "FILE  " << std::left << std::setw(29) << name << std::right
                << std::setw(7) << file.contents.size() << std::setw(8) << file.blocks.size()
                << "  " << std::left << std::setw(12) << allocationName(file.allocation)
                << file.owner << '\n';
        }
        out << "Total: " << fileCount() << " file(s), " << directoryCount()
            << " director(ies), " << usedBlocks() << '/' << totalBlocks()
            << " blocks used (" << std::fixed << std::setprecision(1) << utilisation() << "%)\n";
    }

    const FileEntry& entry(const std::string& path) const {
        const auto found = files_.find(path);
        if (found == files_.end()) throw std::out_of_range("File does not exist: " + path);
        return found->second;
    }

    int freeBlocks() const {
        return static_cast<int>(std::count(blockOwner_.begin(), blockOwner_.end(), 0));
    }

    void printBlockMap(std::ostream& out = std::cout) const {
        out << "Block map: ";
        for (std::size_t i = 0; i < blockOwner_.size(); ++i)
            out << (i ? " " : "") << i << ':' << (blockOwner_[i] == 0 ? "free" : "F" + std::to_string(blockOwner_[i]));
        out << "\nFree blocks: " << freeBlocks() << '/' << blockOwner_.size() << '\n';
    }

    void printJournal(std::ostream& out = std::cout) const {
        out << "Journal\n";
        for (const std::string& event : journal_) out << "  " << event << '\n';
    }

    // Leaves only an intent record to model a crash between transaction begin and commit.
    void simulateInterruptedOperation(const std::string& operation) {
        journal_.push_back("BEGIN " + operation);
    }

    // Rolls back incomplete intent records; committed operations remain applied in this run.
    void recover(std::ostream& out = std::cout) {
        std::set<std::string> pending;
        for (const std::string& event : journal_) {
            if (event.compare(0, 6, "BEGIN ") == 0) pending.insert(event.substr(6));
            else if (event.compare(0, 7, "COMMIT ") == 0) pending.erase(event.substr(7));
        }
        if (pending.empty()) out << "Recovery: journal is clean; all recorded operations committed.\n";
        else {
            out << "Recovery: rolling back incomplete operation(s):\n";
            for (const std::string& op : pending) out << "  " << op << '\n';
            journal_.erase(std::remove_if(journal_.begin(), journal_.end(), [&](const std::string& e) {
                return e.compare(0, 6, "BEGIN ") == 0 && pending.count(e.substr(6));
            }), journal_.end());
        }
    }
};

// Demonstrates directory operations, all allocation methods, access methods, and protection.
inline void demo(std::ostream& out = std::cout) {
    out << "\nIn-memory file-system demo\n";
    FileSystem fs(32, 16);
    fs.createDirectory("/apps");
    fs.createDirectory("/docs");
    fs.writeFile("/apps/notes.txt", "Mini Kernel Simulator notes", Allocation::CONTIGUOUS);
    fs.writeFile("/docs/report.txt", "Operating systems file allocation methods", Allocation::LINKED);
    fs.writeFile("/docs/index.txt", "Indexed allocation stores block pointers in an index block", Allocation::INDEXED);
    fs.list("/");
    fs.list("/docs");
    out << "Sequential read: " << fs.readFile("/apps/notes.txt") << '\n';
    out << "Direct read at offset 0: "
        << fs.readFile("/docs/report.txt", "user", Access::DIRECT, 0, 10) << '\n';
    try { fs.readFile("/apps/notes.txt", "guest"); }
    catch (const std::runtime_error& error) { out << "Protection check: " << error.what() << '\n'; }
    fs.printBlockMap(out);
    fs.printJournal(out);
    fs.simulateInterruptedOperation("write /docs/crash.tmp");
    fs.recover(out);
}

} // namespace filesystem

#endif
