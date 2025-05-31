#include <Arduino.h>
#include <WiFiNINA.h> // For RP2040 Connect

// Include your custom libraries
#include "clamps.h"
#include "buzzer.h"
#include "leds.h"
#include "pyro.h"
#include "html.h"       // Assumes index_html is defined here in PROGMEM
#include "webserver.h"  // Our C-string based web server

// --- Global Objects ---
Clamps clamps = Clamps();
PyroChannel igniter = PyroChannel(PYRO_IGNITION_PIN, 2000);

// --- Wi-Fi Access Point Configuration ---
const char* ssid = "LaunchPad";     // The name of the Wi-Fi network to create
const char* password = "12345678";  // Minimum 8 characters for AP password

// --- Static IP Configuration for AP Mode ---
IPAddress apIP(192, 168, 4, 1);       // Static IP address for the AP
IPAddress apGateway(192, 168, 4, 1);  // Gateway IP (often same as AP IP for simple AP)
IPAddress apSubnet(255, 255, 255, 0); // Subnet mask

// --- Web Server Instance ---
SimpleWebServer server(80); // HTTP port

// --- Control Functions ---
void launch() {
    Serial.println(F("Launch sequence initiated from web."));
    clamps.openClamps(); // Open clamps
    igniter.fire();      // Fire pyro
    // Add any other launch sequence steps here
}

void abortLaunch() {
    Serial.println(F("ABORT sequence initiated from web."));
    clamps.closeClamps(); // Close clamps
    igniter.stop();       // Stop pyro
    // Add any other abort sequence steps here
}

// --- Arduino Setup ---
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000); // Wait for Serial port for native USB

    Serial.println(F("\nLaunch Control System Initializing..."));

    // Start the AP first (without IP config)
    Serial.print(F("Starting Access Point: "));
    Serial.println(ssid);

    // Start the Access Point
    if (WiFi.beginAP(ssid, password) == WL_AP_LISTENING) {
        Serial.println(F("Access Point started successfully!"));
        Serial.print(F("AP IP address: "));
        Serial.println(WiFi.localIP());
    }
    else {
        Serial.println(F("Failed to start Access Point!"));
        while (1) {
            flash(COLOR_RED, 500);
        }
    }

    // --- Define Web Server Routes (Endpoints) ---

    // Serve the main HTML UI from PROGMEM
    server.on("/", HTTP_GET, []() {
        Serial.println(F("Serving main page /"));
        server.send_P(200, "text/html", index_html);
        });

    // Handle /launch command
    server.on("/launch", HTTP_GET, []() {
        Serial.println(F("Received /launch command"));
        launch();
        server.send(200, "text/plain", "Launch sequence triggered.");
        });

    // Handle /abort command
    server.on("/abort", HTTP_GET, []() {
        Serial.println(F("Received /abort command"));
        abortLaunch();
        server.send(200, "text/plain", "Abort sequence triggered.");
        });

    // Handle /clamps/open command
    server.on("/clamps/open", HTTP_GET, []() {
        Serial.println(F("Received /clamps/open command"));
        clamps.openClamps();
        server.send(200, "text/plain", "Clamps opened.");
        });

    // Handle /clamps/close command
    server.on("/clamps/close", HTTP_GET, []() {
        Serial.println(F("Received /clamps/close command"));
        clamps.closeClamps();
        server.send(200, "text/plain", "Clamps closed.");
        });

    // --- Start the Web Server ---
    server.begin();
    Serial.println(F("Web server started."));
    Serial.print(F("Connect to Wi-Fi '"));
    Serial.print(ssid);
    Serial.print(F("' and navigate to http://"));
    Serial.print(WiFi.localIP());
    Serial.println(F("/"));

    igniter.begin();
    igniter.arm();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
}

// --- Global Variables ---
int status = WL_IDLE_STATUS;

// --- Arduino Loop ---
void loop() {
    // This is crucial: it allows the server to process incoming client requests.
    server.handleClient();

    // compare the previous status to the current status
    if (status != WiFi.status()) {
        // it has changed update the variable
        status = WiFi.status();
        if (status == WL_AP_CONNECTED) {
            // a device has connected to the AP
            Serial.println("Device connected to AP");
        }
        else {
            // a device has disconnected from the AP, and we are back in listening mode
            Serial.println("Device disconnected from AP");
        }
    }

    // You can add other non-blocking tasks here if needed.
    // Avoid using long delays in the loop() as it will make the web server unresponsive.
    delay(1); // A very small delay can sometimes be helpful for stability on some platforms
    flash(COLOR_BLUE, 500);

    igniter.update();
}
