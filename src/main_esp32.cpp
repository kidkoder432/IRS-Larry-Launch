#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <clamps.h>
#include <pyro.h>
#include <leds.h>
#include <buzzer.h>
#include <html.h>

Clamps clamps = Clamps();
PyroChannel igniter = PyroChannel(13, 2000);

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
    digitalWrite(13, HIGH);
    Serial.println("Hello World!");
    digitalWrite(13, LOW);

    // Create Access Point
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

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

    // You can add other non-blocking tasks here if needed.
    // Avoid using long delays in the loop() as it will make the web server unresponsive.
    delay(1); // A very small delay can sometimes be helpful for stability on some platforms
    flash(COLOR_BLUE, 500);

    igniter.update();
}