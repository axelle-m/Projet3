import requests
import pandas as pd
from datetime import datetime, timedelta
import math
import time


FLASK_API_URL = "http://localhost:5000/api/gtfs-json"
STOP_TIMES_PATH = "stop_times.txt"
FICHIER_SORTIE = "retards_emontpetit.csv"
INTERVALLE_SECONDES = 5
SEUIL_METRES = 30
TEMPS_EXPIRATION_MINUTES = 5 

# arrêts sélectionnés
arrets = {
    "51213": {"nom": "McKenna (Est)",       "lat": 45.508690, "lon": -73.614048, "dir": 1},
    "51247": {"nom": "Louis-Colin (Est)",   "lat": 45.507417, "lon": -73.615209, "dir": 1},
    "51286": {"nom": "Woodbury (Est)",      "lat": 45.505141, "lon": -73.617049, "dir": 1},
    "51318": {"nom": "de Stirling (Est)",   "lat": 45.504315, "lon": -73.618006, "dir": 1},
    "51214": {"nom": "McKenna (Ouest)",     "lat": 45.501366, "lon": -73.620681, "dir": 2},
    "51248": {"nom": "Louis-Colin (Ouest)", "lat": 45.503404, "lon": -73.618841, "dir": 2},
    "51287": {"nom": "Woodbury (Ouest)",    "lat": 45.505141, "lon": -73.617049, "dir": 2},
    "51319": {"nom": "de Stirling (Ouest)", "lat": 45.507133, "lon": -73.615253, "dir": 2}
}

# fichiers de la STM avec horaires fixes
stop_times = pd.read_csv(STOP_TIMES_PATH)
stop_times['trip_id'] = stop_times['trip_id'].astype(str)
stop_times['stop_id'] = stop_times['stop_id'].astype(str)

# conversion GTFS 
def time_to_datetime(hms):
    try:
        h, m, s = map(int, hms.split(":"))
        now = datetime.now()
        if h >= 24:
            h -= 24
            now += timedelta(days=1)
        return now.replace(hour=h, minute=m, second=s, microsecond=0)
    except:
        return None

def distanceGPS(lat1, lon1, lat2, lon2):
    R = 6371000
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2)**2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c
 
resultats = []
detections_enregistrees = {}  

print("On cherche les bus autour d'Édouard-Montpetit...")

while True:
    try:
        r = requests.get(FLASK_API_URL)
        r.raise_for_status()
        data = r.json()["bus_positions"]
    except Exception as e:
        print(" Erreur de requête :", e)
        time.sleep(INTERVALLE_SECONDES)
        continue

    print(f"\n {datetime.now().strftime('%H:%M:%S')} | Bus reçus : {len(data)}")

    for bus in data:
        trip_id = str(bus.get("trip_id"))

        try:
            direction = int(bus.get("direction"))
        except (TypeError, ValueError):
            print(f"direction manquante : {bus}")
            continue

        lat = bus.get("latitude")
        lon = bus.get("longitude")
        timestamp = bus.get("timestamp")

        if not all([trip_id, lat, lon, timestamp]):
            continue

        for stop_id, arret in arrets.items():
            if direction != arret["dir"]:
                continue

            dist = distanceGPS(lat, lon, arret["lat"], arret["lon"])
            if dist < SEUIL_METRES:
                key = (trip_id, stop_id)
                now = datetime.now()

                if key in detections_enregistrees:
                    delta = now - detections_enregistrees[key]
                    if delta < timedelta(minutes=TEMPS_EXPIRATION_MINUTES):
                        continue  

                detections_enregistrees[key] = now

                heure_reelle = datetime.fromtimestamp(timestamp)
                subset = stop_times[
                    (stop_times["trip_id"] == trip_id) &
                    (stop_times["stop_id"] == stop_id)
                ]

                if subset.empty:
                    print(f" Aucun horaire trouvé pour trip_id={trip_id} à {arret['nom']}")
                    continue

                heure_prev = time_to_datetime(subset.iloc[0]["arrival_time"])
                ecart_min = round((heure_reelle - heure_prev).total_seconds() / 60, 2)

                print(f" Bus {trip_id} à {arret['nom']} ({dist:.1f} m) | "
                      f"Prévu: {heure_prev.strftime('%H:%M:%S')} | Réel: {heure_reelle.strftime('%H:%M:%S')} | "
                      f"Écart: {ecart_min:+.2f} min")

                resultats.append({
                    "datetime": now.strftime("%Y-%m-%d %H:%M:%S"),
                    "stop_id": stop_id,
                    "nom": arret["nom"],
                    "trip_id": trip_id,
                    "direction": direction,
                    "distance_m": round(dist, 2),
                    "heure_prev": heure_prev.strftime("%H:%M:%S"),
                    "heure_reelle": heure_reelle.strftime("%H:%M:%S"),
                    "retard_min": ecart_min
                })

    pd.DataFrame(resultats).to_csv(FICHIER_SORTIE, index=False)
    time.sleep(INTERVALLE_SECONDES)
