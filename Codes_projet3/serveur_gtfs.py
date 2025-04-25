from flask import Flask, jsonify
import requests
from google.transit import gtfs_realtime_pb2

app = Flask(__name__)

GTFS_RT_URL = "https://api.stm.info/pub/od/gtfs-rt/ic/v2/vehiclePositions"
STM_API_KEY = "xxx"

# Chargement des trip_id de chaque sens
def charger_trips(fichier):
    with open(fichier, "r") as f:
        return set(line.strip() for line in f if line.strip())

trips_sens1 = charger_trips("id_ouest.txt")
trips_sens2 = charger_trips("id_est.txt")

@app.route('/api/gtfs-json')
def gtfs_json():
    feed = gtfs_realtime_pb2.FeedMessage()
    
    response = requests.get(GTFS_RT_URL, headers={"apiKey": STM_API_KEY})
    feed.ParseFromString(response.content)

    bus_positions = []
    for entity in feed.entity:
        if entity.HasField('vehicle') and entity.vehicle.trip.route_id == "51":
            vehicle = entity.vehicle
            trip_id = vehicle.trip.trip_id

            # Détermination du sens
            if trip_id in trips_sens1:
                direction = 1
            elif trip_id in trips_sens2:
                direction = 2
            else:
                direction = None  # trip_id inconnu

            bus_positions.append({
                "trip_id": vehicle.trip.trip_id,
                "route_id": vehicle.trip.route_id,
                "latitude": vehicle.position.latitude,
                "longitude": vehicle.position.longitude,
                "timestamp": vehicle.timestamp,
                "direction": direction,
                "achalandage": vehicle.occupancy_status
            })

    return jsonify({"bus_positions": bus_positions})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
