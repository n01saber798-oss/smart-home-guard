(() => {
  "use strict";

  const $ = (id) => document.getElementById(id);

  const state = {
    voltage: 230,
    current: 4.2,
    history: [],
    alerts: [],
    critical: false,
    low: false,
    theme: 0,
    lastStatus: undefined,
    connected: false
  };

  const themes = ["", "theme-cyan", "theme-violet"];
  const MQTT_HOST = "test.mosquitto.org";
  const MQTT_PORT = 8080;
  const client = mqtt.connect(
  `ws://${MQTT_HOST}:${MQTT_PORT}/mqtt`
  );
  const MQTT_TOPIC = "smart-home-guard/setif-esp32/telemetry";

  let toastTimer;
  let mqttClient;
  let lastMessageTime = 0;

  function nowTime() {
    return new Date().toLocaleTimeString([], { hour12: false });
  }

  function updateClock() {
    if ($("clock")) $("clock").textContent = nowTime();
  }

  function notify(message) {
    const toast = $("toast");
    if (!toast) return;

    toast.textContent = message;
    toast.classList.add("show");

    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.remove("show"), 2600);
  }

  function updateConnection(connected, message) {
    state.connected = connected;

    if ($("linkValue")) {
      $("linkValue").textContent = connected ? "LIVE MQTT" : "DISCONNECTED";
    }

    if ($("linkState")) {
      $("linkState").textContent = message;
    }

    if ($("lastUpdate") && !connected) {
      $("lastUpdate").textContent = "Waiting for ESP32 data";
    }

    if ($("statusPill") && !connected) {
      $("statusPill").textContent = "● NO LIVE DATA";
    }

    if (connected && $("statusPill")) {
      const status = statusFor(state.voltage, state.current);
      $("statusPill").textContent =
        status === "critical" ? "● CRITICAL" :
        status === "low" ? "● WARNING" : "● NORMAL";
    }
  }

  function statusFor(v, i) {
    if (v > 250 || i > 16) return "critical";
    if (v < 190) return "low";
    return "normal";
  }

  function renderStatus(v, i) {
    const status = statusFor(v, i);

    document.body.classList.remove("alert-critical", "alert-low");

    if (status === "critical") {
      document.body.classList.add("alert-critical");
      $("statusSymbol").textContent = "!";
      $("systemStatus").textContent = "CRITICAL THRESHOLD EXCEEDED";
      $("statusDescription").textContent =
        v > 250 ? "Simulated overvoltage detected." :
        "Simulated overcurrent detected.";
      $("statusPill").textContent = "● CRITICAL";
    } else if (status === "low") {
      document.body.classList.add("alert-low");
      $("statusSymbol").textContent = "!";
      $("systemStatus").textContent = "VOLTAGE BELOW THRESHOLD";
      $("statusDescription").textContent = "Simulated low-voltage warning.";
      $("statusPill").textContent = "● WARNING";
    } else {
      $("statusSymbol").textContent = "✓";
      $("systemStatus").textContent = "ALL SYSTEMS NOMINAL";
      $("statusDescription").textContent =
        "All simulated readings are within configured thresholds.";
      $("statusPill").textContent = "● NORMAL";
    }

    return status;
  }

  function updateMetrics(v, i, publishedPower) {
    state.voltage = Math.max(0, Math.min(300, Number(v) || 0));
    state.current = Math.max(0, Math.min(30, Number(i) || 0));

    const calculatedPower = state.voltage * state.current;
    const power = Number.isFinite(Number(publishedPower))
      ? Number(publishedPower)
      : calculatedPower;

    $("voltageValue").textContent = state.voltage.toFixed(1);
    $("currentValue").textContent = state.current.toFixed(2);
    $("powerValue").textContent = Math.round(power).toLocaleString();

    $("voltageTrack").style.width = `${state.voltage / 300 * 100}%`;
    $("currentTrack").style.width = `${state.current / 30 * 100}%`;
    $("powerTrack").style.width = `${Math.min(100, power / 3000 * 100)}%`;

    const iBad = state.current > 16;

    $("voltageState").textContent =
      state.voltage < 190 ? "Low voltage" :
      state.voltage > 250 ? "High voltage" : "Within range";

    $("currentState").textContent = iBad ? "Overcurrent" : "Normal load";
    $("currentDot").className = `metric-dot ${iBad ? "amber" : "green"}`;
    $("lastUpdate").textContent = `Updated ${nowTime()}`;

    const status = renderStatus(state.voltage, state.current);
    const changed = status !== state.lastStatus;

    if (changed && state.lastStatus !== undefined && status !== "normal") {
      addAlert(status, state.voltage, state.current);
    }

    state.lastStatus = status;

    state.history.push(state.voltage);
    if (state.history.length > 60) state.history.shift();

    drawChart();
  }

  function drawChart() {
    const svg = $("voltageChart");
    if (!svg) return;

    const range = $("chartRange");
    const count = range ? Number(range.value) : 24;
    const data = state.history.slice(-count);

    const w = 720, h = 240, left = 42, right = 10, top = 16, bottom = 25;
    const plotW = w - left - right;
    const plotH = h - top - bottom;

    const y = val => top + (300 - val) / 300 * plotH;
    const x = idx => left +
      (data.length < 2 ? plotW / 2 : idx * plotW / (data.length - 1));

    let markup = "";

    [0, 100, 190, 250, 300].forEach(val => {
      const yy = y(val);
      const color = val === 190 || val === 250 ? "#9b6d36" : "#1c3024";
      const dash = val === 190 || val === 250
        ? ' stroke-dasharray="5 5"' : "";

      markup += `<line x1="${left}" y1="${yy}" x2="${w-right}" y2="${yy}" stroke="${color}" stroke-width="1"${dash}/>`;
      markup += `<text x="2" y="${yy+4}" fill="#6f8878" font-size="11" font-family="DM Mono, monospace">${val}</text>`;
    });

    if (data.length) {
      const points = data.map((v, idx) => `${x(idx)},${y(v)}`).join(" ");
      const area =
        `M ${x(0)} ${h-bottom} L ${points.replaceAll(" ", " L ")} L ${x(data.length-1)} ${h-bottom} Z`;

      markup += `<path d="${area}" fill="#40e879" opacity=".07"/>`;
      markup += `<polyline points="${points}" fill="none" stroke="#a3ffbf" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round"/>`;

      const last = data[data.length - 1];
      markup += `<circle cx="${x(data.length-1)}" cy="${y(last)}" r="4.5" fill="#a3ffbf" stroke="#102116" stroke-width="2"/>`;
    }

    svg.innerHTML = markup;
  }

  function addAlert(kind, v, i) {
    const label = kind === "critical"
      ? "CRITICAL THRESHOLD"
      : "LOW VOLTAGE WARNING";

    const detail = kind === "critical"
      ? `Voltage ${v.toFixed(1)} V · Current ${i.toFixed(2)} A`
      : `Voltage ${v.toFixed(1)} V`;

    state.alerts.unshift({ kind, label, detail, time: nowTime() });
    state.alerts = state.alerts.slice(0, 8);

    renderAlerts();
    notify(`${label}: live MQTT event`);
  }

  function renderAlerts() {
    const list = $("alertList");
    if (!list) return;

    if (!state.alerts.length) {
      list.innerHTML =
        '<div class="empty-state"><span>✓</span><p>No alerts recorded yet.</p><small>Alerts will appear when live readings cross a threshold.</small></div>';
      return;
    }

    list.innerHTML = state.alerts.map(a =>
      `<div class="alert-entry ${a.kind === "low" ? "warning" : ""}">
        <span class="alert-icon">${a.kind === "low" ? "!" : "⚠"}</span>
        <div><strong>${a.label}</strong><small>${a.detail}</small></div>
        <time>${a.time}</time>
      </div>`
    ).join("");
  }

  function connectMQTT() {
    if (typeof mqtt === "undefined") {
      updateConnection(false, "MQTT library failed to load");
      notify("Could not load MQTT library");
      return;
    }

    const clientId = "SmartGuardWeb-" +
      Math.random().toString(16).slice(2, 10);

    const url = `ws://${MQTT_HOST}:${MQTT_PORT}/mqtt`;

    updateConnection(false, "Connecting to public MQTT broker...");

    mqttClient = mqtt.connect(url, {
      clientId,
      clean: true,
      connectTimeout: 10000,
      reconnectPeriod: 3000,
      keepalive: 30
    });

    mqttClient.on("connect", () => {
      updateConnection(true, "Connected; subscribing to telemetry");
      notify("MQTT broker connected");

      mqttClient.subscribe(MQTT_TOPIC, { qos: 0 }, error => {
        if (error) {
          updateConnection(false, "Subscription failed");
          notify("MQTT subscription failed");
          return;
        }

        updateConnection(true, "Subscribed; waiting for ESP32 readings");
      });
    });

    mqttClient.on("message", (topic, message) => {
      if (topic !== MQTT_TOPIC) return;

      try {
        const data = JSON.parse(message.toString());

        if (typeof data.voltage !== "number" ||
            typeof data.current !== "number") {
          console.warn("MQTT message has missing/invalid voltage or current", data);
          return;
        }

        lastMessageTime = Date.now();

        updateConnection(true, "Live ESP32 telemetry received");

        updateMetrics(
          data.voltage,
          data.current,
          data.power
        );

        console.log("[MQTT] Received:", data);
      } catch (error) {
        console.error("[MQTT] Invalid JSON:", error);
      }
    });

    mqttClient.on("reconnect", () => {
      updateConnection(false, "Reconnecting to MQTT broker...");
    });

    mqttClient.on("offline", () => {
      updateConnection(false, "MQTT client offline");
    });

    mqttClient.on("error", error => {
      console.error("[MQTT] Connection error:", error);
      updateConnection(false, "Connection error; retrying");
    });
  }

  function simulateAlert() {
    notify("Live mode is active; change the Wokwi potentiometers to test alerts.");
  }

  function reset() {
    state.history = [];
    state.alerts = [];
    state.lastStatus = undefined;
    renderAlerts();
    drawChart();
    notify("Alert history cleared");
  }

  $("simulateAlert").addEventListener("click", simulateAlert);
  $("clearAlerts").addEventListener("click", () => {
    state.alerts = [];
    renderAlerts();
    notify("Alert log cleared");
  });
  $("resetDemo").addEventListener("click", reset);
  $("chartRange").addEventListener("change", drawChart);

  $("showPower").addEventListener("change", e => {
    document.querySelector(".power-icon").closest(".metric-card").hidden =
      !e.target.checked;
  });

  $("showChart").addEventListener("change", e => {
    document.querySelector(".chart-panel").hidden = !e.target.checked;
  });

  $("compactMode").addEventListener("change", e => {
    document.body.classList.toggle("compact", e.target.checked);
  });

  $("themeToggle").addEventListener("click", () => {
    for (const theme of themes) {
      if (theme) document.body.classList.remove(theme);
    }

    state.theme = (state.theme + 1) % themes.length;

    if (themes[state.theme]) document.body.classList.add(themes[state.theme]);

    notify(
      state.theme === 0 ? "Neon green theme selected" :
      state.theme === 1 ? "Cyber cyan theme selected" :
      "Cyber violet theme selected"
    );
  });

  document.querySelectorAll(".nav-link").forEach(link =>
    link.addEventListener("click", () => {
      document.querySelectorAll(".nav-link").forEach(item =>
        item.classList.remove("active")
      );
      link.classList.add("active");
    })
  );

  // Initialize without demo readings.
  for (const theme of themes) {
    if (theme) document.body.classList.remove(theme);
  }

  updateClock();
  setInterval(updateClock, 1000);

  renderAlerts();
  updateConnection(false, "Waiting for MQTT connection");
  connectMQTT();

  // Warn if no telemetry has arrived recently.
  setInterval(() => {
    if (state.connected && lastMessageTime &&
        Date.now() - lastMessageTime > 10000) {
      updateConnection(false, "Connected, but no recent ESP32 messages");
    }
  }, 3000);
})();
