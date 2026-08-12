#include "storage.h"

#include <SD.h>
#include <SPIFFS.h>

namespace m5ui {

namespace {

// M5Paper SD wiring.
constexpr int kSdCs = 4;
constexpr int kSdSck = 14;
constexpr int kSdMiso = 13;
constexpr int kSdMosi = 12;

}  // namespace

StorageBackend Storage::Begin(const char* root) {
    _root = root;
    if (!_root.startsWith("/")) _root = "/" + _root;
    if (_root.endsWith("/")) _root.remove(_root.length() - 1);

    SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
    if (SD.begin(kSdCs, SPI, 20000000) && SD.cardType() != CARD_NONE) {
        _backend = StorageBackend::SdCard;
    } else if (SPIFFS.begin(true)) {
        // formatOnFail: the spiffs partition ships unformatted.
        _backend = StorageBackend::Spiffs;
    } else {
        _backend = StorageBackend::None;
        log_e("no storage backend available");
        return _backend;
    }

    MakeDir("");
    log_i("storage: %s, %llu bytes free",
          _backend == StorageBackend::SdCard ? "sd" : "spiffs", FreeBytes());
    return _backend;
}

bool Storage::Remount() {
    const StorageBackend was = _backend;
    SD.end();
    Begin(_root.c_str());
    return was != _backend;
}

String Storage::Resolve(const String& path) const {
    // Reject traversal outright rather than normalising it -- there is no
    // legitimate use of ".." in an app-relative path here.
    if (path.indexOf("..") >= 0) return String();

    String p = path;
    while (p.startsWith("/")) p.remove(0, 1);
    if (p.length() == 0) return _root;
    return _root + "/" + p;
}

// ---------------------------------------------------------------- files ----

bool Storage::Exists(const String& path) {
    const String p = Resolve(path);
    if (p.length() == 0 || !IsReady()) return false;
    return _backend == StorageBackend::SdCard ? SD.exists(p) : SPIFFS.exists(p);
}

uint32_t Storage::Size(const String& path) {
    const String p = Resolve(path);
    if (p.length() == 0 || !IsReady()) return 0;

    File f = _backend == StorageBackend::SdCard ? SD.open(p, FILE_READ)
                                                : SPIFFS.open(p, FILE_READ);
    if (!f) return 0;
    const uint32_t size = (uint32_t)f.size();
    f.close();
    return size;
}

bool Storage::Remove(const String& path) {
    const String p = Resolve(path);
    if (p.length() == 0 || !IsReady()) return false;
    return _backend == StorageBackend::SdCard ? SD.remove(p) : SPIFFS.remove(p);
}

bool Storage::Rename(const String& from, const String& to) {
    const String f = Resolve(from);
    const String t = Resolve(to);
    if (f.length() == 0 || t.length() == 0 || !IsReady()) return false;
    return _backend == StorageBackend::SdCard ? SD.rename(f, t)
                                              : SPIFFS.rename(f, t);
}

bool Storage::MakeDir(const String& path) {
    // SPIFFS is flat -- directories are a naming convention, so this is a
    // no-op there rather than a failure.
    if (_backend != StorageBackend::SdCard) return true;
    const String p = Resolve(path);
    if (p.length() == 0) return false;
    return SD.exists(p) || SD.mkdir(p);
}

String Storage::ReadString(const String& path) {
    const String p = Resolve(path);
    if (p.length() == 0 || !IsReady()) return String();

    File f = _backend == StorageBackend::SdCard ? SD.open(p, FILE_READ)
                                                : SPIFFS.open(p, FILE_READ);
    if (!f) return String();

    String out;
    out.reserve(f.size() + 1);
    while (f.available()) out += (char)f.read();
    f.close();
    return out;
}

size_t Storage::ReadBytes(const String& path, uint8_t* buf, size_t max_len) {
    const String p = Resolve(path);
    if (p.length() == 0 || !IsReady() || buf == nullptr) return 0;

    File f = _backend == StorageBackend::SdCard ? SD.open(p, FILE_READ)
                                                : SPIFFS.open(p, FILE_READ);
    if (!f) return 0;
    const size_t n = f.read(buf, max_len);
    f.close();
    return n;
}

bool Storage::WriteBytes(const String& path, const uint8_t* data, size_t len) {
    const String p = Resolve(path);
    if (p.length() == 0 || !IsReady()) return false;

    // Write-temp-then-rename, so an interrupted save cannot corrupt the target.
    const String tmp = p + ".tmp";
    File f = _backend == StorageBackend::SdCard ? SD.open(tmp, FILE_WRITE)
                                                : SPIFFS.open(tmp, FILE_WRITE);
    if (!f) return false;

    const size_t written = f.write(data, len);
    f.flush();
    f.close();
    if (written != len) {
        _backend == StorageBackend::SdCard ? SD.remove(tmp) : SPIFFS.remove(tmp);
        return false;
    }

    if (_backend == StorageBackend::SdCard) {
        if (SD.exists(p)) SD.remove(p);
        return SD.rename(tmp, p);
    }
    if (SPIFFS.exists(p)) SPIFFS.remove(p);
    return SPIFFS.rename(tmp, p);
}

bool Storage::WriteString(const String& path, const String& contents) {
    return WriteBytes(path, (const uint8_t*)contents.c_str(), contents.length());
}

bool Storage::Append(const String& path, const String& contents) {
    const String p = Resolve(path);
    if (p.length() == 0 || !IsReady()) return false;

    File f = _backend == StorageBackend::SdCard ? SD.open(p, FILE_APPEND)
                                                : SPIFFS.open(p, FILE_APPEND);
    if (!f) return false;
    const size_t n = f.print(contents);
    f.close();
    return n == contents.length();
}

// -------------------------------------------------------------- listing ----

void Storage::ForEach(const String& dir,
                      const std::function<bool(const FileInfo&)>& fn) {
    const String p = Resolve(dir);
    if (p.length() == 0 || !IsReady()) return;

    File d = _backend == StorageBackend::SdCard ? SD.open(p) : SPIFFS.open(p);
    if (!d || !d.isDirectory()) {
        if (d) d.close();
        return;
    }

    for (File entry = d.openNextFile(); entry; entry = d.openNextFile()) {
        FileInfo info;
        info.name = String(entry.name());
        // Backends report either a bare name or a full path; normalise to the
        // leaf so callers see the same thing on SD and SPIFFS.
        const int slash = info.name.lastIndexOf('/');
        if (slash >= 0) info.name = info.name.substring(slash + 1);
        info.size = (uint32_t)entry.size();
        info.is_directory = entry.isDirectory();
        info.modified = (uint32_t)entry.getLastWrite();
        entry.close();

        if (!fn(info)) break;
    }
    d.close();
}

std::vector<FileInfo> Storage::List(const String& dir, uint16_t offset,
                                    uint16_t limit) {
    std::vector<FileInfo> out;
    if (limit == 0) return out;
    out.reserve(limit);

    uint16_t seen = 0;
    ForEach(dir, [&](const FileInfo& info) {
        if (seen++ < offset) return true;
        out.push_back(info);
        return (uint16_t)out.size() < limit;
    });
    return out;
}

uint16_t Storage::CountEntries(const String& dir) {
    uint16_t n = 0;
    ForEach(dir, [&](const FileInfo&) {
        n++;
        return true;
    });
    return n;
}

// ----------------------------------------------------------------- info ----

uint64_t Storage::TotalBytes() const {
    switch (_backend) {
        case StorageBackend::SdCard: return SD.totalBytes();
        case StorageBackend::Spiffs: return SPIFFS.totalBytes();
        default:                     return 0;
    }
}

uint64_t Storage::UsedBytes() const {
    switch (_backend) {
        case StorageBackend::SdCard: return SD.usedBytes();
        case StorageBackend::Spiffs: return SPIFFS.usedBytes();
        default:                     return 0;
    }
}

uint64_t Storage::FreeBytes() const {
    const uint64_t total = TotalBytes();
    const uint64_t used = UsedBytes();
    return total > used ? total - used : 0;
}

}  // namespace m5ui
