/* roseMemoryMappedFile.cpp: memory mapped files

   Copyright (C) 1996-2021 Christian Schenk

   This file is part of the MiKTeX Core Library.

   The MiKTeX Core Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The MiKTeX Core Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the MiKTeX Core Library; if not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#include "../../miktex_Core_config.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <miktex/Core/BufferSizes>
#include <miktex/Trace/Trace>
#include <miktex/Util/PathNameUtil>
#include <miktex/Core/Paths.h>

#include "../../miktex_Core_internal.h"

#include "roseMemoryMappedFile.hpp"
#include "../../Utils/inliners.h"

#include <SDL_log.h>
#include <SDL_timer.h>
#include <miktex/Core/Fndb>

using namespace std;

using namespace MiKTeX::Core;
using namespace MiKTeX::Trace;
using namespace MiKTeX::Util;

MemoryMappedFile* MemoryMappedFile::Create()
{
    return new roseMemoryMappedFile;
}

roseMemoryMappedFile::roseMemoryMappedFile()
    : file_data_(nullptr)
    , file_data_size_(0)
    , header_(nullptr)
    , verify_files_(true) // game_config::os == os_windows
{
    VALIDATE(sizeof(FileNameDatabaseHeader) == 40, null_str);
    VALIDATE(sizeof(FileNameDatabaseRecord) == 16, null_str);
}

roseMemoryMappedFile::~roseMemoryMappedFile()
{
    if (file_data_ != nullptr) {
        VALIDATE(file_data_size_ == 0, null_str);
        free(file_data_);
    }
}

void* roseMemoryMappedFile::Open(const PathName& path_, bool readWrite)
{
    const std::string norm_path = utils::normalize_path(path_.ToString());
    const size_t pos = norm_path.find(MIKTEX_PATH_FNDB_DIR);
    VALIDATE(pos != std::string::npos && norm_path.find(".fndb-") != std::string::npos, null_str);
    VALIDATE(!readWrite, null_str);
    VALIDATE(pos != std::string::npos && pos > 1, null_str);

    VALIDATE(file_data_ == nullptr, null_str);
    VALIDATE(file_data_size_ == 0, null_str);

    int len;
    file_data_ = Fndb::Create_nofile(path_, PathName(norm_path.substr(0, pos - 1)), len);
    VALIDATE(file_data_ != nullptr && len > 0, null_str);
    VALIDATE(parse_3area(file_data_, len), null_str);
 
    file_data_size_ = len;

    // write_file("c:/ddksample/test.fndb-5", (const char*)file_data_, len);

    file_name_ = path_.ToString();
    return file_data_;
}

void roseMemoryMappedFile::Close()
{
    VALIDATE(file_data_ != nullptr, null_str);
    VALIDATE(file_data_size_ > 0, null_str);

    free(file_data_);
    file_data_ = nullptr;

    file_data_size_ = 0;
    header_ = nullptr;
}

bool roseMemoryMappedFile::parse_3area(uint8_t* data, int fsize)
{
    VALIDATE(data != nullptr, null_str);
    VALIDATE(fsize > 0, null_str);

    uint32_t start_ticks = SDL_GetTicks();

    if (fsize < sizeof(*header_)) {
        // Not a file name database file (wrong size).;
        return false;
    }

    header_ = reinterpret_cast<FileNameDatabaseHeader*>(data);
    // foEnd = static_cast<FndbByteOffset>(mmap->GetSize());

    if (header_->signature != FileNameDatabaseHeader::Signature) {
        // Not a file name database file (wrong signature).
        return false;
    }
    if (header_->version != FileNameDatabaseHeader::Version) {
        // Unknown file name database file version.
        return false;
    }

    FndbByteOffset desire_foStrings = header_->foTable + header_->numFiles * sizeof(FileNameDatabaseRecord);
    desire_foStrings = posix_align_ceil2(desire_foStrings, FNDB_2AREA_ALIGN);

    if (header_->foTable != sizeof(FileNameDatabaseHeader) || header_->foStrings != desire_foStrings) {
        return false;
    }

    int desire_fsize = posix_align_ceil2(header_->size, FNDB_PAGESIZE);
    if (header_->foStrings > header_->size || fsize != desire_fsize) {
        return false;
    }

    std::set<FndbByteOffset> str_offsets;

    std::string tmp_str;
    const uint8_t* strpool = data + header_->foStrings;
    const int strpool_size = header_->size - header_->foStrings;
    int start_pos = 0;
    bool has_empty_str = false;
    int spaces = 0;
    for (int at = 0; at < strpool_size; at ++) {
        uint8_t ch = strpool[at];
        if (ch == '\0') {
            if (at - start_pos > 0) {
                tmp_str.assign((const char*)strpool + start_pos, at - start_pos);
            } else {
                SDL_Log("found empty. 0x%x, times: %i", header_->foStrings + at, spaces ++);
                if (has_empty_str) {
                    // return false;
                    int ii = 0;
                }
                has_empty_str = true;
                tmp_str.clear();
            }
            FndbByteOffset fo = header_->foStrings + start_pos;
            str_map_[tmp_str] = fo;
            str_offsets.insert(fo);

            start_pos = at + 1;
        }
    }

    char filename2[256];
    std::string filename;
    std::string directory;
    std::string info;
    const FileNameDatabaseRecord* table = (FileNameDatabaseRecord*)(data + header_->foTable);
    for (size_t idx = 0; idx < header_->numFiles; ++idx) {
        const FileNameDatabaseRecord& rec = table[idx];
        if (rec.foFileName < 0 || (int)rec.foFileName >= fsize || 
            rec.foDirectory < 0 || (int)rec.foDirectory >= fsize ||
            rec.foInfo < 0 || (int)rec.foInfo >= fsize) {
            return false;
        }

        if (str_offsets.count(rec.foFileName) == 0 || str_offsets.count(rec.foDirectory) == 0 || str_offsets.count(rec.foInfo) == 0) {
            return false;
        }

        if (verify_files_) {
            filename.assign((const char*)data + rec.foFileName);
            directory.assign((const char*)data + rec.foDirectory);
            info.assign((const char*)data + rec.foInfo);

            // SDL_Log("#%i file: %s, dir: %s, info: %s", (int)idx, filename.c_str(), directory.c_str(), info.c_str());

            SDL_snprintf(filename2, sizeof(filename2), "%s/%s", directory.c_str(), filename.c_str());
            VALIDATE(files_.count(filename2) == 0, null_str);
            files_.insert(filename2);
        }
    }
    if (verify_files_) {
        VALIDATE(files_.size() == header_->numFiles, null_str);
    }

    SDL_Log("parse_3area cost %u ms", SDL_GetTicks() - start_ticks);
    return true;
}