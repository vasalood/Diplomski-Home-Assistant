import sys
import time
import signal
import os

from dotenv import load_dotenv

from mqtt_client.subscriber import MqttWorker
from nats_client.publisher import NatsPublisher
from services.gateway import SensorGatewayService

load_dotenv()

MQTT_HOST = os.environ["MQTT_HOST"]
MQTT_PORT = int(os.environ["MQTT_PORT"])
MQTT_TOPIC_DATA = os.environ["MQTT_TOPIC_DATA"]
MQTT_TOPIC_EVENTS = os.environ["MQTT_TOPIC_EVENTS"]

NATS_SERVER = os.environ["NATS_SERVER"]

WINDOW_SEC = int(os.environ["WINDOW_SEC"])

SUBJECT_RAW_PREFIX = os.environ["SUBJECT_RAW_PREFIX"]
SUBJECT_AVG_PREFIX = os.environ["SUBJECT_AVG_PREFIX"]
SUBJECT_SECURITY_PREFIX = os.environ["SUBJECT_SECURITY_PREFIX"]

def main():
  print("[main] Starting SensorGatewayService stack...", flush=True)

  print("[main] Starting SensorGatewayService stack...", flush=True)

  print("[main] Config:", flush=True)
  print(f"  MQTT_HOST={MQTT_HOST}", flush=True)
  print(f"  MQTT_PORT={MQTT_PORT}", flush=True)
  print(f"  MQTT_TOPIC_DATA={MQTT_TOPIC_DATA}", flush=True)
  print(f"  MQTT_TOPIC_EVENTS={MQTT_TOPIC_EVENTS}", flush=True)
  print(f"  NATS_SERVERS={NATS_SERVER}", flush=True)
  print(f"  WINDOW_SEC={WINDOW_SEC}", flush=True)
  print(f"  SUBJECT_RAW_PREFIX={SUBJECT_RAW_PREFIX}", flush=True)
  print(f"  SUBJECT_AVG_PREFIX={SUBJECT_AVG_PREFIX}", flush=True)
  print(f"  SUBJECT_SECURITY_PREFIX={SUBJECT_SECURITY_PREFIX}", flush=True)

  nats_pub = NatsPublisher(servers=NATS_SERVER)
  nats_pub.start()  

  gateway = SensorGatewayService(
    window_sec=WINDOW_SEC,
    subject_raw_prefix=SUBJECT_RAW_PREFIX,
    subject_avg_prefix=SUBJECT_AVG_PREFIX,
    subject_security_prefix=SUBJECT_SECURITY_PREFIX,
    nats_publisher=nats_pub
  )

  mqtt_data = MqttWorker(
    host=MQTT_HOST,
    port=MQTT_PORT,
    topic=MQTT_TOPIC_DATA,
    on_measure=gateway.ingest_measure  
  )
  mqtt_data.start()

  mqtt_events = MqttWorker(
    host=MQTT_HOST,
    port=MQTT_PORT,
    topic=MQTT_TOPIC_EVENTS,
    on_measure=gateway.ingest_security_event,
  )
  mqtt_events.start()

  def stop_fn(signum=None, frame=None):
    print("\n[main] Stopping gracefully ...", flush=True)
    try:
      mqtt_data.stop()
    except Exception:
      pass
    try:
      mqtt_events.stop()
    except Exception:
      pass

    try:
      gateway.stop()
    except Exception:
      pass

    try:
      nats_pub.stop()
    except Exception:
      pass

    print("[main] Stopped. Bye.", flush=True)
    sys.exit(0)

  signal.signal(signal.SIGINT, stop_fn)  
  signal.signal(signal.SIGTERM, stop_fn) 

  try:
    while True:
      time.sleep(1)
  except KeyboardInterrupt:
    stop_fn()

if __name__ == "__main__":
  main()