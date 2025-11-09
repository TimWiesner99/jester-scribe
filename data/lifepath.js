// Character counter handler
function handleInput(el) {
  const counter = document.getElementById('char-counter');
  const remaining = 200 - el.value.length;
  counter.textContent = `${remaining} characters left`;
  if (remaining <= 20) {
    counter.style.color = 'var(--warning)';
  } else {
    counter.style.color = 'var(--accent-dark)';
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

    // Show confetti
    confetti({
      particleCount: 100,
      spread: 70,
      origin: { y: 0.6 },
    });

    // Refocus textarea for next message
    textarea.focus();
  });
}

// Handle Enter key press (submit without Shift+Enter)
function handleKeyPress(e) {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault();
    document.getElementById('receipt-form').dispatchEvent(new Event('submit'));
  }
}

// Toggle debug console visibility
function toggleDebug() {
  const console = document.getElementById('debug-console');
  const btn = document.getElementById('debug-toggle');
  if (console.style.display === 'none' || console.style.display === '') {
    console.style.display = 'block';
    btn.textContent = '🔧 Hide';
  } else {
    console.style.display = 'none';
    btn.textContent = '🔧 Debug';
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

// Auto-refresh logs every second
setInterval(updateLogs, 1000);

// Initial log fetch
setTimeout(updateLogs, 100);
