/* roseMemoryMappedFile.h: memory mapped files           -*- C++ -*-

   Copyright (C) 1996-2018 Christian Schenk

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

#ifndef MIKTEX_ROSEMEMORYMAPPEDFILE_HPP
#define MIKTEX_ROSEMEMORYMAPPEDFILE_HPP

#include "miktex/Core/MemoryMappedFile.h"
#include "../../Fndb/fndbmem.h"
#include <unordered_map>

#include "rose_filesystem_dll.hpp"

CORE_INTERNAL_BEGIN_NAMESPACE;

#define FNDB_2AREA_ALIGN		8

class roseMemoryMappedFile: public MiKTeX::Core::MemoryMappedFile
{
public:
	roseMemoryMappedFile();
	~roseMemoryMappedFile() override;

private:
	void* MIKTEXTHISCALL Open(const MiKTeX::Util::PathName& path, bool readWrite) override;
	void MIKTEXTHISCALL Close() override;
	void* MIKTEXTHISCALL Resize(size_t newSize) override
	{
		VALIDATE(false, "it is fake memory-mapped file, only for read *.fndb, not support write.");
		return nullptr;
	}
	void* GetPtr() const override
	{
		return file_data_size_ != 0? file_data_: nullptr;
	}

	std::string GetName() const override
	{
		// name: (OpenFileMapping api require)Provide a unique name for the Windows file mapping object.
		// how to generate name?
		// input path: C:/Users/ancientcc/document/roseppp/launcher/miktex/sandbox/miktex/data/le/9107dba4c41645517d74e16a72f74243.fndb-5
		// =>
		// output name: cusersancientccdocumentroseapplaunchermiktexsandboxmiktexdatale9107dba4c41645517d74e16a72f74243.fndb-5
		
		// roseMemoryMappedFile is a fake memory mapping. Linux returns empty, so let's return empty here as well.
		return null_str;
	}

	size_t GetSize() const override
	{
		return file_data_size_;
	}

	void MIKTEXTHISCALL Flush() override
	{
		VALIDATE(false, "it is fake memory-mapped file, only for read *.fndb, not support write.");
	}

private:
	bool parse_3area(uint8_t* data, int fsize);

private:
	// const uint8_t null_byte = 0;
	// const unsigned FNDB_PAGESIZE = 0x1000;

	std::string file_name_;
	uint8_t* file_data_;
	int file_data_size_;

	FileNameDatabaseHeader* header_;
	bool verify_files_;
	std::set<std::string> files_;
	typedef std::unordered_map<std::string, FndbByteOffset> StringMap;
	StringMap str_map_;
};

CORE_INTERNAL_END_NAMESPACE;

#endif
