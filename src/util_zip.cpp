/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_zip.h"
#include "util_zip.h"
#include <cstring>
#include <zip.h>
#include <algorithm>
#include <unordered_map>

// TODO: Check if 7zpp works on Linux, and if so, switch to 7zpp entirely
#ifdef _WIN32
#define ENABLE_7Z_FOR_READING
#endif

#ifdef ENABLE_7Z_FOR_READING
// See https://github.com/getnamo/7zip-cpp
#include <7zpp/7zpp.h>
#endif

#pragma optimize("", off)
class BaseZipFile {
  public:
	virtual ~BaseZipFile() = default;
	virtual bool GetFileList(std::vector<std::string> &outFileList) const = 0;
	virtual bool AddFile(const std::string &fileName, const void *data, uint64_t size, bool bOverwrite = true) = 0;
	bool AddFile(const std::string &fileName, const std::string &data, bool bOverwrite = true) { return AddFile(fileName, data.data(), data.size(), bOverwrite); }
	virtual bool ReadFile(const std::string &fileName, std::vector<uint8_t> &outData, std::string &outErr) = 0;
	virtual bool ExtractFiles(const std::string &dirName, std::string &outErr) = 0;
};

class LibZipFile : public BaseZipFile {
  public:
	static std::unique_ptr<BaseZipFile> Open(const std::string &fileName, ZIPFile::OpenMode openMode);
	static std::unique_ptr<BaseZipFile> Open(const void *zipData, size_t size);
	virtual ~LibZipFile() override;
	virtual bool GetFileList(std::vector<std::string> &outFileList) const override;
	virtual bool AddFile(const std::string &fileName, const void *data, uint64_t size, bool bOverwrite = true) override;
	virtual bool ReadFile(const std::string &fileName, std::vector<uint8_t> &outData, std::string &outErr) override;
	virtual bool ExtractFiles(const std::string &dirName, std::string &outErr) override;
  private:
	LibZipFile(zip *z) : m_zip {z} {}
	zip *m_zip;
	std::vector<std::unique_ptr<std::vector<uint8_t>>> m_data;
};

std::unique_ptr<BaseZipFile> LibZipFile::Open(const std::string &fileName, ZIPFile::OpenMode openMode)
{
	int flags = 0;
	flags |= ZIP_CREATE;
	if(openMode == ZIPFile::OpenMode::Read)
		flags |= ZIP_RDONLY;

	int err = -1;
	auto *z = zip_open(fileName.c_str(), flags, &err);
	if(z == nullptr)
		return nullptr;
	std::unique_ptr<LibZipFile> zipFile {new LibZipFile {z}};
	return zipFile;
}
std::unique_ptr<BaseZipFile> LibZipFile::Open(const void *zipData, size_t size)
{
	zip_error_t err;
	auto *zs = zip_source_buffer_create(zipData, size, 0, &err); // Will be released automatically
	if(!zs)
		return nullptr;
	auto *z = zip_open_from_source(zs, 0, &err);
	if(!z)
		return nullptr;
	std::unique_ptr<LibZipFile> zipFile {new LibZipFile {z}};
	return zipFile;
}
bool LibZipFile::GetFileList(std::vector<std::string> &outFileList) const
{
	auto n = zip_get_num_entries(m_zip, 0);
	outFileList.reserve(n);
	for(zip_uint64_t i = 0; i < n; ++i) {
		auto *name = zip_get_name(m_zip, i, 0); // TODO: This may be a UTF8-string!
		outFileList.push_back(name);
	}
	return true;
}
bool LibZipFile::AddFile(const std::string &fileName, const void *data, uint64_t size, bool bOverwrite)
{
	m_data.push_back(std::make_unique<std::vector<uint8_t>>(size));
	auto &vdata = *m_data.back();
	memcpy(vdata.data(), data, size);

	auto *zipSrc = zip_source_buffer(m_zip, vdata.data(), size, 0);
	zip_flags_t flags = ZIP_FL_ENC_GUESS;
	if(bOverwrite == true)
		flags |= ZIP_FL_OVERWRITE;
	auto normalizedFileName = fileName;
	for(auto &c : normalizedFileName) {
		if(c == '\\')
			c = '/';
	}
	zip_file_add(m_zip, normalizedFileName.c_str(), zipSrc, flags);
	return true;
}
bool LibZipFile::ReadFile(const std::string &fileName, std::vector<uint8_t> &outData, std::string &outErr)
{
	struct zip_stat st;
	zip_stat_init(&st);
	if(zip_stat(m_zip, fileName.data(), 0, &st) != 0)
		return false;
	auto *f = zip_fopen_index(m_zip, st.index, 0);
	if(!f) {
		auto *err = zip_get_error(m_zip);
		if(err)
			outErr = std::to_string(err->zip_err);
		return false;
	}
	outData.resize(st.size);
	zip_fread(f, outData.data(), outData.size());
	return zip_fclose(f) == 0;
}
bool LibZipFile::ExtractFiles(const std::string &dirName, std::string &outErr)
{
	// TODO: Implement this
	return false;
}

LibZipFile::~LibZipFile() { zip_close(m_zip); }

/////////////

#ifdef ENABLE_7Z_FOR_READING
class SevenZipFile : public BaseZipFile {
  public:
	static std::unique_ptr<BaseZipFile> Open(const std::string &fileName, ZIPFile::OpenMode openMode);

	virtual ~SevenZipFile() override;
	virtual bool GetFileList(std::vector<std::string> &outFileList) const override;
	virtual bool AddFile(const std::string &fileName, const void *data, uint64_t size, bool bOverwrite = true) override;
	virtual bool ReadFile(const std::string &fileName, std::vector<uint8_t> &outData, std::string &outErr) override;
	virtual bool ExtractFiles(const std::string &dirName, std::string &outErr) override;
  private:
	SevenZipFile() {}

	SevenZip::SevenZipLibrary lib;
	std::unique_ptr<SevenZip::SevenZipExtractor> extractor = nullptr;
	std::unique_ptr<SevenZip::SevenZipCompressor> compressor = nullptr;
	std::unordered_map<size_t, uint32_t> m_hashToIndex;
};

static std::string program_name(bool bPost = false)
{
	std::string programPath = "";
#ifdef __linux__
	pid_t pid = getpid();
	char buf[20] = {0};
	sprintf(buf, "%d", pid);
	std::string _link = "/proc/";
	_link.append(buf);
	_link.append("/exe");
	char proc[512];
	int ch = readlink(_link.c_str(), proc, 512);
	if(ch != -1) {
		proc[ch] = 0;
		programPath = proc;
		std::string::size_type t = programPath.find_last_of("/");
		programPath = (bPost == false) ? programPath.substr(0, t) : programPath.substr(t + 1, programPath.length());
	}
#else
	char path[MAX_PATH + 1];
	GetModuleFileName(NULL, path, MAX_PATH + 1);

	programPath = path;
	auto br = programPath.rfind("\\");
	programPath = (bPost == false) ? programPath.substr(0, br) : programPath.substr(br + 1, programPath.length());
#endif
	return programPath;
}

std::unique_ptr<BaseZipFile> SevenZipFile::Open(const std::string &fileName, ZIPFile::OpenMode openMode)
{
	auto r = std::unique_ptr<SevenZipFile> {new SevenZipFile {}};
	auto dllPath = program_name() + "\\";
#ifdef _WIN32
	std::string dllName = "7zip.dll";
#else
	// TODO: Needs to be confirmed!
	std::string dllName = "7z.so";
#endif
	if(!r->lib.Load(dllPath + dllName) && !r->lib.Load(dllPath + "bin/" + dllName))
		return nullptr;

	if(openMode == ZIPFile::OpenMode::Read) {
		r->extractor = std::make_unique<SevenZip::SevenZipExtractor>(r->lib, fileName);
		if(!r->extractor->DetectCompressionFormat())
			return nullptr;

		std::vector<std::string> files;
		if(!r->GetFileList(files))
			return nullptr;
		for(auto i = decltype(files.size()) {0u}; i < files.size(); ++i) {
			auto &f = files[i];
			for(auto &c : f)
				c = std::tolower(c);
			auto hash = std::hash<std::string> {}(f);
			r->m_hashToIndex[hash] = i;
		}
	}
	else {
		r->compressor = std::make_unique<SevenZip::SevenZipCompressor>(r->lib, fileName);
		r->compressor->SetCompressionFormat(SevenZip::CompressionFormat::Zip);
	}
	return r;
}

SevenZipFile::~SevenZipFile() {}

bool SevenZipFile::GetFileList(std::vector<std::string> &outFileList) const
{
	if(!extractor)
		return false;
	auto itemNames = extractor->GetItemsNames();
	outFileList.reserve(itemNames.size());
	for(auto &f : itemNames)
		outFileList.push_back(std::string(f.begin(), f.end()));
	return true;
}

bool SevenZipFile::AddFile(const std::string &fileName, const void *data, uint64_t size, bool bOverwrite)
{
	return false; // Not yet implemented
}

class SevenZipProgressCallback : public SevenZip::ProgressCallback {
  public:
	/*
	Called at beginning
	*/
	virtual void OnStartWithTotal(const SevenZip::TString &archivePath, unsigned __int64 totalBytes) override {}

	/*
	Called Whenever progress has updated with a bytes complete
	*/
	virtual void OnProgress(const SevenZip::TString &archivePath, unsigned __int64 bytesCompleted) override {}

	/*
	Called When progress has reached 100%
	*/
	virtual void OnDone(const SevenZip::TString &archivePath) override { m_complete = true; }

	/*
	Called When single file progress has reached 100%, returns the filepath that completed
	*/
	virtual void OnFileDone(const SevenZip::TString &archivePath, const SevenZip::TString &filePath, unsigned __int64 bytesCompleted) override {}

	/*
	Called to determine if it's time to abort the zip operation. Return true to abort the current operation.
	*/
	virtual bool OnCheckBreak() override { return false; }
	void WaitUntilComplete()
	{
		while(!m_complete)
			;
	} // TODO: Wait on mutex
  private:
	std::atomic<bool> m_complete = false;
};
bool SevenZipFile::ExtractFiles(const std::string &dirName, std::string &outErr)
{
	if(!extractor)
		return false;
	std::vector<uint32_t> fileIndices;
	fileIndices.reserve(m_hashToIndex.size());
	for(auto &pair : m_hashToIndex)
		fileIndices.push_back(pair.second);
	SevenZipProgressCallback progressCallback {};
	auto res = extractor->ExtractFilesFromArchive(fileIndices.data(), fileIndices.size(), dirName, &progressCallback);
	if(!res)
		return false;
	progressCallback.WaitUntilComplete();
	return true;
}
bool SevenZipFile::ReadFile(const std::string &fileName, std::vector<uint8_t> &outData, std::string &outErr)
{
	if(!extractor)
		return false;
	auto lFileName = fileName;
	for(auto &c : lFileName)
		c = std::tolower(c);
	std::replace(lFileName.begin(), lFileName.end(), '/', '\\');
	auto hash = std::hash<std::string> {}(lFileName);
	auto it = m_hashToIndex.find(hash);
	if(it == m_hashToIndex.end())
		return false;
	auto index = it->second;
	SevenZipProgressCallback progressCallback {};
	auto res = extractor->ExtractFileToMemory(index, outData, &progressCallback);
	if(!res)
		return false;
	if(outData.empty())
		return false; // Probably a directory?
	progressCallback.WaitUntilComplete();
	return true;
}
#endif

/////////////

std::unique_ptr<ZIPFile> ZIPFile::Open(const void *zipData, size_t size)
{
	auto baseZip = LibZipFile::Open(zipData, size);
	if(!baseZip)
		return nullptr;
	return std::unique_ptr<ZIPFile>(new ZIPFile(std::move(baseZip)));
}
std::unique_ptr<ZIPFile> ZIPFile::Open(const std::string &filePath, OpenMode openMode)
{
#ifdef ENABLE_7Z_FOR_READING
	if(openMode == OpenMode::Read) {
		auto baseZip = SevenZipFile::Open(filePath, openMode);
		if(!baseZip)
			return nullptr;
		return std::unique_ptr<ZIPFile>(new ZIPFile(std::move(baseZip)));
	}
#endif
	auto baseZip = LibZipFile::Open(filePath, openMode);
	if(!baseZip)
		return nullptr;
	return std::unique_ptr<ZIPFile>(new ZIPFile(std::move(baseZip)));
}

ZIPFile::ZIPFile(std::unique_ptr<BaseZipFile> baseZipFile) : m_baseZipFile(std::move(baseZipFile)) {}

ZIPFile::~ZIPFile() { m_baseZipFile = nullptr; }

bool ZIPFile::ExtractFiles(const std::string &dirName, std::string &outErr) { return m_baseZipFile->ExtractFiles(dirName, outErr); }

bool ZIPFile::GetFileList(std::vector<std::string> &outFileList) { return m_baseZipFile->GetFileList(outFileList); }

bool ZIPFile::ReadFile(const std::string &fileName, std::vector<uint8_t> &outData, std::string &outErr) { return m_baseZipFile->ReadFile(fileName, outData, outErr); }

bool ZIPFile::AddFile(const std::string &fileName, const void *data, uint64_t size, bool bOverwrite) { return m_baseZipFile->AddFile(fileName, data, size, bOverwrite); }

bool ZIPFile::AddFile(const std::string &fileName, const std::string &data, bool bOverwrite) { return AddFile(fileName, data.data(), data.size(), bOverwrite); }
#pragma optimize("", on)
