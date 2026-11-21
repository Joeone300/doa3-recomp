/* Tests the production resize transaction with deterministic allocation failures.
 * No GPU or game assets required. The five candidate resources must all exist
 * before ANY old resource or binding changes. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "video_settings.h"
#include <stddef.h>
ptrdiff_t g_xbox_mem_offset;
HWND d3d8_GetHWND(void) { return NULL; }
typedef struct { float TopLeftX,TopLeftY,Width,Height,MinDepth,MaxDepth; } D3D11_VIEWPORT;
typedef struct {
    unsigned width,height;
    void *default_tex,*default_rtv,*default_srv,*default_depth,*default_dsv;
    void *d3d11_context;
    struct { unsigned X,Y,Width,Height; } viewport;
} D3D8DeviceState;
static D3D8DeviceState g_device_state;
static struct { int valid; } g_srv_bindings;
static int g_off_active,fail_at,live,unbinds,history_invalidated;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static HRESULT d3d11_create_guest_target(D3D8DeviceState *s)
{
    void **resources[]={&s->default_tex,&s->default_rtv,&s->default_srv,&s->default_depth,&s->default_dsv};
    for(int i=0;i<5;++i) {
        if(fail_at==i+1) return E_OUTOFMEMORY;
        *resources[i]=malloc(1); CHECK(*resources[i]); ++live;
    }
    return S_OK;
}
static void d3d11_release_guest_target(D3D8DeviceState *s)
{
    void **resources[]={&s->default_tex,&s->default_rtv,&s->default_srv,&s->default_depth,&s->default_dsv};
    for(int i=0;i<5;++i) if(*resources[i]) { free(*resources[i]); *resources[i]=NULL; --live; }
}
static void mock_targets(void *ctx,unsigned n,void **rtv,void *dsv)
{ (void)ctx;(void)rtv;(void)dsv; if(!n) ++unbinds; }
static void mock_viewport(void *ctx,unsigned n,D3D11_VIEWPORT *vp)
{ (void)ctx; CHECK(n==1 && vp->Width==1921 && vp->Height==1081); }
#define ID3D11DeviceContext_OMSetRenderTargets mock_targets
#define ID3D11DeviceContext_RSSetViewports mock_viewport
static void d3d8_ResetScissorRect(void) {}
static void d3d8_invalidate_frame_history(void) { ++history_invalidated; }
#include "target_resize_impl.h"
int main(void)
{
    unsigned w,h;
    video_set_aspect(VIDEO_ASPECT_16_9); video_set_scale(2);
    video_guest_target_size(video_get_aspect(),&w,&h);
    g_device_state.width=w; g_device_state.height=h;
    CHECK(SUCCEEDED(d3d11_create_guest_target(&g_device_state)));
    video_target_size_result(1);
    for(fail_at=1;fail_at<=5;++fail_at) {
        void *old=g_device_state.default_tex;
        g_srv_bindings.valid=1; g_off_active=1;
        CHECK(video_set_custom_resolution(1921,1081));
        video_set_aspect(VIDEO_ASPECT_4_3_FULL);
        d3d8_apply_target_size_change();
        CHECK(live==5 && !unbinds && !history_invalidated);
        CHECK(g_device_state.default_tex==old && g_device_state.width==w && g_device_state.height==h);
        CHECK(g_srv_bindings.valid==1 && g_off_active==1);
        CHECK(video_get_scale()==2 && video_get_aspect()==VIDEO_ASPECT_16_9);
        CHECK(video_resolution_failed());
        d3d8_apply_target_size_change();
        CHECK(video_resolution_failed()); /* remains visible until edited */
    }
    fail_at=0;
    CHECK(video_set_custom_resolution(1921,1081));
    d3d8_apply_target_size_change();
    CHECK(live==5 && unbinds==1 && history_invalidated==1 && !g_off_active);
    CHECK(g_device_state.width==1921 && g_device_state.height==1081);
    CHECK(g_device_state.viewport.Width==1921 && g_device_state.viewport.Height==1081);
    CHECK(video_get_scale()==0 && !video_resolution_failed());
    d3d11_release_guest_target(&g_device_state); CHECK(!live);
    puts("PASS: all five allocation failures preserve live targets, bindings and settings; success replaces targets and invalidates history");
    return 0;
}
