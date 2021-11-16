/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_ZIP_H__
#define __UTIL_ZIP_H__

#include <memory>
#include <string>
#include <mathutil/umath.h>
#include <vector>

#undef ReplaceFile

struct zip;
class ZIPFile
{
private:
	ZIPFile(zip *z);
	zip *m_zip;
	std::vector<std::unique_ptr<std::vector<uint8_t>>> m_data;
public:
	enum class OpenFlags : int32_t
	{
		None = 0,
		CreateIfNotExist = 1,
		ErrorIfExist = 2,
		StrictChecks = 4,
		TruncateIfExit = 8,
		ReadOnly = 16
	};
	static std::unique_ptr<ZIPFile> Open(const std::string &filePath,OpenFlags openFlags=OpenFlags::ReadOnly);
	static std::unique_ptr<ZIPFile> Open(const void *zipData,size_t size);
	~ZIPFile();
	void AddFile(const std::string &fileName,const void *data,uint64_t size,bool bOverwrite=true);
	void AddFile(const std::string &fileName,const std::string &data,bool bOverwrite=true);
	bool ReadFile(const std::string &fileName,std::vector<uint8_t> &outData,std::string &outErr);
	bool GetFileList(std::vector<std::string> &outFileList);
};

REGISTER_BASIC_ARITHMETIC_OPERATORS(ZIPFile::OpenFlags);

#endif
