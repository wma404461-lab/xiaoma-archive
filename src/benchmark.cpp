#include "core.h"
#include <fstream>
#include <chrono>
#include <psapi.h>
#include <objbase.h>
using namespace xm;
static void Write(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(s.data(),s.size());}
static unsigned long long Bytes(const fs::path& p){if(fs::is_regular_file(p))return fs::file_size(p);unsigned long long n=0;for(auto& e:fs::recursive_directory_iterator(p))if(e.is_regular_file())n+=e.file_size();return n;}
int wmain(int argc,wchar_t** argv){if(argc<5)return 2;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);fs::path root=argv[1],out=argv[2],enginePath=argv[3];std::wstring mode=argv[4];std::ofstream results(out,std::ios::binary);Engine engine(enginePath);std::atomic_bool stop{false};
 auto sample=root/L"samples";auto media=sample/L"media";auto text=sample/L"text";
 if(!fs::exists(media/L"video.mp4")){std::string random(32*1024*1024,0);unsigned state=92183;for(auto& c:random){state^=state<<13;state^=state>>17;state^=state<<5;c=(char)state;}Write(media/L"video.mp4",random);Write(text/L"文件.txt",std::string(8*1024*1024,'a'));}
 for(int iteration=0;iteration<3;iteration++){
  auto trial=root/(mode+L"-"+std::to_wstring(iteration));fs::create_directories(trial);
  auto test=[&](const char* label,const std::function<void()>& action){auto t=std::chrono::steady_clock::now();try{action();results<<label<<","<<iteration<<","<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()<<","<<EnginePeakMemory()<<",PASS\n";}catch(const Failure& e){results<<label<<","<<iteration<<",0,0,FAIL:"<<ToUtf8(e.message)<<"\n";}};
  test("media_zip_create",[&](){engine.Compress({media},trial/L"media.zip",Options{},stop);});
  test("media_zip_extract",[&](){auto extracted=engine.Extract(trial/L"media.zip",trial/L"output",L"",stop);if(Bytes(extracted)!=Bytes(media))throw Failure(L"结果大小不一致");});
  Options seven;seven.format=L"7z";
  test("text_7z_create",[&](){engine.Compress({text},trial/L"text.7z",seven,stop);});
  test("text_7z_extract",[&](){auto extracted=engine.Extract(trial/L"text.7z",trial/L"output",L"",stop);if(Bytes(extracted)!=Bytes(text))throw Failure(L"结果大小不一致");});
  test("catalog_20k",[&](){auto catalog=engine.List(root.parent_path().parent_path()/L"fixtures"/L"many.zip",L"",stop);if(catalog.entries.size()!=20000)throw Failure(L"目录数量不一致");});
 }
 PROCESS_MEMORY_COUNTERS_EX pm{};pm.cb=sizeof(pm);GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&pm,sizeof(pm));results<<"process_peak_bytes,"<<pm.PeakWorkingSetSize<<"\n";CoUninitialize();return 0;
}
