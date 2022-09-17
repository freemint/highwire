/* @(#)highwire/en_atari.h
 *
 * Translation Tables for ATARI System Encoded Fonts (*.FNT).
 *
*/


/*==============================================================================
 * Unicode
 *
 * With Style-Fonts and Atari standard TORG 105, U+20AC EURO SIGN is 236.  With
 * Milan TOS it is 222.  Here I used the "element of" symbol.
 * Rainer Seitel, 2002-03
 *
 * Format: {Unicode, zero to five Atari system font characters, 0}, 0
 * *3.2* before: new in Unicode 3.2
 *               http://www.unicode.org/unicode/reports/tr28/
 *               http://www.unicode.org/charts/PDF/U32-????.pdf
 * *HW* before: HighWire private use character
 * *�* before: not the correct glyph, but recognizable
 */
static const WCHAR ATARI_MULTI_0x0085[] = {'.','.','.',0};
static const WCHAR ATARI_MULTI_0x0089[] = {'�','/','o','o',0};
static const WCHAR ATARI_MULTI_0x00BE[] = {' ','3','/','4',0};
static const WCHAR ATARI_MULTI_0x09F3[] = {'I','N','R',0}; /*�*/
static const WCHAR ATARI_MULTI_0x0E3F[] = {'T','H','B',0}; /*�*/
static const WCHAR ATARI_MULTI_0x17DB[] = {'K','H','R',0}; /*�*/
static const WCHAR ATARI_MULTI_0x2026[] = {'.','.','.',0};
static const WCHAR ATARI_MULTI_0x2030[] = {'�','/','o','o',0}; /*�*/
static const WCHAR ATARI_MULTI_0x20A7[] = {'P','t','s',0};
static const WCHAR ATARI_MULTI_0x20AA[] = {'I','L','S',0}; /*�*/
static const WCHAR ATARI_MULTI_0x2100[] = {'a','/','c',0};
static const WCHAR ATARI_MULTI_0x2101[] = {'a','/','s',0};
static const WCHAR ATARI_MULTI_0x2105[] = {'c','/','o',0};
static const WCHAR ATARI_MULTI_0x2106[] = {'c','/','u',0};
static const WCHAR ATARI_MULTI_0x2295[] = {'(','+',')',0}; /*�*/
static const WCHAR ATARI_MULTI_0x2297[] = {'(','x',')',0}; /*�*/
static const WCHAR ATARI_MULTI_0x2299[] = {'(','�',')',0}; /*�*/
static const WCHAR ATARI_MULTI_0x22EF[] = {'�','�','.',0};
static const WCHAR ATARI_MULTI_0x2680[] = {'[','�',']',0};/*3.2�*/
static const WCHAR ATARI_MULTI_0x2681[] = {'[','�','�',']',0}; /*3.2�*/
static const WCHAR ATARI_MULTI_0x2682[] = {'[','�','�','�',']',0}; /*3.2�*/
static const WCHAR ATARI_MULTI_0x2683[] = {'[',':',':',']',0}; /*3.2�*/
static const WCHAR ATARI_MULTI_0x2684[] = {'[',':','�',':',']',0}; /*3.2�*/
static const WCHAR ATARI_MULTI_0x2685[] = {'[',':',':',':',']',0}; /*3.2�*/
static const WCHAR ATARI_MULTI_0xFB03[] = {'f','f','i',0};
static const WCHAR ATARI_MULTI_0xFB04[] = {'f','f','l',0};
#define SMALL(u,t,x) {u, t,     ((long)x +0) <<16}
#define MULTI(u)     {u, 0xFFFF, (long)ATARI_MULTI_##u}

static const struct s_uni_trans Unicode_to_Atari[] = {
SMALL(/*�*/0x80,'�',), SMALL(0x82,'\'',), SMALL(0x83,'�',), SMALL(0x84,'"',), MULTI(0x0085), 
SMALL(0x86,'�',), SMALL(/*�*/0x87,'�',), SMALL(0x88,'^',), MULTI(0x0089), 
SMALL(/*�*/0x8A,'S',), SMALL(/*�*/0x8B,'<',), SMALL(0x8C,'�',), SMALL(/*�*/0x8E,'Z',), SMALL(0x91,'\'',), 
SMALL(0x92,'\'',), SMALL(0x93,'"',), SMALL(0x94,'"',), SMALL(0x95,'�',), SMALL(0x96,'-',), SMALL(0x97,'-','-'), 
SMALL(/*�*/0x98,'~',), SMALL(0x99,'�',), SMALL(/*�*/0x9A,'s',), SMALL(/*�*/0x9B,'>',), SMALL(0x9C,'�',), 
SMALL(/*�*/0x9E,'z',), SMALL(/*�*/0x9F,'Y',), 
SMALL(0x00A0,NOBRK_ATARI,), SMALL(0x00A1,'�',), SMALL(0x00A2,'�',), SMALL(0x00A3,'�',), SMALL(0x00A5,'�',), 
SMALL(/*�*/0x00A6,17,), SMALL(0x00A7,'�',), SMALL(0x00A8,'�',), SMALL(0x00A9,'�',), SMALL(0x00AA,'�',), 
SMALL(0x00AB,'�',), SMALL(0x00AC,'�',), SMALL(0x00AD,'-',), SMALL(0x00AE,'�',), SMALL(0x00AF,'�',), 
SMALL(0x00B0,'�',), SMALL(0x00B1,'�',), SMALL(0x00B2,'�',), SMALL(0x00B3,'�',), SMALL(0x00B4,'�',), 
SMALL(0x00B5,'�',), SMALL(0x00B6,'�',), SMALL(0x00B7,'�',), SMALL(/*�*/0x00B9,'\'',), SMALL(0x00BA,'�',), 
SMALL(0x00BB,'�',), SMALL(0x00BC,'�',), SMALL(0x00BD,'�',), MULTI(0x00BE), 
SMALL(0x00BF,'�',), SMALL(0x00C0,'�',), SMALL(/*�*/0x00C1,'A',), SMALL(/*�*/0x00C2,'A',), SMALL(0x00C3,'�',), 
SMALL(0x00C4,'�',), SMALL(0x00C5,'�',), SMALL(0x00C6,'�',), SMALL(0x00C7,'�',), SMALL(/*�*/0x00C8,'E',), 
SMALL(0x00C9,'�',), SMALL(/*�*/0x00CA,'E',), SMALL(/*�*/0x00CB,'E',), SMALL(/*�*/0x00CC,'I',), 
SMALL(/*�*/0x00CD,'I',), SMALL(/*�*/0x00CE,'I',), SMALL(/*�*/0x00CF,'I',), SMALL(/*�*/0x00D0,'D',), 
SMALL(0x00D1,'�',), SMALL(/*�*/0x00D2,'O',), SMALL(/*�*/0x00D3,'O',), SMALL(/*�*/0x00D4,'O',), 
SMALL(0x00D5,'�',), SMALL(0x00D6,'�',), SMALL(/*�*/0x00D7,'x',), SMALL(0x00D8,'�',), SMALL(/*�*/0x00D9,'U',), 
SMALL(/*�*/0x00DA,'U',), SMALL(/*�*/0x00DB,'U',), SMALL(0x00DC,'�',), SMALL(/*�*/0x00DD,'Y',), 
SMALL(0x00DF,'�',), SMALL(0x00E0,'�',), SMALL(0x00E1,'�',), SMALL(0x00E2,'�',), SMALL(0x00E3,'�',), 
SMALL(0x00E4,'�',), SMALL(0x00E5,'�',), SMALL(0x00E6,'�',), SMALL(0x00E7,'�',), SMALL(0x00E8,'�',), 
SMALL(0x00E9,'�',), SMALL(0x00EA,'�',), SMALL(0x00EB,'�',), SMALL(0x00EC,'�',), SMALL(0x00ED,'�',), 
SMALL(0x00EE,'�',), SMALL(0x00EF,'�',), SMALL(/*�*/0x00F0,'�',), SMALL(0x00F1,'�',), SMALL(0x00F2,'�',), 
SMALL(0x00F3,'�',), SMALL(0x00F4,'�',), SMALL(0x00F5,'�',), SMALL(0x00F6,'�',), SMALL(0x00F7,'�',), 
SMALL(0x00F8,'�',), SMALL(0x00F9,'�',), SMALL(0x00FA,'�',), SMALL(0x00FB,'�',), SMALL(0x00FC,'�',), 
SMALL(/*�*/0x00FD,'y',), SMALL(0x00FF,'�',), SMALL(/*�*/0x0108,'C','h'), SMALL(/*�*/0x0109,'c','h'), 
SMALL(0x010C,'C',), SMALL(0x010D,'c',), SMALL(0x010F,'d','\''), SMALL(/*�*/0x011C,'G','h'), 
SMALL(/*�*/0x011D,'g','h'), SMALL(/*�*/0x0124,'H','h'), SMALL(/*�*/0x0125,'h','h'), 
SMALL(0x0132,'�',), SMALL(0x0133,'�',), SMALL(/*�*/0x0134,'J','h'), SMALL(/*�*/0x0135,'j','h'), 
SMALL(0x013E,'l','\''), SMALL(0x013F,'L','�'), SMALL(0x0140,'l','�'), SMALL(0x0149,'\'','n'), 
SMALL(0x0152,'�',), SMALL(0x0153,'�',), SMALL(/*�*/0x015C,'S','h'), SMALL(/*�*/0x015D,'s','h'), 
SMALL(0x0160,'S',), SMALL(0x0161,'s',), SMALL(/*�*/0x016D,'u',), SMALL(0x0178,'Y',), SMALL(0x017D,'Z',), 
SMALL(0x017E,'z',), SMALL(/*�*/0x017F,'�',), SMALL(0x01C0,'|',), SMALL(0x01C1,'|','|'), SMALL(0x01C3,'!',), 
SMALL(0x01C7,'L','J'), SMALL(0x01C8,'L','j'), SMALL(0x01C9,'l','j'), SMALL(0x01CA,'N','J'), 
SMALL(0x01CB,'N','j'), SMALL(0x01CC,'n','j'), SMALL(0x01DD,26,), SMALL(0x0259,26,), SMALL(0x02A3,'d','z'), 
SMALL(0x02A6,'t','s'), SMALL(0x02AA,'l','s'), SMALL(0x02AB,'l','z'), SMALL(0x02B9,'\'',), 
SMALL(0x02BA,'"',), SMALL(0x02BC,'\'',), SMALL(0x02C2,'<',), SMALL(0x02C3,'>',), SMALL(0x02C6,'^',), 
SMALL(0x02C8,'\'',), SMALL(0x02C9,'�',), SMALL(0x02CA,'�',), SMALL(0x02CB,'`',), SMALL(0x02CD,'_',), 
SMALL(0x02D0,':',), SMALL(0x02D1,'�',), SMALL(0x02D6,'+',), SMALL(0x02D7,'-',), SMALL(0x02DA,'�',), 
SMALL(0x02DC,'~',), SMALL(0x02EE,'"',), SMALL(/*3.2*/0x034F,0,), SMALL(0x0374,'�',), SMALL(0x037E,';',), 
SMALL(0x0391,'A',), SMALL(0x0392,'B',), SMALL(0x0393,'�',), SMALL(0x0394,127,), SMALL(0x0395,'E',), 
SMALL(0x0396,'Z',), SMALL(0x0397,'H',), SMALL(0x0398,'�',), SMALL(0x0399,'I',), SMALL(0x039A,'K',), 
SMALL(0x039C,'M',), SMALL(0x039D,'N',), SMALL(0x039E,'�',), SMALL(0x039F,'O',), SMALL(0x03A0,'�',), 
SMALL(0x03A1,'P',), SMALL(0x03A3,'�',), SMALL(0x03A4,'T',), SMALL(0x03A5,'Y',), SMALL(0x03A6,'�',), 
SMALL(0x03A7,'X',), SMALL(0x03A9,'�',), SMALL(0x03B1,'�',), SMALL(0x03B2,'�',), SMALL(0x03B4,'�',), 
SMALL(0x03B5,'�',), SMALL(0x03BC,'�',), SMALL(0x03BD,'v',), SMALL(0x03BF,'o',), SMALL(0x03C0,'�',), 
SMALL(0x03C1,'p',), SMALL(0x03C2,'s',), SMALL(0x03C3,'�',), SMALL(0x03C4,'�',), SMALL(0x03C6,'�',), 
SMALL(0x03D5,'�',), SMALL(0x03DC,'F',), SMALL(0x03F2,'c',), SMALL(0x03F3,'j',), SMALL(0x03F4,'�',), 
SMALL(0x03F5,'�',), SMALL(0x05BE,'�',), SMALL(0x05C0,'|',), SMALL(0x05C3,':',), SMALL(0x05D0,'�',), 
SMALL(0x05D1,'�',), SMALL(0x05D2,'�',), SMALL(0x05D3,'�',), SMALL(0x05D4,'�',), SMALL(0x05D5,'�',), 
SMALL(0x05D6,'�',), SMALL(0x05D7,'�',), SMALL(0x05D8,'�',), SMALL(0x05D9,'�',), SMALL(0x05DA,'�',), 
SMALL(0x05DB,'�',), SMALL(0x05DC,'�',), SMALL(0x05DD,'�',), SMALL(0x05DE,'�',), SMALL(0x05DF,'�',), 
SMALL(0x05E0,'�',), SMALL(0x05E1,'�',), SMALL(0x05E2,'�',), SMALL(0x05E3,'�',), SMALL(0x05E4,'�',), 
SMALL(0x05E5,'�',), SMALL(0x05E6,'�',), SMALL(0x05E7,'�',), SMALL(0x05E8,'�',), SMALL(0x05E9,'�',), 
SMALL(0x05EA,'�',), SMALL(0x05F0,'�','�'), SMALL(0x05F1,'�','�'), SMALL(0x05F2,'�','�'), 
SMALL(0x05F3,'\'',), SMALL(0x05F4,'"',), SMALL(0x0660,'0',), SMALL(0x0661,'1',), SMALL(0x0662,'2',), 
SMALL(0x0663,'3',), SMALL(0x0664,'4',), SMALL(0x0665,'5',), SMALL(0x0666,'6',), SMALL(0x0667,'7',), 
SMALL(0x0668,'8',), SMALL(0x0669,'9',), SMALL(0x066A,'%',), SMALL(0x066B,',',), SMALL(0x066C,'\'',), 
SMALL(0x066D,'*',), SMALL(0x070F,0,), MULTI(/*�*/0x09F3), MULTI(/*�*/0x0E3F), 
SMALL(0x10FB,':','�'), MULTI(/*�*/0x17DB), SMALL(0x180B,0,), SMALL(0x180C,0,), SMALL(0x180D,0,), 
SMALL(0x180E,0,), 
SMALL(0x2000,' ',), SMALL(0x2001,' ',' '), SMALL(0x2002,' ',), SMALL(0x2003,' ',' '), SMALL(0x2004,' ',), 
SMALL(0x2005,' ',), SMALL(0x2006,' ',), SMALL(0x2007,' ',), SMALL(0x2008,' ',), SMALL(0x2009,' ',), SMALL(0x200A,0,), 
SMALL(0x200B,0,), SMALL(0x200C,0,), SMALL(0x200D,0,), SMALL(0x2010,'-',), SMALL(0x2011,'-',), SMALL(0x2012,'-',), 
SMALL(0x2013,'-',), SMALL(0x2014,'-','-'), SMALL(0x2015,'-','-'), SMALL(0x2016,'|','|'), 
SMALL(0x2018,'\'',), SMALL(0x2019,'\'',), SMALL(0x201A,'\'',), SMALL(0x201B,'\'',), SMALL(0x201C,'"',), 
SMALL(0x201D,'"',), SMALL(0x201E,'"',), SMALL(0x201F,'"',), SMALL(0x2020,'�',), SMALL(0x2021,'�',), 
SMALL(0x2022,'�',), SMALL(/*�*/0x2023,'�',), SMALL(0x2024,'.',), SMALL(0x2025,'.','.'), 
MULTI(0x2026), SMALL(0x2027,'�',), SMALL(0x202F,' ',), 
MULTI(/*�*/0x2030), SMALL(0x2032,'\'',), SMALL(0x2333,'"',), SMALL(0x2039,'<',), 
SMALL(0x203A,'>',), SMALL(0x203C,'!','!'), SMALL(0x203E,'�',), SMALL(0x2043,'-',), SMALL(0x2044,'/',), 
SMALL(/*3.2*/0x2047,'?','?'), SMALL(/*�*/0x204A,'7',), SMALL(0x204C,'�',), SMALL(0x204D,'�',), 
SMALL(/*3.2*/0x204E,'*',), SMALL(/*3.2*/0x2052,'%',), SMALL(/*3.2*/0x2057,'"','"'), 
SMALL(/*3.2*/0x205F,' ',), SMALL(/*3.2*/0x2060,0,), SMALL(/*3.2*/0x2061,0,), SMALL(/*3.2*/0x2062,' ',), 
SMALL(/*3.2*/0x2063,' ',), SMALL(0x207F,'�',), SMALL(/*�*/0x20A4,'�',), MULTI(0x20A7), 
SMALL(0x20A8,'R','s'), MULTI(/*�*/0x20AA), SMALL(/*�*/0x20AC,'�',), 
SMALL(/*3.2�*/0x20B0,'P','f'), MULTI(0x2100), MULTI(0x2101), 
SMALL(0x2103,'�','C'), MULTI(0x2105), MULTI(0x2106), SMALL(0x2109,'�','F'), 
SMALL(0x2113,'l',), SMALL(/*�*/0x2114,'l','b'), SMALL(0x2116,'N','�'), SMALL(0x2122,'�',), 
SMALL(0x2126,'�',), SMALL(/*�*/0x2127,'S',), SMALL(0x212A,'K',), SMALL(0x212B,'�',), SMALL(/*�*/0x212E,'e',), 
SMALL(/*�*/0x2133,'M',), SMALL(0x2135,'�',), SMALL(0x2136,'�',), SMALL(0x2137,'�',), SMALL(0x2138,'�',), 
SMALL(0x2139,'i',), SMALL(0x2180,'�',), SMALL(0x2190,4,), SMALL(0x2191,1,), SMALL(0x2192,3,), SMALL(0x2193,2,), 
SMALL(0x21E6,4,), SMALL(0x21E7,1,), SMALL(0x21E8,3,), SMALL(0x21E9,2,), SMALL(0x2205,'�',), SMALL(0x2206,127,), 
SMALL(0x2208,'�',), SMALL(/*�*/0x220A,'�',), SMALL(0x2211,'�',), SMALL(0x2212,'-',), SMALL(0x2215,'/',), 
SMALL(0x2216,'\\',), SMALL(0x2217,'*',), SMALL(0x2219,'�',), SMALL(0x221A,'�',), SMALL(0x221D,'�',), 
SMALL(0x221E,'�',), SMALL(0x2223,'|',), SMALL(0x2225,'|','|'), SMALL(0x2227,'�',), SMALL(0x2229,'�',), 
SMALL(0x222A,'U',), SMALL(0x222E,'�',), SMALL(0x2236,':',), SMALL(0x223C,'~',), SMALL(/*�*/0x223F,'~',), 
SMALL(0x2248,'�',), SMALL(0x2261,'�',), SMALL(0x2264,'�',), SMALL(0x2265,'�',), 
MULTI(/*�*/0x2295), MULTI(/*�*/0x2297), 
MULTI(/*�*/0x2299), SMALL(0x22BA,'T',), SMALL(0x22C0,'�',), SMALL(0x22C2,'�',), 
SMALL(0x22C3,'U',), SMALL(0x22C5,'�',), MULTI(0x22EF), SMALL(/*3.2*/0x22FF,'E',), 
SMALL(0x2310,'�',), SMALL(0x2314,8,), SMALL(0x231A,9,), SMALL(0x2320,'�',), SMALL(0x2321,'�',), SMALL(0x2329,'<',), 
SMALL(0x232A,'>',), SMALL(/*3.2*/0x23B7,'�',), SMALL(0x240C,12,), SMALL(0x240D,13,), SMALL(0x241B,27,), 
SMALL(0x266A,11,), MULTI(/*3.2�*/0x2680), MULTI(/*3.2�*/0x2681), 
MULTI(/*3.2�*/0x2682), MULTI(/*3.2�*/0x2683), 
MULTI(/*3.2�*/0x2684), MULTI(/*3.2�*/0x2685), 
SMALL(0x2713,8,), SMALL(/*�*/0x2714,8,), SMALL(/*3.2�*/0x27E8,'<',), SMALL(/*3.2�*/0x27E9,'>',), 
SMALL(/*3.2*/0x29F2,'�',), SMALL(/*3.2*/0x2A0D,'�',), SMALL(/*3.2�*/0x2A2F,'x',), 
SMALL(0x3000,' ',' '), 
SMALL(0x301D,'"',), SMALL(0x301E,'"',), SMALL(0x301F,'"',), 
SMALL(/*HW*/0xF8F0,'j',), SMALL(0xFB00,'f','f'), SMALL(0xFB01,'f','i'), 
SMALL(0xFB02,'f','l'), MULTI(0xFB03), MULTI(0xFB04), 
SMALL(/*�*/0xFB05,'�','t'), SMALL(0xFB06,'s','t'), SMALL(0xFB20,'�',), SMALL(0xFB21,'�',), 
SMALL(0xFB22,'�',), SMALL(0xFB23,'�',), SMALL(0xFB24,'�',), SMALL(0xFB25,'�',), SMALL(0xFB26,'�',), 
SMALL(0xFB27,'�',), SMALL(0xFB28,'�',), SMALL(/*�*/0xFB29,'+',), SMALL(/*�*/0xFB4F,'�','�'), 
SMALL(/*�*/0xFD3E,'(',), SMALL(/*�*/0xFD3F,')',), SMALL(/*3.2*/0xFE00,0,), SMALL(/*3.2*/0xFE01,0,), 
SMALL(/*3.2*/0xFE02,0,), SMALL(/*3.2*/0xFE03,0,), SMALL(/*3.2*/0xFE04,0,), SMALL(/*3.2*/0xFE05,0,), 
SMALL(/*3.2*/0xFE06,0,), SMALL(/*3.2*/0xFE07,0,), SMALL(/*3.2*/0xFE08,0,), SMALL(/*3.2*/0xFE09,0,), 
SMALL(/*3.2*/0xFE0A,0,), SMALL(/*3.2*/0xFE0B,0,), SMALL(/*3.2*/0xFE0C,0,), SMALL(/*3.2*/0xFE0D,0,), 
SMALL(/*3.2*/0xFE0E,0,), SMALL(/*3.2*/0xFE0F,0,), SMALL(0xFEFF,0,), 
SMALL(/*This_MUST_be_the_last_entry!*/0xFFFD,6,) };
#undef SMALL
#undef MULTI
