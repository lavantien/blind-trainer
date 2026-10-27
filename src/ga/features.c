#include "ga/features.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <string.h>

static const bt_ga_tone_marked_row BT_GA_TONE_MARKED[60] = {
    {"\xc3\xa0", "a", BT_GA_TONE_HUYEN},
    {"\xc3\xa1", "a", BT_GA_TONE_SAC},
    {"\xe1\xba\xa3", "a", BT_GA_TONE_HOI},
    {"\xc3\xa3", "a", BT_GA_TONE_NGA},
    {"\xe1\xba\xa1", "a", BT_GA_TONE_NANG},
    {"\xc3\xa8", "e", BT_GA_TONE_HUYEN},
    {"\xc3\xa9", "e", BT_GA_TONE_SAC},
    {"\xe1\xba\xbb", "e", BT_GA_TONE_HOI},
    {"\xe1\xba\xbd", "e", BT_GA_TONE_NGA},
    {"\xe1\xba\xb9", "e", BT_GA_TONE_NANG},
    {"\xc3\xac", "i", BT_GA_TONE_HUYEN},
    {"\xc3\xad", "i", BT_GA_TONE_SAC},
    {"\xe1\xbb\x89", "i", BT_GA_TONE_HOI},
    {"\xc4\xa9", "i", BT_GA_TONE_NGA},
    {"\xe1\xbb\x8b", "i", BT_GA_TONE_NANG},
    {"\xc3\xb2", "o", BT_GA_TONE_HUYEN},
    {"\xc3\xb3", "o", BT_GA_TONE_SAC},
    {"\xe1\xbb\x8f", "o", BT_GA_TONE_HOI},
    {"\xc3\xb5", "o", BT_GA_TONE_NGA},
    {"\xe1\xbb\x8d", "o", BT_GA_TONE_NANG},
    {"\xc3\xb9", "u", BT_GA_TONE_HUYEN},
    {"\xc3\xba", "u", BT_GA_TONE_SAC},
    {"\xe1\xbb\xa7", "u", BT_GA_TONE_HOI},
    {"\xc5\xa9", "u", BT_GA_TONE_NGA},
    {"\xe1\xbb\xa5", "u", BT_GA_TONE_NANG},
    {"\xe1\xbb\xb3", "y", BT_GA_TONE_HUYEN},
    {"\xc3\xbd", "y", BT_GA_TONE_SAC},
    {"\xe1\xbb\xb7", "y", BT_GA_TONE_HOI},
    {"\xe1\xbb\xb9", "y", BT_GA_TONE_NGA},
    {"\xe1\xbb\xb5", "y", BT_GA_TONE_NANG},
    {"\xe1\xba\xb1", "\xc4\x83", BT_GA_TONE_HUYEN},
    {"\xe1\xba\xaf", "\xc4\x83", BT_GA_TONE_SAC},
    {"\xe1\xba\xb3", "\xc4\x83", BT_GA_TONE_HOI},
    {"\xe1\xba\xb5", "\xc4\x83", BT_GA_TONE_NGA},
    {"\xe1\xba\xb7", "\xc4\x83", BT_GA_TONE_NANG},
    {"\xe1\xba\xa7", "\xc3\xa2", BT_GA_TONE_HUYEN},
    {"\xe1\xba\xa5", "\xc3\xa2", BT_GA_TONE_SAC},
    {"\xe1\xba\xa9", "\xc3\xa2", BT_GA_TONE_HOI},
    {"\xe1\xba\xab", "\xc3\xa2", BT_GA_TONE_NGA},
    {"\xe1\xba\xad", "\xc3\xa2", BT_GA_TONE_NANG},
    {"\xe1\xbb\x81", "\xc3\xaa", BT_GA_TONE_HUYEN},
    {"\xe1\xba\xbf", "\xc3\xaa", BT_GA_TONE_SAC},
    {"\xe1\xbb\x83", "\xc3\xaa", BT_GA_TONE_HOI},
    {"\xe1\xbb\x85", "\xc3\xaa", BT_GA_TONE_NGA},
    {"\xe1\xbb\x87", "\xc3\xaa", BT_GA_TONE_NANG},
    {"\xe1\xbb\x93", "\xc3\xb4", BT_GA_TONE_HUYEN},
    {"\xe1\xbb\x91", "\xc3\xb4", BT_GA_TONE_SAC},
    {"\xe1\xbb\x95", "\xc3\xb4", BT_GA_TONE_HOI},
    {"\xe1\xbb\x97", "\xc3\xb4", BT_GA_TONE_NGA},
    {"\xe1\xbb\x99", "\xc3\xb4", BT_GA_TONE_NANG},
    {"\xe1\xbb\x9d", "\xc6\xa1", BT_GA_TONE_HUYEN},
    {"\xe1\xbb\x9b", "\xc6\xa1", BT_GA_TONE_SAC},
    {"\xe1\xbb\x9f", "\xc6\xa1", BT_GA_TONE_HOI},
    {"\xe1\xbb\xa1", "\xc6\xa1", BT_GA_TONE_NGA},
    {"\xe1\xbb\xa3", "\xc6\xa1", BT_GA_TONE_NANG},
    {"\xe1\xbb\xab", "\xc6\xb0", BT_GA_TONE_HUYEN},
    {"\xe1\xbb\xa9", "\xc6\xb0", BT_GA_TONE_SAC},
    {"\xe1\xbb\xad", "\xc6\xb0", BT_GA_TONE_HOI},
    {"\xe1\xbb\xaf", "\xc6\xb0", BT_GA_TONE_NGA},
    {"\xe1\xbb\xb1", "\xc6\xb0", BT_GA_TONE_NANG},
};

typedef struct {
    const char *spelling;
    bt_ga_place place;
    bt_ga_manner manner;
} bt_ga_onset_spelling;

static const bt_ga_onset_spelling BT_GA_ONSET_SPELLINGS[25] = {
    {"ngh", BT_GA_PLACE_VELAR, BT_GA_MANNER_NASAL},
    {"ch", BT_GA_PLACE_PALATAL, BT_GA_MANNER_AFFRICATE},
    {"gh", BT_GA_PLACE_VELAR, BT_GA_MANNER_FRICATIVE},
    {"gi", BT_GA_PLACE_DENTAL, BT_GA_MANNER_FRICATIVE},
    {"kh", BT_GA_PLACE_VELAR, BT_GA_MANNER_FRICATIVE},
    {"nh", BT_GA_PLACE_PALATAL, BT_GA_MANNER_NASAL},
    {"ng", BT_GA_PLACE_VELAR, BT_GA_MANNER_NASAL},
    {"ph", BT_GA_PLACE_LABIAL, BT_GA_MANNER_FRICATIVE},
    {"th", BT_GA_PLACE_DENTAL, BT_GA_MANNER_ASPIRATED},
    {"tr", BT_GA_PLACE_RETROFLEX, BT_GA_MANNER_AFFRICATE},
    {"b", BT_GA_PLACE_LABIAL, BT_GA_MANNER_IMPLOSIVE},
    {"c", BT_GA_PLACE_VELAR, BT_GA_MANNER_STOP},
    {"d", BT_GA_PLACE_DENTAL, BT_GA_MANNER_FRICATIVE},
    {"\xc4\x91", BT_GA_PLACE_DENTAL, BT_GA_MANNER_IMPLOSIVE},
    {"g", BT_GA_PLACE_VELAR, BT_GA_MANNER_FRICATIVE},
    {"h", BT_GA_PLACE_GLOTTAL, BT_GA_MANNER_FRICATIVE},
    {"k", BT_GA_PLACE_VELAR, BT_GA_MANNER_STOP},
    {"l", BT_GA_PLACE_DENTAL, BT_GA_MANNER_APPROXIMANT},
    {"m", BT_GA_PLACE_LABIAL, BT_GA_MANNER_NASAL},
    {"n", BT_GA_PLACE_DENTAL, BT_GA_MANNER_NASAL},
    {"r", BT_GA_PLACE_DENTAL, BT_GA_MANNER_FRICATIVE},
    {"s", BT_GA_PLACE_RETROFLEX, BT_GA_MANNER_FRICATIVE},
    {"t", BT_GA_PLACE_DENTAL, BT_GA_MANNER_STOP},
    {"v", BT_GA_PLACE_LABIAL, BT_GA_MANNER_FRICATIVE},
    {"x", BT_GA_PLACE_DENTAL, BT_GA_MANNER_FRICATIVE},
};

static const bt_ga_rime_row BT_GA_RIMES[95] = {
    {"i", BT_GA_NUC_I, BT_GA_CODA_OPEN},
    {"y", BT_GA_NUC_I, BT_GA_CODA_OPEN},
    {"\xc6\xb0", BT_GA_NUC_UW, BT_GA_CODA_OPEN},
    {"u", BT_GA_NUC_U, BT_GA_CODA_OPEN},
    {"\xc3\xaa", BT_GA_NUC_EH, BT_GA_CODA_OPEN},
    {"\xc6\xa1", BT_GA_NUC_OR, BT_GA_CODA_OPEN},
    {"\xc3\xb4", BT_GA_NUC_OH, BT_GA_CODA_OPEN},
    {"e", BT_GA_NUC_E, BT_GA_CODA_OPEN},
    {"a", BT_GA_NUC_A, BT_GA_CODA_OPEN},
    {"o", BT_GA_NUC_O, BT_GA_CODA_OPEN},
    {"ia", BT_GA_NUC_IA, BT_GA_CODA_OPEN},
    {"ya", BT_GA_NUC_IA, BT_GA_CODA_OPEN},
    {"\xc6\xb0"
     "a",
     BT_GA_NUC_WA, BT_GA_CODA_OPEN},
    {"ua", BT_GA_NUC_UA, BT_GA_CODA_OPEN},
    {"im", BT_GA_NUC_I, BT_GA_CODA_M},
    {"ym", BT_GA_NUC_I, BT_GA_CODA_M},
    {"um", BT_GA_NUC_U, BT_GA_CODA_M},
    {"\xc3\xaam", BT_GA_NUC_EH, BT_GA_CODA_M},
    {"\xc6\xa1m", BT_GA_NUC_OR, BT_GA_CODA_M},
    {"\xc3\xb4m", BT_GA_NUC_OH, BT_GA_CODA_M},
    {"em", BT_GA_NUC_E, BT_GA_CODA_M},
    {"am", BT_GA_NUC_A, BT_GA_CODA_M},
    {"\xc3\xa2m", BT_GA_NUC_AH, BT_GA_CODA_M},
    {"\xc4\x83m", BT_GA_NUC_AB, BT_GA_CODA_M},
    {"om", BT_GA_NUC_O, BT_GA_CODA_M},
    {"i\xc3\xaam", BT_GA_NUC_IA, BT_GA_CODA_M},
    {"\xc6\xb0\xc6\xa1m", BT_GA_NUC_WA, BT_GA_CODA_M},
    {"u\xc3\xb4m", BT_GA_NUC_UA, BT_GA_CODA_M},
    {"in", BT_GA_NUC_I, BT_GA_CODA_N},
    {"un", BT_GA_NUC_U, BT_GA_CODA_N},
    {"\xc3\xaan", BT_GA_NUC_EH, BT_GA_CODA_N},
    {"\xc6\xa1n", BT_GA_NUC_OR, BT_GA_CODA_N},
    {"\xc3\xb4n", BT_GA_NUC_OH, BT_GA_CODA_N},
    {"en", BT_GA_NUC_E, BT_GA_CODA_N},
    {"an", BT_GA_NUC_A, BT_GA_CODA_N},
    {"\xc3\xa2n", BT_GA_NUC_AH, BT_GA_CODA_N},
    {"\xc4\x83n", BT_GA_NUC_AB, BT_GA_CODA_N},
    {"on", BT_GA_NUC_O, BT_GA_CODA_N},
    {"i\xc3\xaan", BT_GA_NUC_IA, BT_GA_CODA_N},
    {"\xc6\xb0\xc6\xa1n", BT_GA_NUC_WA, BT_GA_CODA_N},
    {"u\xc3\xb4n", BT_GA_NUC_UA, BT_GA_CODA_N},
    {"inh", BT_GA_NUC_I, BT_GA_CODA_NG},
    {"ynh", BT_GA_NUC_I, BT_GA_CODA_NG},
    {"ung", BT_GA_NUC_U, BT_GA_CODA_NG},
    {"\xc3\xaanh", BT_GA_NUC_EH, BT_GA_CODA_NG},
    {"\xc6\xb0ng", BT_GA_NUC_UW, BT_GA_CODA_NG},
    {"ang", BT_GA_NUC_A, BT_GA_CODA_NG},
    {"anh", BT_GA_NUC_E, BT_GA_CODA_NG},
    {"\xc3\xa2ng", BT_GA_NUC_AH, BT_GA_CODA_NG},
    {"\xc4\x83ng", BT_GA_NUC_AB, BT_GA_CODA_NG},
    {"ong", BT_GA_NUC_O, BT_GA_CODA_NG},
    {"\xc3\xb4ng", BT_GA_NUC_OH, BT_GA_CODA_NG},
    {"i\xc3\xaang", BT_GA_NUC_IA, BT_GA_CODA_NG},
    {"\xc6\xb0\xc6\xa1ng", BT_GA_NUC_WA, BT_GA_CODA_NG},
    {"u\xc3\xb4ng", BT_GA_NUC_UA, BT_GA_CODA_NG},
    {"ip", BT_GA_NUC_I, BT_GA_CODA_P},
    {"yp", BT_GA_NUC_I, BT_GA_CODA_P},
    {"up", BT_GA_NUC_U, BT_GA_CODA_P},
    {"\xc3\xaap", BT_GA_NUC_EH, BT_GA_CODA_P},
    {"\xc6\xa1p", BT_GA_NUC_OR, BT_GA_CODA_P},
    {"\xc3\xb4p", BT_GA_NUC_OH, BT_GA_CODA_P},
    {"ep", BT_GA_NUC_E, BT_GA_CODA_P},
    {"ap", BT_GA_NUC_A, BT_GA_CODA_P},
    {"\xc3\xa2p", BT_GA_NUC_AH, BT_GA_CODA_P},
    {"\xc4\x83p", BT_GA_NUC_AB, BT_GA_CODA_P},
    {"op", BT_GA_NUC_O, BT_GA_CODA_P},
    {"i\xc3\xaap", BT_GA_NUC_IA, BT_GA_CODA_P},
    {"\xc6\xb0\xc6\xa1p", BT_GA_NUC_WA, BT_GA_CODA_P},
    {"it", BT_GA_NUC_I, BT_GA_CODA_T},
    {"ut", BT_GA_NUC_U, BT_GA_CODA_T},
    {"\xc3\xaat", BT_GA_NUC_EH, BT_GA_CODA_T},
    {"\xc6\xa1t", BT_GA_NUC_OR, BT_GA_CODA_T},
    {"\xc3\xb4t", BT_GA_NUC_OH, BT_GA_CODA_T},
    {"et", BT_GA_NUC_E, BT_GA_CODA_T},
    {"at", BT_GA_NUC_A, BT_GA_CODA_T},
    {"\xc3\xa2t", BT_GA_NUC_AH, BT_GA_CODA_T},
    {"\xc4\x83t", BT_GA_NUC_AB, BT_GA_CODA_T},
    {"ot", BT_GA_NUC_O, BT_GA_CODA_T},
    {"i\xc3\xaat", BT_GA_NUC_IA, BT_GA_CODA_T},
    {"\xc6\xb0\xc6\xa1t", BT_GA_NUC_WA, BT_GA_CODA_T},
    {"u\xc3\xb4t", BT_GA_NUC_UA, BT_GA_CODA_T},
    {"ich", BT_GA_NUC_I, BT_GA_CODA_K},
    {"ych", BT_GA_NUC_I, BT_GA_CODA_K},
    {"uc", BT_GA_NUC_U, BT_GA_CODA_K},
    {"\xc3\xaa"
     "ch",
     BT_GA_NUC_EH, BT_GA_CODA_K},
    {"\xc6\xb0"
     "c",
     BT_GA_NUC_UW, BT_GA_CODA_K},
    {"ac", BT_GA_NUC_A, BT_GA_CODA_K},
    {"ach", BT_GA_NUC_E, BT_GA_CODA_K},
    {"\xc3\xa2"
     "c",
     BT_GA_NUC_AH, BT_GA_CODA_K},
    {"\xc4\x83"
     "c",
     BT_GA_NUC_AB, BT_GA_CODA_K},
    {"oc", BT_GA_NUC_O, BT_GA_CODA_K},
    {"\xc3\xb4"
     "c",
     BT_GA_NUC_OH, BT_GA_CODA_K},
    {"i\xc3\xaa"
     "c",
     BT_GA_NUC_IA, BT_GA_CODA_K},
    {"\xc6\xb0\xc6\xa1"
     "c",
     BT_GA_NUC_WA, BT_GA_CODA_K},
    {"u\xc3\xb4"
     "c",
     BT_GA_NUC_UA, BT_GA_CODA_K},
};

static size_t bt_ga_utf8_decode(const char *s, uint32_t *cp) {
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] < 0x80) {
        *cp = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0) {
        *cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0) {
        *cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) |
              (uint32_t)(p[2] & 0x3F);
        return 3;
    }
    if ((p[0] & 0xF8) == 0xF0) {
        *cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) |
              ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F);
        return 4;
    }
    return 0;
}

int bt_ga_utf8_valid(const char *text) {
    BT_TRACE_ENTER("ga.utf8_valid");
    int ok = 1;
    for (const unsigned char *p = (const unsigned char *)text; ok && *p != '\0';) {
        uint32_t cp = 0;
        size_t width = bt_ga_utf8_decode((const char *)p, &cp);
        if (width == 0) {
            ok = 0;
            break;
        }
        if ((width == 2 && cp < 0x80) || (width == 3 && cp < 0x800) ||
            (width == 4 && cp < 0x10000)) {
            ok = 0;
            break;
        }
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            ok = 0;
            break;
        }
        if (cp > 0x10FFFF) {
            ok = 0;
            break;
        }
        for (size_t k = 1; k < width; k++) {
            if ((p[k] & 0xC0) != 0x80) {
                ok = 0;
                break;
            }
        }
        p += width;
    }
    BT_TRACE_EXIT("ga.utf8_valid");
    return ok;
}

int bt_ga_is_nfc(const char *text) {
    BT_TRACE_ENTER("ga.is_nfc");
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0';) {
        uint32_t cp = 0;
        size_t width = bt_ga_utf8_decode((const char *)p, &cp);
        if (width == 0) {
            BT_TRACE_EXIT("ga.is_nfc");
            return 0;
        }
        if (cp >= 0x300 && cp <= 0x36F) {
            BT_TRACE_EXIT("ga.is_nfc");
            return 0;
        }
        p += width;
    }
    BT_TRACE_EXIT("ga.is_nfc");
    return 1;
}

const bt_ga_tone_marked_row *bt_ga_tone_marked_table(size_t *count) {
    BT_TRACE_ENTER("ga.tone_marked_table");
    *count = sizeof BT_GA_TONE_MARKED / sizeof BT_GA_TONE_MARKED[0];
    BT_TRACE_EXIT("ga.tone_marked_table");
    return BT_GA_TONE_MARKED;
}

const bt_ga_rime_row *bt_ga_rimes_table(size_t *count) {
    BT_TRACE_ENTER("ga.rimes_table");
    *count = sizeof BT_GA_RIMES / sizeof BT_GA_RIMES[0];
    BT_TRACE_EXIT("ga.rimes_table");
    return BT_GA_RIMES;
}

int bt_ga_onset_features_of(bt_ga_onset onset, bt_ga_onset_features *out) {
    BT_TRACE_ENTER("ga.onset_features_of");
    int found = 1;
    switch (onset) {
    case BT_GA_ONSET_B:
        out->place = BT_GA_PLACE_LABIAL;
        out->manner = BT_GA_MANNER_IMPLOSIVE;
        break;
    case BT_GA_ONSET_DD:
        out->place = BT_GA_PLACE_DENTAL;
        out->manner = BT_GA_MANNER_IMPLOSIVE;
        break;
    case BT_GA_ONSET_T:
        out->place = BT_GA_PLACE_DENTAL;
        out->manner = BT_GA_MANNER_STOP;
        break;
    case BT_GA_ONSET_TH:
        out->place = BT_GA_PLACE_DENTAL;
        out->manner = BT_GA_MANNER_ASPIRATED;
        break;
    case BT_GA_ONSET_CH:
        out->place = BT_GA_PLACE_PALATAL;
        out->manner = BT_GA_MANNER_AFFRICATE;
        break;
    case BT_GA_ONSET_TR:
        out->place = BT_GA_PLACE_RETROFLEX;
        out->manner = BT_GA_MANNER_AFFRICATE;
        break;
    case BT_GA_ONSET_K:
        out->place = BT_GA_PLACE_VELAR;
        out->manner = BT_GA_MANNER_STOP;
        break;
    case BT_GA_ONSET_M:
        out->place = BT_GA_PLACE_LABIAL;
        out->manner = BT_GA_MANNER_NASAL;
        break;
    case BT_GA_ONSET_N:
        out->place = BT_GA_PLACE_DENTAL;
        out->manner = BT_GA_MANNER_NASAL;
        break;
    case BT_GA_ONSET_NH:
        out->place = BT_GA_PLACE_PALATAL;
        out->manner = BT_GA_MANNER_NASAL;
        break;
    case BT_GA_ONSET_NG:
        out->place = BT_GA_PLACE_VELAR;
        out->manner = BT_GA_MANNER_NASAL;
        break;
    case BT_GA_ONSET_PH:
        out->place = BT_GA_PLACE_LABIAL;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_X:
        out->place = BT_GA_PLACE_DENTAL;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_S:
        out->place = BT_GA_PLACE_RETROFLEX;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_KH:
        out->place = BT_GA_PLACE_VELAR;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_H:
        out->place = BT_GA_PLACE_GLOTTAL;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_V:
        out->place = BT_GA_PLACE_LABIAL;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_Z:
        out->place = BT_GA_PLACE_DENTAL;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_G:
        out->place = BT_GA_PLACE_VELAR;
        out->manner = BT_GA_MANNER_FRICATIVE;
        break;
    case BT_GA_ONSET_L:
        out->place = BT_GA_PLACE_DENTAL;
        out->manner = BT_GA_MANNER_APPROXIMANT;
        break;
    default:
        found = 0;
        break;
    }
    BT_TRACE_EXIT("ga.onset_features_of");
    return found;
}

static const bt_ga_rime_row *bt_ga_rime_lookup(const char *letters) {
    for (size_t i = 0; i < sizeof BT_GA_RIMES / sizeof BT_GA_RIMES[0]; i++) {
        if (strcmp(BT_GA_RIMES[i].letters, letters) == 0) {
            return &BT_GA_RIMES[i];
        }
    }
    return NULL;
}

int bt_ga_parse(const char *text, bt_ga_features *out) {
    BT_TRACE_ENTER("ga.parse");
    bt_ga_tone tone = BT_GA_TONE_NGANG;
    char stripped[BT_GA_PARSE_MAX];
    size_t stripped_len = 0;
    size_t mark_at = 0;
    int has_mark = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0';) {
        uint32_t cp = 0;
        size_t width = bt_ga_utf8_decode((const char *)p, &cp);
        if (width == 0) {
            BT_TRACE_EXIT("ga.parse");
            return 0;
        }
        const bt_ga_tone_marked_row *row = NULL;
        for (size_t i = 0; i < sizeof BT_GA_TONE_MARKED / sizeof BT_GA_TONE_MARKED[0]; i++) {
            uint32_t marked_cp = 0;
            bt_ga_utf8_decode(BT_GA_TONE_MARKED[i].marked, &marked_cp);
            if (marked_cp == cp) {
                row = &BT_GA_TONE_MARKED[i];
                break;
            }
        }
        if (row != NULL) {
            if (tone != BT_GA_TONE_NGANG) {
                BT_TRACE_EXIT("ga.parse");
                return 0;
            }
            tone = row->tone;
            mark_at = stripped_len;
            has_mark = 1;
            size_t base_len = strlen(row->base);
            memcpy(stripped + stripped_len, row->base, base_len);
            stripped_len += base_len;
        } else if (cp >= 0x300 && cp <= 0x36F) {
            BT_TRACE_EXIT("ga.parse");
            return 0;
        } else {
            if (stripped_len + width >= sizeof stripped) {
                BT_TRACE_EXIT("ga.parse");
                return 0;
            }
            memcpy(stripped + stripped_len, p, width);
            stripped_len += width;
        }
        p += width;
    }
    stripped[stripped_len] = '\0';
    const bt_ga_onset_spelling *onset_match = NULL;
    size_t onset_len = 0;
    for (size_t i = 0; i < sizeof BT_GA_ONSET_SPELLINGS / sizeof BT_GA_ONSET_SPELLINGS[0]; i++) {
        size_t spelling_len = strlen(BT_GA_ONSET_SPELLINGS[i].spelling);
        if (strncmp(stripped, BT_GA_ONSET_SPELLINGS[i].spelling, spelling_len) == 0 &&
            spelling_len > onset_len) {
            onset_match = &BT_GA_ONSET_SPELLINGS[i];
            onset_len = spelling_len;
        }
    }
    const char *rime = stripped + onset_len;
    uint32_t rime_cp = 0;
    bt_ga_utf8_decode(rime, &rime_cp);
    int merged_gi = onset_len == 2 && strncmp(stripped, "gi", 2) == 0 &&
                    ((has_mark && mark_at == 1) || rime_cp == 0xEA);
    const bt_ga_rime_row *rime_row = NULL;
    if (merged_gi) {
        if (rime[0] == '\0') {
            out->nucleus = BT_GA_NUC_I;
            out->coda = BT_GA_CODA_OPEN;
            BT_TRACE_EXIT("ga.parse");
            goto done;
        }
        if (strcmp(rime, "a") == 0) {
            out->nucleus = BT_GA_NUC_IA;
            out->coda = BT_GA_CODA_OPEN;
            BT_TRACE_EXIT("ga.parse");
            goto done;
        }
        uint32_t skip_cp = 0;
        size_t skip = bt_ga_utf8_decode(rime, &skip_cp);
        char expanded[BT_GA_PARSE_MAX];
        snprintf(expanded, sizeof expanded, "i\xc3\xaa%s", rime + skip);
        rime_row = bt_ga_rime_lookup(expanded);
        if (rime_row == NULL) {
            BT_TRACE_EXIT("ga.parse");
            return 0;
        }
    } else {
        rime_row = bt_ga_rime_lookup(rime);
        if (rime_row == NULL) {
            BT_TRACE_EXIT("ga.parse");
            return 0;
        }
    }
    out->nucleus = rime_row->nucleus;
    out->coda = rime_row->coda;
done:
    out->tone = tone;
    out->has_onset = onset_match != NULL;
    if (onset_match != NULL) {
        out->onset.place = onset_match->place;
        out->onset.manner = onset_match->manner;
    } else {
        out->onset.place = 0;
        out->onset.manner = 0;
    }
    BT_TRACE_EXIT("ga.parse");
    return 1;
}

static float bt_ga_tone_confusion(bt_ga_tone a, bt_ga_tone b) {
    if (a == b) {
        return 1.0f;
    }
    if ((a == BT_GA_TONE_HOI && b == BT_GA_TONE_NGA) ||
        (a == BT_GA_TONE_NGA && b == BT_GA_TONE_HOI)) {
        return 0.70f;
    }
    if ((a == BT_GA_TONE_HUYEN && b == BT_GA_TONE_NANG) ||
        (a == BT_GA_TONE_NANG && b == BT_GA_TONE_HUYEN)) {
        return 0.60f;
    }
    if ((a == BT_GA_TONE_SAC && b == BT_GA_TONE_NGA) ||
        (a == BT_GA_TONE_NGA && b == BT_GA_TONE_SAC)) {
        return 0.35f;
    }
    if ((a == BT_GA_TONE_NGANG && b == BT_GA_TONE_HUYEN) ||
        (a == BT_GA_TONE_HUYEN && b == BT_GA_TONE_NGANG)) {
        return 0.30f;
    }
    if ((a == BT_GA_TONE_HOI && b == BT_GA_TONE_NANG) ||
        (a == BT_GA_TONE_NANG && b == BT_GA_TONE_HOI)) {
        return 0.25f;
    }
    return 0.05f;
}

static float bt_ga_nucleus_confusion(bt_ga_nucleus a, bt_ga_nucleus b) {
    if (a == b) {
        return 1.0f;
    }
    if ((a == BT_GA_NUC_EH && b == BT_GA_NUC_E) || (a == BT_GA_NUC_E && b == BT_GA_NUC_EH) ||
        (a == BT_GA_NUC_OH && b == BT_GA_NUC_O) || (a == BT_GA_NUC_O && b == BT_GA_NUC_OH) ||
        (a == BT_GA_NUC_OR && b == BT_GA_NUC_AH) || (a == BT_GA_NUC_AH && b == BT_GA_NUC_OR) ||
        (a == BT_GA_NUC_A && b == BT_GA_NUC_AB) || (a == BT_GA_NUC_AB && b == BT_GA_NUC_A)) {
        return 0.40f;
    }
    return 0.05f;
}

static int bt_ga_coda_place_of(bt_ga_coda coda) {
    switch (coda) {
    case BT_GA_CODA_P:
    case BT_GA_CODA_M:
        return BT_GA_PLACE_LABIAL;
    case BT_GA_CODA_T:
    case BT_GA_CODA_N:
        return BT_GA_PLACE_DENTAL;
    case BT_GA_CODA_K:
    case BT_GA_CODA_NG:
        return BT_GA_PLACE_VELAR;
    default:
        return -1;
    }
}

static float bt_ga_coda_confusion(bt_ga_coda a, bt_ga_coda b) {
    if (a == b) {
        return 1.0f;
    }
    if ((a == BT_GA_CODA_P && b == BT_GA_CODA_T) || (a == BT_GA_CODA_T && b == BT_GA_CODA_P)) {
        return 0.50f;
    }
    if ((a == BT_GA_CODA_T && b == BT_GA_CODA_K) || (a == BT_GA_CODA_K && b == BT_GA_CODA_T)) {
        return 0.40f;
    }
    if ((a == BT_GA_CODA_P && b == BT_GA_CODA_K) || (a == BT_GA_CODA_K && b == BT_GA_CODA_P)) {
        return 0.20f;
    }
    if ((a == BT_GA_CODA_M && b == BT_GA_CODA_N) || (a == BT_GA_CODA_N && b == BT_GA_CODA_M)) {
        return 0.30f;
    }
    if ((a == BT_GA_CODA_N && b == BT_GA_CODA_NG) || (a == BT_GA_CODA_NG && b == BT_GA_CODA_N)) {
        return 0.30f;
    }
    if ((a == BT_GA_CODA_M && b == BT_GA_CODA_NG) || (a == BT_GA_CODA_NG && b == BT_GA_CODA_M)) {
        return 0.15f;
    }
    if ((a == BT_GA_CODA_P && b == BT_GA_CODA_M) || (a == BT_GA_CODA_M && b == BT_GA_CODA_P) ||
        (a == BT_GA_CODA_T && b == BT_GA_CODA_N) || (a == BT_GA_CODA_N && b == BT_GA_CODA_T) ||
        (a == BT_GA_CODA_K && b == BT_GA_CODA_NG) || (a == BT_GA_CODA_NG && b == BT_GA_CODA_K)) {
        return 0.20f;
    }
    return 0.10f;
}

static float bt_ga_onset_confusion(const bt_ga_features *a, const bt_ga_features *b) {
    if (!a->has_onset && !b->has_onset) {
        return 1.0f;
    }
    if (a->has_onset && b->has_onset) {
        if (a->onset.place == b->onset.place && a->onset.manner == b->onset.manner) {
            return 1.0f;
        }
        if (a->onset.place == b->onset.place || a->onset.manner == b->onset.manner) {
            return 0.20f;
        }
        return 0.10f;
    }
    return 0.10f;
}

float bt_ga_confusion(const bt_ga_features *a, const bt_ga_features *b) {
    BT_TRACE_ENTER("ga.confusion");
    float value = bt_ga_tone_confusion(a->tone, b->tone) *
                  bt_ga_nucleus_confusion(a->nucleus, b->nucleus) *
                  bt_ga_coda_confusion(a->coda, b->coda) * bt_ga_onset_confusion(a, b);
    BT_TRACE_EXIT("ga.confusion");
    return value;
}

float bt_ga_flow_penalty(const bt_ga_features *a, size_t a_len, const bt_ga_features *b,
                         size_t b_len) {
    BT_TRACE_ENTER("ga.flow_penalty");
    float penalty = 0.0f;
    if (a->tone == b->tone) {
        penalty += 1.0f;
    }
    int place = bt_ga_coda_place_of(a->coda);
    if (place >= 0 && b->has_onset && (bt_ga_place)place == b->onset.place) {
        penalty += 1.0f;
    }
    size_t diff = a_len > b_len ? a_len - b_len : b_len - a_len;
    penalty += 0.25f * (float)diff;
    BT_TRACE_EXIT("ga.flow_penalty");
    return penalty;
}
