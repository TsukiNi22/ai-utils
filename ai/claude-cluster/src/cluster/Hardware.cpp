/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Hardware.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Hardware.hpp"
#include "cluster/Tools.hpp"
#include <sys/statvfs.h>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <set>

/* tools */
namespace {

namespace fs = std::filesystem;

_cold std::string read_(const fs::path& path)
{
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// "MemTotal:  16057180 kB" -> kB of the key, -1 when missing
_cold long meminfo_(const std::string& text, const std::string& key)
{
    const std::size_t at = text.find(key + ":");
    if (at == std::string::npos) return -1;
    return std::strtol(text.c_str() + at + key.size() + 1, nullptr, 10);
}

// the avg10 of "some" / "full" in /proc/pressure/<name>: -1 when missing
_cold double psi_(const std::string& procDir, const std::string& name, const std::string& kind)
{
    const std::string text = read_(fs::path(procDir) / "pressure" / name);
    const std::size_t at = text.find(kind + " avg10=");
    if (at == std::string::npos) return -1;
    return std::strtod(text.c_str() + at + kind.size() + 7, nullptr);
}

_cold std::string field_(const std::string& line, const std::string& key)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos || cluster::trim(line.substr(0, colon)) != key) return "";
    return cluster::trim(line.substr(colon + 1));
}

} // namespace

/* the scan */
_cold cluster::Hardware cluster::hardware_scan(const std::string& procDir, const std::string& sysDir)
{
    cluster::Hardware hw;
    hw.at = cluster::now();
    const bool real = procDir == "/proc" && sysDir == "/sys";
    // cpu: model, threads, physical cores (the distinct package / core pairs)
    std::istringstream cpuinfo(read_(fs::path(procDir) / "cpuinfo"));
    std::string line;
    int threads = 0;
    std::string package;
    std::set<std::string> physical;
    while (std::getline(cpuinfo, line)) {
        if (hw.cpu.empty()) hw.cpu = field_(line, "model name");
        if (!field_(line, "processor").empty()) ++threads;
        if (const std::string id = field_(line, "physical id"); !id.empty()) package = id;
        if (const std::string core = field_(line, "core id"); !core.empty()) physical.insert(package + "/" + core);
    }
    if (threads == 0 && real) threads = static_cast<int>(std::thread::hardware_concurrency());
    hw.cores = std::max(1, threads);
    hw.physical = physical.empty() ? hw.cores : static_cast<int>(physical.size());
    // memory
    const std::string mem = read_(fs::path(procDir) / "meminfo");
    hw.memMb = std::max(0L, meminfo_(mem, "MemTotal")) / 1024;
    hw.swapMb = std::max(0L, meminfo_(mem, "SwapTotal")) / 1024;
    // disk of the data folder
    struct statvfs disk{};
    if (real && ::statvfs(cluster::data_dir().c_str(), &disk) == 0) {
        hw.diskTotalGb = static_cast<long>(static_cast<double>(disk.f_blocks) * static_cast<double>(disk.f_frsize) / 1073741824.0);
        hw.diskFreeGb = static_cast<long>(static_cast<double>(disk.f_bavail) * static_cast<double>(disk.f_frsize) / 1073741824.0);
    }
    // gpus: nvidia-smi (the best: name, memory, driver), rocm-smi, else lspci
    if (real) {
        std::set<std::string> known;
        if (cluster::has_command("nvidia-smi")) {
            const cluster::Captured out = cluster::capture({"nvidia-smi", "--query-gpu=name,memory.total,driver_version", "--format=csv,noheader,nounits"}, "", 8);
            std::istringstream rows(out.code == 0 ? out.out : "");
            while (std::getline(rows, line)) {
                const std::vector<std::string> cols = cluster::split(line, ',');
                if (cols.size() < 3) continue;
                hw.gpus.push_back({"nvidia", cluster::trim(cols[0]), static_cast<int>(std::strtol(cluster::trim(cols[1]).c_str(), nullptr, 10)), cluster::trim(cols[2])});
                known.insert("nvidia");
            }
        }
        if (cluster::has_command("rocm-smi")) {
            const cluster::Captured out = cluster::capture({"rocm-smi", "--showproductname"}, "", 8);
            std::istringstream rows(out.code == 0 ? out.out : "");
            while (std::getline(rows, line)) {
                const std::size_t at = line.find("Card series:");
                if (at == std::string::npos) continue;
                hw.gpus.push_back({"amd", cluster::trim(line.substr(at + 12)), 0, ""});
                known.insert("amd");
            }
        }
        if (cluster::has_command("lspci")) {
            const cluster::Captured out = cluster::capture({"lspci"}, "", 8);
            std::istringstream rows(out.code == 0 ? out.out : "");
            while (std::getline(rows, line)) {
                if (line.find("VGA compatible") == std::string::npos && line.find("3D controller") == std::string::npos && line.find("Display controller") == std::string::npos) continue;
                const std::string lower = cluster::lower(line);
                const std::string vendor = lower.find("nvidia") != std::string::npos ? "nvidia" : lower.find("amd") != std::string::npos || lower.find("radeon") != std::string::npos ? "amd"
                    : lower.find("intel") != std::string::npos ? "intel" : "other";
                if (known.contains(vendor)) continue; // already told by its own tool
                const std::size_t colon = line.find(": ");
                hw.gpus.push_back({vendor, colon == std::string::npos ? line : cluster::trim(line.substr(colon + 2)), 0, ""});
            }
        }
    }
    // power and temperature: cheap files of sysfs
    hw.power = "unknown";
    std::error_code error;
    for (const fs::directory_entry& entry: fs::directory_iterator(fs::path(sysDir) / "class" / "power_supply", error)) {
        const std::string type = cluster::trim(read_(entry.path() / "type"));
        if (type == "Mains" || type == "USB") {
            if (cluster::trim(read_(entry.path() / "online")) == "1") hw.power = "ac";
            else if (hw.power == "unknown") hw.power = "battery";
        } else if (type == "Battery") {
            const std::string capacity = cluster::trim(read_(entry.path() / "capacity"));
            if (!capacity.empty()) hw.batteryPercent = static_cast<int>(std::strtol(capacity.c_str(), nullptr, 10));
        }
    }
    for (const fs::directory_entry& entry: fs::directory_iterator(fs::path(sysDir) / "class" / "thermal", error)) {
        if (!entry.path().filename().string().starts_with("thermal_zone")) continue;
        const std::string value = cluster::trim(read_(entry.path() / "temp"));
        if (!value.empty()) hw.maxTempC = std::max(hw.maxTempC, std::strtod(value.c_str(), nullptr) / 1000.0);
    }
    return hw;
}

/* the live sample */
_cold cluster::Host cluster::host_sample(const std::string& procDir)
{
    cluster::Host host;
    host.at = cluster::now_ms();
    std::istringstream load(read_(fs::path(procDir) / "loadavg"));
    load >> host.load1 >> host.load5 >> host.load15;
    const std::string mem = read_(fs::path(procDir) / "meminfo");
    host.memTotalMb = std::max(0L, meminfo_(mem, "MemTotal")) / 1024;
    long available = meminfo_(mem, "MemAvailable");
    if (available < 0) available = std::max(0L, meminfo_(mem, "MemFree")) + std::max(0L, meminfo_(mem, "Cached")); // an old kernel
    host.memAvailMb = available / 1024;
    host.swapTotalMb = std::max(0L, meminfo_(mem, "SwapTotal")) / 1024;
    host.swapUsedMb = std::max(0L, host.swapTotalMb - std::max(0L, meminfo_(mem, "SwapFree")) / 1024);
    int cores = 0;
    std::istringstream cpuinfo(read_(fs::path(procDir) / "cpuinfo"));
    std::string line;
    while (std::getline(cpuinfo, line))
        if (!field_(line, "processor").empty()) ++cores;
    if (cores == 0 && procDir == "/proc") cores = static_cast<int>(std::thread::hardware_concurrency());
    host.cores = std::max(1, cores);
    host.loadPerCore = host.load1 / host.cores;
    host.psiCpu = psi_(procDir, "cpu", "some");
    host.psiMem = psi_(procDir, "memory", "some");
    host.psiMemFull = psi_(procDir, "memory", "full");
    host.psiIo = psi_(procDir, "io", "some");
    host.psiIoFull = psi_(procDir, "io", "full");
    return host;
}

/* shown */
_cold cluster::Json cluster::hardware_json(const cluster::Hardware& hw)
{
    cluster::Json gpus = cluster::Json::array();
    for (const cluster::Gpu& gpu: hw.gpus)
        gpus.push_back({{"vendor", gpu.vendor}, {"name", gpu.name}, {"vram_mb", gpu.vramMb}, {"driver", gpu.driver}});
    return {{"scan_version", hw.version}, {"claude_cluster", CLUSTER_VERSION}, {"at", hw.at}, {"cpu", hw.cpu}, {"threads", hw.cores}, {"cores", hw.physical},
        {"mem_mb", hw.memMb}, {"swap_mb", hw.swapMb}, {"disk_total_gb", hw.diskTotalGb}, {"disk_free_gb", hw.diskFreeGb}, {"gpus", gpus},
        {"power", hw.power}, {"battery_percent", hw.batteryPercent}, {"max_temp_c", hw.maxTempC}};
}

_cold cluster::Json cluster::host_json(const cluster::Host& host)
{
    return {{"at", host.at}, {"load1", host.load1}, {"load5", host.load5}, {"load15", host.load15}, {"load_per_core", host.loadPerCore}, {"cores", host.cores},
        {"mem_total_mb", host.memTotalMb}, {"mem_avail_mb", host.memAvailMb}, {"swap_total_mb", host.swapTotalMb}, {"swap_used_mb", host.swapUsedMb},
        {"psi_cpu", host.psiCpu}, {"psi_mem", host.psiMem}, {"psi_mem_full", host.psiMemFull}, {"psi_io", host.psiIo}, {"psi_io_full", host.psiIoFull}};
}

_cold std::string cluster::hardware_text(const cluster::Hardware& hw)
{
    std::string text = std::to_string(hw.cores) + " threads (" + std::to_string(hw.physical) + " cores) " + hw.cpu + ", " + std::to_string(hw.memMb / 1024) + " GB RAM, "
        + std::to_string(hw.swapMb / 1024) + " GB swap";
    if (hw.diskTotalGb > 0) text += ", disk " + std::to_string(hw.diskFreeGb) + " / " + std::to_string(hw.diskTotalGb) + " GB free";
    for (const cluster::Gpu& gpu: hw.gpus)
        text += ", GPU " + gpu.name + (gpu.vramMb > 0 ? " " + std::to_string(gpu.vramMb / 1024) + " GB" : "");
    if (hw.power != "unknown") text += ", power " + hw.power;
    return text;
}
