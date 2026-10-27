def c(v):
    bs = v.encode('utf-8')
    cur = ''
    for i, b in enumerate(bs):
        if b < 0x80 and chr(b) not in '"\\':
            cur += chr(b)
        else:
            esc = '\\x%02x' % b
            nxt = bs[i + 1] if i + 1 < len(bs) else None
            if nxt is not None and (chr(nxt) in '0123456789abcdef' or chr(nxt) in '"\\'):
                cur += esc + '" "'
            else:
                cur += esc
    return '"' + cur + '"'


tone_rows = [
    ('à', 'a', 'HUYEN'), ('á', 'a', 'SAC'), ('ả', 'a', 'HOI'), ('ã', 'a', 'NGA'),
    ('ạ', 'a', 'NANG'), ('è', 'e', 'HUYEN'), ('é', 'e', 'SAC'), ('ẻ', 'e', 'HOI'),
    ('ẽ', 'e', 'NGA'), ('ẹ', 'e', 'NANG'), ('ì', 'i', 'HUYEN'), ('í', 'i', 'SAC'),
    ('ỉ', 'i', 'HOI'), ('ĩ', 'i', 'NGA'), ('ị', 'i', 'NANG'), ('ò', 'o', 'HUYEN'),
    ('ó', 'o', 'SAC'), ('ỏ', 'o', 'HOI'), ('õ', 'o', 'NGA'), ('ọ', 'o', 'NANG'),
    ('ù', 'u', 'HUYEN'), ('ú', 'u', 'SAC'), ('ủ', 'u', 'HOI'), ('ũ', 'u', 'NGA'),
    ('ụ', 'u', 'NANG'), ('ỳ', 'y', 'HUYEN'), ('ý', 'y', 'SAC'), ('ỷ', 'y', 'HOI'),
    ('ỹ', 'y', 'NGA'), ('ỵ', 'y', 'NANG'), ('ằ', 'ă', 'HUYEN'), ('ắ', 'ă', 'SAC'),
    ('ẳ', 'ă', 'HOI'), ('ẵ', 'ă', 'NGA'), ('ặ', 'ă', 'NANG'), ('ầ', 'â', 'HUYEN'),
    ('ấ', 'â', 'SAC'), ('ẩ', 'â', 'HOI'), ('ẫ', 'â', 'NGA'), ('ậ', 'â', 'NANG'),
    ('ề', 'ê', 'HUYEN'), ('ế', 'ê', 'SAC'), ('ể', 'ê', 'HOI'), ('ễ', 'ê', 'NGA'),
    ('ệ', 'ê', 'NANG'), ('ồ', 'ô', 'HUYEN'), ('ố', 'ô', 'SAC'), ('ổ', 'ô', 'HOI'),
    ('ỗ', 'ô', 'NGA'), ('ộ', 'ô', 'NANG'), ('ờ', 'ơ', 'HUYEN'), ('ớ', 'ơ', 'SAC'),
    ('ở', 'ơ', 'HOI'), ('ỡ', 'ơ', 'NGA'), ('ợ', 'ơ', 'NANG'), ('ừ', 'ư', 'HUYEN'),
    ('ứ', 'ư', 'SAC'), ('ử', 'ư', 'HOI'), ('ữ', 'ư', 'NGA'), ('ự', 'ư', 'NANG'),
]
assert len(tone_rows) == 60
out = []
for marked, base, tone in tone_rows:
    out.append('    {%s, %s, BT_GA_TONE_%s},' % (c(marked), c(base), tone))
for i in range(0, 60, 2):
    print(' '.join(out[i:i + 2]))
print('====RIMES')
nuc = {'I': 'I', 'Uw': 'UW', 'U': 'U', 'Eh': 'EH', 'Or': 'OR', 'Oh': 'OH', 'E': 'E',
       'Ah': 'AH', 'A': 'A', 'Ab': 'AB', 'O': 'O', 'Ia': 'IA', 'Wa': 'WA', 'Ua': 'UA'}
codas = {'Open': 'OPEN', 'M': 'M', 'N': 'N', 'Ng': 'NG', 'P': 'P', 'T': 'T', 'K': 'K'}
rimes = [
    ('i', 'I', 'Open'), ('y', 'I', 'Open'), ('ư', 'Uw', 'Open'), ('u', 'U', 'Open'),
    ('ê', 'Eh', 'Open'), ('ơ', 'Or', 'Open'), ('ô', 'Oh', 'Open'), ('e', 'E', 'Open'),
    ('a', 'A', 'Open'), ('o', 'O', 'Open'), ('ia', 'Ia', 'Open'), ('ya', 'Ia', 'Open'),
    ('ưa', 'Wa', 'Open'), ('ua', 'Ua', 'Open'),
    ('im', 'I', 'M'), ('ym', 'I', 'M'), ('um', 'U', 'M'), ('êm', 'Eh', 'M'),
    ('ơm', 'Or', 'M'), ('ôm', 'Oh', 'M'), ('em', 'E', 'M'), ('am', 'A', 'M'),
    ('âm', 'Ah', 'M'), ('ăm', 'Ab', 'M'), ('om', 'O', 'M'), ('iêm', 'Ia', 'M'),
    ('ươm', 'Wa', 'M'), ('uôm', 'Ua', 'M'),
    ('in', 'I', 'N'), ('un', 'U', 'N'), ('ên', 'Eh', 'N'), ('ơn', 'Or', 'N'),
    ('ôn', 'Oh', 'N'), ('en', 'E', 'N'), ('an', 'A', 'N'), ('ân', 'Ah', 'N'),
    ('ăn', 'Ab', 'N'), ('on', 'O', 'N'), ('iên', 'Ia', 'N'), ('ươn', 'Wa', 'N'),
    ('uôn', 'Ua', 'N'),
    ('inh', 'I', 'Ng'), ('ynh', 'I', 'Ng'), ('ung', 'U', 'Ng'), ('ênh', 'Eh', 'Ng'),
    ('ưng', 'Uw', 'Ng'), ('ang', 'A', 'Ng'), ('anh', 'E', 'Ng'), ('âng', 'Ah', 'Ng'),
    ('ăng', 'Ab', 'Ng'), ('ong', 'O', 'Ng'), ('ông', 'Oh', 'Ng'), ('iêng', 'Ia', 'Ng'),
    ('ương', 'Wa', 'Ng'), ('uông', 'Ua', 'Ng'),
    ('ip', 'I', 'P'), ('yp', 'I', 'P'), ('up', 'U', 'P'), ('êp', 'Eh', 'P'),
    ('ơp', 'Or', 'P'), ('ôp', 'Oh', 'P'), ('ep', 'E', 'P'), ('ap', 'A', 'P'),
    ('âp', 'Ah', 'P'), ('ăp', 'Ab', 'P'), ('op', 'O', 'P'), ('iêp', 'Ia', 'P'),
    ('ươp', 'Wa', 'P'),
    ('it', 'I', 'T'), ('ut', 'U', 'T'), ('êt', 'Eh', 'T'), ('ơt', 'Or', 'T'),
    ('ôt', 'Oh', 'T'), ('et', 'E', 'T'), ('at', 'A', 'T'), ('ât', 'Ah', 'T'),
    ('ăt', 'Ab', 'T'), ('ot', 'O', 'T'), ('iêt', 'Ia', 'T'), ('ươt', 'Wa', 'T'),
    ('uôt', 'Ua', 'T'),
    ('ich', 'I', 'K'), ('ych', 'I', 'K'), ('uc', 'U', 'K'), ('êch', 'Eh', 'K'),
    ('ưc', 'Uw', 'K'), ('ac', 'A', 'K'), ('ach', 'E', 'K'), ('âc', 'Ah', 'K'),
    ('ăc', 'Ab', 'K'), ('oc', 'O', 'K'), ('ôc', 'Oh', 'K'), ('iêc', 'Ia', 'K'),
    ('ươc', 'Wa', 'K'), ('uôc', 'Ua', 'K'),
]
assert len(rimes) == 95, len(rimes)
rows = ['    {%s, BT_GA_NUC_%s, BT_GA_CODA_%s},' % (c(t), nuc[n], codas[cd])
        for t, n, cd in rimes]
for i in range(0, 95, 2):
    print(' '.join(rows[i:i + 2]))
