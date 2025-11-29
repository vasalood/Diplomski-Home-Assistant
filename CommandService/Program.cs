using CommandService.Services;
using MongoDB.Driver;

var builder = Host.CreateApplicationBuilder(args);

builder.Services.AddHostedService<MqttCommandWorker>();

// Mongo setup
var mongoConn = builder.Configuration["Mongo:ConnectionString"] 
                ?? "mongodb://mongoadmin:Vasamare123@localhost:27017/";
var mongoDbName = builder.Configuration["Mongo:Database"] ?? "home_assistant";
var commandsCollection = builder.Configuration["Mongo:CommandsCollection"] ?? "commands";

var mongoClient = new MongoClient(mongoConn);
var mongoDatabase = mongoClient.GetDatabase(mongoDbName);

builder.Services.AddSingleton(mongoDatabase);
builder.Services.AddSingleton(sp => new CommandRepository(mongoDatabase, commandsCollection));

var app = builder.Build();
app.Run();
