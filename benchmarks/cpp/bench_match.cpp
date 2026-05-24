#include <cstdio>
int main() { long long total=0; for (int i=0; i<1000000; i++) { switch(i%10) { case 0: total+=1; break; case 1: total+=2; break; case 2: total+=3; break; case 3: total+=4; break; case 4: total+=5; break; case 5: total+=6; break; case 6: total+=7; break; case 7: total+=8; break; case 8: total+=9; break; default: total+=10; } } printf("%lld\n", total); return 0; }
