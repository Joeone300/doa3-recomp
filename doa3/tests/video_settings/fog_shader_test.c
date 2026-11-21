/* The production HLSL generator must retain a separate no-fog override even
 * when direct register combiners do their own fog blend. No graphics device. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "d3d8_combiners.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
#include "combiner_hlsl_impl.h"
int main(int argc, char **argv)
{
    NV2ACombinerState state={0};
    char shader[16384];
    ID3DBlob *code=NULL,*errors=NULL;
    state.num_stages=1;
    state.final_input[0].reg=NV2A_REG_FOG;
    state.final_input[0].alpha_rep=1;
    state.final_input[1].reg=NV2A_REG_R0;
    state.final_input[2].reg=NV2A_REG_FOG;
    int len=d3d8_combiners_generate_hlsl(&state,shader,sizeof shader);
    CHECK(len>0);
    CHECK(sizeof(NV2APSConstants)==336 && offsetof(NV2APSConstants,fog_disable)==320);
    CHECK(strstr(shader,"fog_disable ? 1.0 : input.fog"));
    if (argc < 2 || strcmp(argv[1], "--compile")) {
        puts("PASS: generated final-combiner fog override and matching 336-byte CPU constant layout");
        return 0;
    }
    HRESULT hr=D3DCompile(shader,len,"fog-test",NULL,NULL,"main","ps_5_0",0,0,&code,&errors);
    if(FAILED(hr) && errors) fprintf(stderr,"%s\n",(char *)ID3D10Blob_GetBufferPointer(errors));
    CHECK(SUCCEEDED(hr) && code);
    ID3D10Blob_Release(code);
    if(errors) ID3D10Blob_Release(errors);
    puts("PASS: generated final-combiner fog override compiles; CPU constant buffer size and offset match HLSL");
    return 0;
}
