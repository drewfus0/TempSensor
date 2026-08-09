#pragma once

#include <pgmspace.h>

namespace web {

const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>TempSensor Dashboard</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500;600&display=swap" rel="stylesheet">
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/dygraph/2.1.0/dygraph.min.css" />
  <script src="https://cdnjs.cloudflare.com/ajax/libs/dygraph/2.1.0/dygraph.min.js"></script>

  <style>
    :root {
      --bg: #090d16;
      --card-bg: rgba(21, 27, 43, 0.75);
      --card-border: rgba(255, 255, 255, 0.08);
      --text: #f3f4f6;
      --text-sub: #9ca3af;
      --accent: #06b6d4;
      --accent-hover: #0891b2;
      --success: #10b981;
      --error: #ef4444;
      --warning: #f59e0b;
      --font-main: 'Outfit', sans-serif;
      --font-mono: 'JetBrains Mono', monospace;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      padding: 0 20px 20px 20px;
      font-family: var(--font-main);
      background: linear-gradient(135deg, #070a13 0%, #0f172a 100%);
      color: var(--text);
      min-height: 100vh;
      -webkit-font-smoothing: antialiased;
    }
    .wrap { max-width: 1280px; margin: 0 auto; }
    
    .head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
      padding-bottom: 8px;
      border-bottom: 1px solid var(--card-border);
      flex-wrap: wrap;
      gap: 16px;
    }
    .title-area {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .heartbeat {
      width: 10px;
      height: 10px;
      background-color: var(--success);
      border-radius: 50%;
      box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7);
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7);
      }
      70% {
        transform: scale(1);
        box-shadow: 0 0 0 8px rgba(16, 185, 129, 0);
      }
      100% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(16, 185, 129, 0);
      }
    }
    h1 {
      margin: 0;
      font-size: 1.4rem;
      font-weight: 700;
      letter-spacing: -0.02em;
      background: linear-gradient(to right, #ffffff, #9ca3af);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .pills { display: flex; gap: 8px; flex-wrap: wrap; }
    .pill {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 999px;
      padding: 6px 14px;
      font-size: 0.8rem;
      font-weight: 500;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    
    .grid-main {
      display: grid;
      grid-template-columns: 1fr 1.6fr;
      gap: 20px;
      margin-bottom: 20px;
    }
    .grid-bottom {
      display: grid;
      grid-template-columns: 1fr 1fr 1.2fr;
      gap: 20px;
    }
    
    .card {
      background: var(--card-bg);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 20px;
      box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.3), 0 4px 6px -4px rgba(0, 0, 0, 0.3);
      transition: transform 0.2s ease, box-shadow 0.2s ease;
    }
    .card h2 {
      margin: 0 0 16px 0;
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text);
      text-transform: uppercase;
      letter-spacing: 0.05em;
      border-left: 3px solid var(--accent);
      padding-left: 8px;
    }
    
    .readings {
      display: grid;
      gap: 12px;
    }
    .reading-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px;
      background: rgba(255, 255, 255, 0.015);
      border-radius: 8px;
      border: 1px solid rgba(255, 255, 255, 0.03);
    }
    .reading-label {
      font-size: 0.85rem;
      color: var(--text-sub);
    }
    .reading-val {
      font-family: var(--font-mono);
      font-size: 1.3rem;
      font-weight: 600;
      color: #fff;
    }
    .reading-val.temp { color: #f43f5e; text-shadow: 0 0 10px rgba(244, 63, 94, 0.25); }
    .reading-val.hum { color: #06b6d4; text-shadow: 0 0 10px rgba(6, 182, 212, 0.25); }
    .reading-val.pres { color: #10b981; text-shadow: 0 0 10px rgba(16, 185, 129, 0.25); }
    
    .live-meta {
      margin-top: 12px;
      font-size: 0.75rem;
      color: var(--text-sub);
      text-align: right;
    }
    
    .health-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }
    .health-item {
      background: rgba(255, 255, 255, 0.01);
      border: 1px solid rgba(255, 255, 255, 0.03);
      padding: 10px;
      border-radius: 8px;
    }
    .health-item.full-width {
      grid-column: span 2;
    }
    .health-lbl { font-size: 0.72rem; color: var(--text-sub); margin-bottom: 4px; }
    .health-val { font-family: var(--font-mono); font-size: 0.9rem; font-weight: 600; }
    
    .battery-icon-container {
      position: relative;
      width: 28px;
      height: 14px;
      display: flex;
      align-items: center;
    }
    .battery-body {
      width: 25px;
      height: 14px;
      border: 1.5px solid var(--text-sub);
      border-radius: 3px;
      padding: 1px;
      display: flex;
    }
    .battery-fill {
      height: 100%;
      width: 0%;
      background: var(--success);
      border-radius: 1px;
      transition: width 0.3s ease, background-color 0.3s ease;
    }
    .battery-tip {
      width: 3px;
      height: 6px;
      background: var(--text-sub);
      border-radius: 0 1px 1px 0;
    }
    .charging-bolt {
      position: absolute;
      top: -2px;
      left: 10px;
      color: #fbbf24;
      font-size: 10px;
      font-weight: bold;
      text-shadow: 0 0 2px rgba(0,0,0,0.8);
    }
    
    .progress-bar-container {
      width: 100%;
      height: 4px;
      background: rgba(255, 255, 255, 0.05);
      border-radius: 2px;
      margin-top: 6px;
      overflow: hidden;
    }
    .progress-bar {
      height: 100%;
      background: var(--accent);
      border-radius: 2px;
      width: 0%;
      transition: width 0.3s ease;
    }
    
    .controls-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-bottom: 12px;
    }
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 6px;
    }
    .form-group.full-width {
      grid-column: span 2;
    }
    input, select {
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid var(--card-border);
      border-radius: 6px;
      padding: 8px 10px;
      font-family: var(--font-main);
      color: #fff;
      font-size: 0.85rem;
      transition: border-color 0.2s, box-shadow 0.2s;
    }
    input:focus, select:focus {
      outline: none;
      border-color: var(--accent);
      box-shadow: 0 0 0 2px rgba(6, 182, 212, 0.15);
    }
    input:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .btn-group {
      display: flex;
      gap: 8px;
      margin-top: 10px;
      flex-wrap: wrap;
    }
    button {
      width: auto;
      background: var(--accent);
      color: #fff;
      border: none;
      border-radius: 6px;
      padding: 10px;
      font-family: var(--font-main);
      font-weight: 600;
      font-size: 0.85rem;
      cursor: pointer;
      transition: background 0.2s, transform 0.1s, box-shadow 0.2s;
    }
    button:hover {
      background: var(--accent-hover);
      box-shadow: 0 0 10px rgba(6, 182, 212, 0.3);
    }
    button:active {
      transform: scale(0.98);
    }
    button:disabled {
      opacity: 0.5;
      cursor: not-allowed;
      box-shadow: none;
    }
    button.btn-secondary {
      flex: 1;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--card-border);
      color: var(--text);
    }
    button.btn-secondary:hover {
      background: rgba(255, 255, 255, 0.08);
      box-shadow: none;
    }
    
    .alert-banner {
      padding: 10px;
      border-radius: 6px;
      font-size: 0.78rem;
      margin-top: 10px;
      display: none;
      line-height: 1.3;
    }
    .alert-banner.success {
      background: rgba(16, 185, 129, 0.12);
      border: 1px solid rgba(16, 185, 129, 0.25);
      color: var(--success);
    }
    .alert-banner.warning {
      background: rgba(245, 158, 11, 0.12);
      border: 1px solid rgba(245, 158, 11, 0.25);
      color: var(--warning);
    }
    .alert-banner.error {
      background: rgba(239, 68, 68, 0.12);
      border: 1px solid rgba(239, 68, 68, 0.25);
      color: var(--error);
    }

    .chart-container {
      position: relative;
      width: 100%;
      height: 384px;
      margin-top: 8px;
      border-radius: 6px;
      background: rgba(0, 0, 0, 0.25);
      overflow: hidden;
      border: 1px solid var(--card-border);
      padding: 10px;
    }
    .chart-legend-bar {
      display: flex;
      flex-wrap: wrap;
      gap: 16px;
      align-items: center;
      justify-content: flex-start;
      padding: 8px 12px;
      background: rgba(21, 27, 43, 0.6);
      border: 1px solid var(--card-border);
      border-radius: 6px;
      margin-top: 12px;
      margin-bottom: 4px;
      font-size: 0.82rem;
      min-height: 36px;
      font-family: var(--font-main);
    }
    .chart-legend-bar .dygraph-legend {
      position: static !important;
      background: transparent !important;
      border: none !important;
      padding: 0 !important;
      display: flex !important;
      flex-wrap: wrap !important;
      gap: 16px !important;
      width: 100% !important;
    }
    .dygraph-axis-label {
      color: var(--text-sub) !important;
      font-family: var(--font-main) !important;
      font-size: 11px !important;
    }
    .modal-overlay {
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(5, 8, 15, 0.85);
      backdrop-filter: blur(8px);
      z-index: 1000;
      display: flex;
      justify-content: center;
      align-items: center;
      opacity: 0;
      pointer-events: none;
      transition: opacity 0.3s ease;
    }
    .modal-overlay.active {
      opacity: 1;
      pointer-events: auto;
    }
    .modal-content {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 30px;
      width: 90%;
      max-width: 450px;
      text-align: center;
      box-shadow: 0 20px 40px rgba(0, 0, 0, 0.5);
    }
    .modal-title {
      font-size: 1.25rem;
      font-weight: 600;
      margin-bottom: 8px;
    }
    .modal-subtitle {
      font-size: 0.88rem;
      color: var(--text-sub);
      margin-bottom: 20px;
    }
    .modal-progress-container {
      background: rgba(255, 255, 255, 0.05);
      border-radius: 999px;
      height: 8px;
      width: 100%;
      overflow: hidden;
      margin-bottom: 12px;
    }
    .modal-progress-bar {
      background: var(--accent);
      height: 100%;
      width: 0%;
      transition: width 0.1s ease;
    }
    .modal-percent {
      font-family: var(--font-mono);
      font-size: 0.88rem;
      font-weight: 500;
    }

    
    .scroll-area {
      margin: 0;
      padding: 10px;
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid var(--card-border);
      border-radius: 8px;
      height: 220px;
      overflow-y: auto;
      font-family: var(--font-mono);
      font-size: 0.78rem;
      line-height: 1.4;
    }
    #sdtree {
      white-space: pre;
      color: #a5f3fc;
    }
    .logs-list {
      list-style-type: none;
      padding: 0;
      margin: 0;
    }
    .logs-list li {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 8px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.03);
    }
    .logs-list a {
      color: var(--accent);
      text-decoration: none;
      font-weight: 600;
    }
    .logs-list a:hover {
      text-decoration: underline;
    }
    
    .timeline-container {
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .timeline-item {
      display: flex;
      gap: 10px;
      border-left: 2px solid rgba(255, 255, 255, 0.05);
      padding-left: 10px;
      position: relative;
    }
    .timeline-item::before {
      content: '';
      position: absolute;
      left: -5px;
      top: 4px;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--text-sub);
    }
    .timeline-item.ntp_reestablished::before {
      background: var(--success);
      box-shadow: 0 0 6px var(--success);
    }
    .timeline-item.warning::before {
      background: var(--warning);
    }
    .timeline-item.error::before {
      background: var(--error);
    }
    
    .timeline-time {
      font-size: 0.68rem;
      color: var(--text-sub);
      min-width: 55px;
    }
    .timeline-content {
      font-size: 0.75rem;
    }
    
    @media (max-width: 1024px) {
      .grid-main, .grid-bottom {
        grid-template-columns: 1fr;
      }
    }
    
    /* Modal / Progress Overlay */
    .modal-overlay {
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(9, 13, 22, 0.85);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      display: flex;
      justify-content: center;
      align-items: center;
      z-index: 9999;
      opacity: 0;
      pointer-events: none;
      transition: opacity 0.3s ease;
    }
    .modal-overlay.active {
      opacity: 1;
      pointer-events: auto;
    }
    .modal-content {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 16px;
      padding: 28px;
      width: 90%;
      max-width: 420px;
      box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5), 0 10px 10px -5px rgba(0, 0, 0, 0.5);
      text-align: center;
      transform: scale(0.9);
      transition: transform 0.3s ease;
    }
    .modal-overlay.active .modal-content {
      transform: scale(1);
    }
    .modal-title {
      font-size: 1.15rem;
      font-weight: 600;
      margin-bottom: 8px;
      letter-spacing: 0.02em;
      color: #fff;
    }
    .modal-subtitle {
      font-size: 0.85rem;
      color: var(--text-sub);
      margin-bottom: 20px;
    }
    .modal-progress-container {
      width: 100%;
      height: 8px;
      background: rgba(255, 255, 255, 0.05);
      border-radius: 4px;
      overflow: hidden;
      margin-bottom: 12px;
    }
    .modal-progress-bar {
      height: 100%;
      background: linear-gradient(90deg, var(--accent) 0%, #3b82f6 100%);
      width: 0%;
      border-radius: 4px;
      transition: width 0.1s linear;
    }
     .modal-percent {
      font-family: var(--font-mono);
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--accent);
    }
    
    .header-container {
      position: sticky;
      top: 0;
      z-index: 100;
      background: rgba(9, 13, 22, 0.95);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border-bottom: 1px solid var(--card-border);
      padding: 20px 20px 0 20px;
      margin-bottom: 24px;
    }
    .countdown-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      padding: 8px 16px;
      background: rgba(255, 255, 255, 0.02);
      border-radius: 6px;
      margin-bottom: 16px;
      font-size: 0.75rem;
      color: var(--text-sub);
      flex-wrap: wrap;
      border: 1px solid var(--card-border);
    }
    .countdown-items-wrap {
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
      align-items: center;
    }
    .countdown-item {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 6px;
      padding: 3px 8px;
      border-radius: 4px;
      border: 1px solid var(--card-border);
      background: rgba(255, 255, 255, 0.03);
      transition: background 0.2s ease, border-color 0.2s ease;
      min-width: 130px;
    }
    .countdown-item.fetching {
      background: rgba(6, 182, 212, 0.15);
      border-color: rgba(6, 182, 212, 0.4);
    }
    .countdown-item.fetching .countdown-val {
      color: #f59e0b;
    }
    .countdown-val {
      font-weight: 600;
      color: var(--accent);
      font-family: var(--font-mono);
      display: inline-block;
      min-width: 65px;
      text-align: center;
      font-variant-numeric: tabular-nums;
    }
    .btn-refresh-sm {
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid var(--card-border);
      color: var(--text-sub);
      width: 20px;
      height: 20px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      border-radius: 4px;
      cursor: pointer;
      font-size: 0.72rem;
      font-weight: bold;
      transition: all 0.2s ease;
      padding: 0;
      line-height: 1;
    }
    .btn-refresh-sm:hover {
      background: var(--accent);
      color: var(--text);
      border-color: var(--accent);
    }
    .btn-refresh-sm:active {
      transform: scale(0.9);
    }

    .btn-refresh {
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--card-border);
      color: var(--text-sub);
      width: 28px;
      height: 28px;
      display: flex;
      align-items: center;
      justify-content: center;
      border-radius: 6px;
      cursor: pointer;
      font-size: 1.1rem;
      font-weight: bold;
      transition: all 0.2s ease;
      padding: 0;
    }
    .btn-refresh:hover {
      background: var(--accent);
      color: var(--text);
      border-color: var(--accent);
    }
    .btn-refresh:active {
      transform: scale(0.92);
    }

    /* Tab System Styles */
    .tab-bar {
      display: flex;
      gap: 12px;
      margin-bottom: 0px;
      border-bottom: none;
      padding-bottom: 8px;
    }
    .tab-btn {
      background: none;
      border: none;
      color: var(--text-sub);
      font-family: var(--font-main);
      font-size: 1rem;
      font-weight: 500;
      cursor: pointer;
      padding: 6px 12px;
      border-bottom: 2px solid transparent;
      transition: color 0.2s ease, border-color 0.2s ease;
    }
    .tab-btn:hover {
      color: var(--text);
    }
    .tab-btn.active {
      color: var(--accent);
      border-bottom: 2px solid var(--accent);
    }
    .tab-content {
      display: none;
      animation: fadeIn 0.3s ease;
    }
    .tab-content.active {
      display: block;
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(4px); }
      to { opacity: 1; transform: translateY(0); }
    }
    
    /* Config Form Styles */
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 6px;
      margin-bottom: 16px;
    }
    .form-group label {
      font-size: 0.85rem;
      color: var(--text-sub);
      font-weight: 500;
    }
    .form-control {
      background: rgba(0, 0, 0, 0.3);
      border: 1px solid var(--card-border);
      border-radius: 6px;
      padding: 10px;
      color: var(--text);
      font-family: var(--font-main);
      font-size: 0.9rem;
      width: 100%;
      transition: border-color 0.2s;
    }
    .form-control:focus {
      outline: none;
      border-color: var(--accent);
    }
  </style>
</head>
<body>
  <div class='modal-overlay' id='progressModal'>
    <div class='modal-content'>
      <div class='modal-title'>Retrieving History Data</div>
      <div class='modal-subtitle' id='modalStatus'>Scanning SD card logs...</div>
      <div class='modal-progress-container'>
        <div class='modal-progress-bar' id='modalProgressBar'></div>
      </div>
      <div class='modal-percent' id='modalProgressPercent'>0%</div>
    </div>
  </div>


  <div class='header-container'>
    <div class='wrap'>
      <div class='head'>
        <div class='title-area'>
          <div class='heartbeat' id='heartbeat'></div>
          <h1>TEMPSENSOR LOCAL</h1>
        </div>
        <div class='pills'>
          <div class='pill'><span style='color: var(--text-sub)'>WiFi:</span> <span id='pillWifi'>-</span></div>
          <div class='pill'><span style='color: var(--text-sub)'>SD:</span> <span id='pillSd'>-</span></div>
          <div class='pill'><span style='color: var(--text-sub)'>NTP:</span> <span id='pillNtp'>-</span></div>
          <div class='pill'><span style='color: var(--text-sub)'>Sensor:</span> <span id='pillSensor'>-</span></div>
          <div class='pill'><span style='color: var(--text-sub)'>IP:</span> <span id='pillIp'>-</span></div>
          <div class='pill'><span style='color: var(--text-sub)'>Updated:</span> <span id='pillRef'>-</span></div>
        </div>
      </div>

      <div class='countdown-bar'>
        <div class='countdown-items-wrap'>
          <div class='countdown-item' id='itemLive'>
            <span>Live:</span>
            <span class='countdown-val' id='cntLive'>--s</span>
            <button class='btn-refresh-sm' onclick='forceRefreshTask("live")' title='Force fetch live telemetry'>⟳</button>
          </div>
          <div class='countdown-item' id='itemHealth'>
            <span>Health:</span>
            <span class='countdown-val' id='cntHealth'>--s</span>
            <button class='btn-refresh-sm' onclick='forceRefreshTask("health")' title='Force fetch health status'>⟳</button>
          </div>
          <div class='countdown-item' id='itemEvents'>
            <span>Events:</span>
            <span class='countdown-val' id='cntEvents'>--s</span>
            <button class='btn-refresh-sm' onclick='forceRefreshTask("events")' title='Force fetch events list'>⟳</button>
          </div>
          <div class='countdown-item' id='itemSdTree'>
            <span>SD Tree:</span>
            <span class='countdown-val' id='cntSdTree'>--s</span>
            <button class='btn-refresh-sm' onclick='forceRefreshTask("sdtree")' title='Force fetch SD tree'>⟳</button>
          </div>
          <div class='countdown-item' id='itemBattery'>
            <span>Battery:</span>
            <span class='countdown-val' id='cntBattery'>--s</span>
            <button class='btn-refresh-sm' onclick='forceRefreshTask("battery")' title='Force fetch battery history'>⟳</button>
          </div>
        </div>

        <div class='auto-toggle-group' style='display: flex; align-items: center; gap: 8px;'>
          <span id='cntStatus' style='font-size: 0.7rem; font-weight: 600; color: var(--text-sub); font-family: var(--font-mono);'>READY</span>
          <label style='display: flex; align-items: center; gap: 6px; cursor: pointer; user-select: none; font-size: 0.75rem; background: rgba(255,255,255,0.04); padding: 4px 8px; border-radius: 4px; border: 1px solid var(--card-border);'>
            <input type='checkbox' id='chkAutoUpdate' onchange='toggleAutoUpdate(this.checked)' checked>
            <span style='font-weight: 500; color: var(--text);'>Auto-Update</span>
          </label>
        </div>
      </div>

      <div class='tab-bar'>
        <button class='tab-btn active' onclick="showTab('dashboard')" id='tab-dashboard'>Dashboard</button>
        <button class='tab-btn' onclick="showTab('files')" id='tab-files'>File Manager</button>
        <button class='tab-btn' onclick="showTab('settings')" id='tab-settings'>Settings</button>
      </div>
    </div>
  </div>

  <div class='wrap'>

    <div id='content-dashboard' class='tab-content active'>
      <div class='grid-main'>
        <div style='display: flex; flex-direction: column; gap: 20px;'>
        <section class='card'>
          <div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;'>
            <h2 style='margin: 0;'>Live Snapshot</h2>
            <button class='btn-refresh' onclick='forceRefreshTask("live")' title='Force update live telemetry'>⟳</button>
          </div>
          <div class='readings'>
            <div class='reading-row'>
              <div class='reading-label'>Temperature</div>
              <div class='reading-val temp' id='valTemp'>--.- °C</div>
            </div>
            <div class='reading-row'>
              <div class='reading-label'>Humidity</div>
              <div class='reading-val hum' id='valHum'>--.- %</div>
            </div>
            <div class='reading-row'>
              <div class='reading-label'>Pressure</div>
              <div class='reading-val pres' id='valPres'>----.- hPa</div>
            </div>
          </div>
          <div class='live-meta'>
            <span id='liveTime'>Timestamp: --:--:--</span> Quality: <span id='liveQuality' style='font-weight:600;'>-</span>
          </div>
        </section>

        <section class='card'>
          <div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;'>
            <h2 style='margin: 0;'>Health Snapshot</h2>
            <button class='btn-refresh' onclick='forceRefreshTask("health")' title='Force update system health'>⟳</button>
          </div>
          <div class='health-grid'>
            <div class='health-item full-width'>
              <div class='health-lbl'>Uptime</div>
              <div class='health-val' id='healthUptime'>-</div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Free Heap</div>
              <div class='health-val' id='healthHeap'>-</div>
              <div class='progress-bar-container'>
                <div class='progress-bar' id='barHeap'></div>
              </div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Max Block</div>
              <div class='health-val' id='healthBlock'>-</div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Queue Depth</div>
              <div class='health-val' id='healthQueue'>-</div>
              <div class='progress-bar-container'>
                <div class='progress-bar' id='barQueue' style='background: var(--warning);'></div>
              </div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Dropped Samples</div>
              <div class='health-val' id='healthDropped'>-</div>
            </div>
            <div class='health-item full-width' style='border-top: 1px solid rgba(255,255,255,0.06); padding-top: 12px; margin-top: 4px; background: transparent; border-left: none; border-right: none; border-bottom: none; border-radius: 0;'>
              <div class='health-lbl' style='display:flex; align-items:center; justify-content:space-between;'>
                <span>Battery Status</span>
                <span id='healthBatStatus' style='font-size:0.65rem; color:var(--text-sub);'>-</span>
              </div>
              <div style='display:flex; align-items:center; gap:10px; margin-top:6px;'>
                <div id='batteryIcon' class='battery-icon-container'>
                  <div class='battery-body'>
                    <div id='batteryLevelBar' class='battery-fill'></div>
                  </div>
                  <div class='battery-tip'></div>
                </div>
                <div class='health-val' id='healthBatText' style='margin:0;'>-</div>
              </div>
            </div>
          </div>
        </section>

        <section class='card'>
          <h2>Manual Diagnostics</h2>
          <div class='btn-group' style='display: flex; gap: 10px; margin-top: 10px;'>
            <button class='btn-secondary' id='btnFlush' title='Flush RAM buffer to SD card' style='flex: 1; padding: 10px; background: rgba(255,255,255,0.05); color: var(--text); border: 1px solid var(--card-border); border-radius: 6px; cursor: pointer; font-weight: 500;'>Force Flush</button>
            <button class='btn-secondary' id='btnNtp' title='Force immediate NTP time sync attempt' style='flex: 1; padding: 10px; background: rgba(255,255,255,0.05); color: var(--text); border: 1px solid var(--card-border); border-radius: 6px; cursor: pointer; font-weight: 500;'>Sync NTP</button>
          </div>
          <div class='alert-banner' id='actionAlert' style='margin-top: 10px; display: none;'></div>
        </section>

        <section class='card'>
          <div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;'>
            <h2 style='margin: 0;'>Event Timeline</h2>
            <button class='btn-refresh' onclick='forceRefreshTask("events")' title='Force update events list'>⟳</button>
          </div>

          <form onsubmit='createCustomEvent(event)' style='display: flex; flex-direction: column; gap: 8px; margin-bottom: 12px; padding: 10px; background: rgba(0,0,0,0.2); border: 1px solid var(--card-border); border-radius: 6px;'>
            <div style='display: flex; align-items: center; gap: 6px;'>
              <input type='text' id='newEventMsg' placeholder='Event name (e.g. Watered Plants)' class='form-control' style='flex: 1; min-width: 0;' required maxlength='128'>
              <select id='newEventCat' class='form-control' style='padding: 6px 8px; font-size: 0.75rem; width: auto; flex-shrink: 0;'>
                <option value='1' selected>Custom (Amber)</option>
                <option value='4'>Fault (Red)</option>
                <option value='5'>Sync (Green)</option>
                <option value='6'>Power (Purple)</option>
                <option value='0'>System (Gray)</option>
              </select>
              <button type='submit' class='btn' style='padding: 6px 14px; font-size: 0.8rem; width: auto; flex-shrink: 0; white-space: nowrap;'>+ Add</button>
            </div>
            <div style='display: flex; align-items: center; gap: 6px;'>
              <label for='newEventTs' style='font-size: 0.75rem; color: var(--text-sub); white-space: nowrap;'>Backdate (Optional):</label>
              <input type='datetime-local' id='newEventTs' class='form-control' style='padding: 2px 6px; font-size: 0.75rem; flex: 1;'>
            </div>
          </form>

          <div class='scroll-area'>
            <div class='timeline-container' id='eventsTimeline'>
              <div style='color: var(--text-sub);'>loading...</div>
            </div>
          </div>
        </section>

      </div>

      <div style='display: flex; flex-direction: column; gap: 20px;'>
        <section class='card' style='flex: 1; display: flex; flex-direction: column;'>
          <div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;'>
            <h2 style='margin: 0;'>Historical Chart</h2>
            <label style='display: flex; align-items: center; gap: 6px; font-size: 0.8rem; color: var(--text-sub); cursor: pointer;'>
              <input type='checkbox' id='chkShowEvents' onchange='if(historyLoaded) loadHistory()' checked> Show Events on Graph
            </label>
          </div>
          <div class='controls-grid' style='grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 10px; margin-bottom: 8px;'>
            <div class='form-group'>
              <label for='rangeMode'>Filter Mode</label>
              <select id='rangeMode' onchange='onRangeModeChanged()'>
                <option value='hours' selected>Hours Into Past</option>
                <option value='custom'>Custom Time Window</option>
              </select>
            </div>
            
            <div class='form-group' id='hoursGroup'>
              <label for='hoursPast'>Hours Past</label>
              <input type='number' id='hoursPast' class='form-control' min='1' max='168' value='3'>
            </div>

            <div class='form-group'>
              <label for='binMode'>Bin Size</label>
              <select id='binMode' onchange='loadHistory()'>
                <option value='auto' selected>Auto</option>
                <option value='60'>1 min</option>
                <option value='180'>3 min</option>
                <option value='300'>5 min</option>
                <option value='600'>10 min</option>
                <option value='1800'>30 min</option>
                <option value='3600'>1 hour</option>
              </select>
            </div>
          </div>
          
          <div class='controls-grid' id='customRangeGroup' style='grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 8px; display: none;'>
            <div class='form-group'>
              <label for='start'>Start Date/Time</label>
              <input id='start' type='datetime-local'>
            </div>
            <div class='form-group'>
              <label for='end'>End Date/Time</label>
              <input id='end' type='datetime-local'>
            </div>
          </div>

          <div style='display: flex; justify-content: flex-end; gap: 10px; margin-bottom: 8px;'>
            <button id='btnDownloadCsv' style='width: auto; padding: 10px 20px; background-color: var(--bg-card); border: 1px solid var(--border); color: var(--text-main); cursor: pointer;'>Download Filtered CSV</button>
            <button id='btnLoad' style='width: auto; padding: 10px 20px; cursor: pointer;'>Load Graph Data</button>
          </div>
          
          <div class='alert-banner' id='historyAlert' style='margin-bottom: 10px;'></div>

          <div id='historyLegend' class='chart-legend-bar'></div>
          <div class='chart-container'>
            <div id='chart' style='width: 100%; height: 100%;'></div>
          </div>
          <div class='live-meta' id='historyMeta' style='margin-top: 12px; text-align: left;'>
            No history data loaded.
          </div>
        </section>

        <section class='card'>
          <div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;'>
            <h2 style='margin: 0;'>Battery History & Prediction</h2>
            <div style='display: flex; gap: 8px; align-items: center;'>
              <button class='btn-refresh' onclick='forceRefreshTask("battery")' title='Force update battery history'>⟳</button>
              <button class='btn' id='btnDownloadBatCsv' style='padding: 6px 12px; font-size: 12px; margin: 0;'>Download CSV</button>
            </div>
          </div>
          <div id='batChartLegend' class='chart-legend-bar'></div>
          <div class='chart-container' style='height: 250px;'>
            <div id='batChart' style='width: 100%; height: 100%;'></div>
          </div>
          <div class='live-meta' id='batChartMeta' style='margin-top: 12px; text-align: left;'>
            No battery history loaded.
          </div>
        </section>
      </div> <!-- End right column -->
    </div> <!-- End grid-main -->
  </div> <!-- End content-dashboard -->

    <div id='content-files' class='tab-content'>
      <section class='card'>
        <div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;'>
          <h2 style='margin: 0;'>SD File Explorer</h2>
          <button class='btn-refresh' onclick='forceRefreshTask("sdtree")' title='Force update file explorer'>⟳</button>
        </div>
        <div id='sdtree' style='background: rgba(0, 0, 0, 0.25); border: 1px solid var(--card-border); border-radius: 8px; padding: 10px; font-family: var(--font-mono); font-size: 0.78rem; line-height: 1.4; color: #a5f3fc; white-space: normal;'>loading...</div>
      </section>
    </div> <!-- End content-files -->

    <div id='content-settings' class='tab-content'>
      <section class='card' style='max-width: 600px; margin: 0 auto;'>
        <h2>System Configuration</h2>
        <form id='settingsForm' onsubmit='saveSettings(event)' style='display: flex; flex-direction: column; gap: 16px;'>
          
          <div class='form-group'>
            <label for='cfgWifiSsid'>Wi-Fi SSID</label>
            <input type='text' id='cfgWifiSsid' class='form-control' required>
          </div>
          
          <div class='form-group'>
            <label for='cfgWifiPassword'>Wi-Fi Password</label>
            <input type='password' id='cfgWifiPassword' class='form-control' placeholder='••••••••'>
            <span style='font-size: 0.75rem; color: var(--text-sub);'>Leave blank to keep current password</span>
          </div>
          
          <div class='form-group'>
            <label for='cfgHostname'>Hostname</label>
            <input type='text' id='cfgHostname' class='form-control' required>
          </div>
          
          <div class='form-group'>
            <label for='cfgTimezone'>Timezone Configuration</label>
            <input type='text' id='cfgTimezone' class='form-control' required>
            <span style='font-size: 0.75rem; color: var(--text-sub);'>e.g., AEST-10AEDT,M10.1.0,M4.1.0/3 (Melbourne)</span>
          </div>
          
          <div style='display: grid; grid-template-columns: 1fr 1fr; gap: 16px;'>
            <div class='form-group'>
              <label for='cfgSampleInterval'>Sample Interval (ms)</label>
              <input type='number' id='cfgSampleInterval' class='form-control' min='500' max='60000' required>
            </div>
            <div class='form-group'>
              <label for='cfgLogFlushInterval'>Log Flush Interval (ms)</label>
              <input type='number' id='cfgLogFlushInterval' class='form-control' min='5000' max='3600000' required>
            </div>
          </div>

          <div class='form-group'>
            <label for='cfgDisplayRefreshInterval'>Display Refresh Interval (ms)</label>
            <input type='number' id='cfgDisplayRefreshInterval' class='form-control' min='500' max='60000' required>
          </div>

          <div style='display: grid; grid-template-columns: 1fr 1fr; gap: 16px;'>
            <div class='form-group'>
              <label for='cfgLatitude'>Latitude (decimal degrees)</label>
              <input type='number' id='cfgLatitude' class='form-control' step='0.000001' min='-90' max='90' required>
            </div>
            <div class='form-group'>
              <label for='cfgLongitude'>Longitude (decimal degrees)</label>
              <input type='number' id='cfgLongitude' class='form-control' step='0.000001' min='-180' max='180' required>
            </div>
          </div>

          <button type='submit' class='btn' style='margin-top: 12px; padding: 12px; background: var(--accent); color: var(--text); border: none; border-radius: 6px; font-weight: 600; cursor: pointer; transition: background 0.2s;'>Save & Apply Settings</button>
        </form>
      </section>

      <section class='card' style='max-width: 600px; margin: 20px auto 0 auto;'>
        <h2>Firmware Update (OTA)</h2>
        <div class='form-group'>
          <label for='otaFile'>Select Firmware Binary (.bin)</label>
          <input type='file' id='otaFile' accept='.bin' style='display: none;'>
          <div id='otaDragDrop' style='border: 2px dashed var(--card-border); padding: 20px; text-align: center; border-radius: 6px; cursor: pointer; background: rgba(255,255,255,0.02); transition: all 0.2s; margin-top: 8px;'>
            <span id='otaDragText'>Drag & drop or click to select file</span>
          </div>
        </div>
        <div id='otaProgressContainer' style='display: none; margin-top: 15px;'>
          <div style='display: flex; justify-content: space-between; margin-bottom: 5px; font-size: 13px;'>
            <span id='otaStatus'>Uploading...</span>
            <span id='otaPercent'>0%</span>
          </div>
          <div style='background: rgba(255,255,255,0.1); height: 10px; border-radius: 5px; overflow: hidden;'>
            <div id='otaProgressBar' style='background: var(--accent); width: 0%; height: 100%; transition: width 0.1s; border-radius: 5px;'></div>
          </div>
        </div>
        <button id='btnStartOta' class='btn' style='margin-top: 15px; width: 100%; padding: 12px; background: var(--accent); color: var(--text); border: none; border-radius: 6px; font-weight: 600; cursor: pointer;' disabled>Flash Firmware</button>
        <div class='alert-banner' id='otaAlert' style='margin-top: 10px; display: none;'></div>
      </section>

      <section class='card' style='max-width: 600px; margin: 20px auto 0 auto;'>
        <h2>Web Telemetry Intervals</h2>
        <div style='display: flex; flex-direction: column; gap: 16px;'>
          <div class='form-group'>
            <label for='uiIntervalLive'>Live Telemetry Interval (seconds)</label>
            <input type='number' id='uiIntervalLive' class='form-control' min='1' max='300' value='15'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalHealth'>System Health Interval (seconds)</label>
            <input type='number' id='uiIntervalHealth' class='form-control' min='5' max='600' value='20'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalEvents'>Events List Interval (seconds)</label>
            <input type='number' id='uiIntervalEvents' class='form-control' min='5' max='3600' value='60'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalSdTree'>SD Explorer Interval (seconds)</label>
            <input type='number' id='uiIntervalSdTree' class='form-control' min='10' max='3600' value='60'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalBattery'>Battery History Interval (seconds)</label>
            <input type='number' id='uiIntervalBattery' class='form-control' min='10' max='3600' value='60'>
          </div>
          <button id='btnSaveUiIntervals' class='btn' style='margin-top: 12px; padding: 12px; background: var(--accent); color: var(--text); border: none; border-radius: 6px; font-weight: 600; cursor: pointer; transition: background 0.2s;'>Update Fetch Rates</button>
        </div>
      </section>
    </div> <!-- End content-settings -->
  </div> <!-- End wrap -->

  <script>
    let dygraphInstance = null;
    let batDygraphInstance = null;
    let historyDataset = [];
    let loadedBatteryData = [];
    let historyLoaded = false;
    let isLoadingHistory = false;
    let cachedHistoryMap = new Map();
    let fetchedChunkKeys = new Set();

    const $ = (id) => document.getElementById(id);
    

    let tasks = [];
    let schedulerTimer = null;
    let isFetching = false;
    let activeTaskName = null;
    let autoUpdateEnabled = localStorage.getItem('autoUpdateEnabled') !== 'false';

    function initAutoUpdateUI() {
      const chk = $('chkAutoUpdate');
      if (chk) chk.checked = autoUpdateEnabled;
      updateCountdowns();
    }

    function toggleAutoUpdate(enabled) {
      autoUpdateEnabled = enabled;
      localStorage.setItem('autoUpdateEnabled', enabled);
      updateCountdowns();
    }

    function initTasks() {
      const now = Date.now();
      tasks = [
        { name: 'live', intervalKey: 'uiIntervalLive', action: loadLive, lastRun: now, priority: 5 },
        { name: 'health', intervalKey: 'uiIntervalHealth', action: loadHealth, lastRun: now, priority: 4 },
        { name: 'events', intervalKey: 'uiIntervalEvents', action: loadEvents, lastRun: now, priority: 3 },
        { name: 'battery', intervalKey: 'uiIntervalBattery', action: loadBatteryHistory, lastRun: now, priority: 2 },
        { name: 'sdtree', intervalKey: 'uiIntervalSdTree', action: loadSdTree, lastRun: now, priority: 1 }
      ];
    }

    function getTaskDefault(key) {
      if (key === 'uiIntervalLive') return 10;
      if (key === 'uiIntervalHealth') return 20;
      if (key === 'uiIntervalEvents') return 30;
      if (key === 'uiIntervalSdTree') return 120;
      if (key === 'uiIntervalBattery') return 50;
      return 60;
    }

    function updateCountdowns() {
      const now = Date.now();
      if (tasks.length === 0) return;
      
      for (const t of tasks) {
        const intervalSec = parseInt($(t.intervalKey).value) || getTaskDefault(t.intervalKey);
        const intervalMs = intervalSec * 1000;
        const elapsed = now - t.lastRun;
        const diffMs = intervalMs - elapsed;
        const rem = diffMs >= 0 ? Math.ceil(diffMs / 1000) : Math.floor(diffMs / 1000);
        
        let displayId = '';
        let itemId = '';
        if (t.name === 'live') { displayId = 'cntLive'; itemId = 'itemLive'; }
        else if (t.name === 'health') { displayId = 'cntHealth'; itemId = 'itemHealth'; }
        else if (t.name === 'events') { displayId = 'cntEvents'; itemId = 'itemEvents'; }
        else if (t.name === 'sdtree') { displayId = 'cntSdTree'; itemId = 'itemSdTree'; }
        else if (t.name === 'battery') { displayId = 'cntBattery'; itemId = 'itemBattery'; }
        
        const itemEl = $(itemId);
        if (activeTaskName === t.name) {
          if (displayId) $(displayId).textContent = 'fetching...';
          if (itemEl) itemEl.classList.add('fetching');
        } else {
          if (itemEl) itemEl.classList.remove('fetching');
          if (displayId) {
            if (!autoUpdateEnabled) {
              $(displayId).textContent = 'paused';
            } else {
              $(displayId).textContent = rem + 's';
            }
          }
        }
      }

      const statusEl = $('cntStatus');
      if (statusEl) {
        if (activeTaskName) {
          statusEl.textContent = '⚡ FETCHING (' + activeTaskName.toUpperCase() + ')';
          statusEl.style.color = '#f59e0b';
        } else if (!autoUpdateEnabled) {
          statusEl.textContent = 'PAUSED';
          statusEl.style.color = 'var(--warning)';
        } else {
          statusEl.textContent = 'READY';
          statusEl.style.color = 'var(--text-sub)';
        }
      }
    }

    setInterval(updateCountdowns, 1000);
    function loadUiIntervals() {
      $('uiIntervalLive').value = localStorage.getItem('uiIntervalLive') || 10;
      $('uiIntervalHealth').value = localStorage.getItem('uiIntervalHealth') || 20;
      $('uiIntervalEvents').value = localStorage.getItem('uiIntervalEvents') || 30;
      $('uiIntervalSdTree').value = localStorage.getItem('uiIntervalSdTree') || 120;
      $('uiIntervalBattery').value = localStorage.getItem('uiIntervalBattery') || 50;
    }

    function saveUiIntervals() {
      localStorage.setItem('uiIntervalLive', $('uiIntervalLive').value);
      localStorage.setItem('uiIntervalHealth', $('uiIntervalHealth').value);
      localStorage.setItem('uiIntervalEvents', $('uiIntervalEvents').value);
      localStorage.setItem('uiIntervalSdTree', $('uiIntervalSdTree').value);
      localStorage.setItem('uiIntervalBattery', $('uiIntervalBattery').value);
      
      const now = Date.now();
      for (const t of tasks) {
        t.lastRun = now;
      }
      
      startScheduler();
      alert("Telemetry fetch rates updated successfully!");
    }

    function startScheduler(staggered = false) {
      if (schedulerTimer) clearInterval(schedulerTimer);
      
      const now = Date.now();
      if (staggered && tasks.length > 0) {
        for (const t of tasks) {
          const intervalSec = parseInt($(t.intervalKey).value) || getTaskDefault(t.intervalKey);
          let offsetSec = 0;
          if (t.name === 'live') offsetSec = 0;
          else if (t.name === 'health') offsetSec = 2;
          else if (t.name === 'events') offsetSec = 10;
          else if (t.name === 'battery') offsetSec = 20;
          else if (t.name === 'sdtree') offsetSec = 30;
          t.lastRun = now - (intervalSec - offsetSec) * 1000;
        }
      }
      
      schedulerTimer = setInterval(schedulerTick, 5000);
    }

    function setRefreshButtonsState(disabled, spinningBtn = null) {
      const allRefreshBtns = document.querySelectorAll('.btn-refresh, .btn-refresh-sm');
      allRefreshBtns.forEach(b => {
        b.disabled = disabled;
        if (disabled) {
          if (b === spinningBtn) {
            b.style.opacity = '0.7';
            b.style.cursor = 'wait';
          } else {
            b.style.opacity = '0.3';
            b.style.cursor = 'not-allowed';
          }
        } else {
          b.style.transform = 'none';
          b.style.opacity = '1';
          b.style.cursor = 'pointer';
        }
      });
    }

    async function schedulerTick() {
      if (!autoUpdateEnabled || isLoadingHistory || isFetching) return;
      
      const now = Date.now();
      const dueTasks = [];

      for (const t of tasks) {
        const intervalSec = parseInt($(t.intervalKey).value) || getTaskDefault(t.intervalKey);
        const intervalMs = intervalSec * 1000;
        const overdueMs = (now - t.lastRun) - intervalMs;
        if (overdueMs >= 0) {
          t.overdueMs = overdueMs;
          dueTasks.push(t);
        }
      }

      if (dueTasks.length === 0) return;

      dueTasks.sort((a, b) => {
        const diff = b.overdueMs - a.overdueMs;
        if (Math.abs(diff) < 1000) {
          return b.priority - a.priority;
        }
        return diff;
      });

      const taskToRun = dueTasks[0];
      taskToRun.lastRun = now;
      
      isFetching = true;
      activeTaskName = taskToRun.name;
      setRefreshButtonsState(true);
      updateCountdowns();
      
      try {
        await taskToRun.action();
      } catch (err) {
        console.error(`Scheduler failed to run ${taskToRun.name}:`, err);
      } finally {
        isFetching = false;
        activeTaskName = null;
        setRefreshButtonsState(false);
        updateCountdowns();
      }
    }

    async function forceRefreshTask(name, btnTarget = null) {
      if (isLoadingHistory || isFetching) return;
      const t = tasks.find(x => x.name === name);
      if (!t) return;
      
      const btn = btnTarget || (window.event ? window.event.currentTarget : null);
      isFetching = true;
      activeTaskName = t.name;
      setRefreshButtonsState(true, btn);
      updateCountdowns();
      
      if (btn) {
        btn.style.transition = 'transform 0.4s ease';
        btn.style.transform = 'rotate(180deg)';
      }

      t.lastRun = Date.now();
      try {
        await t.action();
      } catch (err) {
        console.error(`Force refresh failed for ${name}:`, err);
      } finally {
        setTimeout(() => {
          isFetching = false;
          activeTaskName = null;
          setRefreshButtonsState(false);
          updateCountdowns();
        }, 300);
      }
    }

    let lastLive = null;
    let lastHealth = null;

    async function fetchJson(url) {
      const res = await fetch(url, { cache: 'no-store' });
      if (!res.ok) throw new Error(url + ' -> HTTP ' + res.status);
      return await res.json();
    }

    async function fetchText(url) {
      const res = await fetch(url, { cache: 'no-store' });
      if (!res.ok) throw new Error(url + ' -> HTTP ' + res.status);
      return await res.text();
    }

    function stampNow() {
      const d = new Date();
      const p = (n) => String(n).padStart(2, '0');
      return p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());
    }

    function fmtLocalTs(date) {
      const p = (n) => String(n).padStart(2, '0');
      return date.getFullYear() + '-' + p(date.getMonth() + 1) + '-' + p(date.getDate()) +
        ' ' + p(date.getHours()) + ':' + p(date.getMinutes()) + ':' + p(date.getSeconds());
    }

    function updatePills(live, health) {
      const wifi = (health && health.wifi_connected) ? 'UP' : 'DOWN';
      const sd = (health && health.sd_healthy) ? 'OK' : 'BAD';
      const ntp = (health && health.ntp_synced) ? 'SYNC' : 'EST';

      $('pillWifi').textContent = wifi;
      $('pillWifi').style.color = (health && health.wifi_connected) ? 'var(--success)' : 'var(--error)';
      
      $('pillSd').textContent = sd;
      $('pillSd').style.color = (health && health.sd_healthy) ? 'var(--success)' : 'var(--error)';
      
      $('pillNtp').textContent = ntp;
      $('pillNtp').style.color = (health && health.ntp_synced) ? 'var(--success)' : 'var(--warning)';

      let sensorText = 'OFFLINE';
      let sensorColor = 'var(--error)';
      if (health && health.has_health) {
        if (health.sensor_healthy) {
          sensorText = health.sensor_simulated ? 'SIM' : 'OK';
          sensorColor = health.sensor_simulated ? 'var(--warning)' : 'var(--success)';
        } else if (health.sensor_status) {
          sensorText = health.sensor_status;
        }
      }
      const pillSensor = $('pillSensor');
      if (pillSensor) {
        pillSensor.textContent = sensorText;
        pillSensor.style.color = sensorColor;
      }

      let ip = '-';
      if (live && live.has_sample && typeof live.ip === 'string') {
        ip = live.ip;
      } else if (health && typeof health.ip === 'string') {
        ip = health.ip;
      }
      $('pillIp').textContent = ip;
      $('pillRef').textContent = stampNow();
    }

    async function loadSdTree() {
      if (isLoadingHistory) return;
      try {
        const data = await fetchJson('/api/sd-tree');
        const files = Array.isArray(data.files) ? data.files : [];
        const container = $('sdtree');
        if (files.length === 0) {
          container.innerHTML = '<div style="padding: 10px; color: var(--text-sub);">SD card is empty.</div>';
          return;
        }

        let html = '';
        for (const f of files) {
          const path = f.path;
          const name = f.name;
          const isDir = f.is_dir;
          
          let cleanPath = path;
          if (cleanPath.endsWith('/')) cleanPath = cleanPath.slice(0, -1);
          const slashCount = (cleanPath.match(/\//g) || []).length;
          const indent = (slashCount - 1) * 20;

          const sizeText = isDir ? '' : ` (${(f.size / 1024).toFixed(2)} KB)`;
          const icon = isDir ? '📁' : '📄';
          const downloadUrl = '/api/logs/download?file=' + encodeURIComponent(path);

          html += `<div style="display: flex; justify-content: space-between; align-items: center; padding: 6px 8px; margin-left: ${indent}px; border-bottom: 1px solid rgba(255,255,255,0.03);">
                     <div style="display: flex; align-items: center; gap: 8px;">
                       <span>${icon}</span>
                       ${isDir ? `<span style="font-weight: 500;">${name}</span>` : `<a href="${downloadUrl}" style="color: var(--accent); text-decoration: none;">${name}</a>`}
                       <span style="color: var(--text-sub); font-size: 0.75rem;">${sizeText}</span>
                     </div>
                     <div style="display: flex; gap: 8px;">
                       <button onclick="renameFile('${path}', '${name}')" style="padding: 2px 6px; font-size: 0.75rem; background: rgba(255,255,255,0.08); color: var(--text); border: 1px solid var(--card-border); border-radius: 4px; cursor: pointer;">Rename</button>
                       <button onclick="deleteFile('${path}', '${name}')" style="padding: 2px 6px; font-size: 0.75rem; background: rgba(239,68,68,0.2); border: 1px solid var(--error); color: var(--error); border-radius: 4px; cursor: pointer;">Delete</button>
                     </div>
                   </div>`;
        }
        container.innerHTML = html;
        lastSdTreeFetchTime = Date.now();
      } catch (err) {
        $('sdtree').innerHTML = '<div style="padding: 10px; color: var(--error);">' + err.message + '</div>';
      }
    }

    let lastLoadedEvents = [];

    const EVENT_CATEGORIES = {
      0: { name: 'System', code: 'SYS', color: '#9ca3af', bg: 'rgba(156,163,175,0.18)' },
      1: { name: 'Custom', code: 'WEB', color: '#f59e0b', bg: 'rgba(245,158,11,0.18)' },
      2: { name: 'Button A', code: 'BTN-A', color: '#f43f5e', bg: 'rgba(244,63,94,0.18)' },
      3: { name: 'Button B', code: 'BTN-B', color: '#06b6d4', bg: 'rgba(6,182,212,0.18)' },
      4: { name: 'Fault', code: 'FLT', color: '#ef4444', bg: 'rgba(239,68,68,0.18)' },
      5: { name: 'Sync', code: 'NTP', color: '#10b981', bg: 'rgba(16,185,129,0.18)' },
      6: { name: 'Power', code: 'PWR', color: '#a855f7', bg: 'rgba(168,85,247,0.18)' }
    };

    async function createCustomEvent(e) {
      if (e) e.preventDefault();
      const msgInput = $('newEventMsg');
      const tsInput = $('newEventTs');
      const catSelect = $('newEventCat');
      const msg = msgInput.value.trim();
      if (!msg) return;

      let tsStr = '';
      if (tsInput.value) {
        tsStr = tsInput.value.replace('T', ' ') + ':00';
      }

      const catVal = parseInt(catSelect ? catSelect.value : '1');

      try {
        const res = await fetch('/api/events/create', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ event: msg, ts: tsStr, category: catVal })
        });
        if (!res.ok) throw new Error('HTTP ' + res.status);
        msgInput.value = '';
        tsInput.value = '';
        await loadEvents();
        if (historyLoaded) loadHistory();
      } catch (err) {
        alert('Failed to add event: ' + err.message);
      }
    }

    async function editEventRecord(oldTs, currentName) {
      const newName = prompt('Edit Event Name:', currentName);
      if (!newName || newName.trim() === '' || newName.trim() === currentName) return;

      try {
        const res = await fetch('/api/events/update', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ oldTs: oldTs, oldEvent: currentName, event: newName.trim() })
        });
        if (!res.ok) throw new Error('HTTP ' + res.status);
        await loadEvents();
        if (historyLoaded) loadHistory();
      } catch (err) {
        alert('Failed to update event: ' + err.message);
      }
    }

    async function loadEvents() {
      if (isLoadingHistory) return;
      try {
        const res = await fetch('/api/events?limit=30', { cache: 'no-store' });
        const timeline = $('eventsTimeline');
        if (!res.ok) throw new Error('HTTP status ' + res.status);

        const contentType = res.headers.get('content-type') || '';
        let list = [];

        if (contentType.indexOf('octet-stream') >= 0) {
          const recordSize = parseInt(res.headers.get('X-Record-Size') || '136');
          const arrayBuffer = await res.arrayBuffer();
          const view = new DataView(arrayBuffer);
          const decoder = new TextDecoder('utf-8');
          const totalRecords = Math.floor(arrayBuffer.byteLength / recordSize);

          for (let r = 0; r < totalRecords; r++) {
            const offset = r * recordSize;
            const epochTime = view.getUint32(offset, true);
            const quality = view.getUint8(offset + 4);
            const category = view.getUint8(offset + 5);
            if (quality === 2 || epochTime === 0) continue;

            const rawMsgBytes = new Uint8Array(arrayBuffer, offset + 6, 128);
            let msgEnd = rawMsgBytes.indexOf(0);
            if (msgEnd === -1) msgEnd = 128;
            const message = decoder.decode(rawMsgBytes.subarray(0, msgEnd));

            const dt = new Date(epochTime * 1000);
            const timePart = fmtLocalTs(dt);
            list.push({ ts: timePart, quality: quality === 0 ? 'ntp' : 'estimated', category, event: message });
          }
        } else {
          const data = await res.json();
          list = Array.isArray(data.events) ? data.events : [];
        }
        
        lastLoadedEvents = list;

        if (list.length === 0) {
          timeline.innerHTML = '<div style="color: var(--text-sub); font-size:0.75rem;">No events logged.</div>';
          return;
        }

        const reversedList = [...list].reverse();
        let html = '';
        for (const e of reversedList) {
          let severity = 'info';
          const catInfo = EVENT_CATEGORIES[e.category] || EVENT_CATEGORIES[0];
          const evLower = e.event.toLowerCase();
          if (e.category === 4 || evLower.indexOf('error') >= 0 || evLower.indexOf('fail') >= 0 || evLower.indexOf('fault') >= 0) {
            severity = 'error';
          } else if (evLower.indexOf('warn') >= 0) {
            severity = 'warning';
          } else if (e.category === 5 || e.event === 'ntp_reestablished' || e.event === 'ntp_synced') {
            severity = 'ntp_reestablished';
          }
          
          const timePart = e.ts;
          const safeMsg = e.event.replace(/'/g, "\\'").replace(/"/g, '&quot;');
          html += '<div class="timeline-item ' + severity + '" style="display:flex; justify-content:space-between; align-items:center; gap:10px; border-left-color:' + catInfo.color + ';">';
          html += '  <div style="flex:1; min-width:0;">';
          html += '    <div class="timeline-time" style="display:flex; align-items:center; gap:6px;">';
          html += `      <span style="padding: 1px 5px; border-radius: 4px; font-size: 0.65rem; font-weight: 600; background: ${catInfo.bg}; color: ${catInfo.color}; border: 1px solid ${catInfo.color}; flex-shrink:0;">${catInfo.name}</span>`;
          html += '      <span style="color: var(--text-sub); font-size: 0.72rem;">' + timePart + '</span>';
          html += '    </div>';
          html += '    <div class="timeline-content" style="word-break: break-word; margin-top: 2px;">' + e.event + '</div>';
          html += '  </div>';
          html += `  <button onclick="editEventRecord('${timePart}', '${safeMsg}')" style="width: auto; flex-shrink: 0; padding: 3px 8px; font-size: 0.7rem; background: rgba(255,255,255,0.08); border: 1px solid var(--card-border); color: var(--text); border-radius: 4px; cursor: pointer; white-space: nowrap;">Edit</button>`;
          html += '</div>';
        }
        timeline.innerHTML = html;
        lastEventsFetchTime = Date.now();
      } catch (err) {
        $('eventsTimeline').innerHTML = '<div style="color: var(--error); font-size:0.75rem;">' + err.message + '</div>';
      }
    }

    async function loadLive() {
      if (isLoadingHistory) return;
      try {
        const live = await fetchJson('/api/live');
        if (live && live.has_sample) {
          $('valTemp').textContent = parseFloat(live.temp_c).toFixed(1) + ' °C';
          $('valHum').textContent = parseFloat(live.humidity_pct).toFixed(1) + ' %';
          $('valPres').textContent = parseFloat(live.pressure_hpa).toFixed(1) + ' hPa';
          $('liveTime').textContent = 'Timestamp: ' + live.timestamp;
          $('liveQuality').textContent = live.timestamp_quality;
          $('liveQuality').style.color = live.timestamp_quality === 'ntp' ? 'var(--success)' : 'var(--warning)';
        }
        lastLive = live;
        updatePills(lastLive, lastHealth);
        lastLiveFetchTime = Date.now();
      } catch (err) {
        console.error("Live fetch error", err);
      }
    }

    async function loadHealth() {
      if (isLoadingHistory) return;
      try {
        const health = await fetchJson('/api/health');
        if (health && health.has_health) {
          const up = health.uptime_s;
          const hrs = Math.floor(up / 3600);
          const mins = Math.floor((up % 3600) / 60);
          const secs = up % 60;
          $('healthUptime').textContent = hrs + 'h ' + mins + 'm ' + secs + 's';
          
          $('healthHeap').textContent = (health.free_heap_bytes / 1024).toFixed(1) + ' KB';
          const heapPercent = Math.max(0, Math.min(100, (health.free_heap_bytes / 81920) * 100));
          $('barHeap').style.width = heapPercent + '%';

          $('healthBlock').textContent = (health.largest_free_block_bytes / 1024).toFixed(1) + ' KB';
          
          $('healthQueue').textContent = health.log_queue_depth + ' / ' + health.log_queue_capacity;
          const queuePercent = Math.max(0, Math.min(100, (health.log_queue_depth / health.log_queue_capacity) * 100));
          $('barQueue').style.width = queuePercent + '%';

          $('healthDropped').textContent = health.dropped_log_samples;
          if (health.dropped_log_samples > 0) {
            $('healthDropped').style.color = 'var(--error)';
          }

          if (health.battery) {
            const bat = health.battery;
            const percent = bat.percent;
            const voltage = bat.voltage.toFixed(3);
            
            let timeText = "";
            if (bat.status === "Full") {
              timeText = "Full (External Power)";
            } else if (bat.status === "Charging / USB") {
              const tRemaining = bat.time_remaining;
              if (tRemaining > 0) {
                const tHrs = Math.floor(tRemaining / 3600);
                const tMins = Math.floor((tRemaining % 3600) / 60);
                timeText = `${tHrs}h ${tMins}m to full (Charging)`;
              } else {
                timeText = "Charging via USB";
              }
            } else {
              const tRemaining = bat.time_remaining;
              if (tRemaining > 0) {
                const tHrs = Math.floor(tRemaining / 3600);
                const tMins = Math.floor((tRemaining % 3600) / 60);
                timeText = `${tHrs}h ${tMins}m remaining (Discharging)`;
              } else {
                timeText = "Discharging";
              }
            }
            
            $('healthBatStatus').textContent = timeText;
            $('healthBatText').textContent = percent + '% (' + voltage + ' V)';
            
            const fill = $('batteryLevelBar');
            fill.style.width = percent + '%';
            
            if (percent > 50) {
              fill.style.backgroundColor = 'var(--success)';
            } else if (percent > 20) {
              fill.style.backgroundColor = '#fbbf24';
            } else {
              fill.style.backgroundColor = 'var(--error)';
            }
            
            const container = $('batteryIcon');
            let bolt = container.querySelector('.charging-bolt');
            if (bat.status === "Charging / USB") {
              if (!bolt) {
                bolt = document.createElement('div');
                bolt.className = 'charging-bolt';
                bolt.innerHTML = '⚡';
                container.appendChild(bolt);
              }
            } else {
              if (bolt) {
                bolt.remove();
              }
            }
          }
        }
        lastHealth = health;
        updatePills(lastLive, lastHealth);
        lastHealthFetchTime = Date.now();
      } catch (err) {
        console.error("Health fetch error", err);
      }
    }

    function showTab(tabId) {
      document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));
      
      const targetBtn = $('tab-' + tabId);
      const targetContent = $('content-' + tabId);
      if (targetBtn && targetContent) {
        targetBtn.classList.add('active');
        targetContent.classList.add('active');
      }
    }

    async function loadConfig() {
      if (isLoadingHistory) return;
      try {
        const cfg = await fetchJson('/api/config');
        $('cfgWifiSsid').value = cfg.wifi_ssid || '';
        $('cfgWifiPassword').value = '';
        $('cfgHostname').value = cfg.hostname || '';
        $('cfgTimezone').value = cfg.timezone || '';
        $('cfgSampleInterval').value = cfg.sample_interval_ms;
        $('cfgLogFlushInterval').value = cfg.log_flush_interval_ms;
        $('cfgDisplayRefreshInterval').value = cfg.display_refresh_interval_ms;
        $('cfgLatitude').value = cfg.latitude != null ? cfg.latitude : -37.8136;
        $('cfgLongitude').value = cfg.longitude != null ? cfg.longitude : 144.9631;
        
        localStorage.setItem('cfgLatitude', $('cfgLatitude').value);
        localStorage.setItem('cfgLongitude', $('cfgLongitude').value);
      } catch (err) {
        console.error("Config fetch error", err);
      }
    }

    async function saveSettings(e) {
      e.preventDefault();
      if (isLoadingHistory) return;
      
      const payload = {
        wifi_ssid: $('cfgWifiSsid').value,
        hostname: $('cfgHostname').value,
        timezone: $('cfgTimezone').value,
        sample_interval_ms: parseInt($('cfgSampleInterval').value),
        log_flush_interval_ms: parseInt($('cfgLogFlushInterval').value),
        display_refresh_interval_ms: parseInt($('cfgDisplayRefreshInterval').value),
        latitude: parseFloat($('cfgLatitude').value),
        longitude: parseFloat($('cfgLongitude').value)
      };

      localStorage.setItem('cfgLatitude', payload.latitude);
      localStorage.setItem('cfgLongitude', payload.longitude);

      const pwd = $('cfgWifiPassword').value;
      if (pwd.length > 0) {
        payload.wifi_password = pwd;
      }

      try {
        const res = await fetch('/api/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (data.accepted) {
          alert(data.message || 'Settings applied successfully.');
          if (data.reboot) {
            setTimeout(() => { window.location.reload(); }, 5000);
          } else {
            await loadConfig();
          }
        } else {
          alert('Failed to save configuration: ' + data.message);
        }
      } catch (err) {
        alert('Error saving configuration: ' + err.message);
      }
    }

    function setText(id, text) {
      $(id).textContent = text;
    }

    function onRangeChanged() {
      const custom = $('range').value === 'custom';
      $('customRangeGroup').style.display = custom ? 'grid' : 'none';
      if (custom) {
        const now = new Date();
        const oneHourAgo = new Date(now.getTime() - 60 * 60 * 1000);
        if (!$('start').value) {
          $('start').value = new Date(oneHourAgo.getTime() - oneHourAgo.getTimezoneOffset() * 60000).toISOString().slice(0, 16);
        }
        if (!$('end').value) {
          $('end').value = new Date(now.getTime() - now.getTimezoneOffset() * 60000).toISOString().slice(0, 16);
        }
      }
    }

    async function deleteFile(path, name) {
      if (!confirm('Are you sure you want to delete ' + name + '?')) return;
      try {
        const res = await fetch('/api/logs/delete?file=' + encodeURIComponent(path), { method: 'POST' });
        const data = await res.json();
        if (data.success) {
          alert(data.message || 'Deleted successfully.');
          await loadSdTree();
        } else {
          alert('Error: ' + data.message);
        }
      } catch (err) {
        alert('Failed to delete: ' + err.message);
      }
    }

    async function renameFile(path, name) {
      const newName = prompt('Enter new name for ' + name + ':', name);
      if (!newName || newName === name) return;
      try {
        const res = await fetch('/api/logs/rename?file=' + encodeURIComponent(path) + '&new_name=' + encodeURIComponent(newName), { method: 'POST' });
        const data = await res.json();
        if (data.success) {
          alert(data.message || 'Renamed successfully.');
          await loadSdTree();
        } else {
          alert('Error: ' + data.message);
        }
      } catch (err) {
        alert('Failed to rename: ' + err.message);
      }
    }

    async function triggerAction(url, btnId) {
      if (isLoadingHistory) return;
      const alert = $('actionAlert');
      const btn = $(btnId);
      alert.style.display = 'none';
      btn.disabled = true;
      try {
        const res = await fetch(url, { method: 'POST' });
        const data = await res.json();
        alert.style.display = 'block';
        if (data.success) {
          alert.textContent = data.message;
          alert.className = 'alert-banner success';
        } else {
          alert.textContent = "Action failed: " + data.message;
          alert.className = 'alert-banner error';
        }
      } catch (err) {
        alert.textContent = "Network error: " + err.message;
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
      } finally {
        btn.disabled = false;
        setTimeout(() => { alert.style.display = 'none'; }, 6000);
      }
    }

    function downloadFilteredCSV() {
      if (!historyDataset || historyDataset.length === 0) {
        alert("No sensor history data loaded to export.");
        return;
      }
      let csv = "Timestamp,Temperature (C),Humidity (%),Pressure (hPa)\n";
      for (const pt of historyDataset) {
        const ts = fmtLocalTs(pt.date);
        const t = pt.temp !== null && pt.temp !== undefined ? pt.temp.toFixed(2) : '';
        const h = pt.hum !== null && pt.hum !== undefined ? pt.hum.toFixed(2) : '';
        const p = pt.pres !== null && pt.pres !== undefined ? pt.pres.toFixed(2) : '';
        csv += `${ts},${t},${h},${p}\n`;
      }
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `sensor_history_${Date.now()}.csv`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }

    function downloadBatteryCSV() {
      if (!loadedBatteryData || loadedBatteryData.length === 0) {
        alert("No battery history data loaded to export.");
        return;
      }
      let csv = "Timestamp,Voltage (V),Percent (%),Status\n";
      for (const pt of loadedBatteryData) {
        const ts = fmtLocalTs(new Date(pt.ts * 1000));
        const v = pt.v !== null && pt.v !== undefined ? pt.v.toFixed(3) : '';
        const p = pt.p !== null && pt.p !== undefined ? pt.p.toFixed(1) : '';
        const s = pt.s || '';
        csv += `${ts},${v},${p},${s}\n`;
      }
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `battery_history_${Date.now()}.csv`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }

    $('btnFlush').addEventListener('click', () => triggerAction('/api/action/flush-now', 'btnFlush'));
    $('btnNtp').addEventListener('click', () => triggerAction('/api/action/ntp-retry', 'btnNtp'));

    $('btnLoad').addEventListener('click', loadHistory);
    $('btnDownloadCsv').addEventListener('click', downloadFilteredCSV);
    $('btnDownloadBatCsv').addEventListener('click', downloadBatteryCSV);
    $('btnSaveUiIntervals').addEventListener('click', saveUiIntervals);
    $('rangeMode').addEventListener('change', onRangeModeChanged);

    const triggerLoadOnEnter = (e) => {
      if (e.key === 'Enter' || e.keyCode === 13) {
        e.preventDefault();
        loadHistory();
      }
    };

    ['hoursPast', 'start', 'end', 'binMode', 'rangeMode'].forEach(id => {
      const el = $(id);
      if (el) {
        el.addEventListener('keydown', triggerLoadOnEnter);
      }
    });

    // OTA File Upload Handler
    const otaFile = $('otaFile');
    const otaDragDrop = $('otaDragDrop');
    const otaDragText = $('otaDragText');
    const btnStartOta = $('btnStartOta');
    const otaProgressContainer = $('otaProgressContainer');
    const otaProgressBar = $('otaProgressBar');
    const otaPercent = $('otaPercent');
    const otaStatus = $('otaStatus');
    const otaAlert = $('otaAlert');
    let selectedOtaFile = null;

    otaDragDrop.addEventListener('click', () => otaFile.click());

    otaDragDrop.addEventListener('dragover', (e) => {
      e.preventDefault();
      otaDragDrop.style.borderColor = 'var(--accent)';
      otaDragDrop.style.background = 'rgba(6, 182, 212, 0.05)';
    });

    otaDragDrop.addEventListener('dragleave', () => {
      otaDragDrop.style.borderColor = 'var(--card-border)';
      otaDragDrop.style.background = 'rgba(255, 255, 255, 0.02)';
    });

    otaDragDrop.addEventListener('drop', (e) => {
      e.preventDefault();
      otaDragDrop.style.borderColor = 'var(--card-border)';
      otaDragDrop.style.background = 'rgba(255, 255, 255, 0.02)';
      if (e.dataTransfer.files.length > 0) {
        handleOtaFileSelect(e.dataTransfer.files[0]);
      }
    });

    otaFile.addEventListener('change', (e) => {
      if (e.target.files.length > 0) {
        handleOtaFileSelect(e.target.files[0]);
      }
    });

    function handleOtaFileSelect(file) {
      if (!file.name.endsWith('.bin')) {
        showOtaAlert('Only .bin firmware files are supported.', 'error');
        selectedOtaFile = null;
        btnStartOta.disabled = true;
        otaDragText.textContent = 'Drag & drop or click to select file';
        return;
      }
      selectedOtaFile = file;
      otaDragText.innerHTML = `<strong>Selected:</strong> ${file.name} (${(file.size / 1024).toFixed(1)} KB)`;
      btnStartOta.disabled = false;
      otaAlert.style.display = 'none';
    }

    function showOtaAlert(msg, type) {
      otaAlert.style.display = 'block';
      otaAlert.textContent = msg;
      otaAlert.className = 'alert-banner ' + type;
    }

    function resetOtaUI() {
      btnStartOta.disabled = false;
      otaDragDrop.style.pointerEvents = 'auto';
      selectedOtaFile = null;
      otaDragText.textContent = 'Drag & drop or click to select file';
      otaFile.value = '';
    }

    function startRebootCountdown() {
      let count = 10;
      otaDragText.textContent = 'Device is rebooting. Reconnecting...';
      const timer = setInterval(() => {
        count--;
        if (count <= 0) {
          clearInterval(timer);
          window.location.reload();
        } else {
          otaStatus.textContent = `Reconnecting in ${count}s...`;
        }
      }, 1000);
    }

    btnStartOta.addEventListener('click', () => {
      if (!selectedOtaFile) return;

      btnStartOta.disabled = true;
      otaDragDrop.style.pointerEvents = 'none';
      otaProgressContainer.style.display = 'block';
      otaStatus.textContent = 'Uploading...';
      otaPercent.textContent = '0%';
      otaProgressBar.style.width = '0%';
      otaAlert.style.display = 'none';

      const formData = new FormData();
      formData.append('update', selectedOtaFile);

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/update', true);

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          otaProgressBar.style.width = percent + '%';
          otaPercent.textContent = percent + '%';
          if (percent === 100) {
            otaStatus.textContent = 'Flashing...';
          }
        }
      });

      xhr.onload = () => {
        if (xhr.status === 200) {
          try {
            const res = JSON.parse(xhr.responseText);
            if (res.success) {
              showOtaAlert(res.message, 'success');
              otaStatus.textContent = 'Rebooting...';
              startRebootCountdown();
            } else {
              showOtaAlert(res.message || 'OTA update failed', 'error');
              resetOtaUI();
            }
          } catch (e) {
            showOtaAlert('Firmware flashed. Rebooting...', 'success');
            otaStatus.textContent = 'Rebooting...';
            startRebootCountdown();
          }
        } else {
          showOtaAlert('Upload failed. Server status: ' + xhr.status, 'error');
          resetOtaUI();
        }
      };

      xhr.onerror = () => {
        showOtaAlert('Network error occurred during update.', 'error');
        resetOtaUI();
      };

    });

    function updateModalProgress(percent, status) {
      $('modalProgressBar').style.width = percent + '%';
      $('modalProgressPercent').textContent = Math.round(percent) + '%';
      if (status) {
        $('modalStatus').textContent = status;
      }
    }

    async function loadHistory() {
      const alert = $('historyAlert');
      alert.style.display = 'none';

      isLoadingHistory = true;
      const modal = $('progressModal');
      modal.classList.add('active');
      updateModalProgress(0, 'Initializing data request...');

      let startDate = null;
      let endDate = null;
      const mode = $('rangeMode').value;
      const now = new Date();

      if (mode === 'hours') {
        const hrsPast = parseInt($('hoursPast').value) || 3;
        startDate = new Date(now.getTime() - hrsPast * 60 * 60 * 1000);
        endDate = now;
      } else if (mode === 'custom') {
        const s = $('start').value;
        const e = $('end').value;
        if (s) startDate = new Date(s.replace('T', ' ') + ':00');
        if (e) endDate = new Date(e.replace('T', ' ') + ':00');
      }

      if (!startDate || !endDate || isNaN(startDate.getTime()) || isNaN(endDate.getTime())) {
        alert.textContent = "Invalid date range selected.";
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
        modal.classList.remove('active');
        isLoadingHistory = false;
        return;
      }

      const HOUR_MS = 3600000;
      const startChunkKey = Math.floor(startDate.getTime() / HOUR_MS);
      const endChunkKey = Math.floor(endDate.getTime() / HOUR_MS);

      const missingChunkKeys = [];
      for (let k = startChunkKey; k <= endChunkKey; k++) {
        if (!fetchedChunkKeys.has(k)) {
          missingChunkKeys.push(k);
        }
      }

      try {
        if (missingChunkKeys.length > 0) {
          setText('historyMeta', `Fetching missing data (${missingChunkKeys.length} chunk${missingChunkKeys.length > 1 ? 's' : ''})...`);
          
          for (let i = 0; i < missingChunkKeys.length; i++) {
            const k = missingChunkKeys[i];
            const chunkStart = new Date(k * HOUR_MS);
            const chunkEnd = new Date((k + 1) * HOUR_MS - 1000);
            
            const startStr = fmtLocalTs(chunkStart);
            const endStr = fmtLocalTs(chunkEnd);
            
            updateModalProgress(
              (i / missingChunkKeys.length) * 80 + 10,
              `Downloading chunk ${i + 1}/${missingChunkKeys.length} (${Math.round(((i + 1) / missingChunkKeys.length) * 100)}%)...`
            );
            
            const url = '/api/history?start=' + encodeURIComponent(startStr) + '&end=' + encodeURIComponent(endStr);
            const res = await fetch(url, { cache: 'no-store' });
            if (!res.ok) {
              throw new Error(`HTTP status ${res.status} on chunk ${i + 1}`);
            }
            
            const startTimestampStr = res.headers.get('X-Start-Timestamp') || startStr;
            const intervalMs = parseInt(res.headers.get('X-Sample-Interval-Ms') || '1000');
            const recordSize = parseInt(res.headers.get('X-Record-Size') || '17');

            const arrayBuffer = await res.arrayBuffer();
            const view = new DataView(arrayBuffer);
            const totalRecords = arrayBuffer.byteLength / recordSize;
            const baseTime = new Date(startTimestampStr.replace(' ', 'T')).getTime();
            
            for (let r = 0; r < totalRecords; r++) {
              const offset = r * recordSize;
              const quality = view.getUint8(offset + 16);
              if (quality === 2) continue;

              const recTime = new Date(baseTime + r * intervalMs);
              const tsMs = recTime.getTime();
              const temp = view.getFloat32(offset + 4, true);
              const hum = view.getFloat32(offset + 8, true);
              const pres = view.getFloat32(offset + 12, true);

              cachedHistoryMap.set(tsMs, {
                date: recTime,
                temp: temp,
                hum: hum,
                pres: pres
              });
            }
            fetchedChunkKeys.add(k);
            await new Promise(resolve => setTimeout(resolve, 30));
          }
        }

        updateModalProgress(95, 'Rendering chart...');
        await loadEvents();

        const startMs = startDate.getTime();
        const endMs = endDate.getTime();
        const allPoints = [];

        for (const [ts, pt] of cachedHistoryMap.entries()) {
          if (ts >= startMs && ts <= endMs) {
            allPoints.push(pt);
          }
        }

        allPoints.sort((a, b) => a.date.getTime() - b.date.getTime());

        historyLoaded = true;
        historyDataset = allPoints;

        if (dygraphInstance) {
          dygraphInstance.destroy();
        }

        if (allPoints.length > 0) {
          const selectedBinMode = $('binMode').value;
          let binSizeSec = 60;
          let isAuto = false;

          if (selectedBinMode === 'auto') {
            isAuto = true;
            const totalHours = (endDate.getTime() - startDate.getTime()) / (3600 * 1000);
            if (totalHours <= 1) binSizeSec = 60;        // 1 min bins
            else if (totalHours <= 6) binSizeSec = 180;   // 3 min bins
            else if (totalHours <= 24) binSizeSec = 600;  // 10 min bins
            else binSizeSec = 1800;                       // 30 min bins
          } else {
            binSizeSec = parseInt(selectedBinMode);
          }

          const binSizeText = (sec) => {
            if (sec < 60) return sec + 's';
            if (sec < 3600) return (sec / 60) + 'm';
            return (sec / 3600) + 'h';
          };

          const activeBinLabel = isAuto ? `Auto (${binSizeText(binSizeSec)})` : binSizeText(binSizeSec);

          function aggregateBins(rawPoints, binSizeSeconds) {
            if (rawPoints.length === 0) return [];
            const bins = [];
            const binSizeMs = binSizeSeconds * 1000;
            const startMs = rawPoints[0].date.getTime();
            
            let currentBinStart = startMs;
            let currentPoints = [];
            
            for (let i = 0; i < rawPoints.length; i++) {
              const pt = rawPoints[i];
              const ptMs = pt.date.getTime();
              
              while (ptMs >= currentBinStart + binSizeMs) {
                if (currentPoints.length > 0) {
                  bins.push(makeBin(currentBinStart + binSizeMs / 2, currentPoints));
                  currentPoints = [];
                }
                currentBinStart += binSizeMs;
              }
              currentPoints.push(pt);
            }
            if (currentPoints.length > 0) {
              bins.push(makeBin(currentBinStart + binSizeMs / 2, currentPoints));
            }
            return bins;
          }

          function makeBin(centerTimeMs, pts) {
            let tMin = Infinity, tMax = -Infinity, tSum = 0;
            let hMin = Infinity, hMax = -Infinity, hSum = 0;
            let pMin = Infinity, pMax = -Infinity, pSum = 0;
            pts.forEach(p => {
              if (p.temp < tMin) tMin = p.temp;
              if (p.temp > tMax) tMax = p.temp;
              tSum += p.temp;
              if (p.hum < hMin) hMin = p.hum;
              if (p.hum > hMax) hMax = p.hum;
              hSum += p.hum;
              if (p.pres < pMin) pMin = p.pres;
              if (p.pres > pMax) pMax = p.pres;
              pSum += p.pres;
            });
            return {
              date: new Date(centerTimeMs),
              temp: { min: tMin, avg: tSum / pts.length, max: tMax },
              hum: { min: hMin, avg: hSum / pts.length, max: hMax },
              pres: { min: pMin, avg: pSum / pts.length, max: pMax }
            };
          }

          const bins = aggregateBins(allPoints, binSizeSec);

          let tMin = Infinity, tMax = -Infinity;
          let hMin = Infinity, hMax = -Infinity;
          let pMin = Infinity, pMax = -Infinity;

          bins.forEach(b => {
            if (b.temp.min < tMin) tMin = b.temp.min;
            if (b.temp.max > tMax) tMax = b.temp.max;
            if (b.hum.min < hMin) hMin = b.hum.min;
            if (b.hum.max > hMax) hMax = b.hum.max;
            if (b.pres.min < pMin) pMin = b.pres.min;
            if (b.pres.max > pMax) pMax = b.pres.max;
          });

          const getPaddedBounds = (min, max, minRange) => {
            let range = max - min;
            if (range < minRange) {
              const center = (min + max) / 2;
              min = center - minRange / 2;
              max = center + minRange / 2;
              range = minRange;
            }
            return [min - range * 0.05, max + range * 0.05];
          };

          const [tMinP, tMaxP] = getPaddedBounds(tMin, tMax, 2.0);
          const [hMinP, hMaxP] = getPaddedBounds(hMin, hMax, 15.0);
          const [pMinP, pMaxP] = getPaddedBounds(pMin, pMax, 5.0);

          const dyData = bins.map(b => [
            b.date,
            [
              ((b.temp.min - tMinP) / (tMaxP - tMinP)) * 100,
              ((b.temp.avg - tMinP) / (tMaxP - tMinP)) * 100,
              ((b.temp.max - tMinP) / (tMaxP - tMinP)) * 100
            ],
            [
              ((b.hum.min - hMinP) / (hMaxP - hMinP)) * 100,
              ((b.hum.avg - hMinP) / (hMaxP - hMinP)) * 100,
              ((b.hum.max - hMinP) / (hMaxP - hMinP)) * 100
            ],
            [
              ((b.pres.min - pMinP) / (pMaxP - pMinP)) * 100,
              ((b.pres.avg - pMinP) / (pMaxP - pMinP)) * 100,
              ((b.pres.max - pMinP) / (pMaxP - pMinP)) * 100
            ]
          ]);

          dygraphInstance = new Dygraph(
            document.getElementById("chart"),
            dyData,
            {
              customBars: true,
              dateWindow: [ startDate.getTime(), endDate.getTime() ],
              labels: [ "Time", "Temperature", "Humidity", "Pressure" ],
              colors: [ "#f43f5e", "#06b6d4", "#10b981" ],
              strokeWidth: 2,
              gridLineColor: "rgba(255, 255, 255, 0.05)",
              axisLineColor: "rgba(255, 255, 255, 0.1)",
              labelsDiv: "historyLegend",
              legend: "always",
              underlayCallback: function(canvas, area, g) {
                const chk = document.getElementById('chkShowEvents');
                if (chk && !chk.checked) return;
                if (!lastLoadedEvents || lastLoadedEvents.length === 0) return;

                canvas.save();
                for (let i = 0; i < lastLoadedEvents.length; i++) {
                  const ev = lastLoadedEvents[i];
                  const evMs = new Date(ev.ts.replace(' ', 'T')).getTime();
                  const x = g.toDomXCoord(evMs);
                  if (x >= area.x && x <= area.x + area.w) {
                    const cat = EVENT_CATEGORIES[ev.category] || EVENT_CATEGORIES[0];
                    const col = cat.color;

                    // 1. Vertical dashed event line
                    canvas.strokeStyle = col;
                    canvas.setLineDash([4, 4]);
                    canvas.lineWidth = 1.5;
                    canvas.beginPath();
                    canvas.moveTo(x, area.y);
                    canvas.lineTo(x, area.y + area.h);
                    canvas.stroke();

                    // 2. Rotated 90 degrees text label running vertically alongside the event line
                    canvas.save();
                    canvas.translate(x + 4, area.y + 8);
                    canvas.rotate(Math.PI / 2); // 90 degree rotation

                    let label = ev.event;
                    if (label.length > 10) {
                      label = label.substring(0, 8) + '..';
                    }

                    const badgeText = `[${cat.code}] ${label}`;
                    canvas.font = 'bold 10px sans-serif';
                    const txtWidth = canvas.measureText(badgeText).width;

                    canvas.setLineDash([]);
                    canvas.fillStyle = col;
                    canvas.fillRect(-2, -10, txtWidth + 6, 13);
                    canvas.fillStyle = '#ffffff';
                    canvas.fillText(badgeText, 1, 0);
                    canvas.restore();
                  }
                }
                canvas.restore();
              },
              series: {
                "Temperature": { axis: 'y' },
                "Humidity": { axis: 'y2' },
                "Pressure": { axis: 'y' }
              },
              axes: {
                y: {
                  axisLabelColor: '#f43f5e',
                  valueRange: [0, 100],
                  axisLabelFormatter: function(y) {
                    const realT = tMinP + (y / 100) * (tMaxP - tMinP);
                    return realT.toFixed(1) + '°C';
                  }
                },
                y2: {
                  axisLabelColor: '#06b6d4',
                  valueRange: [0, 100],
                  axisLabelFormatter: function(y) {
                    const realH = hMinP + (y / 100) * (hMaxP - hMinP);
                    return Math.round(realH) + '%';
                  }
                }
              },
              legendFormatter: function(data) {
                if (!data.x) {
                  if (bins.length === 0) return '';
                  const last = bins[bins.length - 1];
                  return `<div style="color:var(--text-sub); font-weight:500; margin-right:8px;">Latest:</div>` +
                    `<div style="color:#f43f5e; font-weight:600;">Temp: ${last.temp.avg.toFixed(2)} [${last.temp.min.toFixed(1)}-${last.temp.max.toFixed(1)}] °C</div>` +
                    `<div style="color:#06b6d4; font-weight:600;">Hum: ${last.hum.avg.toFixed(1)} [${last.hum.min.toFixed(0)}-${last.hum.max.toFixed(0)}] %</div>` +
                    `<div style="color:#10b981; font-weight:600;">Pres: ${last.pres.avg.toFixed(1)} [${last.pres.min.toFixed(1)}-${last.pres.max.toFixed(1)}] hPa</div>`;
                }
                const pt = bins.find(b => b.date.getTime() === data.x);
                const timeStr = data.xHTML;
                let html = `<div style="color:var(--text-sub); font-weight:500; margin-right:8px;">${timeStr}</div>`;
                data.series.forEach(s => {
                  if (!s.isVisible) return;
                  let valStr = '--';
                  if (pt) {
                    if (s.label === 'Temperature') {
                      valStr = `${pt.temp.avg.toFixed(2)} [${pt.temp.min.toFixed(1)}-${pt.temp.max.toFixed(1)}] °C`;
                    } else if (s.label === 'Humidity') {
                      valStr = `${pt.hum.avg.toFixed(1)} [${pt.hum.min.toFixed(0)}-${pt.hum.max.toFixed(0)}] %`;
                    } else if (s.label === 'Pressure') {
                      valStr = `${pt.pres.avg.toFixed(1)} [${pt.pres.min.toFixed(1)}-${pt.pres.max.toFixed(1)}] hPa`;
                    }
                  }
                  html += `<div style="color:${s.color}; font-weight:600;">${s.label}: ${valStr}</div>`;
                });
                return html;
              }
            }
          );
          setText('historyMeta', `Total Points: ${allPoints.length} (Cached Chunks: ${fetchedChunkKeys.size}, Bin Size: ${activeBinLabel}, Bins: ${bins.length})`);
        } else {
          document.getElementById("chart").innerHTML = `<div style="color: var(--text-sub); text-align: center; line-height: 300px;">No data in this range.</div>`;
          setText('historyMeta', 'No data loaded.');
        }

        updateModalProgress(100, 'Done!');
        await new Promise(resolve => setTimeout(resolve, 250));
      } catch (err) {
        historyDataset = [];
        historyLoaded = true;
        setText('historyMeta', 'History error: ' + err.message);
        alert.textContent = 'History load failed: ' + err.message;
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
      } finally {
        modal.classList.remove('active');
        setTimeout(() => {
          isLoadingHistory = false;
        }, 100);
      }
    }

    async function loadBatteryHistory() {
      try {
        const response = await fetch('/api/logs/download?file=/logs/battery.bin');
        if (!response.ok) {
          if (response.status === 404) {
            setText('batChartMeta', 'No battery log file found yet (waiting for first 60s sample).');
            return;
          }
          throw new Error(`HTTP ${response.status}`);
        }

        const arrayBuffer = await response.arrayBuffer();
        if (arrayBuffer.byteLength === 0) {
          setText('batChartMeta', 'No battery data file found or file empty.');
          return;
        }

        const recordSize = 14;
        const view = new DataView(arrayBuffer);
        const numRecords = Math.floor(arrayBuffer.byteLength / recordSize);
        const dataset = [];

        for (let i = 0; i < numRecords; i++) {
          const offset = i * recordSize;
          const epochTime = view.getUint32(offset, true);
          if (epochTime === 0) continue;
          const voltage = view.getFloat32(offset + 4, true);
          const percent = view.getUint8(offset + 8);
          const chargingState = view.getUint8(offset + 9);
          const timeRemaining = view.getInt32(offset + 10, true);

          let status = 'Unknown';
          if (chargingState === 1) status = 'Discharging';
          else if (chargingState === 2) status = 'Charging / USB';
          else if (chargingState === 3) status = 'Full';

          dataset.push({ ts: epochTime, v: voltage, p: percent, s: status, tr: timeRemaining });
        }

        dataset.sort((a, b) => a.ts - b.ts);
        loadedBatteryData = dataset;

        const batPoints = dataset.map(pt => [
          new Date(pt.ts * 1000),
          pt.v,
          pt.p
        ]);

        if (batDygraphInstance) {
          batDygraphInstance.destroy();
        }

        if (batPoints.length > 0) {
          batDygraphInstance = new Dygraph(
            document.getElementById("batChart"),
            batPoints,
            {
              labels: ["Time", "Voltage", "Percent"],
              colors: ["#3b82f6", "#10b981"],
              strokeWidth: 2,
              gridLineColor: "rgba(255, 255, 255, 0.05)",
              axisLineColor: "rgba(255, 255, 255, 0.1)",
              labelsDiv: "batChartLegend",
              legend: "always",
              series: {
                "Voltage": { axis: 'y' },
                "Percent": { axis: 'y2' }
              },
              axes: {
                y: {
                  axisLabelColor: '#3b82f6',
                  valueRange: [3.0, 4.3],
                  axisLabelFormatter: function(y) { return y.toFixed(2) + 'V'; }
                },
                y2: {
                  axisLabelColor: '#10b981',
                  valueRange: [0, 100],
                  axisLabelFormatter: function(y) { return Math.round(y) + '%'; }
                }
              },
              legendFormatter: function(data) {
                if (!data.x) {
                  if (dataset.length === 0) return '';
                  const last = dataset[dataset.length - 1];
                  return `<div style="color:var(--text-sub); font-weight:500; margin-right:8px;">Latest:</div>` +
                    `<div style="color:#3b82f6; font-weight:600;">Voltage: ${last.v.toFixed(3)} V</div>` +
                    `<div style="color:#10b981; font-weight:600;">Percent: ${last.p} % (${last.s})</div>`;
                }
                const pt = dataset.find(b => (b.ts * 1000) === data.x);
                const timeStr = data.xHTML;
                let html = `<div style="color:var(--text-sub); font-weight:500; margin-right:8px;">${timeStr}</div>`;
                data.series.forEach(s => {
                  if (!s.isVisible) return;
                  let valStr = '--';
                  if (pt) {
                    if (s.label === 'Voltage') valStr = `${pt.v.toFixed(3)} V`;
                    else if (s.label === 'Percent') valStr = `${pt.p} % (${pt.s})`;
                  }
                  html += `<div style="color:${s.color}; font-weight:600;">${s.label}: ${valStr}</div>`;
                });
                return html;
              }
            }
          );
          setText('batChartMeta', `Battery Logged Points: ${dataset.length}`);
        } else {
          document.getElementById("batChart").innerHTML = `<div style="color: var(--text-sub); text-align: center; line-height: 200px;">No battery data found.</div>`;
          setText('batChartMeta', 'No battery data.');
        }
      } catch (err) {
        setText('batChartMeta', 'Battery load error: ' + err.message);
      }
    }

    function onRangeModeChanged() {
      const mode = $('rangeMode').value;
      $('hoursGroup').style.display = mode === 'hours' ? 'flex' : 'none';
      $('customRangeGroup').style.display = mode === 'custom' ? 'grid' : 'none';

      if (mode === 'custom') {
        const now = new Date();
        const threeHoursAgo = new Date(now.getTime() - 3 * 60 * 60 * 1000);
        if (!$('start').value) {
          $('start').value = fmtLocalTs(threeHoursAgo).replace(' ', 'T').slice(0, 16);
        }
        if (!$('end').value) {
          $('end').value = fmtLocalTs(now).replace(' ', 'T').slice(0, 16);
        }
      }
    }

    window.addEventListener('DOMContentLoaded', () => {
      initTasks();
      loadUiIntervals();
      loadConfig();
      initAutoUpdateUI();
      
      // Load live snapshot and health snapshot immediately on page load
      loadLive();
      loadHealth();
      
      startScheduler(true);
    });
  </script>
</body>
</html>
)HTML";

} // namespace web
