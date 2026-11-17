import os
import getpass
import requests
import pandas as pd

FIREBASE_URL = os.environ.get("FIREBASE_URL", "https://EL-TEU-PROJECTE-default-rtdb.europe-west1.firebasedatabase.app")
GROQ = "https://api.groq.com/openai/v1"
MODELS_PREFERITS = ["openai/gpt-oss-120b", "llama-3.3-70b-versatile", "openai/gpt-oss-20b", "llama-3.1-8b-instant"]
NOM = "la persona usuaria"


def carrega_partides():
    dades = requests.get(f"{FIREBASE_URL}/sessions.json", timeout=10).json()
    if not dades:
        return pd.DataFrame()
    df = pd.DataFrame.from_dict(dades, orient="index")
    df["data"] = pd.to_datetime(df["timestamp"], unit="ms", utc=True).dt.tz_convert("Europe/Madrid")
    if "exercici" in df:
        df = df[df["exercici"] == "laberint"]
    return df.sort_values("data")


def estadistiques(df):
    if df.empty:
        return {"partides": 0}
    completades = df[df["completat"] == True]
    return {
        "partides": int(len(df)),
        "completades": int(len(completades)),
        "dies_actius": int(df["data"].dt.date.nunique()),
        "temps_mitja_s": round(float(completades["temps_s"].mean()), 1) if len(completades) else None,
        "millor_temps_s": round(float(completades["temps_s"].min()), 1) if len(completades) else None,
        "caigudes_mitjanes": round(float(df["caigudes"].mean()), 1),
    }


def tria_model(clau):
    r = requests.get(f"{GROQ}/models", headers={"Authorization": f"Bearer {clau}"}, timeout=15)
    if r.status_code == 401:
        raise SystemExit("La clau de Groq no es valida. Revisa GROQ_API_KEY.")
    r.raise_for_status()
    disponibles = [m["id"] for m in r.json()["data"]]
    for m in MODELS_PREFERITS:
        if m in disponibles:
            return m
    text = [m for m in disponibles if not any(x in m for x in ("whisper", "guard", "tts", "playai"))]
    return text[0]


def demana_resum(aquesta, anterior, clau):
    prompt = f"""Ets l'assistent d'un joc de rehabilitacio suau per a gent gran: un laberint
que es controla inclinant un dispositiu amb la ma (treballa la mobilitat del canell,
la coordinacio i l'atencio).

Dades d'aquesta setmana de {NOM}: {aquesta}
Dades de la setmana anterior: {anterior}
(temps en segons; un temps mes baix i menys caigudes vol dir millor)

Escriu un resum per a la familia o el fisioterapeuta, en catala:
- 4 o 5 frases, to proper i positiu, sense emojis.
- Digues quant ha jugat i com ha evolucionat respecte la setmana anterior, amb els numeros clau.
- Acaba amb un suggeriment practic i motivador per a la setmana que ve.
- No facis diagnostics medics ni inventis dades que no hi siguin."""
    model = tria_model(clau)
    print("Model de Groq:", model)
    r = requests.post(
        f"{GROQ}/chat/completions",
        headers={"Authorization": f"Bearer {clau}"},
        json={"model": model, "temperature": 0.5,
              "messages": [{"role": "user", "content": prompt}]},
        timeout=30,
    )
    if not r.ok:
        raise SystemExit(f"Error de Groq {r.status_code}: {r.text}")
    return r.json()["choices"][0]["message"]["content"].strip()


def main():
    clau = os.environ.get("GROQ_API_KEY") or getpass.getpass("Clau de Groq (no es veura): ")
    df = carrega_partides()
    if df.empty:
        print("Encara no hi ha partides a Firebase.")
        return

    ara = pd.Timestamp.now(tz="Europe/Madrid")
    fa7 = ara - pd.Timedelta(days=7)
    fa14 = ara - pd.Timedelta(days=14)
    aquesta = estadistiques(df[df["data"] > fa7])
    anterior = estadistiques(df[(df["data"] > fa14) & (df["data"] <= fa7)])

    print("Aquesta setmana:", aquesta)
    print("Setmana anterior:", anterior)
    resum = demana_resum(aquesta, anterior, clau)
    print("\n--- RESUM ---\n" + resum)

    requests.post(f"{FIREBASE_URL}/resums.json", timeout=10, json={
        "text": resum,
        "estadistiques": aquesta,
        "timestamp": {".sv": "timestamp"},
    })
    print("\nGuardat a Firebase (/resums). Obre dashboard.html per veure-ho.")


if __name__ == "__main__":
    main()
