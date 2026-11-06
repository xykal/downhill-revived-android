/* Synthetic fast-vs-slow test of the actual native renderer recorder. Does not test the GPU. */
#define DHD_GS_TEST_BUILD 1
#include "gpu_rust.c"
#include <stdint.h>
uint64_t kernel_vsync_count;
uintptr_t guest_base;
static uint32_t rng=0x8b46a291;
static uint32_t rnd(void) { rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng; }
static Gs gsA, gsB;
static Native a, b;
static Rust ra, rb;
static GsVertex verts[260];
static uint8_t flags[258];
static void init(Native *n, Rust *r, Gs *g, int indexed, unsigned prim) {
  memset(n,0,sizeof *n); memset(r,0,sizeof *r); memset(g,0,sizeof *g);
  n->recording=&n->frames[0]; n->current_valid=1; n->indexed=indexed; n->state_prim=prim;
  n->conv_ok=~0u; n->zmax=0xffffffffu; n->zscale=1.0f/4294967296.0f;
  n->uv_scale[0]=1.0f/4096; n->uv_scale[1]=1.0f/8192;
  for(int i=0;i<256;i++) n->vertex_fog[i]=(float)i/255.0f;
  n->box[0]=n->box[1]=INT_MAX; n->box[2]=n->box[3]=INT_MIN;
  r->native=n; n->exact=r; g->backend=r; g->reg[GS_PRIM]=prim; g->reg[GS_PRMODECONT]=1;
}
static int compare(Native *x, Native *y, unsigned trial, const char *where) {
  NativeFrame *p=x->recording, *q=y->recording;
  if(p->vertex_count!=q->vertex_count || p->index_count!=q->index_count ||
     p->path1!=q->path1 || memcmp(p->vertices,q->vertices,p->vertex_count*sizeof *p->vertices) ||
     (p->index_count && memcmp(p->indices,q->indices,p->index_count*sizeof *p->indices)) ||
     memcmp(x->box,y->box,sizeof x->box) || x->stats.draws!=y->stats.draws ||
     x->stats.primitives!=y->stats.primitives || x->conv_ok!=y->conv_ok ||
     x->conv_fst!=y->conv_fst || x->conv_offset!=y->conv_offset ||
     (x->conv_ok==x->conv_serial && (memcmp(x->conv_gs,y->conv_gs,sizeof x->conv_gs) || memcmp(x->conv,y->conv,sizeof x->conv)))) {
       fprintf(stderr,"DIFF trial=%u phase=%s vertices=%u/%u indices=%u/%u draws=%llu/%llu\n",trial,where,p->vertex_count,q->vertex_count,p->index_count,q->index_count,(unsigned long long)x->stats.draws,(unsigned long long)y->stats.draws);return 0;
  }
  return 1;
}
static void release(Native *n) {free(n->recording->vertices);free(n->recording->indices);}
int main(int argc,char **argv) {
  unsigned trials=argc>1 ? (unsigned)atoi(argv[1]):10000;
  for(unsigned t=0;t<trials;t++) {
    unsigned prim=4 | ((rnd()&1)<<3) | ((rnd()&1)<<8); int indexed=rnd()&1;
    init(&a,&ra,&gsA,indexed,prim); init(&b,&rb,&gsB,indexed,prim);
    uint64_t offset=((uint64_t)(rnd()&65535)<<32)|(rnd()&65535);
    gsA.reg[GS_XYOFFSET_1]=gsB.reg[GS_XYOFFSET_1]=offset;
    gsA.source_path=gsB.source_path=(rnd()&1)?1:3;
    unsigned count=3+rnd()%256;
    for(unsigned k=0;k<count;k++) {
      GsVertex *v=&verts[k]; v->x=rnd()&65535;v->y=rnd()&65535;v->z=rnd();v->rgba=rnd();v->fog=rnd()&255;
      v->u=rnd()&0x3fff;v->v=rnd()&0x3fff;v->s=(int)(rnd()%1000)/100.0f;v->t=(int)(rnd()%1000)/100.0f;v->q=(1+rnd()%1000)/100.0f;
    }
    for(unsigned k=0;k<count-2;k++) flags[k]=(rnd()%5)!=0;
    native_draw_strip(&gsA,4,verts,count,flags);
    for(unsigned k=0;k<count-2;k++) if(flags[k]) native_draw(&gsB,4,&verts[k],3);
    if(argc>2 && a.recording->vertex_count) a.recording->vertices[0].rgba ^= 1; /* negative control */
    if(!compare(&a,&b,t,"strip")) return 1;
    /* A following draw checks that surviving conversion cache is identical. */
    native_draw(&gsA,4,&verts[count-3],3); native_draw(&gsB,4,&verts[count-3],3);
    if(!compare(&a,&b,t,"following")) return 1;
    release(&a);release(&b);
  }
  printf("MATCH: %u random native strip batches plus following draws, indexed/nonindexed, flat/smooth, FST/STQ, skipped triangles\n",trials);
  return 0;
}
