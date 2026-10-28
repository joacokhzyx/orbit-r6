#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static void signed_magic(int64_t d, int64_t *mul, int *shift) {
    const uint64_t two63 = UINT64_C(1) << 63;
    uint64_t ad = d < 0 ? UINT64_C(0) - (uint64_t)d : (uint64_t)d;
    uint64_t t = two63 + ((uint64_t)d >> 63);
    uint64_t anc = t - 1 - t % ad;
    uint64_t q1 = two63 / anc, r1 = two63 - q1 * anc;
    uint64_t q2 = two63 / ad, r2 = two63 - q2 * ad;
    uint64_t delta; int p = 63;
    do { ++p; q1 = 2 * q1; r1 = 2 * r1; if (r1 >= anc) { ++q1; r1 -= anc; }
        q2 = 2 * q2; r2 = 2 * r2; if (r2 >= ad) { ++q2; r2 -= ad; }
        delta = ad - r2; } while (q1 < delta || (q1 == delta && r1 == 0));
    *mul = (int64_t)(q2 + 1);
    if (d < 0) *mul = (int64_t)(UINT64_C(0) - (uint64_t)*mul);
    *shift = p - 64;
}
static int64_t run(int64_t n, int64_t d, int rem) {
    uint64_t ad = d < 0 ? UINT64_C(0) - (uint64_t)d : (uint64_t)d;
    if (ad == 1) return rem ? 0 : n;
    if ((ad & (ad - 1)) == 0) {
        unsigned k = 0; while ((UINT64_C(1) << k) != ad) ++k;
        int64_t rcx = n >> 63; rcx = (int64_t)((uint64_t)rcx >> (64 - k));
        int64_t rax = (int64_t)((uint64_t)n + (uint64_t)rcx); rax >>= k;
        if (rem) { rax = (int64_t)((uint64_t)rax << k); rax = -(uint64_t)rax; rax = (int64_t)((uint64_t)rax + (uint64_t)n); }
        else if (d < 0) rax = (int64_t)(-(uint64_t)rax);
        return rax;
    }
    int64_t mul; int shift; signed_magic(d, &mul, &shift);
    int64_t rcx = n;
    __int128 prod = (__int128)mul * rcx;
    int64_t rdx = (int64_t)(prod >> 64);
    if (d > 0 && mul < 0) rdx = (int64_t)((uint64_t)rdx + (uint64_t)rcx);
    if (d < 0 && mul > 0) rdx = (int64_t)((uint64_t)rdx - (uint64_t)rcx);
    if (shift) rdx >>= shift;
    uint64_t sign = (uint64_t)rdx >> 63; rdx = (int64_t)((uint64_t)rdx + sign);
    if (!rem) return rdx;
    int64_t rax = (int64_t)((uint64_t)rdx * (uint64_t)d);
    return (int64_t)((uint64_t)rcx - (uint64_t)rax);
}
static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void){ s ^= s<<13; s ^= s>>7; s ^= s<<17; return s; }
int main(void){
    int64_t edge[] = {0,1,-1,2,-2,3,-3,7,-7,10,-10,100,2147483647LL,-2147483648LL,2147483648LL,4294967296LL,
        INT64_MAX,INT64_MAX-1,INT64_MIN,INT64_MIN+1,INT64_MAX/2,INT64_MIN/2,1000000007LL,-1000000007LL};
    int ne = sizeof edge/sizeof edge[0]; long bad=0, tested=0;
    for (int trial=0; trial<4000; ++trial) {
        int64_t d;
        if (trial < ne*2) d = edge[trial%ne] * (trial<ne?1:3);
        else { d = (int64_t)rnd(); int sh = rnd()%63; d >>= sh; }
        if (d==0||d==-1||d==INT64_MIN) continue;
        for (int k=0;k<3000;++k) {
            int64_t n;
            if (k<ne) n = edge[k]; else if (k<ne*2) n = edge[k-ne]+d; else if (k<ne*3) n=edge[k-2*ne]-d; else if (k<ne*3+100) n = d*(k-ne*3-50); else { n=(int64_t)rnd(); n >>= rnd()%64; }
            int64_t q = n / d, r = n % d;
            tested++;
            if (run(n,d,0)!=q || run(n,d,1)!=r) { if (bad++<10) printf("BAD n=%lld d=%lld got %lld/%lld want %lld/%lld\n",(long long)n,(long long)d,(long long)run(n,d,0),(long long)run(n,d,1),(long long)q,(long long)r); }
        }
    }
    printf("tested %ld bad %ld\n", tested, bad);
    return bad!=0;
}
