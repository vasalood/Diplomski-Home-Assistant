using CommandService.Models;
using MongoDB.Bson;
using MongoDB.Driver;

namespace CommandService.Services;

public class CommandRepository
{
    private readonly IMongoCollection<BsonDocument> _commands;

    public CommandRepository(IMongoDatabase db, string collectionName)
    {
        _commands = db.GetCollection<BsonDocument>(collectionName);
    }

    public async Task InsertCommandAsync(CommandMessage msg, bool desiredStateOn, CancellationToken ct = default)
    {
        var doc = new BsonDocument
        {
            { "ts_server", DateTime.UtcNow },

            { "metadata", new BsonDocument
                {
                    { "sensor_name", msg.SensorName ?? "unknown_sensor" },
                    { "house_id",   msg.HouseId   ?? "unknown_house" },
                    { "user_id",    BsonValue.Create(msg.UserId) },
                    { "device",     msg.Device    ?? "UNKNOWN_DEVICE" }
                }
            },

            { "command",     BsonValue.Create(msg.Command) },
            { "reason",      BsonValue.Create(msg.Reason) },
            { "source",      BsonValue.Create(msg.Source) },
            { "temperature", BsonValue.Create(msg.Temperature) },
            { "humidity",    BsonValue.Create(msg.Humidity) },
            { "pressure",    BsonValue.Create(msg.Pressure) },
            { "millis",      BsonValue.Create(msg.Millis) },

            { "desired_state", desiredStateOn ? "ON" : "OFF" }
        };

        await _commands.InsertOneAsync(doc, cancellationToken: ct);
    }
}
