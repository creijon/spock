// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <efsw/efsw.hpp>

#include <functional>
#include <string>

namespace spock
{
    class Watcher : public efsw::FileWatchListener
    {
    public:
        Watcher(std::string const& path)
        {
            m_watchID = m_fileWatcher.addWatch(path, this);
            m_fileWatcher.watch();
        }

        ~Watcher()
        {
            m_fileWatcher.removeWatch(m_watchID);
        }

    protected:
        virtual void fileModified(std::string const& filename) = 0;

    private:
        void handleFileAction(
            efsw::WatchID watchid,
            const std::string& dir,
            const std::string& filename,
            efsw::Action action,
            const std::string& oldFilename) override
        {
            if (action == efsw::Actions::Modified)
            {
                fileModified(filename);
            }
        }

        efsw::FileWatcher m_fileWatcher;
        efsw::WatchID m_watchID;
    };
} // namespace spock
