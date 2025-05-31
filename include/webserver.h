#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>    // For Serial, millis, delay, PROGMEM, F(), __FlashStringHelper
#include <WiFiNINA.h>   // For WiFiServer, WiFiClient
#include <functional>   // For std::function
#include <vector>       // For std::vector
#include <string.h>     // For C-string functions like strcmp, strcpy, strtok_r, strncpy, strchr, strlen
#include <stdio.h>      // For snprintf, sscanf
#include <avr/pgmspace.h> // For pgm_read_byte, strlen_P, strncpy_P (used in send_P)

// --- Debug Configuration ---
// Uncomment the next line to enable detailed SWS_DEBUG serial prints for troubleshooting
#define SWS_ENABLE_DEBUG_PRINTING 1


// --- Configuration Defines ---
#define SWS_REQUEST_BUFFER_SIZE 256     // Max size for one line of HTTP request
#define SWS_MAX_URI_LENGTH 128          // Max length for the entire URI (path + query)
#define SWS_MAX_PATH_LENGTH 64          // Max length for the path part of the URI
#define SWS_MAX_QUERY_LENGTH 128        // Max length for the query string part
#define SWS_MAX_HANDLER_PATH_LEN 64     // Max length for a registered handler's path
#define SWS_MAX_ARGS 10                 // Max number of URL query arguments to parse
#define SWS_MAX_ARG_KEY_LEN 32          // Max length for an argument key
#define SWS_MAX_ARG_VALUE_LEN 64        // Max length for an argument value
#define SWS_SEND_P_BUFFER_SIZE 64       // Buffer size for sending PROGMEM content in chunks
#define SWS_STATUS_LINE_BUFFER_SIZE 80  // Buffer for constructing HTTP status lines

// Define SWS_MAX_URI_LENGTH_SCANF_VALUE as the literal number for sscanf width
#define SWS_MAX_URI_LENGTH_SCANF_VALUE (SWS_MAX_URI_LENGTH - 1) // This will be e.g. 127

// Helper macros for stringifying constants for sscanf format strings
#define SWS_MACRO_STR_HELPER(x) #x
#define SWS_MACRO_STR(x) SWS_MACRO_STR_HELPER(x) // Ensures x is expanded to its value if it's a macro, then stringified


// Global enum for HTTP methods
enum HTTP_METHOD_ENUM {
    HTTP_ANY, HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS
};

class SimpleWebServer {
public:
    SimpleWebServer(uint16_t port = 80) : _wifiServer(port) {
        _currentUri[0] = '\0';
        _currentPath[0] = '\0';
        _empty_string[0] = '\0';
    }

    void on(const char* path, HTTP_METHOD_ENUM method, std::function<void()> callback) {
        if (!path || strlen(path) >= SWS_MAX_HANDLER_PATH_LEN) {
        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.println(F("SWS_DEBUG: Handler path too long or null, not adding."));
        #endif
            return;
        }
        if (_handlers.size() >= 20) {
        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.println(F("SWS_DEBUG: Max handlers reached. Cannot add more."));
        #endif
            return;
        }
        RequestHandler handler;
        strncpy(handler.path, path, SWS_MAX_HANDLER_PATH_LEN - 1);
        handler.path[SWS_MAX_HANDLER_PATH_LEN - 1] = '\0';
        handler.method = method;
        handler.callback = callback;
        _handlers.push_back(handler);
    }

    void onGet(const char* path, std::function<void()> callback) {
        on(path, HTTP_GET, callback);
    }

    void onPost(const char* path, std::function<void()> callback) {
        on(path, HTTP_POST, callback);
    }

    void begin() {
        _wifiServer.begin();
    }

    bool hasArg(const char* name) {
        if (!name) return false;
        for (const auto& arg : _currentArgs) {
            if (strcmp(arg.key, name) == 0) {
                return true;
            }
        }
        return false;
    }

    const char* arg(const char* name) {
        if (!name) return _empty_string;
        for (const auto& argument : _currentArgs) {
            if (strcmp(argument.key, name) == 0) {
                return argument.value;
            }
        }
        return _empty_string;
    }

    const char* arg(int i) {
        if (i >= 0 && (size_t)i < _currentArgs.size()) {
            return _currentArgs[i].value;
        }
        return _empty_string;
    }

    int args() {
        return _currentArgs.size();
    }

    const char* argName(int i) {
        if (i >= 0 && (size_t)i < _currentArgs.size()) {
            return _currentArgs[i].key;
        }
        return _empty_string;
    }

    void handleClient() {
        WiFiClient client = _wifiServer.available();
        if (client) {
            _currentClient = client;
            resetRequestState();

            char requestLineBuffer[SWS_REQUEST_BUFFER_SIZE];
            int bufferIdx = 0;
            bool firstLineRead = false;
            unsigned long requestStartTime = millis();

        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.println(F("SWS_DEBUG: New client connected. Reading request..."));
        #endif

            while (_currentClient.connected()) {
                if (millis() - requestStartTime > 5000) {
                    Serial.println(F("SWS: Client request timeout."));
                    _currentClient.stop();
                    return;
                }

                if (_currentClient.available()) {
                    char c = _currentClient.read();
                    if (c == '\n') {
                        requestLineBuffer[bufferIdx] = '\0';
                        if (!firstLineRead) {
                        #ifdef SWS_ENABLE_DEBUG_PRINTING
                            Serial.print(F("SWS_DEBUG: Attempting to parse request line: ["));
                            Serial.print(requestLineBuffer);
                            Serial.println(F("]"));
                        #endif
                            parseRequestLine(requestLineBuffer);
                            firstLineRead = true;
                        }
                        else if (bufferIdx == 0) {
                        #ifdef SWS_ENABLE_DEBUG_PRINTING
                            Serial.println(F("SWS_DEBUG: End of headers detected."));
                        #endif
                            break;
                        }
                        bufferIdx = 0;
                    }
                    else if (c != '\r') {
                        if (bufferIdx < SWS_REQUEST_BUFFER_SIZE - 1) {
                            requestLineBuffer[bufferIdx++] = c;
                        }
                        else {
                            Serial.println(F("SWS: Request line buffer overflow."));
                            send(413, "text/plain", "Request line too long.");
                            _currentClient.stop();
                            return;
                        }
                    }
                }
            }

            if (!firstLineRead && !_currentClient.connected()) {
                Serial.println(F("SWS: Client disconnected before request line."));
                return;
            }

            bool handlerFound = false;
            if (_currentPath[0] != '\0') {
            #ifdef SWS_ENABLE_DEBUG_PRINTING
                Serial.print(F("SWS_DEBUG: Searching handler for path: ["));
                Serial.print(_currentPath);
                Serial.print(F("] Method: "));
                Serial.println(_currentMethod);
            #endif
                for (const auto& handler : _handlers) {
                    if ((handler.method == _currentMethod || handler.method == HTTP_ANY) &&
                        (strcmp(handler.path, _currentPath) == 0)) {
                    #ifdef SWS_ENABLE_DEBUG_PRINTING
                        Serial.print(F("SWS_DEBUG: Handler found for path: ["));
                        Serial.print(handler.path);
                        Serial.println(F("]. Executing."));
                    #endif
                        handler.callback();
                        handlerFound = true;
                        break;
                    }
                }
            }
            else if (firstLineRead) {
                Serial.println(F("SWS: Request line parsed but no valid path determined."));
            }


            if (!handlerFound) {
                if (firstLineRead) {
                #ifdef SWS_ENABLE_DEBUG_PRINTING
                    Serial.print(F("SWS_DEBUG: No handler found for URI: ["));
                    Serial.print(_currentUri);
                    Serial.println(F("]. Sending 404."));
                #endif
                    char message404[SWS_MAX_URI_LENGTH + 20];
                    snprintf(message404, sizeof(message404), "Not Found: %s", _currentUri[0] != '\0' ? _currentUri : "unknown");
                    send(404, "text/plain", message404);
                }
                else if (_currentClient.connected()) {
                #ifdef SWS_ENABLE_DEBUG_PRINTING
                    Serial.println(F("SWS_DEBUG: No request line read, but client connected. Sending 400."));
                #endif
                    send(400, "text/plain", "Bad Request");
                }
            }

            if (_currentClient.connected()) {
                _currentClient.stop();
            #ifdef SWS_ENABLE_DEBUG_PRINTING
                Serial.println(F("SWS_DEBUG: Client disconnected by server."));
            #endif
            }
        }
    }

    void send(int httpStatusCode, const char* contentType, const char* content) {
        if (!_currentClient || !_currentClient.connected()) {
            return;
        }
        char statusLineBuf[SWS_STATUS_LINE_BUFFER_SIZE];
        constructHttpStatusLine(httpStatusCode, statusLineBuf, sizeof(statusLineBuf));
        _currentClient.println(statusLineBuf);
        _currentClient.print(F("Content-Type: "));
        _currentClient.println(contentType ? contentType : "application/octet-stream");
        _currentClient.println(F("Connection: close"));
        size_t contentLength = content ? strlen(content) : 0;
        _currentClient.print(F("Content-Length: "));
        _currentClient.println(contentLength);
        _currentClient.println();
        if (contentLength > 0) {
            _currentClient.print(content);
        }
        _currentClient.flush();
    }

    // Place these methods inside your SimpleWebServer class

// Version 1: Content type is also from PROGMEM (e.g., using F("text/html"))
    void send_P(int httpStatusCode, const __FlashStringHelper* contentTypeF, const char* progmemContent) {
        if (!_currentClient || !_currentClient.connected()) {
        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.println(F("SWS_DEBUG: No client or client disconnected, cannot send_P (F() contentType)."));
        #endif
            return;
        }

        char statusLineBuf[SWS_STATUS_LINE_BUFFER_SIZE];
        constructHttpStatusLine(httpStatusCode, statusLineBuf, sizeof(statusLineBuf));
        _currentClient.println(statusLineBuf);

        _currentClient.print(F("Content-Type: "));
        if (contentTypeF) {
            _currentClient.println(contentTypeF); // Print __FlashStringHelper directly
        }
        else {
            _currentClient.println(F("application/octet-stream")); // Default if null
        }
        _currentClient.println(F("Connection: close"));

        size_t contentLength = 0;
        if (progmemContent) {
            contentLength = strlen_P(progmemContent);
        }

        _currentClient.print(F("Content-Length: "));
        _currentClient.println(contentLength);
        _currentClient.println(); // Blank line signifies end of headers

        if (contentLength > 0 && progmemContent) {
            char buffer[64]; // No +1 needed for client.write
            size_t sentBytes = 0;
            while (sentBytes < contentLength) {
                size_t chunkSize = min((size_t)64, contentLength - sentBytes);
                // Copy from PROGMEM to RAM buffer
                memcpy_P(buffer, progmemContent + sentBytes, chunkSize);
                // Send the exact number of bytes from the RAM buffer
                _currentClient.write((const uint8_t*)buffer, chunkSize);
                sentBytes += chunkSize;
            }
        }
        _currentClient.flush(); // Ensure all data is sent
    #ifdef SWS_ENABLE_DEBUG_PRINTING
        Serial.println(F("SWS_DEBUG: send_P (F() contentType) completed."));
    #endif
    }

    // Version 2: Content type is a regular C-string (from RAM)
    void send_P(int httpStatusCode, const char* contentType, const char* progmemContent) {
        if (!_currentClient || !_currentClient.connected()) {
        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.println(F("SWS_DEBUG: No client or client disconnected, cannot send_P (char* contentType)."));
        #endif
            return;
        }

        char statusLineBuf[SWS_STATUS_LINE_BUFFER_SIZE];
        constructHttpStatusLine(httpStatusCode, statusLineBuf, sizeof(statusLineBuf));
        _currentClient.println(statusLineBuf);

        _currentClient.print(F("Content-Type: "));
        _currentClient.println(contentType ? contentType : "application/octet-stream");
        _currentClient.println(F("Connection: close"));

        size_t contentLength = 0;
        if (progmemContent) {
            contentLength = strlen_P(progmemContent);
        }

        _currentClient.print(F("Content-Length: "));
        _currentClient.println(contentLength);
        _currentClient.println(); // Blank line

        if (contentLength > 0 && progmemContent) {
            char buffer[128]; // No +1 needed for client.write
            size_t sentBytes = 0;
            while (sentBytes < contentLength) {
                size_t chunkSize = min((size_t)64, contentLength - sentBytes);
                memcpy_P(buffer, progmemContent + sentBytes, chunkSize);
                _currentClient.write((const uint8_t*)buffer, chunkSize);
                sentBytes += chunkSize;
            }
        }
        _currentClient.flush();
    #ifdef SWS_ENABLE_DEBUG_PRINTING
        Serial.println(F("SWS_DEBUG: send_P (char* contentType) completed."));
    #endif
    }

private:
    WiFiServer _wifiServer;
    WiFiClient _currentClient;

    struct RequestArgument {
        char key[SWS_MAX_ARG_KEY_LEN];
        char value[SWS_MAX_ARG_VALUE_LEN];
    };
    std::vector<RequestArgument> _currentArgs;

    struct RequestHandler {
        char path[SWS_MAX_HANDLER_PATH_LEN];
        HTTP_METHOD_ENUM method;
        std::function<void()> callback;
    };
    std::vector<RequestHandler> _handlers;

    char _currentUri[SWS_MAX_URI_LENGTH];
    char _currentPath[SWS_MAX_PATH_LENGTH];
    HTTP_METHOD_ENUM _currentMethod;

    static char _empty_string[1];

    void resetRequestState() {
        _currentArgs.clear();
        _currentUri[0] = '\0';
        _currentPath[0] = '\0';
        _currentMethod = HTTP_ANY;
    }

    void constructHttpStatusLine(int code, char* buffer, size_t bufferSize) {
        const char* statusText;
        switch (code) {
            case 200: statusText = "OK"; break;
            case 201: statusText = "Created"; break;
            case 204: statusText = "No Content"; break;
            case 400: statusText = "Bad Request"; break;
            case 401: statusText = "Unauthorized"; break;
            case 403: statusText = "Forbidden"; break;
            case 404: statusText = "Not Found"; break;
            case 413: statusText = "Payload Too Large"; break;
            case 500: statusText = "Internal Server Error"; break;
            default:  statusText = "Status"; break;
        }
        snprintf(buffer, bufferSize, "HTTP/1.1 %d %s", code, statusText);
    }

    void parseRequestLine(const char* requestLine) {
        if (!requestLine) return;

    #ifdef SWS_ENABLE_DEBUG_PRINTING
        Serial.print(F("SWS_DEBUG_PARSE: Manual Parse - Raw Req Line: ["));
        Serial.print(requestLine);
        Serial.println(F("]"));
    #endif

        char methodStr[10];
        char uriBuf[SWS_MAX_URI_LENGTH];

        methodStr[0] = '\0';
        uriBuf[0] = '\0';

        // Make a writable copy of the request line for strtok_r or manual manipulation
        char lineCopy[SWS_REQUEST_BUFFER_SIZE];
        strncpy(lineCopy, requestLine, SWS_REQUEST_BUFFER_SIZE - 1);
        lineCopy[SWS_REQUEST_BUFFER_SIZE - 1] = '\0';

        char* token;
        char* rest = lineCopy;
        int part = 0;

        // Part 1: Method
        token = strtok_r(rest, " ", &rest);
        if (token) {
            strncpy(methodStr, token, sizeof(methodStr) - 1);
            methodStr[sizeof(methodStr) - 1] = '\0';
            part++;
        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.print(F("SWS_DEBUG_PARSE: Manual Token 1 (Method): [")); Serial.print(methodStr); Serial.println(F("]"));
        #endif
        }
        else {
            Serial.println(F("SWS: Invalid request line format (No method token)."));
            _currentMethod = HTTP_ANY;
            _currentPath[0] = '\0';
            _currentUri[0] = '\0';
            return;
        }

        // Part 2: URI
        token = strtok_r(rest, " ", &rest);
        if (token) {
            strncpy(uriBuf, token, sizeof(uriBuf) - 1);
            uriBuf[sizeof(uriBuf) - 1] = '\0';
            part++;
        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.print(F("SWS_DEBUG_PARSE: Manual Token 2 (URI): [")); Serial.print(uriBuf); Serial.println(F("]"));
        #endif
        }
        else {
            Serial.println(F("SWS: Invalid request line format (No URI token)."));
            _currentMethod = HTTP_ANY;
            _currentPath[0] = '\0';
            _currentUri[0] = '\0';
            return;
        }

        // Part 3: Protocol (Optional to fully parse, but good to acknowledge its presence)
        // If 'rest' is not NULL here, it points to the protocol string.
        // We don't strictly need to tokenize it further for this server's logic
        // as long as we've got the method and URI.
        if (rest && strlen(rest) > 0) {
            part++; // Acknowledge protocol part exists
        #ifdef SWS_ENABLE_DEBUG_PRINTING
            Serial.print(F("SWS_DEBUG_PARSE: Manual Token 3 (Protocol part): [")); Serial.print(rest); Serial.println(F("]"));
        #endif
        }
        // If itemsParsed < 2 (meaning method or URI was missing)
        if (part < 2) {
            Serial.println(F("SWS: Invalid request line format (parts < 2)."));
            _currentMethod = HTTP_ANY;
            _currentPath[0] = '\0';
            _currentUri[0] = '\0';
            return;
        }


        // --- Process parsed parts ---
        if (strcmp(methodStr, "GET") == 0) _currentMethod = HTTP_GET;
        else if (strcmp(methodStr, "POST") == 0) _currentMethod = HTTP_POST;
        else if (strcmp(methodStr, "PUT") == 0) _currentMethod = HTTP_PUT;
        else if (strcmp(methodStr, "DELETE") == 0) _currentMethod = HTTP_DELETE;
        else if (strcmp(methodStr, "PATCH") == 0) _currentMethod = HTTP_PATCH;
        else if (strcmp(methodStr, "OPTIONS") == 0) _currentMethod = HTTP_OPTIONS;
        else _currentMethod = HTTP_ANY;

        strncpy(_currentUri, uriBuf, SWS_MAX_URI_LENGTH - 1);
        _currentUri[SWS_MAX_URI_LENGTH - 1] = '\0';

        char* questionMark = strchr(uriBuf, '?');
        if (questionMark) {
            size_t pathLen = questionMark - uriBuf;
            if (pathLen < SWS_MAX_PATH_LENGTH) {
                strncpy(_currentPath, uriBuf, pathLen);
                _currentPath[pathLen] = '\0';
            }
            else {
                strncpy(_currentPath, uriBuf, SWS_MAX_PATH_LENGTH - 1);
                _currentPath[SWS_MAX_PATH_LENGTH - 1] = '\0';
            #ifdef SWS_ENABLE_DEBUG_PRINTING
                Serial.println(F("SWS_DEBUG_PARSE: Path (with query) too long, truncated."));
            #endif
            }
            parseArguments(questionMark + 1);
        }
        else {
            size_t uriLen = strlen(uriBuf);
            size_t copyLen = (uriLen < SWS_MAX_PATH_LENGTH - 1) ? uriLen : (SWS_MAX_PATH_LENGTH - 1);
            strncpy(_currentPath, uriBuf, copyLen);
            _currentPath[copyLen] = '\0';
            if (uriLen >= SWS_MAX_PATH_LENGTH - 1 && uriLen > copyLen) {
            #ifdef SWS_ENABLE_DEBUG_PRINTING
                Serial.println(F("SWS_DEBUG_PARSE: Path (no query) too long for _currentPath, truncated."));
            #endif
            }
        }

    #ifdef SWS_ENABLE_DEBUG_PRINTING
        Serial.print(F("SWS_DEBUG_PARSE: Final _currentPath: [")); Serial.print(_currentPath); Serial.println(F("]"));
        Serial.print(F("SWS_DEBUG_PARSE: Final _currentUri: [")); Serial.print(_currentUri); Serial.println(F("]"));
        Serial.print(F("SWS_DEBUG_PARSE: Final _currentMethod: ")); Serial.println(_currentMethod);
    #endif
        return;

    }

    void parseArguments(const char* queryString) {
        if (!queryString || queryString[0] == '\0') return;
        _currentArgs.clear();
        char queryCopy[SWS_MAX_QUERY_LENGTH];
        strncpy(queryCopy, queryString, SWS_MAX_QUERY_LENGTH - 1);
        queryCopy[SWS_MAX_QUERY_LENGTH - 1] = '\0';
        char* restPairs = queryCopy;
        char* pairToken;
    #ifdef SWS_ENABLE_DEBUG_PRINTING
        Serial.print(F("SWS_DEBUG_PARSE_ARGS: Parsing query: [")); Serial.print(queryString); Serial.println(F("]"));
    #endif
        while ((pairToken = strtok_r(restPairs, "&", &restPairs)) != NULL) {
            if (_currentArgs.size() >= SWS_MAX_ARGS) {
            #ifdef SWS_ENABLE_DEBUG_PRINTING
                Serial.println(F("SWS_DEBUG_PARSE_ARGS: Max args reached."));
            #endif
                break;
            }
            char* restValue = pairToken;
            char* keyToken = strtok_r(restValue, "=", &restValue);
            if (keyToken) {
                RequestArgument arg;
                strncpy(arg.key, keyToken, SWS_MAX_ARG_KEY_LEN - 1);
                arg.key[SWS_MAX_ARG_KEY_LEN - 1] = '\0';
                if (restValue) {
                    strncpy(arg.value, restValue, SWS_MAX_ARG_VALUE_LEN - 1);
                    arg.value[SWS_MAX_ARG_VALUE_LEN - 1] = '\0';
                }
                else {
                    arg.value[0] = '\0';
                }
                _currentArgs.push_back(arg);
            #ifdef SWS_ENABLE_DEBUG_PRINTING
                Serial.print(F("SWS_DEBUG_PARSE_ARGS: Arg: key=[")); Serial.print(arg.key);
                Serial.print(F("], val=[")); Serial.print(arg.value); Serial.println(F("]"));
            #endif
            }
        }
    }
};

char SimpleWebServer::_empty_string[1];

#endif // WEBSERVER_H
