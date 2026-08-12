#pragma once
// Unified persistence over SD and SPIFFS (issue #89).
//
// The device boots fine with no SD card, so an SD-only design makes any app
// useless without one. Backend selection is automatic: SD when a card is
// present, SPIFFS otherwise -- and the 3.4 MB spiffs partition declared in
// default_16MB.csv is currently unused, so it is free capacity.
//
// Every path is relative to a defined root and validated, so app code cannot
// walk out of its own directory.

#include <Arduino.h>

#include <functional>
#include <vector>

namespace m5ui {

enum class StorageBackend : uint8_t { None = 0, SdCard, Spiffs };

struct FileInfo {
    String name;
    uint32_t size = 0;
    uint32_t modified = 0; // unix time, 0 when the backend has no timestamps
    bool is_directory = false;
};

class Storage {
   public:
    // Mounts the best available backend under `root`. Returns the one chosen.
    StorageBackend Begin(const char* root = "/app");

    StorageBackend Backend() const {
        return _backend;
    }
    bool IsReady() const {
        return _backend != StorageBackend::None;
    }
    const String& Root() const {
        return _root;
    }

    // ------------------------------------------------------------- files --
    bool Exists(const String& path);
    uint32_t Size(const String& path);
    bool Remove(const String& path);
    bool Rename(const String& from, const String& to);
    bool MakeDir(const String& path);

    String ReadString(const String& path);
    size_t ReadBytes(const String& path, uint8_t* buf, size_t max_len);

    // Atomic: writes to `<path>.tmp`, then renames. A power loss mid-save
    // leaves the previous version intact rather than a truncated note.
    bool WriteString(const String& path, const String& contents);
    bool WriteBytes(const String& path, const uint8_t* data, size_t len);
    bool Append(const String& path, const String& contents);

    // ----------------------------------------------------------- listing --
    // Bounded: reads at most `limit` entries starting at `offset`, so a
    // VirtualList (#42) can page a large directory without materialising it.
    std::vector<FileInfo> List(const String& dir, uint16_t offset = 0,
                               uint16_t limit = 64);
    uint16_t CountEntries(const String& dir);

    // Streaming variant for callers that only need one pass and no vector.
    void ForEach(const String& dir, const std::function<bool(const FileInfo&)>& fn);

    // -------------------------------------------------------------- info --
    uint64_t TotalBytes() const;
    uint64_t UsedBytes() const;
    uint64_t FreeBytes() const;

    // Re-checks for a card that was inserted after boot. Returns true when the
    // backend changed.
    bool Remount();

   private:
    // Joins with the root and rejects "..", absolute escapes and empty names.
    // Returns an empty string when the path is not allowed.
    String Resolve(const String& path) const;

    StorageBackend _backend = StorageBackend::None;
    String _root = "/app";
};

}  // namespace m5ui
