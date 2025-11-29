from datetime import datetime, timezone

class SensorGatewayService:
  """
  SensorGatewayService:
  - prima RAW merenja sa MQTT data topica (ingest_measure)
  - prima security evente sa MQTT events topica (ingest_security_event)
  - svaki RAW merenje prosleđuje NATS-u na SUBJECT_RAW_PREFIX.<sensor_name>
  - radi tumbling avg prozore (temperature, humidity, pressure) i šalje na
    SUBJECT_AVG_PREFIX.<sensor_name>
  - security evente prosleđuje na SUBJECT_SECURITY_PREFIX.<sensor_name>
  """

  def __init__(
    self,
    window_sec: int,
    subject_raw_prefix: str,
    subject_avg_prefix: str,
    subject_security_prefix: str,
    nats_publisher,
  ):
    self.window_sec = window_sec
    self.subject_raw_prefix = subject_raw_prefix
    self.subject_avg_prefix = subject_avg_prefix
    self.subject_security_prefix = subject_security_prefix
    self.nats = nats_publisher

    # bucket za avg prozor
    self._bucket = []  # lista dict-ova sa merenjima
    self._window_started_at = None
    self._running = True

  # -------------------- DATA topic: RAW + AVG --------------------

  def ingest_measure(self, measure: dict):
    """
    Poziva se iz MQTT data workera za svako merenje.

    Očekuje ključeve:
    - sensor_name (str)
    - house_id (str)
    - user_id (str ili None)
    - state (str)  -- npr. "NORMAL"
    - temperature (float)
    - humidity (float)
    - pressure (float)
    - millis (int) -- millis sa Arduino uređaja
    """

    if not self._running:
      return

    try:
      temp = measure.get("temperature")
      hum = measure.get("humidity")
      press = measure.get("pressure")

      # Bez ova 3 nema smisla avg, pa ignorišemo
      if temp is None or hum is None or press is None:
        print("[gateway] Skipping measure without full env data:", measure, flush=True)
        return

      sensor_name = measure.get("sensor_name", "unknown_sensor")
      house_id = measure.get("house_id", "unknown_house")
      user_id = measure.get("user_id")
      millis = measure.get("millis")

      ts_server = datetime.now(timezone.utc).isoformat()

      # 1) RAW → NATS
      raw_payload = {
        "sensor_name": sensor_name,
        "house_id": house_id,
        "user_id": user_id,
        "state": measure.get("state"),
        "temperature": float(temp),
        "humidity": float(hum),
        "pressure": float(press),
        "millis": millis,
        "ts_server": ts_server,
      }

      raw_subject = f"{self.subject_raw_prefix}.{sensor_name}"
      self.nats.publish(raw_subject, raw_payload)
      print(f"[gateway] RAW -> {raw_subject}: {raw_payload}", flush=True)

      # 2) AVG prozor (tumbling po server-satu)
      now_utc = datetime.now(timezone.utc)

      if self._window_started_at is None:
        self._window_started_at = now_utc

      elapsed = (now_utc - self._window_started_at).total_seconds()
      if elapsed >= self.window_sec and self._bucket:
        self._flush_avg()

      # ubaci u trenutni prozor
      self._bucket.append({
        "ts_server": ts_server,
        "sensor_name": sensor_name,
        "house_id": house_id,
        "temperature": float(temp),
        "humidity": float(hum),
        "pressure": float(press),
      })

    except Exception as e:
      print(f"[gateway] Error in ingest_measure: {e}. Payload={measure}", flush=True)

  def _flush_avg(self):
    """
    Izračunaj avg temperature/humidity/pressure iz bucket-a i
    pošalji na SUBJECT_AVG_PREFIX.<sensor_name>.
    """
    if not self._bucket:
      return

    temps = [m["temperature"] for m in self._bucket]
    hums = [m["humidity"] for m in self._bucket]
    press = [m["pressure"] for m in self._bucket]
    count = len(self._bucket)

    avg_temp = sum(temps) / count if count else None
    avg_hum = sum(hums) / count if count else None
    avg_press = sum(press) / count if count else None

    # uzimamo sensor_name/house_id iz poslednjeg elementa (u praksi isti za sve)
    sensor_name = self._bucket[-1]["sensor_name"]
    house_id = self._bucket[-1]["house_id"]

    t_start = self._window_started_at.isoformat()
    t_end = datetime.now(timezone.utc).isoformat()

    out = {
      "sensor_name": sensor_name,
      "house_id": house_id,
      "window": {
        "type": "tumbling",
        "size_sec": self.window_sec,
      },
      "t_start": t_start,
      "t_end": t_end,
      "avg": {
        "temperature": avg_temp,
        "humidity": avg_hum,
        "pressure": avg_press,
      },
      "count": count,
    }

    subject = f"{self.subject_avg_prefix}.{sensor_name}"
    self.nats.publish(subject, out)
    print(f"[gateway] AVG -> {subject}: {out}", flush=True)

    # reset prozora
    self._bucket.clear()
    self._window_started_at = datetime.now(timezone.utc)

  # -------------------- EVENTS topic: SECURITY --------------------

  def ingest_security_event(self, event: dict):
    """
    Poziva se iz MQTT events workera za svaki security event.

    Ne diramo sadržaj event-a (osim što dodamo ts_server),
    samo prosleđujemo na SUBJECT_SECURITY_PREFIX.<sensor_name>.
    """
    if not self._running:
      return

    try:
      sensor_name = event.get("sensor_name", "unknown_sensor")
      subject = f"{self.subject_security_prefix}.{sensor_name}"

      payload = dict(event)  # kopija
      payload.setdefault("ts_server", datetime.now(timezone.utc).isoformat())

      self.nats.publish(subject, payload)
      print(f"[gateway] SECURITY -> {subject}: {payload}", flush=True)

    except Exception as e:
      print(f"[gateway] Error in ingest_security_event: {e}. Payload={event}", flush=True)

  # -------------------- stop --------------------

  def stop(self):
    """
    Zaustavlja servis i flusuje poslednji nepotpuni avg prozor.
    """
    self._running = False
    if self._bucket:
      self._flush_avg()
