// Licensed under MIT
// SPDX-License-Identifier: MIT
// ESP8266 WiFi Setup with Captive Portal Example
// © 2025 Lennart Gutjahr

document.addEventListener('DOMContentLoaded', function() {
    const form = document.getElementById('wifi-form');
    const statusDiv = document.getElementById('status');
    const ssidSelect = document.getElementById('ssid-select');
    const ssidManual = document.getElementById('ssid-manual');

    // Fetch SSID list from the device and populate the dropdown
    fetch('/api/scan')
        .then(res => res.json())
        .then(list => {
            ssidSelect.innerHTML = ''; // Remove any existing options
            list.forEach(ssid => {
                const opt = document.createElement('option');
                opt.value = ssid;
                opt.textContent = ssid;
                ssidSelect.appendChild(opt); // Add each SSID as an option
            });
            // Add "Other..." option for manual SSID entry
            const otherOpt = document.createElement('option');
            otherOpt.value = '__other__';
            otherOpt.textContent = 'Other...';
            ssidSelect.appendChild(otherOpt);
            ssidSelect.selectedIndex = 0; // Select the first SSID by default
        })
        .catch(() => {
            // If scan fails, show a message in the dropdown
            ssidSelect.innerHTML = '<option value="">Scan failed</option>';
        });

    // Show or hide the manual SSID input based on selection
    ssidSelect.addEventListener('change', function() {
        if (ssidSelect.value === '__other__') {
            ssidManual.style.display = ''; // Show manual SSID input
            ssidManual.required = true;   // Make it required
        } else {
            ssidManual.style.display = 'none'; // Hide manual SSID input
            ssidManual.required = false;
        }
    });

    // Handle form submission for WiFi credentials
    form.addEventListener('submit', function(e) {
        e.preventDefault(); // Prevent default form submission
        let ssid = ssidSelect.value;
        if (ssid === '__other__') {
            ssid = ssidManual.value; // Use manual SSID if "Other..." is selected
        }
        const password = document.getElementById('password').value;

        statusDiv.textContent = "Sending..."; // Inform user that credentials are being sent

        // Send credentials to the device using a POST request
        fetch('/api/setupWiFi', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'ssid=' + encodeURIComponent(ssid) +
                  '&password=' + encodeURIComponent(password)
        })
        .then(res => res.text())
        .then(msg => {
            statusDiv.textContent = msg; // Show response from the device
        })
        .catch(() => {
            statusDiv.textContent = "Failed to send credentials."; // Show error if request fails
        });
    });
});