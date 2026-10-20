/**************************************************************\
Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Hardware.hpp

File Description:
##  What this computer is (the scan: cores, RAM, swap, disk, GPU,
##  power) and what it does now (the live sample of /proc: load,
##  memory, swap, pressure stall information) for the resource
##  orchestrator
\**************************************************************/

#ifndef CLUSTER_HARDWARE_H
    #define CLUSTER_HARDWARE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "Types.hpp"            // cluster::Json
    #include <cstdint>              // std::int64_t
    #include <string>               // std::string
    #include <vector>               // std::vector

    //----------------------------------------------------------------//
    /* DEFINE */

    #define HARDWARE_SCAN_VERSION 1     // the format of the scan (saved with it)

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Gpu {
    std::string vendor;             // nvidia | amd | intel | other
    std::string name;
    int vramMb = 0;                 // 0: unknown
    std::string driver;
};

struct Hardware {
    int version = HARDWARE_SCAN_VERSION;
    std::int64_t at = 0;            // unix s
    std::string cpu;
    int cores = 1;                  // logical (threads)
    int physical = 1;
    long memMb = 0;
    long swapMb = 0;
    long diskTotalGb = 0;           // the data folder's file system
    long diskFreeGb = 0;
    std::vector<cluster::Gpu> gpus;
    std::string power;              // ac | battery | unknown
    int batteryPercent = -1;
    double maxTempC = -1;
};

// What the computer does now (every field -1: not available)
struct Host {
    std::int64_t at = 0;            // unix ms
    double load1 = 0;
    double load5 = 0;
    double load15 = 0;
    double loadPerCore = 0;
    int cores = 1;
    long memTotalMb = 0;
    long memAvailMb = 0;
    long swapTotalMb = 0;
    long swapUsedMb = 0;
    double psiCpu = -1;             // PSI avg10 (% of the time some task stalled), cpu / memory / io; "full": all of them
    double psiMem = -1;
    double psiMemFull = -1;
    double psiIo = -1;
    double psiIoFull = -1;
};

//----------------------------------------------------------------//
/* FUNCTION */

// procDir / sysDir: /proc and /sys, or a fake tree (tests); the commands (nvidia-smi, lspci, rocm-smi) only run on the real ones
_cold _nodiscard cluster::Hardware hardware_scan(const std::string& procDir = "/proc", const std::string& sysDir = "/sys");
_cold _nodiscard cluster::Host host_sample(const std::string& procDir = "/proc");
_cold _nodiscard cluster::Json hardware_json(const cluster::Hardware& hardware);
_cold _nodiscard cluster::Json host_json(const cluster::Host& host);
_cold _nodiscard std::string hardware_text(const cluster::Hardware& hardware);   // one line for a human

} // namespace end

#endif /* CLUSTER_HARDWARE_H */
