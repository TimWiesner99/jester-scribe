// Character counter handler
function handleInput(el) {
  const counter = document.getElementById('char-counter');
  const remaining = 200 - el.value.length;
  counter.textContent = `${remaining} characters left`;
  counter.classList.toggle('text-red-500', remaining <= 20);
}

// Form submission handler
function handleSubmit(e) {
  e.preventDefault();
  const formData = new FormData(e.target);
  fetch('/submit', {
    method: 'POST',
    body: formData
  }).then(() => {
    const form = document.getElementById('receipt-form');
    const message = document.getElementById('thank-you');
    form.classList.add('hidden');
    message.classList.remove('hidden');
    confetti({
      particleCount: 100,
      spread: 70,
      origin: { y: 0.6 },
    });
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
  console.classList.toggle('hidden');
  btn.textContent = console.classList.contains('hidden') ? '🔧 Debug' : '🔧 Hide';
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
