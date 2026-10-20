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
##  @file Manager-Taxonomy.cpp

File Description:
##  The taxonomy of the labels of the sessions kept by the manager (taxonomy.json in the data dir, replicated to every interface)
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Manager.hpp"
#include "cluster/Tools.hpp"

#include <algorithm>

_cold static std::filesystem::path taxonomy_file_(void)
{
    return cluster::data_dir() / "taxonomy.json";
}

_cold void cluster::Manager::loadTaxonomy_(void)
{
    const std::optional<cluster::Json> json = cluster::read_json(taxonomy_file_());
    if (!json) return;
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_taxonomy = cluster::Taxonomy::from_json(*json);
}

_cold void cluster::Manager::saveTaxonomy_(void) const
{
    cluster::Json json;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        json = this->_taxonomy.json();
    }
    try {
        cluster::write_json(taxonomy_file_(), json);
    } catch (const utils::exception::IException&) {} // written again at the next change
}

_cold cluster::Taxonomy cluster::Manager::taxonomy(void) const
{
    if (this->_replica) return cluster::Taxonomy::from_json(this->replicaState("taxonomy")); // an interface of the core
    std::lock_guard<std::mutex> lock(this->_mutex);
    return this->_taxonomy;
}

_cold cluster::Json cluster::Manager::taxonomyJson(void) const
{
    // every node with its parent, then the labels used by the sessions (alive and archived) with their number of sessions below
    cluster::Json json = this->taxonomy().json();
    json["edit"] = this->_config.taxonomyEdit;
    return json;
}

_cold void cluster::Manager::relabel_(const std::string& from, const std::string& to)
{
    // a label renamed, merged or removed (to empty): on every session that has it, alive or in the trash
    const std::string a = cluster::clean_label(from);
    const std::string b = cluster::clean_label(to);
    std::vector<cluster::SessionSpec> specs;
    for (const cluster::Snapshot& snap: this->list(false))
        specs.push_back(snap.spec);
    for (const cluster::SessionSpec& spec: this->trash())
        specs.push_back(spec);
    for (const cluster::SessionSpec& spec: specs) {
        const bool inCategory = spec.category == a;
        const bool inTags = std::find(spec.tags.begin(), spec.tags.end(), a) != spec.tags.end();
        if (!inCategory && !inTags) continue;
        std::optional<std::string> category;
        std::optional<std::vector<std::string>> tags;
        if (inCategory) category = b;
        if (inTags) {
            std::vector<std::string> next = spec.tags;
            for (std::string& tag: next)
                if (tag == a) tag = b;
            std::erase(next, std::string());
            tags = cluster::clean_tags(next);
        }
        try {
            this->label(spec.id, category, tags);
        } catch (const utils::exception::IException&) {} // a session gone meanwhile
    }
}

_cold void cluster::Manager::taxonomyEdit(const std::string& action, const std::string& node, const std::string& other, const bool strip)
{
    if (this->_replica) { // an interface: the core changes it, the next sync brings it back
        (void)this->call({{"cmd", "taxonomy"}, {"action", action}, {"node", node}, {"parent", other}, {"to", other}, {"strip", strip}});
        return;
    }
    std::string why;
    bool done = false;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (action == "set_parent") done = this->_taxonomy.setParent(node, other, why);
        else if (action == "rename") done = this->_taxonomy.rename(node, other, why);
        else if (action == "merge") done = this->_taxonomy.merge(node, other, why);
        else if (action == "delete") done = this->_taxonomy.remove(node, why);
        else why = "unknown action " + action + " (list, set_parent, rename, delete, merge)";
    }
    if (!done) throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidArgument, why);
    if (action == "rename" || action == "merge") this->relabel_(node, other);
    else if (action == "delete" && strip) this->relabel_(node, std::string());
    this->saveTaxonomy_();
    ++this->_version;
}

_cold std::vector<std::string> cluster::Manager::groupPath(const cluster::SessionSpec& spec) const
{
    return spec.global ? std::vector<std::string>() : this->taxonomy().group(spec);
}
