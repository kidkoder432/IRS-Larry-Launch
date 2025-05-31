const char PROGMEM index_html[] = R"indexhtml(<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Launch Pad Control</title>
    <style>
        :root {
            --bg-color: #0e0e0e;
            --text-color: #f5f5f5;
            --status-text-color: #aaaaaa;
            --launch-bg: #28a745;
            --launch-hover-bg: #218838;
            --abort-bg: #dc3545;
            --abort-hover-bg: #c82333;
            --clamp-bg: #007bff;
            --clamp-hover-bg: #0069d9;
            --button-text-color: white;
            --container-bg: #1a1a1a;
            /* Slightly lighter than body for contrast */
        }

        body {
            background-color: var(--bg-color);
            color: var(--text-color);
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif, monospace;
            /* Added more common fallbacks */
            text-align: center;
            margin: 0;
            padding: 20px;
            /* Add padding to body for small screens */
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
            /* Ensure body takes at least full viewport height */
            box-sizing: border-box;
        }

        .container {
            background-color: var(--container-bg);
            padding: 20px;
            border-radius: 12px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.3);
            width: 100%;
            max-width: 500px;
            /* Max width for larger screens */
            box-sizing: border-box;
        }

        h1 {
            font-size: clamp(1.5em, 5vw, 2em);
            /* Responsive font size */
            margin-bottom: 30px;
        }

        .controls-grid {
            display: grid;
            grid-template-columns: 1fr;
            /* Single column by default */
            gap: 20px;
            /* Space between grid items */
            margin-bottom: 20px;
        }

        .control-group {
            display: flex;
            flex-direction: column;
            /* Stack button and status vertically */
            align-items: center;
            gap: 10px;
        }

        .button-row {
            display: flex;
            flex-wrap: wrap;
            /* Allow buttons to wrap on smaller screens */
            justify-content: center;
            gap: 15px;
            /* Space between buttons in a row */
        }

        .button {
            padding: 15px 25px;
            font-size: clamp(1em, 4vw, 1.2em);
            /* Responsive font size */
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: background-color 0.3s ease, transform 0.1s ease;
            color: var(--button-text-color);
            min-width: 150px;
            /* Minimum width for buttons */
            flex-grow: 1;
            /* Allow buttons to grow if space allows */
            max-width: 220px;
            /* Prevent buttons from becoming too wide */
        }

        .button:active {
            transform: translateY(1px);
        }

        .launch {
            background-color: var(--launch-bg);
        }

        .launch:hover {
            background-color: var(--launch-hover-bg);
        }

        .abort {
            background-color: var(--abort-bg);
        }

        .abort:hover {
            background-color: var(--abort-hover-bg);
        }

        .clamp {
            background-color: var(--clamp-bg);
        }

        .clamp:hover {
            background-color: var(--clamp-hover-bg);
        }

        .status-display {
            font-size: clamp(1em, 3.5vw, 1.1em);
            /* Responsive font size */
            color: var(--text-color);
            background-color: rgba(255, 255, 255, 0.05);
            padding: 10px 15px;
            border-radius: 6px;
            min-height: 1.5em;
            /* Ensure it has some height even when empty */
            width: 100%;
            max-width: 300px;
            box-sizing: border-box;
            word-wrap: break-word;
            /* Wrap long status messages */
        }

        #overall-status {
            /* Renamed from #status to avoid conflict with window.status */
            margin-top: 30px;
            font-size: clamp(1em, 4vw, 1.2em);
            color: var(--status-text-color);
            padding: 10px;
            background-color: var(--bg-color);
            /* Match body background */
            border-radius: 6px;
            border: 1px solid var(--container-bg);
            /* Subtle border */
        }

        /* Media query for slightly larger screens to put clamp buttons side-by-side */
        @media (min-width: 400px) {
            .control-group .button-row {
                /* For groups like clamps, allow them to be side-by-side if they fit */
            }
        }

        @media (min-width: 600px) {
            .controls-grid {
                /* Could change to 2 columns if desired for wider layouts, but 1 is often fine for mobile-first */
            }
        }
    </style>
</head>

<body>
    <div class="container">
        <h1>🚀 Launch Pad Control</h1>

        <div class="controls-grid">
            <div class="control-group">
                <div class="button-row">
                    <button class="button launch" onclick="triggerLaunch()">Launch</button>
                    <button class="button abort" onclick="triggerAbort()">Abort</button>
                </div>
                <div id="launch-status" class="status-display">Ready to launch</div>
            </div>

            <div class="control-group">
                <div class="button-row">
                    <button class="button clamp" onclick="triggerOpenClamps()">Open Clamps</button>
                    <button class="button clamp" onclick="triggerCloseClamps()">Close Clamps</button>
                </div>
                <div id="clamp-status" class="status-display">Clamps Closed</div>
            </div>
        </div>

        <div id="overall-status">System idle.</div>
    </div>

    <script>
        let countdownInterval;

        function sendCommand(endpoint, statusElementId, successMessage, immediateStatus) {
            const statusEl = document.getElementById("overall-status");
            const specificStatusEl = document.getElementById(statusElementId);

            if (immediateStatus && specificStatusEl) {
                specificStatusEl.innerText = immediateStatus;
            }
            if (statusEl) statusEl.innerText = "Sending: " + endpoint.substring(1) + "...";

            fetch(endpoint)
                .then(response => {
                    if (!response.ok) throw new Error("Network: " + response.statusText);
                    return response.text();
                })
                .then(text => {
                    if (statusEl) statusEl.innerText = "Server: " + text;
                    if (successMessage && specificStatusEl) {
                        // Delay final specific status to allow server confirmation to show first
                        // or if no server confirmation is expected from this command.
                        // For now, let's assume the server response is the primary feedback.
                        // If server response is just "OK", then successMessage is useful.
                        specificStatusEl.innerText = successMessage; 
                    }
                })
                .catch(error => {
                    if (statusEl) statusEl.innerText = "Error: " + error.message;
                    if (specificStatusEl) {
                        specificStatusEl.innerText = "Action failed"; // Or revert to previous state
                    }
                });
        }

        function triggerLaunch() {
            const elem = document.getElementById("launch-status");
            if (!elem) return;

            let countdown = 10; // Integer countdown
            elem.innerText = "🚀 Launching in " + countdown + "s";

            clearInterval(countdownInterval);
            countdownInterval = setInterval(() => {
                countdown--;
                if (countdown > 0) {
                    elem.innerText = "🚀 Launching in " + countdown + "s";
                } else {
                    clearInterval(countdownInterval);
                    elem.innerText = "🚀 IGNITION!";
                    sendCommand("/launch", "launch-status", "Launched!");
                    // Visual feedback after command is sent
                    setTimeout(() => {
                        if (elem.innerText.includes("IGNITION")) { // Check if not aborted
                            elem.innerText = "Launched (Command Sent)";
                        }
                    }, 1500); // Give some time for server response to potentially update overall-status
                }
            }, 1000); // 1-second interval
        }

        function triggerAbort() {
            clearInterval(countdownInterval);
            const launchStatusEl = document.getElementById("launch-status");
            if (launchStatusEl) launchStatusEl.innerText = "ABORTING...";

            sendCommand("/abort", "launch-status", "Aborted!");
            setTimeout(() => {
                if (launchStatusEl && launchStatusEl.innerText.includes("ABORTING")) {
                    launchStatusEl.innerText = "Aborted!";
                }
            }, 1500);
        }

        function triggerCloseClamps() {
            sendCommand("/clamps/close", "clamp-status", "Clamps Closed", "Closing Clamps...");
            // Update specific status after a delay, assuming command was sent
            setTimeout(() => {
                document.getElementById("clamp-status").innerText = "Clamps Closed";
            }, 1000);
        }

        function triggerOpenClamps() {
            sendCommand("/clamps/open", "clamp-status", "Clamps Open", "Opening Clamps...");
            setTimeout(() => {
                document.getElementById("clamp-status").innerText = "Clamps Open";
            }, 1000);
        }

    </script>
</body>

</html>)indexhtml";