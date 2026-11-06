#include "../MaskShaderSource.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <iostream>
std::string Read(const char* p){std::ifstream f(p,std::ios::binary);assert(f.good());std::ostringstream s;s<<f.rdbuf();return s.str();}
int main(int argc,char** argv){
 assert(argc==4);const std::string original=Read(argv[1]),header=Read(argv[2]);
 std::string result=CharacterMaskSource::Build(original,header);
 assert(result.find("uniform float4 leopardMaskColour;")!=std::string::npos);
 // Everything after the original GBuffer include, including native discard,
 // uniforms and entry points, must remain byte-for-byte intact.
 const std::string include="#include \"gbuffer.hlsl\"";
 size_t at=original.find(include)+include.size();
 assert(result.substr(result.size()-(original.size()-at))==original.substr(at));
 bool refused=false;try{CharacterMaskSource::Build("unsupported",header);}catch(const std::runtime_error&){refused=true;}assert(refused);
 std::ofstream out(argv[3],std::ios::binary);out<<result;std::cout<<"PASS native fragment source preserved\n";
}
