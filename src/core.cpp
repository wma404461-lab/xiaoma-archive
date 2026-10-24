#include "core.h"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <chrono>
#include <memory>
#include <regex>
#include <objbase.h>
#include <psapi.h>
#include <climits>
#include <string_view>
#include <thread>
namespace xm {
static std::atomic_ullong peakMemory{0};
unsigned long long EnginePeakMemory(){return peakMemory.load();}
struct Handle { HANDLE h=nullptr; Handle()=default; explicit Handle(HANDLE v):h(v){} ~Handle(){if(h && h!=INVALID_HANDLE_VALUE)CloseHandle(h);} Handle(const Handle&)=delete; Handle& operator=(const Handle&)=delete; };
std::wstring Utf8(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring r(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n);return r;}
std::string ToUtf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string r(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);return r;}
std::wstring Lower(std::wstring s){for(auto& c:s)c=(wchar_t)towlower(c);return s;}
std::wstring Size(unsigned long long n){wchar_t b[64];if(n<1024)swprintf_s(b,L"%llu B",n);else if(n<1048576)swprintf_s(b,L"%.1f KB",n/1024.0);else if(n<1073741824)swprintf_s(b,L"%.1f MB",n/1048576.0);else swprintf_s(b,L"%.2f GB",n/1073741824.0);return b;}
std::wstring Quote(const std::wstring& s){std::wstring r=L"\"";size_t slash=0;for(wchar_t c:s){if(c==L'\\'){slash++;continue;}if(c==L'\"'){r.append(slash*2+1,L'\\');r+=c;}else{r.append(slash,L'\\');r+=c;}slash=0;}r.append(slash*2,L'\\');return r+L"\"";}
fs::path AppDirectory(){std::wstring s(32768,0);DWORD n=GetModuleFileNameW(nullptr,s.data(),(DWORD)s.size());s.resize(n);return fs::path(s).parent_path();}
void CheckCancelled(const std::atomic_bool& stop){if(stop.load())throw Cancelled{};}
bool IsArchive(const fs::path& p){auto e=Lower(p.extension().wstring());static const std::wstring types=L"|.zip|.7z|.rar|.tar|.gz|.gzip|.tgz|.bz2|.tbz|.tbz2|.xz|.txz|.zst|.zstd|.cab|.iso|.wim|.esd|.arj|.lzh|.lzma|.lz|.z|.001|.vhd|.vhdx|.udf|.dmg|.apk|.jar|.war|.xpi|.cpio|.deb|.rpm|";return (!e.empty()&&types.find(L"|"+e+L"|")!=std::wstring::npos)||(e.size()==4&&((iswdigit(e[1])&&iswdigit(e[2])&&iswdigit(e[3]))||((e[1]==L'r'||e[1]==L'z')&&iswdigit(e[2])&&iswdigit(e[3]))));}
fs::path FirstVolume(const fs::path& requested){
 auto p=fs::absolute(requested);auto ext=Lower(p.extension().wstring());auto name=p.filename().wstring();auto lower=Lower(name);fs::path first;
 if(ext.size()==4&&iswdigit(ext[1])&&iswdigit(ext[2])&&iswdigit(ext[3])){first=p;first.replace_extension(L".001");}
 else if(ext.size()==4&&(ext[1]==L'r'||ext[1]==L'z')&&iswdigit(ext[2])&&iswdigit(ext[3])){first=p;first.replace_extension(ext[1]==L'r'?L".rar":L".zip");}
 else if(ext==L".rar"){auto pos=lower.rfind(L".part");if(pos!=std::wstring::npos){size_t end=pos+5;while(end<lower.size()&&iswdigit(lower[end]))end++;if(end>pos+5&&end+4==lower.size()){auto digits=std::wstring(end-pos-5,L'0');digits.back()=L'1';first=p.parent_path()/(name.substr(0,pos+5)+digits+name.substr(end));}}}
 if(!first.empty()&&Lower(first.wstring())!=Lower(p.wstring())){if(!fs::exists(first))throw Failure(L"Missing first volume: "+first.filename().wstring()+L". Please place all volumes in the same directory.");return first;}
 return p;
}
std::wstring ArchiveStem(const fs::path& p){auto s=p.filename().wstring();auto l=Lower(s);for(auto suffix:{L".tar.gz",L".tar.bz2",L".tar.xz",L".tar.zst",L".7z.001",L".zip.001"}){std::wstring t=suffix;if(l.size()>t.size()&&l.substr(l.size()-t.size())==t)return s.substr(0,s.size()-t.size());}auto stem=p.stem().wstring();auto pos=Lower(stem).rfind(L".part1");if(pos!=std::wstring::npos&&pos+6==stem.size())stem.resize(pos);return stem.empty()?L"Extract files":stem;}
void ValidateEntry(const std::wstring& raw){
 if(raw.empty()||raw.size()>30000)throw Failure(L"Compression contains invalid file names.");
 if(raw.front()==L'\\'||raw.front()==L'/'||raw.find(L':')!=std::wstring::npos)throw Failure(L"Blocked absolute paths or special data streams in the archive.");
 std::wstring_view all(raw);size_t offset=0;
 while(offset<all.size()){
  auto end=all.find_first_of(L"/\\",offset);if(end==std::wstring_view::npos)end=all.size();auto part=all.substr(offset,end-offset);offset=end+1;
  if(part==L".")continue;
  if(part.empty()||part==L".."||part.back()==L'.'||part.back()==L' ')throw Failure(L"Blocked unsafe paths in the archive.");
  for(auto c:part)if(c<32||c==L'<'||c==L'>'||c==L'\"'||c==L'|'||c==L'?'||c==L'*')throw Failure(L"Compression contains file names not supported by Windows.");
  auto dot=part.find(L'.');auto view=part.substr(0,dot);if(view.size()<=7){std::wstring base(view);for(auto& c:base)c=(wchar_t)towlower(c);
   if(base==L"con"||base==L"prn"||base==L"aux"||base==L"nul"||base==L"conin$"||base==L"conout$"||(base.size()==4&&(base.substr(0,3)==L"com"||base.substr(0,3)==L"lpt")&&((base[3]>=L'0'&&base[3]<=L'9')||base[3]==L'¹'||base[3]==L'²'||base[3]==L'³')))throw Failure(L"Blocked system-reserved names in the archive.");
  }
 }
}
static bool Below(const fs::path& child,const fs::path& parent){auto c=Lower(fs::absolute(child).lexically_normal().wstring());auto p=Lower(fs::absolute(parent).lexically_normal().wstring());if(!p.empty()&&p.back()!=L'\\')p+=L'\\';return c.rfind(p,0)==0;}
static bool MediaPacked(const fs::path& p){static const std::wstring extensions=L"|.jpg|.jpeg|.png|.webp|.avif|.heic|.mp4|.mkv|.mov|.mp3|.aac|.flac|.ogg|.zip|.7z|.rar|.gz|.xz|.bz2|.zst|";auto ext=Lower(p.extension().wstring());return !ext.empty()&&extensions.find(L"|"+ext+L"|")!=std::wstring::npos;}
static void NoLinks(const fs::path& p){auto q=fs::absolute(p);while(!q.empty()){DWORD a=GetFileAttributesW(q.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))throw Failure(L"To avoid writing to unexpected locations, symbolic links or directory junctions are not supported.");auto parent=q.parent_path();if(parent==q)break;q=parent;}}
static void ScanSafe(const fs::path& root,std::atomic_bool& stop,bool* packed=nullptr){NoLinks(root);if(!fs::is_directory(root)){if(packed&&!MediaPacked(root))*packed=false;return;}size_t n=0;for(const auto& e:fs::recursive_directory_iterator(root)){CheckCancelled(stop);DWORD a=GetFileAttributesW(e.path().c_str());if(a==INVALID_FILE_ATTRIBUTES)throw Failure(L"Unable to read source file properties.");if(a&FILE_ATTRIBUTE_REPARSE_POINT)throw Failure(L"To avoid writing to unexpected locations, symbolic links or directory junctions are not supported.");if(packed&&!(a&FILE_ATTRIBUTE_DIRECTORY)&&!MediaPacked(e.path()))*packed=false;if(++n>1000000)throw Failure(L"Up to 1 million source files can be processed at once. Please compress in batches.");}}
struct Workspace {
 fs::path path;
 explicit Workspace(const fs::path& parent){NoLinks(parent);fs::create_directories(parent);GUID g{};if(FAILED(CoCreateGuid(&g)))throw Failure(L"Unable to create temporary directory.");wchar_t id[64];StringFromGUID2(g,id,64);path=fs::absolute(parent)/(std::wstring(L".xiaoma-work-")+id);if(!fs::create_directory(path))throw Failure(L"Unable to create temporary directory.");}
 ~Workspace(){std::error_code ec;if(!path.empty()&&path.filename().wstring().rfind(L".xiaoma-work-",0)==0)fs::remove_all(path,ec);}
};
int Engine::Run(const std::vector<std::wstring>& args,const fs::path& cwd,std::atomic_bool& stop,const Reporter& report,const std::function<void(const std::wstring&)>& onLine){
 CheckCancelled(stop);if(!fs::exists(executable))throw Failure(L"Compression kernel missing. Please place the engine folder and Xiaoma Compress.exe together.");
 Handle read,write;SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};if(!CreatePipe(&read.h,&write.h,&sa,0))throw Failure(L"Unable to create processing pipeline.");SetHandleInformation(read.h,HANDLE_FLAG_INHERIT,0);
 Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr));
 SIZE_T attributeSize=0;InitializeProcThreadAttributeList(nullptr,1,0,&attributeSize);std::vector<unsigned char> storage(attributeSize);auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
 if(!InitializeProcThreadAttributeList(attributes,1,0,&attributeSize))throw Failure(L"Unable to initialize processing process.");
 struct AttrGuard{LPPROC_THREAD_ATTRIBUTE_LIST p;~AttrGuard(){DeleteProcThreadAttributeList(p);}} guard{attributes};
 HANDLE inherited[2]={write.h,input.h};if(!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr))throw Failure(L"Unable to isolate processing process.");
 STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;si.StartupInfo.hStdInput=input.h;si.StartupInfo.hStdOutput=write.h;si.StartupInfo.hStdError=write.h;si.lpAttributeList=attributes;
 std::wstring command=Quote(executable.wstring());for(const auto& a:args)command+=L" "+Quote(a);
 Handle job(CreateJobObjectW(nullptr,nullptr));if(!job.h)throw Failure(L"Unable to create background task.");JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits));
 PROCESS_INFORMATION pi{};auto working=cwd.empty()?executable.parent_path():cwd;
 if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|BELOW_NORMAL_PRIORITY_CLASS|EXTENDED_STARTUPINFO_PRESENT,nullptr,working.c_str(),&si.StartupInfo,&pi))throw Failure(L"Cannot start compressed kernel. Error "+std::to_wstring(GetLastError()));
 Handle process(pi.hProcess),thread(pi.hThread);
 if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,1);WaitForSingleObject(process.h,INFINITE);throw Failure(L"Failed to isolate background task.");}
 ResumeThread(thread.h);CloseHandle(write.h);write.h=nullptr;
 std::string line;line.reserve(512);ULONGLONG last=0;int lastPercent=-1;
 auto consume=[&](char c){if(c=='\r'||c=='\n'||c=='\b'){if(line.empty())return;auto w=Utf8(line);if(onLine)onLine(w);if(report){size_t pct=w.find(L'%');if(pct!=std::wstring::npos){size_t first=pct;while(first>0&&iswdigit(w[first-1]))first--;if(first<pct){int n=_wtoi(w.substr(first,pct-first).c_str());auto tick=GetTickCount64();if(n>=0&&n<=100&&(n!=lastPercent)&&(tick-last>=100||n==100)){report({n,L"Processing file..."});last=tick;lastPercent=n;}}}}line.clear();}else{if(line.size()>65536)throw Failure(L"Filename or diagnostic line in archive is too long.");line+=c;}};
 char buf[32768];std::atomic_bool watcherDone{false};
 std::thread watcher([&](){while(!watcherDone.load()){if(stop.load()){TerminateJobObject(job.h,1);return;}if(WaitForSingleObject(process.h,25)==WAIT_OBJECT_0)return;}});
 struct WatcherGuard{std::atomic_bool& done;std::thread& thread;~WatcherGuard(){done=true;if(thread.joinable())thread.join();}} watcherGuard{watcherDone,watcher};
 try{for(;;){CheckCancelled(stop);DWORD n=0;if(!ReadFile(read.h,buf,sizeof(buf),&n,nullptr)){if(GetLastError()==ERROR_BROKEN_PIPE)break;throw Failure(L"Failed to read background task result.");}if(!n)break;for(DWORD i=0;i<n;i++)consume(buf[i]);}
 if(!line.empty()&&onLine)onLine(Utf8(line));WaitForSingleObject(process.h,INFINITE);CheckCancelled(stop);PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);if(GetProcessMemoryInfo(process.h,&memory,sizeof(memory))){auto previous=peakMemory.load();while(previous<memory.PeakWorkingSetSize&&!peakMemory.compare_exchange_weak(previous,memory.PeakWorkingSetSize)){}}DWORD code=2;GetExitCodeProcess(process.h,&code);return (int)code;
 }catch(...){TerminateJobObject(job.h,1);WaitForSingleObject(process.h,INFINITE);throw;}
}
void Engine::CheckedRun(const std::vector<std::wstring>& args,const fs::path& cwd,std::atomic_bool& stop,const Reporter& report){
 bool password=false;std::wstring diagnosis;
 int code=Run(args,cwd,stop,report,[&](const std::wstring& line){auto l=Lower(line);if(l.find(L"password")!=std::wstring::npos||l.find(L"encrypted")!=std::wstring::npos)password=true;if(l.find(L"error")!=std::wstring::npos||l.find(L"warning")!=std::wstring::npos){if(diagnosis.size()<1500)diagnosis+=line+L"\n";}});
 if(code!=0){if(password)throw Failure(L"Incorrect password or encrypted file is corrupted. Please check the password and retry.",true);if(code==1)throw Failure(L"Some files cannot be read. Incomplete compressed result not saved. Please close programs using the files and retry.");if(code==8)throw Failure(L"Insufficient memory. Please use fast mode or process files in batches.");if(code==255)throw Cancelled{};throw Failure(diagnosis.empty()?L"Processing failed: file is damaged, format not supported, insufficient space, or no read/write permission.":L"Processing failed: \n"+diagnosis);}
}
Archive Engine::Catalog(const fs::path& requested,const std::wstring& password,std::atomic_bool& stop,const Reporter& report,bool keepEntries,const std::vector<std::wstring>& selected){
 auto archive=FirstVolume(requested);
 std::vector<std::wstring> selection;for(auto name:selected){ValidateEntry(name);std::replace(name.begin(),name.end(),L'/',L'\\');while(name.rfind(L".\\",0)==0)name.erase(0,2);while(!name.empty()&&name.back()==L'\\')name.pop_back();selection.push_back(std::move(name));}
 auto matches=[&](std::wstring name){if(selection.empty())return true;std::replace(name.begin(),name.end(),L'/',L'\\');while(name.rfind(L".\\",0)==0)name.erase(0,2);for(const auto& root:selection){if(name==root||(name.size()>root.size()&&name.compare(0,root.size(),root)==0&&name[root.size()]==L'\\'))return true;}return false;};
 Archive out;std::wstring path,attributes;bool dir=false;unsigned long long size=0;bool entries=false;size_t bytes=0;bool badPassword=false;std::wstring error;
 auto commit=[&](){if(path.empty())return;if((path==L"."||path==L"./")&&dir){path.clear();attributes.clear();dir=false;size=0;return;}ValidateEntry(path);if(attributes.find(L'L')!=std::wstring::npos||attributes.find(L'l')!=std::wstring::npos)throw Failure(L"This archive contains links. Secure extraction is not supported.");if(out.count>=200000||bytes>64*1024*1024)throw Failure(L"Archive directory is too large. Please process in batches (maximum 200,000 entries).");if(ULLONG_MAX-out.total<size)throw Failure(L"Archive size is abnormal.");bytes+=path.size()*2;out.count++;if(matches(path)){out.total+=size;if(keepEntries)out.entries.push_back({path,size,dir});}path.clear();attributes.clear();dir=false;size=0;};
 std::vector<std::wstring> args{L"l",L"-slt",L"-sccUTF-8",L"-bd",L"-p"+(password.empty()?std::wstring(L"-"):password),L"--",fs::absolute(archive).wstring()};
 int code=Run(args,{},stop,report,[&](const std::wstring& line){if(line.find(L"Wrong password")!=std::wstring::npos||line.find(L"password?")!=std::wstring::npos||line.find(L"encrypted archive")!=std::wstring::npos)badPassword=true;if((line.rfind(L"ERROR",0)==0||line.rfind(L"System ERROR",0)==0)&&error.size()<1000)error+=line+L"\n";if(line==L"----------"){entries=true;return;}if(!entries){if(line.rfind(L"Type = ",0)==0)out.type=line.substr(7);return;}
 if(line.rfind(L"Path = ",0)==0){commit();path=line.substr(7);}else if(line.rfind(L"Size = ",0)==0){auto value=line.substr(7);if(value.empty())size=0;else try{size=std::stoull(value);}catch(...){throw Failure(L"Invalid archive size field.");}}else if(line.rfind(L"Folder = ",0)==0)dir=line.substr(9)==L"+";else if(line.rfind(L"Attributes = ",0)==0){attributes=line.substr(13);if(attributes.find(L'D')!=std::wstring::npos)dir=true;}else if(line==L"Encrypted = +")out.encrypted=true;else if((line.rfind(L"Symbolic Link = ",0)==0&&line.size()>16)||(line.rfind(L"Hard Link = ",0)==0&&line.size()>12)||(line.rfind(L"Copy Link = ",0)==0&&line.size()>12))throw Failure(L"This archive contains links. Secure extraction is not supported.");});
 if(code!=0)throw Failure(badPassword?L"Archive requires a password, or the password is incorrect.":(error.empty()?L"Cannot open archive: unsupported format or file is damaged.":L"Cannot open archive: \n"+error),badPassword);if(path.empty()&&out.count==0&&(out.type==L"bzip2"||out.type==L"xz"||out.type==L"zstd"))path=ArchiveStem(archive);commit();return out;
}
Archive Engine::List(const fs::path& p,const std::wstring& password,std::atomic_bool& stop,const Reporter& report){return Catalog(p,password,stop,report,true);}
Archive Engine::Inspect(const fs::path& p,const std::wstring& password,std::atomic_bool& stop,const Reporter& report){return Catalog(p,password,stop,report,false);}
void Engine::Test(const fs::path& archive,const std::wstring& password,std::atomic_bool& stop,const Reporter& report){CheckedRun({L"t",L"-bsp1",L"-bso0",L"-bse1",L"-sccUTF-8",L"-p"+(password.empty()?std::wstring(L"-"):password),L"--",fs::absolute(archive).wstring()},{},stop,report);}
fs::path Engine::Extract(const fs::path& requested,const fs::path& parent,const std::wstring& password,std::atomic_bool& stop,const Reporter& report,const std::vector<std::wstring>& selected,bool flattenMatchingRoot){
 auto archive=FirstVolume(requested);
 if(report)report({-1,L"Checking archive directory..."});auto catalog=Catalog(archive,password,stop,{},!selected.empty(),selected);if(!selected.empty()&&catalog.entries.empty())throw Failure(L"Selected entry no longer exists. Please reload the directory.");if(catalog.encrypted&&password.empty())throw Failure(L"This archive is encrypted. Please enter the password.",true);
 NoLinks(parent);fs::create_directories(parent);std::error_code ec;auto capacity=fs::space(parent,ec);if(!ec&&catalog.total>capacity.available)throw Failure(L"Target disk space is insufficient; at least "+Size(catalog.total)+L".");
 Workspace work(parent);auto payload=work.path/L"content";fs::create_directory(payload);
 std::vector<std::wstring> args{L"x",L"-y",L"-aoa",L"-bsp1",L"-bso0",L"-bse1",L"-sccUTF-8",L"-scsUTF-8",L"-o"+payload.wstring(),L"-p"+(password.empty()?std::wstring(L"-"):password)};
 if(!selected.empty()){auto include=work.path/L"include.txt";std::ofstream file(include,std::ios::binary);for(const auto& entry:catalog.entries)file<<ToUtf8(entry.name)<<"\n";file.close();if(!file)throw Failure(L"Failed to save the selected item list.");args.push_back(L"-spd");args.push_back(L"-i@"+include.wstring());}
 args.push_back(L"--");args.push_back(fs::absolute(archive).wstring());CheckedRun(args,{},stop,report);
 ScanSafe(payload,stop);
 // GZIP/BZIP2/XZ/ZSTD are often envelopes around a TAR. Unwrap this one extra layer.
 auto name=Lower(archive.filename().wstring());bool compound=catalog.type==L"gzip"||catalog.type==L"bzip2"||catalog.type==L"xz"||catalog.type==L"zstd";
 if(compound){std::vector<fs::path> files;for(const auto& e:fs::directory_iterator(payload))files.push_back(e.path());if(files.size()==1&&fs::is_regular_file(files[0])){
 Archive inner;bool tar=false;bool expectedTar=Lower(files[0].extension().wstring())==L".tar"||name.find(L".tar.")!=std::wstring::npos||name.rfind(L"tar.",0)==0||Lower(archive.extension().wstring())==L".tgz"||Lower(archive.extension().wstring())==L".tbz2"||Lower(archive.extension().wstring())==L".txz";try{inner=Inspect(files[0],password,stop);tar=Lower(inner.type)==L"tar";}catch(const Failure& e){if(e.password||expectedTar)throw;}if(tar){capacity=fs::space(parent,ec);if(!ec&&inner.total>capacity.available)throw Failure(L"Insufficient disk space to expand TAR.");auto expanded=work.path/L"Expand";fs::create_directory(expanded);
 CheckedRun({L"x",L"-y",L"-aoa",L"-bsp1",L"-sccUTF-8",L"-o"+expanded.wstring(),L"-p"+(password.empty()?std::wstring(L"-"):password),L"--",files[0].wstring()},{},stop,report);ScanSafe(expanded,stop);payload=expanded;}}}
 CheckCancelled(stop);auto stem=ArchiveStem(archive);ValidateEntry(stem);
 if(flattenMatchingRoot){std::vector<fs::path> roots;for(const auto& item:fs::directory_iterator(payload)){roots.push_back(item.path());if(roots.size()>1)break;}if(roots.size()==1&&fs::is_directory(roots[0])){auto expected=Lower(stem);auto base=expected;auto pos=base.rfind(L" (");if(pos!=std::wstring::npos&&base.back()==L')'&&pos+3<base.size()){bool number=true;for(size_t i=pos+2;i+1<base.size();i++)if(!iswdigit(base[i]))number=false;if(number)base.resize(pos);}auto rootName=Lower(roots[0].filename().wstring());if(rootName==expected||rootName==base)payload=roots[0];}}
 for(unsigned n=0;n<10000;n++){auto destination=fs::absolute(parent)/(stem+(n?L" ("+std::to_wstring(n)+L")":L""));if(fs::exists(destination))continue;ec.clear();fs::rename(payload,destination,ec);if(!ec)return destination;if(fs::exists(destination))continue;throw Failure(L"Failed to save the extracted result. Please check the target directory permissions.");}throw Failure(L"Too many directories with the same name. Please change the target directory.");
}
std::vector<fs::path> Engine::Compress(const std::vector<fs::path>& sources,const fs::path& target,const Options& options,std::atomic_bool& stop,const Reporter& report){
 if(options.format==L"zip")for(auto c:options.password)if(c<32||c>126)throw Failure(L"ZIP password must use English letters, numbers, and symbols. Chinese passwords should use 7z format.");
 if(sources.empty())throw Failure(L"Please add files or folders to be compressed first.");if(options.format!=L"zip"&&options.format!=L"7z"&&options.format!=L"tar")throw Failure(L"Compression format is not supported.");if(options.format==L"tar"&&!options.password.empty())throw Failure(L"TAR does not support passwords. Please use ZIP or 7z instead.");
 auto output=fs::absolute(target);ValidateEntry(output.filename().wstring());auto parent=output.parent_path();NoLinks(parent);if(fs::exists(output)||fs::exists(output.wstring()+L".001"))throw Failure(L"The target file already exists. Please choose a different name.");
 fs::path common=fs::absolute(sources[0]).parent_path();bool packed=true;
 for(const auto& source:sources){CheckCancelled(stop);if(!fs::exists(source))throw Failure(L"Source file does not exist: "+source.filename().wstring());if(fs::is_directory(source)&&Below(output,source))throw Failure(L"Please save the archive outside the source folder to avoid packing the archive into itself.");if(report)report({-1,L"Checking source file…"});ScanSafe(source,stop,&packed);while(common!=common.root_path()&&!Below(fs::absolute(source),common))common=common.parent_path();if(!Below(fs::absolute(source),common))throw Failure(L"Files on different disks should be compressed separately.");}
 Workspace work(parent);auto list=work.path/L"input.txt";{std::ofstream file(list,std::ios::binary);for(const auto& source:sources){auto relative=fs::absolute(source).lexically_relative(common).wstring();if(relative.find(L'\n')!=std::wstring::npos||relative.find(L'\r')!=std::wstring::npos)throw Failure(L"Source filename contains a newline character.");file<<ToUtf8(relative)<<"\n";}if(!file)throw Failure(L"Failed to write the task file.");}
 int effectiveLevel=options.format==L"zip"&&options.level==1&&options.smart&&packed?0:options.level;auto staged=work.path/output.filename();std::vector<std::wstring> args{L"a",L"-t"+options.format,L"-mx="+std::to_wstring(effectiveLevel),L"-bsp1",L"-bso0",L"-bse1",L"-sccUTF-8",L"-scsUTF-8"};if(options.format!=L"tar")args.push_back(L"-mmt="+std::wstring(options.format==L"zip"&&sources.size()<=2&&options.level<=1?L"1":L"2"));
 if(options.format==L"7z"&&options.level>0){args.push_back(L"-m0=lzma2");args.push_back(L"-md="+std::wstring(options.level<=1?L"512k":options.level<=5?L"4m":L"8m"));}
 if(report&&effectiveLevel==0)report({-1,L"Skipped duplicate compression and proceeding with packing…"});if(options.format==L"zip")args.push_back(L"-mcu=on");if(options.format==L"zip"&&!options.password.empty())args.push_back(L"-mem=AES256");if(!options.password.empty()){args.push_back(L"-p"+options.password);if(options.format==L"7z")args.push_back(L"-mhe=on");}
 if(options.volumeMB)args.push_back(L"-v"+std::to_wstring(options.volumeMB)+L"m");args.push_back(staged.wstring());args.push_back(L"@"+list.wstring());
 CheckedRun(args,common,stop,report);CheckCancelled(stop);
 std::vector<fs::path> artifacts;for(const auto& item:fs::directory_iterator(work.path)){auto name=item.path().filename().wstring();if(name==output.filename().wstring()||name.rfind(output.filename().wstring()+L".",0)==0){if(!item.is_regular_file())throw Failure(L"Compression result is abnormal.");if(fs::exists(parent/item.path().filename()))throw Failure(L"A volume with the same name already exists. Please change the archive name.");artifacts.push_back(item.path());}}
 if(artifacts.empty())throw Failure(L"No archive was generated.");std::sort(artifacts.begin(),artifacts.end());std::vector<fs::path> saved;
 try{for(const auto& item:artifacts){CheckCancelled(stop);auto dest=parent/item.filename();fs::rename(item,dest);saved.push_back(dest);}}catch(...){for(const auto& item:saved){std::error_code ec;fs::rename(item,work.path/item.filename(),ec);}throw;}
 return saved;
}
static fs::path TempRoot(){std::wstring p(32768,0);DWORD n=GetTempPathW((DWORD)p.size(),p.data());if(!n||n>=p.size())throw Failure(L"Unable to read the temporary directory.");p.resize(n);return fs::path(p)/L"XiaomaArchive";}
static std::wstring NormalizeName(std::wstring name){std::replace(name.begin(),name.end(),L'/',L'\\');while(name.rfind(L".\\",0)==0)name.erase(0,2);return name;}
std::wstring Engine::ReadText(const fs::path& archive,const std::wstring& member,const std::wstring& password,std::atomic_bool& stop){
 ValidateEntry(member);auto catalog=Catalog(archive,password,stop,{},true,{member});
 if(catalog.entries.size()!=1||catalog.entries[0].directory)throw Failure(L"Please select a text file for preview.");
 if(catalog.total>2*1024*1024)throw Failure(L"Text exceeds 2 MB. Please decompress and view it afterwards.");
 Workspace work(TempRoot());auto result=Extract(archive,work.path,password,stop,{}, {member},false);auto file=result/NormalizeName(member);NoLinks(file);
 if(!Below(file,result)||!fs::is_regular_file(file))throw Failure(L"The selected item is not a regular file that can be previewed.");auto n=fs::file_size(file);if(n>2*1024*1024)throw Failure(L"Text exceeds 2 MB. Please decompress and view it afterwards.");
 std::ifstream in(file,std::ios::binary);std::string bytes((size_t)n,0);in.read(bytes.data(),(std::streamsize)n);if(!in&&n)throw Failure(L"Unable to read the preview file.");CheckCancelled(stop);std::wstring text;
 if(bytes.size()>=2&&((unsigned char)bytes[0]==0xff&&(unsigned char)bytes[1]==0xfe||(unsigned char)bytes[0]==0xfe&&(unsigned char)bytes[1]==0xff)){
  bool le=(unsigned char)bytes[0]==0xff;if((bytes.size()-2)%2)throw Failure(L"Invalid UTF-16 text length.");text.reserve((bytes.size()-2)/2);for(size_t i=2;i+1<bytes.size();i+=2){unsigned a=(unsigned char)bytes[i],b=(unsigned char)bytes[i+1];text.push_back((wchar_t)(le?(a|(b<<8)):((a<<8)|b)));}
 }else{if(bytes.rfind("\xef\xbb\xbf",0)==0)bytes.erase(0,3);UINT codepage=CP_UTF8;int count=MultiByteToWideChar(codepage,MB_ERR_INVALID_CHARS,bytes.data(),(int)bytes.size(),nullptr,0);if(!count&&!bytes.empty()){codepage=CP_ACP;count=MultiByteToWideChar(codepage,0,bytes.data(),(int)bytes.size(),nullptr,0);}text.resize(count);if(count)MultiByteToWideChar(codepage,codepage==CP_UTF8?MB_ERR_INVALID_CHARS:0,bytes.data(),(int)bytes.size(),text.data(),count);}
 for(wchar_t c:text)if(c==0||(c<32&&c!=L'\r'&&c!=L'\n'&&c!=L'\t'))throw Failure(L"The content is not plain text. Please decompress and view it with the appropriate program.");
 std::wstring normalized;normalized.reserve(text.size());for(size_t i=0;i<text.size();i++){if(text[i]==L'\n'&&(i==0||text[i-1]!=L'\r'))normalized+=L'\r';normalized+=text[i];}return normalized;
}
static BY_HANDLE_FILE_INFORMATION Identity(HANDLE handle){BY_HANDLE_FILE_INFORMATION result{};if(!GetFileInformationByHandle(handle,&result))throw Failure(L"Unable to read archive status.");return result;}
static bool SameIdentity(const BY_HANDLE_FILE_INFORMATION& a,const BY_HANDLE_FILE_INFORMATION& b){return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber&&a.nFileIndexHigh==b.nFileIndexHigh&&a.nFileIndexLow==b.nFileIndexLow&&a.nFileSizeHigh==b.nFileSizeHigh&&a.nFileSizeLow==b.nFileSizeLow&&CompareFileTime(&a.ftLastWriteTime,&b.ftLastWriteTime)==0;}
fs::path Engine::Modify(const fs::path& requested,const std::vector<fs::path>& add,const std::vector<std::wstring>& remove,const std::wstring& password,std::atomic_bool& stop,const Reporter& report){
 auto archive=fs::absolute(requested);if(FirstVolume(archive)!=archive||Lower(archive.extension().wstring())==L".001")throw Failure(L"Multi-volume archives are not supported for modification. Please convert to a single ZIP or 7z first.");
 NoLinks(archive);Handle original(CreateFileW(archive.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(original.h==INVALID_HANDLE_VALUE)throw Failure(L"The archive is currently in use, or read permission is denied.");auto originalIdentity=Identity(original.h);
 auto metadata=Inspect(archive,password,stop);if(metadata.type!=L"zip"&&metadata.type!=L"7z")throw Failure(L"Only ZIP and 7z formats support modification. RAR files must be converted first.");if(metadata.encrypted&&password.empty())throw Failure(L"Modifying an encrypted archive requires a password.",true);if(add.empty()&&remove.empty())throw Failure(L"No content to modify has been selected.");
 Workspace work(archive.parent_path());auto staged=work.path/archive.filename();fs::copy_file(archive,staged);
 if(!remove.empty()){auto chosen=Catalog(archive,password,stop,{},true,remove);if(chosen.entries.empty())throw Failure(L"The selected entry does not exist. Please refresh the directory.");auto include=work.path/L"remove.txt";std::ofstream file(include,std::ios::binary);for(const auto& e:chosen.entries)file<<ToUtf8(e.name)<<"\n";file.close();if(!file)throw Failure(L"Unable to write the modification list.");std::vector<std::wstring> args{L"d",L"-mx=1",L"-mmt=1",L"-spd",L"-scsUTF-8",L"-sccUTF-8",L"-bso0",L"-bsp1"};if(metadata.type==L"7z"){args.push_back(L"-m0=lzma2");args.push_back(L"-md=512k");}if(!password.empty())args.push_back(L"-p"+password);args.push_back(L"-i@"+include.wstring());args.push_back(L"--");args.push_back(staged.wstring());CheckedRun(args,{},stop,report);}
 if(!add.empty()){auto common=fs::absolute(add[0]).parent_path();for(const auto& source:add){CheckCancelled(stop);if(!fs::exists(source))throw Failure(L"Source file does not exist.");if(fs::absolute(source)==archive||(fs::is_directory(source)&&Below(archive,source)))throw Failure(L"You cannot include the archive itself or its parent directory within the archive.");ScanSafe(source,stop);while(common!=common.root_path()&&!Below(source,common))common=common.parent_path();if(!Below(source,common))throw Failure(L"Files from different disks must be added separately.");}
  auto include=work.path/L"add.txt";std::ofstream file(include,std::ios::binary);for(const auto& source:add)file<<ToUtf8(fs::absolute(source).lexically_relative(common).wstring())<<"\n";file.close();if(!file)throw Failure(L"Cannot write to the add list.");
  std::vector<std::wstring> args{L"a",L"-mx=1",L"-mmt=1",L"-scsUTF-8",L"-sccUTF-8",L"-bso0",L"-bsp1"};if(metadata.type==L"zip")args.push_back(L"-mcu=on");if(metadata.type==L"7z"){args.push_back(L"-m0=lzma2");args.push_back(L"-md=512k");}if(!password.empty()){args.push_back(L"-p"+password);if(metadata.type==L"7z")args.push_back(L"-mhe=on");else args.push_back(L"-mem=AES256");}args.push_back(staged.wstring());args.push_back(L"@"+include.wstring());CheckedRun(args,common,stop,report);
 }
 Inspect(staged,password,stop);Test(staged,password,stop,report);CheckCancelled(stop);
 Handle current(CreateFileW(archive.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(current.h==INVALID_HANDLE_VALUE||!SameIdentity(originalIdentity,Identity(current.h)))throw Failure(L"The archive has been modified by another program. Saving has been stopped. Please reopen.");
 fs::path backup;for(unsigned i=0;i<10000;i++){backup=archive.wstring()+L".bak"+(i?L"."+std::to_wstring(i):L"");if(!fs::exists(backup))break;}if(fs::exists(backup))throw Failure(L"Too many backup files. Please organize and retry.");
 if(!ReplaceFileW(archive.c_str(),staged.c_str(),backup.c_str(),REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr)){DWORD error=GetLastError();std::error_code ec;if(!fs::exists(archive)&&fs::exists(backup))fs::rename(backup,archive,ec);throw Failure(L"Failed to atomically save changes. Original or backup file has been retained. Error "+std::to_wstring(error));}
 return backup;
}
std::vector<fs::path> Engine::Convert(const fs::path& archive,const fs::path& target,const Options& options,const std::wstring& password,std::atomic_bool& stop,const Reporter& report){
 if(fs::absolute(archive)==fs::absolute(target))throw Failure(L"The conversion result must use a new name.");Workspace work(fs::absolute(target).parent_path());auto extracted=Extract(archive,work.path,password,stop,report,{},false);std::vector<fs::path> files;for(const auto& item:fs::directory_iterator(extracted))files.push_back(item.path());if(files.empty())throw Failure(L"An empty archive has no files to convert.");return Compress(files,target,options,stop,report);
}

}
