/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/10/2026 by @author Tsukini

File Name:
##  @file Taxonomy.cpp

File Description:
##  The taxonomy of the labels of the sessions (see Taxonomy.hpp)
\**************************************************************/

#include "cluster/Taxonomy.hpp"
#include "cluster/Tools.hpp"

#include <algorithm>

//----------------------------------------------------------------//
/* FUNCTION */

_cold std::string cluster::Taxonomy::root(void)
{
    return "global";
}

_cold std::string cluster::Taxonomy::clean(const std::string& name)
{
    const std::string label = cluster::clean_label(name);
    return label == cluster::Taxonomy::root() ? std::string() : label;
}

_cold cluster::Taxonomy cluster::Taxonomy::from_json(const cluster::Json& json)
{
    cluster::Taxonomy taxonomy;
    if (!json.is_object() || !json.contains("parents") || !json["parents"].is_object()) return taxonomy;
    const cluster::Json& listed = json["parents"];
    for (cluster::Json::const_iterator item = listed.begin(); item != listed.end(); ++item) {
        const std::string node = cluster::Taxonomy::clean(item.key());
        const std::string parent = item->is_string() ? cluster::Taxonomy::clean(item->get<std::string>()) : std::string();
        if (!node.empty() && node != parent) taxonomy._parents[node] = parent;
    }
    for (auto &[node, parent]: taxonomy._parents) { // a cycle (a hand-edited file): cut at the node found again
        std::string up = parent;
        for (std::size_t depth = 0; !up.empty() && up != node && depth < 64; ++depth)
            up = taxonomy.parent(up);
        if (up == node) parent.clear();
    }
    return taxonomy;
}

_cold cluster::Json cluster::Taxonomy::json(void) const
{
    cluster::Json parents = cluster::Json::object();
    for (const auto &[node, parent]: this->_parents)
        parents[node] = parent;
    return {{"root", cluster::Taxonomy::root()}, {"parents", parents}};
}

_cold bool cluster::Taxonomy::known(const std::string& node) const
{
    return this->_parents.contains(node);
}

_cold std::string cluster::Taxonomy::parent(const std::string& node) const
{
    const auto it = this->_parents.find(node);
    return it == this->_parents.end() ? std::string() : it->second;
}

_cold std::vector<std::string> cluster::Taxonomy::path(const std::string& node) const
{
    std::vector<std::string> out;
    std::string at = node;
    for (std::size_t depth = 0; !at.empty() && depth < 64; ++depth) {
        out.insert(out.begin(), at);
        at = this->parent(at);
    }
    return out;
}

_cold std::vector<std::string> cluster::Taxonomy::children(const std::string& node) const
{
    std::vector<std::string> out;
    for (const auto &[child, parent]: this->_parents)
        if (parent == node) out.push_back(child);
    return out;
}

_cold std::set<std::string> cluster::Taxonomy::descendants(const std::string& node) const
{
    std::set<std::string> out;
    std::vector<std::string> todo = {node};
    while (!todo.empty()) {
        const std::string at = todo.back();
        todo.pop_back();
        for (const std::string& child: this->children(at))
            if (out.insert(child).second) todo.push_back(child);
    }
    return out;
}

_cold std::vector<std::string> cluster::Taxonomy::nodes(void) const
{
    std::vector<std::string> out;
    for (const auto &[node, parent]: this->_parents)
        out.push_back(node);
    return out;
}

_cold std::string cluster::Taxonomy::primary(const cluster::SessionSpec& spec)
{
    if (!spec.category.empty()) return spec.category;
    return spec.tags.empty() ? std::string() : spec.tags.front();
}

_cold std::vector<std::string> cluster::Taxonomy::group(const cluster::SessionSpec& spec) const
{
    return this->path(cluster::Taxonomy::primary(spec));
}

_cold std::set<std::string> cluster::Taxonomy::scope(const std::string& node, const bool descendants) const
{
    const std::string wanted = cluster::Taxonomy::clean(node);
    std::set<std::string> out = {wanted};
    if (descendants) out.merge(this->descendants(wanted));
    return out;
}

_cold std::size_t cluster::Taxonomy::common_prefix(const std::vector<std::string>& a, const std::vector<std::string>& b)
{
    std::size_t n = 0;
    while (n < a.size() && n < b.size() && a[n] == b[n])
        ++n;
    return n;
}

_cold bool cluster::Taxonomy::under(const cluster::SessionSpec& spec, const std::string& node, const bool descendants) const
{
    const std::string wanted = cluster::Taxonomy::clean(node);
    if (wanted.empty()) return true; // the root: every session
    const std::set<std::string> accepted = this->scope(wanted, descendants);
    if (!spec.category.empty() && accepted.contains(spec.category)) return true;
    return std::any_of(spec.tags.begin(), spec.tags.end(), [&accepted](const std::string& tag) {return accepted.contains(tag);});
}

_cold bool cluster::Taxonomy::setParent(const std::string& node, const std::string& parent, std::string& why)
{
    const std::string name = cluster::Taxonomy::clean(node);
    const std::string up = cluster::Taxonomy::clean(parent);
    if (name.empty()) {
        why = "the root (global) has no parent";
        return false;
    }
    if (name == up) {
        why = name + " cannot be its own parent";
        return false;
    }
    if (!up.empty() && this->descendants(name).contains(up)) {
        why = "cycle refused: " + up + " is below " + name;
        return false;
    }
    this->_parents[name] = up;
    if (!up.empty() && !this->_parents.contains(up)) this->_parents[up] = std::string();
    return true;
}

_cold bool cluster::Taxonomy::rename(const std::string& from, const std::string& to, std::string& why)
{
    const std::string name = cluster::Taxonomy::clean(from);
    const std::string next = cluster::Taxonomy::clean(to);
    if (name.empty() || next.empty()) {
        why = "a node name is needed (global is the root)";
        return false;
    }
    if (name == next) return true;
    if (this->_parents.contains(next)) {
        why = next + " already exists: merge them instead";
        return false;
    }
    const auto it = this->_parents.find(name);
    if (it == this->_parents.end()) return true; // a label of the sessions with no entry: nothing to move here
    const std::string up = it->second;
    this->_parents.erase(it);
    this->_parents[next] = up;
    for (auto &[node, parent]: this->_parents)
        if (parent == name) parent = next;
    return true;
}

_cold bool cluster::Taxonomy::remove(const std::string& node, std::string& why)
{
    const std::string name = cluster::Taxonomy::clean(node);
    if (name.empty()) {
        why = "the root (global) cannot be deleted";
        return false;
    }
    const auto it = this->_parents.find(name);
    if (it == this->_parents.end()) return true;
    const std::string up = it->second;
    this->_parents.erase(it);
    for (auto &[child, parent]: this->_parents) // its children go to its parent
        if (parent == name) parent = up;
    return true;
}

_cold bool cluster::Taxonomy::merge(const std::string& from, const std::string& into, std::string& why)
{
    const std::string name = cluster::Taxonomy::clean(from);
    const std::string target = cluster::Taxonomy::clean(into);
    if (name.empty() || target.empty() || name == target) {
        why = "two different nodes are needed (global is the root)";
        return false;
    }
    if (this->descendants(name).contains(target)) {
        why = "cycle refused: " + target + " is below " + name;
        return false;
    }
    const std::string up = this->parent(name);
    this->_parents.erase(name);
    for (auto &[child, parent]: this->_parents)
        if (parent == name) parent = target;
    if (!this->_parents.contains(target)) this->_parents[target] = up == target ? std::string() : up;
    return true;
}
