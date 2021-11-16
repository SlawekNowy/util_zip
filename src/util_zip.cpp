/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_zip.h"
#include "util_zip.h"
#include <cstring>
#include <zip.h>
#pragma optimize("",off)
std::unique_ptr<ZIPFile> ZIPFile::Open(const void *zipData,size_t size)
{
	zip_error_t err;
	auto *zs = zip_source_buffer_create(zipData,size,0,&err); // Will be released automatically
	if(!zs)
		return nullptr;
	auto *z = zip_open_from_source(zs,0,&err);
	if(!z)
		return nullptr;
	return std::unique_ptr<ZIPFile>(new ZIPFile(z));
}
std::unique_ptr<ZIPFile> ZIPFile::Open(const std::string &filePath,OpenFlags openFlags)
{
	decltype(umath::to_integral(openFlags)) flags = 0;
	if((openFlags &OpenFlags::CreateIfNotExist) != OpenFlags::None)
		flags |= ZIP_CREATE;
	if((openFlags &OpenFlags::ErrorIfExist) != OpenFlags::None)
		flags |= ZIP_EXCL;
	if((openFlags &OpenFlags::StrictChecks) != OpenFlags::None)
		flags |= ZIP_CHECKCONS;
	if((openFlags &OpenFlags::TruncateIfExit) != OpenFlags::None)
		flags |= ZIP_TRUNCATE;
	if((openFlags &OpenFlags::ReadOnly) != OpenFlags::None)
		flags |= ZIP_RDONLY;
	int err = -1;
	auto *z = zip_open(filePath.c_str(),umath::to_integral(openFlags),&err);
	if(z == nullptr)
		return nullptr;
	return std::unique_ptr<ZIPFile>(new ZIPFile(z));
}

ZIPFile::ZIPFile(zip *z)
	: m_zip(z)
{}

ZIPFile::~ZIPFile()
{
	zip_close(m_zip);
}

bool ZIPFile::GetFileList(std::vector<std::string> &outFileList)
{
	auto n = zip_get_num_entries(m_zip,0);
	outFileList.reserve(n);
	for(zip_uint64_t i=0;i<n;++i)
	{
		auto *name = zip_get_name(m_zip,i,0); // TODO: This may be a UTF8-string!
		outFileList.push_back(name);
	}
	return true;
}

#include "LzmaLib.h"
#include <array>
#include <cinttypes>
#include <sharedutils/util_ifile.hpp>
#define LZMA_ID	(('A'<<24)|('M'<<16)|('Z'<<8)|('L'))
#pragma pack(push,1)
struct lzma_header_t
{
	uint32_t id;
	uint32_t actualSize; // always little endian
	uint32_t lzmaSize; // always little endian
	std::array<uint8_t,5> properties;
};
#pragma pack(pop)
bool ZIPFile::ReadFile(const std::string &fileName,std::vector<uint8_t> &outData,std::string &outErr)
{
	struct zip_stat st;
	zip_stat_init(&st);
	if(zip_stat(m_zip,fileName.data(),0,&st) != 0)
		return false;
	auto *f = zip_fopen_index(m_zip,st.index,0);
	if(!f)
	{
#if 0
		// LZMA Compression; Does *not* work
		f = zip_fopen_index(m_zip,st.index,ZIP_FL_COMPRESSED);
		if(f)
		{
			std::vector<uint8_t> compressedData;
			compressedData.resize(st.comp_size);
			zip_fread(f,compressedData.data(),compressedData.size());
			zip_fclose(f);

			ufile::VectorFile vf {std::move(compressedData)};
			auto id = vf.ufile::IFile::Read<uint32_t>();
			vf.Seek(vf.Tell() -sizeof(uint32_t));
			if(id == LZMA_ID)
			{
				auto lzmaHeader = vf.ufile::IFile::Read<lzma_header_t>();

				auto &compressedData = vf.GetVector();
				outData.resize(st.size);
				size_t decompressedSize = outData.size();
				size_t compressedSize = compressedData.size();
				auto result = LzmaUncompress(
					outData.data(),&decompressedSize,
					compressedData.data(),&compressedSize,lzmaHeader.properties.data(),lzmaHeader.properties.size()
				);
				std::cout<<"RESULT: "<<result<<std::endl;
			}
		}
#endif
		auto *err = zip_get_error(m_zip);
		if(err)
			outErr = std::to_string(err->zip_err);
		return false;
	}
	outData.resize(st.size);
	zip_fread(f,outData.data(),outData.size());
	return zip_fclose(f) == 0;
}

void ZIPFile::AddFile(const std::string &fileName,const void *data,uint64_t size,bool bOverwrite)
{
	m_data.push_back(std::make_unique<std::vector<uint8_t>>(size));
	auto &vdata = *m_data.back();
	memcpy(vdata.data(),data,size);

	auto *zipSrc = zip_source_buffer(m_zip,vdata.data(),size,0);
	zip_flags_t flags = ZIP_FL_ENC_GUESS;
	if(bOverwrite == true)
		flags |= ZIP_FL_OVERWRITE;
	zip_file_add(m_zip,fileName.c_str(),zipSrc,flags);
}

void ZIPFile::AddFile(const std::string &fileName,const std::string &data,bool bOverwrite)
{
	AddFile(fileName,data.data(),data.size(),bOverwrite);
}
#pragma optimize("",on)
