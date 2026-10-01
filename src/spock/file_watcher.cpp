// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "file_watcher.hpp"

namespace spock
{
    FileWatcher::FileWatcher(std::string const& path)
    {
        m_watchID = m_fileWatcher.addWatch(path, this);
        m_fileWatcher.watch();
    }

    FileWatcher::~FileWatcher()
    {
        m_fileWatcher.removeWatch(m_watchID);
    }

    std::set<std::string> FileWatcher::takeModified()
    {
        std::lock_guard lock(m_mutex);
        return std::exchange(m_modified, {});
    }

    void FileWatcher::handleFileAction(
        efsw::WatchID watchid,
        const std::string& dir,
        const std::string& filename,
        efsw::Action action,
        const std::string& oldFilename)
    {
        if (action == efsw::Actions::Modified)
        {
            std::lock_guard lock(m_mutex);
            m_modified.insert(filename);
        }
    }
} // namespace spock
