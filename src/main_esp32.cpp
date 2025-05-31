#include <Arduino.h>
#include <Wifi.h>
#include <WebServer.h>
#include <clamps.h>
#include <pyro.h>
#include <leds.h>
#include <buzzer.h>
#include <html.h>

Clamps clamps = Clamps();
PyroChannel igniter = PyroChannel();

const char* ssid = "LaunchPad";       // or whatever WiFi you want to create
const char* password = "12345678";    // min 8 characters for softAP

WebServer server(80);

void launch() {
    clamps.openClamps();
    igniter.fire();
}

void abortLaunch() {
    clamps.closeClamps();
    igniter.stop();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Hello World!");

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Serve the HTML UI
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", index_html); // assume you have `index_html` defined somewhere
        });

    // Handle commands
    server.on("/launch", HTTP_GET, []() {
        Serial.println("Launch sequence initiated");
        launch(); // Your function
        server.send(200, "text/plain", "Launch triggered");
        });

    server.on("/abort", HTTP_GET, []() {
        Serial.println("ABORT!");
        abortLaunch(); // Your function
        server.send(200, "text/plain", "Abort triggered");
        });

    server.on("/clamps/open", HTTP_GET, []() {
        Serial.println("Clamps OPEN");
        clamps.openClamps(); // Your function
        server.send(200, "text/plain", "Clamps opened");
        });

    server.on("/clamps/close", HTTP_GET, []() {
        Serial.println("Clamps CLOSED");
        clamps.closeClamps(); // Your function
        server.send(200, "text/plain", "Clamps closed");
        });

    server.begin();
    Serial.println("Web server started");

    igniter.begin();
    igniter.arm();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
}

void loop() {
    server.handleClient();
}