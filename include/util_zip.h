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

class BaseZipFile;
class ZIPFile {
  public:
	enum class OpenMode : uint8_t { Read = 0u, Write };
	static std::unique_ptr<ZIPFile> Open(const std::string &filePath, OpenMode openFlags = OpenMode::Read);
	static std::unique_ptr<ZIPFile> Open(const void *zipData, size_t size);
	~ZIPFile();
	bool AddFile(const std::string &fileName, const void *data, uint64_t size, bool bOverwrite = true);
	bool AddFile(const std::string &fileName, const std::string &data, bool bOverwrite = true);
	bool ReadFile(const std::string &fileName, std::vector<uint8_t> &outData, std::string &outErr);
	bool GetFileList(std::vector<std::string> &outFileList);
	bool ExtractFiles(const std::string &dirName, std::string &outErr);
  private:
	ZIPFile(std::unique_ptr<BaseZipFile> baseZipFile);
	std::unique_ptr<BaseZipFile> m_baseZipFile;
};

#endif
