import { connect as natsConnect } from "nats";

export async function createNatsConnection(servers) {
  const nats_conn = await natsConnect({ servers });
  console.log(`[nats] Connected to ${servers}`);
  return nats_conn;
}

export function subscribeNATS(nats_conn, subject, handler) {
  const sub = nats_conn.subscribe(subject);

  (async () => {
    for await (const m of sub) {
      try {
        const obj = JSON.parse(m.data.toString());
        await handler(obj);
      } catch (err) {
        console.error("[nats] Bad message (not JSON?):", err);
      }
    }
  })();

  console.log(`[nats] Subscribed to ${subject}`);
  return sub;
}
