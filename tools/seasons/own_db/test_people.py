"""Cases for people.names_agree (from the duplicate scans of 21 October 2026). usage: python3 -I test_people.py"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from people import names_agree  # noqa: E402

SAME = [  # (first, second, common) twice: one person
    (("Lucimar da Silva", "Ferreira", "Lúcio"), ("Lucimar", "da Silva Ferreira", "Lúcio")),
    (("Celso", "Esquivel", ""), ("Celso", "Esquivel González", "")),
    (("Piotr", "Klepczarek", ""), ("Piotr", "Klepczarek I", "")),
    (("Feras", "Abid", ""), ("Abid", "Feras", "")),
    (("Aleksandar Sasa", "Kopunovic", ""), ("Aleksandar", "Kopunovic", "")),
    (("Juliano", "Gonçalves Spadacio", "Juliano"), ("Juliano", "Spadacio", "Juliano Spadacio")),
    (("Sebastian Cornel", "Patrascu", ""), ("Sebastian Cornel", "Petrascu", "")),
    (("Iván", "Rodríguez Núñez", "Iván"), ("Ibán", "Rodríguez Núñez", "Ibán")),
    (("Cristian Fabián", "Díaz", "Christian Díaz"), ("Christian Fabián", "Díaz", "Christian Díaz")),
    (("André Luís", "Garcia", "André Luís"), ("André Luiz", "Garcia", "Andres Luiz")),
    (("Juan Pablo", "Ruiz", "Pablo Ruiz"), ("Pablo", "Ruiz Barrero", "Pablo Ruiz")),
    (("Kabze", "Hasan", ""), ("Hasan Salih", "Kabze", "Hasan Kabze")),
    (("Octavio Darío", "Rodríguez", ""), ("Darío", "Rodríguez", "")),
    (("Asier", "Gallego", "Txiki"), ("Txiki", "Gallego", "Txiki")),
]
OTHER = [  # two people
    (("Guillermo", "Barros Schelotto", ""), ("Gustavo", "Barros Schelotto", "")),
    (("Carlos", "Pereira do Nascimento", "Carlos"), ("Jefferson", "Pereira do Nascimento", "Jefferson")),
    (("José Geraldo", "Arcanjo", "Geraldo"), ("Geraldo", "Oliveira", "Geraldo")),
    (("Willian", "Cardoso", "Willian"), ("Willian", "Pereira Leão", "Willian")),
    (("José Luis", "Peláez", ""), ("José Luis", "Díez", "Txelu")),
    (("José Manuel", "Redondo", ""), ("José Manuel", "Rodríguez Romeo", "Rodri")),
    (("Diogo", "Fonseca", "Diogo"), ("Diogo", "Vieira", "Diogo")),
    (("Erik", "Bjørnstad Hagen", ""), ("Rune", "Bjørnstad Hagen", "")),
    (("Yang", "Long", "Long Yang"), ("Song", "Wang", "Wang Song")),
    (("Dílson", "da Silva", "Dílson"), ("Gílson", "Silva de Souza", "Gílson")),
    (("Paulo César", "Teixeira", "Paulo César"), ("Paulo César", "Barroso", "Paulo César")),
    (("Fredrik", "Jansson", ""), ("Fredrik", "Hansson", "")),
    (("Leandro", "da Silva", "Leandro"), ("Leandro", "Lima Braga", "Leandro")),
    (("Antônio Marcos", "Tobias", "Toninho"), ("Antônio", "Tobias da Silva", "Toninho")),
]
bad = [(a, b) for a, b in SAME if not names_agree(*a, *b)]
bad += [(a, b, names_agree(*a, *b)) for a, b in OTHER if names_agree(*a, *b)]
for x in bad:
    print("FAIL", x)
print("PASS" if not bad else f"FAILED: {len(bad)}")
sys.exit(1 if bad else 0)
