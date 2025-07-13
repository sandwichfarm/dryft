// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/common/nostr_scheme.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(IS_MACOSX)
#include "base/apple/bundle_locations.h"
#endif

ChromeContentClient::ChromeContentClient() = default;
ChromeContentClient::~ChromeContentClient() = default;

void ChromeContentClient::AddAdditionalSchemes(Schemes* schemes) {
  // Register nostr:// and snostr:// schemes
  schemes->standard_schemes.push_back(chrome::kNostrScheme);
  schemes->standard_schemes.push_back(chrome::kSecureNostrScheme);
  
  // Mark snostr:// as secure
  schemes->secure_schemes.push_back(chrome::kSecureNostrScheme);
  
  // Allow CORS for both schemes
  schemes->cors_enabled_schemes.push_back(chrome::kNostrScheme);
  schemes->cors_enabled_schemes.push_back(chrome::kSecureNostrScheme);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  schemes->standard_schemes.push_back(extensions::kExtensionScheme);
  schemes->secure_schemes.push_back(extensions::kExtensionScheme);
  schemes->service_worker_schemes.push_back(extensions::kExtensionScheme);
  schemes->cors_enabled_schemes.push_back(extensions::kExtensionScheme);
  schemes->csp_bypassing_schemes.push_back(extensions::kExtensionScheme);
#endif
}

std::u16string ChromeContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string ChromeContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

gfx::Image& ChromeContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

std::string ChromeContentClient::GetProcessTypeNameInEnglish(int type) {
  switch (type) {
    case PROCESS_TYPE_BROWSER:
      return "Browser";
    case PROCESS_TYPE_RENDERER:
      return "Tab";
    case PROCESS_TYPE_UTILITY:
      return "Utility";
    case PROCESS_TYPE_ZYGOTE:
      return "Zygote";
    case PROCESS_TYPE_SANDBOX_HELPER:
      return "Sandbox helper";
    case PROCESS_TYPE_GPU:
      return "GPU";
    case PROCESS_TYPE_PPAPI_PLUGIN:
      return "Pepper Plugin";
    case PROCESS_TYPE_PPAPI_BROKER:
      return "Pepper Plugin Broker";
    default:
      NOTREACHED();
      return "Unknown";
  }
}

#if BUILDFLAG(IS_MACOSX)
bool ChromeContentClient::GetBundleInfo(base::apple::BundleInfo* info) {
  info->product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  info->creator_code = 'THOR';  // Thorium creator code
  
  base::FilePath framework_path = base::apple::FrameworkBundlePath();
  info->framework_path = framework_path.BaseName();
  
  return true;
}
#endif