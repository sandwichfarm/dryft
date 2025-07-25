// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "content/public/common/content_client.h"

class ChromeContentClient : public content::ContentClient {
 public:
  ChromeContentClient();
  ~ChromeContentClient() override;

  // content::ContentClient:
  void AddAdditionalSchemes(Schemes* schemes) override;
  std::u16string GetLocalizedString(int message_id) override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  std::string GetProcessTypeNameInEnglish(int type) override;

#if BUILDFLAG(IS_MACOSX)
  bool GetBundleInfo(base::apple::BundleInfo* info) override;
#endif

 private:
  ChromeContentClient(const ChromeContentClient&) = delete;
  ChromeContentClient& operator=(const ChromeContentClient&) = delete;
};

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_