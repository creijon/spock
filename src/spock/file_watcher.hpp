// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <efsw/efsw.hpp>

#include <mutex>
#include <set>
#include <string>
#include <utility>

namespace spock
{
    class FileWatcher final : private efsw::FileWatchListener
    {
    public:
        FileWatcher(std::string const& path);

        FileWatcher(FileWatcher const&) = delete;
        FileWatcher& operator=(FileWatcher const&) = delete;
        
        virtual ~FileWatcher();

        // Retrieve the set of modified files since the last call to takeModified().
        // This will clear the internal set of modified files.
        std::set<std::string> takeModified();

    private:
        void handleFileAction(
            efsw::WatchID watchid,
            const std::string& dir,
            const std::string& filename,
            efsw::Action action,
            const std::string& oldFilename) override;

        std::mutex m_mutex;
        std::set<std::string> m_modified;
        efsw::FileWatcher m_fileWatcher;
        efsw::WatchID m_watchID;
    };
} // namespace spock
