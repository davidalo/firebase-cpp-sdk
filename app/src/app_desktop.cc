/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/src/include/firebase/app.h"

#include <string.h>
#include <fstream>

#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"

namespace firebase {
DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace internal {
struct PrivateAppData {
  // A registry that modules can use to expose functions to each other, without
  // requiring a linkage dependency.
  // todo - make all the implementations use something like this, for internal
  // or implementation-specific code.  b/70229654
  FunctionRegistry function_registry;
};

// When Create() is invoked without arguments, it checks for a file named
// google-services-desktop.json, to load options from.  This specifies the
// path to search.
static std::string g_default_config_path;  // NOLINT

}  // namespace internal

// The default App name, the same string as what are used for Android and iOS.
const char* kDefaultAppName = "__FIRAPP_DEFAULT";

App::App() {
  LogDebug("Creating firebase::App for %s", kFirebaseVersionString);
  data_ = new internal::PrivateAppData();
}

App::~App() {
  app_common::RemoveApp(this);
  // Once we use data_, we should delete it here.
  delete static_cast<internal::PrivateAppData*>(data_);
  data_ = nullptr;
}

// Size is arbitrary, just making sure that there is a sane limit.
const int kMaxBuffersize = 1024 * 500;

// Attempts to load a config file from the path specified, and use it to
// populate the AppOptions pointer.  Returns true on success, false otherwise.
bool LoadFromJsonConfigFile(const char* path, AppOptions* options) {
  bool loaded_options = false;
  std::ifstream infile(path, std::ifstream::binary);
  if (infile) {
    infile.seekg(0, infile.end);
    int file_length = infile.tellg();
    // Make sure the file is a sane size:
    if (file_length > kMaxBuffersize) return false;
    infile.seekg(0, infile.beg);

    // Make sure our seeks/tells succeeded
    if (file_length == -1) return false;
    if (infile.fail()) return false;

    char* buffer = new char[file_length + 1];
    infile.read(buffer, file_length);
    if (infile.fail()) return false;
    // Make sure it is null-terminated, as this represents string data.
    buffer[file_length] = '\0';

    loaded_options = AppOptions::LoadFromJsonConfig(buffer, options) != nullptr;

    delete[] buffer;
  }
  return loaded_options;
}

const char* kDefaultGoogleServicesNames[] = {"google-services-desktop.json",
                                             "google-services.json"};

// On desktop, if you create an app with no arguments, it will try to
// load any data it can find from the google-services-desktop.json
// file, or the google-services.json file, in that order.
App* App::Create() {
  std::string config_files;
  size_t number_of_config_filenames = sizeof(kDefaultGoogleServicesNames) /
                                      sizeof(kDefaultGoogleServicesNames[0]);
  for (size_t i = 0; i < number_of_config_filenames; i++) {
    AppOptions options;
    std::string full_path =
        internal::g_default_config_path + kDefaultGoogleServicesNames[i];

    if (LoadFromJsonConfigFile(full_path.c_str(), &options)) {
      return Create(options);
    }
    config_files += full_path;
    if (i < number_of_config_filenames - 1) config_files += ", ";
  }
  LogError(
      "Unable to load options for default app ([%s] are missing or "
      "malformed)",
      config_files.c_str());
  return nullptr;
}

App* App::Create(const AppOptions& options) {  // NOLINT
  return Create(options, kDefaultAppName);
}

App* App::Create(const AppOptions& options, const char* name) {  // NOLINT
  App* existing_app = GetInstance(name);
  if (existing_app) {
    LogError("firebase::App %s already created, options will not be applied.",
             name);
    return existing_app;
  }
  bool is_default_app = strcmp(kDefaultAppName, name) == 0;
  LogInfo("Firebase App initializing app %s (default %d).", name,
          is_default_app ? 1 : 0);

  App* new_app = new App();
  new_app->name_ = name;
  new_app->options_ = options;

  return app_common::AddApp(new_app, is_default_app, &new_app->init_results_);
}

App* App::GetInstance() {  // NOLINT
  return app_common::GetDefaultApp();
}

App* App::GetInstance(const char* name) {  // NOLINT
  return app_common::FindAppByName(name);
}

#ifdef INTERNAL_EXPERIMENTAL
internal::FunctionRegistry* App::function_registry() {
  return &(static_cast<internal::PrivateAppData*>(data_)->function_registry);
}
#endif

void App::RegisterLibrary(const char* library, const char* version) {
  app_common::RegisterLibrary(library, version);
}

const char* App::GetUserAgent() { return app_common::GetUserAgent(); }

void App::SetDefaultConfigPath(const char* path) {
  internal::g_default_config_path = path;

#if defined(WIN32)
  const char kSeperator = '\\';
#else
  const char kSeperator = '/';
#endif

  if (!internal::g_default_config_path.empty()) {
    char last_character = internal::g_default_config_path.back();
    if (last_character != '\\' && last_character != '/') {
      internal::g_default_config_path += kSeperator;
    }
  }
}

// Desktop support is for developer workflow only, so automatic data collection
// is always enabled.
void App::SetDataCollectionDefaultEnabled(bool /* enabled */) {}

// Desktop support is for developer workflow only, so automatic data collection
// is always enabled.
bool App::IsDataCollectionDefaultEnabled() const { return true; }

}  // namespace firebase