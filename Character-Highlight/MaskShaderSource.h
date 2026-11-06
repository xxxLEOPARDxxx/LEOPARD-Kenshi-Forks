#pragma once
#include <string>
#include <stdexcept>
namespace CharacterMaskSource {
inline void ReplaceBody(std::string& s,const char* function,const char* body){
 size_t at=s.find(std::string("void ")+function+"(");
 if(at==std::string::npos)throw std::runtime_error("unsupported GBuffer helper layout");
 size_t start=s.find('{',at),end=start;int depth=0;
 if(start==std::string::npos)throw std::runtime_error("invalid GBuffer helper");
 for(;end<s.size();++end){if(s[end]=='{')++depth;if(s[end]=='}' && --depth==0)break;}
 if(end>=s.size())throw std::runtime_error("unterminated GBuffer helper");
 s.replace(start,end-start+1,body);
}
inline std::string Build(std::string source,std::string header){
 const std::string include="#include \"gbuffer.hlsl\"";size_t at=source.find(include);
 if(at==std::string::npos)throw std::runtime_error("native character shader has no supported GBuffer include");
 ReplaceBody(header,"writeAlbedo","{buffer.buf0=leopardMaskColour;}");
 ReplaceBody(header,"writeMetalness","{}");ReplaceBody(header,"writeGloss","{}");ReplaceBody(header,"writeNormal","{}");
 ReplaceBody(header,"writeDepth","{buffer.buf0.a=leopardMaskDepth;}");
 source.replace(at,include.size(),"uniform float4 leopardMaskColour;uniform float leopardMaskDepth;\n"+header);
 return source;
}
}
