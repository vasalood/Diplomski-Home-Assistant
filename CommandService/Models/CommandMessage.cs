using System.Text.Json.Serialization;

namespace CommandService.Models;

public class CommandMessage
{
    [JsonPropertyName("sensor_name")]
    public string? SensorName { get; set; }

    [JsonPropertyName("house_id")]
    public string? HouseId { get; set; }

    [JsonPropertyName("user_id")]
    public string? UserId { get; set; }

    [JsonPropertyName("state")]
    public string? State { get; set; }

    [JsonPropertyName("temperature")]
    public double? Temperature { get; set; }

    [JsonPropertyName("humidity")]
    public double? Humidity { get; set; }

    [JsonPropertyName("pressure")]
    public double? Pressure { get; set; }

    [JsonPropertyName("millis")]
    public long? Millis { get; set; }

    [JsonPropertyName("device")]
    public string? Device { get; set; }          // "AC", "HUMIDIFIER", "VENT_FAN"

    [JsonPropertyName("command")]
    public string? Command { get; set; }        // TURN_ON_AC, TURN_OFF_AC, TURN_ON_HUMIDIFIER…

    [JsonPropertyName("reason")]
    public string? Reason { get; set; }         // TEMP_LOW, HUMIDITY_HIGH...

    [JsonPropertyName("source")]
    public string? Source { get; set; }         // "ekuiper"
}
