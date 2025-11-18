// ================================
// Smart Home Dashboard v2.0 (with Login + Enhanced UI)
// ================================

const ESP_BASE = "http://10.126.27.70"; // ganti sesuai IP ESP32
const GAS_THRESHOLD = 1.33;

// Login credentials (in production, use server-side authentication)
const USERS = {
  "admin": "admin123",
  "user": "user123",
  "guest": "guest123"
};

// ================================
// Authentication System
// ================================
const auth = {
  isLoggedIn: false,
  currentUser: null,

  init() {
    // Check if user is already logged in (session storage)
    const savedUser = sessionStorage.getItem('loggedInUser');
    if (savedUser) {
      this.currentUser = savedUser;
      this.isLoggedIn = true;
      this.showDashboard();
    } else {
      this.showLogin();
    }
  },

  login(username, password) {
    if (USERS[username] && USERS[username] === password) {
      this.isLoggedIn = true;
      this.currentUser = username;
      sessionStorage.setItem('loggedInUser', username);
      this.showDashboard();
      return true;
    }
    return false;
  },

  logout() {
    this.isLoggedIn = false;
    this.currentUser = null;
    sessionStorage.removeItem('loggedInUser');
    this.showLogin();
    // Clear intervals
    if (window.statusInterval) {
      clearInterval(window.statusInterval);
    }
  },

  showLogin() {
    document.getElementById('login-screen').style.display = 'flex';
    document.getElementById('dashboard').classList.add('dashboard-hidden');
  },

  showDashboard() {
    document.getElementById('login-screen').style.display = 'none';
    document.getElementById('dashboard').classList.remove('dashboard-hidden');
    document.getElementById('current-user').textContent = this.currentUser;
    // Start dashboard
    initDashboard();
  }
};

// Login form handler
document.getElementById('login-form').addEventListener('submit', (e) => {
  e.preventDefault();
  const username = document.getElementById('username').value;
  const password = document.getElementById('password').value;
  const errorEl = document.getElementById('login-error');

  if (auth.login(username, password)) {
    errorEl.classList.remove('show');
  } else {
    errorEl.textContent = '❌ Username atau password salah!';
    errorEl.classList.add('show');
    setTimeout(() => errorEl.classList.remove('show'), 3000);
  }
});

// Logout button handler
document.getElementById('logout-btn').addEventListener('click', () => {
  if (confirm('Apakah Anda yakin ingin keluar?')) {
    auth.logout();
  }
});

// ================================
// Dashboard Elements
// ================================
const els = {
  conn: document.getElementById("conn"),
  temp: document.getElementById("temp"),
  hum: document.getElementById("hum"),
  gas: document.getElementById("gas"),
  gasStatus: document.getElementById("gas-status"),
  rfidUid: document.getElementById("rfid-uid"),
  rfidAuth: document.getElementById("rfid-auth"),
  rfidLast: document.getElementById("rfid-last"),
  camPreview: document.getElementById("cam-preview"),
  lastPhotoLink: document.getElementById("last-photo-link"),
  relay1: document.getElementById("relay-1"),
  relay2: document.getElementById("relay-2"),
  relay3: document.getElementById("relay-3"),
  relay1Status: document.getElementById("relay-1-status"),
  relay2Status: document.getElementById("relay-2-status"),
  relay3Status: document.getElementById("relay-3-status"),
  openDoor: document.getElementById("open-door"),
  closeDoor: document.getElementById("close-door"),
  servoAngle: document.getElementById("servo-angle"),
  btnSnap: document.getElementById("btn-snap"),
  btnRefresh: document.getElementById("btn-refresh"),
  chartDht: document.getElementById("chart-dht"),
  chartGas: document.getElementById("chart-gas")
};

let lastStatus = null;
const MAX_POINTS = 200;

// Charts
let dhtChart = null;
let gasChart = null;

function makeGradient(ctx, color) {
  const g = ctx.createLinearGradient(0, 0, 0, ctx.canvas.height);
  g.addColorStop(0, color.replace("1)", "0.35)"));
  g.addColorStop(1, color.replace("1)", "0.01)"));
  return g;
}

function initCharts() {
  if (typeof Chart === "undefined") return;

  const dctx = els.chartDht.getContext("2d");
  const tempColor = "rgba(255,160,122,1)";
  const humColor = "rgba(122,209,255,1)";

  dhtChart = new Chart(dctx, {
    type: "line",
    data: {
      labels: [],
      datasets: [
        {
          label: "Temp (°C)",
          data: [],
          borderColor: tempColor,
          backgroundColor: makeGradient(dctx, "rgba(255,160,122,1)"),
          yAxisID: "y",
          tension: 0.25,
          pointRadius: 2,
          fill: true,
        },
        {
          label: "Hum (%)",
          data: [],
          borderColor: humColor,
          backgroundColor: makeGradient(dctx, "rgba(122,209,255,1)"),
          yAxisID: "y1",
          tension: 0.25,
          pointRadius: 2,
          fill: true,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { position: "top" },
        tooltip: { mode: "index", intersect: false }
      },
      interaction: { mode: "index", intersect: false },
      scales: {
        x: { display: true, ticks: { maxTicksLimit: 8 } },
        y: { type: "linear", position: "left", title: { display: true, text: "°C" } },
        y1: { type: "linear", position: "right", grid: { drawOnChartArea: false }, title: { display: true, text: "%" } }
      }
    }
  });

  const gctx = els.chartGas.getContext("2d");
  gasChart = new Chart(gctx, {
    type: "line",
    data: {
      labels: [],
      datasets: [{
        label: "Gas (V)",
        data: [],
        borderColor: "rgba(255,204,0,1)",
        backgroundColor: makeGradient(gctx, "rgba(255,204,0,1)"),
        tension: 0.2,
        fill: true,
        pointRadius: 1
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false },
        annotation: {
          annotations: {
            gasLimit: {
              type: 'line',
              yMin: GAS_THRESHOLD,
              yMax: GAS_THRESHOLD,
              borderColor: 'red',
              borderWidth: 2,
              label: {
                content: 'Threshold ' + GAS_THRESHOLD + 'V',
                enabled: true,
                position: 'end'
              }
            }
          }
        }
      },
      scales: {
        x: { display: true, ticks: { maxTicksLimit: 8 } },
        y: { beginAtZero: true, title: { display: true, text: "Voltage (V)" }, suggestedMax: 5 }
      }
    }
  });
}

initCharts();

// ================================
// fetch history logs from ESP and populate charts initially
// ================================
async function fetchLogs(n = 200) {
  try {
    const res = await fetch(ESP_BASE + "/logs?lines=" + n);
    if (!res.ok) throw new Error("HTTP " + res.status);
    const csv = await res.text();
    // parse CSV lines
    const lines = csv.trim().split("\n");
    if (lines.length <= 1) return; // only header
    // header may exist
    let startIndex = 0;
    if (lines[0].toLowerCase().startsWith("ts_ms")) startIndex = 1;
    const labels = [];
    const tempData = [];
    const humData = [];
    const gasData = [];
    for (let i = startIndex; i < lines.length; i++) {
      const cols = lines[i].split(",");
      if (cols.length < 10) continue;
      const ts = parseInt(cols[0]);
      const t = parseFloat(cols[1]);
      const h = parseFloat(cols[2]);
      const gasv = parseFloat(cols[4]);
      labels.push(new Date(ts).toLocaleTimeString());
      tempData.push(isNaN(t) ? null : t);
      humData.push(isNaN(h) ? null : h);
      gasData.push(isNaN(gasv) ? null : gasv);
    }

    // feed into charts
    if (dhtChart) {
      dhtChart.data.labels = labels;
      dhtChart.data.datasets[0].data = tempData;
      dhtChart.data.datasets[1].data = humData;
      dhtChart.update();
    }
    if (gasChart) {
      gasChart.data.labels = labels;
      gasChart.data.datasets[0].data = gasData;
      gasChart.update();
    }
  } catch (e) {
    console.warn("fetchLogs failed:", e);
  }
}

// ================================
// Ambil data /status periodik dan update UI + charts realtime
// ================================
async function fetchStatus() {
  try {
    const res = await fetch(ESP_BASE + "/status");
    if (!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();
    lastStatus = data;
    updateUI(data);
    els.conn.textContent = "Terhubung";
    els.conn.style.color = "#8ef7a8";
  } catch (e) {
    els.conn.textContent = "Tidak terhubung";
    els.conn.style.color = "#ff8b8b";
    console.warn("Gagal ambil status:", e);
  }
}

function updateUI(data) {
  if (!data) return;
  const temp = data.dht?.temp ?? data.dht?.t ?? null;
  const hum = data.dht?.hum ?? data.dht?.h ?? null;
  els.temp.textContent = temp !== null ? temp : "—";
  els.hum.textContent = hum !== null ? hum : "—";

  const gasVal = data.gas?.value ?? data.gas?.raw ?? null;
  els.gas.textContent = gasVal !== null ? gasVal : "—";
  const gasSafe = data.gas?.safe;
  if (typeof gasSafe === "boolean") {
    els.gasStatus.textContent = gasSafe ? "Aman" : "Bahaya";
    els.gasStatus.style.color = gasSafe ? "var(--success)" : "var(--danger)";
  } else {
    const gv = parseFloat(gasVal);
    if (!isNaN(gv) && gv >= GAS_THRESHOLD) {
      els.gasStatus.textContent = "Bahaya"; els.gasStatus.style.color = "var(--danger)";
    } else { els.gasStatus.textContent = "Aman"; els.gasStatus.style.color = "var(--success)"; }
  }

  els.rfidUid.textContent = data.rfid?.uid ?? "—";
  els.rfidAuth.textContent = data.rfid?.authorized ? "YA" : "TIDAK";
  els.rfidLast.textContent = data.rfid?.time ?? "—";

  els.camPreview.src = data.camera_preview || "";
  if (data.lastPhotoUrl) {
    els.lastPhotoLink.href = data.lastPhotoUrl;
    els.lastPhotoLink.textContent = "lihat foto";
  } else {
    els.lastPhotoLink.href = "#";
    els.lastPhotoLink.textContent = "—";
  }

  if (Array.isArray(data.relays)) {
    els.relay1.checked = !!data.relays[0];
    els.relay2.checked = !!data.relays[1];
    els.relay3.checked = !!data.relays[2];
    // Update status text
    els.relay1Status.textContent = data.relays[0] ? "ON" : "OFF";
    els.relay1Status.style.color = data.relays[0] ? "var(--success)" : "var(--muted)";
    els.relay2Status.textContent = data.relays[1] ? "ON" : "OFF";
    els.relay2Status.style.color = data.relays[1] ? "var(--success)" : "var(--muted)";
    els.relay3Status.textContent = data.relays[2] ? "ON" : "OFF";
    els.relay3Status.style.color = data.relays[2] ? "var(--success)" : "var(--muted)";
  }

  els.servoAngle.textContent = data.servoAngle ?? "—";

  // realtime append to charts
  const nowLabel = new Date().toLocaleTimeString();
  if (dhtChart) {
    dhtChart.data.labels.push(nowLabel);
    dhtChart.data.datasets[0].data.push(temp !== null && !isNaN(parseFloat(temp)) ? parseFloat(temp) : null);
    dhtChart.data.datasets[1].data.push(hum !== null && !isNaN(parseFloat(hum)) ? parseFloat(hum) : null);
    while (dhtChart.data.labels.length > MAX_POINTS) {
      dhtChart.data.labels.shift();
      dhtChart.data.datasets.forEach(ds => ds.data.shift());
    }
    dhtChart.update();
  }
  if (gasChart) {
    const gv = gasVal !== null && !isNaN(parseFloat(gasVal)) ? parseFloat(gasVal) : null;
    gasChart.data.labels.push(nowLabel);
    gasChart.data.datasets[0].data.push(gv);
    while (gasChart.data.labels.length > MAX_POINTS) {
      gasChart.data.labels.shift();
      gasChart.data.datasets.forEach(ds => ds.data.shift());
    }
    gasChart.update();
  }
}

// ================================
// control helpers
// ================================
async function sendControl(payload) {
  try {
    const res = await fetch(ESP_BASE + "/control", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    return await res.json();
  } catch (e) {
    console.error("Gagal kirim kontrol:", e);
    return null;
  }
}

// event listeners
els.relay1.addEventListener("change", () => {
  sendControl({ type: "relay", index: 1, value: els.relay1.checked });
  els.relay1Status.textContent = els.relay1.checked ? "ON" : "OFF";
  els.relay1Status.style.color = els.relay1.checked ? "var(--success)" : "var(--muted)";
});
els.relay2.addEventListener("change", () => {
  sendControl({ type: "relay", index: 2, value: els.relay2.checked });
  els.relay2Status.textContent = els.relay2.checked ? "ON" : "OFF";
  els.relay2Status.style.color = els.relay2.checked ? "var(--success)" : "var(--muted)";
});
els.relay3.addEventListener("change", () => {
  sendControl({ type: "relay", index: 3, value: els.relay3.checked });
  els.relay3Status.textContent = els.relay3.checked ? "ON" : "OFF";
  els.relay3Status.style.color = els.relay3.checked ? "var(--success)" : "var(--muted)";
});

els.openDoor.addEventListener("click", () =>
  sendControl({ type: "servo", angle: 90 })
);
els.closeDoor.addEventListener("click", () =>
  sendControl({ type: "servo", angle: 0 })
);

els.btnSnap.addEventListener("click", async () => {
  els.btnSnap.disabled = true;
  await sendControl({ type: "capture_and_send" });
  setTimeout(() => els.btnSnap.disabled = false, 2500);
});

els.btnRefresh.addEventListener("click", () => fetchStatus());

// Dashboard initialization function
function initDashboard() {
  // boot sequence: fetch history then start polling
  fetchLogs(200).then(() => {
    fetchStatus();
    window.statusInterval = setInterval(fetchStatus, 2500);
  });
}

// Start authentication check on page load
auth.init();