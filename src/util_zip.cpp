/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_zip.h"
#include "util_zip.h"
#include <cstring>
#include <zip.h>

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
