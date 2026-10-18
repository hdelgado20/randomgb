/* Disco Aleatorio - port para Game Boy (GBDK-2020)
 * A: elegir al azar   B: Album/Artista   SELECT: filtro de formato   START: menu
 * La coleccion se guarda en la SRAM de la cartucho (8 KB, hasta 170 discos). */
#include <gb/gb.h>
#include <gb/console.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <rand.h>
#include "preload.h"
#include "disc_tiles.h"

#ifndef ENABLE_RAM
#define ENABLE_RAM (*(volatile uint8_t *)0x0000 = 0x0A)
#endif

#define MAXA        170
#define ENT_SIZE    48
#define SRAM_BASE   0xA000u
#define DISC_TILE0  144
#define NONE        255

typedef struct { char artist[20]; char title[27]; uint8_t fmt; } Entry;
typedef struct { char magic[4]; uint8_t count, mode, filter, lang, pal, nether; } Header;
#define HDR ((volatile Header *)SRAM_BASE)
#define ENT(i) ((Entry *)(SRAM_BASE + 16u + (uint16_t)(i) * ENT_SIZE))

/* ---------- textos (0=EN, 1=ES) ---------- */
enum { S_TITLE, S_ALBUM, S_ARTIST, S_ALL, S_HINT, S_HELP1, S_HELP2, S_PREV, S_NONE, S_RECS, S_ALBS,
       S_M_ADD, S_M_LIST, S_M_MODE, S_M_FMT, S_M_LANG, S_M_THEME, S_SORT0, S_SORT1, S_SORT2, S_SORT3,
       S_SURE, S_ITEMHELP, S_FULL, S_LANGNAME, S_CLASSIC, S_HELL, S_UNLOCK, S_FMTQ, S_ADDHELP, S_EMPTY, NS };
static const char *const TXT[2][NS] = {
 { "DISCO ALEATORIO","ALBUM","ARTIST","ALL","PRESS A","A:PICK  B:MODE","SEL:FMT STA:MENU","BEFORE:","NO MATCH","RECORDS","ALBUMS",
   "ADD ALBUM","LIST","MODE","FORMAT","LANGUAGE","THEME","RECENT","ARTIST","ALBUM","FORMAT",
   "A AGAIN: DELETE","A:DELETE B:BACK","FULL","ENGLISH","CLASSIC","BLOCKY HELL","NEW THEME UNLOCKED","FORMAT?","A:OK B:BACK","MENU > ADD" },
 { "DISCO ALEATORIO","ALBUM","ARTISTA","TODO","PULSA A","A:ELEGIR B:MODO","SEL:FMT STA:MENU","ANTES:","NINGUNO","DISCOS","ALBUMES",
   "ANADIR","LISTA","MODO","FORMATO","IDIOMA","TEMA","RECIENTE","ARTISTA","ALBUM","FORMATO",
   "A OTRA VEZ: BORRAR","A:BORRAR B:VOLVER","LLENO","ESPANOL","CLASICO","BLOCKY HELL","NUEVO TEMA","FORMATO?","A:OK B:VOLVER","MENU > ANADIR" },
};
#define TT(k) (TXT[HDR->lang ? 1 : 0][k])
static const char *const FN[2][4] = { {"VINYL","CD","CASS","OTHER"}, {"VINILO","CD","CASETE","OTRO"} };
#define FNAME(f) (FN[HDR->lang ? 1 : 0][f])

/* ---------- estado ---------- */
static uint8_t keys, prevKeys, pressed, seedCnt, seeded;
static uint8_t pickIdx = NONE, lastIdx = NONE;
static char prevText[28];
static uint8_t dframe, dcnt;
static uint8_t cbuf[MAXA], firstA[MAXA], okA[MAXA], order[MAXA];

/* ---------- utilidades ---------- */
static void readKeys(void){ prevKeys = keys; keys = joypad(); pressed = keys & (uint8_t)~prevKeys; }
static void tick(void){ wait_vbl_done(); seedCnt++; readKeys(); }
static void pr(uint8_t x, uint8_t y, const char *s, uint8_t w){
  char b[21]; uint8_t i = 0;
  while(i < w && s[i]){ b[i] = s[i]; i++; }
  while(i < w){ b[i++] = ' '; }
  b[w] = 0; gotoxy(x, y); printf("%s", b);
}
static char lc(char c){ return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static uint8_t eqi(const char *a, const char *b){
  while(*a && *b){ if(lc(*a) != lc(*b)) return 0; a++; b++; }
  return *a == *b;
}
static int8_t cmpi(const char *a, const char *b){
  while(*a && *b){ char x = lc(*a), y = lc(*b); if(x != y) return x < y ? -1 : 1; a++; b++; }
  if(*a) return 1;
  if(*b) return -1;
  return 0;
}
static uint8_t has418(const char *s){
  while(*s){ if(lc(s[0])=='c' && s[1]=='4' && s[2]=='1' && s[3]=='8') return 1; s++; }
  return 0;
}
static uint8_t rnd(uint8_t n){
  if(!seeded){ seeded = 1; initrand(((uint16_t)DIV_REG << 8) | seedCnt); }
  return (uint8_t)((((uint16_t)rand() << 8) | rand()) % n);
}
static uint8_t wrapText(const char *s, uint8_t w, char lines[][15], uint8_t maxl){
  uint8_t n = 0, len = 0; const char *p = s;
  while(*p && n < maxl){
    const char *e; uint8_t wl;
    while(*p == ' ') p++;
    if(!*p) break;
    e = p; while(*e && *e != ' ') e++;
    wl = (uint8_t)(e - p);
    if(wl > w){ wl = w; e = p + w; }
    if(len && (uint8_t)(len + 1 + wl) > w){ lines[n][len] = 0; n++; len = 0; if(n >= maxl) break; }
    if(len) lines[n][len++] = ' ';
    memcpy(&lines[n][len], p, wl); len += wl; p = e;
  }
  if(len && n < maxl){ lines[n][len] = 0; n++; }
  return n;
}
static void setPalette(void){
  uint8_t p = HDR->pal ? 0x1B : 0xE4;
  BGP_REG = p; OBP0_REG = p;
}

/* ---------- disco (sprites) ---------- */
static void spinDisc(uint8_t every){
  uint8_t k;
  if(++dcnt < every) return;
  dcnt = 0; dframe = (dframe + 1) & 3;
  for(k = 0; k < 16; k++) set_sprite_tile(k, DISC_TILE0 + dframe * 16 + k);
}
static void showDisc(uint8_t on){
  uint8_t k;
  for(k = 0; k < 16; k++){
    set_sprite_tile(k, DISC_TILE0 + dframe * 16 + k);
    if(on) move_sprite(k, 16 + (k & 3) * 8, 32 + (k >> 2) * 8); else move_sprite(k, 0, 0);
  }
}

/* ---------- datos ---------- */
static uint8_t inFilter(uint8_t i){ uint8_t f = HDR->filter; return f == 0 || ENT(i)->fmt == f - 1; }
static uint8_t cand(uint8_t skipLast){
  uint8_t n = 0, i;
  for(i = 0; i < HDR->count; i++) if(inFilter(i) && !(skipLast && i == lastIdx)) cbuf[n++] = i;
  return n;
}
static uint8_t pickAlbum(void){
  uint8_t n = cand(1);
  if(!n) n = cand(0);
  if(!n) return NONE;
  return cbuf[rnd(n)];
}
static uint8_t pickArtist(void){
  uint8_t n = cand(0), nu = 0, no = 0, i, j;
  if(!n) return NONE;
  for(i = 0; i < n; i++){
    uint8_t dup = 0;
    for(j = 0; j < nu; j++) if(eqi(ENT(cbuf[i])->artist, ENT(firstA[j])->artist)){ dup = 1; break; }
    if(!dup) firstA[nu++] = cbuf[i];
  }
  for(j = 0; j < nu; j++)
    if(!(nu > 1 && lastIdx != NONE && eqi(ENT(firstA[j])->artist, ENT(lastIdx)->artist))) okA[no++] = firstA[j];
  return okA[rnd(no)];
}
static void delEntry(uint8_t i){
  uint8_t n = HDR->count, j;
  for(j = i; j + 1 < n; j++) memcpy(ENT(j), ENT(j + 1), ENT_SIZE);
  HDR->count = n - 1; lastIdx = NONE; pickIdx = NONE; prevText[0] = 0;
}
static void initStorage(void){
  ENABLE_RAM;
  if(memcmp((const void *)SRAM_BASE, "DAL2", 4) != 0){
    uint8_t i;
    memcpy((void *)SRAM_BASE, "DAL2", 4);
    HDR->count = 0; HDR->mode = 0; HDR->filter = 0; HDR->lang = 0; HDR->pal = 0; HDR->nether = 0;
    for(i = 0; i < PRELOAD_N && i < MAXA; i++){ memcpy(ENT(i), &preload_blob[(uint16_t)i * ENT_SIZE], ENT_SIZE); }
    HDR->count = PRELOAD_N;
  }
  if(HDR->count > MAXA) HDR->count = 0;
}
static void msg(const char *s){
  uint8_t t; pr(0, 9, s, 20);
  for(t = 0; t < 70; t++) tick();
  pr(0, 9, "", 20);
}

/* ---------- pantalla principal ---------- */
static void drawHeader(void){
  char b[21];
  pr(2, 0, TT(S_TITLE), 17);
  pr(0, 1, HDR->mode ? TT(S_ARTIST) : TT(S_ALBUM), 9);
  sprintf(b, "FMT:%s", HDR->filter ? FNAME(HDR->filter - 1) : TT(S_ALL));
  pr(10, 1, b, 10);
}
static void drawPick(uint8_t d){
  char lines[3][15], b[21]; uint8_t i, n, cnt, shown;
  for(i = 2; i <= 8; i++) pr(6, i, "", 14);
  if(d == NONE || d >= HDR->count){
    pr(6, 3, TT(S_HINT), 14);
    if(!HDR->count) pr(6, 5, TT(S_EMPTY), 14);
    return;
  }
  if(HDR->mode){
    n = wrapText(ENT(d)->artist, 14, lines, 3);
    for(i = 0; i < n; i++) pr(6, 2 + i, lines[i], 14);
    cnt = 0; shown = 0;
    for(i = 0; i < HDR->count; i++) if(inFilter(i) && eqi(ENT(i)->artist, ENT(d)->artist)){
      cnt++;
      if(shown < 2){ pr(6, 6 + shown, ENT(i)->title, 14); shown++; }
    }
    sprintf(b, "%u %s", (unsigned)cnt, TT(S_ALBS)); pr(6, 5, b, 14);
  } else {
    n = wrapText(ENT(d)->title, 14, lines, 3);
    for(i = 0; i < n; i++) pr(6, 2 + i, lines[i], 14);
    n = wrapText(ENT(d)->artist, 14, lines, 2);
    for(i = 0; i < n; i++) pr(6, 5 + i, lines[i], 14);
    pr(6, 7, FNAME(ENT(d)->fmt), 14);
  }
}
static void drawStatic(void){
  char b[21];
  cls(); drawHeader(); drawPick(pickIdx);
  if(prevText[0]){ sprintf(b, "%s %s", TT(S_PREV), prevText); pr(0, 10, b, 20); }
  sprintf(b, "%u %s", (unsigned)HDR->count, TT(S_RECS)); pr(0, 12, b, 20);
  pr(0, 15, TT(S_HELP1), 20); pr(0, 16, TT(S_HELP2), 19);
}
static void doPick(void){
  uint8_t fin, t, show, n;
  fin = HDR->mode ? pickArtist() : pickAlbum();
  if(fin == NONE){ msg(TT(S_NONE)); return; }
  for(t = 0; t < 24; t++){
    if(t % 3 == 0){ n = cand(0); if(n){ show = cbuf[rnd(n)]; drawPick(show); } }
    spinDisc(2); tick();
  }
  if(pickIdx != NONE && pickIdx < HDR->count)
    strcpy(prevText, HDR->mode ? ENT(pickIdx)->artist : ENT(pickIdx)->title);
  pickIdx = lastIdx = fin;
  drawStatic();
}

/* ---------- teclado en pantalla ---------- */
static const char *const KROW[5] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789.-'&", "!?,:+()/#*" };
static const uint8_t ACOL[4] = { 1, 6, 11, 17 };
static char keyChar(uint8_t r, uint8_t c, uint8_t lower){
  char ch = KROW[r][c];
  if(lower && ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return ch;
}
static void kbdDrawKeys(uint8_t lower){
  uint8_t r, c, i; char ch[2]; ch[1] = 0;
  for(r = 0; r < 5; r++) for(c = 0; c < 10; c++){ ch[0] = keyChar(r, c, lower); pr(2 * c + 1, 4 + r, ch, 1); }
  pr(1, 10, "DEL", 3); pr(6, 10, "SPC", 3); pr(11, 10, lower ? "abc" : "ABC", 4); pr(17, 10, "OK", 2);
  (void)i;
}
static void kbdCursor(uint8_t r, uint8_t c, uint8_t on){
  if(r < 5) pr(2 * c, 4 + r, on ? ">" : " ", 1);
  else pr(ACOL[c] - 1, 10, on ? ">" : " ", 1);
}
static uint8_t kbd(const char *label, char *buf, uint8_t maxlen){
  uint8_t r = 0, c = 0, lower = 0, len = (uint8_t)strlen(buf), done = 0, res = 0, i, st;
  char ch[2]; ch[1] = 0;
  cls(); pr(0, 0, label, 20); pr(0, 13, TT(S_ADDHELP), 20); pr(0, 14, "SEL:abc  STA:OK", 20);
  kbdDrawKeys(lower); kbdCursor(r, c, 1);
  while(!done){
    st = 0;
    pr(0, 2, "", 20);
    { uint8_t from = len > 19 ? len - 19 : 0; pr(0, 2, buf + from, 19); }
    do { tick(); } while(!pressed);
    kbdCursor(r, c, 0);
    if(pressed & J_UP)    { if(r == 0) r = 5; else r--; }
    if(pressed & J_DOWN)  { r = (r == 5) ? 0 : r + 1; }
    if(pressed & J_LEFT)  { uint8_t m = (r == 5) ? 4 : 10; c = (c == 0) ? m - 1 : c - 1; }
    if(pressed & J_RIGHT) { uint8_t m = (r == 5) ? 4 : 10; c = (c + 1 >= m) ? 0 : c + 1; }
    if(r == 5 && c > 3) c = 3;
    if(pressed & J_SELECT){ lower = !lower; kbdDrawKeys(lower); }
    if(pressed & J_B){ if(len){ buf[--len] = 0; } }
    if(pressed & J_START){ st = 1; }
    if(pressed & J_A){
      if(r < 5){ if(len < maxlen){ buf[len++] = keyChar(r, c, lower); buf[len] = 0; } }
      else if(c == 0){ if(len){ buf[--len] = 0; } }
      else if(c == 1){ if(len < maxlen){ buf[len++] = ' '; buf[len] = 0; } }
      else if(c == 2){ lower = !lower; kbdDrawKeys(lower); }
      else st = 1;
    }
    if(st){ done = 1; res = (len > 0); }
    kbdCursor(r, c, 1);
  }
  (void)i; (void)ch;
  return res;
}
static uint8_t chooseFmt(void){
  uint8_t sel = 0, i;
  cls(); pr(0, 1, TT(S_FMTQ), 20); pr(0, 14, TT(S_ADDHELP), 20);
  for(i = 0; i < 4; i++) pr(2, 4 + i, FNAME(i), 10);
  pr(0, 4, ">", 1);
  while(1){
    do { tick(); } while(!pressed);
    pr(0, 4 + sel, " ", 1);
    if(pressed & J_UP) sel = sel ? sel - 1 : 3;
    if(pressed & J_DOWN) sel = (sel + 1) & 3;
    if(pressed & J_B) return NONE;
    if(pressed & (J_A | J_START)) return sel;
    pr(0, 4 + sel, ">", 1);
  }
}
static void addFlow(void){
  char a[20], t[27]; uint8_t f, n;
  if(HDR->count >= MAXA){ cls(); msg(TT(S_FULL)); return; }
  a[0] = 0; if(!kbd(TT(S_ARTIST), a, 19)) return;
  t[0] = 0; if(!kbd(TT(S_ALBUM), t, 26)) return;
  f = chooseFmt(); if(f == NONE) return;
  n = HDR->count;
  strcpy(ENT(n)->artist, a); strcpy(ENT(n)->title, t); ENT(n)->fmt = f;
  HDR->count = n + 1;
  if(has418(a) && !HDR->nether){
    HDR->nether = 1; cls(); pr(0, 6, TT(S_UNLOCK), 20); pr(4, 8, "BLOCKY HELL", 12);
    { uint8_t k; for(k = 0; k < 120; k++) tick(); }
  }
}

/* ---------- lista ---------- */
static void buildOrder(uint8_t mode){
  uint8_t n = HDR->count, i, j, v;
  for(i = 0; i < n; i++) order[i] = mode == 0 ? (uint8_t)(n - 1 - i) : i;
  if(mode == 0) return;
  for(i = 1; i < n; i++){
    v = order[i]; j = i;
    while(j > 0){
      Entry *x = ENT(order[j - 1]), *y = ENT(v); int8_t r = 0;
      if(mode == 1) r = cmpi(x->artist, y->artist);
      else if(mode == 2) r = cmpi(x->title, y->title);
      else r = (x->fmt > y->fmt) - (x->fmt < y->fmt);
      if(!r) r = cmpi(x->artist, y->artist);
      if(!r) r = cmpi(x->title, y->title);
      if(r <= 0) break;
      order[j] = order[j - 1]; j--;
    }
    order[j] = v;
  }
}
static uint8_t itemScreen(uint8_t idx){
  char lines[3][15]; uint8_t n, i, armed = 0;
  cls();
  n = wrapText(ENT(idx)->title, 14, lines, 3);
  for(i = 0; i < n; i++) pr(1, 2 + i, lines[i], 18);
  pr(1, 6, ENT(idx)->artist, 18); pr(1, 8, FNAME(ENT(idx)->fmt), 18);
  pr(0, 12, TT(S_ITEMHELP), 20);
  while(1){
    do { tick(); } while(!pressed);
    if(pressed & J_B) return 0;
    if(pressed & J_A){
      if(armed){ delEntry(idx); return 1; }
      armed = 1; pr(0, 14, TT(S_SURE), 20);
    }
  }
}
static void drawList(uint8_t mode, uint8_t cur, uint8_t scroll){
  char b[21]; uint8_t r, oi;
  sprintf(b, "%u %s %s", (unsigned)HDR->count, TT(S_RECS), TT(S_SORT0 + mode)); pr(0, 0, b, 20);
  for(r = 0; r < 15; r++){
    oi = scroll + r;
    if(oi < HDR->count){
      Entry *e = ENT(order[oi]);
      sprintf(b, "%c%c%s-%s", oi == cur ? '>' : ' ', "VCTO"[e->fmt], e->artist, e->title);
      pr(0, 1 + r, b, 20);
    } else pr(0, 1 + r, "", 20);
  }
  pr(0, 16, "A:OPEN B:BACK SEL:ORD", 20);
}
static void listScreen(void){
  uint8_t mode = 0, cur = 0, scroll = 0, n, redraw = 1;
  buildOrder(mode); cls();
  while(1){
    n = HDR->count;
    if(redraw){ drawList(mode, cur, scroll); redraw = 0; }
    do { tick(); } while(!pressed);
    if(pressed & J_B) return;
    if(n){
      if((pressed & J_DOWN) && cur + 1 < n){ cur++; }
      if((pressed & J_UP) && cur > 0){ cur--; }
      if((pressed & J_RIGHT)){ cur = (cur + 14 < n) ? cur + 14 : n - 1; }
      if((pressed & J_LEFT)){ cur = (cur > 14) ? cur - 14 : 0; }
      if(cur < scroll) scroll = cur;
      if(cur > scroll + 14) scroll = cur - 14;
      if(pressed & J_A){
        cls();
        if(itemScreen(order[cur])){ buildOrder(mode); n = HDR->count; if(cur >= n) cur = n ? n - 1 : 0; if(scroll > cur) scroll = cur; }
        cls();
      }
    }
    if(pressed & J_SELECT){ mode = (mode + 1) & 3; buildOrder(mode); cur = 0; scroll = 0; cls(); }
    redraw = 1;
  }
}

/* ---------- menu ---------- */
static void drawMenu(uint8_t sel, uint8_t items){
  char b[21]; uint8_t i;
  pr(0, 1, "== MENU ==", 20);
  for(i = 0; i < items; i++){
    switch(i){
      case 0: sprintf(b, "%c%s", sel == i ? '>' : ' ', TT(S_M_ADD)); break;
      case 1: sprintf(b, "%c%s", sel == i ? '>' : ' ', TT(S_M_LIST)); break;
      case 2: sprintf(b, "%c%s:%s", sel == i ? '>' : ' ', TT(S_M_MODE), HDR->mode ? TT(S_ARTIST) : TT(S_ALBUM)); break;
      case 3: sprintf(b, "%c%s:%s", sel == i ? '>' : ' ', TT(S_M_FMT), HDR->filter ? FNAME(HDR->filter - 1) : TT(S_ALL)); break;
      case 4: sprintf(b, "%c%s:%s", sel == i ? '>' : ' ', TT(S_M_LANG), TT(S_LANGNAME)); break;
      default: sprintf(b, "%c%s:%s", sel == i ? '>' : ' ', TT(S_M_THEME), HDR->pal ? TT(S_HELL) : TT(S_CLASSIC)); break;
    }
    pr(0, 3 + i, b, 20);
  }
}
static void menuScreen(void){
  uint8_t sel = 0, items;
  cls();
  while(1){
    items = HDR->nether ? 6 : 5;
    if(sel >= items) sel = 0;
    drawMenu(sel, items);
    do { tick(); } while(!pressed);
    if(pressed & (J_B | J_START)) return;
    if(pressed & J_UP) sel = sel ? sel - 1 : items - 1;
    if(pressed & J_DOWN) sel = (sel + 1 >= items) ? 0 : sel + 1;
    if(pressed & (J_A | J_LEFT | J_RIGHT)){
      if(sel == 0 && (pressed & J_A)){ addFlow(); cls(); }
      else if(sel == 1 && (pressed & J_A)){ listScreen(); cls(); }
      else if(sel == 2) HDR->mode ^= 1;
      else if(sel == 3){ HDR->filter = (HDR->filter + ((pressed & J_LEFT) ? 4 : 1)) % 5; }
      else if(sel == 4){ HDR->lang ^= 1; cls(); }
      else if(sel == 5){ HDR->pal ^= 1; setPalette(); }
    }
  }
}

void main(void){
  uint8_t idle = 0;
  DISPLAY_OFF;
  initStorage();
  setPalette();
  set_sprite_data(DISC_TILE0, 64, disc_tiles);
  SPRITES_8x8; SHOW_BKG; SHOW_SPRITES; DISPLAY_ON;
  keys = 0;
  drawStatic(); showDisc(1);
  while(1){
    tick();
    if(pressed & J_A){ doPick(); }
    if(pressed & J_B){ HDR->mode ^= 1; drawHeader(); drawPick(pickIdx); }
    if(pressed & J_SELECT){ HDR->filter = (HDR->filter + 1) % 5; drawHeader(); }
    if(pressed & J_START){ showDisc(0); menuScreen(); drawStatic(); showDisc(1); }
    if(++idle >= 8){ idle = 0; spinDisc(2); }
  }
}
