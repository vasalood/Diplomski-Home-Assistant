import { MongoClient } from "mongodb";

export async function createMongoClient(uri) {
  try {
    const client = new MongoClient(uri, {
      serverSelectionTimeoutMS: 5000,
      connectTimeoutMS: 5000,
    });

    console.log("[mongo] Connecting to MongoDB...");

    await client.connect();
    await client.db("admin").command({ ping: 1 });

    console.log("[mongo] Connected successfully");
    return client;
  } catch (err) {
    console.error("[mongo] Connection FAILED:");
    console.error(err.message);
    process.exit(1);
  }
}
