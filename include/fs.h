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

    bool createDirectory(const std::string& path) {
        if (!validPath(path) || directories_.count(path) || files_.count(path) ||
            !directories_.count(parentOf(path))) return false;
        journal_.push_back("BEGIN mkdir " + path);
        directories_.insert(path);
        journal_.push_back("COMMIT mkdir " + path);
        return true;
    }

    bool writeFile(const std::string& path, const std::string& contents,
                   Allocation method, const std::string& owner = "user") {
        if (!validPath(path) || owner.empty() || !directories_.count(parentOf(path))) return false;
        const auto existing = files_.find(path);
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
        FileEntry entry{fileId, path, owner, contents, method, blocks, dataBlocks, indexBlock};
        files_[path] = entry;
        if (existing == files_.end()) ++nextFileId_;
        journal_.push_back("COMMIT write " + path);
        return true;
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
        out << "Directory " << path << "\nType  Name                         Blocks  Allocation\n";
        for (const std::string& directory : directories_)
            if (directory != path && parentOf(directory) == path)
                out << "DIR   " << directory.substr(path == "/" ? 1 : path.size() + 1) << "\n";
        for (const auto& pair : files_) if (parentOf(pair.first) == path) {
            const FileEntry& file = pair.second;
            const std::size_t slash = file.path.find_last_of('/');
            const std::string name = file.path.substr(slash + 1);
            out << "FILE  " << std::left << std::setw(28) << name << std::right
                << std::setw(6) << file.dataBlocks.size() << "  "
                << allocationName(file.allocation) << '\n';
        }
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
