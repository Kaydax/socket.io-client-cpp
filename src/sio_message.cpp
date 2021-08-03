#include "sio_message.h"

namespace sio
{

    MapStr::Container& MapStr::map()
    {
        return map_;
    }

    const sio::MapStr::Container& MapStr::map() const
    {
        return map_;
    }

    void MapStr::enumerate(EnumFunc f, void* data) const
    {
        for (auto it : map_) {
            if (!f(it.first.c_str(), it.second.c_str(), data))
                break;
        }
    }

    int MapStr::size() const
    {
        return map_.size();
    }

    const char* MapStr::get(const char* key) const
    {
        if (map_.count(key))
            return map_.at(key).c_str();
        return nullptr;
    }

    bool MapStr::has(const char* key) const
    {
        return map_.count(key);
    }

    bool MapStr::del(const char* key)
    {
        return map_.erase(key);
    }

    bool MapStr::set(const char* key, const char* value)
    {
        bool ret = false;
        if (key) {
            if (value)
                map_[key] = value;
            else
                map_.erase(key);
            ret = true;
        }
        return ret;
    }

    MapMsg::MapMsg()
    {

    }

    sio::MapMsg::Container& MapMsg::map()
    {
        return map_;
    }

    const sio::MapMsg::Container& MapMsg::map() const
    {
        return map_;
    }

    void MapMsg::enumerate(EnumFunc f, void* data) const
    {
        for (auto it : map_) {
            if (!f(it.first.c_str(), it.second, data))
                break;
        }
    }

    int MapMsg::size() const
    {
        return map_.size();
    }

    sio::msgptr MapMsg::get(const char* key) const
    {
        if (map_.count(key))
            return map_.at(key);
        return nullptr;
    }

    bool MapMsg::has(const char* key) const
    {
        return map_.count(key);
    }

    bool MapMsg::del(const char* key)
    {
        return map_.erase(key);
    }

    bool MapMsg::set(const char* key, msgptr value)
    {
        bool ret = false;
        if (key) {
            if (value)
                map_[key] = value;
            else
                map_.erase(key);
            ret = true;
        }
        return ret;
    }

}