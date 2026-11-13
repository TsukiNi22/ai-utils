/**************************************************************\
Edition:
##  @date 15/10/2026 by @author Tsukini

File Name:
##  @file Requirements.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Exception
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Settings.hpp"
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"
#include "cluster/Web.hpp"
#include <cstdlib>
#include <set>

_cold cluster::Requirement cluster::setting_requirement(cluster::Manager& manager, const cluster::Setting& setting, const std::string& value)
{
    // what this value needs and is missing: a program (installable: its package), the router refusing, tailscale off...
    const std::string key = setting.section + "." + setting.key;
    const std::string first = cluster::split(cluster::trim(value), ' ').empty() ? "" : cluster::split(cluster::trim(value), ' ')[0];
    if (key == "web.remote" && value == "router") {
        const cluster::Config& c = manager.config();
        const std::string forward = "forward TCP " + std::to_string(c.web.remotePort > 0 ? c.web.remotePort : c.web.port) + " to this computer, port "
            + std::to_string(c.web.port) + ", in the admin page of the box";
        if (!cluster::has_command("upnpc")) return {"upnpc missing: the port of the router can't be opened by itself (UPnP); install it, or " + forward, "miniupnpc"};
        if (const std::optional<cluster::WebStatus> web = cluster::web_status(manager); web && web->on) {
            if (web->remoteState.find("no public address") != std::string::npos)
                return {"its public address unknown: the box answers no UPnP (turn it on in its admin page), or give a dynamic DNS name in Name from outside", ""};
            if (web->remoteState.find("open the port") != std::string::npos)
                return {"the router did not open the port (its UPnP off or refused): turn UPnP on in the box, or " + forward, ""};
        }
        return {};
    }
    if (key == "web.remote" && value == "tailscale") {
        if (!cluster::has_command("tailscale")) return {"tailscale missing: install it here and on the phone, log both in with the same account", "tailscale"};
        const std::optional<cluster::WebStatus> web = cluster::web_status(manager);
        if (web && web->on && web->remoteState.find("not connected") != std::string::npos) return {"tailscale not connected: log this computer in (sudo tailscale up)", ""};
        if (web && web->on && !web->certificateWhy.empty()) return {"no real certificate (a browser warning): " + web->certificateWhy, ""};
        return {};
    }
    if ((key == "ui.editor" || key == "ui.image_viewer" || key == "ui.terminal" || setting.section == "commands") && !first.empty() && !first.starts_with("/")
        && !cluster::has_command(first))
        return {first + " not found (not installed, or not in the PATH)", ""};
    if ((key == "ui.editor" || key == "ui.image_viewer" || key == "ui.terminal" || setting.section == "commands") && first.starts_with("/")
        && !std::filesystem::exists(first))
        return {first + " does not exist", ""};
    return {};
}

_cold std::vector<std::string> cluster::install_command(const std::string& package)
{
    // the package manager of this system: its install, without questions
    if (cluster::has_command("dnf")) return {"dnf", "install", "-y", package};
    if (cluster::has_command("apt-get")) return {"apt-get", "install", "-y", package};
    if (cluster::has_command("pacman")) return {"pacman", "-S", "--noconfirm", package};
    if (cluster::has_command("zypper")) return {"zypper", "--non-interactive", "install", package};
    return {};
}

_cold std::string cluster::install_package(const std::string& package, cluster::Manager* manager)
{
    // as root by the password window of the desktop (pkexec), its output live in the popup of the interfaces; its error
    // told, empty: installed; the packages a setting needs only (the password window names dnf, not the package)
    static const std::set<std::string> known = {"miniupnpc", "tailscale"};
    if (!known.contains(package)) return "not a package a setting needs (" + package + "): install it yourself";
    const std::vector<std::string> command = cluster::install_command(package);
    if (command.empty()) return "no package manager known here (dnf, apt-get, pacman, zypper): install " + package + " yourself";
    if (!cluster::has_command("pkexec")) return "pkexec missing: run sudo " + command[0] + " install " + package + " yourself";
    std::vector<std::string> args = {"pkexec"};
    args.insert(args.end(), command.begin(), command.end());
    const std::uint64_t shown = manager != nullptr ? manager->progressBegin("Installing " + package) : 0;
    std::string line;
    for (const std::string& part: args)
        line += (line.empty() ? "" : " ") + part;
    if (shown != 0) manager->progressStep(shown, line + " (its password window)");
    const int code = cluster::run_logged(args, [manager, shown](const std::string& out) {if (shown != 0) manager->progressLog(shown, out);});
    const std::string error = code == 0 ? "" : package + " not installed (" + (code == 126 || code == 127 ? std::string("the password refused or the window closed")
        : "exit " + std::to_string(code)) + ")";
    if (shown != 0 && error.empty()) manager->progressEnd(shown, true, package + " installed");
    if (shown != 0 && !error.empty()) {
        manager->progressEnd(shown, false, error, false); // its ✗, then its failure popup
        (void)manager->fail("Install " + package, line, error, [manager, package]() {(void)cluster::install_package(package, manager);});
    }
    return error;
}
