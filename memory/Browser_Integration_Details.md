# Browser Integration Details
## Tungsten Browser - Chromium/Thorium Integration Points

### 1. Chromium Source Modifications

#### 1.1 Core Browser Process Files

```cpp
// chrome/browser/chrome_content_browser_client.cc
// Add Nostr protocol handler registration
void ChromeContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    ukm::SourceIdObj ukm_source_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  // Existing code...
  
  // Add Nostr protocol handler
  auto nostr_factory = std::make_unique<NostrProtocolHandlerFactory>();
  factories->emplace("nostr", std::move(nostr_factory));
}

// chrome/browser/browser_process_impl.cc
// Initialize Nostr services
void BrowserProcessImpl::PreMainMessageLoopRun() {
  // Existing initialization...
  
  // Initialize Nostr services
  nostr_service_manager_ = std::make_unique<NostrServiceManager>();
  nostr_service_manager_->Initialize();
  
  // Start local relay if enabled
  if (prefs()->GetBoolean(prefs::kNostrLocalRelayEnabled)) {
    nostr_service_manager_->StartLocalRelay(
        prefs()->GetInteger(prefs::kNostrLocalRelayPort));
  }
  
  // Start Blossom server if enabled
  if (prefs()->GetBoolean(prefs::kBlossomServerEnabled)) {
    nostr_service_manager_->StartBlossomServer(
        prefs()->GetInteger(prefs::kBlossomServerPort));
  }
}
```

#### 1.2 Renderer Process Modifications

```cpp
// content/renderer/render_frame_impl.cc
// Inject window.nostr object
void RenderFrameImpl::DidCreateScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  // Only inject in main world
  if (world_id != ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }
  
  // Check if NIP-07 is enabled for this origin
  if (ShouldInjectNostr(frame_->GetSecurityOrigin())) {
    NostrBindings::Install(context->Global(), render_frame_.get());
  }
  
  // Call parent implementation
  ContentRendererClient::Get()->DidCreateScriptContext(
      this, context, world_id);
}

// content/renderer/nostr_bindings.cc
// New file for window.nostr implementation
class NostrBindings {
 public:
  static void Install(v8::Local<v8::Object> global,
                     RenderFrame* render_frame) {
    v8::Isolate* isolate = global->GetIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(global->Context());
    
    // Create nostr object
    v8::Local<v8::Object> nostr = v8::Object::New(isolate);
    
    // Add getPublicKey method
    nostr->Set(
        gin::StringToV8(isolate, "getPublicKey"),
        gin::CreateFunctionTemplate(
            isolate,
            base::BindRepeating(&NostrBindings::GetPublicKey,
                              base::Unretained(render_frame)))
            ->GetFunction(global->Context())
            .ToLocalChecked());
    
    // Add signEvent method
    nostr->Set(
        gin::StringToV8(isolate, "signEvent"),
        gin::CreateFunctionTemplate(
            isolate,
            base::BindRepeating(&NostrBindings::SignEvent,
                              base::Unretained(render_frame)))
            ->GetFunction(global->Context())
            .ToLocalChecked());
    
    // Set window.nostr
    global->Set(gin::StringToV8(isolate, "nostr"), nostr);
  }
  
 private:
  static void GetPublicKey(RenderFrame* render_frame,
                          gin::Arguments* args);
  static void SignEvent(RenderFrame* render_frame,
                       gin::Arguments* args);
};
```

#### 1.3 Browser UI Modifications

```cpp
// chrome/browser/ui/views/frame/browser_view.cc
// Add Nostr status indicator
void BrowserView::InitViews() {
  // Existing initialization...
  
  // Add Nostr status indicator to toolbar
  nostr_status_button_ = toolbar_->AddNostrStatusButton();
  nostr_status_button_->SetCallback(
      base::BindRepeating(&BrowserView::OnNostrStatusClicked,
                         base::Unretained(this)));
}

// chrome/browser/ui/toolbar/toolbar_view.cc
// Add Nostr button to toolbar
views::Button* ToolbarView::AddNostrStatusButton() {
  auto nostr_button = std::make_unique<NostrStatusButton>(browser_);
  nostr_button->SetID(VIEW_ID_NOSTR_STATUS_BUTTON);
  nostr_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_BUTTON));
  
  return AddChildView(std::move(nostr_button));
}
```

### 2. Build System Integration

#### 2.1 BUILD.gn Files

```python
# chrome/browser/BUILD.gn
# Add Nostr sources to browser target
static_library("browser") {
  sources += [
    "nostr/nostr_service_manager.cc",
    "nostr/nostr_service_manager.h",
    "nostr/nostr_protocol_handler.cc",
    "nostr/nostr_protocol_handler.h",
    "nostr/nip07_handler.cc",
    "nostr/nip07_handler.h",
    "nostr/key_management/key_service.cc",
    "nostr/key_management/key_service.h",
    "nostr/relay/local_relay_service.cc",
    "nostr/relay/local_relay_service.h",
    "nostr/blossom/blossom_service.cc",
    "nostr/blossom/blossom_service.h",
  ]
  
  deps += [
    "//third_party/nostr_tools",
    "//third_party/secp256k1",
    "//third_party/bech32",
  ]
}

# content/renderer/BUILD.gn
# Add Nostr bindings to renderer
static_library("renderer") {
  sources += [
    "nostr_bindings.cc",
    "nostr_bindings.h",
  ]
}
```

#### 2.2 Feature Flags

```python
# chrome/browser/about_flags.cc
// Add Nostr feature flags
const FeatureEntry kFeatureEntries[] = {
  // Existing features...
  
  {"enable-nostr-protocol",
   flag_descriptions::kEnableNostrProtocolName,
   flag_descriptions::kEnableNostrProtocolDescription,
   kOsAll,
   SINGLE_VALUE_TYPE(switches::kEnableNostrProtocol)},
   
  {"enable-local-relay",
   flag_descriptions::kEnableLocalRelayName,
   flag_descriptions::kEnableLocalRelayDescription,
   kOsAll,
   FEATURE_VALUE_TYPE(features::kNostrLocalRelay)},
   
  {"enable-blossom-server",
   flag_descriptions::kEnableBlossomServerName,
   flag_descriptions::kEnableBlossomServerDescription,
   kOsAll,
   FEATURE_VALUE_TYPE(features::kBlossomServer)},
};

# chrome/browser/flag_descriptions.cc
const char kEnableNostrProtocolName[] = "Enable Nostr Protocol";
const char kEnableNostrProtocolDescription[] =
    "Enables native support for the Nostr protocol including "
    "nostr:// URLs and window.nostr injection.";
```

### 3. Preferences and Settings

```cpp
// chrome/common/pref_names.cc
// Add Nostr preferences
namespace prefs {
  // Nostr feature preferences
  const char kNostrEnabled[] = "nostr.enabled";
  const char kNostrLocalRelayEnabled[] = "nostr.local_relay.enabled";
  const char kNostrLocalRelayPort[] = "nostr.local_relay.port";
  const char kBlossomServerEnabled[] = "nostr.blossom.enabled";
  const char kBlossomServerPort[] = "nostr.blossom.port";
  const char kNostrDefaultRelays[] = "nostr.default_relays";
  
  // Key management preferences
  const char kNostrAccounts[] = "nostr.accounts";
  const char kNostrActiveAccount[] = "nostr.active_account";
  const char kNostrKeyUnlockTimeout[] = "nostr.key_unlock_timeout";
}

// chrome/browser/prefs/browser_prefs.cc
// Register Nostr preferences
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Existing preferences...
  
  // Nostr preferences
  registry->RegisterBooleanPref(prefs::kNostrEnabled, true);
  registry->RegisterBooleanPref(prefs::kNostrLocalRelayEnabled, false);
  registry->RegisterIntegerPref(prefs::kNostrLocalRelayPort, 8081);
  registry->RegisterBooleanPref(prefs::kBlossomServerEnabled, false);
  registry->RegisterIntegerPref(prefs::kBlossomServerPort, 8080);
  registry->RegisterListPref(prefs::kNostrDefaultRelays);
  registry->RegisterDictionaryPref(prefs::kNostrAccounts);
  registry->RegisterStringPref(prefs::kNostrActiveAccount, "");
  registry->RegisterIntegerPref(prefs::kNostrKeyUnlockTimeout, 300);
}
```

### 4. WebUI Pages

```cpp
// chrome/browser/ui/webui/nostr/nostr_ui.cc
// Settings page for Nostr features
class NostrUI : public content::WebUIController {
 public:
  explicit NostrUI(content::WebUI* web_ui);
  
 private:
  void RegisterMessages();
  void HandleGetAccounts(const base::ListValue* args);
  void HandleCreateAccount(const base::ListValue* args);
  void HandleImportAccount(const base::ListValue* args);
  void HandleExportAccount(const base::ListValue* args);
  void HandleGetRelays(const base::ListValue* args);
  void HandleAddRelay(const base::ListValue* args);
  void HandleRemoveRelay(const base::ListValue* args);
  void HandleGetBlossomServers(const base::ListValue* args);
  
  std::unique_ptr<NostrUIHandler> handler_;
};

// chrome/browser/resources/settings/nostr_page/nostr_page.html
// Settings UI template
<settings-animated-pages id="pages" section="nostr">
  <settings-subpage page-title="Nostr Settings">
    <settings-section page-title="Accounts">
      <div class="settings-box">
        <cr-button id="createAccount" on-click="onCreateAccountClick_">
          Create New Account
        </cr-button>
        <cr-button id="importAccount" on-click="onImportAccountClick_">
          Import Account
        </cr-button>
      </div>
      <iron-list items="[[accounts_]]">
        <template>
          <nostr-account-row account="[[item]]">
          </nostr-account-row>
        </template>
      </iron-list>
    </settings-section>
    
    <settings-section page-title="Relays">
      <settings-toggle-button
          pref="{{prefs.nostr.local_relay.enabled}}"
          label="Enable Local Relay">
      </settings-toggle-button>
      <relay-list relays="[[relays_]]"></relay-list>
    </settings-section>
    
    <settings-section page-title="Storage">
      <settings-toggle-button
          pref="{{prefs.nostr.blossom.enabled}}"
          label="Enable Blossom Server">
      </settings-toggle-button>
      <blossom-settings></blossom-settings>
    </settings-section>
  </settings-subpage>
</settings-animated-pages>
```

### 5. IPC Messages

```cpp
// chrome/common/nostr_messages.h
// Define IPC messages for Nostr
IPC_MESSAGE_START(NostrMsgStart)

// Renderer -> Browser messages
IPC_MESSAGE_ROUTED2(NostrHostMsg_GetPublicKey,
                    int /* request_id */,
                    url::Origin /* origin */)

IPC_MESSAGE_ROUTED3(NostrHostMsg_SignEvent,
                    int /* request_id */,
                    url::Origin /* origin */,
                    std::string /* event_json */)

IPC_MESSAGE_ROUTED2(NostrHostMsg_GetRelays,
                    int /* request_id */,
                    url::Origin /* origin */)

// Browser -> Renderer messages
IPC_MESSAGE_ROUTED3(NostrMsg_GetPublicKeyResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* pubkey */)

IPC_MESSAGE_ROUTED3(NostrMsg_SignEventResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* signed_event_json */)

IPC_MESSAGE_ROUTED3(NostrMsg_GetRelaysResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* relays_json */)
```

### 6. Content Security Policy Integration

```cpp
// content/browser/renderer_host/render_frame_host_impl.cc
// Modify CSP for Nostr content
void RenderFrameHostImpl::CommitNavigation(
    NavigationRequest* navigation_request) {
  // Check if this is a Nostr URL
  if (navigation_request->GetURL().SchemeIs("nostr")) {
    // Apply special CSP for Nostr content
    network::mojom::CSPDirectivePtr csp = network::mojom::CSPDirective::New();
    csp->directive = network::mojom::CSPDirectiveName::DefaultSrc;
    csp->sources.push_back(network::mojom::CSPSource::New(
        "self", "", url::PORT_UNSPECIFIED, "", false, false));
    
    // Restrict external connections
    csp->directive = network::mojom::CSPDirectiveName::ConnectSrc;
    csp->sources.push_back(network::mojom::CSPSource::New(
        "none", "", url::PORT_UNSPECIFIED, "", false, false));
    
    navigation_request->SetContentSecurityPolicy(csp);
  }
  
  // Continue with normal commit
  RenderFrameHostImpl::CommitNavigation(navigation_request);
}
```

### 7. Extension API Bridge

```javascript
// chrome/browser/extensions/api/nostr/nostr_api.cc
// Allow extensions to interact with Nostr features
namespace extensions {
namespace api {
namespace nostr {

bool NostrGetPublicKeyFunction::RunAsync() {
  NostrService* service = NostrServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  
  service->GetPublicKey(
      base::BindOnce(&NostrGetPublicKeyFunction::OnGetPublicKey, this));
  
  return true;
}

void NostrGetPublicKeyFunction::OnGetPublicKey(
    const std::string& pubkey) {
  SetResult(std::make_unique<base::Value>(pubkey));
  SendResponse(true);
}

}  // namespace nostr
}  // namespace api
}  // namespace extensions
```

### 8. Testing Infrastructure

```cpp
// chrome/browser/nostr/nostr_browsertest.cc
// Browser tests for Nostr features
class NostrBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    
    // Enable Nostr features
    feature_list_.InitAndEnableFeature(features::kNostrProtocol);
    
    // Start test relay
    test_relay_ = std::make_unique<TestNostrRelay>();
    test_relay_->Start(0);  // Random port
  }
  
  void TearDownOnMainThread() override {
    test_relay_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }
  
  std::unique_ptr<TestNostrRelay> test_relay_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NostrBrowserTest, WindowNostrInjection) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/nostr_test.html"));
  
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  
  // Check window.nostr exists
  bool has_nostr = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(!!window.nostr);",
      &has_nostr));
  EXPECT_TRUE(has_nostr);
}
```

### 9. Metrics and Telemetry

```cpp
// chrome/browser/nostr/nostr_metrics.cc
// UMA metrics for Nostr usage
namespace nostr {
namespace metrics {

void RecordNIP07Usage(NIP07Method method) {
  UMA_HISTOGRAM_ENUMERATION("Nostr.NIP07.MethodCalled",
                           method,
                           NIP07Method::kMaxValue);
}

void RecordRelayConnectionLatency(base::TimeDelta latency) {
  UMA_HISTOGRAM_TIMES("Nostr.Relay.ConnectionLatency", latency);
}

void RecordBlossomCacheHitRate(bool hit) {
  UMA_HISTOGRAM_BOOLEAN("Nostr.Blossom.CacheHit", hit);
}

void RecordNostrProtocolNavigation() {
  UMA_HISTOGRAM_COUNTS_100("Nostr.Protocol.NavigationCount", 1);
}

}  // namespace metrics
}  // namespace nostr
```

### 10. Platform-Specific Integration

```cpp
#if defined(OS_MAC)
// chrome/browser/nostr/mac/keychain_integration.mm
// macOS Keychain integration
@interface NostrKeychainManager : NSObject
- (BOOL)storeKey:(NSString*)key forAccount:(NSString*)account;
- (NSString*)retrieveKeyForAccount:(NSString*)account;
@end

@implementation NostrKeychainManager
- (BOOL)storeKey:(NSString*)key forAccount:(NSString*)account {
  NSMutableDictionary* query = [NSMutableDictionary dictionary];
  [query setObject:(id)kSecClassGenericPassword forKey:(id)kSecClass];
  [query setObject:@"com.tungsten.browser.nostr" forKey:(id)kSecAttrService];
  [query setObject:account forKey:(id)kSecAttrAccount];
  [query setObject:[key dataUsingEncoding:NSUTF8StringEncoding] 
            forKey:(id)kSecValueData];
  
  SecItemDelete((CFDictionaryRef)query);
  OSStatus status = SecItemAdd((CFDictionaryRef)query, NULL);
  
  return status == errSecSuccess;
}
@end
#endif

#if defined(OS_WIN)
// chrome/browser/nostr/win/credential_manager_integration.cc
// Windows Credential Manager integration
bool StoreKeyInCredentialManager(const std::string& account,
                                const std::string& key) {
  CREDENTIALW cred = {0};
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = base::UTF8ToWide(
      "Tungsten_Nostr_" + account).c_str();
  cred.CredentialBlobSize = key.size();
  cred.CredentialBlob = (LPBYTE)key.c_str();
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
  
  return CredWriteW(&cred, 0) == TRUE;
}
#endif
```