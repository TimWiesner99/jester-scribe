// Character counter handler
function handleInput(el) {
  const counter = document.getElementById('char-counter');
  const remaining = 200 - el.value.length;
  counter.textContent = `${remaining} characters left`;
  if (remaining <= 20) {
    counter.style.color = 'var(--warning)';
  } else {
    counter.style.color = 'var(--accent-light)';
  }
}

// Form submission handler
function handleSubmit(e) {
  e.preventDefault();
  const formData = new FormData(e.target);
  fetch('/submit', {
    method: 'POST',
    body: formData
  }).then(() => {
    const textarea = document.getElementById('message');
    const message = document.getElementById('thank-you');

    // Clear the textarea
    textarea.value = '';

    // Reset character counter
    handleInput(textarea);

    // Show thank you message temporarily
    message.style.display = 'block';
    setTimeout(() => {
      message.style.display = 'none';
    }, 3000);

    // Refocus textarea for next message
    textarea.focus();
  });
}

// Print daily joke handler
function printDailyJoke() {
  const button = document.getElementById('joke-button');
  const originalText = button.textContent;

  // Show loading state
  button.disabled = true;
  button.style.opacity = '0.6';

  fetch('/printJoke', {
    method: 'POST'
  })
  .then(response => response.text())
  .then(result => {
    console.log('Joke print result:', result);

    // Visual feedback - briefly change emoji
    button.textContent = '✅';
    setTimeout(() => {
      button.textContent = originalText;
      button.disabled = false;
      button.style.opacity = '1';
    }, 1500);
  })
  .catch(error => {
    console.error('Error printing joke:', error);
    button.textContent = '❌';
    setTimeout(() => {
      button.textContent = originalText;
      button.disabled = false;
      button.style.opacity = '1';
    }, 1500);
  });
}

// Toggle settings menu visibility
function toggleSettings() {
  const menu = document.getElementById('settings-menu');
  const btn = event.target;
  if (menu.style.display === 'none' || menu.style.display === '') {
    menu.style.display = 'block';
    btn.textContent = '⚙️ Hide Settings';
  } else {
    menu.style.display = 'none';
    btn.textContent = '⚙️ Settings';
  }
}

// Toggle debug console visibility
function toggleDebug() {
  const console = document.getElementById('debug-console');
  const btn = event.target;
  if (console.style.display === 'none' || console.style.display === '') {
    console.style.display = 'block';
    btn.textContent = '🔧 Hide Debug';
  } else {
    console.style.display = 'none';
    btn.textContent = '🔧 Debug';
  }
}

// Show forget WiFi confirmation modal
function showForgetWifiModal() {
  const modal = document.getElementById('forget-wifi-modal');
  modal.style.display = 'flex';
}

// Close forget WiFi modal
function closeForgetWifiModal() {
  const modal = document.getElementById('forget-wifi-modal');
  modal.style.display = 'none';
}

// Confirm and execute WiFi forget + restart
function confirmForgetWifi() {
  fetch('/forgetWifi', {
    method: 'POST'
  })
  .then(() => {
    alert('WiFi credentials forgotten. Device will restart now.');
    closeForgetWifiModal();
  })
  .catch(error => {
    console.error('Error forgetting WiFi:', error);
    alert('Error: Could not forget WiFi credentials.');
  });
}

// Fetch and display WiFi information
async function updateWifiInfo() {
  try {
    const response = await fetch('/wifiInfo');
    const data = await response.json();

    document.getElementById('wifi-ssid').textContent = data.ssid || 'Unknown';
    document.getElementById('wifi-ip').textContent = data.ip || 'Unknown';
  } catch (e) {
    console.error('Failed to fetch WiFi info:', e);
    document.getElementById('wifi-ssid').textContent = 'Error';
    document.getElementById('wifi-ip').textContent = 'Error';
  }
}

// Update debug logs from server
async function updateLogs() {
  try {
    const response = await fetch('/logs');
    const logs = await response.text();
    const console = document.getElementById('debug-output');
    console.textContent = logs || 'No logs yet...';
    console.scrollTop = console.scrollHeight;
  } catch (e) {
    console.error('Failed to fetch logs:', e);
  }
}

// Load schedule settings on page load
async function loadScheduleSettings() {
  try {
    const response = await fetch('/api/schedule');
    const data = await response.json();
    document.getElementById('print-time').value = data.dailyPrintTime;
    updateLastPrintInfo(data.lastJokePrintDate);
  } catch (error) {
    console.error('Failed to load schedule:', error);
  }
}

// Save print time when button clicked
async function savePrintTime() {
  const time = document.getElementById('print-time').value;
  const formData = new FormData();
  formData.append('dailyPrintTime', time);

  try {
    const response = await fetch('/api/schedule', {
      method: 'POST',
      body: formData
    });

    if (response.ok) {
      alert('Print time saved: ' + time);
    } else {
      alert('Failed to save time');
    }
  } catch (error) {
    console.error('Error saving time:', error);
    alert('Error saving time');
  }
}

// Update last print info display
function updateLastPrintInfo(lastDate) {
  const elem = document.getElementById('last-print-info');
  if (lastDate && lastDate.length > 0) {
    const today = new Date().toISOString().split('T')[0];
    if (lastDate === today) {
      elem.textContent = 'Last printed: Today';
    } else {
      elem.textContent = 'Last printed: ' + lastDate;
    }
  } else {
    elem.textContent = 'Last printed: Never';
  }
}

// Auto-refresh logs every second
setInterval(updateLogs, 1000);

// Initial fetches
setTimeout(updateLogs, 100);
setTimeout(updateWifiInfo, 100);
setTimeout(loadScheduleSettings, 100);
