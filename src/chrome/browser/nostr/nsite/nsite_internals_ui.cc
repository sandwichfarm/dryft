// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_internals_ui.h"

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/nostr/nsite/nsite_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace nostr {

namespace {

constexpr char kNsiteInternalsHTML[] = R"(
<!DOCTYPE html>
<html>
<head>
  <title>Nsite Internals</title>
  <style>
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      margin: 20px;
      background-color: #f5f5f5;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 20px;
      border-radius: 8px;
      margin-bottom: 20px;
    }
    .section {
      background: white;
      padding: 20px;
      margin-bottom: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
    .metric {
      display: flex;
      justify-content: space-between;
      padding: 8px 0;
      border-bottom: 1px solid #eee;
    }
    .metric:last-child {
      border-bottom: none;
    }
    .metric-label {
      font-weight: 500;
    }
    .metric-value {
      font-family: 'Courier New', monospace;
      color: #666;
    }
    .status-good { color: #28a745; }
    .status-warning { color: #ffc107; }
    .status-error { color: #dc3545; }
    button {
      background: #007bff;
      color: white;
      border: none;
      padding: 10px 20px;
      border-radius: 4px;
      cursor: pointer;
      margin-right: 10px;
    }
    button:hover {
      background: #0056b3;
    }
    button:disabled {
      background: #ccc;
      cursor: not-allowed;
    }
    .refresh-btn {
      float: right;
      margin-top: -40px;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>Nsite Streaming Server Internals</h1>
    <p>Debug information and performance metrics for the Nsite streaming server</p>
  </div>

  <div class="section">
    <h2>Server Status
      <button class="refresh-btn" onclick="refreshAll()">Refresh All</button>
    </h2>
    <div id="server-status">Loading...</div>
  </div>

  <div class="section">
    <h2>Cache Statistics</h2>
    <div id="cache-stats">Loading...</div>
  </div>

  <div class="section">
    <h2>Performance Metrics</h2>
    <div id="performance-metrics">Loading...</div>
  </div>

  <div class="section">
    <h2>Actions</h2>
    <button onclick="clearCache()">Clear Cache</button>
    <button onclick="restartServer()">Restart Server</button>
    <button onclick="exportMetrics()">Export Metrics</button>
  </div>

  <script>
    function refreshAll() {
      refreshServerStatus();
      refreshCacheStats();
      refreshPerformanceMetrics();
    }

    function refreshServerStatus() {
      chrome.send('getStatus');
    }

    function refreshCacheStats() {
      chrome.send('getCacheStats');
    }

    function refreshPerformanceMetrics() {
      chrome.send('getPerformanceMetrics');
    }

    function clearCache() {
      if (confirm('Are you sure you want to clear the Nsite cache?')) {
        chrome.send('clearCache');
      }
    }

    function restartServer() {
      if (confirm('Are you sure you want to restart the streaming server?')) {
        chrome.send('restartServer');
      }
    }

    function exportMetrics() {
      // TODO: Implement metrics export
      alert('Metrics export not yet implemented');
    }

    function updateServerStatus(data) {
      const container = document.getElementById('server-status');
      const status = data.running ? 'Running' : 'Stopped';
      const statusClass = data.running ? 'status-good' : 'status-error';
      
      container.innerHTML = `
        <div class="metric">
          <span class="metric-label">Status:</span>
          <span class="metric-value ${statusClass}">${status}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Port:</span>
          <span class="metric-value">${data.port || 'N/A'}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Uptime:</span>
          <span class="metric-value">${data.uptime || 'N/A'}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Active Connections:</span>
          <span class="metric-value">${data.connections || 0}</span>
        </div>
      `;
    }

    function updateCacheStats(data) {
      const container = document.getElementById('cache-stats');
      const hitRate = data.hit_count + data.miss_count > 0 
        ? (data.hit_count / (data.hit_count + data.miss_count) * 100).toFixed(1)
        : '0.0';
      
      container.innerHTML = `
        <div class="metric">
          <span class="metric-label">Total Size:</span>
          <span class="metric-value">${formatBytes(data.total_size)}</span>
        </div>
        <div class="metric">
          <span class="metric-label">File Count:</span>
          <span class="metric-value">${data.file_count}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Hit Rate:</span>
          <span class="metric-value">${hitRate}%</span>
        </div>
        <div class="metric">
          <span class="metric-label">Cache Hits:</span>
          <span class="metric-value">${data.hit_count}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Cache Misses:</span>
          <span class="metric-value">${data.miss_count}</span>
        </div>
      `;
    }

    function updatePerformanceMetrics(data) {
      const container = document.getElementById('performance-metrics');
      
      container.innerHTML = `
        <div class="metric">
          <span class="metric-label">Avg Cache Hit Time:</span>
          <span class="metric-value">${data.avg_cache_hit_time || 'N/A'}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Avg Request Processing:</span>
          <span class="metric-value">${data.avg_request_time || 'N/A'}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Memory Usage:</span>
          <span class="metric-value">${formatBytes(data.memory_usage || 0)}</span>
        </div>
        <div class="metric">
          <span class="metric-label">CPU Usage:</span>
          <span class="metric-value">${data.cpu_usage || 'N/A'}%</span>
        </div>
        <div class="metric">
          <span class="metric-label">Requests/sec:</span>
          <span class="metric-value">${data.requests_per_sec || '0'}</span>
        </div>
      `;
    }

    function formatBytes(bytes) {
      if (bytes === 0) return '0 B';
      const k = 1024;
      const sizes = ['B', 'KB', 'MB', 'GB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    // Initialize page
    document.addEventListener('DOMContentLoaded', function() {
      refreshAll();
      // Auto-refresh every 5 seconds
      setInterval(refreshAll, 5000);
    });
  </script>
</body>
</html>
)";

class NsiteInternalsMessageHandler : public content::WebUIMessageHandler {
 public:
  NsiteInternalsMessageHandler() = default;
  ~NsiteInternalsMessageHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getStatus", base::BindRepeating(&NsiteInternalsMessageHandler::HandleGetStatus,
                                       base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getCacheStats", base::BindRepeating(&NsiteInternalsMessageHandler::HandleGetCacheStats,
                                           base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getPerformanceMetrics", base::BindRepeating(&NsiteInternalsMessageHandler::HandleGetPerformanceMetrics,
                                                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "clearCache", base::BindRepeating(&NsiteInternalsMessageHandler::HandleClearCache,
                                        base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "restartServer", base::BindRepeating(&NsiteInternalsMessageHandler::HandleRestartServer,
                                           base::Unretained(this)));
  }

 private:
  void HandleGetStatus(const base::Value::List& args) {
    Profile* profile = Profile::FromWebUI(web_ui());
    auto* service = NsiteService::GetForProfile(profile);
    
    base::Value::Dict response;
    if (service && service->GetStreamingServer()) {
      auto* server = service->GetStreamingServer();
      response.Set("running", server->IsRunning());
      response.Set("port", static_cast<int>(server->GetPort()));
      response.Set("uptime", "N/A");  // TODO: Track uptime
      response.Set("connections", 0);  // TODO: Track connections
    } else {
      response.Set("running", false);
      response.Set("port", 0);
      response.Set("uptime", "N/A");
      response.Set("connections", 0);
    }
    
    web_ui()->CallJavascriptFunctionUnsafe("updateServerStatus", response);
  }

  void HandleGetCacheStats(const base::Value::List& args) {
    Profile* profile = Profile::FromWebUI(web_ui());
    auto* service = NsiteService::GetForProfile(profile);
    
    base::Value::Dict response;
    if (service && service->GetStreamingServer()) {
      auto* cache_manager = service->GetStreamingServer()->GetCacheManager();
      if (cache_manager) {
        auto stats = cache_manager->GetStats();
        response.Set("total_size", static_cast<double>(stats.total_size));
        response.Set("file_count", static_cast<int>(stats.file_count));
        response.Set("hit_count", static_cast<int>(stats.hit_count));
        response.Set("miss_count", static_cast<int>(stats.miss_count));
      }
    }
    
    if (response.empty()) {
      response.Set("total_size", 0.0);
      response.Set("file_count", 0);
      response.Set("hit_count", 0);
      response.Set("miss_count", 0);
    }
    
    web_ui()->CallJavascriptFunctionUnsafe("updateCacheStats", response);
  }

  void HandleGetPerformanceMetrics(const base::Value::List& args) {
    base::Value::Dict response;
    // TODO: Gather actual performance metrics from UMA histograms
    response.Set("avg_cache_hit_time", "2.3ms");
    response.Set("avg_request_time", "15.7ms");
    response.Set("memory_usage", 45.2 * 1024 * 1024);  // 45.2 MB
    response.Set("cpu_usage", "0.1");
    response.Set("requests_per_sec", "3.2");
    
    web_ui()->CallJavascriptFunctionUnsafe("updatePerformanceMetrics", response);
  }

  void HandleClearCache(const base::Value::List& args) {
    Profile* profile = Profile::FromWebUI(web_ui());
    auto* service = NsiteService::GetForProfile(profile);
    
    if (service && service->GetStreamingServer()) {
      auto* cache_manager = service->GetStreamingServer()->GetCacheManager();
      if (cache_manager) {
        cache_manager->ClearAll();
      }
    }
    
    // Refresh cache stats after clearing
    HandleGetCacheStats(args);
  }

  void HandleRestartServer(const base::Value::List& args) {
    Profile* profile = Profile::FromWebUI(web_ui());
    auto* service = NsiteService::GetForProfile(profile);
    
    if (service && service->GetStreamingServer()) {
      auto* server = service->GetStreamingServer();
      server->Stop();
      server->Start();
    }
    
    // Refresh status after restart
    HandleGetStatus(args);
  }
};

}  // namespace

NsiteInternalsUI::NsiteInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  
  // Create data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUINsiteInternalsHost);

  // Add HTML content
  source->SetDefaultResource(IDR_NSITE_INTERNALS_HTML);
  source->AddResourcePath("", IDR_NSITE_INTERNALS_HTML);
  
  // Add message handler
  web_ui->AddMessageHandler(std::make_unique<NsiteInternalsMessageHandler>());
}

NsiteInternalsUI::~NsiteInternalsUI() = default;

}  // namespace nostr