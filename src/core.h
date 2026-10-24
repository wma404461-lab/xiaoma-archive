#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <stdexcept>
namespace xm {
namespace fs = std::filesystem;
struct Failure : std::runtime_error { std::wstring message; bool password; Failure(std::wstring m, bool p=false):runtime_error("archive operation failed"),message(std::move(m)),password(p){} };
struct Cancelled {};
struct Entry { std::wstring name; unsigned long long size=0; bool directory=false; };
struct Archive { std::vector<Entry> entries; unsigned long long total=0; bool encrypted=false; std::wstring type; size_t count=0; };
struct Progress { int percent=-1; std::wstring detail; };
using Reporter=std::function<void(const Progress&)>;
struct Options { std::wstring format=L"zip"; int level=1; std::wstring password; unsigned volumeMB=0; bool smart=true; };
std::wstring Utf8(const std::string& s);
std::string ToUtf8(const std::wstring& s);
std::wstring Size(unsigned long long n);
std::wstring Quote(const std::wstring& s);
std::wstring ArchiveStem(const fs::path& p);
bool IsArchive(const fs::path& p);
fs::path FirstVolume(const fs::path& p);
void ValidateEntry(const std::wstring& p);
void CheckCancelled(const std::atomic_bool& stop);
fs::path AppDirectory();
unsigned long long EnginePeakMemory();
class Engine {
 fs::path executable;
 int Run(const std::vector<std::wstring>& args, const fs::path& cwd, std::atomic_bool& stop, const Reporter& report, const std::function<void(const std::wstring&)>& line);
 void CheckedRun(const std::vector<std::wstring>& args,const fs::path& cwd,std::atomic_bool& stop,const Reporter& report);
 Archive Catalog(const fs::path& archive,const std::wstring& password,std::atomic_bool& stop,const Reporter& report,bool keepEntries,const std::vector<std::wstring>& selected={});
public:
 explicit Engine(fs::path p):executable(std::move(p)){}
 Archive List(const fs::path& archive,const std::wstring& password,std::atomic_bool& stop,const Reporter& report={});
 Archive Inspect(const fs::path& archive,const std::wstring& password,std::atomic_bool& stop,const Reporter& report={});
 fs::path Extract(const fs::path& archive,const fs::path& parent,const std::wstring& password,std::atomic_bool& stop,const Reporter& report={},const std::vector<std::wstring>& selected={},bool flattenMatchingRoot=true);
 std::vector<fs::path> Compress(const std::vector<fs::path>& sources,const fs::path& target,const Options& options,std::atomic_bool& stop,const Reporter& report={});
 void Test(const fs::path& archive,const std::wstring& password,std::atomic_bool& stop,const Reporter& report={});
 std::wstring ReadText(const fs::path& archive,const std::wstring& member,const std::wstring& password,std::atomic_bool& stop);
 fs::path Modify(const fs::path& archive,const std::vector<fs::path>& add,const std::vector<std::wstring>& remove,const std::wstring& password,std::atomic_bool& stop,const Reporter& report={});
 std::vector<fs::path> Convert(const fs::path& archive,const fs::path& target,const Options& options,const std::wstring& password,std::atomic_bool& stop,const Reporter& report={});
};
int SelfTest(const fs::path& engine,const fs::path& root);
}
