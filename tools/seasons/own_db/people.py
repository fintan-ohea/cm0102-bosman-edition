"""When two person rows are one person (duplicates.py merges them, fill.py maps an FM player to the template's row).

Words: the first and second names joined, and the common name, accent-insensitive (cmdb.norm), without the
particles of PARTICLES. Two words are the same when they are equal, or when both have 4 letters or more, start with
the same letter and differ by one letter (one changed, added or removed: "Luís" and "Luiz", "Patrascu" and
"Petrascu", "Gunnarsson" and "Gunnarson", "Iván" and "Ibán"); at most one word of a pair may differ so ("Yang Long"
and "Song Wang", "Dílson" and "Gílson" are other people).

names_agree gives the kind of agreement, or None:
- "full": the joined names are the same.
- "common": the common names are the same, have 2 words or more and are not one person's first name ("Pablo Ruiz";
  not "Paulo César" for Paulo César Teixeira and Paulo César Barroso); or the same common name of one word that is
  a word of one joined name, with the same last word ("Asier Gallego" and "Txiki Gallego", both "Txiki").
- "words": every word of the shorter joined name (2 words or more) is a word of the longer one, in any order
  ("Celso Esquivel" and "Celso Esquivel González", "Feras Abid" and "Abid Feras", "Piotr Klepczarek" and "Piotr
  Klepczarek I"), and the common names do not disagree: when both have one, the words of one are all in the other
  person's common or joined name ("Juliano" and "Juliano Spadacio"). "Guillermo Barros Schelotto" and "Gustavo
  Barros Schelotto" (twins, one birthday) do not agree.
Dates: the templates store some dates one day later than FM's (November 2007: about 1 in 7 matched players,
September 2005: about 1 in 20, with no single leap-year rule), so the dates of one person may be up to 2 days apart.
A date of 1 January or 1 February is often a placeholder (Swedish and Portuguese rows: Martin and Mårten
Andersson, 1 February 1974), so with one only "full" counts (days_apart_ok).
"""
from cmdb import norm

PARTICLES = {"da", "de", "do", "das", "dos", "del", "della", "di", "e", "y", "la", "le", "van", "von", "der", "den",
             "ten", "ter", "bin", "ben", "al", "el"}
MAX_DAYS = 2


def words(s):
    return [w for w in norm(s or "").split() if w not in PARTICLES]


def same_word(a, b, fuzzy=True):
    if a == b:
        return True
    if not fuzzy or len(a) < 4 or len(b) < 4 or a[0] != b[0] or abs(len(a) - len(b)) > 1:
        return False
    if len(a) == len(b):
        return sum(x != y for x, y in zip(a, b)) == 1
    s, t = (a, b) if len(a) < len(b) else (b, a)
    return any(t[:i] + t[i + 1:] == s for i in range(len(t)))


def within(short, long_):
    """Every word of short matches a different word of long_, with at most one word that differs by a letter."""
    left, fuzzy = list(long_), True
    for w in short:
        hit = next((i for i, v in enumerate(left) if v == w), None)
        if hit is None and fuzzy:
            hit = next((i for i, v in enumerate(left) if same_word(w, v)), None)
            fuzzy = hit is None
        if hit is None:
            return False
        left.pop(hit)
    return True


def names_agree(af, asn, ac, bf, bsn, bc):
    ja, jb = norm(af + " " + asn), norm(bf + " " + bsn)
    if ja and ja == jb:
        return "full"
    ca, cb = words(ac), words(bc)
    wa, wb = words(af + " " + asn), words(bf + " " + bsn)
    if len(ca) >= 2 and ca == cb and ca not in (words(af), words(bf)):
        return "common"
    if len(ca) == 1 and ca == cb and wa and wb and wa[-1] == wb[-1] and (ca[0] in wa or ca[0] in wb):
        return "common"
    if ca and cb and not (within(ca, cb + wb) or within(cb, ca + wa)):
        return None
    short, long_ = (wa, wb) if len(wa) <= len(wb) else (wb, wa)
    if len(short) >= 2 and within(short, long_):
        return "words"
    return None


PLACEHOLDER_DAYS = (0, 31)       # day of the year: 1 January, 1 February


def days_apart_ok(kind, days, new_year=False):
    """new_year: one of the dates is a placeholder day (PLACEHOLDER_DAYS)."""
    return days <= MAX_DAYS and (kind == "full" or not new_year)
