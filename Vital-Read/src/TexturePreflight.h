#pragma once

#include <mygui/MyGUI_DataManager.h>
#include <mygui/MyGUI_RenderManager.h>

#include <deque>
#include <map>
#include <string>

// GUI locations are registered as resource roots. Probe through the same
// MyGUI DataManager as ImageBox instead of treating names as filesystem paths.
// Startup resource locations stay fixed for the lifetime of a DataManager.
namespace vital_texture
{
class PreflightCache
{
    struct Entry
    {
        bool exists;
        bool reported;
        Entry() : exists(false), reported(false) {}
    };
    enum { kCapacity = 256 };
    MyGUI::DataManager* manager_;
    std::map<std::string, Entry> entries_;
    std::deque<std::string> order_;

    Entry& Add(const std::string& name)
    {
        if (entries_.size() == kCapacity)
        {
            entries_.erase(order_.front());
            order_.pop_front();
        }
        order_.push_back(name);
        return entries_[name];
    }

public:
    PreflightCache() : manager_(0) {}

    bool Exists(const std::string& name)
    {
        MyGUI::DataManager* const manager = MyGUI::DataManager::getInstancePtr();
        if (!manager) return false; // Do not cache "not initialized" as missing.
        if (manager_ != manager)
        {
            entries_.clear();
            order_.clear();
            manager_ = manager;
        }
        // ImageBox also accepts existing/manual textures without a source file.
        // This is a resource-map lookup, not a disk probe. Never retain its pointer.
        MyGUI::RenderManager* const renderer = MyGUI::RenderManager::getInstancePtr();
        try { if (renderer && renderer->getTexture(name)) return true; }
        catch (...) { return true; } // Preserve the loader's actual diagnostic.
        std::map<std::string, Entry>::const_iterator it = entries_.find(name);
        if (it != entries_.end()) return it->second.exists;

        // A probe failure must not suppress a real load and its diagnostic.
        bool exists;
        try { exists = manager->isDataExist(name); }
        catch (...) { return true; }
        Add(name).exists = exists;
        return exists;
    }

    bool ReportMissingOnce(const std::string& name)
    {
        std::map<std::string, Entry>::iterator it = entries_.find(name);
        Entry& entry = it != entries_.end() ? it->second : Add(name);
        if (entry.reported) return false;
        entry.reported = true;
        return true;
    }
};
}

