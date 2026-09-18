#include <cstdio>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>


class LSMTree {
    std::map<std::string, std::string> memtable;
    std::vector<std::string> sstables; // SSTable 文件路径：最旧 -> 最新
    std::string dataDir;
    std::size_t limit;
    int nextId = 0;

    std::string newSSTablePath() {
        return dataDir + "/sstable_" + std::to_string(nextId++) + ".db";
    }

    static void writeSSTable(
        const std::string& file,
        const std::map<std::string, std::string>& sortedData) {
        std::ofstream out(file, std::ios::trunc);
        if (!out) throw std::runtime_error("cannot create " + file);

        // std::map 已按 key 排序，因此写入 SSTable 后，文件中的记录也是有序的。
        for (const auto& entry : sortedData) {
            out << std::quoted(entry.first) << ' '
                << std::quoted(entry.second) << '\n';
        }
    }

    // 找到时返回 true，并通过 foundValue 带回 value；未找到或文件打不开时返回 false。
    static bool findInSSTable(
        const std::string& file,
        const std::string& wantedKey,
        std::string& foundValue) {
        std::ifstream in(file);
        if (!in) return false; // 后续报错修改中新增：处理文件打开失败。

        std::string key;
        std::string value;

        while (in >> std::quoted(key) >> std::quoted(value)) {
            if (key == wantedKey) {
                foundValue = value;
                return true;
            }
            // SSTable 按 key 排序；当前 key 已更大时，后面不可能再出现目标 key。
            if (key > wantedKey) break;
        }
        return false;
    }

public:
    explicit LSMTree(std::string directory = "lsm_data", std::size_t flushLimit = 3)
        : dataDir(std::move(directory)), limit(flushLimit) {
        _mkdir(dataDir.c_str()); // 目录已经存在时也不影响本示例继续运行。
    }

    void put(const std::string& key, const std::string& value) {
        memtable[key] = value;
        if (memtable.size() >= limit) flush();
    }

    void flush() {
        if (memtable.empty()) return;

        std::string file = newSSTablePath();
        writeSSTable(file, memtable);
        sstables.push_back(file);
        memtable.clear();
        std::cout << "flush -> " << file << '\n';
    }

    std::string get(const std::string& key) const {
        // 新数据优先：先查 MemTable，再从最新的 SSTable 向最旧的查找。
        auto memoryValue = memtable.find(key);
        if (memoryValue != memtable.end()) return memoryValue->second;

        for (auto table = sstables.rbegin(); table != sstables.rend(); ++table) {
            std::string value;
            if (findInSSTable(*table, key, value)) return value;
        }
        return "NOT_FOUND";
    }

    void compact() {
        flush();
        if (sstables.size() <= 1) return;

        std::map<std::string, std::string> merged;

        // 按“最旧 -> 最新”合并，因此后读到的新值会覆盖旧值。
        for (const std::string& file : sstables) {
            std::ifstream in(file);
            std::string key;
            std::string value;
            while (in >> std::quoted(key) >> std::quoted(value)) {
                merged[key] = value;
            }
        }

        std::string compacted = newSSTablePath();
        writeSSTable(compacted, merged);

        // 新 SSTable 已包含最终结果，用它替换所有旧的不可变文件。
        for (const std::string& oldTable : sstables) {
            std::remove(oldTable.c_str());
        }
        sstables = {compacted};
        std::cout << "compact -> " << compacted << '\n';
    }

    void showSSTables() const {
        std::cout << "SSTables:\n";
        for (const std::string& file : sstables) {
            std::cout << "  " << file << '\n';
        }
    }
};

int main() {
    LSMTree db("lsm_data", 2);

    db.put("name", "Alice");
    db.put("age", "20");         // 达到阈值，刷新为 SSTable 0
    db.showSSTables();
    std::cout << "before compaction, name = " << db.get("name") << '\n';

    db.put("name", "Bob");       // name 的更新版本
    db.put("city", "Shanghai"); // 达到阈值，刷新为 SSTable 1

    db.compact();
    db.showSSTables();
    std::cout << "after compaction, name = " << db.get("name") << '\n';
}

